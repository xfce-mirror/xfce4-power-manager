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
 * xfpm_power_translate_device_type:
 * 
 **/
const gchar *
xfpm_power_translate_device_type (guint type)
{
    switch (type)
    {
        case XFPM_DEVICE_TYPE_BATTERY:
            return _("Battery");
        case XFPM_DEVICE_TYPE_UPS:
            return _("UPS");
        case XFPM_DEVICE_TYPE_LINE_POWER:
            return _("Line power");
        case XFPM_DEVICE_TYPE_MOUSE:
            return _("Mouse");
        case XFPM_DEVICE_TYPE_KBD:
            return _("Keyboard");
	case XFPM_DEVICE_TYPE_MONITOR:
	    return _("Monitor");
	case XFPM_DEVICE_TYPE_PDA:
	    return _("PDA");
	case XFPM_DEVICE_TYPE_PHONE:
	    return _("Phone");
	case XFPM_DEVICE_TYPE_UNKNOWN:
	    return _("Unknown");
    }
    
    return _("Battery");
}

/**
 * xfpm_power_translate_technology:
 * 
 **/
const gchar *xfpm_power_translate_technology (guint value)
{
    switch (value)
    {
        case 0:
            return _("Unknown");
        case 1:
            return _("Lithium ion");
        case 2:
            return _("Lithium polymer");
        case 3:
            return _("Lithium iron phosphate");
        case 4:
            return _("Lead acid");
        case 5:
            return _("Nickel cadmium");
        case 6:
            return _("Nickel metal hybride");
    }
    
    return _("Unknown");
}

const gchar *xfpm_power_get_icon_name (guint device_type)
{
    switch (device_type)
    {
        case XFPM_DEVICE_TYPE_BATTERY:
            return XFPM_BATTERY_ICON;
        case XFPM_DEVICE_TYPE_UPS:
            return XFPM_UPS_ICON;
        case XFPM_DEVICE_TYPE_LINE_POWER:
            return XFPM_AC_ADAPTER_ICON;
        case XFPM_DEVICE_TYPE_MOUSE:
            return XFPM_MOUSE_ICON;
        case XFPM_DEVICE_TYPE_KBD:
            return XFPM_KBD_ICON;
	case XFPM_DEVICE_TYPE_MONITOR:
	    return "monitor";
	case XFPM_DEVICE_TYPE_PDA:
	    return XFPM_PDA_ICON;
	case XFPM_DEVICE_TYPE_PHONE:
	    return XFPM_PHONE_ICON;
	case XFPM_DEVICE_TYPE_UNKNOWN:
	    return XFPM_BATTERY_ICON;
    }
    
    return XFPM_BATTERY_ICON;
}


