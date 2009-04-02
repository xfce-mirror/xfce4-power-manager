/* * 
 *  Copyright (C) 2009 Ali <aliov@xfce.org>
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

#ifndef __XFPM_DBUS_MONITOR_H
#define __XFPM_DBUS_MONITOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_DBUS_MONITOR        (xfpm_dbus_monitor_get_type () )
#define XFPM_DBUS_MONITOR(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_DBUS_MONITOR, XfpmDBusMonitor))
#define XFPM_IS_DBUS_MONITOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_DBUS_MONITOR))

typedef struct XfpmDBusMonitorPrivate XfpmDBusMonitorPrivate;

typedef struct
{
    GObject        	 	parent;
    XfpmDBusMonitorPrivate     *priv;
    
} XfpmDBusMonitor;

typedef struct
{
    GObjectClass 		parent_class;
    void                        (*connection_lost)		  (XfpmDBusMonitor *monitor,
								   gchar *unique_name);
    
} XfpmDBusMonitorClass;

GType        			xfpm_dbus_monitor_get_type        (void) G_GNUC_CONST;
XfpmDBusMonitor       	       *xfpm_dbus_monitor_new             (void);

gboolean                        xfpm_dbus_monitor_add_match       (XfpmDBusMonitor *monitor,
								   const gchar *unique_name);
								   
gboolean                        xfpm_dbus_monitor_remove_match    (XfpmDBusMonitor *monitor,
								   const gchar *unique_name);

G_END_DECLS

#endif /* __XFPM_DBUS_MONITOR_H */
