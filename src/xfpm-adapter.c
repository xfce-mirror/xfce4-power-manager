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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/hal-manager.h"
#include "libxfpm/hal-device.h"
#include "libxfpm/xfpm-string.h"

#include "xfpm-adapter.h"

/* Init */
static void xfpm_adapter_class_init (XfpmAdapterClass *klass);
static void xfpm_adapter_init       (XfpmAdapter *adapter);
static void xfpm_adapter_finalize   (GObject *object);

#define XFPM_ADAPTER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_ADAPTER, XfpmAdapterPrivate))

struct XfpmAdapterPrivate
{
    HalDevice 	 *device;
    gboolean      present;
};

enum
{
    ADAPTER_CHANGED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static gpointer xfpm_adapter_object = NULL;

G_DEFINE_TYPE(XfpmAdapter, xfpm_adapter, G_TYPE_OBJECT)

static void
xfpm_adapter_device_changed_cb (HalDevice *device, const gchar *udi, const gchar *key,
			        gboolean is_added, gboolean is_removed, XfpmAdapter *adapter)
{
    //FIXME React correctly is device is removed
    if ( xfpm_strequal(key, "ac_adapter.present") )
    {
	adapter->priv->present = hal_device_get_property_bool (adapter->priv->device, "ac_adapter.present");
	g_signal_emit (G_OBJECT(adapter), signals[ADAPTER_CHANGED], 0, adapter->priv->present);
    }
    
}

static void
xfpm_adapter_class_init(XfpmAdapterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[ADAPTER_CHANGED] = 
    	g_signal_new("adapter-changed",
                      XFPM_TYPE_ADAPTER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmAdapterClass, adapter_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
		      
    object_class->finalize = xfpm_adapter_finalize;

    g_type_class_add_private (klass, sizeof(XfpmAdapterPrivate));
}

static void
xfpm_adapter_init(XfpmAdapter *adapter)
{
    HalManager *manager;
    gchar **udi = NULL;
    gchar *form_factor = NULL;
    
    adapter->priv = XFPM_ADAPTER_GET_PRIVATE(adapter);
    
    adapter->priv->device = hal_device_new ();
    hal_device_set_udi (adapter->priv->device, "/org/freedesktop/Hal/devices/computer");
    
    form_factor = hal_device_get_property_string (adapter->priv->device, "system.formfactor");
        
    TRACE("System formfactor=%s\n", form_factor); //FIXME Use this value
    g_free(form_factor);
        
    manager = hal_manager_new ();
    
    udi = hal_manager_find_device_by_capability (manager, "ac_adapter");
    
    if (!udi )//FIXME Adapter should be present on laptops
	goto out;
	
    TRACE("Found AC Adapter with udi=%s\n", udi[0]);
    
    hal_device_set_udi (adapter->priv->device, udi[0]);
    
    hal_manager_free_string_array (udi);
    
    adapter->priv->present = hal_device_get_property_bool (adapter->priv->device, "ac_adapter.present");
    g_signal_connect (adapter->priv->device, "device-changed",
		      G_CALLBACK(xfpm_adapter_device_changed_cb), adapter);
		      
    hal_device_watch (adapter->priv->device);
out:
    g_object_unref (manager);
    
}

static void
xfpm_adapter_finalize(GObject *object)
{
    XfpmAdapter *adapter;

    adapter = XFPM_ADAPTER(object);
    
    if ( adapter->priv->device )
	g_object_unref (adapter->priv->device);
    
    G_OBJECT_CLASS(xfpm_adapter_parent_class)->finalize(object);
}

XfpmAdapter *
xfpm_adapter_new (void)
{
    if ( xfpm_adapter_object != NULL )
    {
	g_object_ref (xfpm_adapter_object);
    }
    else
    {
	xfpm_adapter_object = g_object_new (XFPM_TYPE_ADAPTER, NULL);
	g_object_add_weak_pointer (xfpm_adapter_object, &xfpm_adapter_object);
    }
    return XFPM_ADAPTER (xfpm_adapter_object);
}

gboolean xfpm_adapter_get_present (XfpmAdapter *adapter)
{
    g_return_val_if_fail (XFPM_IS_ADAPTER(adapter), FALSE);
    
    return adapter->priv->present;
}
