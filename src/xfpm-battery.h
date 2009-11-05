/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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
#include <dbus/dbus-glib.h>

#include "xfpm-dkp.h"

G_BEGIN_DECLS

#define XFPM_TYPE_BATTERY        (xfpm_battery_get_type () )
#define XFPM_BATTERY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_BATTERY, XfpmBattery))
#define XFPM_IS_BATTERY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_BATTERY))

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

typedef struct XfpmBatteryPrivate XfpmBatteryPrivate;

typedef struct
{
    GtkStatusIcon      	    parent;
    
    XfpmBatteryPrivate     *priv;
    
} XfpmBattery;

typedef struct
{
    GtkStatusIconClass 	    parent_class;
    
    void		    (*battery_charge_changed)	 (XfpmBattery *battery);
    
} XfpmBatteryClass;

GType        		    xfpm_battery_get_type        (void) G_GNUC_CONST;

GtkStatusIcon              *xfpm_battery_new             (void);

void			    xfpm_battery_monitor_device  (XfpmBattery *battery,
							  DBusGProxy *proxy,
							  DBusGProxy *proxy_prop,
							  XfpmDkpDeviceType device_type);
//FIXME, make these as properties
XfpmDkpDeviceType	    xfpm_battery_get_device_type (XfpmBattery *battery);

XfpmBatteryCharge	    xfpm_battery_get_charge      (XfpmBattery *battery);

G_END_DECLS

#endif /* __XFPM_BATTERY_H */
