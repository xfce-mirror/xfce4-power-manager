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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __XFPM_BRIGHTNESS_HAL_H
#define __XFPM_BRIGHTNESS_HAL_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_BRIGHTNESS_HAL        (xfpm_brightness_hal_get_type () )
#define XFPM_BRIGHTNESS_HAL(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_BRIGHTNESS_HAL, XfpmBrightnessHal))
#define XFPM_IS_BRIGHTNESS_HAL(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_BRIGHTNESS_HAL))

typedef struct XfpmBrightnessHalPrivate XfpmBrightnessHalPrivate;

typedef struct
{
    GObject		 		parent;
    XfpmBrightnessHalPrivate	       *priv;
    
} XfpmBrightnessHal;

typedef struct
{
    GObjectClass 			parent_class;
    
    void                                (*brightness_up)		    (XfpmBrightnessHal *brg,
									     guint level);
									     
    void                                (*brightness_down)		    (XfpmBrightnessHal *brg,
									     guint level);
    
} XfpmBrightnessHalClass;

GType        				xfpm_brightness_hal_get_type        (void) G_GNUC_CONST;
XfpmBrightnessHal      		       *xfpm_brightness_hal_new             (void);

gboolean                                xfpm_brightness_hal_has_hw          (XfpmBrightnessHal *brg) G_GNUC_PURE;

void                                    xfpm_brightness_hal_update_level    (XfpmBrightnessHal *brg,
									     guint level);

guint                                   xfpm_brightness_hal_get_max_level   (XfpmBrightnessHal *brg) G_GNUC_PURE;									     
G_END_DECLS

#endif /* __XFPM_BRIGHTNESS_HAL_H */
