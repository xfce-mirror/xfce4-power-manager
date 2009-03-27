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

#include <dbus/dbus-glib.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "libxfpm/hal-manager.h"
#include "libxfpm/hal-device.h"
#include "libxfpm/xfpm-string.h"

#include "xfpm-brightness-hal.h"
#include "xfpm-button-xf86.h"
#include "xfpm-enum-glib.h"
#include "xfpm-xfconf.h"
#include "xfpm-idle.h"
#include "xfpm-config.h"
#include "xfpm-adapter.h"
#include "xfpm-inhibit.h"

/* Init */
static void xfpm_brightness_hal_class_init (XfpmBrightnessHalClass *klass);
static void xfpm_brightness_hal_init       (XfpmBrightnessHal *brg);
static void xfpm_brightness_hal_finalize   (GObject *object);

#define XFPM_BRIGHTNESS_HAL_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_BRIGHTNESS_HAL, XfpmBrightnessHalPrivate))

struct XfpmBrightnessHalPrivate
{
    DBusGProxy     *proxy;
    
    XfpmXfconf     *conf;
    XfpmIdle       *idle;
    XfpmButtonXf86 *button;
    XfpmAdapter    *adapter;
    XfpmInhibit    *inhibit;
    
    gint            max_level;
    gint            hw_level;
    gboolean        brightness_in_hw;
    gboolean        hw_found;
    gboolean        block;
    gboolean        inhibited;
    
    gboolean        on_battery;
    
};

enum
{
    TIMEOUT_INPUT = 0,
    TIMEOUT_ON_AC_ID,
    TIMEOUT_ON_BATTERY_ID
};

G_DEFINE_TYPE(XfpmBrightnessHal, xfpm_brightness_hal, G_TYPE_OBJECT)

static gint 
xfpm_brightness_hal_get_level (XfpmBrightnessHal *brg)
{
    GError *error = NULL;
    gint level = 0;
    gboolean ret = FALSE;
    
    ret = dbus_g_proxy_call (brg->priv->proxy, "GetBrightness", &error,
	 		     G_TYPE_INVALID,
			     G_TYPE_INT, &level,
			     G_TYPE_INVALID);

    if ( error )
    {
	g_critical ("Error getting brightness level: %s\n", error->message);
	g_error_free (error);
    }
		       
    if (!ret)
    {
	g_warning("GetBrightness failed\n");
    }
    return level;
}

static gboolean
xfpm_brightness_hal_set_level (XfpmBrightnessHal *brg, gint level)
{
    TRACE ("Setting level %d", level);
    
    GError *error = NULL;
    gboolean ret = FALSE;
    gint dummy;
    
    ret = dbus_g_proxy_call (brg->priv->proxy, "SetBrightness", &error,
			     G_TYPE_INT, level,
			     G_TYPE_INVALID,
			     G_TYPE_INT, &dummy, /* Just to avoid a warning! */
			     G_TYPE_INVALID );
		       
    if ( error )
    {
	g_critical ("Error setting brightness level: %s\n", error->message);
	g_error_free (error);
	return FALSE;
    }
    
    if (!ret)
    {
	g_warning("SetBrightness failed\n");
    }
    
    return TRUE;
}

static void
xfpm_brightness_hal_set_proxy (XfpmBrightnessHal *brg, const gchar *udi)
{
    DBusGConnection *bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
    
    brg->priv->hw_found = TRUE;
    
    brg->priv->proxy = dbus_g_proxy_new_for_name (bus,
		   			          "org.freedesktop.Hal",
						  udi,
					          "org.freedesktop.Hal.Device.LaptopPanel");
     
    if ( !brg->priv->proxy )
	g_warning ("Unable to get proxy for device %s\n", udi);
}

static void
xfpm_brightness_hal_get_device (XfpmBrightnessHal *brg, const gchar *udi)
{
    HalDevice *device = hal_device_new ();
    hal_device_set_udi (device, udi);

    brg->priv->max_level = 
	hal_device_get_property_int (device, "laptop_panel.num_levels");
    
    if ( hal_device_has_key (device, "laptop_panel.brightness_in_hardware") )
	brg->priv->brightness_in_hw = hal_device_get_property_bool (device ,"laptop_panel.brightness_in_hardware");
	
    TRACE ("laptop_panel.num_levels=%d\n", brg->priv->max_level);
    
    g_object_unref (device);
}


static gboolean
xfpm_brightness_hal_setup (XfpmBrightnessHal *brg)
{
    gchar **udi = NULL;

    HalManager *manager = hal_manager_new ();
    
    udi = hal_manager_find_device_by_capability (manager, "laptop_panel");
    
    if ( !udi || !udi[0])
    {
	g_object_unref ( manager);
    	return FALSE;
    }
	
    TRACE ("Found laptop_panel with udi=%s\n", udi[0]);
    xfpm_brightness_hal_get_device (brg, udi[0]);
    xfpm_brightness_hal_set_proxy (brg, udi[0]);

    brg->priv->hw_level = xfpm_brightness_hal_get_level (brg);
    TRACE ("Current hw level =%d\n", brg->priv->hw_level);
    
    hal_manager_free_string_array (udi);
    return TRUE;
}

static void
xfpm_brightness_hal_up (XfpmBrightnessHal *brg)
{
    if ( brg->priv->brightness_in_hw )
	return;
    
    if ( brg->priv->hw_level <= brg->priv->max_level -2 )
    {
	TRACE ("Brightness key up");
	xfpm_brightness_hal_set_level (brg, brg->priv->hw_level + 1 );
	brg->priv->hw_level = xfpm_brightness_hal_get_level (brg);
    }
}

static void
xfpm_brightness_hal_down (XfpmBrightnessHal *brg)
{
    if ( brg->priv->brightness_in_hw )
	return;
	
    if ( brg->priv->hw_level != 0)
    {
	TRACE("Brightness key down");
	xfpm_brightness_hal_set_level (brg, brg->priv->hw_level - 1 );
	brg->priv->hw_level = xfpm_brightness_hal_get_level (brg);
    }
}

static void
xfpm_brightness_hal_button_pressed_cb (XfpmButtonXf86 *button, XfpmXF86Button type, XfpmBrightnessHal *brg)
{
    if ( type == BUTTON_MON_BRIGHTNESS_UP )
    {
	brg->priv->block = TRUE;
	xfpm_brightness_hal_up (brg);
    }
    else if ( type == BUTTON_MON_BRIGHTNESS_DOWN )
    {
	brg->priv->block = TRUE;
	xfpm_brightness_hal_down (brg);
    }
}

static void
xfpm_brightness_hal_reset_cb (XfpmIdle *idle, XfpmBrightnessHal *brg)
{
    gint level;
    
    if (brg->priv->block)
	return;
    
    if ( brg->priv->inhibited )
	return;
    
    level = xfpm_brightness_hal_get_level(brg);
     
    if ( level != brg->priv->hw_level )
    {
	TRACE("Resetting brightness level to %d", brg->priv->hw_level);
	xfpm_brightness_hal_set_level(brg, brg->priv->hw_level);
    }
}

static void
xfpm_brightness_timeout_on_ac (XfpmBrightnessHal *brg)
{
    gint level;
    
    if ( brg->priv->on_battery )
	    return;
    
    level = xfpm_brightness_hal_get_level(brg);
    
    if ( level != 0 )
    {
	TRACE ("Reducing brightness, on AC power\n");
	xfpm_brightness_hal_set_level(brg, 0);
    }
}

static void
xfpm_brightness_timeout_on_battery (XfpmBrightnessHal *brg)
{
    gint level;
    
    if ( !brg->priv->on_battery )
	    return;
    
    level = xfpm_brightness_hal_get_level(brg);
    
    if ( level != 0 )
    {
	xfpm_brightness_hal_set_level(brg, 0);
	TRACE ("Reducing brightness, on battery power\n");
    }
}

static void
xfpm_brightness_hal_alarm_timeout_cb (XfpmIdle *idle, guint id, XfpmBrightnessHal *brg)
{
    if ( brg->priv->block )
	brg->priv->block = FALSE;
    
    if ( brg->priv->inhibited )
	return;
    
    id == TIMEOUT_ON_AC_ID ? xfpm_brightness_timeout_on_ac (brg) :
			     xfpm_brightness_timeout_on_battery (brg);
}

static void
xfpm_brightness_hal_adapter_changed_cb (XfpmAdapter *adapter, gboolean present, XfpmBrightnessHal *brg)
{
    brg->priv->on_battery = !present;
}

static void
xfpm_brightness_hal_inhibit_changed_cb (XfpmInhibit *inhibit, gboolean inhibited, XfpmBrightnessHal *brg)
{
    TRACE("Inhibit changed %s", xfpm_bool_to_string (inhibited));
    brg->priv->inhibited = inhibited;
}

static void
xfpm_brightness_get_user_timeouts (XfpmBrightnessHal *brg, guint16 *on_ac, guint16 *on_battery)
{
    *on_ac      = xfpm_xfconf_get_property_int (brg->priv->conf, BRIGHTNESS_ON_AC);
    *on_battery = xfpm_xfconf_get_property_int (brg->priv->conf, BRIGHTNESS_ON_BATTERY);
}

static void
xfpm_brightness_hal_set_timeouts (XfpmBrightnessHal *brg )
{
    guint16 on_ac, on_battery;
    
    xfpm_brightness_get_user_timeouts (brg, &on_ac, &on_battery);
    
    if ( on_ac == 9 )
	on_ac = 0;
    if ( on_battery == 9 )
	on_battery = 0;
    
    xfpm_idle_set_alarm (brg->priv->idle, TIMEOUT_ON_AC_ID, on_ac * 1000);
    
    xfpm_idle_set_alarm (brg->priv->idle, TIMEOUT_ON_BATTERY_ID, on_battery * 1000);
    
    xfpm_idle_alarm_reset_all (brg->priv->idle);
}

static void
xfpm_brightness_hal_settings_changed_cb (XfpmXfconf *conf, XfpmBrightnessHal *brg)
{
    xfpm_brightness_hal_set_timeouts (brg);
}

static void
xfpm_brightness_hal_class_init(XfpmBrightnessHalClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_brightness_hal_finalize;

    g_type_class_add_private(klass,sizeof(XfpmBrightnessHalPrivate));
}

static void
xfpm_brightness_hal_init(XfpmBrightnessHal *brg)
{
    brg->priv = XFPM_BRIGHTNESS_HAL_GET_PRIVATE(brg);
    
    brg->priv->proxy    	= NULL;
    brg->priv->idle             = NULL;
    brg->priv->hw_found 	= FALSE;
    brg->priv->on_battery       = FALSE;
    brg->priv->block            = FALSE;
    brg->priv->brightness_in_hw = FALSE;
    brg->priv->max_level        = 0;
    brg->priv->inhibited        = FALSE;
    
    xfpm_brightness_hal_setup (brg);

    if ( brg->priv->hw_found || brg->priv->max_level != 0 )
    {
	brg->priv->idle     = xfpm_idle_new ();
	brg->priv->conf     = xfpm_xfconf_new ();
	brg->priv->button   = xfpm_button_xf86_new ();
	brg->priv->adapter  = xfpm_adapter_new ();
	brg->priv->inhibit  = xfpm_inhibit_new ();
	
	g_signal_connect (brg->priv->inhibit, "has-inhibit-changed",
			  G_CALLBACK(xfpm_brightness_hal_inhibit_changed_cb), brg);
	
	brg->priv->on_battery = !xfpm_adapter_get_present (brg->priv->adapter);
	
	g_signal_connect (brg->priv->adapter, "adapter-changed",
			  G_CALLBACK(xfpm_brightness_hal_adapter_changed_cb), brg);
	
	g_signal_connect (brg->priv->button, "xf86-button-pressed",	
			  G_CALLBACK(xfpm_brightness_hal_button_pressed_cb), brg);
	
	g_signal_connect (brg->priv->idle, "alarm-timeout",
			  G_CALLBACK(xfpm_brightness_hal_alarm_timeout_cb), brg);
			  
	g_signal_connect (brg->priv->idle, "reset",
			  G_CALLBACK(xfpm_brightness_hal_reset_cb), brg);
			  
	g_signal_connect (brg->priv->conf, "brightness-settings-changed", 
			  G_CALLBACK(xfpm_brightness_hal_settings_changed_cb), brg);
			  
	xfpm_brightness_hal_set_timeouts (brg);
    }
    else
    {
	TRACE("No lcd brightness control found in the system");
    }
}

static void
xfpm_brightness_hal_finalize (GObject *object)
{
    XfpmBrightnessHal *brg;

    brg = XFPM_BRIGHTNESS_HAL(object);
    
    if ( brg->priv->proxy )
	g_object_unref (brg->priv->proxy);
    
    if ( brg->priv->idle )
	g_object_unref (brg->priv->idle);
	
    if ( brg->priv->conf )
	g_object_unref (brg->priv->conf);
	
    if (brg->priv->adapter)
	g_object_unref (brg->priv->adapter);
	
    if ( brg->priv->inhibit )
	g_object_unref (brg->priv->inhibit);
	
    G_OBJECT_CLASS(xfpm_brightness_hal_parent_class)->finalize(object);
}

XfpmBrightnessHal *
xfpm_brightness_hal_new ()
{
    XfpmBrightnessHal *brg = NULL;
    brg = g_object_new (XFPM_TYPE_BRIGHTNESS_HAL, NULL);
    return brg;
}

gboolean xfpm_brightness_hal_has_hw (XfpmBrightnessHal *brg)
{
    g_return_val_if_fail (XFPM_IS_BRIGHTNESS_HAL (brg), FALSE);
    
    return brg->priv->hw_found;
}

void xfpm_brightness_hal_update_level (XfpmBrightnessHal *brg, guint level)
{
    g_return_if_fail (XFPM_IS_BRIGHTNESS_HAL (brg));
    
    brg->priv->hw_level = level;
}
