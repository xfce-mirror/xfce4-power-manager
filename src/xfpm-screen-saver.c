/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/xfpm-dbus.h"

#include "xfpm-screen-saver.h"
#include "xfpm-dbus-monitor.h"

/* Init */
static void xfpm_screen_saver_class_init (XfpmScreenSaverClass *klass);
static void xfpm_screen_saver_init       (XfpmScreenSaver *srv);
static void xfpm_screen_saver_finalize   (GObject *object);

#define XFPM_SCREEN_SAVER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_SCREEN_SAVER, XfpmScreenSaverPrivate))

#define MAX_SCREEN_SAVER_INHIBITORS 10

struct XfpmScreenSaverPrivate
{
    DBusConnection  *bus;
    XfpmDBusMonitor *monitor;
    GPtrArray       *array;
    
    guint            inhibitors;
};

enum
{
    SCREEN_SAVER_INHIBITED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gpointer xfpm_screen_saver_object = NULL;

G_DEFINE_TYPE(XfpmScreenSaver, xfpm_screen_saver, G_TYPE_OBJECT)

static gchar *
xfpm_screen_saver_find_unique_name (XfpmScreenSaver *srv, const gchar *unique_name)
{
    gint i;
    gchar *name;
    
    for ( i = 0; i<srv->priv->array->len; i++)
    {
	name = g_ptr_array_index (srv->priv->array, i);
	if ( g_strcmp0 (name, unique_name) == 0 )
	    return name;
    }
    return NULL;
}

static void
xfpm_screen_saver_uninhibit_message (XfpmScreenSaver *srv, const gchar *unique_name)
{
    gchar *name;
    
    g_return_if_fail (srv->priv->inhibitors != 0 );
    
    name = xfpm_screen_saver_find_unique_name (srv, unique_name);
    
    if ( name )
    {
	TRACE ("%s", name);
	xfpm_dbus_monitor_remove_match (srv->priv->monitor, name);
	g_ptr_array_remove (srv->priv->array, name);
	g_free (name);
	
	srv->priv->inhibitors--;

	if ( srv->priv->inhibitors == 0 )
	{
	    g_signal_emit (G_OBJECT(srv), signals[SCREEN_SAVER_INHIBITED], 0, FALSE);
	}
    }
}

static void
xfpm_screen_saver_inhibit_message (XfpmScreenSaver *srv, const gchar *unique_name)
{
    if (xfpm_screen_saver_find_unique_name (srv, unique_name) )
	return /* We have it already!*/;
	
    TRACE ("%s", unique_name);
    g_ptr_array_add (srv->priv->array, g_strdup (unique_name));
    xfpm_dbus_monitor_add_match (srv->priv->monitor, unique_name);
    
    if ( srv->priv->inhibitors == 0 )
    {
	g_signal_emit (G_OBJECT(srv), signals[SCREEN_SAVER_INHIBITED], 0, TRUE);
    }
    
    srv->priv->inhibitors++;
}

static DBusHandlerResult 
xfpm_screen_saver_filter (DBusConnection *connection, DBusMessage *message, void *data)
{
    XfpmScreenSaver *srv = ( XfpmScreenSaver *)data;
    
    if ( dbus_message_is_method_call (message, "org.gnome.ScreenSaver", "Inhibit") )
    {
	xfpm_screen_saver_inhibit_message (srv, dbus_message_get_sender (message) );
	
    }
    else if ( dbus_message_is_method_call (message, "org.gnome.ScreenSaver", "UnInhibit") )
    {
	xfpm_screen_saver_uninhibit_message (srv, dbus_message_get_sender (message) );
    }
    else if ( dbus_message_is_method_call (message, "org.freedesktop.ScreenSaver", "Inhibit") )
    {
	xfpm_screen_saver_inhibit_message (srv, dbus_message_get_sender (message) );
    }
    else if ( dbus_message_is_method_call (message, "org.freedesktop.ScreenSaver", "UnInhibit") )
    {
	xfpm_screen_saver_uninhibit_message (srv, dbus_message_get_sender (message) );
    }
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; /* Keep on as we just want to spy */
}

static void
xfpm_screen_saver_connection_lost (XfpmDBusMonitor *monitor, gchar *unique_name, XfpmScreenSaver *srv)
{
    xfpm_screen_saver_uninhibit_message (srv, unique_name);
}

static void
xfpm_screen_saver_class_init(XfpmScreenSaverClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

     signals[SCREEN_SAVER_INHIBITED] =
	    g_signal_new("screen-saver-inhibited",
			 XFPM_TYPE_SCREEN_SAVER,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmScreenSaverClass, screen_saver_inhibited),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__BOOLEAN,
			 G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    object_class->finalize = xfpm_screen_saver_finalize;
    
    g_type_class_add_private(klass,sizeof(XfpmScreenSaverPrivate));
}

static void
xfpm_screen_saver_init(XfpmScreenSaver *srv)
{
    DBusError error;
    dbus_error_init (&error);
    
    srv->priv = XFPM_SCREEN_SAVER_GET_PRIVATE(srv);
    
    srv->priv->bus = dbus_bus_get (DBUS_BUS_SESSION, &error);
    srv->priv->monitor = xfpm_dbus_monitor_new ();
    g_signal_connect (srv->priv->monitor, "connection-lost",
		      G_CALLBACK (xfpm_screen_saver_connection_lost), srv);
		      
    srv->priv->array   = g_ptr_array_new ();
    srv->priv->inhibitors = 0;
    
    if ( dbus_error_is_set (&error) )
    {
	g_critical ("Unable to get session bus: %s", error.message);
	dbus_error_free (&error);
	goto out;
    }
    
    dbus_bus_add_match (srv->priv->bus, 
			"type='method_call',interface='org.gnome.ScreenSaver'",
			&error);
			
    if ( dbus_error_is_set (&error) )
    {
	g_warning ("Failed to add match for interface org.gnome.ScreenSaver: %s", error.message);
	dbus_error_free (&error);
    }
	
    dbus_error_init (&error);
    
    dbus_bus_add_match (srv->priv->bus, 
			"type='method_call',interface='org.freedesktop.ScreenSaver'",
			&error);
			
    if ( dbus_error_is_set (&error) )
    {
	g_warning ("Failed to add match for interface org.freedesktop.ScreenSaver: %s", error.message);
	dbus_error_free (&error);
    }
	
    if (!dbus_connection_add_filter (srv->priv->bus, xfpm_screen_saver_filter, srv, NULL) )
    {
	g_warning ("Couldn't add filter");
    }
out:
    ;
}

static void
xfpm_screen_saver_finalize(GObject *object)
{
    XfpmScreenSaver *srv;
    gint i;
    gchar *name;

    srv = XFPM_SCREEN_SAVER(object);
    
    dbus_bus_remove_match (srv->priv->bus,
			  "type='method_call',interface='org.gnome.ScreenSaver'", 
			  NULL);
    
    dbus_bus_remove_match (srv->priv->bus,
			  "type='method_call',interface='org.freedesktop.ScreenSaver'", 
			  NULL);

    dbus_connection_unref (srv->priv->bus);
    
    g_object_unref (srv->priv->monitor);

    
    for ( i = 0; i<srv->priv->array->len; i++)
    {
	name = g_ptr_array_index (srv->priv->array, i);
	g_ptr_array_remove (srv->priv->array, name);
	g_free (name);
    }
    
    g_ptr_array_free (srv->priv->array, TRUE);

    G_OBJECT_CLASS(xfpm_screen_saver_parent_class)->finalize(object);
}

XfpmScreenSaver *
xfpm_screen_saver_new(void)
{
    if ( xfpm_screen_saver_object != NULL )
    {
	g_object_ref (xfpm_screen_saver_object);
    }
    else
    {
	xfpm_screen_saver_object = g_object_new (XFPM_TYPE_SCREEN_SAVER, NULL);
	g_object_add_weak_pointer (xfpm_screen_saver_object, &xfpm_screen_saver_object);
    }
    return XFPM_SCREEN_SAVER (xfpm_screen_saver_object);
}
