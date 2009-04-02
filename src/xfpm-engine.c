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
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/hal-manager.h"
#include "libxfpm/xfpm-string.h"
#include "libxfpm/xfpm-common.h"
#include "libxfpm/xfpm-notify.h"

#ifdef HAVE_DPMS
#include "xfpm-dpms.h"
#endif

#include "xfpm-engine.h"
#include "xfpm-supply.h"
#include "xfpm-adapter.h"
#include "xfpm-xfconf.h"
#include "xfpm-cpu.h"
#include "xfpm-network-manager.h"
#include "xfpm-button-xf86.h"
#include "xfpm-lid-hal.h"
#include "xfpm-inhibit.h"
#include "xfpm-backlight.h"
#include "xfpm-shutdown.h"
#include "xfpm-errors.h"
#include "xfpm-config.h"

/* Init */
static void xfpm_engine_class_init (XfpmEngineClass * klass);
static void xfpm_engine_init (XfpmEngine * engine);
static void xfpm_engine_finalize (GObject * object);

static void xfpm_engine_dbus_class_init (XfpmEngineClass * klass);
static void xfpm_engine_dbus_init (XfpmEngine * engine);

#define XFPM_ENGINE_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_ENGINE, XfpmEnginePrivate))

struct XfpmEnginePrivate
{
    XfpmXfconf 		*conf;
    XfpmSupply 		*supply;
    XfpmNotify 		*notify;

    XfpmCpu 		*cpu;
    XfpmButtonXf86 	*xf86_button;
    XfpmLidHal 		*lid;
    XfpmBacklight 	*bk;
    XfpmAdapter 	*adapter;
    XfpmInhibit 	*inhibit;
    XfpmShutdown        *shutdown;
#ifdef HAVE_DPMS
    XfpmDpms *dpms;
#endif
    gboolean inhibited;

    guint8 power_management;
    gboolean on_battery;
    gboolean is_laptop;

    gboolean has_lcd_brightness;
    gboolean has_lid;
};

enum
{
    ON_BATTERY_CHANGED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmEngine, xfpm_engine, G_TYPE_OBJECT)

static gboolean xfpm_engine_do_suspend (XfpmEngine * engine)
{
    GError *error = NULL;

    xfpm_suspend (engine->priv->shutdown, &error);

    if (error)
    {
	g_warning ("%s", error->message);
	g_error_free (error);
    }
    xfpm_send_message_to_network_manager ("wake");
    return FALSE;
}

static gboolean
xfpm_engine_do_hibernate (XfpmEngine * engine)
{
    GError *error = NULL;

    xfpm_hibernate (engine->priv->shutdown, &error);

    if (error)
    {
	g_warning ("%s", error->message);
	g_error_free (error);
    }
    xfpm_send_message_to_network_manager ("wake");
    return FALSE;
}

static gboolean
xfpm_engine_do_shutdown (XfpmEngine * engine)
{
    GError *error = NULL;

    xfpm_shutdown (engine->priv->shutdown, &error);

    if (error)
    {
	g_warning ("%s", error->message);
	g_error_free (error);
    }
    return FALSE;
}

static void
xfpm_engine_shutdown_request (XfpmEngine * engine,
			      XfpmShutdownRequest shutdown, gboolean critical)
{
    gboolean lock_screen;
    const gchar *action = xfpm_int_to_shutdown_string (shutdown);

    lock_screen =
	xfpm_xfconf_get_property_bool (engine->priv->conf, LOCK_SCREEN_ON_SLEEP);

    if (xfpm_strequal (action, "Nothing"))
    {
	TRACE ("Sleep button disabled in configuration");
	return;
    }
    else if (!engine->priv->inhibited)
    {
	TRACE ("Going to do %s\n", action);
	xfpm_send_message_to_network_manager ("sleep");

	if (shutdown == XFPM_DO_SHUTDOWN)
	{
	    xfpm_engine_do_shutdown (engine);
	}
	else if (shutdown == XFPM_DO_HIBERNATE)
	{
	    g_timeout_add_seconds (2, (GSourceFunc) xfpm_engine_do_hibernate,
				   engine);
	}
	else if (shutdown == XFPM_DO_SUSPEND)
	{
	    g_timeout_add_seconds (2, (GSourceFunc) xfpm_engine_do_suspend,
				   engine);
	}

	if (lock_screen)
	    xfpm_lock_screen ();
    }
}

static void
xfpm_engine_shutdown_request_battery_cb (XfpmSupply * supply,
					 gboolean critical,
					 XfpmShutdownRequest action,
					 XfpmEngine * engine)
{
    if ( engine->priv->inhibited && critical == FALSE ) /* We can ignore the request here */
	return;
	
    xfpm_engine_shutdown_request (engine, action, critical);
}

static void
xfpm_engine_xf86_button_pressed_cb (XfpmButtonXf86 * button,
				    XfpmXF86Button type, XfpmEngine * engine)
{
    TRACE ("Received button press event type %d", type);
    XfpmShutdownRequest shutdown;
  
    if ( engine->priv->inhibited )
    {
	TRACE("Power manager automatic sleep is currently disabled");
	return;
    }
  
    if ( type != BUTTON_POWER_OFF && type != BUTTON_SLEEP )
	return;

    shutdown = xfpm_xfconf_get_property_enum (engine->priv->conf, 
					      type == BUTTON_POWER_OFF ? POWER_SWITCH_CFG :
					      SLEEP_SWITCH_CFG );
    TRACE ("Accepting shutdown request");
    xfpm_engine_shutdown_request (engine, shutdown, FALSE);
}

static void
xfpm_engine_lid_closed_cb (XfpmLidHal * lid, XfpmEngine * engine)
{
    XfpmShutdownRequest shutdown;

    shutdown = xfpm_xfconf_get_property_enum (engine->priv->conf,
					      engine->priv->
					      on_battery ?
					      LID_SWITCH_ON_BATTERY_CFG :
					      LID_SWITCH_ON_AC_CFG);

    if ( engine->priv->inhibited )
    {
	TRACE("Power manager automatic sleep is currently disabled");
	return;
    }
    xfpm_engine_shutdown_request (engine, shutdown, FALSE);
}

static void
xfpm_engine_check_hal_iface (XfpmEngine * engine)
{
    gboolean can_suspend, can_hibernate, caller;

    g_object_get (G_OBJECT (engine->priv->shutdown),
		  "caller-privilege", &caller,
		  "can-suspend", &can_suspend,
		  "can-hibernate", &can_hibernate,
		  NULL);

    if ( caller )
    {
	if (can_hibernate)
	    engine->priv->power_management |= SYSTEM_CAN_HIBERNATE;
	if (can_suspend)
	    engine->priv->power_management |= SYSTEM_CAN_SUSPEND;
    }

  //FIXME: Show errors here
}

static void
xfpm_engine_load_all (XfpmEngine * engine)
{
    HalManager *manager;

    xfpm_engine_check_hal_iface (engine);
    
    manager = hal_manager_new ();

    if ( hal_manager_get_is_laptop (manager))
	engine->priv->is_laptop = TRUE;
    else
	engine->priv->is_laptop = FALSE;

    g_object_unref (manager);

#ifdef HAVE_DPMS
    engine->priv->dpms = xfpm_dpms_new ();
#endif
    if (engine->priv->is_laptop)
	engine->priv->cpu = xfpm_cpu_new ();

    engine->priv->supply = xfpm_supply_new (engine->priv->power_management);
    g_signal_connect (G_OBJECT (engine->priv->supply), "shutdown-request",
		      G_CALLBACK (xfpm_engine_shutdown_request_battery_cb),
		      engine);
    xfpm_supply_monitor (engine->priv->supply);

  /*
   * Keys from XF86
   */
    engine->priv->xf86_button = xfpm_button_xf86_new ();

    g_signal_connect (engine->priv->xf86_button, "xf86-button-pressed",
		      G_CALLBACK (xfpm_engine_xf86_button_pressed_cb), engine);

  /*
   * Lid from HAL 
   */
    if (engine->priv->is_laptop)
    {
	engine->priv->lid = xfpm_lid_hal_new ();
	engine->priv->has_lid = xfpm_lid_hw_found (engine->priv->lid);
	if (engine->priv->has_lid)
	    g_signal_connect (engine->priv->lid, "lid-closed",
			      G_CALLBACK (xfpm_engine_lid_closed_cb), engine);
	else
	    g_object_unref (engine->priv->lid);
    }

  /*
   * Brightness HAL
   */
    if (engine->priv->is_laptop)
    {
	engine->priv->bk = xfpm_backlight_new ();
	engine->priv->has_lcd_brightness =
	xfpm_backlight_has_hw (engine->priv->bk);
    }
}

static void
xfpm_engine_adapter_changed_cb (XfpmAdapter * adapter, gboolean present,
				XfpmEngine * engine)
{
    engine->priv->on_battery = !present;
    g_signal_emit (G_OBJECT (engine), signals [ON_BATTERY_CHANGED], 0, engine->priv->on_battery);
}

static void
xfpm_engine_inhibit_changed_cb (XfpmInhibit * inhibit, gboolean inhibited,
				XfpmEngine * engine)
{
    engine->priv->inhibited = inhibited;
}

static void
xfpm_engine_class_init (XfpmEngineClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals [ON_BATTERY_CHANGED] = 
        g_signal_new("on-battery-changed",
                      XFPM_TYPE_ENGINE,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmEngineClass, on_battery_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    object_class->finalize = xfpm_engine_finalize;

    g_type_class_add_private (klass, sizeof (XfpmEnginePrivate));

    xfpm_engine_dbus_class_init (klass);
}

static void
xfpm_engine_init (XfpmEngine * engine)
{
    engine->priv = XFPM_ENGINE_GET_PRIVATE (engine);

    engine->priv->shutdown = xfpm_shutdown_new ();
    engine->priv->adapter = xfpm_adapter_new ();
    engine->priv->notify = xfpm_notify_new ();

    engine->priv->inhibit = xfpm_inhibit_new ();
    engine->priv->inhibited = FALSE;

    g_signal_connect (engine->priv->inhibit, "has-inhibit-changed",
		      G_CALLBACK (xfpm_engine_inhibit_changed_cb), engine);

    engine->priv->conf = NULL;
    engine->priv->supply = NULL;
#ifdef HAVE_DPMS
    engine->priv->dpms = NULL;
#endif
    engine->priv->cpu = NULL;
    engine->priv->xf86_button = NULL;
    engine->priv->lid = NULL;
    engine->priv->bk = NULL;

    engine->priv->power_management = 0;

    xfpm_engine_dbus_init (engine);

    engine->priv->conf = xfpm_xfconf_new ();

    engine->priv->on_battery =
	!xfpm_adapter_get_present (engine->priv->adapter);

    g_signal_connect (engine->priv->adapter, "adapter-changed",
		      G_CALLBACK (xfpm_engine_adapter_changed_cb), engine);

    xfpm_engine_load_all (engine);
}

static void
xfpm_engine_finalize (GObject * object)
{
    XfpmEngine *engine;

    engine = XFPM_ENGINE (object);

    g_object_unref (engine->priv->conf);

    g_object_unref (engine->priv->supply);

#ifdef HAVE_DPMS
    if (engine->priv->dpms)
	g_object_unref (engine->priv->dpms);
#endif

    if (engine->priv->cpu)
	g_object_unref (engine->priv->cpu);

    if (engine->priv->lid)
	g_object_unref (engine->priv->lid);

    g_object_unref (engine->priv->shutdown);

    g_object_unref (engine->priv->adapter);

    if (engine->priv->bk)
	g_object_unref (engine->priv->bk);

    g_object_unref (engine->priv->notify);

    G_OBJECT_CLASS (xfpm_engine_parent_class)->finalize (object);
}

XfpmEngine *
xfpm_engine_new (void)
{
    XfpmEngine *engine = NULL;
    engine = g_object_new (XFPM_TYPE_ENGINE, NULL);

    return engine;
}

void
xfpm_engine_get_info (XfpmEngine * engine,
		      gboolean * system_laptop,
		      gboolean * user_privilege,
		      gboolean * can_suspend,
		      gboolean * can_hibernate,
		      gboolean * has_lcd_brightness, gboolean * has_lid)
{
    g_return_if_fail (XFPM_IS_ENGINE (engine));

    g_object_get (G_OBJECT (engine->priv->shutdown),
		  "caller-privilege", user_privilege,
		  "can-suspend", can_suspend,
		  "can-hibernate", can_hibernate, NULL);

    *system_laptop = engine->priv->is_laptop;
    *has_lcd_brightness = engine->priv->has_lcd_brightness;
    *has_lid = engine->priv->has_lid;
}

/*
 * 
 * DBus server implementation for org.freedesktop.PowerManagement
 * 
 */

static gboolean xfpm_engine_dbus_hibernate (XfpmEngine * engine,
					    GError ** error);

static gboolean xfpm_engine_dbus_suspend (XfpmEngine * engine,
					  GError ** error);

static gboolean xfpm_engine_dbus_can_hibernate (XfpmEngine * engine,
						gboolean * OUT_can_hibernate,
						GError ** error);

static gboolean xfpm_engine_dbus_can_suspend (XfpmEngine * engine,
					      gboolean * OUT_can_suspend,
					      GError ** error);

static gboolean xfpm_engine_dbus_get_on_battery (XfpmEngine * engine,
						 gboolean * OUT_on_battery,
						 GError ** error);

static gboolean xfpm_engine_dbus_get_low_battery (XfpmEngine * engine,
						  gboolean * OUT_low_battery,
						  GError ** error);

#include "org.freedesktop.PowerManagement.h"

static void
xfpm_engine_dbus_class_init (XfpmEngineClass * klass)
{
    dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
				     &dbus_glib_xfpm_engine_object_info);

    dbus_g_error_domain_register (XFPM_ERROR,
				  "org.freedesktop.PowerManagement",
				  XFPM_TYPE_ERROR);
}

static void
xfpm_engine_dbus_init (XfpmEngine * engine)
{
    DBusGConnection *bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

    dbus_g_connection_register_g_object (bus,
				         "/org/freedesktop/PowerManagement",
				         G_OBJECT (engine));
}

static gboolean
xfpm_engine_dbus_hibernate (XfpmEngine * engine, GError ** error)
{
    TRACE ("Hibernate message received");
    gboolean caller_privilege, can_hibernate;

    g_object_get (G_OBJECT (engine->priv->shutdown),
		  "caller-privilege", &caller_privilege,
		  "can-hibernate", &can_hibernate, NULL);

    if (!caller_privilege)
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
		    _("Permission denied"));
	return FALSE;
    }

    if (!can_hibernate)
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_HIBERNATE_NOT_SUPPORTED,
		    _("Hibernate not supported"));
	return FALSE;
    }

    xfpm_engine_shutdown_request (engine, XFPM_DO_HIBERNATE, FALSE);

    return TRUE;
}

static gboolean
xfpm_engine_dbus_suspend (XfpmEngine * engine, GError ** error)
{
    TRACE ("Suspend message received");
    gboolean caller_privilege, can_suspend;

    g_object_get (G_OBJECT (engine->priv->shutdown),
		  "caller-privilege", &caller_privilege,
		  "can-suspend", &can_suspend, NULL);

    if (!caller_privilege)
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
		    _("Permission denied"));
	return FALSE;
    }

    if (!can_suspend)
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_SUSPEND_NOT_SUPPORTED,
		    _("Suspend not supported"));
	return FALSE;
    }

    xfpm_engine_shutdown_request (engine, XFPM_DO_SUSPEND, FALSE);

    return TRUE;
}

static gboolean
xfpm_engine_dbus_can_hibernate (XfpmEngine * engine,
				gboolean * OUT_can_hibernate, GError ** error)
{
    TRACE ("Can hibernate message received");
    g_object_get (G_OBJECT (engine->priv->shutdown),
		    "can-hibernate", OUT_can_hibernate, NULL);

    return TRUE;
}

static gboolean
xfpm_engine_dbus_can_suspend (XfpmEngine * engine,
			      gboolean * OUT_can_suspend, GError ** error)
{
    TRACE ("Can suspend message received");
    g_object_get (G_OBJECT (engine->priv->shutdown),
		  "can-suspend", OUT_can_suspend, NULL);

    return TRUE;
}

static gboolean
xfpm_engine_dbus_get_on_battery (XfpmEngine * engine,
				 gboolean * OUT_on_battery, GError ** error)
{
    TRACE ("On battery message received");
    *OUT_on_battery = engine->priv->on_battery;

    return TRUE;
}

static gboolean
xfpm_engine_dbus_get_low_battery (XfpmEngine * engine,
				  gboolean * OUT_low_battery, GError ** error)
{
    TRACE ("On low battery message received");
    *OUT_low_battery = xfpm_supply_on_low_battery (engine->priv->supply);

    return TRUE;
}
