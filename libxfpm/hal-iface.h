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

#ifndef __HAL_IFACE_H
#define __HAL_IFACE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define HAL_TYPE_IFACE        (hal_iface_get_type () )
#define HAL_IFACE(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), HAL_TYPE_IFACE, HalIface))
#define HAL_IS_IFACE(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), HAL_TYPE_IFACE))

typedef enum
{
    SYSTEM_CAN_HIBERNATE         =  (1<<0),
    SYSTEM_CAN_SUSPEND           =  (1<<1),
    
} SystemPowerManagement;

typedef struct HalIfacePrivate HalIfacePrivate;

typedef struct
{
    GObject		parent;
    HalIfacePrivate	*priv;
    
} HalIface;

typedef struct
{
    GObjectClass 	parent_class;
    
} HalIfaceClass;

GType        		hal_iface_get_type        	 (void) G_GNUC_CONST;
HalIface       	       *hal_iface_new             	 (void);

gboolean                hal_iface_connect         	 (HalIface *iface);

gboolean		hal_iface_shutdown	 	 (HalIface *iface,
							  const gchar *shutdown,
							  GError **error);

gchar                 **hal_iface_get_cpu_governors 	 (HalIface *iface,
							  GError **error);
							  
gchar 	               *hal_iface_get_cpu_current_governor(HalIface *iface,
							   GError **error);
							
gboolean		hal_iface_set_cpu_governor       (HalIface *iface,
							  const gchar *governor,
							  GError **error);
							   
void                    hal_iface_free_string_array      (gchar **array);
G_END_DECLS

#endif /* __HAL_IFACE_H */
