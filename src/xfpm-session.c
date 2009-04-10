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
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfpm-session.h"

/* Init */
static void xfpm_session_class_init (XfpmSessionClass *klass);
static void xfpm_session_init       (XfpmSession *session);
static void xfpm_session_finalize   (GObject *object);

#define XFPM_SESSION_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_SESSION, XfpmSessionPrivate))

struct XfpmSessionPrivate
{
    SessionClient *client;
    gboolean       managed;
};

enum
{
    SESSION_DIE,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static gpointer xfpm_session_object = NULL;

G_DEFINE_TYPE (XfpmSession, xfpm_session, G_TYPE_OBJECT)

static void
xfpm_session_die (gpointer client_data)
{
    XfpmSession *session;
    
    if ( G_UNLIKELY (xfpm_session_object == NULL ) )
	return;
	
    session = XFPM_SESSION (xfpm_session_object);
    if ( G_UNLIKELY (session->priv->managed == FALSE) )
	return;
	
    TRACE ("Session disconnected signal\n");
    g_signal_emit (G_OBJECT (session), signals [SESSION_DIE], 0);
}

static void
xfpm_session_class_init (XfpmSessionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals[SESSION_DIE] = 
        g_signal_new("session-die",
                      XFPM_TYPE_SESSION,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmSessionClass, session_die),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    object_class->finalize = xfpm_session_finalize;

    g_type_class_add_private (klass, sizeof (XfpmSessionPrivate));
}

static void
xfpm_session_init (XfpmSession *session)
{
    gchar **restart_command;
    
    session->priv = XFPM_SESSION_GET_PRIVATE (session);
    
    session->priv->client = NULL;
    
    restart_command    = g_new (gchar *, 3);
    restart_command[0] = g_strdup ("xfce4-power-manager");
    restart_command[1] = g_strdup ("--restart");
    restart_command[2] = NULL;
    
    session->priv->client = client_session_new_full (NULL,
						     SESSION_RESTART_IMMEDIATELY,
						     40,
						     NULL,
						     (gchar *) PACKAGE_NAME,
						     NULL,
						     restart_command,
						     g_strdupv (restart_command),
						     NULL,
						     NULL,
						     NULL);
    if ( G_UNLIKELY (session->priv->client == NULL ) )
    {
	g_warning ("Failed to connect to session manager");
	return;
    }
    
    session->priv->managed 	   = session_init (session->priv->client);
    session->priv->client->die     = xfpm_session_die;
}

static void
xfpm_session_finalize (GObject *object)
{
    XfpmSession *session;

    session = XFPM_SESSION (object);
    
    if ( session->priv->client != NULL )
	client_session_free (session->priv->client);

    G_OBJECT_CLASS (xfpm_session_parent_class)->finalize (object);
}

XfpmSession *
xfpm_session_new (void)
{
    if ( xfpm_session_object != NULL )
    {
	g_object_ref (xfpm_session_object);
    }
    else
    {
	xfpm_session_object = g_object_new (XFPM_TYPE_SESSION, NULL);
	g_object_add_weak_pointer (xfpm_session_object, &xfpm_session_object);
    }
    return XFPM_SESSION (xfpm_session_object);
}

void xfpm_session_set_client_id (XfpmSession *session, const gchar *client_id)
{
    g_return_if_fail (XFPM_IS_SESSION (session));
    
    if ( G_UNLIKELY (session->priv->client == NULL) )
	return;
    
    client_session_set_client_id (session->priv->client, client_id);
}
