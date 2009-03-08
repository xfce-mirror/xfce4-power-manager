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

#ifndef __XFPM_BUTTON_XF86_H
#define __XFPM_BUTTON_XF86_H

#include <glib-object.h>

#include "xfpm-enum-glib.h"

G_BEGIN_DECLS

#define XFPM_TYPE_BUTTON_XF86   (xfpm_button_xf86_get_type () )
#define XFPM_BUTTON_XF86(o)     (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_BUTTON_XF86, XfpmButtonXf86))
#define XFPM_IS_BUTTON_XF86(o)  (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_BUTTON_XF86))

typedef struct XfpmButtonXf86Private XfpmButtonXf86Private;

typedef struct
{
    GObject		  	parent;
    XfpmButtonXf86Private      *priv;
    
} XfpmButtonXf86;

typedef struct
{
    GObjectClass parent_class;
     
    void                 (*xf86_button_pressed)	(XfpmButtonXf86 *button,
    						 XfpmXF86Button type);
    
} XfpmButtonXf86Class;

GType                 xfpm_button_xf86_get_type        (void) G_GNUC_CONST;
XfpmButtonXf86       *xfpm_button_xf86_new             (void);

G_END_DECLS

#endif /* __XFPM_BUTTON_XF86_H */
