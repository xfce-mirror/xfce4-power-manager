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




const gchar * G_GNUC_CONST
xfpm_battery_get_icon_index (UpDeviceKind type, guint percent)
{
    if (percent < 10)
    {
        return "000";
    }
    else if (percent < 30)
    {
        return "020";
    }
    else if (percent < 50)
    {
        return "040";
    }
    else if (percent < 70)
    {
        return "060";
    }
    else if (percent < 90)
    {
        return "080";
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

gchar *
xfpm_battery_get_icon_prefix_device_enum_type (UpDeviceKind type)
{
    /* mapped from
     * http://cgit.freedesktop.org/upower/tree/libupower-glib/up-types.h
     */
    if ( type == UP_DEVICE_KIND_BATTERY )
    {
	return g_strdup (XFPM_PRIMARY_ICON_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_UPS )
    {
	return g_strdup (XFPM_UPS_ICON_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_MOUSE )
    {
	return g_strdup (XFPM_MOUSE_ICON_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_KEYBOARD )
    {
	return g_strdup (XFPM_KBD_ICON_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_PHONE )
    {
	return g_strdup (XFPM_PHONE_ICON_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_PDA )
    {
	return g_strdup (XFPM_PDA_ICON_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_MEDIA_PLAYER )
    {
	return g_strdup (XFPM_MEDIA_PLAYER_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_LINE_POWER )
    {
	return g_strdup (XFPM_AC_ADAPTER_ICON);
    }
    else if ( type == UP_DEVICE_KIND_MONITOR )
    {
	return g_strdup (XFPM_MONITOR_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_TABLET )
    {
	/* Tablet ... pda, same thing :) */
	return g_strdup (XFPM_PDA_ICON_PREFIX);
    }
    else if ( type == UP_DEVICE_KIND_COMPUTER )
    {
	return g_strdup (XFPM_PRIMARY_ICON_PREFIX);
    }

    return g_strdup (XFPM_PRIMARY_ICON_PREFIX);
}

gchar*
get_device_icon_name (UpClient *upower, UpDevice *device)
{
    gchar *icon_name = NULL, *icon_prefix;
    guint type = 0, state = 0;
    gboolean online;
    gboolean present;
    gdouble percentage;

    /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
    g_object_get (device,
		  "kind", &type,
		  "state", &state,
		  "is-present", &present,
		  "percentage", &percentage,
		  "online", &online,
		   NULL);

    icon_prefix = xfpm_battery_get_icon_prefix_device_enum_type (type);

    if ( type == UP_DEVICE_KIND_LINE_POWER )
    {
	if ( online )
	{
	    icon_name = g_strdup_printf ("%s", XFPM_AC_ADAPTER_ICON);
	}
	else
	{
	    icon_name = g_strdup_printf ("%s060", XFPM_PRIMARY_ICON_PREFIX);
	}
    }
    else if ( type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS )
    {
	if (!present)
	{
	    icon_name = g_strdup_printf ("%s%s", icon_prefix, "missing");
	}
	else if (state == UP_DEVICE_STATE_FULLY_CHARGED )
	{
	    icon_name = g_strdup_printf ("%s%s", icon_prefix, "charged");
	}
	else if ( state == UP_DEVICE_STATE_CHARGING || state == UP_DEVICE_STATE_PENDING_CHARGE)
	{
	    icon_name = g_strdup_printf ("%s%s-%s", icon_prefix, xfpm_battery_get_icon_index (type, percentage), "charging");
	}
	else if ( state == UP_DEVICE_STATE_DISCHARGING || state == UP_DEVICE_STATE_PENDING_DISCHARGE)
	{
	    icon_name = g_strdup_printf ("%s%s", icon_prefix, xfpm_battery_get_icon_index (type, percentage));
	}
	else if ( state == UP_DEVICE_STATE_EMPTY)
	{
	    icon_name = g_strdup_printf ("%s%s", icon_prefix, "000");
	}
    }
    else
    {
	if ( !present || state == UP_DEVICE_STATE_EMPTY )
	{
	    icon_name = g_strdup_printf ("%s000", icon_prefix);
	}
	else if ( state == UP_DEVICE_STATE_FULLY_CHARGED )
	{
	    icon_name = g_strdup_printf ("%s100", icon_prefix);
	}
	else if ( state == UP_DEVICE_STATE_DISCHARGING || state == UP_DEVICE_STATE_CHARGING )
	{
	    icon_name = g_strdup_printf ("%s%s", icon_prefix, xfpm_battery_get_icon_index (type, percentage));
	}
    }

    return icon_name;
}
