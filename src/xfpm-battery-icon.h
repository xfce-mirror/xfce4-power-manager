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

#ifndef __XFPM_BATTERY_ICON_H
#define __XFPM_BATTERY_ICON_H

#include <glib-object.h>
#include <gtk/gtkstatusicon.h>

#include "xfpm-enums.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

G_BEGIN_DECLS

#define XFPM_TYPE_BATTERY_ICON    (xfpm_battery_icon_get_type())
#define XFPM_BATTERY_ICON(o)      (G_TYPE_CHECK_INSTANCE_CAST((o),XFPM_TYPE_BATTERY_ICON,XfpmBatteryIcon))
#define XFPM_IS_BATTERY_ICON(o)   (G_TYPE_CHECK_INSTANCE_TYPE((o),XFPM_TYPE_BATTERY_ICON))

typedef struct XfpmBatteryIconPrivate XfpmBatteryIconPrivate;

typedef struct 
{
    GtkStatusIcon parent;

    XfpmBatteryState state;
    XfpmBatteryType  type;
    
    GQuark icon;
    gboolean icon_loaded;
    
    guint last_full;
    guint critical_level;
    gboolean ac_adapter_present;
    gboolean battery_present;

#ifdef HAVE_LIBNOTIFY
    gboolean notify;
    gboolean discard_notification;
#endif        

} XfpmBatteryIcon;

typedef struct 
{
    
    GtkStatusIconClass parent_class;
    
} XfpmBatteryIconClass;

GType          xfpm_battery_icon_get_type   (void);
GtkStatusIcon *xfpm_battery_icon_new        (guint last_full,
                                            guint battery_type,
                                            guint critical_charge,        
                                            gboolean visible,
                                            gboolean ac_adapter_present);
                                            
void           xfpm_battery_icon_set_state  (XfpmBatteryIcon *battery_icon,
                                            guint charge,
                                            guint remaining_per,
                                            gboolean present,
                                            gboolean is_charging,
                                            gboolean is_discharging);
G_END_DECLS

#endif /* __XFPM_BATTERY_ICON_H */
