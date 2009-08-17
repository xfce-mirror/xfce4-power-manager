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
#include <signal.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "libxfpm/xfpm-dbus.h"
#include "libxfpm/xfpm-popups.h"

#include "xfce-power-manager-dbus-client.h"
#include "xfpm-manager.h"
#include "xfpm-session.h"

static gchar    *client_id = NULL;
static gboolean no_daemon  = FALSE;

static void G_GNUC_NORETURN
show_version (void)
{
    g_print (_("\n"
             "Xfce Power Manager %s\n\n"
             "Part of the Xfce Goodies Project\n"
             "http://goodies.xfce.org\n\n"
             "Licensed under the GNU GPL.\n\n"), VERSION);

    exit (EXIT_SUCCESS);
}

static void
xfpm_quit_signal (gint sig, gpointer data)
{
    XfpmManager *manager = (XfpmManager *) data;
    
    TRACE ("sig %d", sig);
    
    if ( sig != SIGHUP )
	xfpm_manager_stop (manager);
}

static void G_GNUC_NORETURN
xfpm_start (DBusGConnection *bus)
{
    XfpmManager *manager;
    XfpmSession *session;
    GError *error = NULL;
    
    TRACE ("Starting the power manager");
    session = xfpm_session_new ();
    
    if ( client_id != NULL )
	xfpm_session_set_client_id (session, client_id);
	
    xfpm_session_real_init (session);
    
    manager = xfpm_manager_new (bus);
    
    if ( xfce_posix_signal_handler_init (&error)) 
    {
        xfce_posix_signal_handler_set_handler(SIGHUP,
                                              xfpm_quit_signal,
                                              manager, NULL);

        xfce_posix_signal_handler_set_handler(SIGINT,
                                              xfpm_quit_signal,
					      manager, NULL);

        xfce_posix_signal_handler_set_handler(SIGTERM,
                                              xfpm_quit_signal,
                                              manager, NULL);
    } 
    else 
    {
        g_warning ("Unable to set up POSIX signal handlers: %s", error->message);
        g_error_free(error);
    }

    xfpm_manager_start (manager);
    gtk_main ();
    
    g_object_unref (session);
    
    exit (EXIT_SUCCESS);
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
    gboolean reload     = FALSE;
    
    GOptionEntry option_entries[] = 
    {
	{ "run",'r', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &run, NULL, NULL },
	{ "no-daemon",'\0' , G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &no_daemon, N_("Do not daemonize"), NULL },
	{ "restart", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &reload, N_("Restart the running instance of Xfce power manager"), NULL},
	{ "customize", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &config, N_("Show the configuration dialog"), NULL },
	{ "quit", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &quit, N_("Quit any running xfce power manager"), NULL },
	{ "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version, N_("Version information"), NULL },
	{ "sm-client-id", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &client_id, NULL, NULL },
	{ NULL, },
    };

    if ( !g_thread_supported () )
	g_thread_init (NULL);
       
    dbus_g_thread_init ();

    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if (!gtk_init_with_args (&argc, &argv, (gchar *)"", option_entries, (gchar *)PACKAGE, &error)) 
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
    }
    
    if ( run + quit + config + version > 1 )
    {
	g_printerr(_("Too many arguments"));
	g_printerr("\n");
	g_printerr(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
	g_printerr("\n");
	return EXIT_FAILURE;
    }

    if ( no_daemon == FALSE && daemon(0,0) )
    {
	g_critical ("Could not daemonize");
    }
    
    bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
            
    if ( error )
    {
	gchar *message = g_strdup(_("Unable to get connection to the message bus session"));
	message = g_strdup_printf("%s: ",error->message);
	
	xfpm_error (_("Xfce Power Manager"),
		    message );
			   
	g_error ("%s: \n", message);
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
	if (!xfpm_dbus_name_has_owner(dbus_g_connection_get_connection (bus), "org.xfce.PowerManager") &&
	    !xfpm_dbus_name_has_owner (dbus_g_connection_get_connection(bus), "org.freedesktop.PowerManagement"))
	{
	    g_print ("Xfce power manager is not running\n");
	    xfpm_start (bus);
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
	g_print ("%s: %s\n", 
		 _("Xfce Power Manager"),
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
	xfpm_start (bus);
    }
    
    return EXIT_SUCCESS;
}
