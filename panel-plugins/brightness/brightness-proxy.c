/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#include <dbus/dbus-glib.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/hal-manager.h"
#include "libxfpm/hal-device.h"

#include "brightness-proxy.h"

static void brightness_proxy_finalize   (GObject *object);

#define BRIGHTNESS_PROXY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), BRIGHTNESS_TYPE_PROXY, BrightnessProxyPrivate))

struct BrightnessProxyPrivate
{
    DBusGConnection *bus;
    DBusGConnection *session;
    DBusGProxy      *proxy;
    guint            max_level;
    gboolean         has_hw;
};

G_DEFINE_TYPE (BrightnessProxy, brightness_proxy, G_TYPE_OBJECT)

static void
brightness_proxy_get_device (BrightnessProxy *brightness)
{
    HalManager *manager;
    HalDevice *device;
    gchar **udis = NULL;
    
    manager = hal_manager_new ();
    
    udis = hal_manager_find_device_by_capability (manager, "laptop_panel");
    
    if (!udis || !udis[0] )
    {
	TRACE ("No laptop panel found on the system");
	brightness->priv->has_hw = FALSE;
	brightness->priv->proxy = NULL;
	goto out;
    }
    
    device = hal_device_new ();
    hal_device_set_udi (device, udis[0]);
    
    brightness->priv->max_level =
	hal_device_get_property_int (device, "laptop_panel.num_levels") -1;
	
    TRACE("Laptop panel %s with max level %d", udis[0], brightness->priv->max_level);
    
    brightness->priv->proxy = dbus_g_proxy_new_for_name (brightness->priv->bus,
		   			                 "org.freedesktop.Hal",
						         udis[0],
						         "org.freedesktop.Hal.Device.LaptopPanel");
    brightness->priv->has_hw = TRUE;
    
    g_object_unref (device);
out:
    g_object_unref (manager);
}

static void
brightness_proxy_class_init (BrightnessProxyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = brightness_proxy_finalize;

    g_type_class_add_private (klass, sizeof (BrightnessProxyPrivate));
}

static void
brightness_proxy_init (BrightnessProxy *brightness_proxy)
{
    brightness_proxy->priv = BRIGHTNESS_PROXY_GET_PRIVATE (brightness_proxy);
    
    brightness_proxy->priv->max_level = 0;
    
    // FIXME, Don't connect blindly
    brightness_proxy->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
    brightness_proxy->priv->session = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
    
    brightness_proxy_get_device (brightness_proxy);
}

static void
brightness_proxy_finalize (GObject *object)
{
    BrightnessProxy *brightness_proxy;

    brightness_proxy = BRIGHTNESS_PROXY (object);
    
    dbus_g_connection_unref (brightness_proxy->priv->bus);
    dbus_g_connection_unref (brightness_proxy->priv->session);
    
    if ( brightness_proxy->priv->proxy )
	g_object_unref (brightness_proxy->priv->proxy);

    G_OBJECT_CLASS (brightness_proxy_parent_class)->finalize (object);
}

static void
brightness_proxy_update_xfpm_brightness_level (BrightnessProxy *brightness, guint level)
{
    DBusGProxy *proxy;
    
    proxy = dbus_g_proxy_new_for_name (brightness->priv->session,
				       "org.freedesktop.PowerManagement",
				       "/org/freedesktop/PowerManagement/Backlight",
				       "org.freedesktop.PowerManagement.Backlight");
					       
    if ( !proxy )
    {
	g_warning ("Failed to create proxy to Xfpm");
	return;
    }

    dbus_g_proxy_call_no_reply (proxy, "UpdateBrightness",
			        G_TYPE_UINT, level,
				G_TYPE_INVALID,
				G_TYPE_INVALID);
				
    g_object_unref ( proxy );
}

BrightnessProxy *
brightness_proxy_new (void)
{
    BrightnessProxy *brightness_proxy = NULL;
    brightness_proxy = g_object_new (BRIGHTNESS_TYPE_PROXY, NULL);
    return brightness_proxy;
}

gboolean
brightness_proxy_set_level (BrightnessProxy *brightness, guint level)
{
    GError *error = NULL;
    gboolean ret;
    gint dummy;
    
    g_return_val_if_fail (BRIGHTNESS_IS_PROXY (brightness), FALSE);
    
    ret = dbus_g_proxy_call (brightness->priv->proxy, "SetBrightness", &error,
			     G_TYPE_INT, level,
			     G_TYPE_INVALID,
			     G_TYPE_INT, &dummy,
			     G_TYPE_INVALID );
    if ( error )
    {
	g_critical ("Error setting brightness level: %s\n", error->message);
	g_error_free (error);
	return FALSE;
    }
	
    brightness_proxy_update_xfpm_brightness_level (brightness, level);
    
    return ret;
}

guint
brightness_proxy_get_level (BrightnessProxy *brightness)
{
    GError *error = NULL;
    gint level = 0;
    gboolean ret;
    
    g_return_val_if_fail (BRIGHTNESS_IS_PROXY (brightness), 0);
    
    ret = dbus_g_proxy_call (brightness->priv->proxy, "GetBrightness", &error,
	 		     G_TYPE_INVALID,
			     G_TYPE_INT, &level,
			     G_TYPE_INVALID);

    if ( error )
    {
	g_critical ("Error getting brightness level: %s\n", error->message);
	g_error_free (error);
    }
    return level;
}

guint brightness_proxy_get_max_level (BrightnessProxy *brightness)
{
    g_return_val_if_fail (BRIGHTNESS_IS_PROXY (brightness), 0);
    
    return brightness->priv->max_level;
}

gboolean brightness_proxy_has_hw (BrightnessProxy *brightness)
{
    g_return_val_if_fail (BRIGHTNESS_IS_PROXY (brightness), FALSE);
    
    return brightness->priv->has_hw;
}


void brightness_proxy_reload (BrightnessProxy *brightness)
{
    g_return_if_fail (BRIGHTNESS_IS_PROXY (brightness));
    
    if ( brightness->priv->proxy )
	g_object_unref (brightness->priv->proxy);
	
    brightness_proxy_get_device (brightness);
}
