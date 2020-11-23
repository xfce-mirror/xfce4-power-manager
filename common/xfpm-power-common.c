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
#include <upower.h>

#include "xfpm-power-common.h"
#include "xfpm-enum-glib.h"

#include "xfpm-icons.h"
#include "xfpm-debug.h"


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
      return _("Uninterruptible Power Supply");
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
const gchar *
xfpm_power_translate_technology (guint value)
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
      return _("Nickel metal hydride");
  }

  return _("Unknown");
}

const gchar *
xfpm_battery_get_icon_index (guint percent)
{
  if (percent < 10)
    return "0";
  if (percent < 20)
    return "10";
  if (percent < 30)
    return "20";
  if (percent < 40)
    return "30";
  if (percent < 50)
    return "40";
  if (percent < 60)
    return "50";
  if (percent < 70)
    return "60";
  if (percent < 80)
    return "70";
  if (percent < 90)
    return "80";
  if (percent < 100)
    return "90";
  else
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

static gboolean
is_display_device (UpClient *upower, UpDevice *device)
{
  UpDevice *display_device = NULL;
  gboolean ret = FALSE;

#if UP_CHECK_VERSION(0, 99, 0)
  display_device = up_client_get_display_device (upower);
#else
  return FALSE;
#endif

  ret = g_strcmp0 (up_device_get_object_path(device), up_device_get_object_path(display_device)) == 0 ? TRUE : FALSE;

  g_object_unref (display_device);

  return ret;
}

gchar*
get_device_panel_icon_name (UpClient *upower, UpDevice *device)
{
  return get_device_icon_name (upower, device, TRUE);
}


gchar*
get_device_icon_name (UpClient *upower, UpDevice *device, gboolean is_panel)
{
  gchar *icon_name = NULL;
  gchar *icon_suffix;
  gsize icon_base_length;
  gchar *upower_icon;
  guint type = 0, state = 0;
  gdouble percentage;

  /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
  g_object_get (device,
                "kind", &type,
                "state", &state,
                "icon-name", &upower_icon,
                "percentage", &percentage,
                NULL);

  /* Strip away the symbolic suffix for the device icons for the devices tab
   * and the panel plugin's menu */
  icon_suffix = g_strrstr (upower_icon, "-symbolic");
  if (icon_suffix != NULL)
  {
    icon_base_length = icon_suffix - upower_icon;
  }
  else
  {
    icon_base_length = G_MAXINT;
  }

  XFPM_DEBUG ("icon_suffix %s, icon_base_length %ld, upower_icon %s",
              icon_suffix, icon_base_length, upower_icon);

  /* mapped from
   * http://cgit.freedesktop.org/upower/tree/libupower-glib/up-types.h
   * because UPower doesn't return device-specific icon-names
   */
  if ( type == UP_DEVICE_KIND_BATTERY && is_panel )
  {
    if ( state == UP_DEVICE_STATE_CHARGING || state == UP_DEVICE_STATE_PENDING_CHARGE)
      icon_name = g_strdup_printf ("%s-%s-%s", XFPM_BATTERY_LEVEL_ICON, xfpm_battery_get_icon_index (percentage), "charging-symbolic");
    else if ( state == UP_DEVICE_STATE_DISCHARGING || state == UP_DEVICE_STATE_PENDING_DISCHARGE)
      icon_name = g_strdup_printf ("%s-%s-%s", XFPM_BATTERY_LEVEL_ICON, xfpm_battery_get_icon_index (percentage), "symbolic");
    else if ( state == UP_DEVICE_STATE_FULLY_CHARGED)
      icon_name = g_strdup_printf ("%s-%s", XFPM_BATTERY_LEVEL_ICON, "100-charged-symbolic");
    else
      icon_name = g_strdup ("battery-missing-symbolic");
  }
  else if ( type == UP_DEVICE_KIND_UPS )
    icon_name = g_strdup (XFPM_UPS_ICON);
  else if ( type == UP_DEVICE_KIND_MOUSE )
    icon_name = g_strdup (XFPM_MOUSE_ICON);
  else if ( type == UP_DEVICE_KIND_KEYBOARD )
    icon_name = g_strdup (XFPM_KBD_ICON);
  else if ( type == UP_DEVICE_KIND_PHONE )
    icon_name = g_strdup (XFPM_PHONE_ICON);
  else if ( type == UP_DEVICE_KIND_PDA )
    icon_name = g_strdup (XFPM_PDA_ICON);
  else if ( type == UP_DEVICE_KIND_MEDIA_PLAYER )
    icon_name = g_strdup (XFPM_MEDIA_PLAYER_ICON);
  else if ( type == UP_DEVICE_KIND_LINE_POWER )
    icon_name = g_strdup_printf (is_panel ? "%s-%s", XFPM_AC_ADAPTER_ICON, "-symbolic" : "%s", XFPM_AC_ADAPTER_ICON);
  else if ( type == UP_DEVICE_KIND_MONITOR )
    icon_name = g_strdup (XFPM_MONITOR_ICON);
  else if ( type == UP_DEVICE_KIND_TABLET )
    icon_name = g_strdup (XFPM_TABLET_ICON);
  else if ( type == UP_DEVICE_KIND_COMPUTER )
    icon_name = g_strdup (XFPM_COMPUTER_ICON);
  /* As UPower does not tell us whether a system is a desktop or a laptop we
     decide this based on whether there is a battery and/or a a lid */
  else if (!up_client_get_lid_is_present (upower) &&
           !up_client_get_on_battery (upower) &&
           g_strcmp0 (upower_icon, "battery-missing-symbolic") == 0)
    icon_name = g_strdup_printf (is_panel ? "%s-%s", XFPM_AC_ADAPTER_ICON, "-symbolic" : "%s", XFPM_AC_ADAPTER_ICON);
  else if ( g_strcmp0 (upower_icon, "") != 0 )
    icon_name = g_strndup (upower_icon, icon_base_length);

  return icon_name;
}

gchar*
get_device_description (UpClient *upower, UpDevice *device)
{
  gchar *tip = NULL;
  gchar *est_time_str = NULL;
  guint type = 0, state = 0;
  gchar *model = NULL, *vendor = NULL;
  gboolean present, online;
  gdouble percentage;
  guint64 time_to_empty, time_to_full;

  /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
  g_object_get (device,
                "kind", &type,
                "vendor", &vendor,
                "model", &model,
                "state", &state,
                "is-present", &present,
                "percentage", &percentage,
                "time-to-empty", &time_to_empty,
                "time-to-full", &time_to_full,
                "online", &online,
                 NULL);

  if (is_display_device (upower, device))
  {
    g_free (vendor);
    vendor = g_strdup (_("Computer"));

    g_free (model);
    model = g_strdup ("");
  }

  if (vendor == NULL)
    vendor = g_strdup ("");
  else
    vendor = g_strstrip (vendor);
  if (model == NULL)
    model = g_strdup ("");
  else
    model = g_strstrip (model);

  /* If we get a vendor or model we can use it, otherwise translate the
   * device type into something readable (works for things like ac_power)
   */
  if (g_strcmp0(vendor, "") == 0 && g_strcmp0(model, "") == 0)
    vendor = g_strdup_printf ("%s", xfpm_power_translate_device_type (type));

  /* If the device is unknown to the kernel (maybe no-name stuff or
   * whatever), then vendor and model will have a hex ID of 31
   * characters each. We do not want to show them, they are neither
   * useful nor human-readable, so translate and use the device
   * type instead of the hex IDs (see bug #11217).
   */
  else if (strlen(vendor) == 31 && strlen(model) == 31)
  {
    g_free (vendor);
    g_free (model);
    vendor = g_strdup_printf ("%s", xfpm_power_translate_device_type (type));
    model = g_strdup("");
  }

  if ( state == UP_DEVICE_STATE_FULLY_CHARGED )
  {
    if ( time_to_empty > 0 )
    {
      est_time_str = xfpm_battery_get_time_string (time_to_empty);
      tip = g_strdup_printf (_("<b>%s %s</b>\nFully charged (%0.0f%%, %s runtime)"),
                             vendor, model,
                             percentage,
                             est_time_str);
      g_free (est_time_str);
    }
    else
    {
      tip = g_strdup_printf (_("<b>%s %s</b>\nFully charged (%0.0f%%)"),
                             vendor, model,
                             percentage);
    }
  }
  else if ( state == UP_DEVICE_STATE_CHARGING )
  {
    if ( time_to_full != 0 )
    {
      est_time_str = xfpm_battery_get_time_string (time_to_full);
      tip = g_strdup_printf (_("<b>%s %s</b>\nCharging (%0.0f%%, %s)"),
                             vendor, model,
                             percentage,
                             est_time_str);
      g_free (est_time_str);
    }
    else
    {
      tip = g_strdup_printf (_("<b>%s %s</b>\nCharging (%0.0f%%)"),
                             vendor, model,
                             percentage);
    }
  }
  else if ( state == UP_DEVICE_STATE_DISCHARGING )
  {
    if ( time_to_empty != 0 )
    {
      est_time_str = xfpm_battery_get_time_string (time_to_empty);
      tip = g_strdup_printf (_("<b>%s %s</b>\nDischarging (%0.0f%%, %s)"),
                             vendor, model,
                             percentage,
                             est_time_str);
      g_free (est_time_str);
    }
    else
    {
      tip = g_strdup_printf (_("<b>%s %s</b>\nDischarging (%0.0f%%)"),
                             vendor, model,
                             percentage);
    }
  }
  else if ( state == UP_DEVICE_STATE_PENDING_CHARGE )
  {
    tip = g_strdup_printf (_("<b>%s %s</b>\nWaiting to discharge (%0.0f%%)"),
                           vendor, model,
                           percentage);
  }
  else if ( state == UP_DEVICE_STATE_PENDING_DISCHARGE )
  {
    tip = g_strdup_printf (_("<b>%s %s</b>\nWaiting to charge (%0.0f%%)"),
                           vendor, model,
                           percentage);
  }
  else if ( state == UP_DEVICE_STATE_EMPTY )
  {
    tip = g_strdup_printf (_("<b>%s %s</b>\nis empty"),
                           vendor, model);
  }
  else if ( state == UP_DEVICE_STATE_UNKNOWN && percentage != 0.0 )
  {
    tip = g_strdup_printf (_("<b>%s %s</b>\nCurrent charge: %0.0f%%"),
                           vendor, model,
                           percentage);
  }
  else
  {
    if (type == UP_DEVICE_KIND_LINE_POWER)
    {
      /* On the 2nd line we want to know if the power cord is plugged
       * in or not */
      tip = g_strdup_printf (_("<b>%s %s</b>\n%s"),
                     vendor, model, online ? _("Plugged in") : _("Not plugged in"));
    }
    else if (is_display_device (upower, device))
    {
      /* Desktop pc with no battery, just display the vendor and model,
       * which will probably just be Computer */
      tip = g_strdup_printf (_("<b>%s %s</b>"), vendor, model);
    }
    else
    {
      /* unknown device state, just display the percentage */
      tip = g_strdup_printf (_("<b>%s %s</b>\nUnknown state"),
                     vendor, model);
    }
  }

  g_free(model);
  g_free(vendor);

  return tip;
}
