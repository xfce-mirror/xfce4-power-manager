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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <libxfce4util/libxfce4util.h>

#include "xfpm-common.h"
#include "xfpm-debug.h"

const gchar
*xfpm_bool_to_string (gboolean value)
{
  if ( value == TRUE )
    return "TRUE";
  else
    return "FALSE";
}

gboolean
xfpm_string_to_bool (const gchar *string)
{
  if ( !g_strcmp0 (string, "TRUE") )
    return TRUE;
  else if ( !g_strcmp0 (string, "FALSE") )
    return FALSE;

  return FALSE;
}

GtkBuilder
*xfpm_builder_new_from_string (const gchar *ui, GError **error)
{
  GtkBuilder *builder;

  builder = gtk_builder_new ();

  gtk_builder_add_from_string (GTK_BUILDER (builder),
                               ui,
                               -1,
                               error);

  return builder;
}

void
xfpm_preferences (void)
{
  g_spawn_command_line_async ("xfce4-power-manager-settings", NULL);
}

void
xfpm_preferences_device_id (const gchar* object_path)
{
  gchar *string = g_strdup_printf("xfce4-power-manager-settings -d %s", object_path);

  if (string)
    g_spawn_command_line_async (string, NULL);

  g_free (string);
}

void
xfpm_quit (void)
{
  g_spawn_command_line_async ("xfce4-power-manager -q", NULL);
}

void
xfpm_about (gpointer data)
{
  gchar *package = (gchar *)data;

  const gchar* authors[] =
  {
    "Ali Abdallah <aliov@xfce.org>",
     NULL,
  };

  static const gchar *documenters[] =
  {
    "Ali Abdallah <aliov@xfce.org>",
    NULL,
  };

  static const gchar *artists[] =
  {
    "Simon Steinbeiß <simon@xfce.org>",
     NULL,
  };

  gtk_show_about_dialog (NULL,
       "copyright", "Copyright \302\251 2008-2011 Ali Abdallah\nCopyright \302\251 2011-2012 Nick Schermer\nCopyright \302\251 2013-2015 Eric Koegel, Harald Judt, Simon Steinbeiß\nCopyright \302\251 2016-2023 The Xfce development team",
       "destroy-with-parent", TRUE,
       "authors", authors,
       "artists", artists,
       "documenters", documenters,
       "license", XFCE_LICENSE_GPL,
       "program-name", package,
       "translator-credits", _("translator-credits"),
       "version", PACKAGE_VERSION,
       "website", "http://docs.xfce.org/xfce/xfce4-power-manager/1.4/start",
       "logo-icon-name", "org.xfce.powermanager",
       NULL);
}

gboolean
xfpm_is_multihead_connected (void)
{
  GdkDisplay *dpy;
  gint nmonitor;

  dpy = gdk_display_get_default ();

  nmonitor = gdk_display_get_n_monitors (dpy);

  if ( nmonitor > 1 )
  {
    XFPM_DEBUG ("Multiple monitors connected");
    return TRUE;
  }
  else
  {
    return FALSE;
  }

  return FALSE;
}

GdkPixbuf *
xfpm_icon_load (const gchar *icon_name, gint size, gint scale_factor)
{
  GdkPixbuf *pix = NULL;
  GError *error = NULL;

  pix = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (),
                                            icon_name,
                                            size,
                                            scale_factor,
                                            GTK_ICON_LOOKUP_FORCE_SIZE,
                                            &error);

  if ( error )
  {
    g_warning ("Unable to load icon : %s : %s", icon_name, error->message);
    g_error_free (error);
  }

  return pix;
}
