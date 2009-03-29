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

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/xfpm-dbus.h"

#include "xfpm-screen-saver.h"

/* Init */
static void xfpm_screen_saver_class_init (XfpmScreenSaverClass *klass);
static void xfpm_screen_saver_init       (XfpmScreenSaver *srv);
static void xfpm_screen_saver_finalize   (GObject *object);

#define XFPM_SCREEN_SAVER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_SCREEN_SAVER, XfpmScreenSaverPrivate))

struct XfpmScreenSaverPrivate
{
    DBusConnection *bus;
};

enum
{
    SCREEN_SAVER_INHIBITED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gpointer xfpm_screen_saver_object = NULL;

G_DEFINE_TYPE(XfpmScreenSaver, xfpm_screen_saver, G_TYPE_OBJECT)

static DBusHandlerResult 
xfpm_screen_saver_filter (DBusConnection *connection, DBusMessage *message, void *data)
{
    XfpmScreenSaver *srv = ( XfpmScreenSaver *)data;
    
    if ( dbus_message_is_method_call (message, "org.gnome.ScreenSaver", "Inhibit") )
    {
	g_signal_emit (G_OBJECT(srv), signals[SCREEN_SAVER_INHIBITED], 0, TRUE);
    }
    else if ( dbus_message_is_method_call (message, "org.gnome.ScreenSaver", "UnInhibit") )
    {
	g_signal_emit (G_OBJECT(srv), signals[SCREEN_SAVER_INHIBITED], 0, FALSE);
    }
    else if ( dbus_message_is_method_call (message, "org.freedesktop.ScreenSaver", "Inhibit") )
    {
	g_signal_emit (G_OBJECT(srv), signals[SCREEN_SAVER_INHIBITED], 0, TRUE);
    }
    else if ( dbus_message_is_method_call (message, "org.freedesktop.ScreenSaver", "UnInhibit") )
    {
	g_signal_emit (G_OBJECT(srv), signals[SCREEN_SAVER_INHIBITED], 0, FALSE);
    }
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; /* Keep on as we just want to spy */
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

    srv = XFPM_SCREEN_SAVER(object);
    
    dbus_bus_remove_match (srv->priv->bus,
			  "type='method_call',interface='org.gnome.ScreenSaver'", 
			  NULL);
    
    dbus_bus_remove_match (srv->priv->bus,
			  "type='method_call',interface='org.freedesktop.ScreenSaver'", 
			  NULL);

    dbus_connection_unref (srv->priv->bus);

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
