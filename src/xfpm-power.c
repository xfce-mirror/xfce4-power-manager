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

#include "xfpm-power.h"
#include "xfpm-dbus.h"
#include "xfpm-dpms.h"
#include "xfpm-battery.h"
#include "xfpm-xfconf.h"
#include "xfpm-notify.h"
#include "xfpm-errors.h"
#include "xfpm-console-kit.h"
#include "xfpm-inhibit.h"
#include "xfpm-polkit.h"
#include "xfpm-network-manager.h"
#include "xfpm-icons.h"
#include "xfpm-common.h"
#include "xfpm-power-common.h"
#include "xfpm-config.h"
#include "xfpm-debug.h"
#include "xfpm-enum-types.h"
#include "egg-idletime.h"

static void xfpm_power_finalize     (GObject *object);

static void xfpm_power_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec);

static void xfpm_power_dbus_class_init (XfpmPowerClass * klass);
static void xfpm_power_dbus_init (XfpmPower *power);

static void xfpm_power_refresh_adaptor_visible (XfpmPower *power);

#define XFPM_POWER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_POWER, XfpmPowerPrivate))

struct XfpmPowerPrivate
{
    DBusGConnection *bus;
    
    DBusGProxy      *proxy;
    DBusGProxy      *proxy_prop;
    
    GHashTable      *hash;
    
    XfpmConsoleKit  *console;
    XfpmInhibit	    *inhibit;
    XfpmXfconf      *conf;
    GtkStatusIcon   *adapter_icon;
    
    XfpmBatteryCharge overall_state;
    gboolean         critical_action_done;
    
    XfpmPowerMode    power_mode;
    EggIdletime     *idletime;
    
    gboolean	     inhibited;
    
    XfpmNotify	    *notify;
#ifdef ENABLE_POLKIT
    XfpmPolkit 	    *polkit;
#endif
    gboolean	     auth_suspend;
    gboolean	     auth_hibernate;

    /* Properties */
    gboolean	     on_low_battery;
    gboolean	     lid_is_present;
    gboolean         lid_is_closed;
    gboolean	     on_battery;
    gchar           *daemon_version;
    gboolean	     can_suspend;
    gboolean         can_hibernate;
    
    /**
     * Warning dialog to use when notification daemon 
     * doesn't support actions.
     **/
    GtkWidget 	    *dialog;
};

enum
{
    PROP_0,
    PROP_ON_LOW_BATTERY,
    PROP_ON_BATTERY,
    PROP_AUTH_SUSPEND,
    PROP_AUTH_HIBERNATE,
    PROP_CAN_SUSPEND,
    PROP_CAN_HIBERNATE,
    PROP_HAS_LID
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

#ifdef ENABLE_POLKIT
static void
xfpm_power_check_polkit_auth (XfpmPower *power)
{
    power->priv->auth_suspend = xfpm_polkit_check_auth (power->priv->polkit, 
							POLKIT_AUTH_SUSPEND);

    power->priv->auth_hibernate = xfpm_polkit_check_auth (power->priv->polkit, 
							  POLKIT_AUTH_HIBERNATE);

}
#endif

static void
xfpm_power_check_pm (XfpmPower *power, GHashTable *props)
{
    GValue *value;
    gboolean ret;
    
    value = g_hash_table_lookup (props, "CanSuspend");
    
    if (value == NULL) 
    {
	g_warning ("No 'CanSuspend' property");
    }
    ret = g_value_get_boolean (value);
    
    if (ret != power->priv->can_suspend) 
    {
	power->priv->can_suspend = ret;
    }

    value = g_hash_table_lookup (props, "CanHibernate");
    
    if (value == NULL) 
    {
	g_warning ("No 'CanHibernate' property");
    }
    
    ret = g_value_get_boolean (value);
    
    if (ret != power->priv->can_hibernate) 
    {
	power->priv->can_hibernate = ret;
    }
}

static void
xfpm_power_check_power (XfpmPower *power, GHashTable *props)
{
    GValue *value;
    gboolean on_battery;
    
    value = g_hash_table_lookup (props, "OnBattery");
    
    if (G_LIKELY (value)) 
    {
	on_battery = g_value_get_boolean (value);
    
	if (on_battery != power->priv->on_battery ) 
	{
	    GList *list;
	    guint len, i;
	    g_signal_emit (G_OBJECT (power), signals [ON_BATTERY_CHANGED], 0, on_battery);
	    power->priv->on_battery = on_battery;
	    list = g_hash_table_get_values (power->priv->hash);
	    len = g_list_length (list);
	    for ( i = 0; i < len; i++)
	    {
		g_object_set (G_OBJECT (g_list_nth_data (list, i)), 
			      "ac-online", !on_battery,
			      NULL);
	    }
	}
    }
    else
    {
	g_warning ("No 'OnBattery' property");
    }
}

static void
xfpm_power_check_lid (XfpmPower *power, GHashTable *props)
{
    GValue *value;
    
    value = g_hash_table_lookup (props, "LidIsPresent");
    
    if (value == NULL) 
    {
	g_warning ("No 'LidIsPresent' property");
	return;
    }

    power->priv->lid_is_present = g_value_get_boolean (value);

    if (power->priv->lid_is_present) 
    {
	gboolean closed;
	
	value = g_hash_table_lookup (props, "LidIsClosed");
    
	if (value == NULL) 
	{
	    g_warning ("No 'LidIsClosed' property");
	    return;
	}
	
	closed = g_value_get_boolean (value);
	
	if (closed != power->priv->lid_is_closed ) 
	{
	    power->priv->lid_is_closed = closed;
	    g_signal_emit (G_OBJECT (power), signals [LID_CHANGED], 0, power->priv->lid_is_closed);
	}
    }
}

/*
 * Get the properties on org.freedesktop.DeviceKit.Power
 * 
 * DaemonVersion      's'
 * CanSuspend'        'b'
 * CanHibernate'      'b'
 * OnBattery'         'b'
 * OnLowBattery'      'b'
 * LidIsClosed'       'b'
 * LidIsPresent'      'b'
 */
static void
xfpm_power_get_properties (XfpmPower *power)
{
    GHashTable *props;
    
    props = xfpm_power_get_interface_properties (power->priv->proxy_prop, UPOWER_IFACE);
    
    xfpm_power_check_pm (power, props);
    xfpm_power_check_lid (power, props);
    xfpm_power_check_power (power, props);

    g_hash_table_destroy (props);
}

static void
xfpm_power_report_error (XfpmPower *power, const gchar *error, const gchar *icon_name)
{
    GtkStatusIcon *battery = NULL;
    guint i, len;
    GList *list;
    
    list = g_hash_table_get_values (power->priv->hash);
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
    
    xfpm_notify_show_notification (power->priv->notify, 
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
    
    if ( power->priv->inhibited && force == FALSE)
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
        
    g_object_get (G_OBJECT (power->priv->conf),
		  LOCK_SCREEN_ON_SLEEP, &lock_screen,
		  NULL);
    
    if ( lock_screen )
    {
	g_usleep (2000000); /* 2 seconds */
	xfpm_lock_screen ();
    }
    
    dbus_g_proxy_call (power->priv->proxy, sleep_time, &error,
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
    gtk_widget_destroy (power->priv->dialog );
    power->priv->dialog = NULL;
    xfpm_power_sleep (power, "Hibernate", TRUE);
}

static void
xfpm_power_suspend_clicked (XfpmPower *power)
{
    gtk_widget_destroy (power->priv->dialog );
    power->priv->dialog = NULL;
    xfpm_power_sleep (power, "Suspend", TRUE);
}

static void
xfpm_power_shutdown_clicked (XfpmPower *power)
{
    gtk_widget_destroy (power->priv->dialog );
    power->priv->dialog = NULL;
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
#ifdef HAVE_DPMS
    XfpmDpms *dpms;
    
    power->priv->power_mode = mode;
    
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
#endif
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
    
    if ( power->priv->can_hibernate && power->priv->auth_hibernate)
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
    
    if ( power->priv->can_suspend && power->priv->auth_hibernate)
    {
	gtk_widget_set_sensitive (mi, TRUE);
	g_signal_connect_swapped (mi, "activate",
				  G_CALLBACK (xfpm_power_suspend_cb), power);
    }
    
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
/*
    saver_inhibited = xfpm_screen_saver_get_inhibit (tray->priv->srv);
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
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), power->priv->power_mode == XFPM_POWER_MODE_NORMAL);
    gtk_widget_set_sensitive (mi,TRUE);
    
    g_signal_connect_swapped (mi, "activate",
			      G_CALLBACK (xfpm_power_normal_mode_cb), power);
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (subm), mi);
    
    /* Normal*/
    mi = gtk_check_menu_item_new_with_label (_("Presentation"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), power->priv->power_mode == XFPM_POWER_MODE_PRESENTATION);
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
    
    list = g_hash_table_get_values (power->priv->hash);
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
    gboolean can_shutdown;
    
    g_object_get (G_OBJECT (power->priv->console),
		  "can-shutdown", &can_shutdown,
		  NULL);
		  
    if (  power->priv->can_hibernate && power->priv->auth_hibernate )
    {
        xfpm_notify_add_action_to_notification(
			       power->priv->notify,
			       n,
                               "Hibernate",
                               _("Hibernate the system"),
                               (NotifyActionCallback)xfpm_power_notify_action_callback,
                               power);      
    }
    
    if (  power->priv->can_suspend && power->priv->auth_suspend )
    {
        xfpm_notify_add_action_to_notification(
			       power->priv->notify,
			       n,
                               "Suspend",
                               _("Suspend the system"),
                               (NotifyActionCallback)xfpm_power_notify_action_callback,
                               power);      
    }
    
    if (can_shutdown )
	xfpm_notify_add_action_to_notification(
				   power->priv->notify,
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
	xfpm_notify_new_notification (power->priv->notify, 
				      _("Power Manager"), 
				      message, 
				      gtk_status_icon_get_icon_name (GTK_STATUS_ICON (battery)),
				      20000,
				      XFPM_NOTIFY_CRITICAL,
				      GTK_STATUS_ICON (battery));
    
    xfpm_power_add_actions_to_notification (power, n);
    xfpm_notify_critical (power->priv->notify, n);

}

static void
xfpm_power_close_critical_dialog (XfpmPower *power)
{
    gtk_widget_destroy (power->priv->dialog);
    power->priv->dialog = NULL;
}

static void
xfpm_power_show_critical_action_gtk (XfpmPower *power)
{
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *img;
    GtkWidget *cancel;
    const gchar *message;
    gboolean can_shutdown;
    
    g_object_get (G_OBJECT (power->priv->console),
		  "can-shutdown", &can_shutdown,
		  NULL);
    
    message = _("System is running on low power. "\
               "Save your work to avoid losing data");
    
    dialog = gtk_dialog_new_with_buttons (_("Power Manager"), NULL, GTK_DIALOG_MODAL,
                                          NULL);
    
    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_CANCEL);
    
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_box_pack_start_defaults (GTK_BOX (content_area), gtk_label_new (message));
    
    if ( power->priv->can_hibernate && power->priv->auth_hibernate )
    {
	GtkWidget *hibernate;
	hibernate = gtk_button_new_with_label (_("Hibernate"));
	img = gtk_image_new_from_icon_name (XFPM_HIBERNATE_ICON, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (hibernate), img);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), hibernate, GTK_RESPONSE_NONE);
	
	g_signal_connect_swapped (hibernate, "clicked",
			          G_CALLBACK (xfpm_power_hibernate_clicked), power);
    }
    
    if ( power->priv->can_suspend && power->priv->auth_suspend )
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
    if ( power->priv->dialog )
    {
	gtk_widget_destroy (power->priv->dialog);
	power->priv->dialog = NULL;
	
    }
    power->priv->dialog = dialog;
    gtk_widget_show_all (dialog);
}

static void
xfpm_power_show_critical_action (XfpmPower *power, XfpmBattery *battery)
{
    gboolean supports_actions;
    
    g_object_get (G_OBJECT (power->priv->notify),
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
    
    g_object_get (G_OBJECT (power->priv->conf),
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
	if (power->priv->critical_action_done == FALSE)
	{
	    power->priv->critical_action_done = TRUE;
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
    
    if (current_charge == power->priv->overall_state)
	return;
    
    if (current_charge >= XFPM_BATTERY_CHARGE_LOW)
	power->priv->critical_action_done = FALSE;
    
    power->priv->overall_state = current_charge;
    
    if ( current_charge == XFPM_BATTERY_CHARGE_CRITICAL && power->priv->on_battery)
    {
	xfpm_power_system_on_critical_power (power, battery);
	
	power->priv->on_low_battery = TRUE;
	g_signal_emit (G_OBJECT (power), signals [LOW_BATTERY_CHANGED], 0, power->priv->on_low_battery);
	return;
    }
    
    if ( power->priv->on_low_battery )
    {
	power->priv->on_low_battery = FALSE;
	g_signal_emit (G_OBJECT (power), signals [LOW_BATTERY_CHANGED], 0, power->priv->on_low_battery);
    }
    
    g_object_get (G_OBJECT (power->priv->conf),
		  GENERAL_NOTIFICATION_CFG, &notify,
		  NULL);
    
    if ( power->priv->on_battery )
    {
	if ( current_charge == XFPM_BATTERY_CHARGE_LOW )
	{
	    if ( notify )
		xfpm_notify_show_notification (power->priv->notify, 
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
		
		
		xfpm_notify_show_notification (power->priv->notify, 
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
    if ( power->priv->dialog )
    {
	gtk_widget_destroy (power->priv->dialog);
	power->priv->dialog = NULL;
    }
}

static void
xfpm_power_add_device (XfpmPower *power, const gchar *object_path)
{
    DBusGProxy *proxy_prop;
    guint device_type = XFPM_DEVICE_TYPE_UNKNOWN;
    GValue value;
    
    proxy_prop = dbus_g_proxy_new_for_name (power->priv->bus, 
					    UPOWER_NAME,
					    object_path,
					    DBUS_INTERFACE_PROPERTIES);
				       
    if ( !proxy_prop )
    {
	g_warning ("Unable to create proxy for : %s", object_path);
	return;
    }
    
    value = xfpm_power_get_interface_property (proxy_prop, UPOWER_IFACE_DEVICE, "Type");
    
    device_type = g_value_get_uint (&value);
    
    XFPM_DEBUG_ENUM (device_type, XFPM_TYPE_DEVICE_TYPE, " device added");
    
    if ( device_type == XFPM_DEVICE_TYPE_BATTERY || 
	 device_type == XFPM_DEVICE_TYPE_UPS     ||
	 device_type == XFPM_DEVICE_TYPE_MOUSE   ||
	 device_type == XFPM_DEVICE_TYPE_KBD     ||
	 device_type == XFPM_DEVICE_TYPE_PHONE)
    {
	GtkStatusIcon *battery;
	DBusGProxy *proxy;
	XFPM_DEBUG_ENUM (device_type, XFPM_TYPE_DEVICE_TYPE, 
			"Battery device detected at : %s", object_path);
	proxy = dbus_g_proxy_new_for_name (power->priv->bus,
					   UPOWER_NAME,
					   object_path,
					   UPOWER_IFACE_DEVICE);
	battery = xfpm_battery_new ();
	gtk_status_icon_set_visible (battery, FALSE);
	xfpm_battery_monitor_device (XFPM_BATTERY (battery), 
				     proxy, 
				     proxy_prop, 
				     device_type);

	g_hash_table_insert (power->priv->hash, g_strdup (object_path), battery);
	
	g_signal_connect (battery, "popup-menu",
			  G_CALLBACK (xfpm_power_show_tray_menu_battery), power);
	
	g_signal_connect (battery, "battery-charge-changed",
			  G_CALLBACK (xfpm_power_battery_charge_changed_cb), power);
			  
	xfpm_power_refresh_adaptor_visible (power);
    }
    else if ( device_type != XFPM_DEVICE_TYPE_LINE_POWER )
    {
	g_warning ("Unable to monitor unkown power device with object_path : %s", object_path);
	g_object_unref (proxy_prop);
    }
}

static void
xfpm_power_get_power_devices (XfpmPower *power)
{
    GPtrArray *array = NULL;
    guint i;
    
    array = xfpm_power_enumerate_devices (power->priv->proxy);
    
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
xfpm_power_remove_device (XfpmPower *power, const gchar *object_path)
{
    g_hash_table_remove (power->priv->hash, object_path);
    xfpm_power_refresh_adaptor_visible (power);
}

static void
xfpm_power_inhibit_changed_cb (XfpmInhibit *inhibit, gboolean is_inhibit, XfpmPower *power)
{
    power->priv->inhibited = is_inhibit;
}

static void
xfpm_power_changed_cb (DBusGProxy *proxy, XfpmPower *power)
{
    xfpm_power_get_properties (power);
    xfpm_power_refresh_adaptor_visible (power);
}

static void
xfpm_power_device_added_cb (DBusGProxy *proxy, const gchar *object_path, XfpmPower *power)
{
    xfpm_power_add_device (power, object_path);
}

static void
xfpm_power_device_removed_cb (DBusGProxy *proxy, const gchar *object_path, XfpmPower *power)
{
    xfpm_power_remove_device (power, object_path);
}

static void
xfpm_power_device_changed_cb (DBusGProxy *proxy, const gchar *object_path, XfpmPower *power)
{
    xfpm_power_refresh_adaptor_visible (power);
}

#ifdef ENABLE_POLKIT
static void
xfpm_power_polkit_auth_changed_cb (XfpmPower *power)
{
    XFPM_DEBUG ("Auth configuration changed");
    xfpm_power_check_polkit_auth (power);
}
#endif

static void
xfpm_power_hide_adapter_icon (XfpmPower *power)
{
     XFPM_DEBUG ("Hide adaptor icon");
     
    if ( power->priv->adapter_icon )
    {
        g_object_unref (power->priv->adapter_icon);
        power->priv->adapter_icon = NULL;
    }
}

static void
xfpm_power_show_adapter_icon (XfpmPower *power)
{
    g_return_if_fail (power->priv->adapter_icon == NULL);
    
    power->priv->adapter_icon = gtk_status_icon_new ();
    
    XFPM_DEBUG ("Showing adaptor icon");
    
    gtk_status_icon_set_from_icon_name (power->priv->adapter_icon, XFPM_AC_ADAPTER_ICON);
    
    gtk_status_icon_set_visible (power->priv->adapter_icon, TRUE);
    
    g_signal_connect (power->priv->adapter_icon, "popup-menu",
		      G_CALLBACK (xfpm_power_show_tray_menu_adaptor), power);
}

static void
xfpm_power_refresh_adaptor_visible (XfpmPower *power)
{
    XfpmShowIcon show_icon;
    
    g_object_get (G_OBJECT (power->priv->conf),
		  SHOW_TRAY_ICON_CFG, &show_icon,
		  NULL);
		  
    XFPM_DEBUG_ENUM (show_icon, XFPM_TYPE_SHOW_ICON, "Tray icon configuration: ");
    
    if ( show_icon == SHOW_ICON_ALWAYS )
    {
	if ( g_hash_table_size (power->priv->hash) == 0 )
	{
	    xfpm_power_show_adapter_icon (power);
#if GTK_CHECK_VERSION (2, 16, 0)
	    gtk_status_icon_set_tooltip_text (power->priv->adapter_icon, 
					      power->priv->on_battery ? 
					      _("Adaptor is offline") :
					      _("Adaptor is online") );
#else
	    gtk_status_icon_set_tooltip (power->priv->adapter_icon, 
					 power->priv->on_battery ? 
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

    object_class->get_property = xfpm_power_get_property;

    signals [ON_BATTERY_CHANGED] = 
        g_signal_new ("on-battery-changed",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPowerClass, on_battery_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [LOW_BATTERY_CHANGED] = 
        g_signal_new ("low-battery-changed",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPowerClass, low_battery_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [LID_CHANGED] = 
        g_signal_new ("lid-changed",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPowerClass, lid_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [WAKING_UP] = 
        g_signal_new ("waking-up",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPowerClass, waking_up),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [SLEEPING] = 
        g_signal_new ("sleeping",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPowerClass, sleeping),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [ASK_SHUTDOWN] = 
        g_signal_new ("ask-shutdown",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPowerClass, ask_shutdown),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [SHUTDOWN] = 
        g_signal_new ("shutdown",
                      XFPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPowerClass, shutdown),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    g_object_class_install_property (object_class,
                                     PROP_ON_BATTERY,
                                     g_param_spec_boolean ("on-battery",
                                                          NULL, NULL,
                                                          FALSE,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_ON_LOW_BATTERY,
                                     g_param_spec_boolean ("on-low-battery",
                                                          NULL, NULL,
                                                          FALSE,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_AUTH_SUSPEND,
                                     g_param_spec_boolean ("auth-suspend",
                                                          NULL, NULL,
                                                          FALSE,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_AUTH_HIBERNATE,
                                     g_param_spec_boolean ("auth-hibernate",
                                                          NULL, NULL,
                                                          FALSE,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_CAN_HIBERNATE,
                                     g_param_spec_boolean ("can-hibernate",
                                                          NULL, NULL,
                                                          FALSE,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_CAN_SUSPEND,
                                     g_param_spec_boolean ("can-suspend",
                                                          NULL, NULL,
                                                          FALSE,
                                                          G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_HAS_LID,
                                     g_param_spec_boolean ("has-lid",
                                                          NULL, NULL,
                                                          FALSE,
                                                          G_PARAM_READABLE));

    g_type_class_add_private (klass, sizeof (XfpmPowerPrivate));
    
    xfpm_power_dbus_class_init (klass);
}

static void
xfpm_power_init (XfpmPower *power)
{
    GError *error = NULL;
    
    power->priv = XFPM_POWER_GET_PRIVATE (power);
    
    power->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    power->priv->lid_is_present  = FALSE;
    power->priv->lid_is_closed   = FALSE;
    power->priv->on_battery      = FALSE;
    power->priv->on_low_battery  = FALSE;
    power->priv->daemon_version  = NULL;
    power->priv->can_suspend     = FALSE;
    power->priv->can_hibernate   = FALSE;
    power->priv->auth_hibernate  = TRUE;
    power->priv->auth_suspend    = TRUE;
    power->priv->dialog          = NULL;
    power->priv->adapter_icon    = NULL;
    power->priv->overall_state   = XFPM_BATTERY_CHARGE_OK;
    power->priv->critical_action_done = FALSE;
    power->priv->power_mode      = XFPM_POWER_MODE_NORMAL;
    
    power->priv->inhibit = xfpm_inhibit_new ();
    power->priv->notify  = xfpm_notify_new ();
    power->priv->conf    = xfpm_xfconf_new ();
    power->priv->console = xfpm_console_kit_new ();
    
    g_signal_connect_swapped (power->priv->conf, "notify::" SHOW_TRAY_ICON_CFG,
			      G_CALLBACK (xfpm_power_refresh_adaptor_visible), power);
    
#ifdef ENABLE_POLKIT
    power->priv->polkit  = xfpm_polkit_get ();
    g_signal_connect_swapped (power->priv->polkit, "auth-changed",
			      G_CALLBACK (xfpm_power_polkit_auth_changed_cb), power);
#endif
    
    g_signal_connect (power->priv->inhibit, "has-inhibit-changed",
		      G_CALLBACK (xfpm_power_inhibit_changed_cb), power);
    
    power->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Unable to connect to the system bus : %s", error->message);
	g_error_free (error);
	goto out;
    }

    power->priv->proxy = dbus_g_proxy_new_for_name (power->priv->bus,
						    UPOWER_NAME,
						    UPOWER_PATH,
						    UPOWER_IFACE);
    
    
    power->priv->proxy_prop = dbus_g_proxy_new_for_name (power->priv->bus,
							 UPOWER_NAME,
							 UPOWER_PATH,
							 DBUS_INTERFACE_PROPERTIES);
    if (power->priv->proxy_prop == NULL) 
    {
	g_critical ("Unable to create proxy for %s", UPOWER_NAME);
	goto out;
    }
    
    xfpm_power_get_power_devices (power);
    xfpm_power_get_properties (power);
#ifdef ENABLE_POLKIT
    xfpm_power_check_polkit_auth (power);
#endif

    dbus_g_proxy_add_signal (power->priv->proxy, "Changed", G_TYPE_INVALID);
    dbus_g_proxy_add_signal (power->priv->proxy, "DeviceAdded", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (power->priv->proxy, "DeviceRemoved", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (power->priv->proxy, "DeviceChanged", G_TYPE_STRING, G_TYPE_INVALID);
    
    dbus_g_proxy_connect_signal (power->priv->proxy, "Changed",
				 G_CALLBACK (xfpm_power_changed_cb), power, NULL);
    dbus_g_proxy_connect_signal (power->priv->proxy, "DeviceRemoved",
				 G_CALLBACK (xfpm_power_device_removed_cb), power, NULL);
    dbus_g_proxy_connect_signal (power->priv->proxy, "DeviceAdded",
				 G_CALLBACK (xfpm_power_device_added_cb), power, NULL);
   
    dbus_g_proxy_connect_signal (power->priv->proxy, "DeviceChanged",
				 G_CALLBACK (xfpm_power_device_changed_cb), power, NULL);

    
out:
    xfpm_power_refresh_adaptor_visible (power);

    xfpm_power_dbus_init (power);

    /*
     * Emit org.freedesktop.PowerManagement session signals on startup
     */
    g_signal_emit (G_OBJECT (power), signals [ON_BATTERY_CHANGED], 0, power->priv->on_battery);
}

static void xfpm_power_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
    XfpmPower *power;
    power = XFPM_POWER (object);

    switch (prop_id)
    {
	case PROP_ON_BATTERY:
	    g_value_set_boolean (value, power->priv->on_battery);
	    break;
	case PROP_AUTH_HIBERNATE:
	    g_value_set_boolean (value, power->priv->auth_hibernate);
	    break;
	case PROP_AUTH_SUSPEND:
	    g_value_set_boolean (value, power->priv->auth_suspend);
	    break;
	case PROP_CAN_SUSPEND:
	    g_value_set_boolean (value, power->priv->can_suspend);
	    break;
	case PROP_CAN_HIBERNATE:
	    g_value_set_boolean (value, power->priv->can_hibernate);
	    break;
	case PROP_HAS_LID:
	    g_value_set_boolean (value, power->priv->lid_is_present);
	    break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
xfpm_power_finalize (GObject *object)
{
    XfpmPower *power;

    power = XFPM_POWER (object);
    
    g_free (power->priv->daemon_version);
    
    g_object_unref (power->priv->inhibit);
    g_object_unref (power->priv->notify);
    g_object_unref (power->priv->conf);
    g_object_unref (power->priv->console);
    
    xfpm_power_hide_adapter_icon (power);
    
    dbus_g_connection_unref (power->priv->bus);
    
    if ( power->priv->proxy )
    {
	dbus_g_proxy_disconnect_signal (power->priv->proxy, "Changed",
					G_CALLBACK (xfpm_power_changed_cb), power);
	dbus_g_proxy_disconnect_signal (power->priv->proxy, "DeviceRemoved",
					G_CALLBACK (xfpm_power_device_removed_cb), power);
	dbus_g_proxy_disconnect_signal (power->priv->proxy, "DeviceAdded",
					G_CALLBACK (xfpm_power_device_added_cb), power);
	dbus_g_proxy_disconnect_signal (power->priv->proxy, "DeviceChanged",
					G_CALLBACK (xfpm_power_device_changed_cb), power);
	g_object_unref (power->priv->proxy);
    }
    
    if ( power->priv->proxy_prop )
	g_object_unref (power->priv->proxy_prop);

    g_hash_table_destroy (power->priv->hash);

#ifdef ENABLE_POLKIT
    g_object_unref (power->priv->polkit);
#endif

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

void xfpm_power_suspend (XfpmPower *power, gboolean force)
{
    xfpm_power_sleep (power, "Suspend", force);
}

void xfpm_power_hibernate (XfpmPower *power, gboolean force)
{
    xfpm_power_sleep (power, "Hibernate", force);
}

gboolean xfpm_power_has_battery (XfpmPower *power)
{
    GtkStatusIcon *battery = NULL;
    guint i, len;
    GList *list;
    
    gboolean ret = FALSE;
    
    list = g_hash_table_get_values (power->priv->hash);
    len = g_list_length (list);
    
    for ( i = 0; i < len; i++)
    {
	XfpmDeviceType type;
	battery = g_list_nth_data (list, i);
	type = xfpm_battery_get_device_type (XFPM_BATTERY (battery));
	if ( type == XFPM_DEVICE_TYPE_BATTERY ||
	     type == XFPM_DEVICE_TYPE_UPS )
	{
	    ret = TRUE;
	    break;
	}
    }
    
    return ret;
}

XfpmPowerMode  xfpm_power_get_mode (XfpmPower *power)
{
    g_return_val_if_fail (XFPM_IS_POWER (power), XFPM_POWER_MODE_NORMAL);
    
    return power->priv->power_mode;
}

/*
 * 
 * DBus server implementation for org.freedesktop.PowerManagement
 * 
 */
static gboolean xfpm_power_dbus_shutdown (XfpmPower *power,
				        GError **error);

static gboolean xfpm_power_dbus_reboot   (XfpmPower *power,
					GError **error);
					   
static gboolean xfpm_power_dbus_hibernate (XfpmPower * power,
					 GError **error);

static gboolean xfpm_power_dbus_suspend (XfpmPower * power,
				       GError ** error);

static gboolean xfpm_power_dbus_can_reboot (XfpmPower * power,
					  gboolean * OUT_can_reboot, 
					  GError ** error);

static gboolean xfpm_power_dbus_can_shutdown (XfpmPower * power,
					    gboolean * OUT_can_reboot, 
					    GError ** error);

static gboolean xfpm_power_dbus_can_hibernate (XfpmPower * power,
					     gboolean * OUT_can_hibernate,
					     GError ** error);

static gboolean xfpm_power_dbus_can_suspend (XfpmPower * power,
					   gboolean * OUT_can_suspend,
					   GError ** error);

static gboolean xfpm_power_dbus_get_power_save_status (XfpmPower * power,
						     gboolean * OUT_save_power,
						     GError ** error);

static gboolean xfpm_power_dbus_get_on_battery (XfpmPower * power,
					      gboolean * OUT_on_battery,
					      GError ** error);

static gboolean xfpm_power_dbus_get_low_battery (XfpmPower * power,
					       gboolean * OUT_low_battery,
					       GError ** error);

#include "org.freedesktop.PowerManagement.h"

static void
xfpm_power_dbus_class_init (XfpmPowerClass * klass)
{
    dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
                                     &dbus_glib_xfpm_power_object_info);
}

static void
xfpm_power_dbus_init (XfpmPower *power)
{
    DBusGConnection *bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

    dbus_g_connection_register_g_object (bus,
                                         "/org/freedesktop/PowerManagement",
                                         G_OBJECT (power));
}

static gboolean xfpm_power_dbus_shutdown (XfpmPower *power,
				        GError **error)
{
    gboolean can_reboot;
    
    g_object_get (G_OBJECT (power->priv->console),
		  "can-shutdown", &can_reboot,
		  NULL);
    
    if ( !can_reboot)
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
                    _("Permission denied"));
        return FALSE;
    }
    
    xfpm_console_kit_shutdown (power->priv->console, error);
    
    return TRUE;
}

static gboolean xfpm_power_dbus_reboot   (XfpmPower *power,
					GError **error)
{
    gboolean can_reboot;
    
    g_object_get (G_OBJECT (power->priv->console),
		  "can-restart", &can_reboot,
		  NULL);
    
    if ( !can_reboot)
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
                    _("Permission denied"));
        return FALSE;
    }
    
    xfpm_console_kit_reboot (power->priv->console, error);
    
    return TRUE;
}
					   
static gboolean xfpm_power_dbus_hibernate (XfpmPower * power,
					 GError **error)
{
    if ( !power->priv->auth_suspend )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
                    _("Permission denied"));
        return FALSE;
	
    }
    
    if (!power->priv->can_hibernate )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_NO_HARDWARE_SUPPORT,
                    _("Suspend not supported"));
        return FALSE;
    }
    
    xfpm_power_sleep (power, "Hibernate", FALSE);
    
    return TRUE;
}

static gboolean xfpm_power_dbus_suspend (XfpmPower * power,
				       GError ** error)
{
    if ( !power->priv->auth_suspend )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
                    _("Permission denied"));
        return FALSE;
	
    }
    
    if (!power->priv->can_suspend )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_NO_HARDWARE_SUPPORT,
                    _("Suspend not supported"));
        return FALSE;
    }
    
    xfpm_power_sleep (power, "Suspend", FALSE);
    
    return TRUE;
}

static gboolean xfpm_power_dbus_can_reboot (XfpmPower * power,
					  gboolean * OUT_can_reboot, 
					  GError ** error)
{
    g_object_get (G_OBJECT (power->priv->console),
		  "can-reboot", OUT_can_reboot,
		  NULL);
		  
    return TRUE;
}

static gboolean xfpm_power_dbus_can_shutdown (XfpmPower * power,
					    gboolean * OUT_can_shutdown, 
					    GError ** error)
{
    g_object_get (G_OBJECT (power->priv->console),
		  "can-shutdown", OUT_can_shutdown,
		  NULL);
    return TRUE;
}

static gboolean xfpm_power_dbus_can_hibernate (XfpmPower * power,
					     gboolean * OUT_can_hibernate,
					     GError ** error)
{
    *OUT_can_hibernate = power->priv->can_hibernate;
    return TRUE;
}

static gboolean xfpm_power_dbus_can_suspend (XfpmPower * power,
					   gboolean * OUT_can_suspend,
					   GError ** error)
{
    *OUT_can_suspend = power->priv->can_suspend;
    
    return TRUE;
}

static gboolean xfpm_power_dbus_get_power_save_status (XfpmPower * power,
						     gboolean * OUT_save_power,
						     GError ** error)
{
    //FIXME
    return TRUE;
}

static gboolean xfpm_power_dbus_get_on_battery (XfpmPower * power,
					      gboolean * OUT_on_battery,
					      GError ** error)
{
    *OUT_on_battery = power->priv->on_battery;
    
    return TRUE;
}

static gboolean xfpm_power_dbus_get_low_battery (XfpmPower * power,
					       gboolean * OUT_low_battery,
					       GError ** error)
{
    *OUT_low_battery = power->priv->on_low_battery;
    
    return TRUE;
}
