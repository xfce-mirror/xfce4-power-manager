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

#ifndef __XFPM_ENUM_GLIB_H
#define __XFPM_ENUM_GLIB_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

G_BEGIN_DECLS

typedef enum
{
    XFPM_DO_NOTHING,
    XFPM_DO_SUSPEND,
    XFPM_DO_HIBERNATE,
    XFPM_DO_SHUTDOWN
    
} XfpmShutdownRequest;

typedef enum
{
    BATTERY_FULLY_CHARGED,
    BATTERY_NOT_FULLY_CHARGED,
    BATTERY_IS_CHARGING,
    BATTERY_IS_DISCHARGING,
    BATTERY_CHARGE_LOW,
    BATTERY_CHARGE_CRITICAL,
    BATTERY_NOT_PRESENT
    
} XfpmBatteryState;

typedef enum
{
    BUTTON_POWER_OFF,
    BUTTON_SLEEP,
    BUTTON_MON_BRIGHTNESS_UP,
    BUTTON_MON_BRIGHTNESS_DOWN,
    BUTTON_LID_CLOSED
    
} XfpmButtonKey;

G_END_DECLS

#endif /* __XFPM_ENUM_GLIB_H */
