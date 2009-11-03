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

/*
 * Based on code from gnome power manager
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
 * 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus-glib.h>

#include "hal-device.h"
#include "hal-marshal.h"

static void hal_device_finalize   (GObject *object);

#define HAL_DEVICE_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_DEVICE, HalDevicePrivate))

struct HalDevicePrivate
{
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    gchar           *udi;
    
    gboolean         watch_added;
    gboolean         watch_condition_added;
};

enum
{
    DEVICE_CHANGED,
    DEVICE_CONDITION,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(HalDevice, hal_device, G_TYPE_OBJECT)

static void
hal_device_class_init (HalDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    signals[DEVICE_CHANGED] =
	    g_signal_new("device-changed",
			 HAL_TYPE_DEVICE,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(HalDeviceClass, device_changed),
			 NULL, NULL,
			 _hal_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN,
			 G_TYPE_NONE, 4, 
			 G_TYPE_STRING, G_TYPE_STRING,
			 G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
			 
    signals[DEVICE_CONDITION] =
	    g_signal_new("device-condition",
			 HAL_TYPE_DEVICE,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(HalDeviceClass, device_condition),
			 NULL, NULL,
			 _hal_marshal_VOID__STRING_STRING,
			 G_TYPE_NONE, 2, 
			 G_TYPE_STRING, G_TYPE_STRING);
			 
    object_class->finalize = hal_device_finalize;

    g_type_class_add_private (klass, sizeof(HalDevicePrivate));
}

static void
hal_device_property_modified_cb (DBusGProxy *proxy, 
				 gint type, 
				 GPtrArray *properties, 
				 HalDevice *device)
{
    GValueArray *array;
    const gchar *udi;
    const gchar *key;
    gboolean     is_added;
    gboolean     is_removed;
    guint i;
    
    udi = dbus_g_proxy_get_path (proxy);
    
    for ( i = 0; i < properties->len; i++ )
    {
	array = g_ptr_array_index (properties, i);
	if ( array->n_values != 3 )
	    continue;
	    
	key = g_value_get_string (g_value_array_get_nth (array, 0));
	is_removed = g_value_get_boolean (g_value_array_get_nth (array, 1));
	is_added = g_value_get_boolean (g_value_array_get_nth (array, 2));
	g_signal_emit (G_OBJECT(device), signals[DEVICE_CHANGED], 0, udi, key, is_added, is_removed);
    }
}
				 
static void
hal_device_add_watch (HalDevice *device)
{
    GType array_gtype, gtype;

    g_return_if_fail ( DBUS_IS_G_PROXY (device->priv->proxy) );

    gtype = dbus_g_type_get_struct ("GValueArray",
				    G_TYPE_STRING,
				    G_TYPE_BOOLEAN,
				    G_TYPE_BOOLEAN,
				    G_TYPE_INVALID);

    array_gtype = dbus_g_type_get_collection ("GPtrArray", gtype);

    dbus_g_object_register_marshaller (_hal_marshal_VOID__INT_BOXED,
					G_TYPE_NONE, G_TYPE_INT,
				        array_gtype, G_TYPE_INVALID);
    
    				   
    dbus_g_proxy_add_signal (device->priv->proxy, 
			     "PropertyModified", 
			     G_TYPE_INT,
			     array_gtype, 
			     G_TYPE_INVALID);
    
    dbus_g_proxy_connect_signal (device->priv->proxy, "PropertyModified",
				 G_CALLBACK (hal_device_property_modified_cb), device, NULL);
    
    device->priv->watch_added = TRUE;
}

static void
hal_device_condition_cb (DBusGProxy  *proxy,
			 const gchar *condition,
			 const gchar *details,
			 HalDevice  *device)
{
    g_signal_emit (device, signals [DEVICE_CONDITION], 0, condition, details);
}

static void
hal_device_add_watch_condition (HalDevice *device)
{
    g_return_if_fail ( DBUS_IS_G_PROXY (device->priv->proxy) );
    
    dbus_g_object_register_marshaller (_hal_marshal_VOID__STRING_STRING,
				       G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING,
				       G_TYPE_INVALID);

    dbus_g_proxy_add_signal (device->priv->proxy, "Condition",
			     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
			     
    dbus_g_proxy_connect_signal (device->priv->proxy, "Condition",
				     G_CALLBACK (hal_device_condition_cb), device, NULL);
				     
    device->priv->watch_condition_added = TRUE;
}

static void
hal_device_proxy_destroy_cb (HalDevice *device)
{
    g_return_if_fail (HAL_IS_DEVICE (device));
    
    device->priv->proxy = NULL;
}

static void
hal_device_init (HalDevice *device)
{
    GError *error = NULL;
    
    device->priv = HAL_DEVICE_GET_PRIVATE(device);
    
    device->priv->proxy  	= NULL;
    device->priv->udi           = NULL;
    device->priv->watch_added 	= FALSE;
    device->priv->watch_condition_added = FALSE;
    
    device->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Unable to get bus system: %s\n", error->message);
	g_error_free (error);
    }
   
}

static void
hal_device_finalize (GObject *object)
{
    HalDevice *device;

    device = HAL_DEVICE (object);

    if ( device->priv->udi )
	g_free (device->priv->udi);

    if ( device->priv->bus )
	dbus_g_connection_unref (device->priv->bus);
	
    if ( device->priv->watch_added && device->priv->proxy )
	dbus_g_proxy_disconnect_signal (device->priv->proxy, "PropertyModified",
					G_CALLBACK(hal_device_property_modified_cb), device);
					
    if ( device->priv->watch_condition_added && device->priv->proxy )
    {
	dbus_g_proxy_disconnect_signal (device->priv->proxy, "Condition",
					G_CALLBACK(hal_device_condition_cb), device);
    }
    
    
    if ( device->priv->proxy )
	g_object_unref (device->priv->proxy);

    G_OBJECT_CLASS (hal_device_parent_class)->finalize (object);
}

HalDevice *
hal_device_new (void)
{
    HalDevice *device = NULL;
    device = g_object_new (HAL_TYPE_DEVICE, NULL);
    
    return device;
}

void hal_device_set_udi (HalDevice *device, const gchar *udi)
{
    g_return_if_fail (HAL_IS_DEVICE (device));
    
    if ( device->priv->udi )
    {
	g_free (device->priv->udi);
	device->priv->udi = NULL;
	g_object_unref (device->priv->proxy);
    }
    
    g_return_if_fail (udi != NULL);
	
    device->priv->udi = g_strdup (udi);
    
    device->priv->proxy = dbus_g_proxy_new_for_name (device->priv->bus,
						     "org.freedesktop.Hal",
						     device->priv->udi,
						     "org.freedesktop.Hal.Device");
    g_signal_connect_swapped (G_OBJECT (device->priv->proxy), "destroy",
			      G_CALLBACK (hal_device_proxy_destroy_cb), device);
}

const gchar *hal_device_get_udi (HalDevice *device)
{
    g_return_val_if_fail (HAL_IS_DEVICE(device), NULL);
    
    return device->priv->udi;
}

gboolean
hal_device_watch (HalDevice *device)
{
    g_return_val_if_fail (device->priv->udi != NULL, FALSE);
    hal_device_add_watch(device);

    return device->priv->watch_added;
}

gboolean
hal_device_watch_condition (HalDevice *device)
{
    g_return_val_if_fail (device->priv->udi != NULL, FALSE);
    
    hal_device_add_watch_condition (device);

    return device->priv->watch_condition_added;
}
    
gboolean hal_device_get_property_bool (HalDevice *device, const gchar *property)
{
    gboolean value = FALSE;
    GError *error = NULL;
    
    g_return_val_if_fail (HAL_IS_DEVICE(device), FALSE);
    g_return_val_if_fail (device->priv->udi != NULL, FALSE);
    
    dbus_g_proxy_call (device->priv->proxy, "GetPropertyBoolean", &error,
		       G_TYPE_STRING, property,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &value,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_critical ("Error getting bool property on device %s: %s\n", device->priv->udi, error->message);
	g_error_free (error);
    }
    
    return value;
}
									 
gint hal_device_get_property_int (HalDevice *device, const gchar *property)
{
    gint value = 0;
    GError *error = NULL;
    
    g_return_val_if_fail (HAL_IS_DEVICE(device), 0);
    g_return_val_if_fail (device->priv->udi != NULL, 0);
    
    dbus_g_proxy_call (device->priv->proxy, "GetPropertyInteger", &error,
		       G_TYPE_STRING, property,
		       G_TYPE_INVALID,
		       G_TYPE_INT, &value,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_critical ("Error getting integer property on device %s: %s\n", device->priv->udi, error->message);
	g_error_free (error);
    }
    
    return value;
}

gchar *hal_device_get_property_string  (HalDevice *device, const gchar *property)
{
    gchar *value = NULL;
    GError *error = NULL;
    
    g_return_val_if_fail (HAL_IS_DEVICE(device), NULL);
    g_return_val_if_fail (device->priv->udi != NULL, NULL);
    
    dbus_g_proxy_call (device->priv->proxy, "GetPropertyString", &error,
		       G_TYPE_STRING, property,
		       G_TYPE_INVALID,
		       G_TYPE_STRING, &value,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_critical ("Error getting string property on device %s: %s\n", device->priv->udi, error->message);
	g_error_free (error);
    }
    
    return value;
}

gboolean hal_device_has_key (HalDevice *device, const gchar *key)
{
    gboolean value = FALSE;
    GError *error = NULL;
    
    g_return_val_if_fail (HAL_IS_DEVICE(device), FALSE);
    g_return_val_if_fail (device->priv->udi != NULL, FALSE);
    
    dbus_g_proxy_call (device->priv->proxy, "PropertyExists", &error,
		       G_TYPE_STRING, key,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &value,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_critical ("Error getting property exists on device %s: %s\n", device->priv->udi, error->message);
	g_error_free (error);
    }
    
    return value;
}

									 
gboolean hal_device_has_capability (HalDevice *device, const gchar *capability)
{
    gboolean value = FALSE;
    GError *error = NULL;
    
    g_return_val_if_fail (HAL_IS_DEVICE(device), FALSE);
    g_return_val_if_fail (device->priv->udi != NULL, FALSE);
    
    dbus_g_proxy_call (device->priv->proxy, "QueryCapability", &error,
		       G_TYPE_STRING, capability,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &value,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_critical ("Error getting querying capability on device %s: %s\n", device->priv->udi, error->message);
	g_error_free (error);
    }
    
    return value;
}
