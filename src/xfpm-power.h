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

#define XFPM_TYPE_POWER        (xfpm_power_get_type () )
#define XFPM_POWER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_POWER, XfpmPower))
#define XFPM_IS_POWER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_POWER))

typedef struct XfpmPowerPrivate XfpmPowerPrivate;

typedef struct
{
    GObject         	parent;
    
    XfpmPowerPrivate     *priv;
    
} XfpmPower;

typedef struct
{
    GObjectClass 	parent_class;
    
    void                (*on_battery_changed)         	(XfpmPower *power,
						         gboolean on_battery);
    
    void                (*low_battery_changed)        	(XfpmPower *power,
							 gboolean low_battery);
    
    void		(*lid_changed)			(XfpmPower *power,
							 gboolean lid_is_closed);
							
    void		(*waking_up)			(XfpmPower *power);
    
    void		(*sleeping)			(XfpmPower *power);
    
    void		(*ask_shutdown)			(XfpmPower *power);
    
    void		(*shutdown)			(XfpmPower *power);
    
} XfpmPowerClass;

GType        		xfpm_power_get_type        	(void) G_GNUC_CONST;

XfpmPower       	       *xfpm_power_get             	(void);

void			xfpm_power_suspend         	(XfpmPower *power,
							 gboolean force);

void			xfpm_power_hibernate       	(XfpmPower *power,
							 gboolean force);

gboolean		xfpm_power_has_battery		(XfpmPower *power);

gboolean        xfpm_power_is_in_presentation_mode (XfpmPower *power);

G_END_DECLS

#endif /* __XFPM_POWER_H */
