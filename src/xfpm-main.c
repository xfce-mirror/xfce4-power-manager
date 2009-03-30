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

#include <unistd.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "libxfpm/xfpm-dbus.h"
#include "libxfpm/xfpm-popups.h"

#include "xfce-power-manager-dbus-client.h"
#include "xfpm-manager.h"

static void
show_version()
{
    g_print (_("\n"
             "Xfce Power Manager %s\n\n"
             "Part of the Xfce Goodies Project\n"
             "http://goodies.xfce.org\n\n"
             "Licensed under the GNU GPL.\n\n"), VERSION);
}

int main(int argc, char **argv)
{
    DBusGConnection *bus;
    GError *error = NULL;
    DBusGProxy *proxy;
     
    gboolean run        = FALSE;
    gboolean quit       = FALSE;
    gboolean config     = FALSE;
    gboolean version    = FALSE;
    gboolean no_daemon  = FALSE;
    gboolean reload     = FALSE;
    
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    GOptionEntry option_entries[] = 
    {
	{ "run",'r', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &run, NULL, NULL },
	{ "no-daemon",'\0' , G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &no_daemon, N_("Do not daemonize"), NULL },
	{ "restart", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &reload, N_("Restart the running instance of Xfce power manager"), NULL},
	{ "customize", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &config, N_("Show the configuration dialog"), NULL },
	{ "quit", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &quit, N_("Quit any running xfce power manager"), NULL },
	{ "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version, N_("Version information"), NULL },
	{ NULL, },
    };

    if(!gtk_init_with_args(&argc, &argv, "", option_entries, PACKAGE, &error)) 
    {
        if(G_LIKELY(error)) 
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
    
    if ( version )    
    {
	show_version();
    	return EXIT_SUCCESS;
    }
    
    if ( run + quit + config + version > 1 )
    {
	g_printerr(_("Too many arguments"));
	g_printerr("\n");
	g_printerr(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
	g_printerr("\n");
	return EXIT_FAILURE;
    }

    if ( !g_thread_supported () )
	g_thread_init (NULL);
       
    dbus_g_thread_init ();
    
    bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
            
    if ( error )
    {
	gchar *message = g_strdup(_("Unable to get connection to the message bus session"));
	message = g_strdup_printf("%s: ",error->message);
	
	xfpm_error (_("Xfce Power Manager"),
		    message );
			   
	g_error("%s: \n",message);
	g_print("\n");
	g_error_free(error);
	g_free(message);
	
	return EXIT_FAILURE;
    }
    
    if ( quit )
    {
	if (!xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), 
				      "org.xfce.PowerManager") )
        {
            g_print(_("Xfce power manager is not running"));
	    g_print("\n");
            return EXIT_SUCCESS;
        }
	else
	{
	    proxy = dbus_g_proxy_new_for_name(bus, 
			                      "org.xfce.PowerManager",
					      "/org/xfce/PowerManager",
					      "org.xfce.Power.Manager");
	    if ( !proxy ) 
	    {
		g_critical ("Failed to get proxy");
		dbus_g_connection_unref(bus);
            	return EXIT_FAILURE;
	    }
	    xfpm_manager_dbus_client_quit(proxy , &error);
	    g_object_unref (proxy);
	    
	    if ( error)
	    {
		g_critical ("Failed to send quit message %s:\n", error->message);
		g_error_free (error);
	    }
	}
	return EXIT_SUCCESS;
    }
    
    if ( config )
    {
	g_spawn_command_line_async ("xfce4-power-manager-settings", &error);
	
	if ( error )
	{
	    g_critical ("Failed to execute xfce4-power-manager-settings: %s", error->message);
	    g_error_free (error);
	    return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
    }
    
    if ( reload )
    {
	if (!xfpm_dbus_name_has_owner(dbus_g_connection_get_connection (bus),
				      "org.xfce.PowerManager") )
	{
	    xfpm_info (_("Xfce Power Manager"),
		       _("Xfce power manager is not running"));
		       
	    
	    return EXIT_FAILURE;
	}
	proxy = dbus_g_proxy_new_for_name(bus, 
			                      "org.xfce.PowerManager",
					      "/org/xfce/PowerManager",
					      "org.xfce.Power.Manager");
	if ( !proxy ) 
	{
	    g_critical ("Failed to get proxy");
	    dbus_g_connection_unref(bus);
	    return EXIT_FAILURE;
	}
	    
	if ( !xfpm_manager_dbus_client_restart (proxy, NULL) )
	{
	    g_critical ("Unable to send reload message");
	    g_object_unref (proxy);
	    dbus_g_connection_unref (bus);
	    return EXIT_SUCCESS;
	}
	return EXIT_SUCCESS;
    }
    
    if (xfpm_dbus_name_has_owner (dbus_g_connection_get_connection(bus), "org.freedesktop.PowerManagement") )
    {
	
	xfpm_info(_("Xfce Power Manager"),
		  _("Another power manager is already running"));
    }
    else if (xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), 
				      "org.xfce.PowerManager"))
    {
	g_print (_("Xfce power manager is already running"));
	g_print ("\n");
    	return EXIT_SUCCESS;
    }
    else
    {	
	TRACE("Starting the power manager\n");
	if ( no_daemon == FALSE && daemon(0,0) )
	{
	    g_critical ("Could not daemonize");
	}
    	XfpmManager *manager;
    	manager = xfpm_manager_new(bus);
    	xfpm_manager_start(manager);
	gtk_main();
    }
    return EXIT_SUCCESS;
}
