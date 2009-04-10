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

#ifndef __XFPM_BUTTON_H
#define __XFPM_BUTTON_H

#include <glib-object.h>

#include "xfpm-enum-glib.h"

G_BEGIN_DECLS

#define XFPM_TYPE_BUTTON        (xfpm_button_get_type () )
#define XFPM_BUTTON(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_BUTTON, XfpmButton))
#define XFPM_IS_BUTTON(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_BUTTON))

typedef struct XfpmButtonPrivate XfpmButtonPrivate;

typedef struct
{
    GObject         		 parent;
    XfpmButtonPrivate     	*priv;
    
} XfpmButton;

typedef struct
{
    GObjectClass 		 parent_class;
    void                         (*button_pressed)	     (XfpmButton *button,
							      XfpmButtonKey key);
    
} XfpmButtonClass;

GType        			 xfpm_button_get_type        (void) G_GNUC_CONST;
XfpmButton       		*xfpm_button_new             (void);

G_END_DECLS

#endif /* __XFPM_BUTTON_H */
