/*
 * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
 * Copyright (C) 2012      Nick Schermer <nick@xfce.org>
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

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libupower-glib/upower.h>

#include "xfpm-battery.h"
#include "xfpm-dbus.h"
#include "xfpm-icons.h"
#include "xfpm-xfconf.h"
#include "xfpm-notify.h"
#include "xfpm-config.h"
#include "xfpm-button.h"
#include "xfpm-enum-glib.h"
#include "xfpm-enum-types.h"
#include "xfpm-debug.h"
#include "xfpm-power-common.h"
#include "xfpm-common.h"

#define KIND_IS_BATTERY_OR_UPS(kind) ((kind) == UP_DEVICE_KIND_BATTERY || (kind) == UP_DEVICE_KIND_UPS)



static void xfpm_battery_finalize   (GObject *object);



struct _XfpmBatteryClass
{
    GtkStatusIconClass __parent__;
};

struct _XfpmBattery
{
    GtkStatusIcon __parent__;

    UpDevice               *device;
    UpDeviceKind            device_kind;
    UpDeviceState           device_state;

    /* translated device name */
    const gchar            *device_name;

    /* icon prefix for this device kind */
    const gchar            *icon_prefix;

    gboolean                ac_online;
    XfpmBatteryCharge       charge;

    XfpmXfconf             *conf;
    XfpmNotify             *notify;
    XfpmButton             *button;

    gulong                  sig;
    gulong                  sig_bt;
};

enum
{
    PROP_0,
    PROP_AC_ONLINE,
    PROP_CHARGE_STATUS
};

enum
{
    BATTERY_CHARGE_CHANGED,
    LAST_SIGNAL
};



static guint signals [LAST_SIGNAL] = { 0 };



G_DEFINE_TYPE (XfpmBattery, xfpm_battery, GTK_TYPE_STATUS_ICON)



static const gchar *
xfpm_battery_get_icon_index (UpDeviceKind kind, gdouble percentage)
{
    if (KIND_IS_BATTERY_OR_UPS (kind))
    {
        if (percentage < 20)
            return "000";
        else if (percentage < 40)
            return "020";
        else if (percentage < 60)
            return "040";
        else if (percentage < 80)
            return "060";
        else if (percentage < 100)
            return "080";
        else
            return "100";
    }
    else
    {
        if (percentage < 30)
            return "000";
        else if (percentage < 60)
            return "030";
        else if (percentage < 100)
            return "060";
        else
            return "100";
    }
}

static void
xfpm_battery_refresh_visible (XfpmBattery *battery)
{
    XfpmShowIcon show_icon;
    gboolean visible;
    gboolean is_present;

    g_object_get (battery->device,
                  "is-present", &is_present,
                  NULL);

    g_object_get (G_OBJECT (battery->conf),
                  SHOW_TRAY_ICON_CFG, &show_icon,
                  NULL);

    if ( show_icon == SHOW_ICON_ALWAYS )
        visible = TRUE;
    else if ( show_icon == NEVER_SHOW_ICON )
        visible = FALSE;
    else if ( show_icon == SHOW_ICON_WHEN_BATTERY_PRESENT )
        visible = is_present;
    else if ( show_icon == SHOW_ICON_WHEN_BATTERY_CHARGING_DISCHARGING )
        visible = battery->device_state != UP_DEVICE_STATE_FULLY_CHARGED;
    else
        visible = FALSE;

    XFPM_DEBUG_ENUM (show_icon, XFPM_TYPE_SHOW_ICON, "visible=%s", xfpm_bool_to_string (visible));

    gtk_status_icon_set_visible (GTK_STATUS_ICON (battery), visible);
}


/*
 * Taken from gpm
 */
static gchar *
xfpm_battery_get_time_string (gint64 seconds)
{
    gint hours;
    gint minutes;

    /* Add 0.5 to do rounding */
    minutes = (int) ( ( seconds / 60.0 ) + 0.5 );

    if (minutes == 0)
    {
        return g_strdup (_("Unknown time"));
    }

    if (minutes < 60)
    {
        return g_strdup_printf (ngettext ("%i minute",
                                          "%i minutes",
                                          minutes), minutes);
    }

    hours = minutes / 60;
    minutes = minutes % 60;

    if (minutes == 0)
    {
        return g_strdup_printf (ngettext ("%i hour", "%i hours", hours), hours);
    }
    else
    {
        /* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
         * Swap order with "%2$s %2$i %1$s %1$i if needed */
        return g_strdup_printf (_("%i %s %i %s"),
                                hours, ngettext ("hour", "hours", hours),
                                minutes, ngettext ("minute", "minutes", minutes));
    }
}

static gchar *
xfpm_battery_get_message_from_battery_state (XfpmBattery *battery)
{
    gchar *msg = NULL;
    guint64 time_to_full;
    gchar *tmp;
    gint64 time_to_empty;
    gdouble percentage;
    gchar *est_time_str;

    if (KIND_IS_BATTERY_OR_UPS (battery->device_kind))
    {
        switch (battery->device_state)
        {
            case UP_DEVICE_STATE_FULLY_CHARGED:
                msg = g_strdup_printf (_("Your %s is fully charged"), battery->device_name);
                break;

            case UP_DEVICE_STATE_CHARGING:
                msg = g_strdup_printf (_("Your %s is charging"), battery->device_name);

                g_object_get (battery->device,
                              "time-to-full", &time_to_full,
                              "percentage", &percentage,
                              NULL);
                
                if ( time_to_full > 0 )
                {
                    est_time_str = xfpm_battery_get_time_string (time_to_full);
                    tmp = msg;

                    msg = g_strdup_printf (_("%s (%i%%)\n%s until is fully charged."),
                                           tmp, (gint) percentage, est_time_str);

                    g_free (est_time_str);
                    g_free (tmp);
                }

                break;

            case UP_DEVICE_STATE_DISCHARGING:
                if (battery->ac_online)
                    msg =  g_strdup_printf (_("Your %s is discharging"), battery->device_name);
                else
                    msg =  g_strdup_printf (_("System is running on %s power"), battery->device_name);

                g_object_get (battery->device,
                              "time-to-empty", &time_to_empty,
                              "percentage", &percentage,
                              NULL);

                if ( time_to_empty > 0 )
                {
                    est_time_str = xfpm_battery_get_time_string (time_to_empty);
                    tmp = msg;

                    msg = g_strdup_printf (_("%s (%i%%)\nEstimated time left is %s."),
                                           tmp, (gint) percentage, est_time_str);

                    g_free (tmp);
                    g_free (est_time_str);
                }
                break;

            case UP_DEVICE_STATE_EMPTY:
                msg = g_strdup_printf (_("Your %s is empty"), battery->device_name);
                break;

            default:
                break;
        }

    }
    else if (battery->device_kind == UP_DEVICE_KIND_MONITOR)
    {
        switch (battery->device_state)
        {
            case UP_DEVICE_STATE_FULLY_CHARGED:
                msg = g_strdup_printf (_("Your %s is fully charged"), battery->device_name);
                break;
                
            case UP_DEVICE_STATE_CHARGING:
                msg = g_strdup_printf (_("Your %s is charging"), battery->device_name);
                break;
                
            case UP_DEVICE_STATE_DISCHARGING:
                msg =  g_strdup_printf (_("Your %s is discharging"), battery->device_name);
                break;
                
            case UP_DEVICE_STATE_EMPTY:
                msg = g_strdup_printf (_("Your %s is empty"), battery->device_name);
                break;
                
            default:
                break;
        }
    }

    return msg;
}

static void
xfpm_battery_refresh_icon (XfpmBattery *battery)
{
    gchar icon_name[128];
    gboolean is_present;
    gdouble percentage;

    XFPM_DEBUG ("Battery state %d", battery->device_state);

    g_snprintf (icon_name, sizeof (icon_name), GTK_STOCK_MISSING_IMAGE);

    g_object_get (battery->device,
                  "is-present", &is_present,
                  "percentage", &percentage,
                  NULL);

    if (KIND_IS_BATTERY_OR_UPS (battery->device_kind))
    {
        if (!is_present)
        {
            g_snprintf (icon_name, sizeof (icon_name), "%s%s",
                        battery->icon_prefix, "missing");
        }
        else
        {
            switch (battery->device_state)
            {
                case UP_DEVICE_STATE_FULLY_CHARGED:
                    g_snprintf (icon_name, sizeof (icon_name), "%s%s",
                                battery->icon_prefix,
                                battery->ac_online ? "charged" : "100");
                    break;

                case UP_DEVICE_STATE_CHARGING:
                case UP_DEVICE_STATE_PENDING_CHARGE:
                    g_snprintf (icon_name, sizeof (icon_name), "%s%s-%s",
                                battery->icon_prefix,
                                xfpm_battery_get_icon_index (battery->device_kind, percentage),
                                "charging");
                    break;

                case UP_DEVICE_STATE_DISCHARGING:
                case UP_DEVICE_STATE_PENDING_DISCHARGE:
                    g_snprintf (icon_name, sizeof (icon_name), "%s%s",
                                battery->icon_prefix,
                                xfpm_battery_get_icon_index (battery->device_kind, percentage));
                    break;

                case UP_DEVICE_STATE_EMPTY:
                    g_snprintf (icon_name, sizeof (icon_name), "%s%s",
                                battery->icon_prefix,
                                battery->ac_online ? "000-charging" : "000");
                    break;

                default:
                    g_assert_not_reached ();
                    break;
            }
        }
    }
    else
    {
        if (!is_present || battery->device_state == UP_DEVICE_STATE_EMPTY)
        {
            g_snprintf (icon_name, sizeof (icon_name), "%s-000",
                        battery->icon_prefix);
        }
        else if (battery->device_state == UP_DEVICE_STATE_FULLY_CHARGED)
        {
            g_snprintf (icon_name, sizeof (icon_name), "%s-100",
                        battery->icon_prefix);
        }
        else if (battery->device_state == UP_DEVICE_STATE_DISCHARGING)
        {
            g_snprintf (icon_name, sizeof (icon_name), "%s-%s",
                        battery->icon_prefix,
                        xfpm_battery_get_icon_index (battery->device_kind, percentage));
        }
    }

    gtk_status_icon_set_from_icon_name (GTK_STATUS_ICON (battery), icon_name);
}

static void
xfpm_battery_notify (XfpmBattery *battery)
{
    gchar *message = NULL;

    message = xfpm_battery_get_message_from_battery_state (battery);

    if ( !message )
        return;

    xfpm_notify_show_notification (battery->notify,
                                   _("Power Manager"),
                                   message,
                                   gtk_status_icon_get_icon_name (GTK_STATUS_ICON (battery)),
                                   8000,
                                   FALSE,
                                   XFPM_NOTIFY_NORMAL,
                                   GTK_STATUS_ICON (battery));

    g_free (message);
}

static gboolean
xfpm_battery_notify_idle (gpointer data)
{
    XfpmBattery *battery = XFPM_BATTERY (data);

    xfpm_battery_notify (battery);

    return FALSE;
}

static void
xfpm_battery_notify_state (XfpmBattery *battery)
{
    gboolean notify;
    static gboolean starting_up = TRUE;

    if ( !gtk_status_icon_get_visible (GTK_STATUS_ICON (battery)) )
        return;

    if ( starting_up )
    {
        starting_up = FALSE;
        return;
    }

    if (KIND_IS_BATTERY_OR_UPS (battery->device_kind))
    {
        g_object_get (G_OBJECT (battery->conf),
                      GENERAL_NOTIFICATION_CFG, &notify,
                      NULL);

        if ( notify )
        {
            g_idle_add ((GSourceFunc) xfpm_battery_notify_idle, battery);
        }
    }
}

/*
 * Refresh tooltip function for UPS and battery device only.
 */
static void
xfpm_battery_set_tooltip_primary (XfpmBattery *battery, GtkTooltip *tooltip)
{
    gchar *tip;
    gchar *est_time_str;
    const gchar *power_status;
    gdouble percentage;
    gint64 time_to_empty;
    gint64 time_to_full;

    g_object_get (battery->device,
                  "percentage", &percentage,
                  "time-to-empty", &time_to_empty,
                  "time-to-full", &time_to_full,
                  NULL);

    if ( battery->ac_online )
        power_status = _("Adaptor is online");
    else
        power_status = _("System is running on battery power");

    if ( battery->device_state == UP_DEVICE_STATE_FULLY_CHARGED )
    {
        if ( time_to_empty > 0 )
        {
            est_time_str = xfpm_battery_get_time_string (time_to_empty);
            tip = g_strdup_printf (_("%s\nYour %s is fully charged (%i%%).\nProvides %s runtime"),
                                   power_status,
                                   battery->device_name,
                                   (gint) percentage,
                                   est_time_str);
            g_free (est_time_str);
        }
        else
        {
            tip = g_strdup_printf (_("%s\nYour %s is fully charged (%i%%)."),
                                   power_status,
                                   battery->device_name,
                                   (gint) percentage);
        }
    }
    else if ( battery->device_state == UP_DEVICE_STATE_CHARGING )
    {
        if ( time_to_full > 0 )
        {
            est_time_str = xfpm_battery_get_time_string (time_to_full);
            tip = g_strdup_printf (_("%s\nYour %s is charging (%i%%)\n%s until is fully charged."),
                                   power_status,
                                   battery->device_name,
                                   (gint) percentage,
                                   est_time_str);
            g_free (est_time_str);
        }
        else
        {
            tip = g_strdup_printf (_("%s\nYour %s is charging (%i%%)."),
                                   power_status,
                                   battery->device_name,
                                   (gint) percentage);
        }
    }
    else if ( battery->device_state == UP_DEVICE_STATE_DISCHARGING )
    {
        if ( time_to_empty > 0 )
        {
            est_time_str = xfpm_battery_get_time_string (time_to_empty);
            tip = g_strdup_printf (_("%s\nYour %s is discharging (%i%%)\nEstimated time left is %s."),
                                   power_status,
                                   battery->device_name,
                                   (gint) percentage,
                                   est_time_str);
            g_free (est_time_str);
        }
        else
        {
            tip = g_strdup_printf (_("%s\nYour %s is discharging (%i%%)."),
                                   power_status,
                                   battery->device_name,
                                   (gint) percentage);
        }

    }
    else if ( battery->device_state == UP_DEVICE_STATE_PENDING_CHARGE )
    {
        tip = g_strdup_printf (_("%s\n%s waiting to discharge (%i%%)."),
                               power_status, battery->device_name,
                               (gint) percentage);
    }
    else if ( battery->device_state == UP_DEVICE_STATE_PENDING_DISCHARGE )
    {
        tip = g_strdup_printf (_("%s\n%s waiting to charge (%i%%)."),
                               power_status,
                               battery->device_name,
                               (gint) percentage);
    }
    else
    {
        tip = g_strdup_printf (_("%s\nYour %s is empty"),
                               power_status,
                               battery->device_name);
    }

    gtk_tooltip_set_text (tooltip, tip);
    g_free (tip);
}

static void
xfpm_battery_check_charge (XfpmBattery *battery)
{
    XfpmBatteryCharge charge;
    guint critical_level, low_level;
    gdouble percentage;
    

    g_object_get (G_OBJECT (battery->conf),
                  CRITICAL_POWER_LEVEL, &critical_level,
                  NULL);

    low_level = critical_level + 10;
    
    g_object_get (battery->device,
                  "percentage", &percentage,
                  NULL);

    if ( percentage > low_level )
        charge = XFPM_BATTERY_CHARGE_OK;
    else if ( percentage <= low_level && percentage > critical_level )
        charge = XFPM_BATTERY_CHARGE_LOW;
    else if ( percentage <= critical_level )
        charge = XFPM_BATTERY_CHARGE_CRITICAL;

    if ( charge != battery->charge)
    {
        battery->charge = charge;
        /*
         * only emit signal when when battery charge changes from ok->low->critical
         * and not the other way round.
         */
        if ( battery->charge != XFPM_BATTERY_CHARGE_CRITICAL || charge != XFPM_BATTERY_CHARGE_LOW )
            g_signal_emit (G_OBJECT (battery), signals [BATTERY_CHARGE_CHANGED], 0);
    }
}

static void
xfpm_battery_button_pressed_cb (XfpmButton *button, XfpmButtonKey type, XfpmBattery *battery)
{
    if (type == BUTTON_BATTERY)
        xfpm_battery_notify (battery);
}

static void
xfpm_battery_changed_cb (UpDevice *device, XfpmBattery *battery)
{
    UpDeviceState state;

    g_return_if_fail (battery->device == device);

    g_object_get (battery->device, "state", &state, NULL);

    if ( state != battery->device_state )
    {
        battery->device_state = state;
        xfpm_battery_refresh_visible (battery);
        xfpm_battery_notify_state (battery);
    }

    xfpm_battery_check_charge (battery);

    xfpm_battery_refresh_icon (battery);
}

static gboolean
xfpm_battery_query_tooltip (GtkStatusIcon *icon,
                            gint x,
                            gint y,
                            gboolean keyboard_mode,
                            GtkTooltip *tooltip)
{
    XfpmBattery *battery;

    battery = XFPM_BATTERY (icon);

    if ( KIND_IS_BATTERY_OR_UPS (battery->device_kind) )
    {
        xfpm_battery_set_tooltip_primary (battery, tooltip);
        return TRUE;
    }

    return FALSE;
}

static void
xfpm_battery_tray_icon_settings_changed (GObject *obj, GParamSpec *spec, XfpmBattery *battery)
{
    xfpm_battery_refresh_visible (battery);
}

static void xfpm_battery_get_property (GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
    XfpmBattery *battery;

    battery = XFPM_BATTERY (object);

    switch (prop_id)
    {
        case PROP_AC_ONLINE:
            g_value_set_boolean (value, battery->ac_online);
            break;
        case PROP_CHARGE_STATUS:
            g_value_set_enum (value, battery->charge);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void xfpm_battery_set_property (GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
    XfpmBattery *battery;

    battery = XFPM_BATTERY (object);

    switch (prop_id)
    {
        case PROP_AC_ONLINE:
            battery->ac_online = g_value_get_boolean (value);
            xfpm_battery_refresh_icon (battery);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


static void
xfpm_battery_class_init (XfpmBatteryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkStatusIconClass *status_icon_class = GTK_STATUS_ICON_CLASS (klass);

    object_class->finalize = xfpm_battery_finalize;
    object_class->get_property = xfpm_battery_get_property;
    object_class->set_property = xfpm_battery_set_property;

    status_icon_class->query_tooltip = xfpm_battery_query_tooltip;

    signals [BATTERY_CHARGE_CHANGED] =
        g_signal_new ("battery-charge-changed",
                      XFPM_TYPE_BATTERY,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    g_object_class_install_property (object_class,
                                     PROP_AC_ONLINE,
                                     g_param_spec_boolean("ac-online",
                                                          NULL, NULL,
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                                     PROP_CHARGE_STATUS,
                                     g_param_spec_enum ("charge-status",
                                                        NULL, NULL,
                                                        XFPM_TYPE_BATTERY_CHARGE,
                                                        XFPM_BATTERY_CHARGE_UNKNOWN,
                                                        G_PARAM_READABLE));
}

static void
xfpm_battery_init (XfpmBattery *battery)
{
    battery->conf = xfpm_xfconf_new ();
    battery->notify = xfpm_notify_new ();
    battery->device_state = UP_DEVICE_STATE_UNKNOWN;
    battery->device_kind = UP_DEVICE_KIND_UNKNOWN;
    battery->charge = XFPM_BATTERY_CHARGE_UNKNOWN;
    battery->button = xfpm_button_new ();
    battery->ac_online = TRUE;

    battery->sig = g_signal_connect (G_OBJECT (battery->conf), "notify::" SHOW_TRAY_ICON_CFG,
                                     G_CALLBACK (xfpm_battery_tray_icon_settings_changed), battery);


    battery->sig_bt = g_signal_connect (G_OBJECT (battery->button), "button-pressed",
                                        G_CALLBACK (xfpm_battery_button_pressed_cb), battery);
}

static void
xfpm_battery_finalize (GObject *object)
{
    XfpmBattery *battery = XFPM_BATTERY (object);

    g_signal_handlers_disconnect_by_func (battery->device,
        G_CALLBACK (xfpm_battery_changed_cb), battery);

    if ( g_signal_handler_is_connected (battery->conf, battery->sig ) )
        g_signal_handler_disconnect (G_OBJECT (battery->conf), battery->sig);

     if ( g_signal_handler_is_connected (battery->button, battery->sig_bt ) )
        g_signal_handler_disconnect (G_OBJECT (battery->button), battery->sig_bt);

    g_object_unref (battery->conf);
    g_object_unref (battery->notify);
    g_object_unref (battery->button);
    g_object_unref (battery->device);

    G_OBJECT_CLASS (xfpm_battery_parent_class)->finalize (object);
}

static const gchar *
xfpm_battery_get_icon_prefix (UpDeviceKind kind)
{
    switch (kind)
    {
        case UP_DEVICE_KIND_BATTERY:
            return XFPM_PRIMARY_ICON_PREFIX;

        case UP_DEVICE_KIND_UPS:
            return XFPM_UPS_ICON_PREFIX;

        case UP_DEVICE_KIND_MOUSE:
            return XFPM_MOUSE_ICON_PREFIX;

        case UP_DEVICE_KIND_KEYBOARD:
            return XFPM_KBD_ICON_PREFIX;

        case UP_DEVICE_KIND_PHONE:
            return XFPM_PHONE_ICON_PREFIX;

        default:
            return XFPM_PRIMARY_ICON_PREFIX;
    }
}

static const gchar *
xfpm_battery_get_kind_name (UpDeviceKind kind)
{
    switch (kind)
    {
        case UP_DEVICE_KIND_BATTERY:
            return _("battery");

        case UP_DEVICE_KIND_UPS:
            return _("UPS");

        case UP_DEVICE_KIND_MONITOR:
            return _("monitor battery");

        case UP_DEVICE_KIND_MOUSE:
            return _("mouse battery");

        case UP_DEVICE_KIND_KEYBOARD:
            return _("keyboard battery");

        case UP_DEVICE_KIND_PDA:
            return _("PDA battery");

        case UP_DEVICE_KIND_PHONE:
            return _("Phone battery");

        default:
            return _("Unknown");
    }
}

GtkStatusIcon *
xfpm_battery_new (UpDevice *device)
{
    XfpmBattery *battery = NULL;
    UpDeviceKind device_kind;

    battery = g_object_new (XFPM_TYPE_BATTERY,
                            "has-tooltip", TRUE, NULL);

    g_object_get (device, "kind", &device_kind, NULL);

    battery->device = g_object_ref (device);
    battery->device_kind = device_kind;
    battery->device_name = xfpm_battery_get_kind_name (device_kind);
    battery->icon_prefix = xfpm_battery_get_icon_prefix (device_kind);

    g_signal_connect (device, "changed",
                      G_CALLBACK (xfpm_battery_changed_cb), battery);
    xfpm_battery_changed_cb (device, battery);

    return GTK_STATUS_ICON (battery);
}

UpDeviceKind
xfpm_battery_get_kind (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY (battery), UP_DEVICE_KIND_UNKNOWN );

    return battery->device_kind;
}

XfpmBatteryCharge
xfpm_battery_get_charge (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY (battery), XFPM_BATTERY_CHARGE_UNKNOWN);

    return battery->charge;
}

const gchar *
xfpm_battery_get_name (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY (battery), NULL);

    return battery->device_name;
}

gchar *
xfpm_battery_get_time_left (XfpmBattery *battery)
{
    gint64 time_to_empty;

    g_return_val_if_fail (XFPM_IS_BATTERY (battery), NULL);

    g_object_get (battery->device, "time-to-empty", &time_to_empty, NULL);

    return xfpm_battery_get_time_string (time_to_empty);
}
