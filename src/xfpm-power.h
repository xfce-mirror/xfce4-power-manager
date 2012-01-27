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

#ifndef __XFPM_POWER_H
#define __XFPM_POWER_H

#include <glib-object.h>
#include "xfpm-enum-glib.h"

G_BEGIN_DECLS

typedef struct _XfpmPowerClass XfpmPowerClass;
typedef struct _XfpmPower      XfpmPower;

#define XFPM_TYPE_POWER            (xfpm_power_get_type ())
#define XFPM_POWER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFPM_TYPE_POWER, XfpmPower))
#define XFPM_POWER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFPM_TYPE_POWER, XfpmPowerClass))
#define XFPM_IS_POWER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFPM_TYPE_POWER))
#define XFPM_IS_POWER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFPM_TYPE_POWER))
#define XFPM_POWER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFPM_TYPE_POWER, XfpmPowerClass))

GType                   xfpm_power_get_type             (void) G_GNUC_CONST;

XfpmPower               *xfpm_power_get                 (void);

void                    xfpm_power_suspend              (XfpmPower *power,
                                                         gboolean   force);

void                    xfpm_power_hibernate            (XfpmPower *power,
                                                         gboolean   force);

gboolean                xfpm_power_can_suspend          (XfpmPower  *power,
                                                         gboolean   *can_suspend,
                                                         GError    **error);

gboolean                xfpm_power_can_hibernate        (XfpmPower  *power,
                                                         gboolean   *can_hibernate,
                                                         GError    **error);

gboolean                xfpm_power_get_on_battery       (XfpmPower  *power,
                                                         gboolean   *on_battery,
                                                         GError    **error);

gboolean                xfpm_power_get_low_battery      (XfpmPower  *power,
                                                         gboolean   *low_battery,
                                                         GError    **error);

XfpmPowerMode           xfpm_power_get_mode             (XfpmPower *power);

G_END_DECLS

#endif /* __XFPM_POWER_H */
