/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include "xfpm-enum-glib.h"

G_BEGIN_DECLS

typedef struct _XfpmBatteryClass XfpmBatteryClass;
typedef struct _XfpmBattery      XfpmBattery;

#define XFPM_TYPE_BATTERY            (xfpm_battery_get_type ())
#define XFPM_BATTERY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFPM_TYPE_BATTERY, XfpmBattery))
#define XFPM_BATTERY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFPM_TYPE_BATTERY, XfpmBatteryClass))
#define XFPM_IS_BATTERY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFPM_TYPE_BATTERY))
#define XFPM_IS_BATTERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFPM_TYPE_BATTERY))
#define XFPM_BATTERY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFPM_TYPE_BATTERY, XfpmBatteryClass))

#define KIND_IS_BATTERY_OR_UPS(kind) ((kind) == UP_DEVICE_KIND_BATTERY || (kind) == UP_DEVICE_KIND_UPS)

GType                       xfpm_battery_get_type        (void) G_GNUC_CONST;

GtkStatusIcon              *xfpm_battery_new             (UpDevice    *device);

UpDeviceKind                xfpm_battery_get_kind        (XfpmBattery *battery);

XfpmBatteryCharge           xfpm_battery_get_charge      (XfpmBattery *battery);

const gchar                *xfpm_battery_get_name        (XfpmBattery *battery);

gchar                      *xfpm_battery_get_time_left   (XfpmBattery *battery);

UpDevice                   *xfpm_battery_get_device      (XfpmBattery *battery);

G_END_DECLS

#endif /* __XFPM_BATTERY_H */
