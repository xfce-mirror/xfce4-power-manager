/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#ifndef __XFPM_BUTTON_HAL_H
#define __XFPM_BUTTON_HAL_H

#include <glib-object.h>

#include "xfpm-enum-glib.h"

G_BEGIN_DECLS

#define XFPM_TYPE_BUTTON_HAL        (xfpm_button_hal_get_type () )
#define XFPM_BUTTON_HAL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_BUTTON_HAL, XfpmButtonHal))
#define XFPM_IS_BUTTON_HAL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_BUTTON_HAL))

typedef struct XfpmButtonHalPrivate XfpmButtonHalPrivate;

typedef struct
{
    GObject         	      parent;
    XfpmButtonHalPrivate     *priv;
    
} XfpmButtonHal;

typedef struct
{
    GObjectClass 	      parent_class;
    
    void                      (*hal_button_pressed)	      (XfpmButtonHal *button,
							       XfpmButtonKey type);
    
} XfpmButtonHalClass;

GType        		      xfpm_button_hal_get_type        (void) G_GNUC_CONST;
XfpmButtonHal       	     *xfpm_button_hal_new             (void);

void                          xfpm_button_hal_get_keys        (XfpmButtonHal *button,
							       gboolean lid_only,
							       guint8 buttons);
G_END_DECLS

#endif /* __XFPM_BUTTON_HAL_H */
