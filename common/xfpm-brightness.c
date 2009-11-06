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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <libxfce4util/libxfce4util.h>

#ifdef WITH_HAL
#include <dbus/dbus-glib.h>
#include "libhal/hal-manager.h"
#include "libhal/hal-device.h"
#endif

#include "xfpm-brightness.h"
#include "xfpm-debug.h"

static void xfpm_brightness_finalize   (GObject *object);

#define XFPM_BRIGHTNESS_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_BRIGHTNESS, XfpmBrightnessPrivate))

struct XfpmBrightnessPrivate
{
    XRRScreenResources *resource;
    Atom		backlight;
    gint 		output;
    gboolean		xrandr_has_hw;
    
    guint		max_level;
    guint		current_level;
    guint		min_level;
    guint		step;
    
#ifdef WITH_HAL
    DBusGProxy         *hal_proxy;
    gboolean		hal_brightness_in_hw;
    gboolean		hal_hw_found;
#endif
};

G_DEFINE_TYPE (XfpmBrightness, xfpm_brightness, G_TYPE_OBJECT)

static gboolean
xfpm_brightness_xrand_get_limit (XfpmBrightness *brightness, RROutput output, guint *min, guint *max)
{
    XRRPropertyInfo *info;
    gboolean ret = TRUE;

    info = XRRQueryOutputProperty (GDK_DISPLAY (), output, brightness->priv->backlight);
    
    if (info == NULL) 
    {
	g_warning ("could not get output property");
	return FALSE;
    }
    
    if (!info->range || info->num_values != 2) 
    {
	g_warning ("no range found");
	ret = FALSE;
	goto out;
    }
    
    *min = info->values[0];
    *max = info->values[1];
    
out:
    XFree (info);
    return ret;
}

static gboolean
xfpm_brightness_xrandr_get_level (XfpmBrightness *brightness, RROutput output, guint *current)
{
    unsigned long nitems;
    unsigned long bytes_after;
    guint *prop;
    Atom actual_type;
    int actual_format;
    gboolean ret = FALSE;

    if (XRRGetOutputProperty (GDK_DISPLAY (), output, brightness->priv->backlight,
			      0, 4, False, False, None,
			      &actual_type, &actual_format,
			      &nitems, &bytes_after, ((unsigned char **)&prop)) != Success) 
    {
	g_warning ("failed to get property");
	return FALSE;
    }
    
    if (actual_type == XA_INTEGER && nitems == 1 && actual_format == 32) 
    {
	memcpy (current, prop, sizeof (guint));
	ret = TRUE;
    }
    
    XFree (prop);
    
    return ret;
}

static gboolean
xfpm_brightness_xrandr_set_level (XfpmBrightness *brightness, RROutput output, guint level)
{
    gboolean ret = TRUE;

    gdk_error_trap_push ();
    XRRChangeOutputProperty (GDK_DISPLAY (), output, brightness->priv->backlight, XA_INTEGER, 32,
			     PropModeReplace, (unsigned char *) &level, 1);
			     
    XFlush (GDK_DISPLAY ());
    gdk_flush ();
    
    if ( gdk_error_trap_pop () ) 
    {
	    g_warning ("failed to XRRChangeOutputProperty for brightness %i", level);
	    ret = FALSE;
    }
    
    return ret;
}

static gboolean
xfpm_brightness_setup_xrandr (XfpmBrightness *brightness)
{
    GdkScreen *screen;
    XRROutputInfo *info;
    Window window;
    gint major, minor, screen_num;
    guint min, max;
    gboolean ret = FALSE;
    gint i;
    
    if ( !XRRQueryVersion (GDK_DISPLAY (), &major, &minor) )
    {
	TRACE ("No XRANDR extension found");
	return FALSE;
    }
    
    brightness->priv->backlight = XInternAtom (GDK_DISPLAY (), "BACKLIGHT", True);
    
    if (brightness->priv->backlight == None) 
    {
	TRACE ("No outputs have backlight property");
	return FALSE;
    }
	
    screen = gdk_display_get_default_screen (gdk_display_get_default ());
    
    screen_num = gdk_screen_get_number (screen);
    
    window = RootWindow (GDK_DISPLAY (), screen_num);
    
#if (RANDR_MAJOR == 1 && RANDR_MINOR >=3 )
    brightness->priv->resource = XRRGetScreenResourcesCurrent (GDK_DISPLAY (), window);
#else
    brightness->priv->resource = XRRGetScreenResources (GDK_DISPLAY (), window);
#endif
    
    for ( i = 0; i < brightness->priv->resource->noutput; i++)
    {
	info = XRRGetOutputInfo (GDK_DISPLAY (), brightness->priv->resource, brightness->priv->resource->outputs[i]);
	
	if ( !g_strcmp0 (info->name, "LVDS") )
	{
	    if ( xfpm_brightness_xrand_get_limit (brightness, brightness->priv->resource->outputs[i], &min, &max) &&
		 min != max )
	    {
		ret = TRUE;
		brightness->priv->output = brightness->priv->resource->outputs[i];
	    }
	    
	}
	XRRFreeOutputInfo (info);
    }
    return ret;
}

static gboolean
xfpm_brightness_xrand_up (XfpmBrightness *brightness, guint *new_level)
{
    guint hw_level;
    gboolean ret;
    
    ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, &hw_level);
    
    if ( !ret )
	return FALSE;
	
    if ( hw_level + brightness->priv->step <= brightness->priv->max_level)
	ret = xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, hw_level + brightness->priv->step);
    else
	ret = xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, brightness->priv->max_level);
	
    ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, new_level);
    
    if ( !ret )
	return FALSE;
	
    /* Nothing changed in the hardware*/
    if ( *new_level == hw_level )
	return FALSE;
    
    return ret;
}

static gboolean
xfpm_brightness_xrand_down (XfpmBrightness *brightness, guint *new_level)
{
    guint hw_level;
    gboolean ret;
    
    ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, &hw_level);
    
    if ( !ret )
	return FALSE;
	
    if ( hw_level - brightness->priv->step >= brightness->priv->min_level)
	ret = xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, hw_level - brightness->priv->step);
    else
	ret = xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, brightness->priv->min_level);
	
    ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, new_level);
    
    if ( !ret )
	return FALSE;
    
    /* Nothing changed in the hardware*/
    if ( *new_level == hw_level )
	return FALSE;
    
    return ret;

}

/*
 * Begin HAL optional brightness code. 
 * 
 */

#ifdef WITH_HAL
static gboolean
xfpm_brightness_hal_get_level (XfpmBrightness *brg, guint *level)
{
    GError *error = NULL;
    gboolean ret = FALSE;
    
    ret = dbus_g_proxy_call (brg->priv->hal_proxy, "GetBrightness", &error,
	 		     G_TYPE_INVALID,
			     G_TYPE_INT, level,
			     G_TYPE_INVALID);

    if (error)
    {
	g_warning ("GetBrightness failed : %s\n", error->message);
	g_error_free (error);
    }
    
    return ret;
}

static gboolean
xfpm_brightness_hal_set_level (XfpmBrightness *brg, gint level)
{
    GError *error = NULL;
    gboolean ret = FALSE;
    gint dummy;
    
    TRACE ("Setting level %d", level);
    
    ret = dbus_g_proxy_call (brg->priv->hal_proxy, "SetBrightness", &error,
			     G_TYPE_INT, level,
			     G_TYPE_INVALID,
			     G_TYPE_INT, &dummy, /* Just to avoid a warning! */
			     G_TYPE_INVALID );
		       
    if ( error )
    {
	g_critical ("Error setting brightness level: %s\n", error->message);
	g_error_free (error);
	return FALSE;
    }
    
    if (!ret)
    {
	g_warning ("SetBrightness failed\n");
    }
    
    return TRUE;
}


static gboolean
xfpm_brightness_hal_up (XfpmBrightness *brightness)
{
    guint hw_level;
    gboolean ret = TRUE;
    
    ret = xfpm_brightness_hal_get_level (brightness, &hw_level);
    
    if ( !ret )
	return FALSE;
	
    if ( hw_level + brightness->priv->step <= brightness->priv->max_level )
    {
	ret = xfpm_brightness_hal_set_level (brightness, hw_level + brightness->priv->step);
    }
    else
    {
	ret = xfpm_brightness_hal_set_level (brightness, brightness->priv->max_level);
    }
    
    return ret;
}

static gboolean
xfpm_brightness_hal_down (XfpmBrightness *brightness)
{
    guint hw_level;
    gboolean ret = TRUE;
    
    ret = xfpm_brightness_hal_get_level (brightness, &hw_level);
    
    if ( !ret )
	return FALSE;
    
    if ( hw_level - brightness->priv->step >= brightness->priv->min_level  )
    {
	ret = xfpm_brightness_hal_set_level (brightness, hw_level - brightness->priv->step);
    }
    else
    {
	ret = xfpm_brightness_hal_set_level (brightness, brightness->priv->min_level);
    }
    
    return ret;
}

static gboolean
xfpm_brightness_setup_hal (XfpmBrightness *brightness)
{
    DBusGConnection *bus;
    HalDevice *device;
    gchar **udi = NULL;
    HalManager *manager;
    
    manager = hal_manager_new ();
    
    udi = hal_manager_find_device_by_capability (manager, "laptop_panel");
    g_object_unref ( manager);
    
    if ( !udi || !udi[0])
    {
    	return FALSE;
    }
    
    device = hal_device_new ();
    hal_device_set_udi (device, udi[0]);
    
    brightness->priv->max_level = hal_device_get_property_int (device, "laptop_panel.num_levels") - 1;
    brightness->priv->step = brightness->priv->max_level <= 20 ? 1 : brightness->priv->max_level / 20;
    brightness->priv->min_level = brightness->priv->step;
    brightness->priv->hal_hw_found = TRUE;
    
    if ( hal_device_has_key (device, "laptop_panel.brightness_in_hardware") )
	brightness->priv->hal_brightness_in_hw = hal_device_get_property_bool (device ,"laptop_panel.brightness_in_hardware");
	
    TRACE ("laptop_panel.num_levels=%d\n", brightness->priv->max_level);
    
    g_object_unref (device);

    /*FIXME, check for errors*/
    bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
    
    brightness->priv->hal_proxy = dbus_g_proxy_new_for_name (bus,
							     "org.freedesktop.Hal",
							     udi[0],
							     "org.freedesktop.Hal.Device.LaptopPanel");
     
    if ( !brightness->priv->hal_proxy )
    {
	g_warning ("Unable to get proxy for device %s\n", udi[0]);
	brightness->priv->hal_hw_found = FALSE;
    }
    
    hal_manager_free_string_array (udi);
    
    return brightness->priv->hal_hw_found;
}
#endif /* WITH_HAL*/

static void
xfpm_brightness_class_init (XfpmBrightnessClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_brightness_finalize;

    g_type_class_add_private (klass, sizeof (XfpmBrightnessPrivate));
}

static void
xfpm_brightness_init (XfpmBrightness *brightness)
{
    brightness->priv = XFPM_BRIGHTNESS_GET_PRIVATE (brightness);
    
    brightness->priv->resource = NULL;
    brightness->priv->xrandr_has_hw = FALSE;
    brightness->priv->max_level = 0;
    brightness->priv->min_level = 0;
    brightness->priv->current_level = 0;
    brightness->priv->output = 0;
    brightness->priv->step = 0;
    
#ifdef WITH_HAL
    brightness->priv->hal_proxy = NULL;
    brightness->priv->hal_brightness_in_hw = FALSE;
    brightness->priv->hal_hw_found = FALSE;
#endif
}

static void
xfpm_brightness_finalize (GObject *object)
{
    XfpmBrightness *brightness;

    brightness = XFPM_BRIGHTNESS (object);

    if ( brightness->priv->resource )
	XRRFreeScreenResources (brightness->priv->resource);

    G_OBJECT_CLASS (xfpm_brightness_parent_class)->finalize (object);
}

XfpmBrightness *
xfpm_brightness_new (void)
{
    XfpmBrightness *brightness = NULL;
    brightness = g_object_new (XFPM_TYPE_BRIGHTNESS, NULL);
    return brightness;
}

gboolean
xfpm_brightness_setup (XfpmBrightness *brightness)
{
    brightness->priv->xrandr_has_hw = xfpm_brightness_setup_xrandr (brightness);

    if ( brightness->priv->xrandr_has_hw )
    {
	xfpm_brightness_xrand_get_limit (brightness,
					 brightness->priv->output, 
					 &brightness->priv->min_level, 
					 &brightness->priv->max_level);
	g_debug ("Brightness controlled by xrandr, min_level=%u max_level=%u", 
		 brightness->priv->min_level, 
		 brightness->priv->max_level);
		 
    }
#ifdef WITH_HAL    
    else if ( !brightness->priv->xrandr_has_hw )
    {
	g_debug ("Unable to control brightness via xrandr, trying to use HAL");
	if ( xfpm_brightness_setup_hal (brightness) )
	    return TRUE;
    }
#endif
    
    return brightness->priv->xrandr_has_hw;
}

gboolean xfpm_brightness_up (XfpmBrightness *brightness, guint *new_level)
{
    gboolean ret = FALSE;
    
    if ( brightness->priv->xrandr_has_hw )
    {
	ret = xfpm_brightness_xrand_up (brightness, new_level);
    }	
#ifdef WITH_HAL
    else if ( brightness->priv->hal_hw_found )
    {
	ret = xfpm_brightness_hal_up (brightness);
	if ( ret )
	    ret = xfpm_brightness_hal_get_level (brightness, new_level);
    }
#endif
    return ret;
}

gboolean xfpm_brightness_down (XfpmBrightness *brightness, guint *new_level)
{
    gboolean ret = FALSE;
    
    if ( brightness->priv->xrandr_has_hw )
    {
	ret = xfpm_brightness_xrand_down (brightness, new_level);
	if ( ret )
	    ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, new_level);
    }	
#ifdef WITH_HAL
    else if ( brightness->priv->hal_hw_found )
    {
	ret = xfpm_brightness_hal_down (brightness);
	if ( ret )
	    ret = xfpm_brightness_hal_get_level (brightness, new_level);
    }
#endif
    return ret;
}

gboolean xfpm_brightness_has_hw (XfpmBrightness *brightness)
{
#ifdef WITH_HAL
    if ( !brightness->priv->xrandr_has_hw )
	return brightness->priv->hal_hw_found;
#endif
    
    return brightness->priv->xrandr_has_hw;
}

guint xfpm_brightness_get_max_level (XfpmBrightness *brightness)
{
    return brightness->priv->max_level;
}

gboolean xfpm_brightness_get_level	(XfpmBrightness *brightness, guint *level)
{
    gboolean ret = FALSE;
    
    if ( brightness->priv->xrandr_has_hw )
	ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, level);
#ifdef WITH_HAL
    else if ( brightness->priv->hal_hw_found )
	ret = xfpm_brightness_hal_get_level (brightness, level);
#endif

    return ret;
}

gboolean xfpm_brightness_set_level (XfpmBrightness *brightness, guint level)
{
    gboolean ret = FALSE;
    
    if (brightness->priv->xrandr_has_hw )
	ret = xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, level);
#ifdef WITH_HAL
    else if ( brightness->priv->hal_hw_found )
	ret = xfpm_brightness_hal_set_level (brightness, level);
#endif
    
    return ret;
}

gboolean xfpm_brightness_dim_down (XfpmBrightness *brightness)
{
    gboolean ret = FALSE;
    
    if (brightness->priv->xrandr_has_hw )
	ret = xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, brightness->priv->min_level);
#ifdef WITH_HAL
    else if ( brightness->priv->hal_hw_found )
	ret = xfpm_brightness_hal_set_level (brightness, brightness->priv->min_level);
#endif
    
    return ret;
}

XfpmBrightnessControl xfpm_brightness_get_control (XfpmBrightness *brightness)
{
    if ( brightness->priv->xrandr_has_hw )
	return XFPM_BRIGHTNESS_CONTROL_XRANDR;
#ifdef WITH_HAL
    else if ( brightness->priv->hal_hw_found )
	return XFPM_BRIGHTNESS_CONTROL_HAL;
#endif
    return XFPM_BRIGHTNESS_CONTROL_UNKNOWN;
}

gboolean xfpm_brightness_in_hw	(XfpmBrightness *brightness)
{
#ifdef WITH_HAL
    return brightness->priv->hal_brightness_in_hw;
#endif
    return FALSE;
}
