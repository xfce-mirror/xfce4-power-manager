/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

const gchar *xfpm_bool_to_string (gboolean value)
{
    if ( value == TRUE ) return "TRUE";
    else 		 return "FALSE";
}

gboolean xfpm_string_to_bool (const gchar *string)
{
    if ( !g_strcmp0 (string, "TRUE") ) return TRUE;
    else if ( !g_strcmp0 (string, "FALSE") ) return FALSE;
    
    return FALSE;
}

GtkBuilder *xfpm_builder_new_from_string (const gchar *ui, GError **error)
{
    GtkBuilder *builder;

    builder = gtk_builder_new ();
    
    gtk_builder_add_from_string (GTK_BUILDER (builder),
                                 ui,
                                 -1,
                                 error);
    
    return builder;
}

static void
xfpm_link_browser (GtkAboutDialog *about, const gchar *linkto, gpointer data)
{
    gchar *cmd;
    
    cmd = g_strdup_printf ("%s %s","xdg-open", linkto);
    
    if ( !g_spawn_command_line_async (cmd, NULL) )
    {
	g_free (cmd);
	cmd = g_strdup_printf ("%s %s","xfbrowser4", linkto);
	g_spawn_command_line_async (cmd, NULL);
    }
    g_free (cmd);
	
}

static void
xfpm_link_mailto (GtkAboutDialog *about, const gchar *linkto, gpointer data)
{
    gchar *cmd = g_strdup_printf( "%s %s", "xdg-email", linkto);

    g_spawn_command_line_async (cmd, NULL);
    
    g_free (cmd);
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
		     "copyright", "Copyright \302\251 2008-2011 Ali Abdallah",
		     "destroy-with-parent", TRUE,
		     "documenters", documenters,
		     "license", XFCE_LICENSE_GPL,
		     "name", package,
		     "translator-credits", _("translator-credits"),
		     "version", PACKAGE_VERSION,
		     "website", "http://goodies.xfce.org",
		     NULL);
						 
}

gboolean xfpm_is_multihead_connected (void)
{
    GdkDisplay *dpy;
    GdkScreen *screen;
    gint nscreen;
    gint nmonitor;
    
    dpy = gdk_display_get_default ();
    
    nscreen = gdk_display_get_n_screens (dpy);
    
    if ( nscreen == 1 )
    {
	screen = gdk_display_get_screen (dpy, 0);
	if ( screen )
	{
	    nmonitor = gdk_screen_get_n_monitors (screen);
	    if ( nmonitor > 1 )
	    {
		g_debug ("Multiple monitor connected");
		return TRUE; 
	    }
	    else
		return FALSE;
	}
    }
    else if ( nscreen > 1 )
    {
	g_debug ("Multiple screen connected");
	return TRUE;
    }
    
    return FALSE;
}

GdkPixbuf *xfpm_icon_load (const gchar *icon_name, gint size)
{
    GdkPixbuf *pix = NULL;
    GError *error = NULL;
    
    pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), 
				    icon_name, 
				    size,
				    GTK_ICON_LOOKUP_USE_BUILTIN,
				    &error);
				    
    if ( error )
    {
	g_warning ("Unable to load icon : %s : %s", icon_name, error->message);
	g_error_free (error);
    }
    
    return pix;
}

