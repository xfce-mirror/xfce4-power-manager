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

#ifndef __HAL_DEVICE_H
#define __HAL_DEVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define HAL_TYPE_DEVICE        (hal_device_get_type () )
#define HAL_DEVICE(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), HAL_TYPE_DEVICE, HalDevice))
#define HAL_IS_DEVICE(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), HAL_TYPE_DEVICE))

typedef struct HalDevicePrivate HalDevicePrivate;

typedef struct
{
    GObject		  parent;
    HalDevicePrivate	 *priv;
    
} HalDevice;

typedef struct
{
    GObjectClass 	  parent_class;
    
    void		  (*device_changed)               (HalDevice *device,
							   const gchar *udi,
							   const gchar *key,
							   gboolean is_added,
							   gboolean is_removed);
    
} HalDeviceClass;

GType        	 	  hal_device_get_type             (void) G_GNUC_CONST;

HalDevice       	 *hal_device_new                  (void);

void                      hal_device_set_udi              (HalDevice *device,
							   const gchar *udi);

const gchar 		 *hal_device_get_udi 		  (HalDevice *device);

gboolean         	  hal_device_watch                (HalDevice *device);

gboolean		  hal_device_get_property_bool    (HalDevice *device,
							   const gchar *property);

gboolean                  hal_device_set_property_int     (HalDevice *device,
							   const gchar *property,
							   gint value);
							   
gint			  hal_device_get_property_int     (HalDevice *device,
							   const gchar *property);
									 
gchar			 *hal_device_get_property_string  (HalDevice *device,
							   const gchar *property);

gboolean                  hal_device_has_key		  (HalDevice *device,
							   const gchar *key);
									 
gboolean                  hal_device_has_capability       (HalDevice *device, 
							   const gchar *capability);

G_END_DECLS

#endif /* __HAL_DEVICE_H */
