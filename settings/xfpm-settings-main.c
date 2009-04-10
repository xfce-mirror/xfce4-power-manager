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

#include "libxfpm/xfpm-popups.h"
#include "libxfpm/xfpm-string.h"
#include "libxfpm/xfpm-common.h"
#include "libxfpm/xfpm-dbus.h"

#include "xfce-power-manager-dbus-client.h"
#include "xfpm-settings.h"
#include "xfpm-config.h"

int main(int argc, char **argv)
{
    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    GError *error = NULL;
    DBusGConnection *bus;
    gboolean system_laptop;
    gboolean user_privilege;
    gboolean can_suspend;
    gboolean can_hibernate;
    gboolean has_lcd_brightness;
    
    GdkNativeWindow socket_id = 0;
	
    XfconfChannel *channel;
    DBusGProxy *proxy;
    
    GOptionEntry option_entries[] = 
    {
	{ "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
	{ NULL, },
    };
    
    if( !gtk_init_with_args (&argc, &argv, "", option_entries, PACKAGE, &error)) 
    {
        if( error) 
        {
            g_printerr("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr("\n");
            g_error_free(error);
        }
        else
        {
            g_error("Unable to open display.");
	}
        return EXIT_FAILURE;
    }
    
    bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
    
    if ( error )
    {
	g_error("%s\n",error->message);
	g_error_free(error);
	return EXIT_FAILURE;
    }

    if ( xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager") ) 
    {
	TRACE("Xfce power manager is running\n");
	
	if ( xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager.Config") )
	{
	    TRACE("Settings dialog already open\n");
	    dbus_g_connection_unref(bus);
	    return EXIT_SUCCESS;
	}
	
	xfpm_dbus_register_name(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager.Config");
	
	if ( !xfconf_init(&error) )
    	{
	    g_critical("xfconf init failed: %s using default settings\n", error->message);
	    
	    xfpm_popup_message(_("Xfce Power Manager"),_("Failed to load power manager configuration, "\
	    			"using defaults"), GTK_MESSAGE_WARNING);
	    g_error_free(error);
	    error = NULL;
	    return EXIT_FAILURE;
    	}
	
	if ( !g_thread_supported () )
	    g_thread_init (NULL);
	    
	dbus_g_thread_init ();
	
	channel = xfconf_channel_new(XFPM_CHANNEL_CFG);
	
   	proxy = dbus_g_proxy_new_for_name(bus,
				           "org.xfce.PowerManager",
				           "/org/xfce/PowerManager",
				           "org.xfce.Power.Manager");
	
	xfpm_manager_dbus_client_get_config (proxy, &system_laptop, &user_privilege,
					     &can_suspend, &can_hibernate, &has_lcd_brightness,
					     &error);
					     
	if ( error )
	{
	    g_critical ("Unable to get configuration information from xfce power manager: %s", error->message);
	    xfpm_error (_("Xfce Power Manager Settings"),
		       _("Unable to connect to Xfce Power Manager") );
	    g_error_free (error);
	    return EXIT_FAILURE;
	}
	
	xfpm_settings_dialog_new (channel, system_laptop, user_privilege,
				  can_suspend, can_hibernate, has_lcd_brightness,
				  system_laptop, socket_id);
					   
	gtk_main();
	
	xfpm_dbus_release_name(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager.Config");
	dbus_g_connection_unref(bus);
	
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
