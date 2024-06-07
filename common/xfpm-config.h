/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
 * * Copyright (C) 2019 Kacper Piwiński
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

#include <glib.h>

G_BEGIN_DECLS

#define XFPM_CHANNEL "xfce4-power-manager"
#define XFPM_PROPERTIES_PREFIX "/xfce4-power-manager/"

#define INACTIVITY_ON_AC "inactivity-on-ac"
#define DEFAULT_INACTIVITY_ON_AC 0
#define INACTIVITY_ON_BATTERY "inactivity-on-battery"
#define DEFAULT_INACTIVITY_ON_BATTERY 0
#define INACTIVITY_SLEEP_MODE_ON_AC "inactivity-sleep-mode-on-ac"
#define DEFAULT_INACTIVITY_SLEEP_MODE_ON_AC XFPM_DO_SUSPEND
#define INACTIVITY_SLEEP_MODE_ON_BATTERY "inactivity-sleep-mode-on-battery"
#define DEFAULT_INACTIVITY_SLEEP_MODE_ON_BATTERY XFPM_DO_SUSPEND

#define CRITICAL_POWER_LEVEL "critical-power-level"
#define MIN_CRITICAL_POWER_LEVEL 1
#define MAX_CRITICAL_POWER_LEVEL 70
#define DEFAULT_CRITICAL_POWER_LEVEL 10
#define CRITICAL_POWER_ACTION "critical-power-action"
#define DEFAULT_CRITICAL_POWER_ACTION XFPM_DO_NOTHING

#define DPMS_ENABLED "dpms-enabled"
#define DEFAULT_DPMS_ENABLED TRUE
#define DPMS_ON_AC_SLEEP "dpms-on-ac-sleep"
#define DEFAULT_DPMS_ON_AC_SLEEP 10
#define DPMS_ON_AC_OFF "dpms-on-ac-off"
#define DEFAULT_DPMS_ON_AC_OFF 15
#define DPMS_ON_BATTERY_SLEEP "dpms-on-battery-sleep"
#define DEFAULT_DPMS_ON_BATTERY_SLEEP 5
#define DPMS_ON_BATTERY_OFF "dpms-on-battery-off"
#define DEFAULT_DPMS_ON_BATTERY_OFF 10
#define DPMS_SLEEP_MODE "dpms-sleep-mode"
#define DEFAULT_DPMS_SLEEP_MODE "Standby"

#define PROFILE_ON_AC "profile-on-ac"
#define DEFAULT_PROFILE_ON_AC "balanced"
#define PROFILE_ON_BATTERY "profile-on-battery"
#define DEFAULT_PROFILE_ON_BATTERY "balanced"

#define LOCK_SCREEN_SUSPEND_HIBERNATE "lock-screen-suspend-hibernate"
#define DEFAULT_LOCK_SCREEN_SUSPEND_HIBERNATE TRUE
#define GENERAL_NOTIFICATION "general-notification"
#define DEFAULT_GENERAL_NOTIFICATION TRUE
#define PRESENTATION_MODE "presentation-mode"
#define DEFAULT_PRESENTATION_MODE FALSE
#define HEARTBEAT_COMMAND "heartbeat-command"
#define DEFAULT_HEARTBEAT_COMMAND NULL
#define SHOW_TRAY_ICON "show-tray-icon"
#define DEFAULT_SHOW_TRAY_ICON FALSE

#define POWER_BUTTON_ACTION "power-button-action"
#define DEFAULT_POWER_BUTTON_ACTION XFPM_DO_NOTHING
#define HIBERNATE_BUTTON_ACTION "hibernate-button-action"
#define DEFAULT_HIBERNATE_BUTTON_ACTION XFPM_DO_NOTHING
#define SLEEP_BUTTON_ACTION "sleep-button-action"
#define DEFAULT_SLEEP_BUTTON_ACTION XFPM_DO_NOTHING
#define BATTERY_BUTTON_ACTION "battery-button-action"
#define DEFAULT_BATTERY_BUTTON_ACTION XFPM_DO_NOTHING
#define LID_ACTION_ON_AC "lid-action-on-ac"
#define DEFAULT_LID_ACTION_ON_AC LID_TRIGGER_LOCK_SCREEN
#define LID_ACTION_ON_BATTERY "lid-action-on-battery"
#define DEFAULT_LID_ACTION_ON_BATTERY LID_TRIGGER_LOCK_SCREEN

#define LOGIND_HANDLE_POWER_KEY "logind-handle-power-key"
#define DEFAULT_LOGIND_HANDLE_POWER_KEY FALSE
#define LOGIND_HANDLE_SUSPEND_KEY "logind-handle-suspend-key"
#define DEFAULT_LOGIND_HANDLE_SUSPEND_KEY FALSE
#define LOGIND_HANDLE_HIBERNATE_KEY "logind-handle-hibernate-key"
#define DEFAULT_LOGIND_HANDLE_HIBERNATE_KEY FALSE
#define LOGIND_HANDLE_LID_SWITCH "logind-handle-lid-switch"
#define DEFAULT_LOGIND_HANDLE_LID_SWITCH FALSE

#define BRIGHTNESS_ON_AC "brightness-on-ac"
#define MIN_BRIGHTNESS_ON_AC 9
#define DEFAULT_BRIGHTNESS_ON_AC 9
#define BRIGHTNESS_ON_BATTERY "brightness-on-battery"
#define MIN_BRIGHTNESS_ON_BATTERY 9
#define DEFAULT_BRIGHTNESS_ON_BATTERY 300
#define BRIGHTNESS_LEVEL_ON_AC "brightness-level-on-ac"
#define DEFAULT_BRIGHTNESS_LEVEL_ON_AC 80
#define BRIGHTNESS_LEVEL_ON_BATTERY "brightness-level-on-battery"
#define DEFAULT_BRIGHTNESS_LEVEL_ON_BATTERY 20
#define BRIGHTNESS_SLIDER_MIN_LEVEL "brightness-slider-min-level"
#define MIN_BRIGHTNESS_SLIDER_MIN_LEVEL -1
#define DEFAULT_BRIGHTNESS_SLIDER_MIN_LEVEL -1
#define BRIGHTNESS_STEP_COUNT "brightness-step-count"
#define MIN_BRIGHTNESS_STEP_COUNT 2
#define MAX_BRIGHTNESS_STEP_COUNT 100
#define DEFAULT_BRIGHTNESS_STEP_COUNT 10
#define BRIGHTNESS_EXPONENTIAL "brightness-exponential"
#define DEFAULT_BRIGHTNESS_EXPONENTIAL FALSE
#define BRIGHTNESS_SWITCH "brightness-switch"
#define MIN_BRIGHTNESS_SWITCH -1
#define MAX_BRIGHTNESS_SWITCH 1
#define DEFAULT_BRIGHTNESS_SWITCH -1
#define BRIGHTNESS_SWITCH_RESTORE_ON_EXIT "brightness-switch-restore-on-exit"
#define DEFAULT_BRIGHTNESS_SWITCH_RESTORE_ON_EXIT -1
#define HANDLE_BRIGHTNESS_KEYS "handle-brightness-keys"
#define DEFAULT_HANDLE_BRIGHTNESS_KEYS TRUE
#define SHOW_BRIGHTNESS_POPUP "show-brightness-popup"
#define DEFAULT_SHOW_BRIGHTNESS_POPUP TRUE
#define SHOW_PANEL_LABEL "show-panel-label"
#define DEFAULT_SHOW_PANEL_LABEL PANEL_LABEL_PERCENTAGE
#define SHOW_PRESENTATION_INDICATOR "show-presentation-indicator"
#define DEFAULT_SHOW_PRESENTATION_INDICATOR FALSE

G_END_DECLS

#endif /* __XFPM_CONFIG_H */
