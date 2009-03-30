/* * 
 *  Copyright (C) 2009 Ali <aliov@xfce.org>
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

#ifndef __XFPM_BRIGHTNESS_WIDGET_H
#define __XFPM_BRIGHTNESS_WIDGET_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_BRIGHTNESS_WIDGET        (xfpm_brightness_widget_get_type () )
#define XFPM_BRIGHTNESS_WIDGET(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_BRIGHTNESS_WIDGET, XfpmBrightnessWidget))
#define XFPM_IS_BRIGHTNESS_WIDGET(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_BRIGHTNESS_WIDGET))

typedef struct XfpmBrightnessWidgetPrivate XfpmBrightnessWidgetPrivate;

typedef struct
{
    GObject         		     parent;
    XfpmBrightnessWidgetPrivate     *priv;
    
} XfpmBrightnessWidget;

typedef struct
{
    GObjectClass 		     parent_class;
    
} XfpmBrightnessWidgetClass;

GType        		   	     xfpm_brightness_widget_get_type    (void) G_GNUC_CONST;

XfpmBrightnessWidget       	    *xfpm_brightness_widget_new         (void);

void				     xfpm_brightness_widget_set_max_level (XfpmBrightnessWidget *widget, 
									   guint level);

void                        	     xfpm_brightness_widget_set_level   (XfpmBrightnessWidget *widget,
									 guint level);

G_END_DECLS

#endif /* __XFPM_BRIGHTNESS_WIDGET_H */
