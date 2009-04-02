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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib.h>

#include <libxfce4util/libxfce4util.h>

#include "xfpm-common.h"
#include "xfpm-string.h"

static void
xfpm_link_browser (GtkAboutDialog *about, const gchar *link, gpointer data)
{
    gchar *cmd = g_strdup_printf ("%s %s","xfbrowser4", link);
    g_spawn_command_line_async (cmd, NULL);
    g_free (cmd);
	
}

static void
xfpm_link_mailto (GtkAboutDialog *about, const gchar *link, gpointer data)
{
    gchar *cmd = g_strdup_printf( "%s %s", "xdg-email", link);

    g_spawn_command_line_async (cmd, NULL);
    
    g_free (cmd);
}
	

GdkPixbuf *
xfpm_load_icon (const char *icon_name, gint size)
{
    GdkPixbuf *icon;
    GError *error = NULL;
    
    icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                     icon_name,
                                     size,
                                     GTK_ICON_LOOKUP_FORCE_SVG,
                                     &error);
    if ( error )
    {
        g_warning ("Error occured while loading icon %s: %s\n", icon_name, error->message);
        g_error_free (error);
    }
    return icon;                               
}

/*
 * Map of int to strings shutdown values
 */
const gchar    *xfpm_int_to_shutdown_string (gint val)
{
    if ( val == 0 )
	return "Nothing";
    else if ( val == 1)
	return "Suspend";
    else if ( val == 2)
	return "Hibernate";
    else if ( val == 3)
	return "Shutdown";
    
    return "Invalid";
}

gint xfpm_shutdown_string_to_int (const gchar *string)
{
    if ( xfpm_strequal("Nothing", string) )
	return 0;
    else if ( xfpm_strequal("Suspend", string) )
	return 1;
    else if  (xfpm_strequal("Hibernate", string) )
	return 2;
    else if (xfpm_strequal("Shutdown", string) )
	return 3;
	
    return -1; /* error here */
}

void       
xfpm_lock_screen (void)
{
    gboolean ret = g_spawn_command_line_async ("xflock4", NULL);
    
    if ( !ret )
    {
        g_spawn_command_line_async ("gnome-screensaver-command -l", NULL);
    }
    
    if ( !ret )
    {
        /* this should be the default*/
        ret = g_spawn_command_line_async ("xdg-screensaver lock", NULL);
    }
    
    if ( !ret )
    {
        ret = g_spawn_command_line_async ("xscreensaver-command -lock", NULL);
    }
    
    if ( !ret )
    {
        g_critical ("Connot lock screen\n");
    }
}

void       
xfpm_preferences (void) 
{
    g_spawn_command_line_async ("xfce4-power-manager-settings", NULL);
}

void       
xfpm_help (void)
{
    g_spawn_command_line_async ("xfhelp4 xfce4-power-manager.html", NULL);
}

void
xfpm_quit (void)
{
    g_spawn_command_line_async ("xfce4-power-manager -q", NULL);
}

void       
xfpm_about (GtkWidget *widget, gpointer data)
{
    gchar *package = (gchar *)data;
    
    const gchar* authors[3] = 
    {
	"Ali Abdallah <aliov@xfce.org>", 
	 NULL
    };
							    
    static const gchar *documenters[] =
    {
	"Ali Abdallah <aliov@xfce.org>",
	NULL,
    };
    

    gtk_about_dialog_set_url_hook (xfpm_link_browser, NULL, NULL);
    gtk_about_dialog_set_email_hook (xfpm_link_mailto, NULL, NULL);
    
    gtk_show_about_dialog (NULL,
		     "authors", authors,
		     "copyright", "Copyright \302\251 2008 Ali Abdallah",
		     "destroy-with-parent", TRUE,
		     "documenters", documenters,
		     "license", XFCE_LICENSE_GPL,
		     "name", package,
		     "translator-credits", _("translator-credits"),
		     "version", PACKAGE_VERSION,
		     "website", "http://goodies.xfce.org",
		     NULL);
						 
}
