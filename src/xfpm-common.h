/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <ali.slackware@gmail.com>
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

#ifndef __XFPM_COMMON_H
#define __XFPM_COMMON_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#define BORDER 8

/* Configuration */
#define XFPM_CHANNEL_CFG            "xfce4-power-manager"
#define CRITICAL_BATT_CFG           "/xfce4-power-manager/critical-battery-charge"     /* default 12%  */
#define CRITICAL_BATT_ACTION_CFG    "/xfce4-power-manager/critical-battery-action"     /* default 0 nothing, 1 = shutdown, 2 = hibernate if supported) */

#define CPU_FREQ_SCALING_CFG        "/xfce4-power-manager/cpu-scaling-gov"
#define ON_AC_CPU_GOV_CFG           "/xfce4-power-manager/on-ac-cpu-gov"
#define ON_BATT_CPU_GOV_CFG         "/xfce4-power-manager/on-batt-cpu-gov"

#define POWER_SAVE_CFG              "/xfce4-power-manager/power-save"
#define CAN_POWER_SAVE              "/xfce4-power-manager/can-power-save"
#define LCD_BRIGHTNESS_CFG          "/xfce4-power-manager/lcd-brightness"
#define LID_SWITCH_CFG              "/xfce4-power-manager/lid-switch-action"
#define SLEEP_SWITCH_CFG            "/xfce4-power-manager/sleep-switch-action"
#define POWER_SWITCH_CFG            "/xfce4-power-manager/power-switch-action"

#ifdef HAVE_LIBNOTIFY
#define BATT_STATE_NOTIFICATION_CFG "/xfce4-power-manager/battery-state-notification"  /* default TRUE */
#define GENERAL_NOTIFICATION_CFG    "/xfce4-power-manager/general-notification"        /* default TRUE not yet used */
#define SHOW_SLEEP_ERRORS_CFG       "/xfce4-power-manager/show-sleep-errors"
#define SHOW_POWER_MANAGEMENT_ERROR "/xfce4-power-manager/show-power-management-error"
#endif
#define SHOW_TRAY_ICON_CFG          "/xfce4-power-manager/show-tray-icon"              /* default 0 = always,1 = when charging or discharging, 2 = when battery is present */

#ifdef HAVE_DPMS
#define DPMS_ENABLE_CFG             "/xfce4-power-manager/dpms-enabled" 
#define ON_BATT_DPMS_TIMEOUTS_CFG   "/xfce4-power-manager/on-battery-monitor-dpms-timeouts"
#define ON_AC_DPMS_TIMEOUTS_CFG     "/xfce4-power-manager/on-ac-monitor-dpms-timeouts"
#endif

typedef enum
{
    SYSTEM_CAN_SHUTDOWN          =  (1<<0),
    SYSTEM_CAN_HIBERNATE         =  (1<<2),
    SYSTEM_CAN_SUSPEND           =  (1<<3),
    SYSTEM_CAN_POWER_SAVE        =  (1<<4)
    
} SystemPowerManagement;

typedef enum 
{
    SYSTEM_LAPTOP,
    SYSTEM_DESKTOP,
    SYSTEM_SERVER,
    SYSTEM_UNKNOWN
    
} SystemFormFactor;

typedef enum
{
    LID_SWITCH   = (1<<0),
    SLEEP_SWITCH = (1<<1),
    POWER_SWITCH = (1<<2)
    
} XfpmSwitchButton;

GdkPixbuf* xfpm_load_icon(const gchar *icon_name,gint size);
void       xfpm_lock_screen(void);
void       xfpm_preferences(void);
void       xfpm_about(GtkWidget *widget,gpointer data);

#endif /* XFPM_COMMON_H */
