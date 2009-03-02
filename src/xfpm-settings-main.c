/*
 * * Copyright (C) 2008-2009 Ali <aliov@xfce.org>
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

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <xfconf/xfconf.h>

#include "xfce-power-manager-dbus-client.h"

#include "xfpm-string.h"
#include "xfpm-dbus-messages.h"

#include "xfpm-settings.h"
#include "xfpm-common.h"
#include "xfpm-popups.h"

static void dialog_response_cb(GtkDialog *dialog, gint response, gpointer data)
{
    XfconfChannel   *channel = (XfconfChannel *)g_object_get_data(G_OBJECT(data), "channel");
    DBusGConnection *bus     = (DBusGConnection *)g_object_get_data(G_OBJECT(data), "bus");    
    
    switch(response)
    {
	case GTK_RESPONSE_HELP:
	    xfpm_help();
	    break;
	default:
	    g_object_unref(G_OBJECT(channel));
	    xfpm_dbus_release_name(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager.Config");
	    dbus_g_connection_unref(bus);
	    xfconf_shutdown();
	    g_object_unref(G_OBJECT(data));
	    gtk_widget_destroy(GTK_WIDGET(dialog));
	    gtk_main_quit();
	    break;
    }
    
    
}

int main(int argc, char **argv)
{
    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");
    
    gtk_init(&argc, &argv);
    
    GError *error = NULL;
    
    DBusGConnection *bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
    
    if ( error )
    {
	g_error("%s\n",error->message);
	g_error_free(error);
	return EXIT_FAILURE;
    }

    if ( xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager") ) 
    {
	g_print("Xfce power manager is running\n");
	
	if ( xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager.Config") )
	{
	    g_warning("Settings dialog already open\n");
	    dbus_g_connection_unref(bus);
	    return EXIT_SUCCESS;
	}
	
	xfpm_dbus_register_name(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager.Config");
	
	if ( !xfconf_init(&error) )
    	{
	    g_critical("xfconf init failed: %s using default settings\n",error->message);
	    xfpm_popup_message(_("Xfce4 Power Manager"),_("Failed to load power manager configuration, "\
				"using defaults"),GTK_MESSAGE_WARNING);
	    g_error_free(error);
	    error = NULL;
    	}
	
	XfconfChannel *channel = xfconf_channel_new(XFPM_CHANNEL_CFG);
	
	DBusGProxy *proxy;
    
   	proxy = dbus_g_proxy_new_for_name(bus,
				           "org.xfce.PowerManager",
				           "/org/xfce/PowerManager",
				           "org.xfce.Power.Manager");
	gboolean   system_laptop;
	gint       power_management;
	gboolean   with_dpms;
	gint       governor;
	gint       switch_buttons;
	gboolean   brightness_control;
	gboolean   ups_found;
	
	xfpm_driver_dbus_client_get_conf(proxy, &system_laptop, &power_management
					 ,&with_dpms, &governor, &switch_buttons,
					 &brightness_control, &ups_found, &error);
					 
	if ( error ) g_error("%s \n", error->message);
	
	g_print("laptop=%d pm=%d dpms=%d gov=%d bt=%d br=%d ups=%d",
	 system_laptop, power_management,with_dpms, governor,
	switch_buttons, brightness_control, ups_found);
	
	GtkWidget *dialog = xfpm_settings_new(channel, 
					      system_laptop,
					      power_management,
					      with_dpms,
					      governor,
					      switch_buttons,
					      brightness_control,
					      ups_found);
	
	xfce_gtk_window_center_on_monitor_with_pointer(GTK_WINDOW(dialog));
	gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
	
	gdk_x11_window_set_user_time(dialog->window, gdk_x11_get_server_time (dialog->window));
	
	gpointer data;
	data = g_object_new(G_TYPE_OBJECT, NULL);
	
	g_object_set_data(G_OBJECT(data), "bus", bus);
	g_object_set_data(G_OBJECT(data), "channel", channel);
	
	g_signal_connect(dialog, "response", G_CALLBACK(dialog_response_cb), data);
	
	gtk_widget_show(dialog);
	
	gtk_main();
	
	return EXIT_SUCCESS;
    }
    else
    {
	g_print(_("Xfce power manager is not running"));
	g_print("\n");
  	gboolean ret = 
	    xfce_confirm(_("Xfce4 Power Manager is not running, do you want to launch it now?"),
			GTK_STOCK_YES,
			_("Run"));
	if ( ret ) 
	{
	    g_spawn_command_line_async("xfce4-power-manager",NULL);
	}
	
	dbus_g_connection_unref(bus);
    }
    
    return EXIT_SUCCESS;
}
