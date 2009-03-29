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

#include "xfpm-inhibit.h"
#include "xfpm-screen-saver.h"
#include "xfpm-errors.h"

/* Init */
static void xfpm_inhibit_class_init (XfpmInhibitClass *klass);
static void xfpm_inhibit_init       (XfpmInhibit *inhibit);
static void xfpm_inhibit_finalize   (GObject *object);

static void xfpm_inhibit_dbus_class_init  (XfpmInhibitClass *klass);
static void xfpm_inhibit_dbus_init	  (XfpmInhibit *inhibit);

#define XFPM_INHIBIT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_INHIBIT, XfpmInhibitPrivate))

struct XfpmInhibitPrivate
{
    GHashTable      *hash;
    gboolean         inhibited;
};

enum
{
    HAS_INHIBIT_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gpointer xfpm_inhibit_object = NULL;

G_DEFINE_TYPE (XfpmInhibit, xfpm_inhibit, G_TYPE_OBJECT)

static void
xfpm_inhibit_class_init(XfpmInhibitClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[HAS_INHIBIT_CHANGED] =
	    g_signal_new("has-inhibit-changed",
			 XFPM_TYPE_INHIBIT,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmInhibitClass, has_inhibit_changed),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__BOOLEAN,
			 G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    object_class->finalize = xfpm_inhibit_finalize;

    g_type_class_add_private(klass,sizeof(XfpmInhibitPrivate));
    
    xfpm_inhibit_dbus_class_init (klass);
}

static void
xfpm_inhibit_init(XfpmInhibit *inhibit)
{
    inhibit->priv = XFPM_INHIBIT_GET_PRIVATE(inhibit);
    
    inhibit->priv->hash = g_hash_table_new_full (NULL, NULL, NULL, g_free);
    
    xfpm_inhibit_dbus_init (inhibit);
}

static void
xfpm_inhibit_finalize(GObject *object)
{
    XfpmInhibit *inhibit;

    inhibit = XFPM_INHIBIT(object);
    
    g_hash_table_destroy (inhibit->priv->hash);

    G_OBJECT_CLASS(xfpm_inhibit_parent_class)->finalize(object);
}

static guint
xfpm_inhibit_get_cookie (XfpmInhibit *inhibit)
{
    GList *list;
    guint cookie;
    guint max = 0;
    guint hash_cookie;
    gint i;
    
    list = g_hash_table_get_keys (inhibit->priv->hash);

    for ( i = 0; i < g_list_length (list); i++)
    {
	hash_cookie = GPOINTER_TO_INT ((gpointer) g_list_nth_data(list, i));
	max = MAX(max, hash_cookie);
    }
	
    if ( list )
	g_list_free (list);
	
    /*
     * Should work in all the cases as we will not have thounsands of applications inhibiting us
     */
    cookie = (guint) g_random_int_range ( max + 1, max + 40);
    
    return cookie;
}

static guint
xfpm_inhibit_add_application (XfpmInhibit *inhibit, const gchar *app_name)
{
    guint cookie = xfpm_inhibit_get_cookie (inhibit);
    g_hash_table_insert (inhibit->priv->hash, 
			 GINT_TO_POINTER(cookie),
			 g_strdup (app_name));
    return cookie;
}

static gboolean
xfpm_inhibit_remove_application (XfpmInhibit *inhibit, guint cookie)
{
    return g_hash_table_remove (inhibit->priv->hash, GINT_TO_POINTER(cookie));
}

XfpmInhibit *
xfpm_inhibit_new(void)
{
    if ( xfpm_inhibit_object != NULL )
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
static gboolean xfpm_inhibit_dbus_inhibit  	(XfpmInhibit *inhibit,
						 const gchar *IN_appname,
						 const gchar *IN_reason,
						 guint       *OUT_cookie,
						 GError     **error);

static gboolean xfpm_inhibit_dbus_un_inhibit    (XfpmInhibit *inhibit,
						 guint        IN_cookie,
						 GError     **error);

static gboolean xfpm_inhibit_dbus_has_inhibit   (XfpmInhibit *inhibit,
						 gboolean    *OUT_has_inhibit,
						 GError     **error);

#include "org.freedesktop.PowerManagement.Inhibit.h"

static void xfpm_inhibit_dbus_class_init  (XfpmInhibitClass *klass)
{
    dbus_g_object_type_install_info(G_TYPE_FROM_CLASS(klass),
				    &dbus_glib_xfpm_inhibit_object_info);
				    
    dbus_g_error_domain_register (XFPM_ERROR, 
				  "org.freedesktop.PowerManagement.Inhibit",
				  XFPM_TYPE_ERROR);
}

static void xfpm_inhibit_dbus_init	  (XfpmInhibit *inhibit)
{
    DBusGConnection *bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
    
    dbus_g_connection_register_g_object (bus,
					 "/org/freedesktop/PowerManagement/Inhibit",
					 G_OBJECT(inhibit));
}

static gboolean xfpm_inhibit_dbus_inhibit  	(XfpmInhibit *inhibit,
						 const gchar *IN_appname,
						 const gchar *IN_reason,
						 guint       *OUT_cookie,
						 GError     **error)
{
    guint cookie = xfpm_inhibit_add_application (inhibit, IN_appname);
    
    TRACE("Inhibit send application name=%s reason=%s", IN_appname, IN_reason);
    
    if ( !inhibit->priv->inhibited )
    {
	inhibit->priv->inhibited = TRUE;
	g_signal_emit (G_OBJECT(inhibit), signals[HAS_INHIBIT_CHANGED], 0, inhibit->priv->inhibited);
    }
    
    *OUT_cookie = cookie;
    
    return TRUE;
}

static gboolean xfpm_inhibit_dbus_un_inhibit    (XfpmInhibit *inhibit,
						 guint        IN_cookie,
						 GError     **error)
{
    TRACE("UnHibit message received");
    
    if (!xfpm_inhibit_remove_application (inhibit, IN_cookie))
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_INVALID_COOKIE, _("Invalid cookie"));
	return FALSE;
    }
    
    if ( g_hash_table_size (inhibit->priv->hash) == 0)
    {
	TRACE("Inhibit removed");
	inhibit->priv->inhibited = FALSE;
	g_signal_emit (G_OBJECT(inhibit), signals[HAS_INHIBIT_CHANGED], 0, inhibit->priv->inhibited);
    }
    
    return TRUE;
}

static gboolean xfpm_inhibit_dbus_has_inhibit   (XfpmInhibit *inhibit,
						 gboolean    *OUT_has_inhibit,
						 GError     **error)
{
    TRACE("Has Inhibit message received");
    
    *OUT_has_inhibit = inhibit->priv->inhibited;
    
    return TRUE;
}
