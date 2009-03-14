/*
 * * Copyright (C) 2008-2009 Ali <aliov@xfce.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "libxfpm/dbus-hal.h"
#include "libxfpm/xfpm-string.h"
#include "libxfpm/xfpm-common.h"

#ifdef HAVE_DPMS
#include "xfpm-dpms.h"
#endif

#include "xfpm-engine.h"
#include "xfpm-supply.h"
#include "xfpm-cpu.h"
#include "xfpm-network-manager.h"
#include "xfpm-button-xf86.h"
#include "xfpm-lid-hal.h"
#include "xfpm-brightness-hal.h"
#include "xfpm-config.h"

#define DUPLICATE_SHUTDOWN_TIMEOUT 4.0f

/* Init */
static void xfpm_engine_class_init (XfpmEngineClass *klass);
static void xfpm_engine_init       (XfpmEngine *engine);
static void xfpm_engine_finalize   (GObject *object);

#define XFPM_ENGINE_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_ENGINE, XfpmEnginePrivate))

struct XfpmEnginePrivate
{
    DbusHal   	      *hbus;
    XfconfChannel     *channel;
    XfpmSupply        *supply;
    XfpmCpu           *cpu;
    XfpmButtonXf86    *xf86_button;
    XfpmLidHal        *lid;
    XfpmBrightnessHal *brg_hal;
    
#ifdef HAVE_DPMS
    XfpmDpms          *dpms;
#endif

    GTimer            *button_timer;
    
    guint8             power_management;
    gboolean           on_battery;
    
    gboolean           block_shutdown;
    
    /*Configuration */
    XfpmShutdownRequest sleep_button;
    XfpmShutdownRequest lid_button_ac;
    XfpmShutdownRequest lid_button_battery;
    gboolean            lock_screen;
    
};

G_DEFINE_TYPE(XfpmEngine, xfpm_engine, G_TYPE_OBJECT)

static void
xfpm_engine_class_init(XfpmEngineClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_engine_finalize;

    g_type_class_add_private(klass,sizeof(XfpmEnginePrivate));
}

static void
xfpm_engine_init (XfpmEngine *engine)
{
    engine->priv = XFPM_ENGINE_GET_PRIVATE(engine);
    
    engine->priv->hbus = dbus_hal_new ();

    engine->priv->button_timer= g_timer_new ();

    engine->priv->channel     = NULL;
    engine->priv->supply      = NULL;
#ifdef HAVE_DPMS
    engine->priv->dpms        = NULL;
#endif    
    engine->priv->cpu         = NULL;
    engine->priv->xf86_button = NULL;
    engine->priv->lid         = NULL;
    engine->priv->brg_hal     = NULL;
}

static void
xfpm_engine_finalize (GObject *object)
{
    XfpmEngine *engine;

    engine = XFPM_ENGINE(object);
    
    if ( engine->priv->channel )
    	g_object_unref (engine->priv->channel);
	
    if ( engine->priv->supply )
    	g_object_unref (engine->priv->supply);
#ifdef HAVE_DPMS
    if ( engine->priv->dpms )
    	g_object_unref (engine->priv->dpms);
#endif

    if ( engine->priv->cpu )
    	g_object_unref (engine->priv->cpu);

    if ( engine->priv->lid)
    	g_object_unref(engine->priv->lid);
    
    if ( engine->priv->hbus)
    	g_object_unref(engine->priv->hbus);

    if ( engine->priv->button_timer)
    	g_timer_destroy (engine->priv->button_timer);
	
    G_OBJECT_CLASS(xfpm_engine_parent_class)->finalize(object);
}

static void
xfpm_engine_shutdown_request (XfpmEngine *engine, XfpmShutdownRequest shutdown)
{
    const gchar *action = xfpm_int_to_shutdown_string (shutdown);
	
    if ( xfpm_strequal(action, "Nothing") )
    {
	TRACE("Sleep button disabled in configuration");
	return;
    }
    else
    {
	TRACE("Going to do %s\n", action);
	xfpm_send_message_to_network_manager ("sleep");
	
	if ( shutdown != XFPM_DO_SHUTDOWN && engine->priv->lock_screen )
	    xfpm_lock_screen ();
	    
	dbus_hal_shutdown (engine->priv->hbus, action, NULL);
	xfpm_send_message_to_network_manager ("wake");
    }
}

static void
xfpm_engine_shutdown_request_battery_cb (XfpmSupply *supply, XfpmShutdownRequest action, XfpmEngine *engine)
{
    xfpm_engine_shutdown_request (engine, action);
}

static void
xfpm_engine_on_battery_cb (XfpmSupply *supply, gboolean on_battery, XfpmEngine *engine)
{
    engine->priv->on_battery = on_battery;
#ifdef HAVE_DPMS
    xfpm_dpms_set_on_battery (engine->priv->dpms, on_battery);
#endif
    xfpm_cpu_set_on_battery (engine->priv->cpu, on_battery);
    xfpm_brightness_hal_set_on_battery (engine->priv->brg_hal, on_battery);
}

static void
xfpm_engine_xf86_button_pressed_cb (XfpmButtonXf86 *button, XfpmXF86Button type, XfpmEngine *engine)
{
    TRACE("Received button press event type %d", type);
    
    if ( type == BUTTON_POWER_OFF || type == BUTTON_SLEEP )
    {
	if ( engine->priv->block_shutdown )
	    return;
	    
	if ( g_timer_elapsed (engine->priv->button_timer, NULL ) < DUPLICATE_SHUTDOWN_TIMEOUT )
	{
	    TRACE ("Not accepting shutdown request");
	    return;
	}
    	else
	{
	    TRACE("Accepting shutdown request");
	    xfpm_engine_shutdown_request (engine, engine->priv->sleep_button);
	}
	    
    }
    g_timer_reset (engine->priv->button_timer);
}

static void
xfpm_engine_lid_closed_cb (XfpmLidHal *lid, XfpmEngine *engine)
{
    g_return_if_fail (engine->priv->lid_button_ac != XFPM_DO_SHUTDOWN );
    g_return_if_fail (engine->priv->lid_button_battery != XFPM_DO_SHUTDOWN );

    if ( engine->priv->on_battery && engine->priv->lid_button_battery == XFPM_DO_NOTHING )
    {
	TRACE("System on battery, doing nothing: user settings\n");
    	return;
    }
	
    if ( !engine->priv->on_battery && engine->priv->lid_button_ac == XFPM_DO_NOTHING )
    {
	TRACE("System on AC, doing nothing: user settings\n");
    	return;
    }
    
    if ( engine->priv->block_shutdown )
    	return;
	
    if ( g_timer_elapsed (engine->priv->button_timer, NULL ) < DUPLICATE_SHUTDOWN_TIMEOUT )
    {
	TRACE ("Not accepting shutdown request");
	return;
    }
	
    TRACE("Accepting shutdown request");
    
    xfpm_engine_shutdown_request (engine, engine->priv->on_battery ? 
					  engine->priv->lid_button_battery :
					  engine->priv->lid_button_ac);
    g_timer_reset (engine->priv->button_timer);
}

static void
xfpm_engine_load_all (XfpmEngine *engine)
{
#ifdef HAVE_DPMS		      
    engine->priv->dpms = xfpm_dpms_new (engine->priv->channel);
#endif
    engine->priv->cpu = xfpm_cpu_new (engine->priv->channel, engine->priv->hbus);

    engine->priv->supply = xfpm_supply_new (engine->priv->hbus, engine->priv->channel);

    g_signal_connect (G_OBJECT(engine->priv->supply), "shutdown-request",
		      G_CALLBACK (xfpm_engine_shutdown_request_battery_cb), engine);

    g_signal_connect (G_OBJECT(engine->priv->supply), "on-battery",
		      G_CALLBACK (xfpm_engine_on_battery_cb), engine);
		      
    /*
     * Keys from XF86
     */
    engine->priv->xf86_button = xfpm_button_xf86_new ();
    
    g_signal_connect (engine->priv->xf86_button, "xf86-button-pressed", 
		      G_CALLBACK(xfpm_engine_xf86_button_pressed_cb), engine);
		      
    /*
     * Lid from HAL 
     */
    engine->priv->lid = xfpm_lid_hal_new ();
    
    if ( xfpm_lid_hw_found (engine->priv->lid ))
    	g_signal_connect (engine->priv->lid, "lid-closed",
			  G_CALLBACK(xfpm_engine_lid_closed_cb), engine);
			  
    /*
     * Brightness HAL
     */
    engine->priv->brg_hal = xfpm_brightness_hal_new (engine->priv->channel);
    
}

static void
xfpm_engine_load_configuration (XfpmEngine *engine)
{
    gchar *str;
    gint val;
    
    str = xfconf_channel_get_string (engine->priv->channel, SLEEP_SWITCH_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);
    
    if ( val == -1 || val == 3)
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, SLEEP_SWITCH_CFG);
	engine->priv->sleep_button = XFPM_DO_NOTHING;
	xfconf_channel_set_string (engine->priv->channel, SLEEP_SWITCH_CFG, "Nothing");
    }
    else engine->priv->sleep_button = val;
    
    g_free (str);
    
    str = xfconf_channel_get_string (engine->priv->channel, LID_SWITCH_ON_AC_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);

    if ( val == -1 || val == 3)
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, LID_SWITCH_ON_AC_CFG);
	engine->priv->lid_button_ac = XFPM_DO_NOTHING;
	xfconf_channel_set_string (engine->priv->channel, LID_SWITCH_ON_AC_CFG, "Nothing");
    }
    else engine->priv->lid_button_ac = val;
    
    g_free (str);
    
    str = xfconf_channel_get_string (engine->priv->channel, LID_SWITCH_ON_BATTERY_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);
    
    if ( val == -1 || val == 3)
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, LID_SWITCH_ON_BATTERY_CFG);
	engine->priv->lid_button_battery = XFPM_DO_NOTHING;
	xfconf_channel_set_string (engine->priv->channel, LID_SWITCH_ON_BATTERY_CFG, "Nothing");
    }
    else engine->priv->lid_button_battery = val;
    
    g_free (str);
    
    engine->priv->lock_screen = xfconf_channel_get_bool (engine->priv->channel, LOCK_SCREEN_ON_SLEEP, TRUE);
}

static void
xfpm_engine_property_changed_cb (XfconfChannel *channel, gchar *property, GValue *value, XfpmEngine *engine)
{
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
        return;

    if ( xfpm_strequal (property, SLEEP_SWITCH_CFG) )
    {
        const gchar *str = g_value_get_string (value);
	gint val = xfpm_shutdown_string_to_int (str);
	if ( val == -1 || val == 1 )
	{
	    g_warning ("Invalid value %s for property %s, using default\n", str, SLEEP_SWITCH_CFG);
	    engine->priv->sleep_button = XFPM_DO_NOTHING;
	}
	else
	    engine->priv->sleep_button = val;
    }
    else if ( xfpm_strequal (property, LID_SWITCH_ON_AC_CFG) )
    {
	const gchar *str = g_value_get_string (value);
	gint val = xfpm_shutdown_string_to_int (str);
	if ( val == -1 || val == 3 )
	{
	    g_warning ("Invalid value %s for property %s, using default\n", str, LID_SWITCH_ON_AC_CFG);
	    engine->priv->lid_button_ac = XFPM_DO_NOTHING;
	}
	else
	    engine->priv->lid_button_ac = val;
    }
    else if ( xfpm_strequal (property, LID_SWITCH_ON_BATTERY_CFG) )
    {
	const gchar *str = g_value_get_string (value);
	gint val = xfpm_shutdown_string_to_int (str);
	if ( val == -1 || val == 3 )
	{
	    g_warning ("Invalid value %s for property %s, using default\n", str, LID_SWITCH_ON_BATTERY_CFG);
	    engine->priv->lid_button_battery = XFPM_DO_NOTHING;
	}
	else
	    engine->priv->lid_button_battery = val;
    }
    else if ( xfpm_strequal (property, LOCK_SCREEN_ON_SLEEP ) )
    {
	engine->priv->lock_screen = g_value_get_boolean (value);
    }
}

XfpmEngine *
xfpm_engine_new(void)
{
    XfpmEngine *engine = NULL;
    engine = g_object_new (XFPM_TYPE_ENGINE, NULL);

    GError *error = NULL;
    if ( !xfconf_init(&error) )
    {
    	g_critical ("xfconf_init failed: %s\n", error->message);
       	g_error_free (error);
    }	
    
    engine->priv->channel   = xfconf_channel_new ("xfce4-power-manager");
    
    g_signal_connect (engine->priv->channel, "property-changed",
		      G_CALLBACK(xfpm_engine_property_changed_cb), engine);
    
    xfpm_engine_load_configuration (engine);
    xfpm_engine_load_all (engine);
    
    return engine;
}
