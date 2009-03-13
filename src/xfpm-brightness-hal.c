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

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/xfpm-string.h"

#include "xfpm-brightness-hal.h"

/* Init */
static void xfpm_brightness_hal_class_init (XfpmBrightnessHalClass *klass);
static void xfpm_brightness_hal_init       (XfpmBrightnessHal *brg);
static void xfpm_brightness_hal_finalize   (GObject *object);

#define XFPM_BRIGHTNESS_HAL_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_BRIGHTNESS_HAL, XfpmBrightnessHalPrivate))

struct XfpmBrightnessHalPrivate
{
    //HalCtx   *ctx;
    gboolean  hw_found;
};

G_DEFINE_TYPE(XfpmBrightnessHal, xfpm_brightness_hal, G_TYPE_OBJECT)

static void
xfpm_brightness_hal_class_init(XfpmBrightnessHalClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_brightness_hal_finalize;


    g_type_class_add_private(klass,sizeof(XfpmBrightnessHalPrivate));
}

static void
xfpm_brightness_hal_init(XfpmBrightnessHal *brg)
{
    brg->priv = XFPM_BRIGHTNESS_HAL_GET_PRIVATE(brg);
    
    //brg->priv->ctx 	= NULL;
    brg->priv->hw_found = FALSE;
}

static void
xfpm_brightness_hal_finalize(GObject *object)
{
    XfpmBrightnessHal *brg;

    brg = XFPM_BRIGHTNESS_HAL(object);
    
    G_OBJECT_CLASS(xfpm_brightness_hal_parent_class)->finalize(object);
}

/*
static void
xfpm_brightness_hal_property_modified_cb (LibHalContext *ctx,
					  const gchar *udi,
					  const gchar *key, 
					  dbus_bool_t is_removed,
					  dbus_bool_t is_added)
{
    g_print("BRIGHTNESS: udi=%s key=%s is_removed=%s is_added=%s\n", 
	    udi, key, xfpm_bool_to_string(is_removed), xfpm_bool_to_string(is_added) );
    
}

static gboolean
xfpm_brightness_hal_setup (XfpmBrightnessHal *brg)
{
    brg->priv->ctx = hal_ctx_new ();
    
    if ( !hal_ctx_connect(brg->priv->ctx) )
    	return FALSE;

    gchar **udi = NULL;
    gint num = 0;
    
    udi = hal_ctx_get_device_by_capability (brg->priv->ctx, "laptop_panel", &num );
    
    if ( !udi )
    	return FALSE;
	
    g_return_val_if_fail (num == 1, FALSE);
    
    hal_ctx_set_device_property_callback (brg->priv->ctx, xfpm_brightness_hal_property_modified_cb);
    hal_ctx_set_user_data (brg->priv->ctx, brg);
    
    if ( hal_ctx_device_has_key (brg->priv->ctx, udi[0], "laptop_panel.num_levels") )
    {
	TRACE ("Found laptop_panel with udi=%s\n", udi[0]);
	brg->priv->hw_found = TRUE;
	hal_ctx_watch_device (brg->priv->ctx, udi[0] );
    }
		    
    libhal_free_string_array (udi);
    
    return TRUE;
}
*/
XfpmBrightnessHal *
xfpm_brightness_hal_new(void)
{
    XfpmBrightnessHal *brg = NULL;
    brg = g_object_new (XFPM_TYPE_BRIGHTNESS_HAL, NULL);
    
    //xfpm_brightness_hal_setup (brg);
    
    return brg;
}
