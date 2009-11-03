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

#include <dbus/dbus-glib.h>

#ifdef HAVE_POLKIT
#include <polkit/polkit.h>
#endif

#include "xfpm-polkit.h"

static void xfpm_polkit_finalize   (GObject *object);

#define XFPM_POLKIT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_POLKIT, XfpmPolkitPrivate))

struct XfpmPolkitPrivate
{
    DBusGConnection   *bus;
#ifdef HAVE_POLKIT
    PolkitAuthority   *authority;
#endif
};

enum
{
    AUTH_CHANGED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmPolkit, xfpm_polkit, G_TYPE_OBJECT)

static gboolean 
xfpm_polkit_check_auth_intern (XfpmPolkit *polkit, const gchar *action_id)
{
#ifdef HAVE_POLKIT
    PolkitSubject *subj;
    PolkitAuthorizationResult *res;
    GError *error = NULL;
    gboolean ret = FALSE;
    
    subj = polkit_unix_process_new (getpid ());
    
    res = polkit_authority_check_authorization_sync (polkit->priv->authority, 
						     subj, 
						     action_id,
						     NULL,
						     POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE,
						     NULL,
						     &error);
    
    if ( error )
    {
	g_warning ("Unable to get authorization result for action :%s : %s", action_id, error->message);
	g_error_free (error);
	goto out;
    }
    
    if (polkit_authorization_result_get_is_authorized (res))
    {
	ret = TRUE;
    }
    
out:
    if (res)
	g_object_unref (res);
	
    return ret;
    
#endif
    return TRUE;
}

#ifdef HAVE_POLKIT
static void
xfpm_polkit_auth_changed_cb (PolkitAuthority *authority, XfpmPolkit *polkit)
{
}
#endif

static void
xfpm_polkit_class_init (XfpmPolkitClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_polkit_finalize;

    signals [AUTH_CHANGED] = 
        g_signal_new ("auth-changed",
                      XFPM_TYPE_POLKIT,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPolkitClass, auth_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    g_type_class_add_private (klass, sizeof (XfpmPolkitPrivate));
}

static void
xfpm_polkit_init (XfpmPolkit *polkit)
{
    GError *error = NULL;
    
    polkit->priv = XFPM_POLKIT_GET_PRIVATE (polkit);
    
    polkit->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Error getting system bus connection : %s", error->message);
	g_error_free (error);
	goto out;
    }
#ifdef HAVE_POLKIT
    polkit->priv->authority = polkit_authority_get ();
    g_signal_connect (polkit->priv->authority, "changed",
		      G_CALLBACK (xfpm_polkit_auth_changed_cb), polkit);
#endif
    
    
out:
    ;
    
}

static void
xfpm_polkit_finalize (GObject *object)
{
    XfpmPolkit *polkit;

    polkit = XFPM_POLKIT (object);
#ifdef HAVE_POLKIT
    if (polkit->priv->authority )
	g_object_unref (polkit->priv->authority);
#endif

    if ( polkit->priv->bus )
	dbus_g_connection_unref (polkit->priv->bus);

    G_OBJECT_CLASS (xfpm_polkit_parent_class)->finalize (object);
}

XfpmPolkit *
xfpm_polkit_get (void)
{
    static gpointer xfpm_polkit_obj = NULL;
    
    if ( G_LIKELY (xfpm_polkit_obj) )
    {
	g_object_ref (xfpm_polkit_obj);
    }
    else
    {
	xfpm_polkit_obj = g_object_new (XFPM_TYPE_POLKIT, NULL);
    }
    
    return XFPM_POLKIT (xfpm_polkit_obj);
}

gboolean xfpm_polkit_check_auth	(XfpmPolkit *polkit, const gchar *action_id)
{
    return xfpm_polkit_check_auth_intern (polkit, action_id);
}
