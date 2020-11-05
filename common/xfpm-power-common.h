/*
 * * Copyright (C) 2010-2011 Ali <aliov@xfce.org>
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

#define UPOWER_NAME 	      "org.freedesktop.UPower"
#define UPOWER_PATH 	      "/org/freedesktop/UPower"

#define UPOWER_IFACE 	      "org.freedesktop.UPower"
#define UPOWER_IFACE_DEVICE   "org.freedesktop.UPower.Device"
#define UPOWER_PATH_DEVICE    "/org/freedesktop/UPower/devices/"

#define UPOWER_PATH_WAKEUPS   "/org/freedesktop/UPower/Wakeups"
#define UPOWER_IFACE_WAKEUPS  "org.freedesktop.UPower.Wakeups"

#define POLKIT_AUTH_SUSPEND_UPOWER	"org.freedesktop.upower.suspend"
#define POLKIT_AUTH_HIBERNATE_UPOWER	"org.freedesktop.upower.hibernate"

#define POLKIT_AUTH_SUSPEND_LOGIND	"org.freedesktop.login1.suspend"
#define POLKIT_AUTH_HIBERNATE_LOGIND	"org.freedesktop.login1.hibernate"

#define POLKIT_AUTH_SUSPEND_XFPM	"org.xfce.power.xfce4-pm-helper"
#define POLKIT_AUTH_HIBERNATE_XFPM	"org.xfce.power.xfce4-pm-helper"

#define POLKIT_AUTH_SUSPEND_CONSOLEKIT2   "org.freedesktop.consolekit.system.suspend"
#define POLKIT_AUTH_HIBERNATE_CONSOLEKIT2 "org.freedesktop.consolekit.system.hibernate"

const gchar *xfpm_power_translate_device_type         (guint         type);
const gchar *xfpm_power_translate_technology          (guint         value);
gchar       *xfpm_battery_get_time_string             (guint         seconds);
gchar       *get_device_panel_icon_name               (UpClient     *upower,
                                                       UpDevice     *device);
gchar       *get_device_icon_name                     (UpClient     *upower,
                                                       UpDevice     *device,
                                                       gboolean      is_panel);
gchar       *get_device_description                   (UpClient     *upower,
                                                       UpDevice     *device);

#endif /* XFPM_UPOWER_COMMON */
