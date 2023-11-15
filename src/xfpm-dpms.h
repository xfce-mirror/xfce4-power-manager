/*
 * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
 * Copyright (C) 2023 Gaël Bonithon <gael@xfce.org>
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

#ifndef __XFPM_DPMS_H__
#define __XFPM_DPMS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_DPMS (xfpm_dpms_get_type ())
G_DECLARE_DERIVABLE_TYPE (XfpmDpms, xfpm_dpms, XFPM, DPMS, GObject)

typedef enum _XfpmDpmsMode
{
  XFPM_DPMS_MODE_OFF,
  XFPM_DPMS_MODE_SUSPEND,
  XFPM_DPMS_MODE_STANDBY,
  XFPM_DPMS_MODE_ON,
} XfpmDpmsMode;

struct _XfpmDpmsClass
{
  GObjectClass parent_class;

  void        (*set_mode)            (XfpmDpms         *dpms,
                                      XfpmDpmsMode      mode);
  void        (*set_enabled)         (XfpmDpms         *dpms,
                                      gboolean          enabled);
  void        (*set_timeouts)        (XfpmDpms         *dpms,
                                      gboolean          standby,
                                      guint             sleep_timeout,
                                      guint             off_timeout);
};

XfpmDpms       *xfpm_dpms_new                 (void);
void            xfpm_dpms_set_inhibited       (XfpmDpms         *dpms,
                                               gboolean          inhibited);
void            xfpm_dpms_set_on_battery      (XfpmDpms         *dpms,
                                               gboolean          on_battery);

void            xfpm_dpms_set_mode            (XfpmDpms         *dpms,
                                               XfpmDpmsMode      mode);

G_END_DECLS

#endif /* __XFPM_DPMS_H__ */
