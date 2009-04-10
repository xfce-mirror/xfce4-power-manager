/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <aliov@xfce.org>
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

#ifndef __XFPM_CONFIG_H
#define __XFPM_CONFIG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

G_BEGIN_DECLS

/* Configuration */
#define 	XFPM_CHANNEL_CFG            "xfce4-power-manager"
#define 	CRITICAL_BATT_ACTION_CFG    "/xfce4-power-manager/critical-battery-action"
#define         CRITICAL_POWER_LEVEL        "/xfce4-power-manager/critical-power-level"

#define 	POWER_SAVE_ON_BATTERY       "/xfce4-power-manager/power-save-on-battery"

#define         LOCK_SCREEN_ON_SLEEP        "/xfce4-power-manager/lock-screen-suspend-hibernate"

#define 	LCD_BRIGHTNESS_CFG          "/xfce4-power-manager/lcd-brightness"
#define         BRIGHTNESS_ON_AC            "/xfce4-power-manager/brightness-on-ac"
#define         BRIGHTNESS_ON_BATTERY       "/xfce4-power-manager/brightness-on-battery"

#define 	LID_SWITCH_ON_AC_CFG        "/xfce4-power-manager/lid-action-on-ac"
#define 	LID_SWITCH_ON_BATTERY_CFG   "/xfce4-power-manager/lid-action-on-battery"
#define 	POWER_SWITCH_CFG            "/xfce4-power-manager/power-switch-action"
#define         HIBERNATE_SWITCH_CFG        "/xfce4-power-manager/hibernate-switch-action"
#define 	SLEEP_SWITCH_CFG            "/xfce4-power-manager/sleep-switch-action"

#define 	GENERAL_NOTIFICATION_CFG    "/xfce4-power-manager/general-notification"

#ifdef HAVE_LIBNOTIFY
#define 	BATT_STATE_NOTIFICATION_CFG "/xfce4-power-manager/battery-state-notification"  /* default TRUE */

#define 	SHOW_SLEEP_ERRORS_CFG       "/xfce4-power-manager/show-sleep-errors"
#define 	SHOW_POWER_MANAGEMENT_ERROR "/xfce4-power-manager/show-power-management-error"
#endif

#define 	SHOW_TRAY_ICON_CFG          "/xfce4-power-manager/show-tray-icon"              /* default 0 = always,1 = when charging or discharging, 2 = when battery is present */

#ifdef HAVE_DPMS
#define 	DPMS_ENABLED_CFG             "/xfce4-power-manager/dpms-enabled" 
#define 	ON_BATT_DPMS_SLEEP 	     "/xfce4-power-manager/on-battery-dpms-sleep"
#define 	ON_BATT_DPMS_OFF	     "/xfce4-power-manager/on-battery-dpms-off"
#define 	ON_AC_DPMS_SLEEP 	     "/xfce4-power-manager/on-ac-dpms-sleep"
#define 	ON_AC_DPMS_OFF	     	     "/xfce4-power-manager/on-ac-dpms-off"
#define         DPMS_SLEEP_MODE		     "/xfce4-power-manager/dpms-sleep-mode"	/* 0= sleep, 1=suspend */
#endif

G_END_DECLS

#endif /* __XFPM_CONFIG_H */
