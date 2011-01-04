/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#ifndef __BRIGHTNESS_BUTTON_H
#define __BRIGHTNESS_BUTTON_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libxfce4panel/xfce-panel-plugin.h>

G_BEGIN_DECLS

#define BRIGHTNESS_TYPE_BUTTON        (brightness_button_get_type () )
#define BRIGHTNESS_BUTTON(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), BRIGHTNESS_TYPE_BUTTON, BrightnessButton))
#define BRIGHTNESS_IS_BUTTON(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRIGHTNESS_TYPE_BUTTON))

typedef struct BrightnessButtonPrivate BrightnessButtonPrivate;

typedef struct
{
    GtkButton         		 parent;
    BrightnessButtonPrivate     *priv;
    
} BrightnessButton;

typedef struct
{
    GtkButtonClass 		 parent_class;
    
} BrightnessButtonClass;

GType        			 brightness_button_get_type        (void) G_GNUC_CONST;

GtkWidget       		*brightness_button_new             (XfcePanelPlugin *plugin);

void                             brightness_button_show            (BrightnessButton *button);

G_END_DECLS

#endif /* __BRIGHTNESS_BUTTON_H */
