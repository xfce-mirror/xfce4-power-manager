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

#include "xfpm-button.h"
#include "xfpm-button-xf86.h"
#include "xfpm-button-hal.h"
#include "xfpm-shutdown.h"
#include "xfpm-enum.h"
#include "xfpm-enum-types.h"

static void xfpm_button_finalize   (GObject *object);

#define XFPM_BUTTON_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_BUTTON, XfpmButtonPrivate))

#define SLEEP_KEY_TIMEOUT 6.0f

struct XfpmButtonPrivate
{
    XfpmButtonXf86 *xf86;
    XfpmButtonHal  *hal;
    XfpmShutdown   *shutdown;
    GTimer         *timer;
};

enum
{
    BUTTON_PRESSED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmButton, xfpm_button, G_TYPE_OBJECT)

static void
xfpm_button_xf86_emit_signal (XfpmButton *button, XfpmButtonKey key)
{
    if ( key == BUTTON_LID_CLOSED || key == BUTTON_POWER_OFF || key == BUTTON_SLEEP || key == BUTTON_HIBERNATE )
    {
	if ( g_timer_elapsed (button->priv->timer, NULL) > SLEEP_KEY_TIMEOUT )
	{
	    g_signal_emit (G_OBJECT (button), signals [BUTTON_PRESSED], 0, key);
	    g_timer_reset (button->priv->timer);
	}
    }
    else
    {
	g_signal_emit (G_OBJECT (button), signals [BUTTON_PRESSED], 0, key);
    }
}

static void
xfpm_button_xf86_button_pressed_cb (XfpmButtonXf86 *xf86, XfpmButtonKey key, XfpmButton *button)
{
    xfpm_button_xf86_emit_signal (button, key);
}

static void
xfpm_button_hal_button_pressed_cb (XfpmButtonHal *hal, XfpmButtonKey key, XfpmButton *button)
{
    xfpm_button_xf86_emit_signal (button, key);
}

static void
xfpm_button_waking_up_cb (XfpmShutdown *shutdown, XfpmButton *button)
{
    g_timer_reset (button->priv->timer);
}

static void
xfpm_button_class_init (XfpmButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals[BUTTON_PRESSED] = 
        g_signal_new("button-pressed",
                      XFPM_TYPE_BUTTON,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmButtonClass, button_pressed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__ENUM,
                      G_TYPE_NONE, 1, XFPM_TYPE_BUTTON_KEY);

    object_class->finalize = xfpm_button_finalize;
    g_type_class_add_private (klass, sizeof (XfpmButtonPrivate));
}

static void
xfpm_button_init (XfpmButton *button)
{
    guint8 xf86_mapped;
    gboolean only_lid = FALSE;
    
    button->priv = XFPM_BUTTON_GET_PRIVATE (button);
    button->priv->xf86 = xfpm_button_xf86_new ();
    button->priv->timer = g_timer_new ();
    button->priv->shutdown = xfpm_shutdown_new ();
    
    xf86_mapped = xfpm_button_xf86_get_mapped_buttons (button->priv->xf86);

    button->priv->hal = xfpm_button_hal_new ();
    
    if ( xf86_mapped & SLEEP_KEY && xf86_mapped & POWER_KEY && 
	 xf86_mapped & BRIGHTNESS_KEY && xf86_mapped & HIBERNATE_KEY )
	only_lid = TRUE;

    xfpm_button_hal_get_keys (button->priv->hal, only_lid, xf86_mapped);
    
    g_signal_connect (button->priv->xf86, "xf86-button-pressed",
		      G_CALLBACK (xfpm_button_xf86_button_pressed_cb), button);
		      
    g_signal_connect (button->priv->hal, "hal-button-pressed",
		      G_CALLBACK (xfpm_button_hal_button_pressed_cb), button);
		      
    g_signal_connect (button->priv->shutdown, "waking-up",
		      G_CALLBACK (xfpm_button_waking_up_cb), button);
}

static void
xfpm_button_finalize (GObject *object)
{
    XfpmButton *button;

    button = XFPM_BUTTON (object);
    
    g_object_unref (button->priv->hal);
    g_object_unref (button->priv->xf86);
    g_object_unref (button->priv->shutdown);
    
    g_timer_destroy (button->priv->timer);

    G_OBJECT_CLASS (xfpm_button_parent_class)->finalize (object);
}

XfpmButton *
xfpm_button_new (void)
{
    static gpointer xfpm_button_object = NULL;
    
    if ( G_LIKELY (xfpm_button_object != NULL) )
    {
	g_object_ref (xfpm_button_object);
    }
    else
    {
	xfpm_button_object = g_object_new (XFPM_TYPE_BUTTON, NULL);
	g_object_add_weak_pointer (xfpm_button_object, &xfpm_button_object);
    }
    return XFPM_BUTTON (xfpm_button_object);
}

guint8 xfpm_button_get_mapped (XfpmButton *button)
{
    guint8 mapped_keys = 0;
    guint8 hal_mapped;
    guint8 xf86_mapped;
    
    g_return_val_if_fail (XFPM_IS_BUTTON (button), 0);
    
    hal_mapped  = xfpm_button_hal_get_mapped_keys (button->priv->hal);
    xf86_mapped = xfpm_button_xf86_get_mapped_buttons (button->priv->xf86);
    
    mapped_keys = hal_mapped | xf86_mapped;
    
    return mapped_keys;
}
