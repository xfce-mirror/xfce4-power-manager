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

#ifndef __HAL_MANAGER_H
#define __HAL_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define HAL_TYPE_MANAGER        (hal_manager_get_type () )
#define HAL_MANAGER(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), HAL_TYPE_MANAGER, HalManager))
#define HAL_IS_MANAGER(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), HAL_TYPE_MANAGER))

typedef struct HalManagerPrivate HalManagerPrivate;

typedef struct
{
    GObject		  	parent;
    HalManagerPrivate	       *priv;
    
} HalManager;

typedef struct
{
    GObjectClass 		parent_class;
    
    void		        (*device_added)				(HalManager *manager,
									 const gchar *udi);
    void                        (*device_removed)			(HalManager *manager,
									 const gchar *udi);
    
} HalManagerClass;

GType        	  		hal_manager_get_type        		(void) G_GNUC_CONST;
HalManager       	       *hal_manager_new            		(void);

gchar 			      **hal_manager_find_device_by_capability 	(HalManager *manager,
									 const gchar *capability);
						 
void                            hal_manager_free_string_array           (gchar **array);

G_END_DECLS

#endif /* __HAL_MANAGER_H */
