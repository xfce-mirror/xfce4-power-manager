/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

#include <glib.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libxfce4util/libxfce4util.h>

#include "xfpm-inhibit.h"
#include "xfpm-dbus-monitor.h"
#include "xfpm-errors.h"
#include "xfpm-debug.h"

static void xfpm_inhibit_finalize   (GObject *object);

static void xfpm_inhibit_dbus_class_init  (XfpmInhibitClass *klass);
static void xfpm_inhibit_dbus_init	  (XfpmInhibit *inhibit);

#define XFPM_INHIBIT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_INHIBIT, XfpmInhibitPrivate))

struct XfpmInhibitPrivate
{
    XfpmDBusMonitor *monitor;
    GPtrArray       *array;
    gboolean         inhibited;
};

typedef struct
{
    gchar *app_name;
    gchar *unique_name;
    guint  cookie;
    
} Inhibitor;

enum
{
    HAS_INHIBIT_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmInhibit, xfpm_inhibit, G_TYPE_OBJECT)

static void
xfpm_inhibit_free_inhibitor (XfpmInhibit *inhibit, Inhibitor *inhibitor)
{
    g_return_if_fail (inhibitor != NULL );
    
    g_ptr_array_remove (inhibit->priv->array, inhibitor);
    
    g_free (inhibitor->app_name);
    g_free (inhibitor->unique_name);
    g_free (inhibitor);
}

static gboolean
xfpm_inhibit_has_inhibit_changed (XfpmInhibit *inhibit)
{
    if ( inhibit->priv->array->len == 0 && inhibit->priv->inhibited == TRUE )
    {
	XFPM_DEBUG("Inhibit removed");
	inhibit->priv->inhibited = FALSE;
	g_signal_emit (G_OBJECT(inhibit), signals[HAS_INHIBIT_CHANGED], 0, inhibit->priv->inhibited);
    }
    else if ( inhibit->priv->array->len != 0 && inhibit->priv->inhibited == FALSE )
    {
	XFPM_DEBUG("Inhibit added");
	inhibit->priv->inhibited = TRUE;
	g_signal_emit (G_OBJECT(inhibit), signals[HAS_INHIBIT_CHANGED], 0, inhibit->priv->inhibited);
    }
    
    return inhibit->priv->inhibited;
}

static guint
xfpm_inhibit_get_cookie (XfpmInhibit *inhibit)
{
    guint max = 0;
    guint i;
    Inhibitor *inhibitor;
    
    for ( i = 0; i<inhibit->priv->array->len; i++)
    {
	inhibitor = g_ptr_array_index (inhibit->priv->array, i);
	max = MAX (max, inhibitor->cookie);
    }
    return (guint) g_random_int_range ( max + 1, max + 40);
}

static guint
xfpm_inhibit_add_application (XfpmInhibit *inhibit, const gchar *app_name, const gchar *unique_name)
{
    guint cookie;
    Inhibitor *inhibitor;
    
    inhibitor = g_new0 (Inhibitor, 1);
    
    cookie = xfpm_inhibit_get_cookie (inhibit);
    
    inhibitor->cookie      = cookie;
    inhibitor->app_name    = g_strdup (app_name);
    inhibitor->unique_name = g_strdup (unique_name);
    
    g_ptr_array_add (inhibit->priv->array, inhibitor);
    
    return cookie;
}

static Inhibitor *
xfpm_inhibit_find_application_by_cookie (XfpmInhibit *inhibit, guint cookie)
{
    guint i;
    Inhibitor *inhibitor;
    for ( i = 0; i < inhibit->priv->array->len; i++)
    {
	inhibitor = g_ptr_array_index (inhibit->priv->array, i);
	if ( inhibitor->cookie == cookie )
	{
	    return inhibitor;
	}
    }
    return NULL;
}

static Inhibitor *
xfpm_inhibit_find_application_by_unique_connection_name (XfpmInhibit *inhibit, const gchar *unique_name)
{
    guint i;
    Inhibitor *inhibitor;
    for ( i = 0; i < inhibit->priv->array->len; i++)
    {
	inhibitor = g_ptr_array_index (inhibit->priv->array, i);
	if ( g_strcmp0 (inhibitor->unique_name, unique_name ) == 0 )
	{
	    return inhibitor;
	}
    }
    return NULL;
}

static gboolean
xfpm_inhibit_remove_application_by_cookie (XfpmInhibit *inhibit, guint cookie)
{
    Inhibitor *inhibitor;
    
    inhibitor = xfpm_inhibit_find_application_by_cookie (inhibit, cookie);
    
    if ( inhibitor )
    {
	xfpm_dbus_monitor_remove_unique_name (inhibit->priv->monitor, DBUS_BUS_SESSION, inhibitor->unique_name);
	xfpm_inhibit_free_inhibitor (inhibit, inhibitor);
	return TRUE;
    }
    return FALSE;
}

static void
xfpm_inhibit_connection_lost_cb (XfpmDBusMonitor *monitor, gchar *unique_name, 
				 gboolean on_session, XfpmInhibit *inhibit)
{
    Inhibitor *inhibitor;
    
    if ( !on_session)
	return;
    
    inhibitor = xfpm_inhibit_find_application_by_unique_connection_name (inhibit, unique_name );
    
    if ( inhibitor )
    {
	XFPM_DEBUG ("Application=%s with unique connection name=%s disconnected", inhibitor->app_name, inhibitor->unique_name);
	xfpm_inhibit_free_inhibitor (inhibit, inhibitor);
	xfpm_inhibit_has_inhibit_changed (inhibit);
    }
}

static void
xfpm_inhibit_class_init(XfpmInhibitClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[HAS_INHIBIT_CHANGED] =
	    g_signal_new ("has-inhibit-changed",
			 XFPM_TYPE_INHIBIT,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmInhibitClass, has_inhibit_changed),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__BOOLEAN,
			 G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    object_class->finalize = xfpm_inhibit_finalize;

    g_type_class_add_private (klass, sizeof (XfpmInhibitPrivate));
    
    xfpm_inhibit_dbus_class_init (klass);
}

static void
xfpm_inhibit_init (XfpmInhibit *inhibit)
{
    inhibit->priv = XFPM_INHIBIT_GET_PRIVATE(inhibit);
    
    inhibit->priv->array   = g_ptr_array_new ();
    inhibit->priv->monitor = xfpm_dbus_monitor_new ();
    
    g_signal_connect (inhibit->priv->monitor, "unique-name-lost",
		      G_CALLBACK (xfpm_inhibit_connection_lost_cb), inhibit);
		      
    xfpm_inhibit_dbus_init (inhibit);
}

static void
xfpm_inhibit_finalize (GObject *object)
{
    XfpmInhibit *inhibit;
    Inhibitor *inhibitor;
    guint i;

    inhibit = XFPM_INHIBIT(object);
    
    g_object_unref (inhibit->priv->monitor);
    
    for ( i = 0; i<inhibit->priv->array->len; i++)
    {
	inhibitor = g_ptr_array_index (inhibit->priv->array, i);
	xfpm_inhibit_free_inhibitor (inhibit, inhibitor);
    }
    
    g_ptr_array_free (inhibit->priv->array, TRUE);

    G_OBJECT_CLASS(xfpm_inhibit_parent_class)->finalize(object);
}

XfpmInhibit *
xfpm_inhibit_new(void)
{
    static gpointer xfpm_inhibit_object = NULL;
    
    if ( G_LIKELY (xfpm_inhibit_object != NULL) )
    {
	g_object_ref (xfpm_inhibit_object);
    }
    else
    {
	xfpm_inhibit_object = g_object_new (XFPM_TYPE_INHIBIT, NULL);
	g_object_add_weak_pointer (xfpm_inhibit_object, &xfpm_inhibit_object);
    }
    return XFPM_INHIBIT (xfpm_inhibit_object);
}

/*
 * 
 * DBus server implementation for org.freedesktop.PowerManagement.Inhibit
 * 
 */
static void xfpm_inhibit_inhibit  	(XfpmInhibit *inhibit,
					 const gchar *IN_appname,
					 const gchar *IN_reason,
					 DBusGMethodInvocation *context);

static gboolean xfpm_inhibit_un_inhibit (XfpmInhibit *inhibit,
					 guint        IN_cookie,
					 GError     **error);

static gboolean xfpm_inhibit_has_inhibit(XfpmInhibit *inhibit,
					 gboolean    *OUT_has_inhibit,
					 GError     **error);

static gboolean xfpm_inhibit_get_inhibitors (XfpmInhibit *inhibit,
					     gchar ***OUT_inhibitor,
					     GError **error);

#include "org.freedesktop.PowerManagement.Inhibit.h"

static void xfpm_inhibit_dbus_class_init  (XfpmInhibitClass *klass)
{
    dbus_g_object_type_install_info(G_TYPE_FROM_CLASS(klass),
				    &dbus_glib_xfpm_inhibit_object_info);
				    
}

static void xfpm_inhibit_dbus_init	  (XfpmInhibit *inhibit)
{
    DBusGConnection *bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
    
    dbus_g_connection_register_g_object (bus,
					 "/org/freedesktop/PowerManagement/Inhibit",
					 G_OBJECT(inhibit));
}

static void xfpm_inhibit_inhibit  	(XfpmInhibit *inhibit,
					 const gchar *IN_appname,
					 const gchar *IN_reason,
					 DBusGMethodInvocation *context)
{
    GError *error = NULL;
    gchar *sender;
    guint cookie;
    
    if ( IN_appname == NULL || IN_reason == NULL )
    {
	g_set_error (&error, XFPM_ERROR, XFPM_ERROR_INVALID_ARGUMENTS, _("Invalid arguments"));
	dbus_g_method_return_error (context, error);
	return;
    }

    sender = dbus_g_method_get_sender (context);
    cookie = xfpm_inhibit_add_application (inhibit, IN_appname, sender);
     
    XFPM_DEBUG("Inhibit send application name=%s reason=%s sender=%s", IN_appname, IN_reason ,sender);
    
    xfpm_inhibit_has_inhibit_changed (inhibit);
    
    xfpm_dbus_monitor_add_unique_name (inhibit->priv->monitor, DBUS_BUS_SESSION, sender);
    
    g_free (sender);
    dbus_g_method_return (context, cookie);
}

static gboolean xfpm_inhibit_un_inhibit    (XfpmInhibit *inhibit,
					    guint        IN_cookie,
					    GError     **error)
{
    XFPM_DEBUG("UnHibit message received");
    
    if (!xfpm_inhibit_remove_application_by_cookie (inhibit, IN_cookie))
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_COOKIE_NOT_FOUND, _("Invalid cookie"));
	return FALSE;
    }
    
    xfpm_inhibit_has_inhibit_changed (inhibit);
   
    return TRUE;
}

static gboolean xfpm_inhibit_has_inhibit   (XfpmInhibit *inhibit,
					    gboolean    *OUT_has_inhibit,
					    GError     **error)
{
    XFPM_DEBUG("Has Inhibit message received");

    *OUT_has_inhibit = inhibit->priv->inhibited;

    return TRUE;
}

static gboolean xfpm_inhibit_get_inhibitors (XfpmInhibit *inhibit,
					     gchar ***OUT_inhibitors,
					     GError **error)
{
    guint i;
    Inhibitor *inhibitor;

    XFPM_DEBUG ("Get Inhibitors message received");
    
    *OUT_inhibitors = g_new (gchar *, inhibit->priv->array->len + 1);
    
    for ( i = 0; i<inhibit->priv->array->len; i++)
    {
	inhibitor = g_ptr_array_index (inhibit->priv->array, i);
	(*OUT_inhibitors)[i] = g_strdup (inhibitor->app_name);
    }
    
    (*OUT_inhibitors)[inhibit->priv->array->len] = NULL;
    
    return TRUE;
}
