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

#include "xfpm-common.h"
#include "xfpm-debug.h"

GdkPixbuf *
xfpm_load_icon(const char *icon_name,gint size)
{
    GdkPixbuf *icon;
    GError *error = NULL;
    
    icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default (),
                                   icon_name,
                                   size,
                                   GTK_ICON_LOOKUP_FORCE_SVG,
                                   &error);
    if ( error )
    {
        XFPM_DEBUG("Error occured while loading icon %s: %s\n",icon_name,error->message);
        g_error_free(error);
    }
    return icon;                               
}

void       
xfpm_lock_screen(void)
{
    gboolean ret = g_spawn_command_line_async("gnome-screensaver-command -l",NULL);
    
    if ( !ret )
    {
        /* this should be the default*/
        ret = g_spawn_command_line_async("xdg-screensaver lock",NULL);
    }
    
    if ( !ret )
    {
        ret = g_spawn_command_line_async("xscreensaver-command -lock",NULL);
    }
    
    if ( !ret )
    {
        g_critical("Connot lock screen\n");
    }
    
}

void       
xfpm_preferences(void) 
{
    g_spawn_command_line_async("xfce4-power-manager -c",NULL);
}
