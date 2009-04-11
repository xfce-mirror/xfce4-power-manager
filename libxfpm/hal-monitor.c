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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <dbus/dbus-glib.h>

#include "hal-monitor.h"

/* Init */
static void hal_monitor_class_init (HalMonitorClass *klass);
static void hal_monitor_init       (HalMonitor *monitor);
static void hal_monitor_finalize   (GObject *object);

#define HAL_MONITOR_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_MONITOR, HalMonitorPrivate))

struct HalMonitorPrivate
{
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    
    gboolean         hal_running;
};

enum
{
    CONNECTION_CHANGED,
    LAST_SIGNAL
};

static gpointer hal_monitor_object = NULL;

static guint signals[LAST_SIGNAL] = { 0 };
    
G_DEFINE_TYPE(HalMonitor, hal_monitor, G_TYPE_OBJECT)

static void
hal_monitor_get_hal (HalMonitor *monitor)
{
    gboolean ret;
    GError *error = NULL;
    
    dbus_g_proxy_call (monitor->priv->proxy, "NameHasOwner", &error,
		       G_TYPE_STRING, "org.freedesktop.Hal",
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &ret,
		       G_TYPE_INVALID);
    
    if ( error )
    {
	g_critical ("%s", error->message);
	g_error_free (error);
	return;
    }
    
    if ( ret == FALSE )
    {
	g_critical ("Hal is not running");
	monitor->priv->hal_running = FALSE;
    }
    else
    {
	monitor->priv->hal_running = TRUE;
    }
}

static void
hal_monitor_name_owner_changed_cb (DBusGProxy *proxy, const gchar *name,
				   const gchar *prev, const gchar *new,
				   HalMonitor *monitor)
{
    if ( g_strcmp0 (name, "org.freedesktop.Hal") != 0)
	return;

    if ( strlen (prev) != 0)
    {
	monitor->priv->hal_running = FALSE;
	g_signal_emit (G_OBJECT (monitor), signals [CONNECTION_CHANGED], 0, FALSE);
    }
    else if ( strlen (new) != 0)
    {
	monitor->priv->hal_running = TRUE;
	g_signal_emit (G_OBJECT (monitor), signals [CONNECTION_CHANGED], 0, TRUE);
    }
}


static void
hal_monitor_get_dbus (HalMonitor *monitor)
{
    monitor->priv->proxy = dbus_g_proxy_new_for_name_owner (monitor->priv->bus,
							     "org.freedesktop.DBus",
							     "/org/freedesktop/DBus",
							     "org.freedesktop.DBus",
							     NULL);
    
    dbus_g_proxy_add_signal (monitor->priv->proxy, "NameOwnerChanged", 
			     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
			     
    dbus_g_proxy_connect_signal (monitor->priv->proxy, "NameOwnerChanged",
				 G_CALLBACK(hal_monitor_name_owner_changed_cb), monitor, NULL);
}

static void
hal_monitor_class_init(HalMonitorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[CONNECTION_CHANGED] =
    	g_signal_new("connection-changed",
		     HAL_TYPE_MONITOR,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalMonitorClass, connection_changed),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__BOOLEAN,
		     G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    object_class->finalize = hal_monitor_finalize;

    g_type_class_add_private (klass, sizeof(HalMonitorPrivate));
}

static void
hal_monitor_init (HalMonitor *monitor)
{
    GError *error = NULL;
    
    monitor->priv = HAL_MONITOR_GET_PRIVATE (monitor);
    
    monitor->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("error getting system bus connection: %s", error->message);
	g_error_free (error);
	goto out;
    }
    hal_monitor_get_dbus (monitor);
    hal_monitor_get_hal  (monitor);
out:
    ;
}

static void
hal_monitor_finalize(GObject *object)
{
    HalMonitor *monitor;

    monitor = HAL_MONITOR(object);

    dbus_g_connection_unref (monitor->priv->bus);
    
    if ( monitor->priv->proxy )
	g_object_unref (monitor->priv->proxy);

    G_OBJECT_CLASS(hal_monitor_parent_class)->finalize(object);
}

HalMonitor *
hal_monitor_new (void)
{
    if ( hal_monitor_object != NULL )
    {
	g_object_ref (hal_monitor_object );
    }
    else
    {
	hal_monitor_object = g_object_new (HAL_TYPE_MONITOR, NULL);
	g_object_add_weak_pointer (hal_monitor_object, &hal_monitor_object );
    }
    return HAL_MONITOR (hal_monitor_object );
}

gboolean hal_monitor_get_connected   (HalMonitor *monitor)
{
    g_return_val_if_fail (HAL_IS_MONITOR (monitor), FALSE);
    
    return monitor->priv->hal_running;
}
