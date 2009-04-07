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

#ifndef __XFPM_LID_HAL_H
#define __XFPM_LID_HAL_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_LID_HAL        (xfpm_lid_hal_get_type () )
#define XFPM_LID_HAL(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_LID_HAL, XfpmLidHal))
#define XFPM_IS_LID_HAL(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_LID_HAL))

typedef struct XfpmLidHalPrivate XfpmLidHalPrivate;

typedef struct
{
    GObject		  parent;
    XfpmLidHalPrivate	 *priv;
    
} XfpmLidHal;

typedef struct
{
    GObjectClass          parent_class;
    
    void                 (*lid_closed) 	       (XfpmLidHal *lid);
    
} XfpmLidHalClass;

GType        	  	  xfpm_lid_hal_get_type        (void) G_GNUC_CONST;

XfpmLidHal       	 *xfpm_lid_hal_new             (void);

gboolean          	  xfpm_lid_hw_found            (XfpmLidHal *lid) G_GNUC_PURE;

void                      xfpm_lid_hal_reload          (XfpmLidHal *lid);

G_END_DECLS

#endif /* __XFPM_LID_HAL_H */
