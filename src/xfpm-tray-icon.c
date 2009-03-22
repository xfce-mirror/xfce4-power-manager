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

#include <glib.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "libxfpm/xfpm-common.h"

#include "xfpm-tray-icon.h"
#include "xfpm-string.h"

/* Init */
static void xfpm_tray_icon_class_init (XfpmTrayIconClass *klass);
static void xfpm_tray_icon_init       (XfpmTrayIcon *tray);
static void xfpm_tray_icon_finalize   (GObject *object);

#define XFPM_TRAY_ICON_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_TRAY_ICON, XfpmTrayIconPrivate))

struct XfpmTrayIconPrivate
{
    GtkStatusIcon *icon;
    GQuark icon_quark;
};

G_DEFINE_TYPE(XfpmTrayIcon, xfpm_tray_icon, G_TYPE_OBJECT)

static gboolean
xfpm_tray_icon_size_changed_cb (GtkStatusIcon *icon, gint size, XfpmTrayIcon *tray)
{
    GdkPixbuf *pix;
    
    g_return_val_if_fail (size > 0, FALSE);

    if ( tray->priv->icon_quark == 0 )
	return FALSE;
    
    pix = xfce_themed_icon_load (g_quark_to_string (tray->priv->icon_quark), size);
    
    if ( pix )
    {
	gtk_status_icon_set_from_pixbuf (GTK_STATUS_ICON(tray->priv->icon), pix);
	g_object_unref (pix);
	return TRUE;
    }
    
    return FALSE;
}

static void
xfpm_tray_icon_class_init(XfpmTrayIconClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_tray_icon_finalize;

    g_type_class_add_private(klass,sizeof(XfpmTrayIconPrivate));
}

static void
xfpm_tray_icon_init(XfpmTrayIcon *tray)
{
    tray->priv = XFPM_TRAY_ICON_GET_PRIVATE(tray);
    
    tray->priv->icon = gtk_status_icon_new();
    
    tray->priv->icon_quark = 0;
    
    g_signal_connect (tray->priv->icon, "size-changed",
		      G_CALLBACK (xfpm_tray_icon_size_changed_cb), tray);
}

static void
xfpm_tray_icon_finalize(GObject *object)
{
    XfpmTrayIcon *icon;

    icon = XFPM_TRAY_ICON(object);

    if ( icon->priv->icon )
    	g_object_unref(icon->priv->icon);

    G_OBJECT_CLASS(xfpm_tray_icon_parent_class)->finalize(object);
}

XfpmTrayIcon *
xfpm_tray_icon_new(void)
{
    XfpmTrayIcon *tray = NULL;
    tray = g_object_new(XFPM_TYPE_TRAY_ICON,NULL);
    return tray;
}

void xfpm_tray_icon_set_icon (XfpmTrayIcon *icon, const gchar *icon_name)
{
    g_return_if_fail(XFPM_IS_TRAY_ICON(icon));
    
    icon->priv->icon_quark = g_quark_from_string(icon_name);
    
    xfpm_tray_icon_size_changed_cb (icon->priv->icon,
				    gtk_status_icon_get_size(icon->priv->icon),
				    icon);
}

void xfpm_tray_icon_set_tooltip (XfpmTrayIcon *icon, const gchar *tooltip)
{
    g_return_if_fail(XFPM_IS_TRAY_ICON(icon));

#if GTK_CHECK_VERSION (2, 16, 0)
    gtk_status_icon_set_tooltip_text (GTK_STATUS_ICON(icon->priv->icon), tooltip);
#else
    gtk_status_icon_set_tooltip (GTK_STATUS_ICON(icon->priv->icon), tooltip);
#endif
}

void xfpm_tray_icon_set_visible (XfpmTrayIcon *icon, gboolean visible)
{
    g_return_if_fail(XFPM_IS_TRAY_ICON(icon));
    
    gtk_status_icon_set_visible(GTK_STATUS_ICON(icon->priv->icon), visible);
}

gboolean xfpm_tray_icon_get_visible (XfpmTrayIcon *icon)
{
    g_return_val_if_fail (XFPM_IS_TRAY_ICON(icon), FALSE);
    
    return gtk_status_icon_get_visible (GTK_STATUS_ICON(icon->priv->icon));
}

GtkStatusIcon *xfpm_tray_icon_get_tray_icon (XfpmTrayIcon *icon)
{
    g_return_val_if_fail(XFPM_IS_TRAY_ICON(icon), NULL);
    
    return icon->priv->icon;
}

const gchar *xfpm_tray_icon_get_icon_name   (XfpmTrayIcon *icon)
{
    g_return_val_if_fail(XFPM_IS_TRAY_ICON(icon), NULL);
    
    if ( icon->priv->icon_quark == 0 ) return NULL;
    
    return  g_quark_to_string (icon->priv->icon_quark);
}
