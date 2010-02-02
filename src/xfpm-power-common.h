/*
 * * Copyright (C) 2010 Ali <aliov@xfce.org>
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

#ifndef XFPM_UPOWER_COMMON
#define XFPM_UPOWER_COMMON

#include <dbus/dbus-glib.h>

#define DKP_NAME 	     "org.freedesktop.DeviceKit.Power"
#define DKP_PATH 	     "/org/freedesktop/DeviceKit/Power"

#define DKP_IFACE 	     "org.freedesktop.DeviceKit.Power"
#define DKP_IFACE_DEVICE     "org.freedesktop.DeviceKit.Power.Device"
#define DKP_PATH_DEVICE      "/org/freedesktop/DeviceKit/Power/devices/"

#define DKP_PATH_WAKEUPS     "/org/freedesktop/DeviceKit/Power/Wakeups"
#define DKP_IFACE_WAKEUPS    "org.freedesktop.DeviceKit.Power.Wakeups"


#define UPOWER_NAME 	     "org.freedesktop.UPower"
#define UPOWER_PATH 	     "/org/freedesktop/UPower"

#define UPOWER_IFACE 	     "org.freedesktop.UPower"
#define UPOWER_IFACE_DEVICE  "org.freedesktop.UPower.Device"
#define UPOWER_PATH_DEVICE   "/org/freedesktop/UPower/devices/"

#define UPOWER_PATH_WAKEUPS  "/org/freedesktop/UPower/Wakeups"
#define UPOWER_IFACE_WAKEUPS "org.freedesktop.UPower.Wakeups"

GPtrArray 	*xfpm_power_enumerate_devices		(DBusGProxy *proxy);

GHashTable	*xfpm_power_get_interface_properties 	(DBusGProxy *proxy_prop, 
							 const gchar *iface_name);

GValue 		 xfpm_power_get_interface_property   	(DBusGProxy *proxy, 
							 const gchar *iface_name, 
							 const gchar *prop_name);

const gchar 	*xfpm_power_translate_device_type 	(guint type);

const gchar	*xfpm_power_translate_technology	(guint value);

const gchar	*xfpm_power_get_icon_name		(guint device_type);


#endif /* XFPM_UPOWER_COMMON */
