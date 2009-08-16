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

#ifndef __XFPM_COMMON_H
#define __XFPM_COMMON_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkBuilder     *xfpm_builder_new_from_string   	(const gchar *file,
						 GError **error);

GdkPixbuf* 	xfpm_load_icon    		(const gchar *icon_name,
						 gint size) G_GNUC_MALLOC;

void       	xfpm_lock_screen  		(void);

void       	xfpm_preferences		(void);

void       	xfpm_help			(void);

void            xfpm_quit                       (void);

void       	xfpm_about			(GtkWidget *widget, 
						 gpointer data);

gboolean	xfpm_guess_is_multimonitor	(void);

G_END_DECLS

#endif /* XFPM_COMMON_H */
