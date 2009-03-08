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

#ifndef __DBUS_HAL_H
#define __DBUS_HAL_H

#include <glib-object.h>

G_BEGIN_DECLS

#define DBUS_TYPE_HAL        	    (dbus_hal_get_type () )
#define DBUS_HAL(o)          	    (G_TYPE_CHECK_INSTANCE_CAST((o), DBUS_TYPE_HAL, DbusHal))
#define DBUS_IS_HAL(o)              (G_TYPE_CHECK_INSTANCE_TYPE((o), DBUS_TYPE_HAL))

typedef enum
{
    SYSTEM_CAN_HIBERNATE         =  (1<<0),
    SYSTEM_CAN_SUSPEND           =  (1<<1),
    
} SystemPowerManagement;

typedef struct DbusHalPrivate DbusHalPrivate;

typedef struct
{
    GObject		  parent;
    DbusHalPrivate	 *priv;
    
} DbusHal;

typedef struct
{
    GObjectClass          parent_class;
	
} DbusHalClass;

GType                     dbus_hal_get_type        	  	(void) G_GNUC_CONST;
DbusHal       		 *dbus_hal_new             	  	(void);
gboolean             	  dbus_hal_shutdown        	  	(DbusHal *bus,
						      	   	 const gchar *shutdown,
							   	 GError **error);
gchar   	   	**dbus_hal_get_cpu_available_governors  (DbusHal *bus, 
							       	 GError **gerror);

gchar       	         *dbus_hal_get_cpu_current_governor 	(DbusHal *bus, 
							   	 GError **gerror);

gboolean 	          dbus_hal_set_cpu_governor 	  	(DbusHal *bus,
								 const gchar *governor,
							   	 GError **gerror);
void                      dbus_hal_free_string_array            (char **array);
G_END_DECLS

#endif /* __DBUS_HAL_H */
