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

#include "hal-device.h"
#include "hal-marshal.h"

/* Init */
static void hal_device_class_init (HalDeviceClass *klass);
static void hal_device_init       (HalDevice *device);
static void hal_device_finalize   (GObject *object);

static void hal_device_set_property(GObject *object,
				    guint prop_id,
				    const GValue *value,
				    GParamSpec *pspec);

static void hal_device_get_property(GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec);

#define HAL_DEVICE_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_DEVICE, HalDevicePrivate))

struct HalDevicePrivate
{
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    gchar           *udi;
    
    gboolean         watch_added;
};

enum
{
    PROP_0,
    PROP_UDI
};

enum
{
    DEVICE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(HalDevice, hal_device, G_TYPE_OBJECT)

static void
hal_device_class_init(HalDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->set_property = hal_device_set_property;
    object_class->get_property = hal_device_get_property;
    
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
			 
    g_object_class_install_property(object_class,
				    PROP_UDI,
				    g_param_spec_string("udi",
							NULL, NULL,
						        NULL,
						        G_PARAM_CONSTRUCT_ONLY | 
							G_PARAM_WRITABLE |
							G_PARAM_READABLE));
    object_class->finalize = hal_device_finalize;

    g_type_class_add_private(klass,sizeof(HalDevicePrivate));
}

static void hal_device_set_property(GObject *object,
				    guint prop_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
    HalDevice *device = (HalDevice *) object;
    
    switch (prop_id )
    {
	case PROP_UDI:
	    device->priv->udi = g_strdup(g_value_get_string (value) );
	    break;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	    break;
    }
    
}

static void hal_device_get_property(GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec)
{
    HalDevice *device = (HalDevice *) object;
    
    switch (prop_id )
    {
	case PROP_UDI:
	    g_value_set_string (value, device->priv->udi);
	    break;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	    break;
    }
    
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
    int i;
    
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
    device->priv->proxy = dbus_g_proxy_new_for_name (device->priv->bus,
						     "org.freedesktop.Hal",
						     device->priv->udi,
						     "org.freedesktop.Hal.Device");

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
				 G_CALLBACK(hal_device_property_modified_cb), device, NULL);
    
    device->priv->watch_added = TRUE;
}

static void
hal_device_init(HalDevice *device)
{
    device->priv = HAL_DEVICE_GET_PRIVATE(device);
    
    device->priv->bus    	= NULL;
    device->priv->proxy  	= NULL;
    device->priv->udi           = NULL;
    device->priv->watch_added 	= FALSE;
    
}

static void
hal_device_finalize(GObject *object)
{
    HalDevice *device;

    device = HAL_DEVICE(object);
    
    if ( device->priv->udi )
	g_free (device->priv->udi);
	
    if ( device->priv->bus )
	dbus_g_connection_unref (device->priv->bus);
	
    if ( device->priv->watch_added )
	dbus_g_proxy_disconnect_signal (device->priv->proxy, "PropertyModified",
					G_CALLBACK(hal_device_property_modified_cb), device);
	
    if ( device->priv->proxy )
	g_object_unref (device->priv->proxy);

    G_OBJECT_CLASS(hal_device_parent_class)->finalize(object);
}

HalDevice *
hal_device_new (const gchar *udi)
{
    HalDevice *device = NULL;
    device = g_object_new (HAL_TYPE_DEVICE, "udi", udi, NULL);
    return device;
}

gboolean
hal_device_watch (HalDevice *device)
{
    GError *error = NULL;

    device->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Unable to get bus system: %s\n", error->message);
	g_error_free (error);
	goto out;
    }

    hal_device_add_watch(device);

out:
    return device->priv->watch_added;
    ;
}
