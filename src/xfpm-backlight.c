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

#include <libxfce4util/libxfce4util.h>

#include "xfpm-backlight.h"
#include "xfpm-brightness-hal.h"
#include "xfpm-brightness-widget.h"

static void xfpm_backlight_finalize   (GObject *object);

static void xfpm_backlight_dbus_class_init (XfpmBacklightClass *klass);
static void xfpm_backlight_dbus_init       (XfpmBacklight *bk);

#define XFPM_BACKLIGHT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_BACKLIGHT, XfpmBacklightPrivate))

struct XfpmBacklightPrivate
{
    XfpmBrightnessHal    *br;
    XfpmBrightnessWidget *widget;
    
    gboolean has_hw;
};

G_DEFINE_TYPE(XfpmBacklight, xfpm_backlight, G_TYPE_OBJECT)

static void
xfpm_backlight_brightness_up (XfpmBrightnessHal *brg, guint level, XfpmBacklight *bk)
{
    xfpm_brightness_widget_set_level (bk->priv->widget, level);
}

static void
xfpm_backlight_brightness_down (XfpmBrightnessHal *brg, guint level, XfpmBacklight *bk)
{
    xfpm_brightness_widget_set_level (bk->priv->widget, level);
}

static void
xfpm_backlight_get_device (XfpmBacklight *bk)
{
    guint max_level;
    bk->priv->br     = xfpm_brightness_hal_new ();
    bk->priv->has_hw = xfpm_brightness_hal_has_hw (bk->priv->br);
    
    if ( bk->priv->has_hw == FALSE )
	g_object_unref (bk->priv->br);
    else
    {
	bk->priv->widget = xfpm_brightness_widget_new ();
	g_signal_connect (G_OBJECT(bk->priv->br), "brigthness-up",
			  G_CALLBACK (xfpm_backlight_brightness_up), bk);
			  
	g_signal_connect (G_OBJECT(bk->priv->br), "brigthness-down",
			  G_CALLBACK (xfpm_backlight_brightness_down), bk);
	
	max_level = xfpm_brightness_hal_get_max_level (bk->priv->br);
	xfpm_brightness_widget_set_max_level (bk->priv->widget,
					      max_level);
    }
    
}

static void
xfpm_backlight_class_init(XfpmBacklightClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_backlight_finalize;

    g_type_class_add_private(klass,sizeof(XfpmBacklightPrivate));
    
    xfpm_backlight_dbus_class_init (klass);
}

static void
xfpm_backlight_init(XfpmBacklight *bk)
{
    bk->priv = XFPM_BACKLIGHT_GET_PRIVATE(bk);
    
    xfpm_backlight_get_device (bk);
    
    xfpm_backlight_dbus_init (bk);
}

static void
xfpm_backlight_finalize(GObject *object)
{
    XfpmBacklight *bk;

    bk = XFPM_BACKLIGHT(object);
    
    if ( bk->priv->has_hw == TRUE )
    {
	g_object_unref (bk->priv->br);
	g_object_unref (bk->priv->widget);
    }
    
    G_OBJECT_CLASS(xfpm_backlight_parent_class)->finalize(object);
}

XfpmBacklight *
xfpm_backlight_new(void)
{
    XfpmBacklight *bk = NULL;
    bk = g_object_new (XFPM_TYPE_BACKLIGHT, NULL);
    return bk;
}

/*
 * 
 * DBus server implementation for org.freedesktop.PowerManagement.Backlight (Not standard) 
 *
 */

static gboolean xfpm_backlight_dbus_update_brightness (XfpmBacklight *bk,
						       guint IN_level,
						       GError **error);

#include "org.freedesktop.PowerManagement.Backlight.h"

static void xfpm_backlight_dbus_class_init  (XfpmBacklightClass *klass)
{
    dbus_g_object_type_install_info (G_TYPE_FROM_CLASS(klass),
				     &dbus_glib_xfpm_backlight_object_info);
}

static void xfpm_backlight_dbus_init	  (XfpmBacklight *bk)
{
    DBusGConnection *bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
    
    dbus_g_connection_register_g_object (bus,
					 "/org/freedesktop/PowerManagement/Backlight",
					 G_OBJECT(bk));
}


static gboolean xfpm_backlight_dbus_update_brightness (XfpmBacklight *bk,
						       guint IN_level,
						       GError **error)
{
    TRACE("Update backlight message received");
    if ( bk->priv->has_hw )
	xfpm_brightness_hal_update_level (bk->priv->br, IN_level);
    
    return TRUE;
}

gboolean xfpm_backlight_has_hw (XfpmBacklight *bk)
{
    g_return_val_if_fail (XFPM_IS_BACKLIGHT (bk), FALSE);
    
    return bk->priv->has_hw;
}

void xfpm_backlight_reload (XfpmBacklight *bk)
{
    g_return_if_fail (XFPM_IS_BACKLIGHT (bk));
    
    if ( bk->priv->has_hw == TRUE )
    {
	g_object_unref (bk->priv->br);
	g_object_unref (bk->priv->widget);
    }
    
    xfpm_backlight_get_device (bk);
}
