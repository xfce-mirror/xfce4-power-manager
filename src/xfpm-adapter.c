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

#include "libxfpm/xfpm-string.h"

#include "xfpm-adapter.h"
#include "xfpm-tray-icon.h"

/* Init */
static void xfpm_adapter_class_init (XfpmAdapterClass *klass);
static void xfpm_adapter_init       (XfpmAdapter *adapter);
static void xfpm_adapter_finalize   (GObject *object);

#define XFPM_ADAPTER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_ADAPTER, XfpmAdapterPrivate))

struct XfpmAdapterPrivate
{
    XfpmTrayIcon *icon;
    HalDevice    *device;
    gboolean      present;
};

enum
{
    ADAPTER_CHANGED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmAdapter, xfpm_adapter, G_TYPE_OBJECT)

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
    adapter->priv = XFPM_ADAPTER_GET_PRIVATE(adapter);
    
    adapter->priv->device = NULL;
    
    adapter->priv->icon = xfpm_tray_icon_new ();
    xfpm_tray_icon_set_visible (adapter->priv->icon, FALSE);
    xfpm_tray_icon_set_icon (adapter->priv->icon, "gpm-ac-adapter");
}

static void
xfpm_adapter_finalize(GObject *object)
{
    XfpmAdapter *adapter;

    adapter = XFPM_ADAPTER(object);
    
    if ( adapter->priv->icon )
    	g_object_unref (adapter->priv->icon);
	
    if ( adapter->priv->device)
    	g_object_unref (adapter->priv->device);

    G_OBJECT_CLASS(xfpm_adapter_parent_class)->finalize(object);
}

static void
xfpm_adapter_changed (XfpmAdapter *adapter)
{
    adapter->priv->present = hal_device_get_property_bool (adapter->priv->device, "ac_adapter.present");
    TRACE("Adapter changed is_present =%s", xfpm_bool_to_string(adapter->priv->present));
    g_signal_emit (G_OBJECT(adapter), signals[ADAPTER_CHANGED], 0, adapter->priv->present);
}

static void
xfpm_adapter_device_changed_cb (HalDevice *device,
				const gchar *udi,
				const gchar *key, 
				gboolean is_removed,
				gboolean is_added,
				XfpmAdapter *adapter)
{
    if ( xfpm_strequal (key, "ac_adapter.present") )
    {
	xfpm_adapter_changed (adapter);
    }
}

XfpmAdapter *
xfpm_adapter_new (const HalDevice *device)
{
    XfpmAdapter *adapter = NULL;
    adapter = g_object_new (XFPM_TYPE_ADAPTER, NULL);
    
    adapter->priv->device = g_object_ref (G_OBJECT(device));
    
    g_signal_connect (adapter->priv->device, "device-changed", 
		      G_CALLBACK (xfpm_adapter_device_changed_cb),
		      adapter);
    
    xfpm_adapter_changed (adapter);
    
    return adapter;
}

gboolean          xfpm_adapter_get_presence    (XfpmAdapter *adapter)
{
    g_return_val_if_fail (XFPM_IS_ADAPTER (adapter), FALSE);
    
    return adapter->priv->present;
}

void xfpm_adapter_set_visible (XfpmAdapter *adapter, gboolean visible)
{
    g_return_if_fail ( XFPM_IS_ADAPTER (adapter));
    
    xfpm_tray_icon_set_visible(adapter->priv->icon, visible);
    
}

void xfpm_adapter_set_tooltip (XfpmAdapter *adapter, const gchar *text)
{
     g_return_if_fail ( XFPM_IS_ADAPTER (adapter));
    
    xfpm_tray_icon_set_tooltip (adapter->priv->icon, text);
}
