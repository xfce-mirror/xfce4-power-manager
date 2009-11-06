/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#include "xfpm-dkp.h"
#include "xfpm-dbus.h"
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
#include "xfpm-config.h"
#include "xfpm-enum-glib.h"
#include "xfpm-debug.h"
#include "xfpm-enum-types.h"

static void xfpm_dkp_finalize     (GObject *object);

static void xfpm_dkp_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec);

static void xfpm_dkp_dbus_class_init (XfpmDkpClass * klass);
static void xfpm_dkp_dbus_init (XfpmDkp *dkp);

#define XFPM_DKP_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_DKP, XfpmDkpPrivate))

struct XfpmDkpPrivate
{
    DBusGConnection *bus;
    
    DBusGProxy      *proxy;
    DBusGProxy      *proxy_prop;
    
    GHashTable      *hash;
    
    XfpmConsoleKit  *console;
    XfpmInhibit	    *inhibit;
    XfpmXfconf      *conf;
    gboolean	     inhibited;
    
    XfpmNotify	    *notify;
#ifdef HAVE_POLKIT
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


G_DEFINE_TYPE (XfpmDkp, xfpm_dkp, G_TYPE_OBJECT)

#ifdef HAVE_POLKIT
static void
xfpm_dkp_check_polkit_auth (XfpmDkp *dkp)
{
    dkp->priv->auth_suspend = xfpm_polkit_check_auth (dkp->priv->polkit, 
						      "org.freedesktop.devicekit.power.suspend");

    dkp->priv->auth_hibernate = xfpm_polkit_check_auth (dkp->priv->polkit, 
							"org.freedesktop.devicekit.power.hibernate");

}
#endif

static void
xfpm_dkp_check_pm (XfpmDkp *dkp, GHashTable *props)
{
    GValue *value;
    gboolean ret;
    
    value = g_hash_table_lookup (props, "CanSuspend");
    
    if (value == NULL) 
    {
	g_warning ("No 'CanSuspend' property");
    }
    ret = g_value_get_boolean (value);
    
    if (ret != dkp->priv->can_suspend) 
    {
	dkp->priv->can_suspend = ret;
    }

    value = g_hash_table_lookup (props, "CanHibernate");
    
    if (value == NULL) 
    {
	g_warning ("No 'CanHibernate' property");
    }
    
    ret = g_value_get_boolean (value);
    
    if (ret != dkp->priv->can_hibernate) 
    {
	dkp->priv->can_hibernate = ret;
    }
}

static void
xfpm_dkp_check_power (XfpmDkp *dkp, GHashTable *props)
{
    GValue *value;
    gboolean on_battery;
    
    value = g_hash_table_lookup (props, "OnBattery");
    
    if (G_LIKELY (value)) 
    {
	on_battery = g_value_get_boolean (value);
    
	if (on_battery != dkp->priv->on_battery ) 
	{
	    GList *list;
	    guint len, i;
	    g_signal_emit (G_OBJECT (dkp), signals [ON_BATTERY_CHANGED], 0, on_battery);
	    dkp->priv->on_battery = on_battery;
	    list = g_hash_table_get_values (dkp->priv->hash);
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
xfpm_dkp_check_lid (XfpmDkp *dkp, GHashTable *props)
{
    GValue *value;
    
    value = g_hash_table_lookup (props, "LidIsPresent");
    
    if (value == NULL) 
    {
	g_warning ("No 'LidIsPresent' property");
	return;
    }

    dkp->priv->lid_is_present = g_value_get_boolean (value);

    if (dkp->priv->lid_is_present) 
    {
	gboolean closed;
	
	value = g_hash_table_lookup (props, "LidIsClosed");
    
	if (value == NULL) 
	{
	    g_warning ("No 'LidIsClosed' property");
	    return;
	}
	
	closed = g_value_get_boolean (value);
	
	if (closed != dkp->priv->lid_is_closed ) 
	{
	    dkp->priv->lid_is_closed = closed;
	    g_signal_emit (G_OBJECT (dkp), signals [LID_CHANGED], 0, dkp->priv->lid_is_closed);
	}
    }
}

static void
xfpm_dkp_check_daemon_version (XfpmDkp *dkp, GHashTable *props)
{
    GValue *value;
    gint dkp_version;
	
    value = g_hash_table_lookup (props, "DaemonVersion");

    if (value == NULL) 
    {
	g_warning ("No 'DaemonVersion' property");
	/*
	 * Version less than 011 uses dash-dash
	 */
	value = g_hash_table_lookup (props, "daemon-version");
	
	if (value == NULL) 
	{
	    g_warning ("No 'daemon-version' property");
	    goto out;
	}
    }
    
    dkp->priv->daemon_version = g_strdup (g_value_get_string (value));
    XFPM_DEBUG ("Dkp daemon version %s", dkp->priv->daemon_version);
    dkp_version = strtol (dkp->priv->daemon_version, NULL, 10);
    
    if ( dkp_version < 15)
    {
	XfconfChannel *channel;
	gboolean show_error;
	
	channel = xfpm_xfconf_get_channel (dkp->priv->conf);
	
	show_error = xfconf_channel_get_bool (channel, PROPERTIES_PREFIX "show-dkp-version-error", TRUE);
	
	XFPM_WARNING ("Dkp version %d is less than the required minimum version 011", dkp_version);
	
	
	if ( show_error )
	{
	    GError *error = NULL;
	    gchar *message;
	    message = g_strdup_printf ("%s %s", 
				   _("Xfce Power Manager requires version 011 of devicekit-power " 
				   "to work properly while the version found is"), 
				   dkp->priv->daemon_version);
		
	    g_set_error (&error, 0, 0, message);
	    xfce_dialog_show_error (NULL, error, "%s", _("Devicekit-power version 011 or above not found"));
	    xfconf_channel_set_bool (channel, PROPERTIES_PREFIX "show-dkp-version-error", FALSE);
	    g_free (message);
	    g_error_free (error);
	}
	
	g_error (_("Devicekit-power version 011 or above not found"));
    }
out:
    ;
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
xfpm_dkp_get_properties (XfpmDkp *dkp)
{
    GHashTable *props;
    
    props = xfpm_dbus_get_interface_properties (dkp->priv->proxy_prop, DKP_IFACE);
    
    xfpm_dkp_check_pm (dkp, props);
    xfpm_dkp_check_lid (dkp, props);
    xfpm_dkp_check_power (dkp, props);

    if ( dkp->priv->daemon_version == NULL )
    {
	xfpm_dkp_check_daemon_version (dkp, props);
    }
out:
    g_hash_table_destroy (props);
}

static void
xfpm_dkp_report_error (XfpmDkp *dkp, const gchar *error, const gchar *icon_name)
{
    GtkStatusIcon *battery = NULL;
    guint i, len;
    GList *list;
    
    list = g_hash_table_get_values (dkp->priv->hash);
    len = g_list_length (list);
    
    for ( i = 0; i < len; i++)
    {
	XfpmDkpDeviceType type;
	battery = g_list_nth_data (list, i);
	type = xfpm_battery_get_device_type (XFPM_BATTERY (battery));
	if ( type == XFPM_DKP_DEVICE_TYPE_BATTERY ||
	     type == XFPM_DKP_DEVICE_TYPE_UPS )
	     break;
    }
    
    xfpm_notify_show_notification (dkp->priv->notify, 
				   _("Xfce power manager"), 
				   error, 
				   icon_name,
				   10000,
				   FALSE,
				   XFPM_NOTIFY_CRITICAL,
				   battery);
    
}

static void
xfpm_dkp_sleep (XfpmDkp *dkp, const gchar *sleep, gboolean force)
{
    GError *error = NULL;
    gboolean lock_screen;
    
    if ( dkp->priv->inhibited && force == FALSE)
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
    
    g_signal_emit (G_OBJECT (dkp), signals [SLEEPING], 0);
    xfpm_send_message_to_network_manager ("sleep");
        
    g_object_get (G_OBJECT (dkp->priv->conf),
		  LOCK_SCREEN_ON_SLEEP, &lock_screen,
		  NULL);
    
    if ( lock_screen )
    {
	g_usleep (2000000); /* 2 seconds */
	xfpm_lock_screen ();
    }
    
    dbus_g_proxy_call (dkp->priv->proxy, sleep, &error,
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
	    if ( !g_strcmp0 (sleep, "Hibernate") )
		icon_name = XFPM_HIBERNATE_ICON;
	    else
		icon_name = XFPM_SUSPEND_ICON;
	    
	    xfpm_dkp_report_error (dkp, error->message, icon_name);
	    g_error_free (error);
	}
    }
    
    g_signal_emit (G_OBJECT (dkp), signals [WAKING_UP], 0);
    xfpm_send_message_to_network_manager ("wake");
}

static void
xfpm_dkp_hibernate_cb (XfpmDkp *dkp)
{
    xfpm_dkp_sleep (dkp, "Hibernate", FALSE);
}

static void
xfpm_dkp_suspend_cb (XfpmDkp *dkp)
{
    xfpm_dkp_sleep (dkp, "Suspend", FALSE);
}

static void
xfpm_dkp_hibernate_clicked (XfpmDkp *dkp)
{
    gtk_widget_destroy (dkp->priv->dialog );
    dkp->priv->dialog = NULL;
    xfpm_dkp_sleep (dkp, "Hibernate", TRUE);
}

static void
xfpm_dkp_suspend_clicked (XfpmDkp *dkp)
{
    gtk_widget_destroy (dkp->priv->dialog );
    dkp->priv->dialog = NULL;
    xfpm_dkp_sleep (dkp, "Suspend", TRUE);
}

static void
xfpm_dkp_shutdown_clicked (XfpmDkp *dkp)
{
    gtk_widget_destroy (dkp->priv->dialog );
    dkp->priv->dialog = NULL;
    g_signal_emit (G_OBJECT (dkp), signals [SHUTDOWN], 0);
}

static void
xfpm_dkp_battery_info_cb (GtkStatusIcon *icon)
{
    xfpm_battery_show_info (XFPM_BATTERY (icon));
}

static void
xfpm_dkp_tray_exit_activated_cb (gpointer data)
{
    gboolean ret;
    
    ret = xfce_dialog_confirm (NULL, 
			       GTK_STOCK_YES, 
			       _("Quit"),
			       _("All running instances of the power manager will exit"),
			       "%s",
			        _("Quit Xfce power manager?"));
    if ( ret )
    {
	xfpm_quit ();
    }
}

static void
xfpm_dkp_show_tray_menu (XfpmDkp *dkp, 
			 GtkStatusIcon *icon, 
			 guint button, 
			 guint activate_time,
			 gboolean show_info_item)
{
    GtkWidget *menu, *mi, *img;

    menu = gtk_menu_new();

    // Hibernate menu option
    mi = gtk_image_menu_item_new_with_label (_("Hibernate"));
    img = gtk_image_new_from_icon_name (XFPM_HIBERNATE_ICON, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);
    gtk_widget_set_sensitive (mi, FALSE);
    
    if ( dkp->priv->can_hibernate && dkp->priv->auth_hibernate)
    {
	gtk_widget_set_sensitive (mi, TRUE);
	g_signal_connect_swapped (G_OBJECT (mi), "activate",
				  G_CALLBACK (xfpm_dkp_hibernate_cb), dkp);
    }
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    // Suspend menu option
    mi = gtk_image_menu_item_new_with_label (_("Suspend"));
    img = gtk_image_new_from_icon_name (XFPM_SUSPEND_ICON, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);
    
    gtk_widget_set_sensitive (mi, FALSE);
    
    if ( dkp->priv->can_suspend && dkp->priv->auth_hibernate)
    {
	gtk_widget_set_sensitive (mi, TRUE);
	g_signal_connect_swapped (mi, "activate",
				  G_CALLBACK (xfpm_dkp_suspend_cb), dkp);
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
    if ( show_info_item )
    {
	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
	// Battery informations
    
	mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_INFO, NULL);
	
	gtk_widget_set_sensitive (mi,FALSE);
	gtk_widget_set_sensitive (mi,TRUE);
	
	g_signal_connect_swapped (mi,"activate",
				  G_CALLBACK (xfpm_dkp_battery_info_cb), icon);
			 
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    
	// Separator
	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    }
	
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_HELP, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate", G_CALLBACK (xfpm_help), NULL);
	
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate", G_CALLBACK (xfpm_about), _("Xfce Power Manager"));
    
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
    g_signal_connect_swapped (mi, "activate", G_CALLBACK (xfpm_dkp_tray_exit_activated_cb), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

    g_signal_connect (menu, "selection-done",
		      G_CALLBACK (gtk_widget_destroy), NULL);

    // Popup the menu
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		   gtk_status_icon_position_menu, 
		   icon, button, activate_time);
    
}

static void 
xfpm_dkp_show_tray_menu_battery (GtkStatusIcon *icon, guint button, 
			         guint activate_time, XfpmDkp *dkp)
{
    xfpm_dkp_show_tray_menu (dkp, icon, button, activate_time, TRUE);
}

static XfpmBatteryCharge
xfpm_dkp_get_current_charge_state (XfpmDkp *dkp)
{
    GList *list;
    XfpmBatteryCharge charge = XFPM_BATTERY_CHARGE_UNKNOWN;
    guint len, i;
    
    list = g_hash_table_get_values (dkp->priv->hash);
    len = g_list_length (list);
    
    for ( i = 0; i < len; i++)
    {
	charge = MAX (charge, xfpm_battery_get_charge (XFPM_BATTERY (g_list_nth_data (list, i))));
    }
    
    return charge;
}

static void
xfpm_dkp_notify_action_callback (NotifyNotification *n, gchar *action, XfpmDkp *dkp)
{
    if ( !g_strcmp0 (action, "Shutdown") )
	g_signal_emit (G_OBJECT (dkp), signals [SHUTDOWN], 0);
    else
	xfpm_dkp_sleep (dkp, action, TRUE);
}

static void
xfpm_dkp_add_actions_to_notification (XfpmDkp *dkp, NotifyNotification *n)
{
    gboolean can_shutdown;
    
    g_object_get (G_OBJECT (dkp->priv->console),
		  "can-shutdown", &can_shutdown,
		  NULL);
		  
    if (  dkp->priv->can_hibernate && dkp->priv->auth_hibernate )
    {
        xfpm_notify_add_action_to_notification(
			       dkp->priv->notify,
			       n,
                               "Hibernate",
                               _("Hibernate the system"),
                               (NotifyActionCallback)xfpm_dkp_notify_action_callback,
                               dkp);      
    }
    
    if (  dkp->priv->can_suspend && dkp->priv->auth_suspend )
    {
        xfpm_notify_add_action_to_notification(
			       dkp->priv->notify,
			       n,
                               "Suspend",
                               _("Suspend the system"),
                               (NotifyActionCallback)xfpm_dkp_notify_action_callback,
                               dkp);      
    }
    
    if (can_shutdown )
	xfpm_notify_add_action_to_notification(
				   dkp->priv->notify,
				   n,
				   "Shutdown",
				   _("Shutdown the system"),
				   (NotifyActionCallback)xfpm_dkp_notify_action_callback,
				   dkp);    
}

static void
xfpm_dkp_show_critical_action_notification (XfpmDkp *dkp, XfpmBattery *battery)
{
    const gchar *message;
    NotifyNotification *n;
    
    message = _("System is running on low power. "\
               "Save your work to avoid losing data");
	      
    n = 
	xfpm_notify_new_notification (dkp->priv->notify, 
				      _("Xfce power manager"), 
				      message, 
				      gtk_status_icon_get_icon_name (GTK_STATUS_ICON (battery)),
				      20000,
				      XFPM_NOTIFY_CRITICAL,
				      GTK_STATUS_ICON (battery));
    
    xfpm_dkp_add_actions_to_notification (dkp, n);
    xfpm_notify_critical (dkp->priv->notify, n);

}

static void
xfpm_dkp_close_critical_dialog (XfpmDkp *dkp)
{
    gtk_widget_destroy (dkp->priv->dialog);
    dkp->priv->dialog = NULL;
}

static void
xfpm_dkp_show_critical_action_gtk (XfpmDkp *dkp)
{
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *img;
    GtkWidget *cancel;
    const gchar *message;
    gboolean can_shutdown;
    
    g_object_get (G_OBJECT (dkp->priv->console),
		  "can-shutdown", &can_shutdown,
		  NULL);
    
    message = _("System is running on low power. "\
               "Save your work to avoid losing data");
    
    dialog = gtk_dialog_new_with_buttons (_("Xfce Power Manager"), NULL, GTK_DIALOG_MODAL,
                                          NULL);
    
    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_CANCEL);
    
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_box_pack_start_defaults (GTK_BOX (content_area), gtk_label_new (message));
    
    if ( dkp->priv->can_hibernate && dkp->priv->auth_hibernate )
    {
	GtkWidget *hibernate;
	hibernate = gtk_button_new_with_label (_("Hibernate"));
	img = gtk_image_new_from_icon_name (XFPM_HIBERNATE_ICON, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (hibernate), img);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), hibernate, GTK_RESPONSE_NONE);
	
	g_signal_connect_swapped (hibernate, "clicked",
			          G_CALLBACK (xfpm_dkp_hibernate_clicked), dkp);
    }
    
    if ( dkp->priv->can_suspend && dkp->priv->auth_suspend )
    {
	GtkWidget *suspend;
	
	suspend = gtk_button_new_with_label (_("Suspend"));
	img = gtk_image_new_from_icon_name (XFPM_SUSPEND_ICON, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (suspend), img);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), suspend, GTK_RESPONSE_NONE);
	
	g_signal_connect_swapped (suspend, "clicked",
			          G_CALLBACK (xfpm_dkp_suspend_clicked), dkp);
    }
    
    if ( can_shutdown )
    {
	GtkWidget *shutdown;
	
	shutdown = gtk_button_new_with_label (_("Shutdown"));
	img = gtk_image_new_from_icon_name (XFPM_SUSPEND_ICON, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (shutdown), img);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), shutdown, GTK_RESPONSE_NONE);
	
	g_signal_connect_swapped (shutdown, "clicked",
			          G_CALLBACK (xfpm_dkp_shutdown_clicked), dkp);
    }
    
    cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancel, GTK_RESPONSE_NONE);
    
    g_signal_connect_swapped (cancel, "clicked",
			      G_CALLBACK (xfpm_dkp_close_critical_dialog), dkp);
    
    g_signal_connect_swapped (dialog, "destroy",
			      G_CALLBACK (xfpm_dkp_close_critical_dialog), dkp);
    if ( dkp->priv->dialog )
    {
	gtk_widget_destroy (dkp->priv->dialog);
	dkp->priv->dialog = NULL;
	
    }
    dkp->priv->dialog = dialog;
    gtk_widget_show_all (dialog);
}

static void
xfpm_dkp_show_critical_action (XfpmDkp *dkp, XfpmBattery *battery)
{
    gboolean supports_actions;
    
    g_object_get (G_OBJECT (dkp->priv->notify),
		  "actions", &supports_actions,
		  NULL);
		  
    if ( supports_actions )
	xfpm_dkp_show_critical_action_notification (dkp, battery);
    else
	xfpm_dkp_show_critical_action_gtk (dkp);
}

static void
xfpm_dkp_process_critical_action (XfpmDkp *dkp, XfpmShutdownRequest req)
{
    if ( req == XFPM_ASK )
	g_signal_emit (G_OBJECT (dkp), signals [ASK_SHUTDOWN], 0);
    else if ( req == XFPM_DO_SUSPEND )
	xfpm_dkp_sleep (dkp, "Suspend", TRUE);
    else if ( req == XFPM_DO_HIBERNATE )
	xfpm_dkp_sleep (dkp, "Hibernate", TRUE);
    else if ( req == XFPM_DO_SHUTDOWN )
	g_signal_emit (G_OBJECT (dkp), signals [SHUTDOWN], 0);
}

static void
xfpm_dkp_system_on_low_power (XfpmDkp *dkp, XfpmBattery *battery)
{
    XfpmShutdownRequest critical_action;
    
    g_object_get (G_OBJECT (dkp->priv->conf),
	          CRITICAL_BATT_ACTION_CFG, &critical_action,
		  NULL);

    XFPM_DEBUG ("System is running on low power");
    XFPM_DEBUG_ENUM (critical_action, XFPM_TYPE_SHUTDOWN_REQUEST, "Critical battery action");

    if ( critical_action == XFPM_DO_NOTHING )
    {
	xfpm_dkp_show_critical_action (dkp, battery);
    }
    else
    {
	xfpm_dkp_process_critical_action (dkp, critical_action);
    }
}

static void
xfpm_dkp_battery_charge_changed_cb (XfpmBattery *battery, XfpmDkp *dkp)
{
    gboolean notify;
    XfpmBatteryCharge battery_charge;
    XfpmBatteryCharge current_charge;
    
    battery_charge = xfpm_battery_get_charge (battery);
    current_charge = xfpm_dkp_get_current_charge_state (dkp);
    
    if ( current_charge == XFPM_BATTERY_CHARGE_CRITICAL && dkp->priv->on_battery)
    {
	xfpm_dkp_system_on_low_power (dkp, battery);
	dkp->priv->on_low_battery = TRUE;
	g_signal_emit (G_OBJECT (dkp), signals [LOW_BATTERY_CHANGED], 0, dkp->priv->on_low_battery);
	return;
    }
    
    if ( dkp->priv->on_low_battery )
    {
	dkp->priv->on_low_battery = FALSE;
	g_signal_emit (G_OBJECT (dkp), signals [LOW_BATTERY_CHANGED], 0, dkp->priv->on_low_battery);
    }
    
    g_object_get (G_OBJECT (dkp->priv->conf),
		  GENERAL_NOTIFICATION_CFG, &notify,
		  NULL);
    
    if ( current_charge == XFPM_BATTERY_CHARGE_LOW && dkp->priv->on_battery )
    {
	if ( notify )
	    xfpm_notify_show_notification (dkp->priv->notify, 
					   _("Xfce power manager"), 
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
	    xfpm_notify_show_notification (dkp->priv->notify, 
					   _("Xfce power manager"), 
					   _("Battery charge level is low"), 
					   gtk_status_icon_get_icon_name (GTK_STATUS_ICON (battery)),
					   10000,
					   FALSE,
					   XFPM_NOTIFY_NORMAL,
					   GTK_STATUS_ICON (battery));
    }
    
    /*Current charge is okay now, then close the dialog*/
    if ( dkp->priv->dialog )
    {
	gtk_widget_destroy (dkp->priv->dialog);
	dkp->priv->dialog = NULL;
    }
}

static void
xfpm_dkp_add_device (XfpmDkp *dkp, const gchar *object_path)
{
    DBusGProxy *proxy_prop;
    guint device_type;
    GValue value;
    
    proxy_prop = dbus_g_proxy_new_for_name (dkp->priv->bus, 
					    DKP_NAME,
					    object_path,
					    DBUS_INTERFACE_PROPERTIES);
				       
    if ( !proxy_prop )
    {
	g_warning ("Unable to create proxy for : %s", object_path);
	return;
    }
    
    value = xfpm_dbus_get_interface_property (proxy_prop, DKP_IFACE_DEVICE, "Type");
    
    device_type = g_value_get_uint (&value);
    
    if ( device_type == XFPM_DKP_DEVICE_TYPE_LINE_POWER )
    {
	//FIXME: Handel adaptor.
    }
    else if ( device_type == XFPM_DKP_DEVICE_TYPE_BATTERY || 
	      device_type == XFPM_DKP_DEVICE_TYPE_UPS     ||
	      device_type == XFPM_DKP_DEVICE_TYPE_MOUSE   ||
	      device_type == XFPM_DKP_DEVICE_TYPE_KBD     ||
	      device_type == XFPM_DKP_DEVICE_TYPE_PHONE)
    {
	GtkStatusIcon *battery;
	DBusGProxy *proxy;
	XFPM_DEBUG ("Battery device detected at : %s", object_path);
	proxy = dbus_g_proxy_new_for_name (dkp->priv->bus,
					   DKP_NAME,
					   object_path,
					   DKP_IFACE_DEVICE);
	battery = xfpm_battery_new ();
	gtk_status_icon_set_visible (battery, FALSE);
	xfpm_battery_monitor_device (XFPM_BATTERY (battery), proxy, proxy_prop, device_type);
	g_hash_table_insert (dkp->priv->hash, g_strdup (object_path), battery);
	
	g_signal_connect (battery, "popup-menu",
			  G_CALLBACK (xfpm_dkp_show_tray_menu_battery), dkp);
	
	g_signal_connect (battery, "battery-charge-changed",
			  G_CALLBACK (xfpm_dkp_battery_charge_changed_cb), dkp);
    }
    else 
    {
	g_warning ("Unable to monitor unkown power device with object_path : %s", object_path);
	g_object_unref (proxy_prop);
    }
}

/*
 * Get the object path of all the power devices
 * on dkp.
 */
static GPtrArray *
xfpm_dkp_enumerate_devices (XfpmDkp *dkp)
{
    gboolean ret;
    GError *error = NULL;
    GPtrArray *array = NULL;
    GType g_type_array;

    g_type_array = dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH);
    
    ret = dbus_g_proxy_call (dkp->priv->proxy, "EnumerateDevices", &error,
			     G_TYPE_INVALID,
			     g_type_array, &array,
			     G_TYPE_INVALID);
    if (!ret) 
    {
	g_critical ("Couldn't enumerate power devices: %s", error->message);
	g_error_free (error);
    }
    
    return array;
}

static void
xfpm_dkp_get_power_devices (XfpmDkp *dkp)
{
    GPtrArray *array = NULL;
    guint i;
    
    array = xfpm_dkp_enumerate_devices (dkp);
    
    for ( i = 0; i < array->len; i++)
    {
	const gchar *object_path = ( const gchar *) g_ptr_array_index (array, i);
	XFPM_DEBUG ("Power device detected at : %s", object_path);
	xfpm_dkp_add_device (dkp, object_path);
    }
    
    g_ptr_array_free (array, TRUE);
}

static void
xfpm_dkp_remove_device (XfpmDkp *dkp, const gchar *object_path)
{
    g_hash_table_remove (dkp->priv->hash, object_path);
}

static void
xfpm_dkp_inhibit_changed_cb (XfpmInhibit *inhibit, gboolean is_inhibit, XfpmDkp *dkp)
{
    dkp->priv->inhibited = is_inhibit;
}

static void
xfpm_dkp_changed_cb (DBusGProxy *proxy, XfpmDkp *dkp)
{
    xfpm_dkp_get_properties (dkp);
}

static void
xfpm_dkp_device_added_cb (DBusGProxy *proxy, const gchar *object_path, XfpmDkp *dkp)
{
    xfpm_dkp_add_device (dkp, object_path);
}

static void
xfpm_dkp_device_removed_cb (DBusGProxy *proxy, const gchar *object_path, XfpmDkp *dkp)
{
    xfpm_dkp_remove_device (dkp, object_path);
}

static void
xfpm_dkp_device_changed_cb (DBusGProxy *proxy, const gchar *object_path, XfpmDkp *dkp)
{
    XfpmBattery *battery;
    
    battery = g_hash_table_lookup (dkp->priv->hash, object_path);
    
    if ( battery )
    {
	
    }
}

#ifdef HAVE_POLKIT
static void
xfpm_dkp_polkit_auth_changed_cb (XfpmDkp *dkp)
{
    xfpm_dkp_check_polkit_auth (dkp);
}
#endif

static void
xfpm_dkp_class_init (XfpmDkpClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_dkp_finalize;

    object_class->get_property = xfpm_dkp_get_property;

    signals [ON_BATTERY_CHANGED] = 
        g_signal_new ("on-battery-changed",
                      XFPM_TYPE_DKP,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmDkpClass, on_battery_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [LOW_BATTERY_CHANGED] = 
        g_signal_new ("low-battery-changed",
                      XFPM_TYPE_DKP,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmDkpClass, low_battery_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [LID_CHANGED] = 
        g_signal_new ("lid-changed",
                      XFPM_TYPE_DKP,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmDkpClass, lid_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals [WAKING_UP] = 
        g_signal_new ("waking-up",
                      XFPM_TYPE_DKP,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmDkpClass, waking_up),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [SLEEPING] = 
        g_signal_new ("sleeping",
                      XFPM_TYPE_DKP,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmDkpClass, sleeping),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [ASK_SHUTDOWN] = 
        g_signal_new ("ask-shutdown",
                      XFPM_TYPE_DKP,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmDkpClass, ask_shutdown),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    signals [SHUTDOWN] = 
        g_signal_new ("shutdown",
                      XFPM_TYPE_DKP,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmDkpClass, shutdown),
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

    g_type_class_add_private (klass, sizeof (XfpmDkpPrivate));
    
    xfpm_dkp_dbus_class_init (klass);
}

static void
xfpm_dkp_init (XfpmDkp *dkp)
{
    GError *error = NULL;
    
    dkp->priv = XFPM_DKP_GET_PRIVATE (dkp);
    
    dkp->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    dkp->priv->lid_is_present  = FALSE;
    dkp->priv->lid_is_closed   = FALSE;
    dkp->priv->on_battery      = FALSE;
    dkp->priv->on_low_battery  = FALSE;
    dkp->priv->daemon_version  = NULL;
    dkp->priv->can_suspend     = FALSE;
    dkp->priv->can_hibernate   = FALSE;
    dkp->priv->auth_hibernate  = TRUE;
    dkp->priv->auth_suspend    = TRUE;
    dkp->priv->dialog          = NULL;
    
    dkp->priv->inhibit = xfpm_inhibit_new ();
    dkp->priv->notify  = xfpm_notify_new ();
    dkp->priv->conf    = xfpm_xfconf_new ();
    dkp->priv->console = xfpm_console_kit_new ();
#ifdef HAVE_POLKIT
    dkp->priv->polkit  = xfpm_polkit_get ();
    g_signal_connect_swapped (dkp->priv->polkit, "auth-changed",
			      G_CALLBACK (xfpm_dkp_polkit_auth_changed_cb), dkp);
#endif
    
    g_signal_connect (dkp->priv->inhibit, "has-inhibit-changed",
		      G_CALLBACK (xfpm_dkp_inhibit_changed_cb), dkp);
    
    dkp->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Unable to connect to the system bus : %s", error->message);
	g_error_free (error);
	goto out;
    }
    
    dkp->priv->proxy = dbus_g_proxy_new_for_name (dkp->priv->bus,
					          DKP_NAME,
						  DKP_PATH,
						  DKP_IFACE);
    if (dkp->priv->proxy == NULL) 
    {
	g_critical ("Unable to create proxy for %s", DKP_NAME);
	goto out;
    }
    
    dkp->priv->proxy_prop = dbus_g_proxy_new_for_name (dkp->priv->bus,
						       DKP_NAME,
						       DKP_PATH,
						       DBUS_INTERFACE_PROPERTIES);
    if (dkp->priv->proxy_prop == NULL) 
    {
	g_critical ("Unable to create proxy for %s", DKP_NAME);
	goto out;
    }
    
    xfpm_dkp_get_power_devices (dkp);
    xfpm_dkp_get_properties (dkp);
#ifdef HAVE_POLKIT
    xfpm_dkp_check_polkit_auth (dkp);
#endif

    dbus_g_proxy_add_signal (dkp->priv->proxy, "Changed", G_TYPE_INVALID);
    dbus_g_proxy_add_signal (dkp->priv->proxy, "DeviceAdded", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (dkp->priv->proxy, "DeviceRemoved", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (dkp->priv->proxy, "DeviceChanged", G_TYPE_STRING, G_TYPE_INVALID);
    
    dbus_g_proxy_connect_signal (dkp->priv->proxy, "Changed",
				 G_CALLBACK (xfpm_dkp_changed_cb), dkp, NULL);
    dbus_g_proxy_connect_signal (dkp->priv->proxy, "DeviceRemoved",
				 G_CALLBACK (xfpm_dkp_device_removed_cb), dkp, NULL);
    dbus_g_proxy_connect_signal (dkp->priv->proxy, "DeviceAdded",
				 G_CALLBACK (xfpm_dkp_device_added_cb), dkp, NULL);
   
    dbus_g_proxy_connect_signal (dkp->priv->proxy, "DeviceChanged",
				 G_CALLBACK (xfpm_dkp_device_changed_cb), dkp, NULL);

    
out:
    xfpm_dkp_dbus_init (dkp);

    /*
     * Emit org.freedesktop.PowerManagement session signals on startup
     */
    g_signal_emit (G_OBJECT (dkp), signals [ON_BATTERY_CHANGED], 0, dkp->priv->on_battery);
}

static void xfpm_dkp_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
    XfpmDkp *dkp;
    dkp = XFPM_DKP (object);

    switch (prop_id)
    {
	case PROP_ON_BATTERY:
	    g_value_set_boolean (value, dkp->priv->on_battery);
	    break;
	case PROP_AUTH_HIBERNATE:
	    g_value_set_boolean (value, dkp->priv->auth_hibernate);
	    break;
	case PROP_AUTH_SUSPEND:
	    g_value_set_boolean (value, dkp->priv->auth_suspend);
	    break;
	case PROP_CAN_SUSPEND:
	    g_value_set_boolean (value, dkp->priv->can_suspend);
	    break;
	case PROP_CAN_HIBERNATE:
	    g_value_set_boolean (value, dkp->priv->can_hibernate);
	    break;
	case PROP_HAS_LID:
	    g_value_set_boolean (value, dkp->priv->lid_is_present);
	    break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
xfpm_dkp_finalize (GObject *object)
{
    XfpmDkp *dkp;

    dkp = XFPM_DKP (object);
    
    g_free (dkp->priv->daemon_version);
    
    g_object_unref (dkp->priv->inhibit);
    g_object_unref (dkp->priv->notify);
    g_object_unref (dkp->priv->conf);
    g_object_unref (dkp->priv->console);
    
    dbus_g_connection_unref (dkp->priv->bus);
    
    if ( dkp->priv->proxy )
    {
	dbus_g_proxy_disconnect_signal (dkp->priv->proxy, "Changed",
					G_CALLBACK (xfpm_dkp_changed_cb), dkp);
	dbus_g_proxy_disconnect_signal (dkp->priv->proxy, "DeviceRemoved",
					G_CALLBACK (xfpm_dkp_device_removed_cb), dkp);
	dbus_g_proxy_disconnect_signal (dkp->priv->proxy, "DeviceAdded",
					G_CALLBACK (xfpm_dkp_device_added_cb), dkp);
	dbus_g_proxy_disconnect_signal (dkp->priv->proxy, "DeviceChanged",
					G_CALLBACK (xfpm_dkp_device_changed_cb), dkp);
	g_object_unref (dkp->priv->proxy);
    }
    
    if ( dkp->priv->proxy_prop )
	g_object_unref (dkp->priv->proxy_prop);

    g_hash_table_destroy (dkp->priv->hash);

#ifdef HAVE_POLKIT
    g_object_unref (dkp->priv->polkit);
#endif

    G_OBJECT_CLASS (xfpm_dkp_parent_class)->finalize (object);
}

XfpmDkp *
xfpm_dkp_get (void)
{
    static gpointer xfpm_dkp_object = NULL;
    
    if ( G_LIKELY (xfpm_dkp_object != NULL ) )
    {
	g_object_ref (xfpm_dkp_object);
    }
    else
    {
	xfpm_dkp_object = g_object_new (XFPM_TYPE_DKP, NULL);
	g_object_add_weak_pointer (xfpm_dkp_object, &xfpm_dkp_object);
    }
    
    return XFPM_DKP (xfpm_dkp_object);
}

void xfpm_dkp_suspend (XfpmDkp *dkp, gboolean force)
{
    xfpm_dkp_sleep (dkp, "Suspend", force);
}

void xfpm_dkp_hibernate (XfpmDkp *dkp, gboolean force)
{
    xfpm_dkp_sleep (dkp, "Hibernate", force);
}

gboolean xfpm_dkp_has_battery (XfpmDkp *dkp)
{
    GtkStatusIcon *battery = NULL;
    guint i, len;
    GList *list;
    
    gboolean ret = FALSE;
    
    list = g_hash_table_get_values (dkp->priv->hash);
    len = g_list_length (list);
    
    for ( i = 0; i < len; i++)
    {
	XfpmDkpDeviceType type;
	battery = g_list_nth_data (list, i);
	type = xfpm_battery_get_device_type (XFPM_BATTERY (battery));
	if ( type == XFPM_DKP_DEVICE_TYPE_BATTERY ||
	     type == XFPM_DKP_DEVICE_TYPE_UPS )
	{
	    ret = TRUE;
	    break;
	}
    }
    
    return ret;
}

/*
 * 
 * DBus server implementation for org.freedesktop.PowerManagement
 * 
 */
static gboolean xfpm_dkp_dbus_shutdown (XfpmDkp *dkp,
				        GError **error);

static gboolean xfpm_dkp_dbus_reboot   (XfpmDkp *dkp,
					GError **error);
					   
static gboolean xfpm_dkp_dbus_hibernate (XfpmDkp * dkp,
					 GError **error);

static gboolean xfpm_dkp_dbus_suspend (XfpmDkp * dkp,
				       GError ** error);

static gboolean xfpm_dkp_dbus_can_reboot (XfpmDkp * dkp,
					  gboolean * OUT_can_reboot, 
					  GError ** error);

static gboolean xfpm_dkp_dbus_can_shutdown (XfpmDkp * dkp,
					    gboolean * OUT_can_reboot, 
					    GError ** error);

static gboolean xfpm_dkp_dbus_can_hibernate (XfpmDkp * dkp,
					     gboolean * OUT_can_hibernate,
					     GError ** error);

static gboolean xfpm_dkp_dbus_can_suspend (XfpmDkp * dkp,
					   gboolean * OUT_can_suspend,
					   GError ** error);

static gboolean xfpm_dkp_dbus_get_power_save_status (XfpmDkp * dkp,
						     gboolean * OUT_save_power,
						     GError ** error);

static gboolean xfpm_dkp_dbus_get_on_battery (XfpmDkp * dkp,
					      gboolean * OUT_on_battery,
					      GError ** error);

static gboolean xfpm_dkp_dbus_get_low_battery (XfpmDkp * dkp,
					       gboolean * OUT_low_battery,
					       GError ** error);

#include "org.freedesktop.PowerManagement.h"

static void
xfpm_dkp_dbus_class_init (XfpmDkpClass * klass)
{
    dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
                                     &dbus_glib_xfpm_dkp_object_info);
}

static void
xfpm_dkp_dbus_init (XfpmDkp *dkp)
{
    DBusGConnection *bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

    dbus_g_connection_register_g_object (bus,
                                         "/org/freedesktop/PowerManagement",
                                         G_OBJECT (dkp));
}

static gboolean xfpm_dkp_dbus_shutdown (XfpmDkp *dkp,
				        GError **error)
{
    gboolean can_reboot;
    
    g_object_get (G_OBJECT (dkp->priv->console),
		  "can-shutdown", &can_reboot,
		  NULL);
    
    if ( !can_reboot)
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
                    _("Permission denied"));
        return FALSE;
    }
    
    xfpm_console_kit_shutdown (dkp->priv->console, error);
    
    return TRUE;
}

static gboolean xfpm_dkp_dbus_reboot   (XfpmDkp *dkp,
					GError **error)
{
    gboolean can_reboot;
    
    g_object_get (G_OBJECT (dkp->priv->console),
		  "can-reboot", &can_reboot,
		  NULL);
    
    if ( !can_reboot)
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
                    _("Permission denied"));
        return FALSE;
    }
    
    xfpm_console_kit_reboot (dkp->priv->console, error);
    
    return TRUE;
}
					   
static gboolean xfpm_dkp_dbus_hibernate (XfpmDkp * dkp,
					 GError **error)
{
    if ( !dkp->priv->auth_suspend )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
                    _("Permission denied"));
        return FALSE;
	
    }
    
    if (!dkp->priv->can_hibernate )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_NO_HARDWARE_SUPPORT,
                    _("Suspend not supported"));
        return FALSE;
    }
    
    xfpm_dkp_sleep (dkp, "Hibernate", FALSE);
    
    return TRUE;
}

static gboolean xfpm_dkp_dbus_suspend (XfpmDkp * dkp,
				       GError ** error)
{
    if ( !dkp->priv->auth_suspend )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_PERMISSION_DENIED,
                    _("Permission denied"));
        return FALSE;
	
    }
    
    if (!dkp->priv->can_suspend )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_NO_HARDWARE_SUPPORT,
                    _("Suspend not supported"));
        return FALSE;
    }
    
    xfpm_dkp_sleep (dkp, "Suspend", FALSE);
    
    return TRUE;
}

static gboolean xfpm_dkp_dbus_can_reboot (XfpmDkp * dkp,
					  gboolean * OUT_can_reboot, 
					  GError ** error)
{
    g_object_get (G_OBJECT (dkp->priv->console),
		  "can-reboot", OUT_can_reboot,
		  NULL);
		  
    return TRUE;
}

static gboolean xfpm_dkp_dbus_can_shutdown (XfpmDkp * dkp,
					    gboolean * OUT_can_shutdown, 
					    GError ** error)
{
    g_object_get (G_OBJECT (dkp->priv->console),
		  "can-shutdown", OUT_can_shutdown,
		  NULL);
    return TRUE;
}

static gboolean xfpm_dkp_dbus_can_hibernate (XfpmDkp * dkp,
					     gboolean * OUT_can_hibernate,
					     GError ** error)
{
    *OUT_can_hibernate = dkp->priv->can_hibernate;
    return TRUE;
}

static gboolean xfpm_dkp_dbus_can_suspend (XfpmDkp * dkp,
					   gboolean * OUT_can_suspend,
					   GError ** error)
{
    *OUT_can_suspend = dkp->priv->can_suspend;
    
    return TRUE;
}

static gboolean xfpm_dkp_dbus_get_power_save_status (XfpmDkp * dkp,
						     gboolean * OUT_save_power,
						     GError ** error)
{
    //FIXME
    return TRUE;
}

static gboolean xfpm_dkp_dbus_get_on_battery (XfpmDkp * dkp,
					      gboolean * OUT_on_battery,
					      GError ** error)
{
    *OUT_on_battery = dkp->priv->on_battery;
    
    return TRUE;
}

static gboolean xfpm_dkp_dbus_get_low_battery (XfpmDkp * dkp,
					       gboolean * OUT_low_battery,
					       GError ** error)
{
    *OUT_low_battery = dkp->priv->on_low_battery;
    
    return TRUE;
}
