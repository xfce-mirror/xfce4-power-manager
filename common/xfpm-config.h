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
#define INACTIVITY_ON_BATTERY "inactivity-on-battery"
#define INACTIVITY_SLEEP_MODE_ON_AC "inactivity-sleep-mode-on-ac"
#define INACTIVITY_SLEEP_MODE_ON_BATTERY "inactivity-sleep-mode-on-battery"

#define CRITICAL_POWER_LEVEL "critical-power-level"
#define CRITICAL_POWER_ACTION "critical-power-action"

#define DPMS_ENABLED "dpms-enabled"
#define DPMS_ON_AC_SLEEP "dpms-on-ac-sleep"
#define DPMS_ON_AC_OFF "dpms-on-ac-off"
#define DPMS_ON_BATTERY_SLEEP "dpms-on-battery-sleep"
#define DPMS_ON_BATTERY_OFF "dpms-on-battery-off"
#define DPMS_SLEEP_MODE "dpms-sleep-mode"

#define PROFILE_ON_AC "profile-on-ac"
#define PROFILE_ON_BATTERY "profile-on-battery"

#define LOCK_SCREEN_SUSPEND_HIBERNATE "lock-screen-suspend-hibernate"
#define GENERAL_NOTIFICATION "general-notification"
#define PRESENTATION_MODE "presentation-mode"
#define HEARTBEAT_COMMAND "heartbeat-command"
#define SHOW_TRAY_ICON "show-tray-icon"

#define POWER_BUTTON_ACTION "power-button-action"
#define HIBERNATE_BUTTON_ACTION "hibernate-button-action"
#define SLEEP_BUTTON_ACTION "sleep-button-action"
#define BATTERY_BUTTON_ACTION "battery-button-action"
#define LID_ACTION_ON_AC "lid-action-on-ac"
#define LID_ACTION_ON_BATTERY "lid-action-on-battery"

#define LOGIND_HANDLE_POWER_KEY "logind-handle-power-key"
#define LOGIND_HANDLE_SUSPEND_KEY "logind-handle-suspend-key"
#define LOGIND_HANDLE_HIBERNATE_KEY "logind-handle-hibernate-key"
#define LOGIND_HANDLE_LID_SWITCH "logind-handle-lid-switch"

#define BRIGHTNESS_ON_AC "brightness-on-ac"
#define BRIGHTNESS_ON_BATTERY "brightness-on-battery"
#define BRIGHTNESS_LEVEL_ON_AC "brightness-level-on-ac"
#define BRIGHTNESS_LEVEL_ON_BATTERY "brightness-level-on-battery"
#define BRIGHTNESS_SLIDER_MIN_LEVEL "brightness-slider-min-level"
#define BRIGHTNESS_STEP_COUNT "brightness-step-count"
#define BRIGHTNESS_EXPONENTIAL "brightness-exponential"
#define BRIGHTNESS_SWITCH "brightness-switch"
#define BRIGHTNESS_SWITCH_RESTORE_ON_EXIT "brightness-switch-restore-on-exit"
#define HANDLE_BRIGHTNESS_KEYS "handle-brightness-keys"
#define SHOW_BRIGHTNESS_POPUP "show-brightness-popup"
#define SHOW_PANEL_LABEL "show-panel-label"
#define SHOW_PRESENTATION_INDICATOR "show-presentation-indicator"

G_END_DECLS

#endif /* __XFPM_CONFIG_H */
