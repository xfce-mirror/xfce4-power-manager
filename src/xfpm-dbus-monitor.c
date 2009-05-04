/* * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <dbus/dbus-glib.h>

#include <libxfce4util/libxfce4util.h>

#include "xfpm-dbus-monitor.h"

static void xfpm_dbus_monitor_finalize   (GObject *object);

#define XFPM_DBUS_MONITOR_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_DBUS_MONITOR, XfpmDBusMonitorPrivate))

struct XfpmDBusMonitorPrivate
{
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    GPtrArray       *array;
};

static gpointer xfpm_dbus_monitor_object = NULL;

enum
{
    CONNECTION_LOST,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmDBusMonitor, xfpm_dbus_monitor, G_TYPE_OBJECT)

static void
xfpm_dbus_monitor_unique_connection_name_lost (XfpmDBusMonitor *monitor, const gchar *name)
{
    guint i = 0;
    gchar *array_name;
    
    for ( i = 0; i < monitor->priv->array->len; i++ )
    {
	array_name = g_ptr_array_index (monitor->priv->array, i);
	if ( g_strcmp0 (array_name, name) == 0 )
	{
	    g_signal_emit (G_OBJECT(monitor), signals [CONNECTION_LOST], 0, array_name);
	    //g_free (array_name);
	    //g_ptr_array_remove_index (monitor->priv->array, i);
	}
    }
}

static void
xfpm_dbus_monitor_name_owner_changed_cb (DBusGProxy *proxy, const gchar *name,
					 const gchar *prev, const gchar *new,
					 XfpmDBusMonitor *monitor)
{
    if ( strlen (prev) != 0 )
    {
	xfpm_dbus_monitor_unique_connection_name_lost (monitor, prev);
    }
}

static void
xfpm_dbus_monitor_class_init (XfpmDBusMonitorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals [CONNECTION_LOST] =
    	g_signal_new("connection-lost",
		     XFPM_TYPE_DBUS_MONITOR,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(XfpmDBusMonitorClass, connection_lost),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__STRING,
		     G_TYPE_NONE, 1, G_TYPE_STRING);
		     
    object_class->finalize = xfpm_dbus_monitor_finalize;

    g_type_class_add_private (klass, sizeof (XfpmDBusMonitorPrivate));
}

static void
xfpm_dbus_monitor_init (XfpmDBusMonitor *monitor)
{
    monitor->priv = XFPM_DBUS_MONITOR_GET_PRIVATE (monitor);
    
    monitor->priv->bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
    monitor->priv->array = g_ptr_array_new ();
    
    monitor->priv->proxy = dbus_g_proxy_new_for_name_owner (monitor->priv->bus,
							    "org.freedesktop.DBus",
							    "/org/freedesktop/DBus",
							    "org.freedesktop.DBus",
							    NULL);
    if ( !monitor->priv->proxy )
    {
	g_critical ("Unable to create proxy on /org/freedesktop/DBus");
	return;
    }
    
    dbus_g_proxy_add_signal (monitor->priv->proxy, "NameOwnerChanged", 
			     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
			     
    dbus_g_proxy_connect_signal (monitor->priv->proxy, "NameOwnerChanged",
				 G_CALLBACK(xfpm_dbus_monitor_name_owner_changed_cb), monitor, NULL);
	
}

static void
xfpm_dbus_monitor_finalize (GObject *object)
{
    XfpmDBusMonitor *monitor;
    guint i;
    gchar *name;

    monitor = XFPM_DBUS_MONITOR (object);
    
    dbus_g_connection_unref (monitor->priv->bus);
    
    g_object_unref (monitor->priv->proxy);

    for ( i = 0; i<monitor->priv->array->len; i++)
    {
	name = g_ptr_array_index (monitor->priv->array, i);
	g_ptr_array_remove (monitor->priv->array, name);
	g_free (name);
    }
    
    g_ptr_array_free (monitor->priv->array, TRUE);

    G_OBJECT_CLASS (xfpm_dbus_monitor_parent_class)->finalize (object);
}

XfpmDBusMonitor *
xfpm_dbus_monitor_new (void)
{
    if ( xfpm_dbus_monitor_object != NULL )
    {
	g_object_ref (xfpm_dbus_monitor_object);
    }
    else
    {
	xfpm_dbus_monitor_object = g_object_new (XFPM_TYPE_DBUS_MONITOR, NULL);
	g_object_add_weak_pointer (xfpm_dbus_monitor_object, &xfpm_dbus_monitor_object);
    }
    
    return XFPM_DBUS_MONITOR (xfpm_dbus_monitor_object);
}

gboolean xfpm_dbus_monitor_add_match (XfpmDBusMonitor *monitor, const gchar *unique_name)
{
    guint i = 0;
    gchar *name;
    
    g_return_val_if_fail (XFPM_IS_DBUS_MONITOR (monitor), FALSE);
    g_return_val_if_fail (unique_name != NULL, FALSE);
    
    for ( i = 0; i<monitor->priv->array->len; i++)
    {
	name = g_ptr_array_index (monitor->priv->array, i);
	if ( g_strcmp0 (name, unique_name) == 0 )
	{
	    return FALSE;
	}
    }
    
    g_ptr_array_add (monitor->priv->array, g_strdup (unique_name));
    
    return TRUE;
}

gboolean xfpm_dbus_monitor_remove_match (XfpmDBusMonitor *monitor, const gchar *unique_name)
{
    guint i ;
    gchar *name;
    
    g_return_val_if_fail (XFPM_IS_DBUS_MONITOR (monitor), FALSE);
    
    for ( i = 0; i<monitor->priv->array->len; i++)
    {
	name = g_ptr_array_index (monitor->priv->array, i);
	
	if ( g_strcmp0 (name, unique_name) == 0 )
	{
	    g_free (name);
	    g_ptr_array_remove_index (monitor->priv->array, i);
	    return TRUE;
	}
    }
    return FALSE;
}
