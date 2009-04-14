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

#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "libxfpm/xfpm-common.h"
#include "libxfpm/xfpm-dbus.h"
#include "libxfpm/xfpm-notify.h"
#include "libxfpm/xfpm-string.h"


typedef struct
{
    XfcePanelPlugin  *plugin;
    
    DBusGConnection  *bus;
    DBusGProxy       *proxy;
    DBusGProxy       *monitor_proxy;
    
    XfpmNotify       *notify;
    
    GtkWidget        *button;
    GtkWidget        *image;
    
    gboolean          connected;
    guint             cookie;
    guint             saver_cookie;
    
    gboolean          plugin_inhibited;
    gboolean          saver_inhibited;

} inhibit_t;

/*
 * Read a bool property
 * Returns: bool property value, true is the fallback
 */
static gboolean
inhibit_plugin_read_bool_entry (const gchar *property)
{
    gchar *file;
    XfceRc *rc;
    gboolean value;
    
    file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/panel/inhibit.rc", TRUE);
    rc = xfce_rc_simple_open (file, FALSE);
    g_free (file);
    
    value = xfce_rc_read_bool_entry (rc, property, TRUE);
    return value;
}

/*
 * Save a bool entry
 */
static void
inhibit_plugin_save_bool_entry (const gchar *property, gboolean value)
{
    gchar *file;
    XfceRc *rc;
    
    file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/panel/inhibit.rc", TRUE);
    
    rc = xfce_rc_simple_open (file, FALSE);
    g_free (file);
    
    xfce_rc_write_bool_entry (rc, property, value);
    xfce_rc_close (rc);
}

/*
 *  Used to set of update the icon size
 *  returns true if successful, false if failure.
 */
static gboolean
inhibit_plugin_set_icon (inhibit_t *inhibit, gint width)
{
    GdkPixbuf *pixbuf;

    pixbuf = xfce_themed_icon_load ("gnome-inhibit-applet", width);
    
    if ( pixbuf )
    {
	gtk_image_set_from_pixbuf (GTK_IMAGE(inhibit->image), pixbuf);
	g_object_unref (pixbuf);
	return TRUE;
    }
    return FALSE;
}

/*
 * Size of the panel changed
 */
static gboolean
inhibit_plugin_size_changed_cb (XfcePanelPlugin *plugin, gint size, inhibit_t *inhibit)
{
    gint width = size -2 - 2* MAX(inhibit->button->style->xthickness,
				  inhibit->button->style->xthickness);
				 
    gtk_widget_set_size_request (GTK_WIDGET(plugin), size, size);
    
    return inhibit_plugin_set_icon (inhibit, width);
}

/*
 * Get the inhibition state of the running instance of the power manager
 * returns: false if not instance running or no inhibit is set.
 *          true  if the power manager is inhibited.
 */
static gboolean
inhibit_plugin_get_inhibit (inhibit_t *inhibit)
{
    GError *error = NULL;
    gboolean inhibited;
    
    if ( !inhibit->connected )
	return FALSE;
   
    dbus_g_proxy_call (inhibit->proxy, "HasInhibit", &error,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &inhibited,
		       G_TYPE_INVALID );
		       
    if (error)
    {
	g_critical ("Unable to get inhibit state: %s", error->message);
	g_error_free (error);
	return FALSE;
    }
    return inhibited;
}

/*
 * Send the inhibit message and store the cookie to be used
 * later when we want to UnInhibit
 */
static void
inhibit_plugin_set_inhibit (inhibit_t *inhibit)
{
    GError *error = NULL;
    
    const gchar *app = "Inhibit plugin";
    const gchar *reason = "User settings";
    
    dbus_g_proxy_call (inhibit->proxy, "Inhibit", &error,
		       G_TYPE_STRING, app,
		       G_TYPE_STRING, reason,
		       G_TYPE_INVALID,
		       G_TYPE_UINT, &inhibit->cookie,
		       G_TYPE_INVALID );
		       
    if (error)
    {
	g_critical ("Unable to set inhibit: %s", error->message);
	g_error_free (error);
	return;
    }
}

/*
 * Send the unset inhibit message with the cookie already saved
 */
static void
inhibit_plugin_unset_inhibit (inhibit_t *inhibit)
{
    GError *error = NULL;
    
    dbus_g_proxy_call (inhibit->proxy, "UnInhibit", &error,
		       G_TYPE_UINT, inhibit->cookie,
		       G_TYPE_INVALID,
		       G_TYPE_INVALID );
		       
    if (error)
    {
	g_critical ("Unable to set UnInhibit: %s", error->message);
	g_error_free (error);
	return;
    }
}

/*
 * Set the tooltip of the button widget
 */
static void
inhibit_plugin_set_tooltip (inhibit_t *inhibit)
{
    gboolean inhibited;
    
    if ( !inhibit->connected )
	gtk_widget_set_tooltip_text (inhibit->button, _("No power manager instance running") );
    else if ( inhibit->plugin_inhibited )
    {
	gtk_widget_set_tooltip_text (inhibit->button, _("Automatic sleep inhibited") );
    }
    else
    {
	inhibited = inhibit_plugin_get_inhibit (inhibit);
	if ( inhibited )
	    gtk_widget_set_tooltip_text (inhibit->button, _("Another application is disabling the automatic sleep") );
	else
	    gtk_widget_set_tooltip_text (inhibit->button, _("Automatic sleep enabled"));
    }
}

/*
 * Refresh all the info (button+tooltips)
 */
static void
inhibit_plugin_refresh_info (inhibit_t *inhibit)
{
    inhibit_plugin_set_tooltip (inhibit);
}

static void
inhibit_plugin_notify_callback (NotifyNotification *n, const gchar *id, inhibit_t *inhibit)
{
    if ( xfpm_strequal (id, "inhibit-changed-notification") )
    {
	inhibit_plugin_save_bool_entry ("inhibit-changed-notification", FALSE);
    }
    else if ( xfpm_strequal (id, "power-manager-disconnected-notification") )
    {
	inhibit_plugin_save_bool_entry ("power-manager-disconnected-notification", FALSE);
    }
    else if ( xfpm_strequal (id, "power-manager-connected-notification") )
    {
	inhibit_plugin_save_bool_entry ("power-manager-connected-notification", FALSE);
    }
}

/*
 * Standard signal sent by the power manager to inform us about the 
 * inhibition status
 */
static void
inhibit_changed_cb (DBusGProxy *proxy, gboolean inhibited, inhibit_t *inhibit)
{
    gboolean show_notification;
    NotifyNotification *n;
    const gchar *message;
    
    TRACE("Inhibit changed %d", inhibited);
    inhibit_plugin_refresh_info (inhibit);
    
    show_notification = inhibit_plugin_read_bool_entry ("inhibit-changed-notification");
    
    if ( show_notification )
    {
	message = inhibited ? (_("Power manager automatic sleep is disabled")) :
			      (_("Power manager automatic sleep is enabled")) ;
			      
	n = xfpm_notify_new_notification (inhibit->notify,
				      (_("Inhibit plugin")),
				      message,
				      "gnome-inhibit-applet",
				      5000,
				      XFPM_NOTIFY_NORMAL,
				      NULL);
				      
	xfpm_notify_add_action_to_notification (inhibit->notify,
						n,
						"inhibit-changed-notification",
						(_("Don't show again")),
						(NotifyActionCallback) inhibit_plugin_notify_callback,
						inhibit);
						
	notify_notification_attach_to_widget (n, inhibit->button);
	
	xfpm_notify_present_notification (inhibit->notify, n, FALSE);
    }
}

/*
 * This should be called when the running instance of the power manager
 * disappears from the session bus name. 
 */
static void
proxy_destroy_cb (DBusGProxy *proxy, inhibit_t *inhibit)
{
    gboolean notify;
    NotifyNotification *n;
    TRACE("Power manager disconnected");
    
    notify = inhibit_plugin_read_bool_entry ("power-manager-disconnected-notification");
    if ( notify )
    {
    
	n = xfpm_notify_new_notification (inhibit->notify,
				      (_("Inhibit plugin")),
				      (_("Power manager disconnected")),
				      "gnome-inhibit-applet",
				      5000,
				      XFPM_NOTIFY_NORMAL,
				      NULL);
				      
	xfpm_notify_add_action_to_notification (inhibit->notify,
						n,
						"power-manager-disconnected-notification",
						(_("Don't show again")),
						(NotifyActionCallback) inhibit_plugin_notify_callback,
						inhibit);
				      
	notify_notification_attach_to_widget (n, inhibit->button);
	xfpm_notify_present_notification (inhibit->notify, n, FALSE);
    }
    inhibit->proxy = NULL;
    inhibit->connected = FALSE;
    inhibit_plugin_refresh_info (inhibit);
}

/*
 * Destroying the proxy, but we disconnect the destroy signal before, cause we 
 * want to get the destroy signal only if the running instance of 
 * the power manager disappears from the session bus.
 */
static void
inhibit_plugin_disconnect_proxy (inhibit_t *inhibit)
{
    g_signal_handlers_disconnect_by_func (inhibit->proxy, proxy_destroy_cb, inhibit);
    g_object_unref (inhibit->proxy);
    inhibit->proxy = NULL;
    inhibit->connected = FALSE;
}

/*
 * Free all the allocated memory, called when the plugin is removed from the panel
 */
static void 
inhibit_plugin_free_data_cb (XfcePanelPlugin *plugin, inhibit_t *inhibit)
{
    if ( inhibit->bus )
	dbus_g_connection_unref (inhibit->bus);
	
    if ( inhibit->proxy )
	inhibit_plugin_disconnect_proxy (inhibit);
	
    g_object_unref (inhibit->notify);
    
    if ( inhibit->monitor_proxy )
	g_object_unref (inhibit->monitor_proxy);
    
    g_free (inhibit);
}

/*
 * Create the proxy on the inhibit interface and then connect to the signals
 * this function sets the boolean inhibit->connected to false if failure and to
 * true if all goes fine.
 */
static void
inhibit_plugin_connect_more (inhibit_t *inhibit)
{
    GError *error = NULL;
    
    inhibit->proxy = dbus_g_proxy_new_for_name_owner  (inhibit->bus,
						      "org.freedesktop.PowerManagement",
						      "/org/freedesktop/PowerManagement/Inhibit",
						      "org.freedesktop.PowerManagement.Inhibit",
						      &error);
    if ( error )
    {
	g_warning ("Unable to get name owner: %s", error->message);
	g_error_free (error);
	inhibit->connected = FALSE;
	return;
    }
    
    dbus_g_proxy_add_signal (inhibit->proxy, "HasInhibitChanged", 
			     G_TYPE_BOOLEAN, G_TYPE_INVALID);
    
    dbus_g_proxy_connect_signal (inhibit->proxy, "HasInhibitChanged", 
				 G_CALLBACK (inhibit_changed_cb), inhibit, NULL);
    
    g_signal_connect (inhibit->proxy, "destroy",
		      G_CALLBACK(proxy_destroy_cb), inhibit);
		      
    inhibit->connected = TRUE;
}

/*
 * Checks if a power manager found on the session bus and has an inhibit interface
 * The names are Freedesktop standard.
 */
static void
inhibit_plugin_connect (inhibit_t *inhibit)
{
    if ( !xfpm_dbus_name_has_owner (dbus_g_connection_get_connection(inhibit->bus),
				    "org.freedesktop.PowerManagement") )
    {
	gtk_widget_set_tooltip_text (inhibit->button, _("No power manager instance running"));
	inhibit->connected = FALSE;
	return;
    }
}

/*
 * Disconnecting the proxy and reloading all information
 */
static void
reload_activated (GtkWidget *widget, inhibit_t *inhibit)
{
    if ( inhibit->proxy )
    {
	inhibit_plugin_disconnect_proxy (inhibit);
	inhibit->connected = FALSE;
    }
	
    inhibit_plugin_connect (inhibit);
    inhibit_plugin_connect_more (inhibit);
    inhibit_plugin_refresh_info (inhibit);
}

static void
inhibit_plugin_pos_menu (GtkMenu *menu, gint *x, gint *y,
			 gboolean *push, gpointer data)
{
    GtkWidget *button;
    gint sc_height, sc_width, menu_width, menu_height, button_height;
    GtkRequisition requisition;
    
    button = (GtkWidget *) data;
    
    g_return_if_fail (x && y);

    gdk_window_get_origin (button->window, x, y);
    gdk_drawable_get_size (button->window, NULL, &button_height);

    if (GTK_WIDGET_NO_WINDOW (button))
        *x += button->allocation.x;

    sc_height = gdk_screen_get_height (gdk_screen_get_default ());
    sc_width = gdk_screen_get_width (gdk_screen_get_default ());

    gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

    menu_width = requisition.width;
    menu_height = requisition.height;

    if (*y + button_height + menu_height < sc_height) 
    {
        *y = *y + button_height;
    } 
    else 
    {
        if (*y + button_height < sc_height / 2) 
	    *y = *y + button_height;
        else 
	    *y = *y - menu_height;
    }

    if (*x + menu_width > sc_width)
        *x = sc_width - menu_width;
}

static gboolean
inhibit_plugin_inhibit_screen_saver (inhibit_t *inhibit, const gchar *name, 
				     const gchar *path, const gchar *interface)
{
    DBusGProxy *proxy;
    GError *error = NULL;
    
    proxy = dbus_g_proxy_new_for_name (inhibit->bus,
				       name,
				       path,
				       interface);
    if ( !proxy )
	return FALSE;
    
    const gchar *app = "Inhibit plugin";
    const gchar *reason = "User settings";
    
    dbus_g_proxy_call (proxy, "Inhibit", &error,
		       G_TYPE_STRING, app,
		       G_TYPE_STRING, reason,
		       G_TYPE_INVALID,
		       G_TYPE_UINT, &inhibit->saver_cookie,
		       G_TYPE_INVALID );

    g_object_unref (proxy);
    if (error)
    {
	g_critical ("Unable to set inhibit: %s", error->message);
	g_error_free (error);
	return FALSE;
    }
    return TRUE;
}

static gboolean
inhibit_plugin_uninhibit_screen_saver (inhibit_t *inhibit, const gchar *name, 
				     const gchar *path, const gchar *interface)
{
    DBusGProxy *proxy;
    GError *error = NULL;
    
    proxy = dbus_g_proxy_new_for_name (inhibit->bus,
				       name,
				       path,
				       interface);
    if ( !proxy )
	return FALSE;
	
    dbus_g_proxy_call (proxy, "UnInhibit", &error,
		       G_TYPE_UINT, inhibit->saver_cookie,
		       G_TYPE_INVALID,
		       G_TYPE_INVALID );
		    
    g_object_unref (proxy);
		       
    if (error)
    {
	g_critical ("Unable to set UnInhibit: %s", error->message);
	g_error_free (error);
	return FALSE;
    }
    return TRUE;
}

static void
inhibit_plugin_set_inhibit_screen_saver (inhibit_t *inhibit)
{

    if ( inhibit_plugin_inhibit_screen_saver (inhibit, 
					      "org.freedesktop.ScreenSaver",
					      "/org/freedesktop/ScreenSaver",
					      "org.freedesktop.ScreenSaver") )
    {
	inhibit->saver_inhibited = TRUE;
    }
    else if ( inhibit_plugin_inhibit_screen_saver (inhibit, 
						   "org.gnome.ScreenSaver",
						   "/org/gnome/ScreenSaver",
					           "org.gnome.ScreenSaver") )
    {
	
	inhibit->saver_inhibited = TRUE;
    }
    else
    {
	g_warning ("Unable to inhibit screen saver");
	inhibit->saver_inhibited = FALSE;
    }
}

static void
inhibit_plugin_set_uninhibit_screen_saver (inhibit_t *inhibit)
{
    if ( inhibit_plugin_uninhibit_screen_saver (inhibit, 
					      "org.freedesktop.ScreenSaver",
					      "/org/freedesktop/ScreenSaver/UnInhibit",
					      "org.freedesktop.ScreenSaver") )
    {
	inhibit->saver_inhibited = FALSE;
    }
    else if ( inhibit_plugin_uninhibit_screen_saver (inhibit, 
					      "org.gnome.ScreenSaver",
					      "/org/gnome/ScreenSaver/UnInhibit",
					      "org.gnome.ScreenSaver") )
    {
	
	inhibit->saver_inhibited = FALSE;
    }
    else
    {
	g_warning ("Unable to uninhibit screen saver");
	inhibit->saver_inhibited = TRUE;
    }
   
}

static void
backlight_sleep_toggled_cb (GtkWidget *widget, inhibit_t *inhibit)
{
    gboolean active;
    if ( G_UNLIKELY (inhibit->connected == FALSE) )
	return ;
	
    active = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget));
    
    if ( inhibit->saver_inhibited == FALSE && active == FALSE)
	inhibit_plugin_set_inhibit_screen_saver (inhibit);
    else
	inhibit_plugin_set_uninhibit_screen_saver (inhibit);
}

static void
automatic_sleep_toggled_cb (GtkWidget *widget, inhibit_t *inhibit)
{
    gboolean active;
    if ( G_UNLIKELY (inhibit->connected == FALSE) )
	return ;
	
    active = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget));
    
    if ( inhibit->plugin_inhibited  == FALSE && active == FALSE )
    {
	inhibit_plugin_set_inhibit (inhibit);
	inhibit->plugin_inhibited = TRUE;
    }
    else
    {
	inhibit_plugin_unset_inhibit (inhibit);
	inhibit->plugin_inhibited = FALSE;
    }
}

static void
inhibit_plugin_menu_selection_done_cb (GtkWidget *menu, inhibit_t *inhibit)
{
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (inhibit->button), FALSE);
    gtk_widget_destroy (menu);
}

/*
 * Button press events, Inhibit if pressed, UnInhibit if released
 */
static gboolean
button_press_event_cb (GtkWidget *button, GdkEventButton *ev, inhibit_t *inhibit)
{
    GtkWidget *menu;
    GtkWidget *mi;
    GtkWidget *img;
    
    if ( ev->button != 1 )
	return FALSE;
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    
    menu = gtk_menu_new ();
    
    mi = gtk_image_menu_item_new_with_label (_("Inhibit plugin"));
    img = gtk_image_new_from_icon_name ("gnome-inhibit-applet", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), img);
    gtk_widget_set_sensitive (mi, FALSE);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    mi = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    mi = gtk_check_menu_item_new_with_label (_("Enable backlight sleep"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), !inhibit->saver_inhibited);
    g_signal_connect (mi, "activate", 
		      G_CALLBACK (backlight_sleep_toggled_cb), inhibit);
    gtk_widget_set_sensitive (mi, inhibit->connected);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    mi = gtk_check_menu_item_new_with_label (_("Enable automatic sleep"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), !inhibit->plugin_inhibited);
    g_signal_connect (mi, "activate", 
		      G_CALLBACK (automatic_sleep_toggled_cb), inhibit);
    gtk_widget_set_sensitive (mi, inhibit->connected);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    gtk_widget_show_all (menu);
    g_signal_connect (menu, "selection-done",
		      G_CALLBACK (inhibit_plugin_menu_selection_done_cb), inhibit);
		      
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
		    inhibit_plugin_pos_menu, button, 
		    0, gtk_get_current_event_time ());
    return TRUE;
}

/*
 * Monitor if a power manager won the org.freedesktop.PowerManagement.Inhibit name
 */
static void
inhibit_plugin_name_owner_changed_cb (DBusGProxy *proxy, const gchar *name,
				      const gchar *prev, const gchar *new,
				      inhibit_t *inhibit)
{
    gboolean show_notification;
    NotifyNotification *n;
	
    if ( g_strcmp0 (name, "org.freedesktop.PowerManagement") != 0)
	return;
	
    if ( strlen (new) != 0 )
    {
	if ( inhibit->connected == TRUE )
	{
	    TRACE("Plugin is already connected!");
	    return;
	}
	show_notification = inhibit_plugin_read_bool_entry ("power-manager-connected-notification");
    
	if ( show_notification )
	{
	    n = xfpm_notify_new_notification (inhibit->notify,
					  (_("Inhibit plugin")),
					  (_("Power manager is connected")),
					  "gnome-inhibit-applet",
					  5000,
					  XFPM_NOTIFY_NORMAL,
					  NULL);
					  
	    xfpm_notify_add_action_to_notification (inhibit->notify,
						    n,
						    "power-manager-connected-notification",
						    (_("Don't show again")),
						    (NotifyActionCallback) inhibit_plugin_notify_callback,
						    inhibit);
					  
	    notify_notification_attach_to_widget (n, inhibit->button);
	    xfpm_notify_present_notification (inhibit->notify, n, FALSE);
	}
	reload_activated (NULL, inhibit);
    }
}

/*
 * Constructor of the plugin
 */
static void
inhibit_plugin_construct (inhibit_t *inhibit)
{
    GtkWidget *mi;
    
    inhibit->image = gtk_image_new ();
    inhibit->button = gtk_toggle_button_new ();
    inhibit->notify = xfpm_notify_new ();
    inhibit->plugin_inhibited = FALSE;
    inhibit->saver_inhibited  = FALSE;
    
    gtk_container_add (GTK_CONTAINER(inhibit->button), inhibit->image);
    
    gtk_button_set_relief (GTK_BUTTON(inhibit->button), GTK_RELIEF_NONE);
    
    g_signal_connect (inhibit->button, "button-press-event",
		      G_CALLBACK (button_press_event_cb), inhibit);
    
    gtk_container_add (GTK_CONTAINER(inhibit->plugin), inhibit->button);
    
    xfce_panel_plugin_add_action_widget (inhibit->plugin, inhibit->button);
    
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_REFRESH, NULL);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate",
		      G_CALLBACK(reload_activated), inhibit);
		      
    xfce_panel_plugin_menu_insert_item (inhibit->plugin, GTK_MENU_ITEM(mi));
    
    /*
     * Create the monitor proxy
     */

    inhibit->monitor_proxy 
	= dbus_g_proxy_new_for_name_owner (inhibit->bus,
					   "org.freedesktop.DBus",
					   "/org/freedesktop/DBus",
					   "org.freedesktop.DBus",
					   NULL);
    if ( !inhibit->monitor_proxy )
	goto out;

    dbus_g_proxy_add_signal (inhibit->monitor_proxy, "NameOwnerChanged",
			     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
			     
    dbus_g_proxy_connect_signal (inhibit->monitor_proxy, "NameOwnerChanged",
				 G_CALLBACK (inhibit_plugin_name_owner_changed_cb), inhibit, NULL);

out:
    gtk_widget_show_all (inhibit->button);
}

/*
 * register_inhibit_plugin: called by the panel
 */
static void
register_inhibit_plugin (XfcePanelPlugin *plugin)
{
    inhibit_t *inhibit;
    
    inhibit = g_new0 (inhibit_t, 1); 
    
    inhibit->plugin = plugin;
    
    inhibit->bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
    inhibit_plugin_construct (inhibit);
    
    inhibit_plugin_connect (inhibit);
    inhibit_plugin_connect_more (inhibit);
    
    inhibit_plugin_refresh_info (inhibit);
    
    g_signal_connect (plugin, "free-data",
		      G_CALLBACK(inhibit_plugin_free_data_cb), inhibit);
		      
    g_signal_connect (plugin, "size-changed",
		      G_CALLBACK(inhibit_plugin_size_changed_cb), inhibit);
		      
    xfce_panel_plugin_menu_show_about(plugin);
    
    notify_init ("inhibit-plugin");

    g_signal_connect (plugin, "about", G_CALLBACK(xfpm_about), _("Inhibit plugin"));
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(register_inhibit_plugin);
