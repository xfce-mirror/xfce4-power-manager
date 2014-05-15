/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

/*
 * Order matters
 */
typedef enum
{
    XFPM_BATTERY_CHARGE_UNKNOWN,
    XFPM_BATTERY_CHARGE_CRITICAL,
    XFPM_BATTERY_CHARGE_LOW,
    XFPM_BATTERY_CHARGE_OK

} XfpmBatteryCharge;

typedef enum
{
    XFPM_DO_NOTHING,
    XFPM_DO_SUSPEND,
    XFPM_DO_HIBERNATE,
    XFPM_ASK,
    XFPM_DO_SHUTDOWN

} XfpmShutdownRequest;

typedef enum
{
    LID_TRIGGER_NOTHING,
    LID_TRIGGER_SUSPEND,
    LID_TRIGGER_HIBERNATE,
    LID_TRIGGER_LOCK_SCREEN,

} XfpmLidTriggerAction;

typedef enum
{
    BUTTON_UNKNOWN = 0,
    BUTTON_POWER_OFF,
    BUTTON_HIBERNATE,
    BUTTON_SLEEP,
    BUTTON_MON_BRIGHTNESS_UP,
    BUTTON_MON_BRIGHTNESS_DOWN,
    BUTTON_LID_CLOSED,
    BUTTON_BATTERY,
    BUTTON_KBD_BRIGHTNESS_UP,
    BUTTON_KBD_BRIGHTNESS_DOWN,
    NUMBER_OF_BUTTONS

} XfpmButtonKey;

typedef enum
 {
    SPIN_DOWN_HDD_NEVER,
    SPIN_DOWN_HDD_ON_BATTERY,
    SPIN_DOWN_HDD_PLUGGED_IN,
    SPIN_DOWN_HDD_ALWAYS

} XfpmSpindownRequest;

typedef enum
{
    SHOW_ICON_ALWAYS,
    SHOW_ICON_WHEN_BATTERY_PRESENT,
    SHOW_ICON_WHEN_BATTERY_CHARGING_DISCHARGING,
    NEVER_SHOW_ICON

} XfpmShowIcon;

G_END_DECLS

#endif /* __XFPM_ENUM_GLIB_H */
