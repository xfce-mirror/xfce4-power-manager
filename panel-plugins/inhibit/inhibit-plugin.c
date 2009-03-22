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

#include "inhibit-client.h"

typedef struct
{
    XfcePanelPlugin  *plugin;
    
    DBusGConnection  *bus;
    DBusGProxy       *proxy;
    
    GtkWidget        *button;
    GtkWidget        *image;
    
    gboolean          connected;
    guint             cookies;
    
    gboolean          inhibited;

} inhibit_t;

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

static void
inhibit_plugin_set_button (inhibit_t *inhibit)
{
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(inhibit->button), inhibit->inhibited);
    gtk_widget_set_tooltip_text (inhibit->button, 
			         inhibit->inhibited ? _("Automatic power savings on") :
						      _("Automatic power savings off"));
}

static gboolean
inhibit_plugin_size_changed_cb (XfcePanelPlugin *plugin, gint size, inhibit_t *inhibit)
{
    gint width = size -2 - 2* MAX(inhibit->button->style->xthickness,
				  inhibit->button->style->xthickness);
				 
    gtk_widget_set_size_request (GTK_WIDGET(plugin), size, size);
    return inhibit_plugin_set_icon (inhibit, width);
}

static void 
inhibit_plugin_free_data_cb (XfcePanelPlugin *plugin, inhibit_t *inhibit)
{
    g_free (inhibit);
}

static void
inhibit_changed_cb (DBusGProxy *proxy, gboolean inhibited, inhibit_t *inhibit)
{
    TRACE("Inhibit changed %d", inhibited);
    inhibit->inhibited = inhibited;
    inhibit_plugin_set_button (inhibit);
}

static void
inhibit_plugin_get_inhibit (inhibit_t *plugin)
{
    GError *error = NULL;
    
    if (!xfpm_inhibit_dbus_client_has_inhibit (plugin->proxy, &plugin->inhibited, &error) )
    {
	g_critical ("Unable to get inhibit state: %s", error->message);
	g_error_free (error);
	return;
    }
    
    inhibit_plugin_set_button (plugin);
}

static void
inhibit_plugin_set_inhibit (inhibit_t *plugin)
{
    GError *error;
    
    const gchar *app = "Inhibit plugin";
    const gchar *reason = "User settings";
    
    if (!xfpm_inhibit_dbus_client_inhibit (plugin->proxy, app, reason, &plugin->cookies, &error))
    {
	g_critical ("Unable to set inhibit: %s", error->message);
	g_error_free (error);
	return;
    }
    
    inhibit_plugin_set_button (plugin);
}

static void
inhibit_plugin_unset_inhibit (inhibit_t *plugin)
{
    GError *error;
    
    if (!xfpm_inhibit_dbus_client_un_inhibit (plugin->proxy, plugin->cookies, &error))
    {
	g_critical ("Unable to set UnInhibit: %s", error->message);
	g_error_free (error);
	return;
    }
    inhibit_plugin_set_button (plugin);
}

static void
proxy_destroy_cb (DBusGProxy *proxy, inhibit_t *inhibit)
{
    TRACE("Power manager disconnected");
    
    gtk_widget_set_tooltip_text (inhibit->button, _("Power manager disconnected"));
   // dbus_g_proxy_disconnect_signal (inhibit->proxy, "HasInhibitChanged",
	//			    G_CALLBACK(inhibit_changed_cb), inhibit);
    inhibit->proxy = NULL;
    inhibit->connected = FALSE;
}

static void
inhibit_plugin_connect_more (inhibit_t *plugin)
{
    GError *error = NULL;
    
    plugin->proxy = dbus_g_proxy_new_for_name_owner  (plugin->bus,
						      "org.freedesktop.PowerManagement",
						      "/org/freedesktop/PowerManagement/Inhibit",
						      "org.freedesktop.PowerManagement.Inhibit",
						      &error);
    if ( error )
    {
	g_warning ("Unable to get name owner: %s", error->message);
	g_error_free (error);
	plugin->connected = FALSE;
	return;
    }
    
    dbus_g_proxy_add_signal (plugin->proxy, "HasInhibitChanged", 
			     G_TYPE_BOOLEAN, G_TYPE_INVALID);
    
    dbus_g_proxy_connect_signal (plugin->proxy, "HasInhibitChanged", 
				 G_CALLBACK (inhibit_changed_cb), plugin, NULL);
    
    g_signal_connect (plugin->proxy, "destroy",
		      G_CALLBACK(proxy_destroy_cb), plugin);
		      
    plugin->connected = TRUE;
    inhibit_plugin_get_inhibit (plugin);
}

static void
inhibit_plugin_connect (inhibit_t *plugin)
{
    if (!plugin->bus )
	plugin->bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
    
    if ( !xfpm_dbus_name_has_owner (dbus_g_connection_get_connection(plugin->bus),
				    "org.freedesktop.PowerManagement") )
    {
	gtk_widget_set_tooltip_text (plugin->button, _("No power manager instance running"));
	plugin->connected = FALSE;
	return;
    }
    
    if ( !xfpm_dbus_name_has_owner (dbus_g_connection_get_connection(plugin->bus),
				    "org.freedesktop.PowerManagement.Inhibit") )
    {
	gtk_widget_set_tooltip_text (plugin->button, _("No power manager instance running"));
	plugin->connected = FALSE;
	return;
    }
}

static void
reload_activated (GtkWidget *widget, inhibit_t *plugin)
{
    if ( plugin->proxy )
	proxy_destroy_cb (plugin->proxy, plugin);
	
    inhibit_plugin_connect (plugin);
    inhibit_plugin_connect_more (plugin);
}

static void
button_toggled_cb (GtkWidget *widget, inhibit_t *plugin)
{
    if ( !plugin->connected )
	return;
	
    gboolean toggled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
    
    if ( toggled == TRUE && toggled != plugin->inhibited )
	inhibit_plugin_set_inhibit (plugin);
    else if ( toggled == FALSE && toggled != plugin->inhibited )
	inhibit_plugin_unset_inhibit (plugin);
}

static void
inhibit_plugin_construct (inhibit_t *plugin)
{
    GtkWidget *mi;
    
    plugin->image = gtk_image_new ();
    plugin->button = gtk_toggle_button_new ();
    
    gtk_container_add (GTK_CONTAINER(plugin->button), plugin->image);
    
    gtk_button_set_relief (GTK_BUTTON(plugin->button), GTK_RELIEF_NONE);
    
    g_signal_connect (plugin->button, "toggled",
		      G_CALLBACK(button_toggled_cb), plugin);
    
    gtk_container_add (GTK_CONTAINER(plugin->plugin), plugin->button);
    
    xfce_panel_plugin_add_action_widget (plugin->plugin, plugin->button);
    
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_REFRESH, NULL);
    gtk_widget_show (mi);
    
    g_signal_connect (mi, "activate",
		      G_CALLBACK(reload_activated), plugin);
		      
    xfce_panel_plugin_menu_insert_item (plugin->plugin, GTK_MENU_ITEM(mi));

    gtk_widget_show_all (plugin->button);
}

static void
register_inhibit_plugin (XfcePanelPlugin *plugin)
{
    inhibit_t *inhibit;
    
    inhibit = g_new0 (inhibit_t, 1); 
    
    inhibit->plugin = plugin;
    
    inhibit_plugin_construct (inhibit);
    
    inhibit_plugin_connect (inhibit);
    inhibit_plugin_connect_more (inhibit);
    
    g_signal_connect (plugin, "free-data",
		      G_CALLBACK(inhibit_plugin_free_data_cb), inhibit);
		      
    g_signal_connect (plugin, "size-changed",
		      G_CALLBACK(inhibit_plugin_size_changed_cb), inhibit);
		      
    xfce_panel_plugin_menu_show_about(plugin);

    g_signal_connect (plugin, "about", G_CALLBACK(xfpm_about), _("Inhibit plugin"));
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(register_inhibit_plugin);
