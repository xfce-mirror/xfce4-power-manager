/*
 * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
 * Copyright (C) 2023 Gaël Bonithon <gael@xfce.org>
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

#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "xfpm-brightness-x11.h"
#include "xfpm-debug.h"

static gboolean       xfpm_brightness_x11_setup           (XfpmBrightness       *brightness,
                                                           gint32               *min_level,
                                                           gint32               *max_level);
static gboolean       xfpm_brightness_x11_get_level       (XfpmBrightness       *brightness,
                                                           gint32               *level);
static gboolean       xfpm_brightness_x11_set_level       (XfpmBrightness       *brightness,
                                                           gint32                level);
static gboolean       xfpm_brightness_x11_get_switch      (XfpmBrightness       *brightness,
                                                           gint                 *_switch);
static gboolean       xfpm_brightness_x11_set_switch      (XfpmBrightness       *brightness,
                                                           gint                  _switch);

struct _XfpmBrightnessX11
{
  XfpmBrightness __parent__;

  Atom backlight;
  gint output;
};



G_DEFINE_FINAL_TYPE (XfpmBrightnessX11, xfpm_brightness_x11, XFPM_TYPE_BRIGHTNESS)



static void
xfpm_brightness_x11_class_init (XfpmBrightnessX11Class *klass)
{
  XfpmBrightnessClass *brightness_class = XFPM_BRIGHTNESS_CLASS (klass);

  brightness_class->setup = xfpm_brightness_x11_setup;
  brightness_class->get_level = xfpm_brightness_x11_get_level;
  brightness_class->set_level = xfpm_brightness_x11_set_level;
  brightness_class->get_switch = xfpm_brightness_x11_get_switch;
  brightness_class->set_switch = xfpm_brightness_x11_set_switch;
}

static void
xfpm_brightness_x11_init (XfpmBrightnessX11 *brightness)
{
}



static gboolean
get_limit (XfpmBrightnessX11 *brightness,
           RROutput output,
           gint *min,
           gint *max)
{
  GdkDisplay *display = gdk_display_get_default ();
  XRRPropertyInfo *info;

  gdk_x11_display_error_trap_push (display);
  info = XRRQueryOutputProperty (gdk_x11_get_default_xdisplay (), output, brightness->backlight);

  if (gdk_x11_display_error_trap_pop (display) != 0 || info == NULL)
  {
    g_warning ("Failed to XRRQueryOutputProperty");
    return FALSE;
  }

  if (!info->range || info->num_values != 2)
  {
    g_warning ("No range found");
    XFree (info);
    return FALSE;
  }

  *min = info->values[0];
  *max = info->values[1];
  XFree (info);

  return TRUE;
}

static gboolean
xfpm_brightness_x11_setup (XfpmBrightness *_brightness,
                           gint32 *min_level,
                           gint32 *max_level)
{
  XfpmBrightnessX11 *brightness = XFPM_BRIGHTNESS_X11 (_brightness);
  Display *display = gdk_x11_get_default_xdisplay ();
  GdkDisplay *gdisplay = gdk_display_get_default ();
  XRRScreenResources *resource;
  Window window;
  gint major, minor;
  int event_base, error_base;
  gboolean success = FALSE;

  gdk_x11_display_error_trap_push (gdisplay);
  if (!XRRQueryExtension (gdk_x11_get_default_xdisplay (), &event_base, &error_base) ||
      !XRRQueryVersion (gdk_x11_get_default_xdisplay (), &major, &minor))
  {
    gdk_x11_display_error_trap_pop_ignored (gdisplay);
    g_warning ("No XRANDR extension found");
    return FALSE;
  }
  gdk_x11_display_error_trap_pop_ignored (gdisplay);

  if (major == 1 && minor < 2)
  {
    g_warning ("XRANDR version < 1.2");
    return FALSE;
  }

#ifdef RR_PROPERTY_BACKLIGHT
  brightness->backlight = XInternAtom (display, RR_PROPERTY_BACKLIGHT, True);
  if (brightness->backlight == None) /* fall back to deprecated name */
#endif
  brightness->backlight = XInternAtom (display, "BACKLIGHT", True);
  if (brightness->backlight == None)
  {
    XFPM_DEBUG ("No outputs have backlight property");
    return FALSE;
  }

  gdk_x11_display_error_trap_push (gdisplay);

  window = gdk_x11_get_default_root_xwindow ();
#if (RANDR_MAJOR == 1 && RANDR_MINOR >=3 )
  if (major > 1 || minor >= 3)
    resource = XRRGetScreenResourcesCurrent (display, window);
  else
#endif
    resource = XRRGetScreenResources (display, window);

  for (gint i = 0; i < resource->noutput; i++)
  {
    XRROutputInfo *info = XRRGetOutputInfo (display, resource, resource->outputs[i]);
    if (g_str_has_prefix (info->name, "LVDS") || g_str_has_prefix (info->name, "eDP"))
    {
      if (get_limit (brightness, resource->outputs[i], min_level, max_level)
          && *min_level != *max_level)
      {
        success = TRUE;
        brightness->output = resource->outputs[i];
      }
    }

    XRRFreeOutputInfo (info);
    if (success)
      break;
  }

  XRRFreeScreenResources (resource);

  if (gdk_x11_display_error_trap_pop (gdisplay) != 0)
    g_critical ("Failed to get output/resource info");

  if (!success)
  {
    XFPM_DEBUG ("Could not find output to control");
    return FALSE;
  }

  XFPM_DEBUG ("Brightness controlled by xrandr, min_level=%d, max_level=%d", *min_level, *max_level);
  return TRUE;
}

static gboolean
xfpm_brightness_x11_get_level (XfpmBrightness *_brightness,
                               gint32 *level)
{
  XfpmBrightnessX11 *brightness = XFPM_BRIGHTNESS_X11 (_brightness);
  GdkDisplay *display = gdk_display_get_default ();
  unsigned long nitems;
  unsigned long bytes_after;
  gint32 *prop;
  Atom actual_type;
  int actual_format;

  gdk_x11_display_error_trap_push (display);
  if (XRRGetOutputProperty (gdk_x11_get_default_xdisplay (), brightness->output, brightness->backlight,
                            0, 4, False, False, None,
                            &actual_type, &actual_format,
                            &nitems, &bytes_after, ((unsigned char **) &prop)) != Success
                            || gdk_x11_display_error_trap_pop (display) != 0)
  {
    g_warning ("Failed to XRRGetOutputProperty");
    return FALSE;
  }

  if (actual_type == XA_INTEGER && nitems == 1 && actual_format == 32)
  {
    memcpy (level, prop, sizeof (*level));
    XFree (prop);
    return TRUE;
  }

  XFree (prop);
  return FALSE;
}

static gboolean
xfpm_brightness_x11_set_level (XfpmBrightness *_brightness,
                               gint32 level)
{
  XfpmBrightnessX11 *brightness = XFPM_BRIGHTNESS_X11 (_brightness);
  Display *display = gdk_x11_get_default_xdisplay ();
  GdkDisplay *gdisplay = gdk_display_get_default ();

  gdk_x11_display_error_trap_push (gdisplay);
  XRRChangeOutputProperty (display, brightness->output, brightness->backlight, XA_INTEGER, 32,
                           PropModeReplace, (unsigned char *) &level, 1);
  XFlush (display);
  gdk_display_flush (gdisplay);

  if (gdk_x11_display_error_trap_pop (gdisplay))
  {
    g_warning ("Failed to XRRChangeOutputProperty for brightness %d", level);
    return FALSE;
  }

  return TRUE;
}

static gboolean
xfpm_brightness_x11_get_switch (XfpmBrightness *brightness,
                                gint *_switch)
{
  return FALSE;
}

static gboolean
xfpm_brightness_x11_set_switch (XfpmBrightness *brightness,
                                gint _switch)
{
  return FALSE;
}
