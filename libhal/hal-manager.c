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
#include <glib.h>

#include "hal-manager.h"
#include "hal-device.h"

static void hal_manager_finalize   (GObject *object);

#define HAL_MANAGER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_MANAGER, HalManagerPrivate))

struct HalManagerPrivate
{
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    gboolean 	     connected;
    gboolean         is_laptop;
};

enum
{
    DEVICE_ADDED,
    DEVICE_REMOVED,
    LAST_SIGNAL
};

static gpointer hal_manager_object = NULL;

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
    manager->priv->connected = TRUE;
    
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
hal_manager_get_is_laptop_internal (HalManager *manager)
{
    HalDevice *device;
    gchar *form_factor;
    
    device = hal_device_new ();
    
    hal_device_set_udi (device, "/org/freedesktop/Hal/devices/computer");

    form_factor = hal_device_get_property_string (device, "system.formfactor");

    if ( g_strcmp0 (form_factor, "laptop") == 0)
	manager->priv->is_laptop = TRUE;
    else
	manager->priv->is_laptop = FALSE;
    
    if (form_factor)
	g_free (form_factor);

    g_object_unref (device);
}

static void
hal_manager_class_init(HalManagerClass *klass)
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
    
    hal_manager_connect (manager);
    hal_manager_get_is_laptop_internal (manager);
}

static void
hal_manager_finalize(GObject *object)
{
    HalManager *manager;

    manager = HAL_MANAGER(object);
    
    if ( manager->priv->proxy )
	g_object_unref (manager->priv->proxy);
	
    if ( manager->priv->bus )
	dbus_g_connection_unref (manager->priv->bus);

    G_OBJECT_CLASS(hal_manager_parent_class)->finalize(object);
}

HalManager *
hal_manager_new (void)
{
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

gboolean hal_manager_get_is_laptop (HalManager *manager)
{
    g_return_val_if_fail (HAL_IS_MANAGER (manager), FALSE);
    
    return manager->priv->is_laptop;
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
