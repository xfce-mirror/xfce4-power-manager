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
#include "xfpm-inhibit.h"
#include "xfpm-dbus-monitor.h"

static void xfpm_screen_saver_finalize   (GObject *object);

#define XFPM_SCREEN_SAVER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_SCREEN_SAVER, XfpmScreenSaverPrivate))

struct XfpmScreenSaverPrivate
{
    DBusGConnection *bus;
    XfpmInhibit     *inhibit;
    
    gboolean         inhibited;
};

enum
{
    SCREEN_SAVER_INHIBITED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmScreenSaver, xfpm_screen_saver, G_TYPE_OBJECT)

static void
xfpm_screen_saver_inhibit_changed_cb (XfpmInhibit *inhbit, gboolean inhibited, XfpmScreenSaver *srv)
{
    g_signal_emit (G_OBJECT (srv), signals [SCREEN_SAVER_INHIBITED], 0, inhibited);
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
    srv->priv = XFPM_SCREEN_SAVER_GET_PRIVATE(srv);
    
    srv->priv->inhibit = xfpm_inhibit_new ();
    g_signal_connect (srv->priv->inhibit, "has-inhibit-changed",
		      G_CALLBACK (xfpm_screen_saver_inhibit_changed_cb), srv);
}

static void
xfpm_screen_saver_finalize(GObject *object)
{
    XfpmScreenSaver *srv;

    srv = XFPM_SCREEN_SAVER(object);
    
    g_object_unref (srv->priv->inhibit);
    
    G_OBJECT_CLASS(xfpm_screen_saver_parent_class)->finalize(object);
}

XfpmScreenSaver *
xfpm_screen_saver_new(void)
{
    static gpointer xfpm_screen_saver_object = NULL;
    
    if ( G_LIKELY (xfpm_screen_saver_object != NULL) )
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

void xfpm_screen_saver_inhibit (XfpmScreenSaver *srv)
{
    g_return_if_fail (XFPM_IS_SCREEN_SAVER (srv));
    
    if ( srv->priv->inhibited == FALSE )
    {
	srv->priv->inhibited = TRUE;
	g_signal_emit (G_OBJECT (srv), signals[SCREEN_SAVER_INHIBITED], 0, srv->priv->inhibited);
    }
}

void xfpm_screen_saver_uninhibit (XfpmScreenSaver *srv)
{
    g_return_if_fail (XFPM_IS_SCREEN_SAVER (srv));
    
    if ( srv->priv->inhibited == TRUE )
    {
	srv->priv->inhibited = FALSE;
	g_signal_emit (G_OBJECT (srv), signals[SCREEN_SAVER_INHIBITED], 0, srv->priv->inhibited);
    }
}

gboolean xfpm_screen_saver_get_inhibit (XfpmScreenSaver *srv)
{
    g_return_val_if_fail (XFPM_IS_SCREEN_SAVER (srv), FALSE);
    
    return srv->priv->inhibited;
}
