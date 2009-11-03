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

#ifndef __XFPM_MANAGER_H
#define __XFPM_MANAGER_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define XFPM_TYPE_MANAGER          (xfpm_manager_get_type () )
#define XFPM_MANAGER(o)            (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_MANAGER, XfpmManager))
#define XFPM_IS_MANAGER(o)         (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_MANAGER))
#define XFPM_MANAGER_GET_CLASS(k)  (G_TYPE_INSTANCE_GET_CLASS((k), XFPM_TYPE_MANAGER, XfpmManagerClass))

typedef struct XfpmManagerPrivate XfpmManagerPrivate;

typedef struct
{
    GObject		  parent;
    XfpmManagerPrivate	 *priv;
    
} XfpmManager;

typedef struct
{
    GObjectClass 	  parent_class;
    
} XfpmManagerClass;

GType        		  xfpm_manager_get_type        (void) G_GNUC_CONST;

XfpmManager    		 *xfpm_manager_new             (DBusGConnection *bus,
							const gchar *client_id);

void            	  xfpm_manager_start           (XfpmManager *manager);

void                      xfpm_manager_stop            (XfpmManager *manager);

G_END_DECLS

#endif /* __XFPM_MANAGER_H */
