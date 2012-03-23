/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libupower-glib/upower.h>

#include "xfpm-power.h"
#include "xfpm-dbus.h"
#include "xfpm-dpms.h"
#include "xfpm-battery.h"
#include "xfpm-xfconf.h"
#include "xfpm-notify.h"
#include "xfpm-errors.h"
#include "xfpm-network-manager.h"
#include "xfpm-icons.h"
#include "xfpm-common.h"
#include "xfpm-power-common.h"
#include "xfpm-config.h"
#include "xfpm-debug.h"
#include "xfpm-inhibit.h"
#include "xfpm-enum-types.h"
#include "egg-idletime.h"

static void     xfpm_power_finalize                (GObject *object);

static gboolean xfpm_power_dbus_suspend            (XfpmPower *power,
                                                    GError **error);
static gboolean xfpm_power_dbus_hibernate          (XfpmPower *power,
                                                    GError **error);
static gboolean xfpm_power_dbus_can_suspend        (XfpmPower *power,
                                                    gboolean *OUT_can_suspend,
                                                    GError **error);
static gboolean xfpm_power_dbus_can_hibernate      (XfpmPower *power,
                                                    gboolean *OUT_can_hibernate,
                                                    GError **error);
static gboolean xfpm_power_dbus_get_on_battery     (XfpmPower *power,
                                                    gboolean *OUT_on_battery,
                                                    GError **error);
static gboolean xfpm_power_dbus_get_low_battery    (XfpmPower *power,
                                                    gboolean *OUT_low_battery,
                                                    GError **error);

static void     xfpm_power_refresh_adaptor_visible (XfpmPower *power);


/* include generate dbus infos */
#include "xfpm-power-service-infos.h"


struct _XfpmPowerClass
{
    GObjectClass __parent__;
};

struct _XfpmPower
{
    GObjectClass __parent__;

    XfpmXfconf  *conf;
    XfpmInhibit	*inhibit;
    XfpmNotify  *notify;

    DBusGConnection *connection;

    /* UPower client */
    UpClient *up_client;

    /* UPower proxy for async calls */
    DBusGProxy *up_proxy;

    /* XfpmBattery items */
    GSList *batteries;

    /* last known lid state */
    gboolean lid_closed;

    /* last know on-battery state */
    gboolean on_battery;

    /* ac-adaptor icon */
    GtkStatusIcon *adaptor_icon;

    XfpmBatteryCharge overall_state;
    gboolean          critical_action_done;

    XfpmPowerMode    power_mode;
};

enum
{
    ON_BATTERY_CHANGED,
    LOW_BATTERY_CHANGED,
    LID_CHANGED,
    NOTIFY_SLEEP,
    NOTIFY_RESUME,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };


G_DEFINE_TYPE (XfpmPower, xfpm_power, G_TYPE_OBJECT)



static void
xfpm_power_hibernate_clicked (XfpmPower *power)
{
    GError *error = NULL;

    if (!xfpm_power_hibernate (power, TRUE, &error))
    {
        xfce_dialog_show_error (NULL, error, _("Failed to hibernate the system"));
        g_error_free (error);
    }
}

static void
xfpm_power_suspend_clicked (XfpmPower *power)
{
    GError *error = NULL;

    if (!xfpm_power_suspend (power, TRUE, &error))
    {
        xfce_dialog_show_error (NULL, error, _("Failed to suspend the system"));
        g_error_free (error);
    }
}

static void
xfpm_power_power_info_cb (gpointer data)
{
    g_spawn_command_line_async ("xfce4-power-information", NULL);
}

static void
xfpm_power_tray_exit_activated_cb (gpointer data)
{
    gboolean ret;

    ret = xfce_dialog_confirm (NULL,
                               GTK_STOCK_YES,
                               _("Quit"),
                               _("All running instances of the power manager will exit"),
                               "%s",
                                _("Quit the power manager?"));
    if ( ret )
    {
        xfpm_quit ();
    }
}

static void
xfpm_power_change_mode (XfpmPower *power, XfpmPowerMode mode)
{
    XfpmDpms *dpms;

    power->power_mode = mode;

    dpms = xfpm_dpms_new ();
    xfpm_dpms_refresh (dpms);
    g_object_unref (dpms);

    if (mode == XFPM_POWER_MODE_NORMAL)
    {
        EggIdletime *idletime;
        idletime = egg_idletime_new ();
        egg_idletime_alarm_reset_all (idletime);

        g_object_unref (idletime);
    }
}

static void
xfpm_power_normal_mode_cb (XfpmPower *power)
{
    xfpm_power_change_mode (power, XFPM_POWER_MODE_NORMAL);
}

static void
xfpm_power_presentation_mode_cb (XfpmPower *power)
{
    xfpm_power_change_mode (power, XFPM_POWER_MODE_PRESENTATION);
}

static void
xfpm_power_show_tray_menu (XfpmPower *power,
                         GtkStatusIcon *icon,
                         guint button,
                         guint activate_time,
                         gboolean show_info_item)
{
    GtkWidget *menu, *mi, *img, *subm;
    gboolean can_hibernate = FALSE;
    gboolean can_suspend = FALSE;

    menu = gtk_menu_new();

    /* Hibernate */
    mi = gtk_image_menu_item_new_with_label (_("Hibernate"));
    img = gtk_image_new_from_icon_name (XFPM_HIBERNATE_ICON, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    gtk_widget_show (mi);

    if (xfpm_power_can_hibernate (power, &can_hibernate, NULL))
    {
        g_signal_connect_swapped (G_OBJECT (mi), "activate",
                                  G_CALLBACK (xfpm_power_hibernate_clicked),
                                  power);
    }
    gtk_widget_set_sensitive (mi, can_hibernate);

    /* Suspend */
    mi = gtk_image_menu_item_new_with_label (_("Suspend"));
    img = gtk_image_new_from_icon_name (XFPM_SUSPEND_ICON, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    gtk_widget_show (mi);

    if (xfpm_power_can_suspend (power, &can_suspend, NULL))
    {
        g_signal_connect_swapped (G_OBJECT (mi), "activate",
                                  G_CALLBACK (xfpm_power_suspend_clicked),
                                  power);
    }
    gtk_widget_set_sensitive (mi, can_suspend);

/*
    saver_inhibited = xfpm_screen_saver_get_inhibit (tray->srv);
    mi = gtk_check_menu_item_new_with_label (_("Monitor power control"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), !saver_inhibited);
    gtk_widget_set_tooltip_text (mi, _("Disable or enable monitor power control, "\
                                       "for example you could disable the screen power "\
                                       "control to avoid screen blanking when watching a movie."));

    g_signal_connect (G_OBJECT (mi), "activate",
                      G_CALLBACK (xfpm_tray_icon_inhibit_active_cb), tray);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
*/

    mi = gtk_separator_menu_item_new ();
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    /* Power information */
    mi = gtk_image_menu_item_new_with_label (_("Power Information"));
    img = gtk_image_new_from_stock (GTK_STOCK_INFO, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);

    gtk_widget_set_sensitive (mi,TRUE);

    g_signal_connect_swapped (mi, "activate",
                              G_CALLBACK (xfpm_power_power_info_cb), icon);

    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    /**
     * Power Mode
     **/
    /* TRANSLATOR: Mode here is the power profile (presentation, power save, normal) */
    mi = gtk_image_menu_item_new_with_label (_("Mode"));
    img = gtk_image_new_from_icon_name (XFPM_AC_ADAPTOR_ICON, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);
    gtk_widget_set_sensitive (mi,TRUE);
    gtk_widget_show (mi);

    subm = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), subm);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    /* Normal*/
    mi = gtk_check_menu_item_new_with_label (_("Normal"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), power->power_mode == XFPM_POWER_MODE_NORMAL);
    gtk_widget_set_sensitive (mi,TRUE);

    g_signal_connect_swapped (mi, "activate",
                              G_CALLBACK (xfpm_power_normal_mode_cb), power);
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (subm), mi);

    /* Normal*/
    mi = gtk_check_menu_item_new_with_label (_("Presentation"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), power->power_mode == XFPM_POWER_MODE_PRESENTATION);
    gtk_widget_set_sensitive (mi, TRUE);

    g_signal_connect_swapped (mi, "activate",
                              G_CALLBACK (xfpm_power_presentation_mode_cb), power);
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (subm), mi);

    mi = gtk_separator_menu_item_new ();
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_HELP, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate", G_CALLBACK (xfpm_help), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate", G_CALLBACK (xfpm_about), _("Power Manager"));
    gtk_menu_shell_append (GTK_MENU_SHELL(menu), mi);

    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate",G_CALLBACK (xfpm_preferences), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    mi = gtk_separator_menu_item_new ();
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect_swapped (mi, "activate", G_CALLBACK (xfpm_power_tray_exit_activated_cb), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    g_signal_connect (menu, "selection-done",
                      G_CALLBACK (gtk_widget_destroy), NULL);

    /* Popup the menu */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   gtk_status_icon_position_menu,
                   icon, button, activate_time);

}

static void
xfpm_power_show_tray_menu_battery (GtkStatusIcon *icon, guint button,
                                   guint activate_time, XfpmPower *power)
{
    xfpm_power_show_tray_menu (power, icon, button, activate_time, TRUE);
}

static void
xfpm_power_show_tray_menu_adaptor (GtkStatusIcon *icon, guint button,
                                   guint activate_time, XfpmPower *power)
{
    xfpm_power_show_tray_menu (power, icon, button, activate_time, FALSE);
}

static XfpmBatteryCharge
xfpm_power_get_current_charge_state (XfpmPower *power)
{
    XfpmBattery *battery;
    UpDeviceKind kind;
    GSList *li;
    XfpmBatteryCharge max_charge, charge;

    max_charge = XFPM_BATTERY_CHARGE_UNKNOWN;

    for (li = power->batteries; li != NULL; li = li->next)
    {
        battery = XFPM_BATTERY (li->data);
        kind = xfpm_battery_get_kind (battery);
        if (kind == UP_DEVICE_KIND_BATTERY || kind == UP_DEVICE_KIND_UPS)
        {
            charge = xfpm_battery_get_charge (battery);
            max_charge = MAX (max_charge, charge);
        }
    }

    return max_charge;
}

static void
xfpm_power_system_on_critical_power (XfpmPower *power, XfpmBattery *battery)
{
    /*TODO
    XfpmShutdownRequest critical_action;

    g_object_get (G_OBJECT (power->conf),
                  CRITICAL_BATT_ACTION_CFG, &critical_action,
                  NULL);

    XFPM_DEBUG ("System is running on low power");
    XFPM_DEBUG_ENUM (critical_action, XFPM_TYPE_SHUTDOWN_REQUEST, "Critical battery action");

    if ( critical_action == XFPM_DO_NOTHING )
    {
        xfpm_power_show_critical_action (power, battery);
    }
    else
    {
        if (power->critical_action_done == FALSE)
        {
            power->critical_action_done = TRUE;
            xfpm_power_process_critical_action (power, critical_action);
        }
        else
        {
            xfpm_power_show_critical_action (power, battery);
        }
    }
    * */
}

static void
xfpm_power_battery_charge_changed_cb (XfpmBattery *battery, XfpmPower *power)
{
    gboolean notify;
    XfpmBatteryCharge battery_charge;
    XfpmBatteryCharge current_charge;

    battery_charge = xfpm_battery_get_charge (battery);
    current_charge = xfpm_power_get_current_charge_state (power);

    XFPM_DEBUG_ENUM (current_charge, XFPM_TYPE_BATTERY_CHARGE, "Current system charge status");

    if (current_charge == power->overall_state)
        return;

    if (current_charge >= XFPM_BATTERY_CHARGE_LOW)
        power->critical_action_done = FALSE;

    power->overall_state = current_charge;

    //TODO if ( current_charge == XFPM_BATTERY_CHARGE_CRITICAL && power->on_battery)
    {
        xfpm_power_system_on_critical_power (power, battery);

        //TODO power->on_low_battery = TRUE;
        //TODOg_signal_emit (G_OBJECT (power), signals [LOW_BATTERY_CHANGED], 0, power->on_low_battery);
        return;
    }

    //TODO if ( power->on_low_battery )
    {
        //TODO power->on_low_battery = FALSE;
        //TODOg_signal_emit (G_OBJECT (power), signals [LOW_BATTERY_CHANGED], 0, power->on_low_battery);
    }

    g_object_get (G_OBJECT (power->conf),
                  GENERAL_NOTIFICATION_CFG, &notify,
                  NULL);

    //TODO if ( power->on_battery )
    {
        if ( current_charge == XFPM_BATTERY_CHARGE_LOW )
        {
            if ( notify )
                xfpm_notify_show_notification (power->notify,
                                               _("Power Manager"),
                                               _("System is running on low power"),
                                               gtk_status_icon_get_icon_name (GTK_STATUS_ICON (battery)),
                                               10000,
                                               FALSE,
                                               XFPM_NOTIFY_NORMAL,
                                               GTK_STATUS_ICON (battery));

        }
        else if ( battery_charge == XFPM_BATTERY_CHARGE_LOW )
        {
            if ( notify )
            {
                gchar *msg;
                gchar *time_str;

                const gchar *battery_name = xfpm_battery_get_name (battery);

                time_str = xfpm_battery_get_time_left (battery);

                msg = g_strdup_printf (_("Your %s charge level is low\nEstimated time left %s"), battery_name, time_str);


                xfpm_notify_show_notification (power->notify,
                                               _("Power Manager"),
                                               msg,
                                               gtk_status_icon_get_icon_name (GTK_STATUS_ICON (battery)),
                                               10000,
                                               FALSE,
                                               XFPM_NOTIFY_NORMAL,
                                               GTK_STATUS_ICON (battery));
                g_free (msg);
                g_free (time_str);
            }
        }
    }
}

static void
xfpm_power_add_device (XfpmPower *power, UpDevice *device)
{
    UpDeviceKind kind;
    GtkStatusIcon *battery;

    g_return_if_fail (UP_IS_DEVICE (device));
    g_return_if_fail (XFPM_IS_POWER (power));

    g_object_get (device, "kind", &kind, NULL);

    XFPM_DEBUG ("Device added %s", up_device_get_object_path (device));

    switch (kind)
    {
        case UP_DEVICE_KIND_BATTERY:
        case UP_DEVICE_KIND_UPS:
        case UP_DEVICE_KIND_MOUSE:
        case UP_DEVICE_KIND_KEYBOARD:
        case UP_DEVICE_KIND_PHONE:
            battery = xfpm_battery_new (device);

            g_signal_connect (battery, "popup-menu",
                              G_CALLBACK (xfpm_power_show_tray_menu_battery), power);
            g_signal_connect (battery, "battery-charge-changed",
                              G_CALLBACK (xfpm_power_battery_charge_changed_cb), power);

            power->batteries = g_slist_prepend (power->batteries, battery);

            break;

        case UP_DEVICE_KIND_LINE_POWER:
            g_warning ("Unable to monitor unkown power device: %s",
                       up_device_get_object_path (device));
            break;

        default:
            /* other devices types we don't care about */
            break;
    }
}

static void
xfpm_power_get_power_devices (XfpmPower *power)
{
    GPtrArray *array;
    guint i;
    UpDevice *device;
    GError *error = NULL;

    if (!up_client_enumerate_devices_sync (power->up_client, NULL, &error))
    {
        g_critical ("Failed to enumerate devices: %s", error->message);
        g_error_free (error);

        return;
    }

    array = up_client_get_devices (power->up_client);

    if ( array )
    {
        for ( i = 0; i < array->len; i++)
        {
            device = g_ptr_array_index (array, i);
            g_return_if_fail (UP_IS_DEVICE (device));

            xfpm_power_add_device (power, device);
        }
        g_ptr_array_unref (array);
    }
}

static void
xfpm_power_remove_device (XfpmPower *power, UpDevice *device)
{
    GSList *li;

    XFPM_DEBUG ("Device removed %s", up_device_get_object_path (device));

    for (li = power->batteries; li != NULL; li = li->next)
    {
        if (xfpm_battery_get_device (XFPM_BATTERY (li->data)) == device)
        {
            power->batteries = g_slist_delete_link (power->batteries, li);

            g_object_unref (G_OBJECT (li->data));

            xfpm_power_refresh_adaptor_visible (power);

            break;
        }
    }
}

static void
xfpm_power_changed_cb (UpClient *up_client, XfpmPower *power)
{
    gboolean lid_closed;
    gboolean on_battery;
    GSList *li;

    g_return_if_fail (power->up_client == up_client);

    if (up_client_get_lid_is_present (power->up_client))
    {
        lid_closed = up_client_get_lid_is_closed (power->up_client);

        if (power->lid_closed != lid_closed)
        {
            /* notify the manager the lid state changed */
            power->lid_closed = lid_closed;
            g_signal_emit (G_OBJECT (power),
                           signals [LID_CHANGED],
                           0, lid_closed);
        }
    }

    on_battery = up_client_get_on_battery (power->up_client);
    if (power->on_battery != on_battery)
    {
        /* notify the system about the battery change */
        power->on_battery = on_battery;
        g_signal_emit (G_OBJECT (power),
                       signals [ON_BATTERY_CHANGED],
                       0, on_battery);

        /* tell all batteries the ac-power changed */
        for (li = power->batteries; li != NULL; li = li->next)
        {
            g_object_set (G_OBJECT (li->data),
                          "ac-online", !on_battery,
                          NULL);
        }
    }

    xfpm_power_refresh_adaptor_visible (power);
}

static void
xfpm_power_device_added_cb (UpClient *up_client, UpDevice *device, XfpmPower *power)
{
    g_return_if_fail (power->up_client == up_client);
    xfpm_power_add_device (power, device);
    xfpm_power_refresh_adaptor_visible (power);
}

static void
xfpm_power_device_removed_cb (UpClient *up_client, UpDevice *device, XfpmPower *power)
{
    g_return_if_fail (power->up_client == up_client);
    xfpm_power_remove_device (power, device);
}

static void
xfpm_power_device_changed_cb (UpClient *up_client, UpDevice *device, XfpmPower *power)
{
    g_return_if_fail (power->up_client == up_client);
    xfpm_power_refresh_adaptor_visible (power);
}

static void
xfpm_power_hide_adaptor_icon (XfpmPower *power)
{
    if ( power->adaptor_icon )
    {
        XFPM_DEBUG ("Hide adaptor icon");

        g_object_unref (power->adaptor_icon);
        power->adaptor_icon = NULL;
    }
}

static void
xfpm_power_show_adaptor_icon (XfpmPower *power)
{
    g_return_if_fail (power->adaptor_icon == NULL);

    power->adaptor_icon = gtk_status_icon_new ();

    XFPM_DEBUG ("Showing adaptor icon");

    gtk_status_icon_set_from_icon_name (power->adaptor_icon, XFPM_AC_ADAPTOR_ICON);

    gtk_status_icon_set_visible (power->adaptor_icon, TRUE);

    g_signal_connect (power->adaptor_icon, "popup-menu",
                      G_CALLBACK (xfpm_power_show_tray_menu_adaptor), power);
}

static void
xfpm_power_refresh_adaptor_visible (XfpmPower *power)
{
    XfpmShowIcon show_icon;

    g_object_get (G_OBJECT (power->conf),
                  SHOW_TRAY_ICON_CFG, &show_icon,
                  NULL);

    if ( show_icon == SHOW_ICON_ALWAYS )
    {
        if (power->batteries == NULL)
        {
            xfpm_power_show_adaptor_icon (power);

            gtk_status_icon_set_tooltip_text (power->adaptor_icon,
                                              power->on_battery ?
                                              _("Adaptor is offline") :
                                              _("Adaptor is online") );
        }
        else
        {
            xfpm_power_hide_adaptor_icon (power);
        }
    }
    else
    {
        xfpm_power_hide_adaptor_icon (power);
    }
}

static void
xfpm_power_class_init (XfpmPowerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_power_finalize;

    signals [ON_BATTERY_CHANGED] =
        g_signal_new (g_intern_static_string ("on-battery-changed"),
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [LOW_BATTERY_CHANGED] =
        g_signal_new (g_intern_static_string ("low-battery-changed"),
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [LID_CHANGED] =
        g_signal_new (g_intern_static_string ("lid-changed"),
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [NOTIFY_RESUME] =
        g_signal_new (g_intern_static_string ("notify-resume"),
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [NOTIFY_SLEEP] =
        g_signal_new (g_intern_static_string ("notify-sleep"),
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    /* install the D-BUS info for our class */
    dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
                                     &dbus_glib_xfpm_power_dbus_object_info);
}

static void
xfpm_power_init (XfpmPower *power)
{
    GError *error = NULL;
    DBusGConnection *bus;

    power->adaptor_icon = NULL;
    power->overall_state = XFPM_BATTERY_CHARGE_OK;
    power->critical_action_done = FALSE;
    power->power_mode = XFPM_POWER_MODE_NORMAL;
    power->notify = xfpm_notify_new ();
    power->inhibit = xfpm_inhibit_new ();

    /* try to connect to the session bus */
    power->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (G_LIKELY (power->connection))
    {
        /* register the /org/xfce/PowerManager object */
        dbus_g_connection_register_g_object (power->connection,
                                             "/org/xfce/PowerManagement",
                                             G_OBJECT (power));

        xfpm_dbus_register_name (dbus_g_connection_get_connection (power->connection),
                                                                   "org.xfce.PowerManagement");
    }
    else
    {
        /* notify the user that D-BUS service won't be available */
        g_printerr ("%s: Failed to connect to the D-BUS session bus: %s\n",
                    PACKAGE_NAME, error->message);
        g_clear_error (&error);
    }

    bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    if (bus)
    {
        power->up_proxy = dbus_g_proxy_new_for_name (bus,
                                                     "org.freedesktop.UPower",
                                                     "/org/freedesktop/UPower",
                                                     "org.freedesktop.UPower");
    }
    else
    {
        g_warning ("Couldn't connect to system bus: %s", error->message);
        g_error_free (error);
    }

    power->up_client = up_client_new ();

    power->lid_closed = up_client_get_lid_is_closed (power->up_client);
    power->on_battery = up_client_get_on_battery (power->up_client);

    xfpm_power_get_power_devices (power);

    g_signal_connect (power->up_client, "changed",
                      G_CALLBACK (xfpm_power_changed_cb), power);
    g_signal_connect (power->up_client, "device-removed",
                      G_CALLBACK (xfpm_power_device_removed_cb), power);
    g_signal_connect (power->up_client, "device-added",
                      G_CALLBACK (xfpm_power_device_added_cb), power);
    g_signal_connect (power->up_client, "device-changed",
                      G_CALLBACK (xfpm_power_device_changed_cb), power);

    power->conf = xfpm_xfconf_new ();
    g_signal_connect_swapped (power->conf, "notify::" SHOW_TRAY_ICON_CFG,
                              G_CALLBACK (xfpm_power_refresh_adaptor_visible), power);

    xfpm_power_refresh_adaptor_visible (power);

    if (power->on_battery)
    {
        g_signal_emit (G_OBJECT (power), signals [ON_BATTERY_CHANGED], 0, power->on_battery);
    }
}

static void
xfpm_power_finalize (GObject *object)
{
    XfpmPower *power = XFPM_POWER (object);

    if (G_LIKELY (power->connection != NULL))
        dbus_g_connection_unref (power->connection);

    if (power->up_proxy)
        g_object_unref (power->up_proxy);

    g_object_unref (power->conf);
    g_object_unref (power->up_client);
    g_object_unref (power->inhibit);

    g_slist_foreach (power->batteries, (GFunc) g_object_unref, NULL);
    g_slist_free (power->batteries);

    xfpm_power_hide_adaptor_icon (power);

    G_OBJECT_CLASS (xfpm_power_parent_class)->finalize (object);
}

static gboolean
xfpm_power_check_inhibited (XfpmPower *power,
                            const gchar *label)
{
    if (!xfpm_inhibit_get_inhibited (power->inhibit))
        return FALSE;

    return !xfce_dialog_confirm (NULL, GTK_STOCK_YES, label,
                                 _("An application is currently disabling the automatic sleep,"
                                   " doing this action now may damage the working state of this application,"
                                   " are you sure you want to hibernate the system?"),
                                 NULL);
}

static gboolean
xfpm_power_dbus_suspend (XfpmPower  *power,
                         GError    **error)
{
    return xfpm_power_suspend (power, FALSE, error);
}

static gboolean
xfpm_power_dbus_hibernate (XfpmPower  *power,
                           GError    **error)
{
    return xfpm_power_hibernate (power, FALSE, error);
}

static gboolean
xfpm_power_dbus_can_suspend (XfpmPower  *power,
                             gboolean   *OUT_can_suspend,
                             GError    **error)
{
    return xfpm_power_can_suspend (power, OUT_can_suspend, error);
}

static gboolean
xfpm_power_dbus_can_hibernate (XfpmPower  *power,
                               gboolean   *OUT_can_hibernate,
                               GError    **error)
{
    return xfpm_power_can_hibernate (power, OUT_can_hibernate, error);
}

static gboolean
xfpm_power_dbus_get_on_battery (XfpmPower  *power,
                                gboolean   *OUT_on_battery,
                                GError    **error)
{
    *OUT_on_battery = xfpm_power_get_on_battery (power);
    return TRUE;
}

static gboolean
xfpm_power_dbus_get_low_battery (XfpmPower  *power,
                                 gboolean   *OUT_low_battery,
                                 GError    **error)
{
    *OUT_low_battery = xfpm_power_get_low_battery (power);
    return TRUE;
}

static void
xfpm_power_async_upower_cb (DBusGProxy *proxy,
                            DBusGProxyCall *call,
                            gpointer user_data)
{
    GError *error = NULL;

    if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID, G_TYPE_INVALID))
    {
        xfce_dialog_show_error (NULL, error, _("Failed to suspend the system"));
        g_error_free (error);
    }
}

static gboolean
xfpm_power_async_upower (XfpmPower *power,
                         const gchar *method,
                         GError **error)
{
    DBusGProxyCall *call;

    g_return_val_if_fail (power->up_proxy != NULL, FALSE);

    call = dbus_g_proxy_begin_call (power->up_proxy,
                                    method,
                                    xfpm_power_async_upower_cb,
                                    power,
                                    G_TYPE_INVALID,
                                    G_TYPE_INVALID);

    return call != NULL;
}

XfpmPower *
xfpm_power_get (void)
{
    static gpointer xfpm_power_object = NULL;

    if ( G_LIKELY (xfpm_power_object != NULL ) )
    {
        g_object_ref (xfpm_power_object);
    }
    else
    {
        xfpm_power_object = g_object_new (XFPM_TYPE_POWER, NULL);
        g_object_add_weak_pointer (xfpm_power_object, &xfpm_power_object);
    }

    return XFPM_POWER (xfpm_power_object);
}

gboolean
xfpm_power_suspend (XfpmPower *power,
                    gboolean force,
                    GError **error)
{
    if (!force && xfpm_power_check_inhibited (power, _("Suspend")))
        return TRUE;

    XFPM_DEBUG ("Ask UPower to Suspend");

    /*return up_client_suspend_sync (power->up_client, NULL, error);*/

    return xfpm_power_async_upower (power, "Suspend", error);
}

gboolean
xfpm_power_hibernate (XfpmPower *power,
                      gboolean force,
                      GError **error)
{

    g_return_val_if_fail (power->up_proxy != NULL, FALSE);

    if (!force && xfpm_power_check_inhibited (power, _("Hibernate")))
        return TRUE;

    XFPM_DEBUG ("Ask UPower to Hibernate");

    /*return up_client_hibernate_sync (power->up_client, NULL, error);*/

    return xfpm_power_async_upower (power, "Hibernate", error);
}

gboolean
xfpm_power_can_suspend (XfpmPower  *power,
                        gboolean   *can_suspend,
                        GError    **error)
{
    if (!up_client_get_properties_sync (power->up_client, NULL, error))
        return FALSE;

    *can_suspend = up_client_get_can_suspend (power->up_client) && power->up_proxy;
    return TRUE;
}

gboolean
xfpm_power_can_hibernate (XfpmPower  *power,
                          gboolean   *can_hibernate,
                          GError    **error)
{
    if (!up_client_get_properties_sync (power->up_client, NULL, error))
        return FALSE;

    *can_hibernate = up_client_get_can_hibernate (power->up_client) && power->up_proxy;
    return TRUE;
}

gboolean
xfpm_power_get_on_battery (XfpmPower *power)
{
    g_return_val_if_fail (XFPM_IS_POWER (power), FALSE);
    return power->on_battery;
}

gboolean
xfpm_power_get_low_battery (XfpmPower *power)
{
    g_return_val_if_fail (XFPM_IS_POWER (power), FALSE);
    return FALSE /*TODO */;
}

XfpmPowerMode
xfpm_power_get_mode (XfpmPower *power)
{
    g_return_val_if_fail (XFPM_IS_POWER (power), XFPM_POWER_MODE_NORMAL);
    return power->power_mode;
}

