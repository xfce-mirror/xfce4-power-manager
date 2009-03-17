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

#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "libxfpm/xfpm-string.h"
#include "libxfpm/hal-proxy.h"
#include "libxfpm/xfpm-dbus.h"

#include "xfpm-manager.h"
#include "xfpm-engine.h"
#include "xfpm-manager.h"

/* Init */
static void xfpm_manager_class_init (XfpmManagerClass *klass);
static void xfpm_manager_init       (XfpmManager *xfpm_manager);
static void xfpm_manager_finalize   (GObject *object);

static void xfpm_manager_dbus_class_init (XfpmManagerClass *klass);
static void xfpm_manager_dbus_init	 (XfpmManager *manager);

#define XFPM_MANAGER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_MANAGER, XfpmManagerPrivate))

struct XfpmManagerPrivate
{
    XfpmEngine 	    *engine;
    HalProxy        *hproxy;
    
    DBusGConnection *session_bus;
};

G_DEFINE_TYPE(XfpmManager, xfpm_manager, G_TYPE_OBJECT)

static void
xfpm_manager_hal_disconnected_cb (HalProxy *hproxy, XfpmManager *manager)
{
    TRACE("hald disconnected ");
    //g_object_unref (manager->priv->engine);
}

static void
xfpm_manager_class_init (XfpmManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_manager_finalize;

    g_type_class_add_private(klass,sizeof(XfpmManagerPrivate));
}

static void
xfpm_manager_init(XfpmManager *manager)
{
    manager->priv = XFPM_MANAGER_GET_PRIVATE(manager);

    manager->priv->session_bus   = NULL;
    manager->priv->engine        = NULL;
    manager->priv->hproxy        = NULL;
}

static void
xfpm_manager_finalize(GObject *object)
{
    XfpmManager *manager;

    manager = XFPM_MANAGER(object);

    if ( manager->priv->session_bus )
	dbus_g_connection_unref(manager->priv->session_bus);
	
    if ( manager->priv->engine )
    	g_object_unref (manager->priv->engine);

    G_OBJECT_CLASS(xfpm_manager_parent_class)->finalize(object);
}

static gboolean
xfpm_manager_quit (XfpmManager *manager)
{
    xfpm_dbus_release_name(dbus_g_connection_get_connection(manager->priv->session_bus),
			   "org.xfce.PowerManager");

    xfpm_dbus_release_name (dbus_g_connection_get_connection(manager->priv->session_bus),
			    "org.freedesktop.PowerManagement");
				  
    g_object_unref(G_OBJECT(manager));
    gtk_main_quit ();
    return TRUE;
}

XfpmManager *
xfpm_manager_new(DBusGConnection *bus)
{
    XfpmManager *manager = NULL;
    manager = g_object_new(XFPM_TYPE_MANAGER,NULL);

    manager->priv->session_bus = bus;
    
    xfpm_manager_dbus_class_init(XFPM_MANAGER_GET_CLASS(manager));
    xfpm_manager_dbus_init(manager);
    
    return manager;
}

void xfpm_manager_start (XfpmManager *manager)
{
    if ( !xfpm_dbus_register_name (dbus_g_connection_get_connection(manager->priv->session_bus),
				  "org.xfce.PowerManager") ) 
    {
	g_critical("Unable to reserve bus name\n");
    }
    
    if (!xfpm_dbus_register_name (dbus_g_connection_get_connection(manager->priv->session_bus),
				  "org.freedesktop.PowerManagement") )
    {
    
	g_critical ("Unable to reserve bus name\n");
    }
    
    manager->priv->hproxy = hal_proxy_new ();
    
    g_signal_connect (manager->priv->hproxy, "hal-disconnected",
		      G_CALLBACK(xfpm_manager_hal_disconnected_cb), manager);
    
    
    manager->priv->engine = xfpm_engine_new ();
    
}

/*
 * 
 * DBus server implementation
 * 
 */
static gboolean xfpm_manager_dbus_quit     (XfpmManager *manager,
					    GError **error);
static gboolean xfpm_manager_dbus_get_all  (XfpmManager *manager,
					    GHashTable **OUT_hash,
					    GError **error);
static gboolean xfpm_manager_dbus_get_info (XfpmManager *manager,
					    gchar **OUT_name,
					    gchar **OUT_version,
					    gchar **OUT_vendor,
					    GError **error);

#include "xfce-power-manager-dbus-server.h"

static void
xfpm_manager_dbus_class_init(XfpmManagerClass *klass)
{
     dbus_g_object_type_install_info(G_TYPE_FROM_CLASS(klass),
				    &dbus_glib_xfpm_manager_object_info);
}

static void
xfpm_manager_dbus_init(XfpmManager *manager)
{
    dbus_g_connection_register_g_object(manager->priv->session_bus,
					"/org/xfce/PowerManager",
					G_OBJECT(manager));
}

static gboolean
xfpm_manager_dbus_quit(XfpmManager *manager, GError **error)
{
    TRACE("Quit message received\n");
    
    xfpm_manager_quit(manager);
    
    return TRUE;
}

static gboolean
xfpm_manager_dbus_get_all(XfpmManager *manager, 
			  GHashTable **OUT_hash, 
			  GError **error)
{
    TRACE("Get all message received\n");

   *OUT_hash = g_hash_table_new(g_str_hash, g_str_equal);
    
    return TRUE;
}

static gboolean 
xfpm_manager_dbus_get_info (XfpmManager *manager,
			    gchar **OUT_name,
			    gchar **OUT_version,
			    gchar **OUT_vendor,
			    GError **error)
{
    
    *OUT_name    = g_strdup(PACKAGE);
    *OUT_version = g_strdup(VERSION);
    *OUT_vendor  = g_strdup("Xfce-goodies");
    
    return TRUE;
}
