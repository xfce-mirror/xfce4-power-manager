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
#include "xfpm-enum-types.h"
#include "egg-idletime.h"

static void xfpm_power_finalize     (GObject *object);

static void xfpm_power_refresh_adaptor_visible (XfpmPower *power);

struct _XfpmPowerClass
{
    GObjectClass __parent__;
};

struct _XfpmPower
{
    GObjectClass __parent__;
    
    UpClient *up_client;
    
    GSList   *devices;

    XfpmXfconf      *conf;
    GtkStatusIcon   *adapter_icon;
    
    XfpmBatteryCharge overall_state;
    gboolean          critical_action_done;
    
    XfpmPowerMode    power_mode;
    EggIdletime     *idletime;
    
    XfpmNotify      *notify;
    
    /**
     * Warning dialog to use when notification daemon 
     * doesn't support actions.
     **/
    GtkWidget       *dialog;
};

enum
{
    ON_BATTERY_CHANGED,
    LOW_BATTERY_CHANGED,
    LID_CHANGED,
    WAKING_UP,
    SLEEPING,
    ASK_SHUTDOWN,
    SHUTDOWN,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (XfpmPower, xfpm_power, G_TYPE_OBJECT)



static void
xfpm_power_report_error (XfpmPower *power, const gchar *error, const gchar *icon_name)
{
    GtkStatusIcon *battery = NULL;
    guint i, len;
    GList *list;
    
    list = g_hash_table_get_values (power->hash);
    len = g_list_length (list);
    
    for ( i = 0; i < len; i++)
    {
        XfpmDeviceType type;
        battery = g_list_nth_data (list, i);
        type = xfpm_battery_get_device_type (XFPM_BATTERY (battery));
        if ( type == XFPM_DEVICE_TYPE_BATTERY ||
             type == XFPM_DEVICE_TYPE_UPS )
             break;
    }
    
    xfpm_notify_show_notification (power->notify, 
                                   _("Power Manager"), 
                                   error, 
                                   icon_name,
                                   10000,
                                   FALSE,
                                   XFPM_NOTIFY_CRITICAL,
                                   battery);
    
}

static void
xfpm_power_sleep (XfpmPower *power, const gchar *sleep_time, gboolean force)
{
    GError *error = NULL;
    gboolean lock_screen;
    
    if (!force)
    {
        gboolean ret;
        
        ret = xfce_dialog_confirm (NULL,
                                   GTK_STOCK_YES,
                                   "Yes",
                                   _("An application is currently disabling the automatic sleep,"
                                   " doing this action now may damage the working state of this application,"
                                   " are you sure you want to hibernate the system?"),
                                   NULL);
                                   
        if ( !ret )
            return;
    }
    
    g_signal_emit (G_OBJECT (power), signals [SLEEPING], 0);
    xfpm_network_manager_sleep (TRUE);
        
    g_object_get (G_OBJECT (power->conf),
                  LOCK_SCREEN_ON_SLEEP, &lock_screen,
                  NULL);
    
    if ( lock_screen )
    {
        g_usleep (2000000); /* 2 seconds */
        xfpm_lock_screen ();
    }
    
    dbus_g_proxy_call (power->proxy, sleep_time, &error,
                       G_TYPE_INVALID,
                       G_TYPE_INVALID);
    
    if ( error )
    {
        if ( g_error_matches (error, DBUS_GERROR, DBUS_GERROR_NO_REPLY) )
        {
            XFPM_DEBUG ("D-Bus time out, but should be harmless");
        }
        else
        {
            const gchar *icon_name;
            if ( !g_strcmp0 (sleep_time, "Hibernate") )
                icon_name = XFPM_HIBERNATE_ICON;
            else
                icon_name = XFPM_SUSPEND_ICON;
            
            xfpm_power_report_error (power, error->message, icon_name);
            g_error_free (error);
        }
    }
    
    g_signal_emit (G_OBJECT (power), signals [WAKING_UP], 0);
    xfpm_network_manager_sleep (FALSE);
}

static void
xfpm_power_hibernate_cb (XfpmPower *power)
{
    xfpm_power_sleep (power, "Hibernate", FALSE);
}

static void
xfpm_power_suspend_cb (XfpmPower *power)
{
    xfpm_power_sleep (power, "Suspend", FALSE);
}

static void
xfpm_power_hibernate_clicked (XfpmPower *power)
{
    gtk_widget_destroy (power->dialog );
    power->dialog = NULL;
    xfpm_power_sleep (power, "Hibernate", TRUE);
}

static void
xfpm_power_suspend_clicked (XfpmPower *power)
{
    gtk_widget_destroy (power->dialog );
    power->dialog = NULL;
    xfpm_power_sleep (power, "Suspend", TRUE);
}

static void
xfpm_power_shutdown_clicked (XfpmPower *power)
{
    gtk_widget_destroy (power->dialog );
    power->dialog = NULL;
    g_signal_emit (G_OBJECT (power), signals [SHUTDOWN], 0);
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

    menu = gtk_menu_new();

    // Hibernate menu option
    mi = gtk_image_menu_item_new_with_label (_("Hibernate"));
    img = gtk_image_new_from_icon_name (XFPM_HIBERNATE_ICON, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);
    gtk_widget_set_sensitive (mi, FALSE);
    
    //TODOif ( power->can_hibernate && power->auth_hibernate)
    {
        gtk_widget_set_sensitive (mi, TRUE);
        g_signal_connect_swapped (G_OBJECT (mi), "activate",
                                  G_CALLBACK (xfpm_power_hibernate_cb), power);
    }
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    // Suspend menu option
    mi = gtk_image_menu_item_new_with_label (_("Suspend"));
    img = gtk_image_new_from_icon_name (XFPM_SUSPEND_ICON, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);
    
    gtk_widget_set_sensitive (mi, FALSE);
    
    //TODOif ( power->can_suspend && power->auth_hibernate)
    {
        gtk_widget_set_sensitive (mi, TRUE);
        g_signal_connect_swapped (mi, "activate",
                                  G_CALLBACK (xfpm_power_suspend_cb), power);
    }
    
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
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

    // Power information
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
    img = gtk_image_new_from_icon_name (XFPM_AC_ADAPTER_ICON, GTK_ICON_SIZE_MENU);
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

    // Popup the menu
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
    GList *list;
    guint len, i;
    XfpmBatteryCharge max_charge_status = XFPM_BATTERY_CHARGE_UNKNOWN;
    
    list = g_hash_table_get_values (power->hash);
    len = g_list_length (list);
    
    for ( i = 0; i < len; i++)
    {
        XfpmBatteryCharge battery_charge;
        XfpmDeviceType type;
        
        g_object_get (G_OBJECT (g_list_nth_data (list, i)),
                      "charge-status", &battery_charge,
                      "device-type", &type,
                      NULL);
        if ( type != XFPM_DEVICE_TYPE_BATTERY && 
             type != XFPM_DEVICE_TYPE_UPS )
            continue;
        
        max_charge_status = MAX (max_charge_status, battery_charge);
    }
    
    return max_charge_status;
}

static void
xfpm_power_notify_action_callback (NotifyNotification *n, gchar *action, XfpmPower *power)
{
    if ( !g_strcmp0 (action, "Shutdown") )
        g_signal_emit (G_OBJECT (power), signals [SHUTDOWN], 0);
    else
        xfpm_power_sleep (power, action, TRUE);
}

static void
xfpm_power_add_actions_to_notification (XfpmPower *power, NotifyNotification *n)
{
    gboolean can_shutdown = FALSE;

    //TODO if (  power->can_hibernate && power->auth_hibernate )
    {
        xfpm_notify_add_action_to_notification(
                               power->notify,
                               n,
                               "Hibernate",
                               _("Hibernate the system"),
                               (NotifyActionCallback)xfpm_power_notify_action_callback,
                               power);      
    }
    
    //TODO if (  power->can_suspend && power->auth_suspend )
    {
        xfpm_notify_add_action_to_notification(
                               power->notify,
                               n,
                               "Suspend",
                               _("Suspend the system"),
                               (NotifyActionCallback)xfpm_power_notify_action_callback,
                               power);      
    }
    
    if (can_shutdown )
        xfpm_notify_add_action_to_notification(
                                   power->notify,
                                   n,
                                   "Shutdown",
                                   _("Shutdown the system"),
                                   (NotifyActionCallback)xfpm_power_notify_action_callback,
                                   power);    
}

static void
xfpm_power_show_critical_action_notification (XfpmPower *power, XfpmBattery *battery)
{
    const gchar *message;
    NotifyNotification *n;
    
    message = _("System is running on low power. "\
               "Save your work to avoid losing data");
              
    n = 
        xfpm_notify_new_notification (power->notify, 
                                      _("Power Manager"), 
                                      message, 
                                      gtk_status_icon_get_icon_name (GTK_STATUS_ICON (battery)),
                                      20000,
                                      XFPM_NOTIFY_CRITICAL,
                                      GTK_STATUS_ICON (battery));
    
    xfpm_power_add_actions_to_notification (power, n);
    xfpm_notify_critical (power->notify, n);

}

static void
xfpm_power_close_critical_dialog (XfpmPower *power)
{
    gtk_widget_destroy (power->dialog);
    power->dialog = NULL;
}

static void
xfpm_power_show_critical_action_gtk (XfpmPower *power)
{
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *img;
    GtkWidget *cancel;
    const gchar *message;
    gboolean can_shutdown = FALSE; /* TODO */
    
    message = _("System is running on low power. "\
               "Save your work to avoid losing data");
    
    dialog = gtk_dialog_new_with_buttons (_("Power Manager"), NULL, GTK_DIALOG_MODAL,
                                          NULL);
    
    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_CANCEL);
    
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_box_pack_start_defaults (GTK_BOX (content_area), gtk_label_new (message));
    
    //TODO if ( power->can_hibernate && power->auth_hibernate )
    {
        GtkWidget *hibernate;
        hibernate = gtk_button_new_with_label (_("Hibernate"));
        img = gtk_image_new_from_icon_name (XFPM_HIBERNATE_ICON, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (hibernate), img);
        gtk_dialog_add_action_widget (GTK_DIALOG (dialog), hibernate, GTK_RESPONSE_NONE);
        
        g_signal_connect_swapped (hibernate, "clicked",
                                  G_CALLBACK (xfpm_power_hibernate_clicked), power);
    }
    
    //TODO if ( power->can_suspend && power->auth_suspend )
    {
        GtkWidget *suspend;
        
        suspend = gtk_button_new_with_label (_("Suspend"));
        img = gtk_image_new_from_icon_name (XFPM_SUSPEND_ICON, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (suspend), img);
        gtk_dialog_add_action_widget (GTK_DIALOG (dialog), suspend, GTK_RESPONSE_NONE);
        
        g_signal_connect_swapped (suspend, "clicked",
                                  G_CALLBACK (xfpm_power_suspend_clicked), power);
    }
    
    if ( can_shutdown )
    {
        GtkWidget *shutdown;
        
        shutdown = gtk_button_new_with_label (_("Shutdown"));
        img = gtk_image_new_from_icon_name (XFPM_SUSPEND_ICON, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (shutdown), img);
        gtk_dialog_add_action_widget (GTK_DIALOG (dialog), shutdown, GTK_RESPONSE_NONE);
        
        g_signal_connect_swapped (shutdown, "clicked",
                                  G_CALLBACK (xfpm_power_shutdown_clicked), power);
    }
    
    cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancel, GTK_RESPONSE_NONE);
    
    g_signal_connect_swapped (cancel, "clicked",
                              G_CALLBACK (xfpm_power_close_critical_dialog), power);
    
    g_signal_connect_swapped (dialog, "destroy",
                              G_CALLBACK (xfpm_power_close_critical_dialog), power);
    if ( power->dialog )
    {
        gtk_widget_destroy (power->dialog);
        power->dialog = NULL;
        
    }
    power->dialog = dialog;
    gtk_widget_show_all (dialog);
}

static void
xfpm_power_show_critical_action (XfpmPower *power, XfpmBattery *battery)
{
    gboolean supports_actions;
    
    g_object_get (G_OBJECT (power->notify),
                  "actions", &supports_actions,
                  NULL);
                  
    if ( supports_actions )
        xfpm_power_show_critical_action_notification (power, battery);
    else
        xfpm_power_show_critical_action_gtk (power);
}

static void
xfpm_power_process_critical_action (XfpmPower *power, XfpmShutdownRequest req)
{
    if ( req == XFPM_ASK )
        g_signal_emit (G_OBJECT (power), signals [ASK_SHUTDOWN], 0);
    else if ( req == XFPM_DO_SUSPEND )
        xfpm_power_sleep (power, "Suspend", TRUE);
    else if ( req == XFPM_DO_HIBERNATE )
        xfpm_power_sleep (power, "Hibernate", TRUE);
    else if ( req == XFPM_DO_SHUTDOWN )
        g_signal_emit (G_OBJECT (power), signals [SHUTDOWN], 0);
}

static void
xfpm_power_system_on_critical_power (XfpmPower *power, XfpmBattery *battery)
{
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
                
                const gchar *battery_name = xfpm_battery_get_battery_name (battery);
                
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
    
    /*Current charge is okay now, then close the dialog*/
    if ( power->dialog )
    {
        gtk_widget_destroy (power->dialog);
        power->dialog = NULL;
    }
}

static void
xfpm_power_add_device (XfpmPower *power, UpDevice *device)
{
    UpDeviceKind device_kind;
    GtkStatusIcon *battery;
    
    g_object_get (device, "kind", &kind, NULL);
    
    XFPM_DEBUG_ENUM (device_type, XFPM_TYPE_DEVICE_TYPE, " device added");
                                       
    switch (kind)
    {
        case UP_DEVICE_KIND_BATTERY:
        case UP_DEVICE_KIND_UPS:
        case UP_DEVICE_KIND_MOUSE:
        case UP_DEVICE_KIND_KEYBOARD:
        case UP_DEVICE_KIND_PHONE:
            battery = xfpm_battery_new (device);
            gtk_status_icon_set_visible (battery, FALSE);
            g_signal_connect (battery, "popup-menu",
                              G_CALLBACK (xfpm_power_show_tray_menu_battery), power);
            g_signal_connect (battery, "battery-charge-changed",
                              G_CALLBACK (xfpm_power_battery_charge_changed_cb), power);
            
            power->devices = g_slist_prepend (power->devices, device);

            xfpm_power_refresh_adaptor_visible (power);

            break;
        
        case UP_DEVICE_KIND_LINE_POWER:
            g_warning ("Unable to monitor unkown power device with object_path : %s", object_path);
            break;
        
        default:
            /* other devices types we don't care about */
            break;
    }
}

static void
xfpm_power_get_power_devices (XfpmPower *power)
{
    GPtrArray *array = NULL;
    guint i;
    
    array = xfpm_power_enumerate_devices (power->proxy);
    
    if ( array )
    {
        for ( i = 0; i < array->len; i++)
        {
            const gchar *object_path = ( const gchar *) g_ptr_array_index (array, i);
            XFPM_DEBUG ("Power device detected at : %s", object_path);
            xfpm_power_add_device (power, object_path);
        }
        g_ptr_array_free (array, TRUE);
    }
    
}

static void
xfpm_power_remove_device (XfpmPower *power, UpDevice *device)
{
    power->devices = g_slist_remove (power->devices, device);
    xfpm_power_refresh_adaptor_visible (power);
}


static void
xfpm_power_changed_cb (UpClient *up_client, XfpmPower *power)
{
    g_return_if_fail (power->up_client == up_client);
    xfpm_power_refresh_adaptor_visible (power);
}

static void
xfpm_power_device_added_cb (UpClient *up_client, UpDevice *device, XfpmPower *power)
{
    g_return_if_fail (power->up_client == up_client);
    xfpm_power_add_device (power, device);
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
xfpm_power_hide_adapter_icon (XfpmPower *power)
{
     XFPM_DEBUG ("Hide adaptor icon");
     
    if ( power->adapter_icon )
    {
        g_object_unref (power->adapter_icon);
        power->adapter_icon = NULL;
    }
}

static void
xfpm_power_show_adapter_icon (XfpmPower *power)
{
    g_return_if_fail (power->adapter_icon == NULL);
    
    power->adapter_icon = gtk_status_icon_new ();
    
    XFPM_DEBUG ("Showing adaptor icon");
    
    gtk_status_icon_set_from_icon_name (power->adapter_icon, XFPM_AC_ADAPTER_ICON);
    
    gtk_status_icon_set_visible (power->adapter_icon, TRUE);
    
    g_signal_connect (power->adapter_icon, "popup-menu",
                      G_CALLBACK (xfpm_power_show_tray_menu_adaptor), power);
}

static void
xfpm_power_refresh_adaptor_visible (XfpmPower *power)
{
    XfpmShowIcon show_icon;
    
    g_object_get (G_OBJECT (power->conf),
                  SHOW_TRAY_ICON_CFG, &show_icon,
                  NULL);
                  
    XFPM_DEBUG_ENUM (show_icon, XFPM_TYPE_SHOW_ICON, "Tray icon configuration: ");
    
    if ( show_icon == SHOW_ICON_ALWAYS )
    {
        if ( g_hash_table_size (power->hash) == 0 )
        {
            xfpm_power_show_adapter_icon (power);
#if GTK_CHECK_VERSION (2, 16, 0)
            gtk_status_icon_set_tooltip_text (power->adapter_icon, 
                                              TRUE ? //power->on_battery ? 
                                              _("Adaptor is offline") :
                                              _("Adaptor is online") );
#else
            gtk_status_icon_set_tooltip (power->adapter_icon, 
                                         TRUE ?//power->on_battery ? 
                                         _("Adaptor is offline") :
                                         _("Adaptor is online") );
#endif
        }
        else
        {
            xfpm_power_hide_adapter_icon (power);
        }
    }
    else
    {
        xfpm_power_hide_adapter_icon (power);
    }
}

static void
xfpm_power_class_init (XfpmPowerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_power_finalize;

    signals [ON_BATTERY_CHANGED] = 
        g_signal_new ("on-battery-changed",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [LOW_BATTERY_CHANGED] = 
        g_signal_new ("low-battery-changed",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [LID_CHANGED] = 
        g_signal_new ("lid-changed",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [WAKING_UP] = 
        g_signal_new ("waking-up",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [SLEEPING] = 
        g_signal_new ("sleeping",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [ASK_SHUTDOWN] = 
        g_signal_new ("ask-shutdown",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [SHUTDOWN] = 
        g_signal_new ("shutdown",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);
}

static void
xfpm_power_init (XfpmPower *power)
{
    GError *error = NULL;
    gboolean on_battery;

    power->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    power->dialog          = NULL;
    power->adapter_icon    = NULL;
    power->overall_state   = XFPM_BATTERY_CHARGE_OK;
    power->critical_action_done = FALSE;
    power->power_mode      = XFPM_POWER_MODE_NORMAL;
    power->notify  = xfpm_notify_new ();
    
    power->up_client = up_client_new ();
    
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

    if (xfpm_power_get_on_battery (power, &on_battery, &error))
    {
        g_signal_emit (G_OBJECT (power), signals [ON_BATTERY_CHANGED], 0, on_battery);
    }
    else
    {
        g_critical ("Failed to get on-battery: %s", error->message);
        g_error_free (error);
    }
}

static void
xfpm_power_finalize (GObject *object)
{
    XfpmPower *power;

    power = XFPM_POWER (object);

    g_object_unref (power->conf);
    
    xfpm_power_hide_adapter_icon (power);
    
    dbus_g_connection_unref (power->bus);
    
    if ( power->proxy )
    {
        dbus_g_proxy_disconnect_signal (power->proxy, "Changed",
                                        G_CALLBACK (xfpm_power_changed_cb), power);
        dbus_g_proxy_disconnect_signal (power->proxy, "DeviceRemoved",
                                        G_CALLBACK (xfpm_power_device_removed_cb), power);
        dbus_g_proxy_disconnect_signal (power->proxy, "DeviceAdded",
                                        G_CALLBACK (xfpm_power_device_added_cb), power);
        dbus_g_proxy_disconnect_signal (power->proxy, "DeviceChanged",
                                        G_CALLBACK (xfpm_power_device_changed_cb), power);
        g_object_unref (power->proxy);
    }
    
    if ( power->proxy_prop )
        g_object_unref (power->proxy_prop);

    g_hash_table_destroy (power->hash);

    G_OBJECT_CLASS (xfpm_power_parent_class)->finalize (object);
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

void 
xfpm_power_suspend (XfpmPower *power, gboolean force)
{
    xfpm_power_sleep (power, "Suspend", force);
}

void 
xfpm_power_hibernate (XfpmPower *power, gboolean force)
{
    xfpm_power_sleep (power, "Hibernate", force);
}

gboolean 
xfpm_power_can_suspend (XfpmPower  *power, 
                        gboolean   *can_suspend,
                        GError    **error)
{
  if (!up_client_get_properties_sync (power->up_client, NULL, error))
    return FALSE;

  *can_suspend = up_client_get_can_suspend (power->up_client);
  return TRUE;
}

gboolean 
xfpm_power_can_hibernate (XfpmPower  *power, 
                          gboolean   *can_hibernate,
                          GError    **error)
{
  if (!up_client_get_properties_sync (power->up_client, NULL, error))
    return FALSE;

  *can_hibernate = up_client_get_can_hibernate (power->up_client);
  return TRUE;
}

gboolean                
xfpm_power_get_on_battery (XfpmPower  *power,
                           gboolean   *on_battery,
                           GError    **error)
{
  if (!up_client_get_properties_sync (power->up_client, NULL, error))
    return FALSE;

  *on_battery = up_client_get_on_battery (power->up_client);
  return TRUE;
}

gboolean                
xfpm_power_get_low_battery (XfpmPower  *power,
                            gboolean   *low_battery,
                            GError    **error)
{
  if (!up_client_get_properties_sync (power->up_client, NULL, error))
    return FALSE;

  *low_battery = up_client_get_on_low_battery (power->up_client);
  return TRUE;
}

XfpmPowerMode  xfpm_power_get_mode (XfpmPower *power)
{
    g_return_val_if_fail (XFPM_IS_POWER (power), XFPM_POWER_MODE_NORMAL);
    
    return power->power_mode;
}

