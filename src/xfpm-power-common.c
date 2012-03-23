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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "xfpm-power-common.h"
#include "xfpm-enum-glib.h"

#include "xfpm-icons.h"

/**
 * xfpm_power_translate_kind:
 *
 **/
const gchar *
xfpm_power_translate_kind (UpDeviceKind kind)
{
    switch (kind)
    {
        case UP_DEVICE_KIND_BATTERY:
            return _("Battery");

        case UP_DEVICE_KIND_UPS:
            return _("UPS");

        case UP_DEVICE_KIND_LINE_POWER:
            return _("Line power");

        case UP_DEVICE_KIND_MOUSE:
            return _("Mouse");

        case UP_DEVICE_KIND_KEYBOARD:
            return _("Keyboard");

        case UP_DEVICE_KIND_MONITOR:
            return _("Monitor");

        case UP_DEVICE_KIND_PDA:
            return _("PDA");

        case UP_DEVICE_KIND_PHONE:
            return _("Phone");

        case UP_DEVICE_KIND_UNKNOWN:
            return _("Unknown");

        default:
            return _("Battery");
    }
}

/**
 * xfpm_power_translate_technology:
 *
 **/
const gchar *
xfpm_power_translate_technology (UpDeviceTechnology technology)
{
    switch (technology)
    {
        case UP_DEVICE_TECHNOLOGY_LITHIUM_ION:
            return _("Lithium ion");

        case UP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER:
            return _("Lithium polymer");

        case UP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE:
            return _("Lithium iron phosphate");

        case UP_DEVICE_TECHNOLOGY_LEAD_ACID:
            return _("Lead acid");

        case UP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM:
            return _("Nickel cadmium");

        case UP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE:
            return _("Nickel metal hybride");

        default:
            return _("Unknown");
    }
}

const gchar *
xfpm_power_get_icon_name (UpDeviceKind kind)
{
    switch (kind)
    {
        case UP_DEVICE_KIND_BATTERY:
            return XFPM_BATTERY_ICON;

        case UP_DEVICE_KIND_UPS:
            return XFPM_UPS_ICON;

        case UP_DEVICE_KIND_LINE_POWER:
            return XFPM_AC_ADAPTOR_ICON;

        case UP_DEVICE_KIND_MOUSE:
            return XFPM_MOUSE_ICON;

        case UP_DEVICE_KIND_KEYBOARD:
            return XFPM_KBD_ICON;

        case UP_DEVICE_KIND_MONITOR:
            return "monitor";

        case UP_DEVICE_KIND_PDA:
            return XFPM_PDA_ICON;

        case UP_DEVICE_KIND_PHONE:
            return XFPM_PHONE_ICON;

        case UP_DEVICE_KIND_UNKNOWN:
            return XFPM_BATTERY_ICON;

        default:
            return XFPM_BATTERY_ICON;
    }
}


