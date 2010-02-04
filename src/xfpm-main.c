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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "xfpm-dbus.h"
#include "xfpm-debug.h"
#include "xfpm-common.h"

#include "xfce-power-manager-dbus-client.h"
#include "xfpm-manager.h"

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
    
    XFPM_DEBUG ("sig %d", sig);
    
    if ( sig != SIGHUP )
	xfpm_manager_stop (manager);
}

static const gchar *
xfpm_bool_to_local_string (gboolean value)
{
    return value == TRUE ? _("True") : _("False");
}

static void
xfpm_dump (GHashTable *hash)
{
    gboolean has_battery;
    gboolean auth_suspend;
    gboolean auth_hibernate;
    gboolean can_suspend;
    gboolean can_hibernate;
    gboolean can_shutdown;
    gboolean has_lcd_brightness;
    gboolean has_sleep_button;
    gboolean has_hibernate_button;
    gboolean has_power_button;
    gboolean has_lid;
    gboolean can_spin_down;
    gboolean devkit_disk;
    
    has_battery = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-battery"));
    has_lid = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-lid"));
    can_suspend = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-suspend"));
    can_hibernate = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-hibernate"));
    auth_suspend = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-suspend"));
    auth_hibernate = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-hibernate"));
    has_lcd_brightness = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-brightness"));
    has_sleep_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "sleep-button"));
    has_power_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "power-button"));
    has_hibernate_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "hibernate-button"));
    can_shutdown = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-shutdown"));
    can_spin_down = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-spin"));
    devkit_disk = xfpm_string_to_bool (g_hash_table_lookup (hash, "devkit-disk"));
	
    g_print ("---------------------------------------------------\n");
    g_print ("       Xfce power manager version %s\n", VERSION);
#ifdef WITH_HAL
    g_print (_("With HAL support\n"));
#else
    g_print (_("Without HAL support\n"));
#endif
#ifdef ENABLE_POLKIT
    g_print (_("With policykit support\n"));
#else
    g_print (_("Without policykit support\n"));
#endif
#ifdef WITH_NETWORK_MANAGER
    g_print (_("With network manager support\n"));
#else
    g_print (_("Without network manager support\n"));
#endif
#ifdef HAVE_DPMS
    g_print (_("With DPMS support\n"));
#else
    g_print (_("Without DPMS support\n"));
#endif
    g_print ("---------------------------------------------------\n");
    g_print ( "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n"
	      "%s: %s\n",
	     _("Can suspend"),
	     xfpm_bool_to_local_string (can_suspend),
	     _("Can hibernate"),
	     xfpm_bool_to_local_string (can_hibernate),
	     _("Can spin down hard disks"),
	     xfpm_bool_to_local_string (devkit_disk),
	     _("Authorized to suspend"),
	     xfpm_bool_to_local_string (auth_suspend),
	     _("Authorized to hibernate"),
	     xfpm_bool_to_local_string (auth_hibernate),
	     _("Authorized to shutdown"),
	     xfpm_bool_to_local_string (can_shutdown),
	     _("Authorized to spin down hard disks"),
	     xfpm_bool_to_local_string (can_spin_down),
	     _("Has brightness panel"),
	     xfpm_bool_to_local_string (has_lcd_brightness),
	     _("Has power button"),
	     xfpm_bool_to_local_string (has_power_button),
	     _("Has hibernate button"),
	     xfpm_bool_to_local_string (has_hibernate_button),
	     _("Has sleep button"),
	      xfpm_bool_to_local_string (has_sleep_button),
	     _("Has LID"),
	      xfpm_bool_to_local_string (has_lid));
}

static void
xfpm_dump_remote (DBusGConnection *bus)
{
    DBusGProxy *proxy;
    GError *error = NULL;
    GHashTable *hash;
    
    proxy = dbus_g_proxy_new_for_name (bus,
				       "org.xfce.PowerManager",
				       "/org/xfce/PowerManager",
				       "org.xfce.Power.Manager");
	
    xfpm_manager_dbus_client_get_config (proxy, 
					 &hash,
					 &error);
					     
    g_object_unref (proxy);
    
    if ( error )
    {
	g_error ("%s", error->message);
	exit (EXIT_FAILURE);
    }
    
    xfpm_dump (hash);
    g_hash_table_destroy (hash);
}

static void G_GNUC_NORETURN
xfpm_start (DBusGConnection *bus, const gchar *client_id, gboolean dump)
{
    XfpmManager *manager;
    GError *error = NULL;
    
    XFPM_DEBUG ("Starting the power manager");
    
    manager = xfpm_manager_new (bus, client_id);
    
    if ( xfce_posix_signal_handler_init (&error)) 
    {
        xfce_posix_signal_handler_set_handler (SIGHUP,
                                               xfpm_quit_signal,
                                               manager, NULL);

        xfce_posix_signal_handler_set_handler (SIGINT,
                                               xfpm_quit_signal,
					       manager, NULL);

        xfce_posix_signal_handler_set_handler (SIGTERM,
                                               xfpm_quit_signal,
                                               manager, NULL);
    } 
    else 
    {
        g_warning ("Unable to set up POSIX signal handlers: %s", error->message);
        g_error_free (error);
    }

    xfpm_manager_start (manager);
    
    if ( dump )
    {
	GHashTable *hash;
	hash = xfpm_manager_get_config (manager);
	xfpm_dump (hash);
	g_hash_table_destroy (hash);
    }

    
    gtk_main ();
    
    g_object_unref (manager);
    
    exit (EXIT_SUCCESS);
}

int main (int argc, char **argv)
{
    DBusGConnection *bus;
    GError *error = NULL;
    DBusGProxy *proxy;
     
    gboolean run        = FALSE;
    gboolean quit       = FALSE;
    gboolean config     = FALSE;
    gboolean version    = FALSE;
    gboolean reload     = FALSE;
    gboolean no_daemon  = FALSE;
    gboolean debug      = FALSE;
    gboolean dump       = FALSE;
    gchar   *client_id  = NULL;
    
    GOptionEntry option_entries[] = 
    {
	{ "run",'r', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &run, NULL, NULL },
	{ "no-daemon",'\0' , G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &no_daemon, N_("Do not daemonize"), NULL },
	{ "debug",'\0' , G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &debug, N_("Enable debugging"), NULL },
	{ "dump",'\0' , G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &dump, N_("Dump all information"), NULL },
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
    
    g_set_application_name (PACKAGE_NAME);
    
    if (!gtk_init_with_args (&argc, &argv, (gchar *)"", option_entries, (gchar *)PACKAGE, &error)) 
    {
        if (G_LIKELY (error)) 
        {
            g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr ("\n");
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
	}

        return EXIT_FAILURE;
    }
    
    if ( version )    
    {
	show_version ();
    }
    
    xfpm_debug_init (debug);

    if ( dump == FALSE && debug == FALSE && no_daemon == FALSE && daemon(0,0) )
    {
	g_critical ("Could not daemonize");
    }
    
    bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
            
    if ( error )
    {
	xfce_dialog_show_error (NULL, 
				error, 
				"%s",
				_("Unable to get connection to the message bus session"));
	g_error ("%s: \n", error->message);
    }
    
    if ( quit )
    {
	if (!xfpm_dbus_name_has_owner (dbus_g_connection_get_connection(bus), 
				       "org.xfce.PowerManager") )
        {
            g_print (_("Xfce power manager is not running"));
	    g_print ("\n");
            return EXIT_SUCCESS;
        }
	else
	{
	    proxy = dbus_g_proxy_new_for_name (bus, 
			                       "org.xfce.PowerManager",
					       "/org/xfce/PowerManager",
					       "org.xfce.Power.Manager");
	    if ( !proxy ) 
	    {
		g_critical ("Failed to get proxy");
		dbus_g_connection_unref(bus);
            	return EXIT_FAILURE;
	    }
	    xfpm_manager_dbus_client_quit (proxy , &error);
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
	if (!xfpm_dbus_name_has_owner (dbus_g_connection_get_connection (bus), "org.xfce.PowerManager") &&
	    !xfpm_dbus_name_has_owner (dbus_g_connection_get_connection (bus), "org.freedesktop.PowerManagement"))
	{
	    g_print ("Xfce power manager is not running\n");
	    xfpm_start (bus, client_id, dump);
	}
	
	proxy = dbus_g_proxy_new_for_name (bus, 
			                   "org.xfce.PowerManager",
					   "/org/xfce/PowerManager",
					   "org.xfce.Power.Manager");
	if ( !proxy ) 
	{
	    g_critical ("Failed to get proxy");
	    dbus_g_connection_unref (bus);
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
    
    if (dump)
    {
	if (xfpm_dbus_name_has_owner (dbus_g_connection_get_connection (bus), 
				      "org.xfce.PowerManager"))
	{
	    xfpm_dump_remote (bus);
	    return EXIT_SUCCESS;
	}
    }
    
    if (xfpm_dbus_name_has_owner (dbus_g_connection_get_connection (bus), "org.freedesktop.PowerManagement") )
    {
	g_print ("%s: %s\n", 
		 _("Xfce Power Manager"),
		 _("Another power manager is already running"));
		  
    }
    else if (xfpm_dbus_name_has_owner (dbus_g_connection_get_connection(bus), 
				       "org.xfce.PowerManager"))
    {
	g_print (_("Xfce power manager is already running"));
	g_print ("\n");
    	return EXIT_SUCCESS;
    }
    else
    {	
	xfpm_start (bus, client_id, dump);
    }
    
    return EXIT_SUCCESS;
}
