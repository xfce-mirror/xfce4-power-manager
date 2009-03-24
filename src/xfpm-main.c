/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <aliov@xfce.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gtk/gtk.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <dbus/dbus-glib-lowlevel.h>

#include "xfpm-driver.h"
#include "xfpm-hal.h"
#include "xfpm-dbus-messages.h"
#include "xfpm-popups.h"
#include "xfpm-debug.h"

#include "xfce-power-manager-dbus-client.h"

static gboolean run     = FALSE;
static gboolean quit    = FALSE;
static gboolean config  = FALSE;
static gboolean version = FALSE;
static gboolean reload  = FALSE;

static GOptionEntry option_entries[] = {
    { "run",'r', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,&run,NULL,NULL },
    { "reload",'\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &reload, NULL, NULL},
	{ "customize", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &config, N_("Show the configuration dialog"), NULL },
	{ "quit", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &quit, N_("Quit any running xfce power manager"), NULL },
    { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version, N_("Version information"), NULL },
    { NULL, },
};

static void
show_version()
{
	g_print (_("\n"
             "Xfce Power Manager %s\n\n"
             "Part of the Xfce Goodies Project\n"
             "http://goodies.xfce.org\n\n"
             "Licensed under the GNU GPL.\n\n"), VERSION);
}			 
	
int main(int argc,char **argv) 
{
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

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
    
    if ( version )    {
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
	    xfpm_popup_message(_("Xfce power manager"),
                                  _("Unable to run Xfce4 power manager, " \
                                  "make sure the hardware abstract layer and the message bus daemon "\
                                  "are running"),
                                  GTK_MESSAGE_ERROR);
        g_error(_("Unable to load xfce4 power manager"));
        g_print("\n");
	g_error_free(error);
        return EXIT_FAILURE;        
	}
	
	if ( config )
	{
	    if (!xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus), "org.xfce.PowerManager"))
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
		return EXIT_SUCCESS;
	    }
	    else
	    {
	        g_spawn_command_line_async("xfce4-power-manager-settings", NULL);
	        return EXIT_SUCCESS;
	    }
	}     
	 
	if ( quit )
    {
        if (!xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus),  "org.xfce.PowerManager"))
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
            	return EXIT_SUCCESS;
	    }
	    
	    xfpm_driver_dbus_client_quit(proxy , &error);
	    
	    if ( error )
	    {
		g_critical("Failed to sent quit message %s", error->message);
		g_error_free(error);
	    }
	    
	    g_object_unref(proxy);
	    dbus_g_connection_unref(bus);
            return EXIT_SUCCESS;
        }
    }    
    
    if ( reload )
    {
        if (!xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus),  "org.xfce.PowerManager"))
        {
            g_print(_("Xfce power manager is not running"));
	    g_print("\n");
            return EXIT_SUCCESS;
	}
	GError *error = NULL;
	DBusGProxy *proxy = dbus_g_proxy_new_for_name(bus, 
						      "org.xfce.PowerManager",
						      "/org/xfce/PowerManager",
						      "org.xfce.Power.Manager");
						      
	if ( !proxy ) 
	{
	    g_critical ("Failed to get proxy");
	    dbus_g_connection_unref(bus);
	    return EXIT_SUCCESS;
	}
	
	xfpm_driver_dbus_client_reload (proxy , &error);
	
	if ( error )
	{
	    g_critical("Failed to sent reload message %s", error->message);
	    g_error_free(error);
	}
	
	g_object_unref(proxy);
	dbus_g_connection_unref(bus);
	return EXIT_SUCCESS;
	
    }
    
    if (!xfpm_dbus_name_has_owner(dbus_g_connection_get_connection(bus),  "org.xfce.PowerManager") )
    {
        XfpmDriver *driver = xfpm_driver_new(bus);
        if (!xfpm_driver_monitor(driver)) 
        {
            xfpm_popup_message(_("Xfce power manager"),
                              _("Unable to run Xfce4 power manager, " \
                              "make sure the hardware abstract layer and the message bus daemon "\
                              "are running"),
                              GTK_MESSAGE_ERROR);
            g_error(_("Unable to load xfce4 power manager"));
            g_print("\n");
            g_object_unref(driver);
            return EXIT_FAILURE;
        }
    }
    else
    {
        g_print(_("Xfce power manager is already running"));
        g_print("\n");
        return EXIT_SUCCESS;
    }
	    
    return EXIT_SUCCESS;
}    
