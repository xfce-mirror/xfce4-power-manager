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

#include "hal-proxy.h"

/* Init */
static void hal_proxy_class_init (HalProxyClass *klass);
static void hal_proxy_init       (HalProxy *hal_proxy);
static void hal_proxy_finalize   (GObject *object);

#define HAL_PROXY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_PROXY, HalProxyPrivate))

struct HalProxyPrivate
{
    DBusGProxy *proxy;
};

enum
{
    HAL_DISCONNECTED,
    HAL_CONNECTED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
    
G_DEFINE_TYPE(HalProxy, hal_proxy, G_TYPE_OBJECT)
/*
static void
hal_proxy_destroy_cb (DBusGProxy *proxy, HalProxy *hal_proxy)
{
    g_signal_emit (G_OBJECT(hal_proxy), signals[HAL_DISCONNECTED], 0);
}
*/

static void
hal_proxy_name_owner_changed_cb (DBusGProxy *proxy, const gchar *name,
				 const gchar *prev, const gchar *new,
				 HalProxy *hproxy)
{
    
    g_print("name=%s prev=%s new=%s\n", name, prev, new);
}


static void
hal_proxy_name_aquired_cb (DBusGProxy *proxy, const gchar *name, gpointer data)
{
    
    g_print("name=%s\n", name);
}

static void
hal_proxy_get (HalProxy *hal_proxy)
{
    GError *error = NULL;
    DBusGConnection *bus;
    
    bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("error getting system bus connection: %s", error->message);
	g_error_free (error);
	return;
    }
    
    hal_proxy->priv->proxy = dbus_g_proxy_new_for_name_owner (bus,
							      "org.freedesktop.Hal",
							      "/org/freedesktop/Hal/Manager",
							      "org.freedesktop.Hal.Manager",
							      &error);
    if ( error )
    {
	g_critical ("Error getting proxy to hald: %s", error->message);
	g_error_free (error);
	return;
    }
    
    //g_signal_connect (hal_proxy->priv->proxy, "destroy",
	//	      G_CALLBACK(hal_proxy_destroy_cb), hal_proxy);
    
    dbus_g_proxy_add_signal (hal_proxy->priv->proxy, "NameOwnerChanged", 
			     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
			     
    dbus_g_proxy_connect_signal (hal_proxy->priv->proxy, "NameOwnerChanged",
				 G_CALLBACK(hal_proxy_name_owner_changed_cb), hal_proxy, NULL);
    		     
    dbus_g_proxy_add_signal (hal_proxy->priv->proxy, "NameAcquired", 
			     G_TYPE_STRING, G_TYPE_INVALID);
			     
    dbus_g_proxy_connect_signal (hal_proxy->priv->proxy, "NameAcquired",
				 G_CALLBACK(hal_proxy_name_aquired_cb), hal_proxy, NULL);
}

static void
hal_proxy_class_init(HalProxyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[HAL_DISCONNECTED] =
    	g_signal_new("hal-disconnected",
		     HAL_TYPE_PROXY,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalProxyClass, hal_disconnected),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0, G_TYPE_NONE);

    object_class->finalize = hal_proxy_finalize;

    g_type_class_add_private(klass,sizeof(HalProxyPrivate));
}

static void
hal_proxy_init(HalProxy *hal_proxy)
{
    hal_proxy->priv = HAL_PROXY_GET_PRIVATE(hal_proxy);
    
    hal_proxy_get (hal_proxy);
}

static void
hal_proxy_finalize(GObject *object)
{
    HalProxy *hal_proxy;

    hal_proxy = HAL_PROXY(object);

    G_OBJECT_CLASS(hal_proxy_parent_class)->finalize(object);
}

HalProxy *
hal_proxy_new(void)
{
    HalProxy *hal_proxy = NULL;
    hal_proxy = g_object_new(HAL_TYPE_PROXY,NULL);
    return hal_proxy;
}
