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

#ifndef __HAL_POWER_H
#define __HAL_POWER_H

#include <glib-object.h>

#include "hal-device.h"

G_BEGIN_DECLS

#define HAL_TYPE_POWER        (hal_power_get_type () )
#define HAL_POWER(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), HAL_TYPE_POWER, HalPower))
#define HAL_IS_POWER(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), HAL_TYPE_POWER))

typedef struct HalPowerPrivate HalPowerPrivate;

typedef struct
{
    GObject		  parent;
    HalPowerPrivate	 *priv;
    
} HalPower;

typedef struct
{
    GObjectClass parent_class;
    
    void 	(*device_added)	       (HalPower *power,
					const HalDevice *device);
    void        (*device_removed)      (HalPower *power,
    					const HalDevice *device);
    
} HalPowerClass;

GType        hal_power_get_type        (void) G_GNUC_CONST;
HalPower    *hal_power_new             (void);
GPtrArray   *hal_power_get_devices     (HalPower *power);

G_END_DECLS

#endif /* __HAL_POWER_H */
