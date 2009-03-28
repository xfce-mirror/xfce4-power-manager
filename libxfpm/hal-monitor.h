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

#ifndef __HAL_MONITOR_H
#define __HAL_MONITOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define HAL_TYPE_MONITOR        (hal_monitor_get_type () )
#define HAL_MONITOR(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), HAL_TYPE_MONITOR, HalMonitor))
#define HAL_IS_MONITOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), HAL_TYPE_MONITOR))

typedef struct HalMonitorPrivate HalMonitorPrivate;

typedef struct
{
    GObject		  parent;
    HalMonitorPrivate	 *priv;
	
} HalMonitor;

typedef struct
{
    GObjectClass 	  parent_class;
    
    void                  (*connection_changed)	    (HalMonitor *monitor,
						     gboolean connected);
    
} HalMonitorClass;

GType        		  hal_monitor_get_type        (void) G_GNUC_CONST;
HalMonitor       	 *hal_monitor_new             (void);

gboolean                  hal_monitor_get_connected   (HalMonitor *monitor) G_GNUC_PURE;

G_END_DECLS

#endif /* __HAL_MONITOR_H */
