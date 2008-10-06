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

#include "xfpm-driver.h"
#include "xfpm-hal.h"
#include "xfpm-dbus-messages.h"

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
    int msg = handle_arguments(argc,argv);
    
    if ( msg == PM_HELP ) return 0;
    
    if ( msg == PM_CUSTOMIZE ) 
    {
        int reply;
        if (!xfpm_dbus_send_message_with_reply("Running",&reply))
        {
            return -1;
        }
        if ( reply != 1 )
        {
            g_print("Xfce power manager is not running\n");
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
            return -1;
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
        gtk_init(&argc,&argv);
        int reply;
        if (!xfpm_dbus_send_message_with_reply("Running",&reply))
        {
            return -1;
        }
        
        if ( reply == 1 )
        {
            g_print("Xfce power manager is already running\n");
            return 0;
        }
        XfpmDriver *driver = xfpm_driver_new();
        if (!xfpm_driver_monitor(driver)) 
        {
             /* g_disaster */
            g_error("Unable to load xfce4 power manager driver\n");
            g_object_unref(driver);
            return -1;
        }    
    }
        
    return 0;
}    
