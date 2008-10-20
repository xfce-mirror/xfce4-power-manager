/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <ali.slackware@gmail.com>
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

#include <libxfcegui4/libxfcegui4.h>

#include "xfpm-driver.h"
#include "xfpm-hal.h"
#include "xfpm-dbus-messages.h"
#include "xfpm-popups.h"

#ifndef _
#define _(x) x
#endif

enum 
{
    PM_HELP,
    PM_CUSTOMIZE,
    PM_RUN,
    PM_QUIT
};    

static void
show_usage() 
{
    
    printf( "\n"
		"Usage: xfce_power_manager [options] \n"
		"\n"
		"Options:\n"
		"-h, --help       Print this help message\n"
		"-r, --run        Start xfce power manager\n"
		"-c, --customize  Show Configuration dialog\n"
		"-q, --quit       Quit any running xfce power manager\n"
		"\n");
		    
}    

static void
autostart()
{
    const gchar *home;
    
    if ( ( home = getenv("HOME")) == NULL )
    {
        xfpm_popup_message(_("Xfce4 Power Manager"),
                           _("Unable to read HOME environment variable, autostart option may not work"),
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

static int
handle_arguments(int argc,char **argv) 
{
    
    int msg = -1;
    
    if ( argc > 1 && argv[1][0] == '-' ) 
    {    
        if (!strcmp(argv[1],"-h")     ||
            !strcmp(argv[1],"--help"))
        {
            show_usage();
            msg  = PM_HELP;            
        }        
        else
        {
            if (!strcmp(argv[1],"-c") ||
                !strcmp(argv[1],"--cutomize"))
            {    
                msg = PM_CUSTOMIZE;
            } else if (!strcmp(argv[1],"-r") ||
                !strcmp(argv[1],"--run"))
            {    
                msg = PM_RUN;
            }
            else if (!strcmp(argv[1],"-q") ||
                     !strcmp(argv[1],"--quit"))
            {
                msg = PM_QUIT;
            }
            else
            {
                show_usage();
                msg = PM_HELP;
            }    
        }
     }
     else 
     {
         show_usage();
         msg = PM_HELP;
     }    
     
     return msg;
}


int main(int argc,char **argv) 
{
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");
    
    gtk_init(&argc,&argv);
    
    int msg = handle_arguments(argc,argv);
    
    if ( msg == PM_HELP ) return 0;
    
    if ( msg == PM_CUSTOMIZE ) 
    {
        int reply;
        if (!xfpm_dbus_send_message_with_reply("Running",&reply))
        {
            return 1;
        }
        if ( reply != 1 )
        {
            g_print("Xfce power manager is not running\n");
            gboolean ret = 
            xfce_confirm(_("Xfce4 Power Manager is not running, do you want to launch it now"),
                        GTK_STOCK_YES,
                        _("Run"));
            if ( ret ) 
            {
                g_spawn_command_line_async("xfce4-power-manager -r",NULL);
            }
            return 0;
        }
        xfpm_dbus_send_message("Customize");
        return 0;
    }    
    
    if ( msg == PM_QUIT ) 
    {
        int reply;
        if (!xfpm_dbus_send_message_with_reply("Quit",&reply))
        {
            return 1;
        }
        
        if ( reply == 0 )
        {
            g_print("Xfce power manager is not running\n");
            return 0;
        }
        return 0;
    }
    
    if ( msg == PM_RUN ) 
    {
        int reply;
        if (!xfpm_dbus_send_message_with_reply("Running",&reply))
        {
            return 1;
        }
        
        if ( reply == 1 )
        {
            g_print("Xfce power manager is already running\n");
            return 0;
        }
        XfpmDriver *driver = xfpm_driver_new();
        autostart();
        if (!xfpm_driver_monitor(driver)) 
        {
             /* g_disaster */
            xfpm_popup_message(_("Xfce4 power manager"),
                              _("Impossible to run Xfce4 power manager, " \
                              "Please make sure that the hardware abstract layer (HAL) is running "\
                              "and then message bus daemon (DBus) is running."),
                              GTK_MESSAGE_ERROR);
            g_error("Unable to load xfce4 power manager driver\n");        
            g_object_unref(driver);
            return 1;
        }
    }
        
    return 0;
}    
