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
#include <upower.h>

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
	case UP_DEVICE_KIND_TABLET:
	    return _("Tablet");
	case UP_DEVICE_KIND_COMPUTER:
	    return _("Computer");
	case UP_DEVICE_KIND_UNKNOWN:
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
        case UP_DEVICE_KIND_BATTERY:
            return XFPM_BATTERY_ICON;
        case UP_DEVICE_KIND_UPS:
            return XFPM_UPS_ICON;
        case UP_DEVICE_KIND_LINE_POWER:
            return XFPM_AC_ADAPTER_ICON;
        case UP_DEVICE_KIND_MOUSE:
            return XFPM_MOUSE_ICON;
        case UP_DEVICE_KIND_KEYBOARD:
            return XFPM_KBD_ICON;
	case UP_DEVICE_KIND_MONITOR:
	    return "monitor";
	case UP_DEVICE_KIND_COMPUTER:
	    return "monitor";		/* fixme */
	case UP_DEVICE_KIND_PDA:
	    return XFPM_PDA_ICON;
	case UP_DEVICE_KIND_PHONE:
	    return XFPM_PHONE_ICON;
	case UP_DEVICE_KIND_TABLET:
	    return XFPM_PHONE_ICON;
	case UP_DEVICE_KIND_MEDIA_PLAYER:
	    return XFPM_PHONE_ICON;	/* fixme */
	case UP_DEVICE_KIND_UNKNOWN:
	    return XFPM_BATTERY_ICON;
    }

    return XFPM_BATTERY_ICON;
}

const gchar * G_GNUC_CONST
xfpm_battery_get_icon_index (UpDeviceKind type, guint percent)
{
    if (percent < 10)
    {
        return "000";
    }
    else if (percent < 30)
    {
        return ( (type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS) ? "020" : "030");
    }
    else if (percent < 50)
    {
        return ( (type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS ) ? "040" : "030");
    }
    else if (percent < 70)
    {
        return "060";
    }
    else if (percent < 90)
    {
        return ((type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS) ? "080" : "060");
    }

    return "100";
}

/*
 * Taken from gpm
 */
gchar *
xfpm_battery_get_time_string (guint seconds)
{
    char* timestring = NULL;
    gint  hours;
    gint  minutes;

    /* Add 0.5 to do rounding */
    minutes = (int) ( ( seconds / 60.0 ) + 0.5 );

    if (minutes == 0)
    {
	timestring = g_strdup (_("Unknown time"));
	return timestring;
    }

    if (minutes < 60)
    {
	timestring = g_strdup_printf (ngettext ("%i minute",
			              "%i minutes",
				      minutes), minutes);
	return timestring;
    }

    hours = minutes / 60;
    minutes = minutes % 60;

    if (minutes == 0)
	timestring = g_strdup_printf (ngettext (
			    "%i hour",
			    "%i hours",
			    hours), hours);
    else
	/* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
	 * Swap order with "%2$s %2$i %1$s %1$i if needed */
	timestring = g_strdup_printf (_("%i %s %i %s"),
			    hours, ngettext ("hour", "hours", hours),
			    minutes, ngettext ("minute", "minutes", minutes));
    return timestring;
}
