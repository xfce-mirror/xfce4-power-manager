/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __XFPM_BATTERY_H
#define __XFPM_BATTERY_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "libxfpm/hal-battery.h"

#include "xfpm-enum-glib.h"
#include "xfpm-notify.h"

G_BEGIN_DECLS

#define XFPM_TYPE_BATTERY        (xfpm_battery_get_type () )
#define XFPM_BATTERY(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_BATTERY, XfpmBattery))
#define XFPM_IS_BATTERY(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_BATTERY))

typedef struct XfpmBatteryPrivate XfpmBatteryPrivate;

typedef struct
{
    GObject		 parent;
    XfpmBatteryPrivate	*priv;
    
} XfpmBattery;

typedef struct
{
    GObjectClass         parent_class;
    
    void	        (*battery_state_changed)	(XfpmBattery *battery,
    					            	 XfpmBatteryState state);
							 
    void                (*popup_battery_menu)		(XfpmBattery *battery,
						         GtkStatusIcon *icon,
							 guint button,
							 guint activate_time,
							 guint type);
       
} XfpmBatteryClass;

GType        		 xfpm_battery_get_type           (void) G_GNUC_CONST;
XfpmBattery    		*xfpm_battery_new                (const HalBattery *device);

void                     xfpm_battery_set_adapter_presence(XfpmBattery *battery,
							  gboolean adapter_present);
							  
void                     xfpm_battery_set_show_icon      (XfpmBattery *battery,
							  XfpmShowIcon show_icon);
							  
const HalBattery	*xfpm_battery_get_device         (XfpmBattery *battery);

XfpmBatteryState         xfpm_battery_get_state          (XfpmBattery *battery);

GtkStatusIcon  		*xfpm_battery_get_status_icon    (XfpmBattery *battery);

const gchar    		*xfpm_battery_get_icon_name      (XfpmBattery *battery);

void            	 xfpm_battery_show_info          (XfpmBattery *battery);

void                     xfpm_battery_set_critical_level (XfpmBattery *battery,
							  guint8 critical_level);

G_END_DECLS

#endif /* __XFPM_BATTERY_H */
