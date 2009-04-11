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

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/hal-manager.h"
#include "libxfpm/hal-device.h"

#include "libxfpm/xfpm-string.h"

#include "xfpm-button-hal.h"
#include "xfpm-enum.h"
#include "xfpm-enum-types.h"

/* Init */
static void xfpm_button_hal_class_init (XfpmButtonHalClass *klass);
static void xfpm_button_hal_init       (XfpmButtonHal *button);
static void xfpm_button_hal_finalize   (GObject *object);

#define XFPM_BUTTON_HAL_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_BUTTON_HAL, XfpmButtonHalPrivate))

struct XfpmButtonHalPrivate
{
    GPtrArray  *array;
    guint8 	keys;
};

enum
{
    HAL_BUTTON_PRESSED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmButtonHal, xfpm_button_hal, G_TYPE_OBJECT)

static void
xfpm_button_hal_emit_signals (XfpmButtonHal *bt, const gchar *condition, const gchar *detail)
{
    if ( !xfpm_strequal (condition, "ButtonPressed") )
	return;

    if ( xfpm_strequal (detail, "power")  && !(bt->priv->keys & POWER_KEY) )
    {
	TRACE ("Emitting signal button press condition %s detail %s", condition, detail);
	g_signal_emit (G_OBJECT (bt), signals [HAL_BUTTON_PRESSED], 0, BUTTON_POWER_OFF);
    }
    else if ( xfpm_strequal (detail, "sleep")  && !(bt->priv->keys & SLEEP_KEY) )
    {
	TRACE ("Emitting signal button press condition %s detail %s", condition, detail);
	g_signal_emit (G_OBJECT (bt), signals [HAL_BUTTON_PRESSED], 0, BUTTON_SLEEP);
    }
    else if ( xfpm_strequal (detail, "hibernate") && !(bt->priv->keys & HIBERNATE_KEY) )
    {
	TRACE ("Emitting signal button press condition %s detail %s", condition, detail);
	g_signal_emit (G_OBJECT (bt), signals [HAL_BUTTON_PRESSED], 0, BUTTON_HIBERNATE);
    }
    else if ( xfpm_strequal (detail, "brightness-up")  && !(bt->priv->keys & BRIGHTNESS_KEY) )
    {
	TRACE ("Emitting signal button press condition %s detail %s", condition, detail);
	g_signal_emit (G_OBJECT (bt), signals [HAL_BUTTON_PRESSED], 0, BUTTON_MON_BRIGHTNESS_UP);
    }
    else if ( xfpm_strequal (detail, "brightness-down")  && !(bt->priv->keys & BRIGHTNESS_KEY) )
    {
	TRACE ("Emitting signal button press condition %s detail %s", condition, detail);
	g_signal_emit (G_OBJECT (bt), signals [HAL_BUTTON_PRESSED], 0, BUTTON_MON_BRIGHTNESS_DOWN);
    }
}

static void
xfpm_button_hal_device_changed_cb (HalDevice *device, const gchar *udi, const gchar *key,
				   gboolean is_added, gboolean is_removed, XfpmButtonHal *bt)
{
    gboolean pressed;
    gchar   *button_type;
    
    if ( !xfpm_strequal (key, "button.state.value") )
	return;
	
    if ( hal_device_has_key (device, "button.type") )
    {
	button_type = hal_device_get_property_string (device, "button.type");
	
	if ( button_type == NULL )
	    return;
	    
	if ( xfpm_strequal (button_type, "lid") )
	{
	    pressed = hal_device_get_property_bool (device, key);
    
	    if ( pressed )
	    {
		TRACE ("Emitting signal lid closed");
		g_signal_emit (G_OBJECT (bt), signals [HAL_BUTTON_PRESSED], 0, BUTTON_LID_CLOSED );
	    }
	}
	g_free (button_type);
    }
}

static void
xfpm_button_hal_condition_cb (HalDevice *device, const gchar *condition, 
			      const gchar *detail, XfpmButtonHal *bt)
{
    xfpm_button_hal_emit_signals (bt, condition, detail);
}

static void
xfpm_button_hal_add_button (XfpmButtonHal *bt, const gchar *udi, gboolean lid_only)
{
    HalDevice *device;
    gchar *button_type;
    
    device = hal_device_new ();
    
    hal_device_set_udi (device, udi);
   
    if ( lid_only == TRUE )
    {
	if ( hal_device_has_key (device, "button.type") == FALSE )
	{
	    g_object_unref (device);
	    return;
	}
	
	button_type = hal_device_get_property_string (device, "button.type");
	if ( button_type == NULL ) return;
	
	if ( xfpm_strequal (button_type, "lid") )
	{
	    g_free (button_type);
	    goto out;
	}
	else
	{
	    g_free (button_type);
	    g_object_unref (device);
	    return;
	}
    }
    
out:    
    g_signal_connect (device, "device-changed",
		      G_CALLBACK (xfpm_button_hal_device_changed_cb), bt);
		      
    g_signal_connect (device, "device-condition",
		      G_CALLBACK (xfpm_button_hal_condition_cb), bt);
		      
    hal_device_watch (device);
    hal_device_watch_condition (device);
   
    g_ptr_array_add (bt->priv->array, device);
}

static void
xfpm_button_hal_get_buttons (XfpmButtonHal *bt, gboolean lid_only)
{
    HalManager *manager;
    gchar     **udi;
    int 	i;
    
    manager = hal_manager_new ();
    
    udi = hal_manager_find_device_by_capability (manager, "button");
    
    g_object_unref (manager);
    
    if ( udi == NULL || udi[0] == NULL )
	return;
	
    for ( i = 0; udi[i]; i++)
    {
	xfpm_button_hal_add_button (bt, udi[i], lid_only);
    }
    hal_manager_free_string_array (udi);
}

static void
xfpm_button_hal_class_init (XfpmButtonHalClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals[HAL_BUTTON_PRESSED] = 
        g_signal_new("hal-button-pressed",
                      XFPM_TYPE_BUTTON_HAL,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmButtonHalClass, hal_button_pressed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__ENUM,
                      G_TYPE_NONE, 1, XFPM_TYPE_BUTTON_KEY);

    object_class->finalize = xfpm_button_hal_finalize;

    g_type_class_add_private (klass, sizeof (XfpmButtonHalPrivate));
}

static void
xfpm_button_hal_init (XfpmButtonHal *button)
{
    button->priv = XFPM_BUTTON_HAL_GET_PRIVATE (button);
    button->priv->array = g_ptr_array_new ();
    button->priv->keys  = 0;
}

static void
xfpm_button_hal_free_device_array (XfpmButtonHal *button)
{
    HalDevice *device;
    int i;
    
    for ( i = 0 ; i<button->priv->array->len; i++)
    {
	device = g_ptr_array_index (button->priv->array, i);
	g_object_unref (device);
    }
}

static void
xfpm_button_hal_finalize (GObject *object)
{
    XfpmButtonHal *button;

    button = XFPM_BUTTON_HAL (object);
    
    xfpm_button_hal_free_device_array (button);
    
    g_ptr_array_free (button->priv->array, TRUE);

    G_OBJECT_CLASS (xfpm_button_hal_parent_class)->finalize (object);
}

XfpmButtonHal *
xfpm_button_hal_new (void)
{
    XfpmButtonHal *button = NULL;
    button = g_object_new (XFPM_TYPE_BUTTON_HAL, NULL);
    return button;
}

void xfpm_button_hal_get_keys (XfpmButtonHal *button, gboolean lid_only, guint8 keys)
{
    g_return_if_fail (XFPM_IS_BUTTON_HAL (button));
    
    button->priv->keys = keys;
    xfpm_button_hal_get_buttons (button, lid_only);
}
