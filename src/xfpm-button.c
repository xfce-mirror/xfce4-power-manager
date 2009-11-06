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


/*
 * Based on code from gpm-button (gnome power manager)
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
 * 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/X.h>
#include <X11/XF86keysym.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <glib.h>

#include <libxfce4util/libxfce4util.h>

#include "xfpm-button.h"
#include "xfpm-enum.h"
#include "xfpm-enum-types.h"
#include "xfpm-debug.h"

#ifdef WITH_HAL
#include "libhal/hal-manager.h"
#include "libhal/hal-device.h"
#endif

static void xfpm_button_finalize   (GObject *object);

#define XFPM_BUTTON_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_BUTTON, XfpmButtonPrivate))

static struct
{
    XfpmButtonKey    key;
    guint            key_code;
} xfpm_key_map [NUMBER_OF_BUTTONS] = { {0, 0}, };

struct XfpmButtonPrivate
{
    GdkScreen	*screen;
    GdkWindow   *window;
    
    guint8       mapped_buttons;
#ifdef WITH_HAL
    GPtrArray  *array;
#endif
};

enum
{
    BUTTON_PRESSED,
    LAST_SIGNAL
};

#define DUPLICATE_SHUTDOWN_TIMEOUT 4.0f

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmButton, xfpm_button, G_TYPE_OBJECT)

static guint
xfpm_button_get_key (unsigned int keycode)
{
    XfpmButtonKey key = BUTTON_UNKNOWN;
    guint i;
    
    for ( i = 0; i < G_N_ELEMENTS (xfpm_key_map); i++)
    {
	if ( xfpm_key_map [i].key_code == keycode )
	    key = xfpm_key_map [i].key;
    }
    
    return key;
}

static GdkFilterReturn
xfpm_button_filter_x_events (GdkXEvent *xevent, GdkEvent *ev, gpointer data)
{
    XfpmButtonKey key;
    XfpmButton *button;
    
    XEvent *xev = (XEvent *) xevent;
    
    if ( xev->type != KeyPress )
    	return GDK_FILTER_CONTINUE;
    
    key = xfpm_button_get_key (xev->xkey.keycode);
    
    if ( key != BUTTON_UNKNOWN )
    {
	button = (XfpmButton *) data;
    
	XFPM_DEBUG_ENUM ("Key press", key, XFPM_TYPE_BUTTON_KEY);
    
	g_signal_emit (G_OBJECT(button), signals[BUTTON_PRESSED], 0, key);
	return GDK_FILTER_REMOVE;
    }
    
    return GDK_FILTER_CONTINUE;
}

static gboolean
xfpm_button_grab_keystring (XfpmButton *button, guint keycode)
{
    Display *display;
    guint ret;
    guint modmask = 0;
    
    display = GDK_DISPLAY ();
    
    gdk_error_trap_push ();

    ret = XGrabKey (display, keycode, modmask,
		    GDK_WINDOW_XID (button->priv->window), True,
		    GrabModeAsync, GrabModeAsync);
		    
    if ( ret == BadAccess )
    {
	g_warning ("Failed to grab modmask=%u, keycode=%li",
		    modmask, (long int) keycode);
	return FALSE;
    }
	
    ret = XGrabKey (display, keycode, LockMask | modmask,
		    GDK_WINDOW_XID (button->priv->window), True,
		    GrabModeAsync, GrabModeAsync);
			
    if (ret == BadAccess)
    {
	g_warning ("Failed to grab modmask=%u, keycode=%li",
		   LockMask | modmask, (long int) keycode);
	return FALSE;
    }

    gdk_flush ();
    gdk_error_trap_pop ();
    return TRUE;
}


static gboolean
xfpm_button_xevent_key (XfpmButton *button, guint keysym , XfpmButtonKey key)
{
    guint keycode = XKeysymToKeycode (GDK_DISPLAY(), keysym);

    if ( keycode == 0 )
    {
	g_warning ("could not map keysym %x to keycode\n", keysym);
	return FALSE;
    }
    
    if ( !xfpm_button_grab_keystring(button, keycode)) 
    {
    	g_warning ("Failed to grab %i\n", keycode);
	return FALSE;
    }
    
    XFPM_DEBUG_ENUM_FULL (key, XFPM_TYPE_BUTTON_KEY, "Grabbed key %li ", (long int) keycode);
    
    xfpm_key_map [key].key_code = keycode;
    xfpm_key_map [key].key = key;
    
    return TRUE;
}

static void
xfpm_button_setup (XfpmButton *button)
{
    button->priv->screen = gdk_screen_get_default ();
    button->priv->window = gdk_screen_get_root_window (button->priv->screen);
    
    if ( xfpm_button_xevent_key (button, XF86XK_PowerOff, BUTTON_POWER_OFF) )
	button->priv->mapped_buttons |= POWER_KEY;
    
#ifdef HAVE_XF86XK_HIBERNATE
    if ( xfpm_button_xevent_key (button, XF86XK_Hibernate, BUTTON_HIBERNATE) )
	button->priv->mapped_buttons |= HIBERNATE_KEY;
#endif 

#ifdef HAVE_XF86XK_SUSPEND
    if ( xfpm_button_xevent_key (button, XF86XK_Suspend, BUTTON_HIBERNATE) )
	button->priv->mapped_buttons |= HIBERNATE_KEY;
#endif 

    if ( xfpm_button_xevent_key (button, XF86XK_Sleep, BUTTON_SLEEP) )
	button->priv->mapped_buttons |= SLEEP_KEY;
	
    if ( xfpm_button_xevent_key (button, XF86XK_MonBrightnessUp, BUTTON_MON_BRIGHTNESS_UP) )
	button->priv->mapped_buttons |= BRIGHTNESS_KEY_UP;
	
    if (xfpm_button_xevent_key (button, XF86XK_MonBrightnessDown, BUTTON_MON_BRIGHTNESS_DOWN) )
	button->priv->mapped_buttons |= BRIGHTNESS_KEY_DOWN;

    gdk_window_add_filter (button->priv->window, 
			   xfpm_button_filter_x_events, button);
}

#ifdef WITH_HAL
static void
xfpm_button_hal_emit_signals (XfpmButton *button, const gchar *condition, const gchar *detail)
{
    if ( g_strcmp0 (condition, "ButtonPressed") )
	return;

    TRACE ("Button press condition %s detail %s", condition, detail);

    if ( !g_strcmp0 (detail, "power") )
	g_signal_emit (G_OBJECT (button), signals [BUTTON_PRESSED], 0, BUTTON_POWER_OFF);
    else if ( !g_strcmp0 (detail, "sleep")  || !g_strcmp0 (detail, "suspend") )
	g_signal_emit (G_OBJECT (button), signals [BUTTON_PRESSED], 0, BUTTON_SLEEP);
    else if ( !g_strcmp0 (detail, "hibernate"))
	g_signal_emit (G_OBJECT (button), signals [BUTTON_PRESSED], 0, BUTTON_HIBERNATE);
    else if ( !g_strcmp0 (detail, "brightness-up") )
	g_signal_emit (G_OBJECT (button), signals [BUTTON_PRESSED], 0, BUTTON_MON_BRIGHTNESS_UP);
    else if ( !g_strcmp0 (detail, "brightness-down") )
	g_signal_emit (G_OBJECT (button), signals [BUTTON_PRESSED], 0, BUTTON_MON_BRIGHTNESS_DOWN);
}

static void
xfpm_button_hal_condition_cb (HalDevice *device, const gchar *condition, 
			      const gchar *detail, XfpmButton *button)
{
    xfpm_button_hal_emit_signals (button, condition, detail);
}

static void
xfpm_button_add_button_hal (XfpmButton *button, const gchar *udi)
{
    HalDevice *device;
    gchar *button_type = NULL;
    
    device = hal_device_new ();
    
    hal_device_set_udi (device, udi);
   
    if ( hal_device_has_key (device, "button.type") )
    {
	button_type = hal_device_get_property_string (device, "button.type");
	
	if ( button_type == NULL ) 
	{
	    g_object_unref (device);
	    return;
	}
	
	if ( !g_strcmp0 (button_type, "sleep") && !(button->priv->mapped_buttons & SLEEP_KEY))
	    button->priv->mapped_buttons |= SLEEP_KEY;
	else if ( !g_strcmp0 (button_type, "suspend") && !(button->priv->mapped_buttons & SLEEP_KEY))
	    button->priv->mapped_buttons |= SLEEP_KEY;
	else if ( !g_strcmp0 (button_type, "hibernate") && !(button->priv->mapped_buttons & HIBERNATE_KEY))
	    button->priv->mapped_buttons |= HIBERNATE_KEY;
	else if ( !g_strcmp0 (button_type, "power") && !(button->priv->mapped_buttons & POWER_KEY))
	    button->priv->mapped_buttons |= POWER_KEY;
	else if ( !g_strcmp0 (button_type, "brightness-up") && !(button->priv->mapped_buttons & BRIGHTNESS_KEY_UP))
	    button->priv->mapped_buttons |= BRIGHTNESS_KEY_UP;
	else if ( !g_strcmp0 (button_type, "brightness-down") && !(button->priv->mapped_buttons & BRIGHTNESS_KEY_DOWN))
	    button->priv->mapped_buttons |= BRIGHTNESS_KEY_DOWN;
	else
	{
	    g_object_unref (device);
	    if ( button_type )
		g_free (button_type);
	    return;
	}
	
	if ( button_type )
		g_free (button_type);
    }
    else
    {
	g_object_unref (device);
	return;
    }
    
    g_signal_connect (device, "device-condition",
		      G_CALLBACK (xfpm_button_hal_condition_cb), button);
		      
    hal_device_watch_condition (device);
   
    if ( button->priv->array == NULL )
    {
	button->priv->array = g_ptr_array_new ();
    }
    g_ptr_array_add (button->priv->array, device);

}

static void
xfpm_button_setup_failed_hal (XfpmButton *button)
{
    HalManager *manager;
    gchar     **udi;
    int 	i;
    
    g_debug ("Getting missing buttons from HAL");
    
    manager = hal_manager_new ();
    
    udi = hal_manager_find_device_by_capability (manager, "button");
    
    g_object_unref (manager);
    
    if ( udi == NULL || udi[0] == NULL )
	return;
	
    for ( i = 0; udi[i]; i++)
    {
	xfpm_button_add_button_hal (button, udi[i]);
    }
    
    hal_manager_free_string_array (udi);
    if ( button->priv->array )
	g_debug ("Mapped HAL buttons : %u", button->priv->array->len);
}
#endif /* WITH_HAL*/

static void
xfpm_button_class_init(XfpmButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals [BUTTON_PRESSED] = 
        g_signal_new ("button-pressed",
                      XFPM_TYPE_BUTTON,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XfpmButtonClass, button_pressed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__ENUM,
                      G_TYPE_NONE, 1, XFPM_TYPE_BUTTON_KEY);

    object_class->finalize = xfpm_button_finalize;

    g_type_class_add_private (klass, sizeof (XfpmButtonPrivate));
}

static void
xfpm_button_init (XfpmButton *button)
{
    button->priv = XFPM_BUTTON_GET_PRIVATE (button);
    
    button->priv->mapped_buttons = 0;
    button->priv->screen = NULL;
    button->priv->window = NULL;
    
    xfpm_button_setup (button);
#ifdef WITH_HAL
    if ( !(button->priv->mapped_buttons & BRIGHTNESS_KEY_DOWN ) ||
         !(button->priv->mapped_buttons & BRIGHTNESS_KEY_UP )   ||
	 !(button->priv->mapped_buttons & SLEEP_KEY )   ||
	 !(button->priv->mapped_buttons & HIBERNATE_KEY)   ||
	 !(button->priv->mapped_buttons & POWER_KEY )  )
	xfpm_button_setup_failed_hal (button);
#endif
    
}

#ifdef WITH_HAL
static void
xfpm_button_free_device_array (XfpmButton *button)
{
    HalDevice *device;
    guint i;
    
    for ( i = 0 ; i<button->priv->array->len; i++)
    {
	device = g_ptr_array_index (button->priv->array, i);
	g_object_unref (device);
    }
}
#endif

static void
xfpm_button_finalize (GObject *object)
{
    XfpmButton *button;

    button = XFPM_BUTTON (object);
    
#ifdef WITH_HAL
    if ( button->priv->array )
    {
	xfpm_button_free_device_array (button);
	g_ptr_array_free (button->priv->array, TRUE);
    }
#endif
    
    G_OBJECT_CLASS(xfpm_button_parent_class)->finalize(object);
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
    g_return_val_if_fail (XFPM_IS_BUTTON (button), 0);
    
    return button->priv->mapped_buttons;
}
