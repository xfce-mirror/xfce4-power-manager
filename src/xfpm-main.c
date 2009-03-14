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
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    static gboolean run     = FALSE;
    static gboolean quit    = FALSE;
    static gboolean config  = FALSE;
    static gboolean version = FALSE;

    static GOptionEntry option_entries[] = 
    {
	{ "run",'r', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,&run,NULL,NULL },
	{ "customize", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &config, N_("Show the configuration dialog"), NULL },
	{ "quit", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &quit, N_("Quit any running xfce power manager"), NULL },
	{ "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version, N_("Version information"), NULL },
	{ NULL, },
    };

    GError *error = NULL;
    
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
    
    DBusGConnection *bus;
    bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
    
    if ( error )
    {
	gchar *message = g_strdup(_("Unable to connection to the message bus session"));
	message = g_strdup_printf("%s: ",error->message);
	xfpm_popup_message(_("Xfce power manager"),
			   message,
			   GTK_MESSAGE_ERROR);
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
	    GError *error = NULL;
	    DBusGProxy *proxy = dbus_g_proxy_new_for_name(bus, 
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
	if (!xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), 
				      "org.xfce.PowerManager"))
	{
	    g_print (_("Xfce power manager is not running"));
	    g_print ("\n");
	    /* FIXME: dialog to run */
	}
	return EXIT_SUCCESS;
    }
    
    
    if (xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), 
				      "org.xfce.PowerManager"))
    {
	g_print (_("Xfce power manager is already running"));
	g_print ("\n");
    	return EXIT_SUCCESS;
    }
    else
    {	
	TRACE("Starting the power manager\n");
    	XfpmManager *manager;
    	manager = xfpm_manager_new(bus);
    	xfpm_manager_start(manager);
    }
    
    return EXIT_SUCCESS;
}
