/*
 * * Copyright (C) 2008-2009 Ali <aliov@xfce.org>
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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#include "hal-manager.h"
#include "hal-device.h"

#include "libdbus/xfpm-dbus-monitor.h"
#include "libdbus/xfpm-dbus.h"

static void hal_manager_finalize   (GObject *object);

#define HAL_MANAGER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_MANAGER, HalManagerPrivate))

struct HalManagerPrivate
{
    XfpmDBusMonitor *monitor;
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    gboolean 	     connected;
    gboolean         is_laptop;
    
    gulong	     sig_hal;
};

enum
{
    DEVICE_ADDED,
    DEVICE_REMOVED,
    CONNECTION_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (HalManager, hal_manager, G_TYPE_OBJECT)

static void
hal_manager_device_added_cb (DBusGProxy *proxy, const gchar *udi, HalManager *manager)
{
    g_signal_emit (G_OBJECT(manager), signals[DEVICE_ADDED], 0, udi);
}

static void
hal_manager_device_removed_cb (DBusGProxy *proxy, const gchar *udi, HalManager *manager)
{
    g_signal_emit (G_OBJECT(manager), signals[DEVICE_REMOVED], 0, udi);
}

static void
hal_manager_connect (HalManager *manager)
{
    GError *error = NULL;
    
    manager->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Failed to get bus system %s\n", error->message);
	g_error_free (error);
	goto out;
    }
    
    manager->priv->connected = xfpm_dbus_name_has_owner (dbus_g_connection_get_connection (manager->priv->bus),
							 "org.freedesktop.Hal");
    
    if ( !manager->priv->connected )
	goto out;
    
    manager->priv->proxy = dbus_g_proxy_new_for_name (manager->priv->bus,
		  				      "org.freedesktop.Hal",
						      "/org/freedesktop/Hal/Manager",
						      "org.freedesktop.Hal.Manager");
    
    if ( !manager->priv->proxy )
    {
	g_critical ("Unable to get proxy for \n");
	goto out;
    }
    
    dbus_g_proxy_add_signal (manager->priv->proxy, "DeviceAdded",
			     G_TYPE_STRING, G_TYPE_INVALID);
			     
    dbus_g_proxy_connect_signal (manager->priv->proxy, "DeviceAdded",
				 G_CALLBACK (hal_manager_device_added_cb), manager, NULL);

    dbus_g_proxy_add_signal (manager->priv->proxy, "DeviceRemoved",
			     G_TYPE_STRING, G_TYPE_INVALID);
	
    dbus_g_proxy_connect_signal (manager->priv->proxy, "DeviceRemoved",
				 G_CALLBACK (hal_manager_device_removed_cb), manager, NULL);
out:
	;
}

static void
hal_manager_service_connection_changed_cb (XfpmDBusMonitor *monitor, 
					   gchar *service_name, 
					   gboolean is_connected,
					   gboolean on_session, 
					   HalManager *manager)
{
    if ( !on_session)
    {
	if (!g_strcmp0 (service_name, "org.freedesktop.Hal") )
	{
	    if ( manager->priv->connected != is_connected )
	    {
		manager->priv->connected = is_connected;
		g_signal_emit (G_OBJECT (manager), signals [CONNECTION_CHANGED], 0, is_connected);
	    }
	}
    }
}

static void
hal_manager_class_init (HalManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    signals[DEVICE_ADDED] =
    	g_signal_new("device-added",
		     HAL_TYPE_MANAGER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalManagerClass, device_added),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__STRING,
		     G_TYPE_NONE, 1, G_TYPE_STRING);
		     
    signals[DEVICE_REMOVED] =
    	g_signal_new("device-removed",
		     HAL_TYPE_MANAGER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalManagerClass, device_removed),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__STRING,
		     G_TYPE_NONE, 1, G_TYPE_STRING);
    
    signals[CONNECTION_CHANGED] =
    	g_signal_new("connection-changed",
		     HAL_TYPE_MANAGER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalManagerClass, connection_changed),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
    
    object_class->finalize = hal_manager_finalize;

    g_type_class_add_private (klass,sizeof(HalManagerPrivate));
}

static void
hal_manager_init (HalManager *manager)
{
    manager->priv = HAL_MANAGER_GET_PRIVATE (manager);
    
    manager->priv->bus 	     = NULL;
    manager->priv->proxy     = NULL;
    manager->priv->connected = FALSE;
    manager->priv->monitor   = xfpm_dbus_monitor_new ();
    
    hal_manager_connect (manager);
    
    xfpm_dbus_monitor_add_service (manager->priv->monitor, DBUS_BUS_SYSTEM, "org.freedesktop.Hal");
    
    manager->priv->sig_hal = 
    g_signal_connect (manager->priv->monitor, "service-connection-changed",
		      G_CALLBACK (hal_manager_service_connection_changed_cb), manager);
}

static void
hal_manager_finalize (GObject *object)
{
    HalManager *manager;

    manager = HAL_MANAGER(object);

    if ( g_signal_handler_is_connected (manager->priv->monitor, manager->priv->sig_hal) )
	g_signal_handler_disconnect (manager->priv->monitor, manager->priv->sig_hal);
    
    if ( manager->priv->proxy )
	g_object_unref (manager->priv->proxy);
	
    if ( manager->priv->bus )
	dbus_g_connection_unref (manager->priv->bus);

    xfpm_dbus_monitor_remove_service (manager->priv->monitor, DBUS_BUS_SYSTEM, "org.freedesktop.Hal");

    g_object_unref (manager->priv->monitor);

    G_OBJECT_CLASS(hal_manager_parent_class)->finalize(object);
}

HalManager *
hal_manager_new (void)
{
    static gpointer hal_manager_object = NULL;
    
    if ( hal_manager_object != NULL )
    {
	g_object_ref (hal_manager_object);
    }
    else
    {
	hal_manager_object = g_object_new (HAL_TYPE_MANAGER, NULL);
	g_object_add_weak_pointer (hal_manager_object, &hal_manager_object);
    }
    
    return HAL_MANAGER (hal_manager_object);
}

gchar **hal_manager_find_device_by_capability (HalManager *manager, const gchar *capability)
{
    GError *error = NULL;
    gchar  **udi = NULL;
    
    g_return_val_if_fail (HAL_IS_MANAGER(manager), NULL);
    g_return_val_if_fail (manager->priv->connected, NULL);
    
    dbus_g_proxy_call (manager->priv->proxy, "FindDeviceByCapability", &error, 
		       G_TYPE_STRING, capability, 
		       G_TYPE_INVALID,
		       G_TYPE_STRV, &udi,
		       G_TYPE_INVALID);
    if ( error )
    {
	g_critical ("Error finding devices by capability %s\n", error->message);
	g_error_free (error);
    }
    
    return udi;
}

void hal_manager_free_string_array (gchar **array)
{
    gint i;

    if (array == NULL)	
	    return;
    
    for (i=0; array[i]; i++) 
	g_free (array[i]);
    
    g_free (array);
}

gboolean hal_manager_get_is_connected (HalManager *manager)
{
    g_return_val_if_fail (HAL_IS_MANAGER (manager), FALSE);
    
    return manager->priv->connected;
}
