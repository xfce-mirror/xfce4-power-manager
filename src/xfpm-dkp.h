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

#ifndef __XFPM_DKP_H
#define __XFPM_DKP_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_DKP        (xfpm_dkp_get_type () )
#define XFPM_DKP(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_DKP, XfpmDkp))
#define XFPM_IS_DKP(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_DKP))

#define DKP_NAME 	     "org.freedesktop.DeviceKit.Power"
#define DKP_PATH 	     "/org/freedesktop/DeviceKit/Power"
#define DKP_IFACE 	     "org.freedesktop.DeviceKit.Power"
#define DKP_IFACE_DEVICE     "org.freedesktop.DeviceKit.Power.Device"

typedef enum
{
    XFPM_DKP_DEVICE_TYPE_UNKNOWN,
    XFPM_DKP_DEVICE_TYPE_LINE_POWER,
    XFPM_DKP_DEVICE_TYPE_BATTERY,
    XFPM_DKP_DEVICE_TYPE_UPS,
    XFPM_DKP_DEVICE_TYPE_MONITOR,
    XFPM_DKP_DEVICE_TYPE_MOUSE,
    XFPM_DKP_DEVICE_TYPE_KBD,
    XFPM_DKP_DEVICE_TYPE_PDA,
    XFPM_DKP_DEVICE_TYPE_PHONE
    
} XfpmDkpDeviceType;
  
typedef enum
{
    XFPM_DKP_DEVICE_STATE_UNKNOWN,
    XFPM_DKP_DEVICE_STATE_CHARGING,
    XFPM_DKP_DEVICE_STATE_DISCHARGING,
    XFPM_DKP_DEVICE_STATE_EMPTY,
    XFPM_DKP_DEVICE_STATE_FULLY_CHARGED,
    XFPM_DKP_DEVICE_STATE_PENDING_CHARGING,
    XFPM_DKP_DEVICE_STATE_PENDING_DISCHARGING
    
} XfpmDkpDeviceState;

typedef struct XfpmDkpPrivate XfpmDkpPrivate;

typedef struct
{
    GObject         	parent;
    
    XfpmDkpPrivate     *priv;
    
} XfpmDkp;

typedef struct
{
    GObjectClass 	parent_class;
    
    void                (*on_battery_changed)         	(XfpmDkp *dkp,
						         gboolean on_battery);
    
    void                (*low_battery_changed)        	(XfpmDkp *dkp,
							 gboolean low_battery);
    
    void		(*lid_changed)			(XfpmDkp *dkp,
							 gboolean lid_is_closed);
							
    void		(*waking_up)			(XfpmDkp *dkp);
    
    void		(*sleeping)			(XfpmDkp *dkp);
    
    void		(*ask_shutdown)			(XfpmDkp *dkp);
    
} XfpmDkpClass;

GType        		xfpm_dkp_get_type        	(void) G_GNUC_CONST;

XfpmDkp       	       *xfpm_dkp_get             	(void);

void			xfpm_dkp_suspend         	(XfpmDkp *dkp,
							 gboolean force);

void			xfpm_dkp_hibernate       	(XfpmDkp *dkp,
							 gboolean force);

gboolean		xfpm_dkp_has_battery		(XfpmDkp *dkp);

G_END_DECLS

#endif /* __XFPM_DKP_H */
