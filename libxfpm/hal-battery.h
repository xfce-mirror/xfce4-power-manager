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

#ifndef __HAL_BATTERY_H
#define __HAL_BATTERY_H

#include <glib-object.h>

#include "hal-device.h"
#include "hal-enum.h"

G_BEGIN_DECLS

#define HAL_TYPE_BATTERY        (hal_battery_get_type () )
#define HAL_BATTERY(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), HAL_TYPE_BATTERY, HalBattery))
#define HAL_IS_BATTERY(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), HAL_TYPE_BATTERY))

typedef struct HalBatteryPrivate HalBatteryPrivate;

typedef struct
{
    HalDevice		   parent;
    HalBatteryPrivate	  *priv;
    
} HalBattery;

typedef struct
{
    HalDeviceClass         parent_class;
    
    void                  (*battery_changed) (HalBattery *device);
    
} HalBatteryClass;

GType        	  hal_battery_get_type        (void) G_GNUC_CONST;
HalBattery       *hal_battery_new             (const gchar *udi);

G_END_DECLS

#endif /* __HAL_BATTERY_H */
