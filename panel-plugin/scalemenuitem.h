/* -*- c-basic-offset: 2 -*- vi:set ts=2 sts=2 sw=2:
 * * Copyright (C) 2014 Eric Koegel <eric@xfce.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * Based on the scale menu item implementation of the indicator applet:
 * Authors:
 *    Cody Russell <crussell@canonical.com>
 * http://bazaar.launchpad.net/~indicator-applet-developers/ido/trunk.14.10/view/head:/src/idoscalemenuitem.h
 */


#ifndef __XFPM_SCALE_MENU_ITEM_H__
#define __XFPM_SCALE_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFPM_TYPE_SCALE_MENU_ITEM (xfpm_scale_menu_item_get_type ())
#ifndef glib_autoptr_clear_GtkImageMenuItem
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkImageMenuItem, g_object_unref)
#endif
G_DECLARE_FINAL_TYPE (XfpmScaleMenuItem, xfpm_scale_menu_item, XFPM, SCALE_MENU_ITEM, GtkImageMenuItem)

GtkWidget *
xfpm_scale_menu_item_new_with_range (gdouble min,
                                     gdouble max,
                                     gdouble step);
GtkWidget *
xfpm_scale_menu_item_get_scale (XfpmScaleMenuItem *menuitem);
const gchar *
xfpm_scale_menu_item_get_description_label (XfpmScaleMenuItem *menuitem);
const gchar *
xfpm_scale_menu_item_get_percentage_label (XfpmScaleMenuItem *menuitem);
void
xfpm_scale_menu_item_set_description_label (XfpmScaleMenuItem *menuitem,
                                            const gchar *label);
void
xfpm_scale_menu_item_set_percentage_label (XfpmScaleMenuItem *menuitem,
                                           const gchar *label);
void
xfpm_scale_menu_item_set_value (XfpmScaleMenuItem *item,
                                gdouble value);

G_END_DECLS

#endif /* __XFPM_SCALE_MENU_ITEM_H__ */
