/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <ali.slackware@gmail.com>
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

#ifndef __XFPM_MANAGER
#define __XFPM_MANAGER

#include <glib-object.h>

G_BEGIN_DECLS

#define XFCE_TYPE_POWER_MANAGER            (xfpm_manager_get_type ())
#define XFPM_MANAGER(o)                    (G_TYPE_CHECK_INSTANCE_CAST((o),XFCE_TYPE_POWER_MANAGER,XfpmManager))
#define XFPM_MANAGER_CLASS(k)              (G_TYPE_CHECK_CLASS_CAST((k),XFCE_TYPE_POWER_MANAGER,XfpmManagerClass))
#define XFCE_IS_POWER_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_TYPE((o),XFCE_TYPE_POWER_MANAGER))
#define XFCE_IS_POWER_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_TYPE((k),XFCE_TYPE_POWER_MANAGER))
#define XFPM_MANAGER_GET_CLASS(k)          (G_TYPE_INSTANCE_GET_CLASS((k),XFCE_TYPE_POWER_MANAGER,XfpmManagerClass))

typedef struct XfpmManagerPrivate XfpmManagerPrivate;

typedef struct {
    
    GObject        parent;
    XfpmManagerPrivate *priv;
    
} XfpmManager;     

typedef struct {
    
    GObjectClass parent_class;
    
} XfpmManagerClass;    

GType                xfpm_manager_get_type                     (void);
XfpmManager         *xfpm_manager_new                          (void);
gboolean             xfpm_manager_start                        (XfpmManager *manager);
                                                         
G_END_DECLS

#endif /* __XFPM_MANAGER */
