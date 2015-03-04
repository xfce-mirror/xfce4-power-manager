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


#ifndef _SCALE_MENU_ITEM_H_
#define _SCALE_MENU_ITEM_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TYPE_SCALE_MENU_ITEM         (scale_menu_item_get_type ())
#define SCALE_MENU_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_SCALE_MENU_ITEM, ScaleMenuItem))
#define SCALE_MENU_ITEM_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_SCALE_MENU_ITEM, ScaleMenuItemClass))
#define IS_SCALE_MENU_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_SCALE_MENU_ITEM))
#define IS_SCALE_MENU_ITEM_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TYPE_SCALE_MENU_ITEM))
#define SCALE_MENU_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_SCALE_MENU_ITEM, ScaleMenuItemClass))


typedef struct _ScaleMenuItem        ScaleMenuItem;
typedef struct _ScaleMenuItemClass   ScaleMenuItemClass;
typedef struct _ScaleMenuItemPrivate ScaleMenuItemPrivate;

struct _ScaleMenuItem
{
  GtkImageMenuItem parent_instance;

  ScaleMenuItemPrivate *priv;
};

struct _ScaleMenuItemClass
{
  GtkImageMenuItemClass parent_class;
};


GType        scale_menu_item_get_type              (void) G_GNUC_CONST;

GtkWidget   *scale_menu_item_new_with_range        (gdouble           min,
                                                    gdouble           max,
                                                    gdouble           step);

GtkWidget   *scale_menu_item_get_scale             (ScaleMenuItem *menuitem);

const gchar *scale_menu_item_get_description_label (ScaleMenuItem *menuitem);
const gchar *scale_menu_item_get_percentage_label  (ScaleMenuItem *menuitem);

void         scale_menu_item_set_description_label (ScaleMenuItem *menuitem,
                                                    const gchar      *label);
void         scale_menu_item_set_percentage_label  (ScaleMenuItem *menuitem,
                                                    const gchar      *label);

void        scale_menu_item_set_value (ScaleMenuItem *item,
                                       gdouble        value);


G_END_DECLS

#endif /* _SCALE_MENU_ITEM_H_ */
