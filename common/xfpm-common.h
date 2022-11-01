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

#ifndef __XFPM_COMMON_H
#define __XFPM_COMMON_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GdkPixbuf      *xfpm_icon_load                  (const gchar *icon_name,
                                                 gint         size);
const gchar    *xfpm_bool_to_string             (gboolean     value) G_GNUC_PURE;
gboolean        xfpm_string_to_bool             (const gchar *string) G_GNUC_PURE;
GtkBuilder     *xfpm_builder_new_from_string    (const gchar *file,
                                                 GError     **error);
void            xfpm_preferences                (void);
void            xfpm_preferences_device_id      (const gchar *object_path);
void            xfpm_quit                       (void);
void            xfpm_about                      (gpointer     data);
gboolean        xfpm_is_multihead_connected     (void);

G_END_DECLS

#endif /* XFPM_COMMON_H */
