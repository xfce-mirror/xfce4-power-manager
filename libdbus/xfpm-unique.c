/*
 * * Copyright (C) 2010-2011 Ali <aliov@xfce.org>
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

#include "xfpm-unique.h"
#include "xfpm-dbus.h"

#include <dbus/dbus-glib-lowlevel.h>

static void xfpm_unique_dbus_class_init 	(XfpmUniqueClass *klass);
static void xfpm_unique_dbus_init		(XfpmUnique *unique);

static void xfpm_unique_finalize   		(GObject *object);

#define XFPM_UNIQUE_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_UNIQUE, XfpmUniquePrivate))

struct XfpmUniquePrivate
{
    DBusGConnection *bus;
    
    gchar *name;
};

enum
{
    PROP_0,
    PROP_NAME
};

enum
{
    PING_RECEIVED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmUnique, xfpm_unique, G_TYPE_OBJECT)

static void xfpm_unique_get_property (GObject *object,
				      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    XfpmUnique *unique;
    unique = XFPM_UNIQUE (object);
    
    switch (prop_id)
    {
	case PROP_NAME:
	    g_value_set_string (value, unique->priv->name);
	    break;
	default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

}

static void xfpm_unique_set_property (GObject *object,
				      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    
    XfpmUnique *unique;
    unique = XFPM_UNIQUE (object);
    
    switch (prop_id)
    {
	case PROP_NAME:
	    unique->priv->name = g_value_dup_string (value);
	    break;
	default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
xfpm_unique_class_init (XfpmUniqueClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_unique_finalize;
    
    object_class->set_property = xfpm_unique_set_property;
    object_class->get_property = xfpm_unique_get_property;

    signals [PING_RECEIVED] = 
        g_signal_new ("ping-received",
                      XFPM_TYPE_UNIQUE,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XfpmUniqueClass, ping_received),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    g_object_class_install_property (object_class,
                                     PROP_NAME,
                                     g_param_spec_string  ("name",
                                                           NULL, NULL,
                                                           NULL,
                                                           G_PARAM_READWRITE|
							   G_PARAM_CONSTRUCT_ONLY));

    g_type_class_add_private (klass, sizeof (XfpmUniquePrivate));
    
    xfpm_unique_dbus_class_init (klass);
}

static void
xfpm_unique_init (XfpmUnique *unique)
{
    GError *error = NULL;
    
    unique->priv = XFPM_UNIQUE_GET_PRIVATE (unique);
    
    unique->priv->name = NULL;
    
    unique->priv->bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    
    if ( error )
	g_error ("Failed to connect to the session bus : %s", error->message);
	
    xfpm_unique_dbus_init (unique);
}

static void
xfpm_unique_finalize (GObject *object)
{
    XfpmUnique *unique;

    unique = XFPM_UNIQUE (object);
    
    xfpm_dbus_release_name (dbus_g_connection_get_connection (unique->priv->bus),
			    unique->priv->name);
    
    dbus_g_connection_unref (unique->priv->bus);
    g_free (unique->priv->name);

    G_OBJECT_CLASS (xfpm_unique_parent_class)->finalize (object);
}

XfpmUnique *
xfpm_unique_new (const gchar *name)
{
    XfpmUnique *unique = NULL;
    
    unique = g_object_new (XFPM_TYPE_UNIQUE, "name", name, NULL);
    
    return unique;
}

gboolean xfpm_unique_app_is_running (XfpmUnique *unique)
{
    g_return_val_if_fail (XFPM_IS_UNIQUE (unique), FALSE);
    
    if (xfpm_dbus_name_has_owner (dbus_g_connection_get_connection (unique->priv->bus),
				  unique->priv->name))
    {
	DBusGProxy *proxy;
	GError *error = NULL;
	
	proxy = dbus_g_proxy_new_for_name (unique->priv->bus,
					   unique->priv->name,
					   "/org/xfce/unique",
					   "org.xfce.unique");
	
	/*Shoudln't happen, but check anyway*/
	if ( !proxy )
	{
	    g_critical ("Unable to create proxy for %s", unique->priv->name);
	    return FALSE;
	}
	
	dbus_g_proxy_call (proxy, "Ping", &error,
			   G_TYPE_INVALID,
			   G_TYPE_INVALID);
			   
	if ( error )
	{
	    g_warning ("Failed to 'Ping' %s", unique->priv->name);
	    
	}
	
	g_object_unref (proxy);
	return TRUE;
    }
    
    xfpm_dbus_register_name (dbus_g_connection_get_connection (unique->priv->bus),
			     unique->priv->name);
    
    return FALSE;
}

static gboolean xfce_unique_ping (XfpmUnique *unique,
				  GError *error);	

#include "org.xfce.unique.h"

static void xfpm_unique_dbus_class_init (XfpmUniqueClass *klass)
{
    dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
                                     &dbus_glib_xfce_unique_object_info);
}

static void xfpm_unique_dbus_init (XfpmUnique *unique)
{
    dbus_g_connection_register_g_object (unique->priv->bus,
                                         "/org/xfce/unique",
                                         G_OBJECT (unique));
}

static gboolean xfce_unique_ping (XfpmUnique *unique,
				  GError *error)
{
    g_signal_emit (unique, signals[PING_RECEIVED], 0);
    return TRUE;
}
