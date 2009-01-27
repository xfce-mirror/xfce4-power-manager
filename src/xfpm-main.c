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

#include "xfpm-driver.h"
#include "xfpm-hal.h"
#include "xfpm-dbus-messages.h"
#include "xfpm-popups.h"
#include "xfpm-debug.h"

static GdkNativeWindow socket_id = 0;
static gboolean run     = FALSE;
static gboolean quit    = FALSE;
static gboolean config  = FALSE;
static gboolean version = FALSE;

static GOptionEntry option_entries[] = {
	{ "run", 'r', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &run, N_("Start xfce power manager"), NULL },
	{ "customize", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &config, N_("Show the configuration dialog"), NULL },
	{ "quit", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &quit, N_("Quit any running xfce power manager"), NULL },
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
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
	
static void
autostart()
{
    const gchar *home;
    
    if ( ( home = getenv("HOME")) == NULL )
    {
        xfpm_popup_message(_("Xfce Power Manager"),
                           _("Unable to read your home directory environment variable,"\
						    " autostart option may not work"),
                           GTK_MESSAGE_INFO);
        g_warning("Unable to read HOME environment variable, autostart will not work\n");
        return;
    }
    
    gchar *file;
    file = g_strdup_printf("%s/.config/autostart",home);
    
    if ( !g_file_test(file,G_FILE_TEST_IS_DIR) )
    {
        g_mkdir_with_parents(file,0700);
    }
    
    file = g_strdup_printf("%s/xfce4-power-manager.desktop",file);
    
    if ( g_file_test(file,G_FILE_TEST_EXISTS) )
    {
        XFPM_DEBUG("xfce4 power manager autostart.desktop file already exists\n");
        g_free(file);
        return;
    }
    
    GKeyFile *key;
    GError *error = NULL;
    
    key = g_key_file_new();
    
    g_key_file_set_value(key,"Desktop Entry","Version","1.0");
    g_key_file_set_string(key,"Desktop Entry","Type","Application");    
    g_key_file_set_string(key,"Desktop Entry","Name","Xfce4 Power Manager"); 
    g_key_file_set_string(key,"Desktop Entry","Icon","gpm-ac-adapter"); 
    g_key_file_set_string(key,"Desktop Entry","Exec","xfce4-power-manager -r"); 
    g_key_file_set_boolean(key,"Desktop Entry","StartupNotify",FALSE); 
    g_key_file_set_boolean(key,"Desktop Entry","Terminal",FALSE); 
    g_key_file_set_boolean(key,"Desktop Entry","Hidden",FALSE); 
    
    gchar *content = g_key_file_to_data(key,NULL,&error);
    
    if ( error )
    {
        g_critical("%s\n",error->message);
        g_error_free(error);
        g_free(file);
        g_key_file_free(key);
        return;
    }
    
    g_file_set_contents(file,content,-1,&error);
    
    if ( error )
    {
        g_critical("Unable to set content for the autostart desktop file%s\n",error->message);
        g_error_free(error);
        g_free(file);
        g_key_file_free(key);
        return;
    }
    
    g_free(file);
    g_key_file_free(key);
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
	
	if ( config )
	{
		int reply;
        if (!xfpm_dbus_send_message_with_reply("Running",&reply))
        {
            return EXIT_FAILURE;
        }
        if ( reply != 1 )
        {
            g_print(_("Xfce power manager is not running"));
			g_print("\n");
            gboolean ret = 
            xfce_confirm(_("Xfce4 Power Manager is not running, do you want to launch it now?"),
                        GTK_STOCK_YES,
                        _("Run"));
            if ( ret ) 
            {
                g_spawn_command_line_async("xfce4-power-manager -r",NULL);
            }
            return EXIT_SUCCESS;
        }
        xfpm_dbus_send_customize_message(socket_id);
        return EXIT_SUCCESS;
	}
	
	if ( run )
	{
		int reply;
        if (!xfpm_dbus_send_message_with_reply("Running",&reply))
        {
            return EXIT_FAILURE;
        }
        
        if ( reply == 1 )
        {
            g_print(_("Xfce power manager is already running"));
			g_print("\n");
            return EXIT_SUCCESS;
        }
        XfpmDriver *driver = xfpm_driver_new();
        autostart();
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
    
    if ( quit )
    {
    	int reply;
        if (!xfpm_dbus_send_message_with_reply("Quit",&reply))
        {
            return EXIT_FAILURE;
        }
        
        if ( reply == 0 )
        {
            g_print(_("Xfce power manager is not running"));
			g_print("\n");
            return EXIT_SUCCESS;
        }
        return EXIT_SUCCESS;
    }    
    	
    return EXIT_SUCCESS;
}    
