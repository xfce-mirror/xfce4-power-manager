/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XFPM_CONFIG_H
#define __XFPM_CONFIG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

G_BEGIN_DECLS

#define 	XFPM_CHANNEL_CFG             	"xfce4-power-manager"

/*
 * Backward compatibility with old version.
 */
#define 	PROPERTIES_PREFIX		"/xfce4-power-manager/"


#define 	POWER_SAVE_ON_BATTERY        	"power-save-on-battery"
#define     	CPU_FREQ_CONTROL             	"enable-cpu-freq-control"
#define     	LOCK_SCREEN_ON_SLEEP         	"lock-screen-suspend-hibernate"

#define 	DPMS_ENABLED_CFG             	"dpms-enabled" 
#define 	ON_AC_DPMS_SLEEP 	     	"dpms-on-ac-sleep"
#define 	ON_AC_DPMS_OFF	     	     	"dpms-on-ac-off"
#define 	ON_BATT_DPMS_SLEEP 	     	"dpms-on-battery-sleep"
#define 	ON_BATT_DPMS_OFF	     	"dpms-on-battery-off"
#define     	DPMS_SLEEP_MODE		     	"dpms-sleep-mode"		

#define 	GENERAL_NOTIFICATION_CFG     	"general-notification"

#define     	ON_AC_INACTIVITY_TIMEOUT     	"inactivity-on-ac"
#define     	ON_BATTERY_INACTIVITY_TIMEOUT 	"inactivity-on-battery"
#define     	INACTIVITY_SLEEP_MODE        	"inactivity-sleep-mode"

#define     	BRIGHTNESS_ON_AC             	"brightness-on-ac"
#define     	BRIGHTNESS_ON_BATTERY        	"brightness-on-battery"


#define     	BRIGHTNESS_LEVEL_ON_AC          "brightness-level-on-ac"
#define     	BRIGHTNESS_LEVEL_ON_BATTERY     "brightness-level-on-battery"

#define     	CRITICAL_POWER_LEVEL        	"critical-power-level"
#define     	SHOW_BRIGHTNESS_POPUP        	"show-brightness-popup"
#define     	ENABLE_BRIGHTNESS_CONTROL       "change-brightness-on-key-events"

#define 	SHOW_TRAY_ICON_CFG          	"show-tray-icon"
#define 	CRITICAL_BATT_ACTION_CFG    	"critical-power-action"

#define 	POWER_SWITCH_CFG            	"power-button-action"
#define     	HIBERNATE_SWITCH_CFG        	"hibernate-button-action"
#define 	SLEEP_SWITCH_CFG            	"sleep-button-action"

#define 	LID_SWITCH_ON_AC_CFG        	"lid-action-on-ac"
#define 	LID_SWITCH_ON_BATTERY_CFG   	"lid-action-on-battery"

#define         SPIN_DOWN_ON_AC			"spin-down-on-ac"
#define         SPIN_DOWN_ON_BATTERY		"spin-down-on-battery"

#define         SPIN_DOWN_ON_AC_TIMEOUT		"spin-down-on-ac-timeout"
#define         SPIN_DOWN_ON_BATTERY_TIMEOUT	"spin-down-on-battery-timeout"

#define         NETWORK_MANAGER_SLEEP           "network-manager-sleep"

#define     LOGIND_HANDLE_POWER_KEY     "logind-handle-power-key"
#define     LOGIND_HANDLE_SUSPEND_KEY   "logind-handle-suspend-key"
#define     LOGIND_HANDLE_HIBERNATE_KEY "logind-handle-hibernate-key"
#define     LOGIND_HANDLE_LID_SWITCH    "logind-handle-lid-switch"

#define         PRESENTATION_MODE               "presentation-mode"

G_END_DECLS

#endif /* __XFPM_CONFIG_H */
