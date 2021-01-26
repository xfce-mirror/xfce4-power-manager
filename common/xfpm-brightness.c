/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <libxfce4util/libxfce4util.h>

#include "xfpm-brightness.h"
#include "xfpm-debug.h"

static void xfpm_brightness_finalize   (GObject *object);

struct XfpmBrightnessPrivate
{
  XRRScreenResources *resource;
  Atom    backlight;
  gint    output;
  gboolean    xrandr_has_hw;
  gboolean    helper_has_hw;
  gboolean    use_exp_step;

  gint32    max_level;
  gint32    current_level;
  gint32    min_level;
  gint32    step;
  gfloat    exp_step;
};

G_DEFINE_TYPE_WITH_PRIVATE (XfpmBrightness, xfpm_brightness, G_TYPE_OBJECT)

static gint32
xfpm_brightness_inc (XfpmBrightness *brightness, gint32 level)
{
  if (brightness->priv->use_exp_step)
  {
    gint32 new_level = roundf (level * brightness->priv->exp_step);
    if (new_level == level)  ++new_level;
    return new_level;
  }
  else
    return level + brightness->priv->step;
}

static gint32
xfpm_brightness_dec (XfpmBrightness *brightness, gint32 level)
{
  if (brightness->priv->use_exp_step)
  {
    gint32 new_level = roundf (level / brightness->priv->exp_step);
    if (new_level == level)  --new_level;
    return new_level;
  }
  else
    return level - brightness->priv->step;
}

static gboolean
xfpm_brightness_xrand_get_limit (XfpmBrightness *brightness, RROutput output, gint *min, gint *max)
{
  XRRPropertyInfo *info;
  gboolean ret = TRUE;
  GdkDisplay *gdisplay;

  gdisplay = gdk_display_get_default ();

  gdk_x11_display_error_trap_push (gdisplay);
  info = XRRQueryOutputProperty (gdk_x11_get_default_xdisplay (), output, brightness->priv->backlight);

  if (gdk_x11_display_error_trap_pop (gdisplay) != 0
      || info == NULL)
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
xfpm_brightness_xrandr_get_level (XfpmBrightness *brightness, RROutput output, gint32 *current)
{
  unsigned long nitems;
  unsigned long bytes_after;
  gint32 *prop;
  Atom actual_type;
  int actual_format;
  gboolean ret = FALSE;
  GdkDisplay *gdisplay;

  gdisplay = gdk_display_get_default ();

  gdk_x11_display_error_trap_push (gdisplay);
  if (XRRGetOutputProperty (gdk_x11_get_default_xdisplay (), output, brightness->priv->backlight,
                            0, 4, False, False, None,
                            &actual_type, &actual_format,
                            &nitems, &bytes_after, ((unsigned char **)&prop)) != Success
                            || gdk_x11_display_error_trap_pop (gdisplay) != 0)
  {
    g_warning ("failed to get property");
    return FALSE;
  }

  if (actual_type == XA_INTEGER && nitems == 1 && actual_format == 32)
  {
    memcpy (current, prop, sizeof (*current));
    ret = TRUE;
  }

  XFree (prop);

  return ret;
}

static gboolean
xfpm_brightness_xrandr_set_level (XfpmBrightness *brightness, RROutput output, gint32 level)
{
  gboolean ret = TRUE;
  Display *display;
  GdkDisplay *gdisplay;

  display = gdk_x11_get_default_xdisplay ();
  gdisplay = gdk_display_get_default ();

  gdk_x11_display_error_trap_push (gdisplay);
  XRRChangeOutputProperty (display, output, brightness->priv->backlight, XA_INTEGER, 32,
                           PropModeReplace, (unsigned char *) &level, 1);

  XFlush (display);
  gdk_display_flush (gdisplay);

  if ( gdk_x11_display_error_trap_pop (gdisplay) )
  {
    g_warning ("failed to XRRChangeOutputProperty for brightness %d", level);
    ret = FALSE;
  }

  return ret;
}

static gboolean
xfpm_brightness_setup_xrandr (XfpmBrightness *brightness)
{
  GdkScreen *screen;
  GdkDisplay *gdisplay;
  XRROutputInfo *info;
  Window window;
  gint major, minor, screen_num;
  int event_base, error_base;
  gint32 min, max;
  gboolean ret = FALSE;
  gint i;

  gdisplay = gdk_display_get_default ();

  gdk_x11_display_error_trap_push (gdisplay);
  if (!XRRQueryExtension (gdk_x11_get_default_xdisplay (), &event_base, &error_base) ||
      !XRRQueryVersion (gdk_x11_get_default_xdisplay (), &major, &minor) )
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
  brightness->priv->backlight = XInternAtom (gdk_x11_get_default_xdisplay (), RR_PROPERTY_BACKLIGHT, True);
  if (brightness->priv->backlight == None) /* fall back to deprecated name */
#endif
  brightness->priv->backlight = XInternAtom (gdk_x11_get_default_xdisplay (), "BACKLIGHT", True);

  if (brightness->priv->backlight == None)
  {
    g_warning ("No outputs have backlight property");
    return FALSE;
  }

  screen = gdk_display_get_default_screen (gdisplay);

  screen_num = gdk_x11_screen_get_screen_number (screen);

  gdk_x11_display_error_trap_push (gdisplay);

  window = RootWindow (gdk_x11_get_default_xdisplay (), screen_num);

#if (RANDR_MAJOR == 1 && RANDR_MINOR >=3 )
  if (major > 1 || minor >= 3)
    brightness->priv->resource = XRRGetScreenResourcesCurrent (gdk_x11_get_default_xdisplay (), window);
  else
#endif
    brightness->priv->resource = XRRGetScreenResources (gdk_x11_get_default_xdisplay (), window);

  for ( i = 0; i < brightness->priv->resource->noutput; i++)
  {
    info = XRRGetOutputInfo (gdk_x11_get_default_xdisplay (), brightness->priv->resource, brightness->priv->resource->outputs[i]);

    if ( g_str_has_prefix (info->name, "LVDS") || g_str_has_prefix (info->name, "eDP") )
    {
      if ( xfpm_brightness_xrand_get_limit (brightness, brightness->priv->resource->outputs[i], &min, &max) &&
           min != max )
      {
        ret = TRUE;
        brightness->priv->output = brightness->priv->resource->outputs[i];
        brightness->priv->step =  max <= 20 ? 1 : max / 10;
        brightness->priv->exp_step = 2;
      }
    }

    XRRFreeOutputInfo (info);
  }

  if (gdk_x11_display_error_trap_pop (gdisplay) != 0)
    g_critical ("Failed to get output/resource info");

  return ret;
}

static gboolean
xfpm_brightness_xrand_up (XfpmBrightness *brightness, gint32 *new_level)
{
  gint32 hw_level;
  gboolean ret = FALSE;
  gint32 set_level;

  ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, &hw_level);

  if ( !ret )
    return FALSE;

  if ( hw_level == brightness->priv->max_level )
  {
    *new_level = brightness->priv->max_level;
    return TRUE;
  }

  set_level = MIN (xfpm_brightness_inc (brightness, hw_level), brightness->priv->max_level);

  g_warn_if_fail (xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, set_level));

  ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, new_level);

  if ( !ret )
  {
    g_warning ("xfpm_brightness_xrand_up failed for %d", set_level);
    return FALSE;
  }

  /* Nothing changed in the hardware*/
  if ( *new_level == hw_level )
  {
    g_warning ("xfpm_brightness_xrand_up did not change the hw level to %d", set_level);
    return FALSE;
  }

  return TRUE;
}

static gboolean
xfpm_brightness_xrand_down (XfpmBrightness *brightness, gint32 *new_level)
{
  gint32 hw_level;
  gboolean ret;
  gint32 set_level;

  ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, &hw_level);

  if ( !ret )
    return FALSE;

  if ( hw_level == brightness->priv->min_level )
  {
    *new_level = brightness->priv->min_level;
    return TRUE;
  }

  set_level = MAX (xfpm_brightness_dec (brightness, hw_level), brightness->priv->min_level);

  g_warn_if_fail (xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, set_level));

  ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, new_level);

  if ( !ret )
  {
    g_warning ("xfpm_brightness_xrand_down failed for %d", set_level);
    return FALSE;
  }

  /* Nothing changed in the hardware*/
  if ( *new_level == hw_level )
  {
    g_warning ("xfpm_brightness_xrand_down did not change the hw level to %d", set_level);
    return FALSE;
  }

  return TRUE;
}

/*
 * Non-XRandR fallback using xfpm-backlight-helper
 */

#ifdef ENABLE_POLKIT

static gint
xfpm_brightness_helper_get_value (const gchar *argument)
{
  gboolean ret;
  GError *error = NULL;
  gchar *stdout_data = NULL;
  gint exit_status = 0;
  gint value = -1;
  gchar *command = NULL;

  command = g_strdup_printf (SBINDIR "/xfpm-power-backlight-helper --%s", argument);
  ret = g_spawn_command_line_sync (command,
                                   &stdout_data, NULL, &exit_status, &error);
  if ( !ret )
  {
    if (error)
    {
      g_warning ("failed to get value: %s", error->message);
      g_error_free (error);
    }
    goto out;
  }
  g_debug ("executed %s; retval: %i", command, exit_status);

  if ( exit_status != 0 )
    goto out;

#if !defined(BACKEND_TYPE_FREEBSD)
  if ( stdout_data[0] == 'N' )
    value = 0;
  else if ( stdout_data[0] == 'Y' )
    value = 1;
  else
    value = atoi (stdout_data);
#else
  value = atoi (stdout_data);
#endif

out:
  g_free (command);
  g_free (stdout_data);
  return value;
}

static gboolean
xfpm_brightness_setup_helper (XfpmBrightness *brightness)
{
  gint32 ret;

  ret = (gint32) xfpm_brightness_helper_get_value ("get-max-brightness");
  g_debug ("xfpm_brightness_setup_helper: get-max-brightness returned %i", ret);
  if ( ret < 0 )
  {
    brightness->priv->helper_has_hw = FALSE;
  }
  else
  {
    brightness->priv->helper_has_hw = TRUE;
    brightness->priv->min_level = 0;
    brightness->priv->max_level = ret;
    brightness->priv->step =  ret <= 20 ? 1 : ret / 10;
    brightness->priv->exp_step = 2;
  }

  return brightness->priv->helper_has_hw;
}

static gboolean
xfpm_brightness_helper_get_level (XfpmBrightness *brg, gint32 *level)
{
  gint32 ret;

  if ( ! brg->priv->helper_has_hw )
    return FALSE;

  ret = (gint32) xfpm_brightness_helper_get_value ("get-brightness");

  g_debug ("xfpm_brightness_helper_get_level: get-brightness returned %i", ret);

  if ( ret >= 0 )
  {
    *level = ret;
    return TRUE;
  }

  return FALSE;
}

static gboolean
xfpm_brightness_helper_set_level (XfpmBrightness *brg, gint32 level)
{
  gboolean ret;
  GError *error = NULL;
  gint exit_status = 0;
  gchar *command = NULL;

  command = g_strdup_printf ("pkexec " SBINDIR "/xfpm-power-backlight-helper --set-brightness %i", level);
  ret = g_spawn_command_line_sync (command, NULL, NULL, &exit_status, &error);
  if ( !ret )
  {
    if (error)
    {
      g_warning ("xfpm_brightness_helper_set_level: failed to set value: %s", error->message);
      g_error_free (error);
    }
    goto out;
  }
  g_debug ("executed %s; retval: %i", command, exit_status);
  ret = (exit_status == 0);

out:
  g_free (command);
  return ret;
}

static gboolean
xfpm_brightness_helper_get_switch (XfpmBrightness *brg, gint *brightness_switch)
{
  gint ret;

  ret = xfpm_brightness_helper_get_value ("get-brightness-switch");

  if ( ret >= 0 )
  {
    *brightness_switch = ret;
    return TRUE;
  }

  return FALSE;
}

static gboolean
xfpm_brightness_helper_set_switch (XfpmBrightness *brg, gint brightness_switch)
{
  gboolean ret;
  GError *error = NULL;
  gint exit_status = 0;
  gchar *command = NULL;

  command = g_strdup_printf ("pkexec " SBINDIR "/xfpm-power-backlight-helper --set-brightness-switch %i", brightness_switch);
  ret = g_spawn_command_line_sync (command, NULL, NULL, &exit_status, &error);
  if ( !ret )
  {
    if (error)
    {
      g_warning ("xfpm_brightness_helper_set_switch: failed to set value: %s", error->message);
      g_error_free (error);
    }
    goto out;
  }
  g_debug ("executed %s; retval: %i", command, exit_status);
  ret = (exit_status == 0);

out:
  g_free (command);
  return ret;
}

static gboolean
xfpm_brightness_helper_up (XfpmBrightness *brightness, gint32 *new_level)
{
  gint32 hw_level;
  gboolean ret = FALSE;
  gint32 set_level;

  ret = xfpm_brightness_helper_get_level (brightness, &hw_level);

  if ( !ret )
    return FALSE;

  if ( hw_level >= brightness->priv->max_level )
  {
    *new_level = brightness->priv->max_level;
    return TRUE;
  }

  set_level = MIN (xfpm_brightness_inc (brightness, hw_level), brightness->priv->max_level);

  g_warn_if_fail (xfpm_brightness_helper_set_level (brightness, set_level));

  ret = xfpm_brightness_helper_get_level (brightness, new_level);

  if ( !ret )
  {
    g_warning ("xfpm_brightness_helper_up failed for %d", set_level);
    return FALSE;
  }

  /* Nothing changed in the hardware*/
  if ( *new_level == hw_level )
  {
    g_warning ("xfpm_brightness_helper_up did not change the hw level to %d", set_level);
    return FALSE;
  }

  return TRUE;
}

static gboolean
xfpm_brightness_helper_down (XfpmBrightness *brightness, gint32 *new_level)
{
  gint32 hw_level;
  gboolean ret;
  gint32 set_level;

  ret = xfpm_brightness_helper_get_level (brightness, &hw_level);

  if ( !ret )
    return FALSE;

  if ( hw_level <= brightness->priv->min_level )
  {
    *new_level = brightness->priv->min_level;
    return TRUE;
  }

  set_level = MAX (xfpm_brightness_dec (brightness, hw_level), brightness->priv->min_level);

  g_warn_if_fail (xfpm_brightness_helper_set_level (brightness, set_level));

  ret = xfpm_brightness_helper_get_level (brightness, new_level);

  if ( !ret )
  {
    g_warning ("xfpm_brightness_helper_down failed for %d", set_level);
    return FALSE;
  }

  /* Nothing changed in the hardware*/
  if ( *new_level == hw_level )
  {
    g_warning ("xfpm_brightness_helper_down did not change the hw level to %d", set_level);
    return FALSE;
  }

  return TRUE;
}

#endif

static void
xfpm_brightness_class_init (XfpmBrightnessClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xfpm_brightness_finalize;
}

static void
xfpm_brightness_init (XfpmBrightness *brightness)
{
  brightness->priv = xfpm_brightness_get_instance_private (brightness);

  brightness->priv->resource = NULL;
  brightness->priv->xrandr_has_hw = FALSE;
  brightness->priv->helper_has_hw = FALSE;
  brightness->priv->use_exp_step = FALSE;
  brightness->priv->max_level = 0;
  brightness->priv->min_level = 0;
  brightness->priv->current_level = 0;
  brightness->priv->output = 0;
  brightness->priv->step = 0;
  brightness->priv->exp_step = 1;
}

static void
xfpm_brightness_free_data (XfpmBrightness *brightness)
{
  if ( brightness->priv->resource )
    XRRFreeScreenResources (brightness->priv->resource);
}

static void
xfpm_brightness_finalize (GObject *object)
{
  XfpmBrightness *brightness;

  brightness = XFPM_BRIGHTNESS (object);

  xfpm_brightness_free_data (brightness);

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
  xfpm_brightness_free_data (brightness);
  brightness->priv->xrandr_has_hw = xfpm_brightness_setup_xrandr (brightness);

  if ( brightness->priv->xrandr_has_hw )
  {
    xfpm_brightness_xrand_get_limit (brightness,
                                     brightness->priv->output,
                                     &brightness->priv->min_level,
                                     &brightness->priv->max_level);
    g_debug ("Brightness controlled by xrandr, min_level=%d max_level=%d",
             brightness->priv->min_level,
             brightness->priv->max_level);

    return TRUE;
  }
#ifdef ENABLE_POLKIT
  else
  {
    if ( xfpm_brightness_setup_helper (brightness) ) {
#if defined(BACKEND_TYPE_FREEBSD)
      g_debug ("xrandr not available, brightness controlled by sysctl helper; min_level=%d max_level=%d",
#else
      g_debug ("xrandr not available, brightness controlled by sysfs helper; min_level=%d max_level=%d",
#endif
               brightness->priv->min_level,
               brightness->priv->max_level);
      return TRUE;
    }
  }
#endif
  g_debug ("no brightness controls available");
  return FALSE;
}

gboolean xfpm_brightness_up (XfpmBrightness *brightness, gint32 *new_level)
{
  gboolean ret = FALSE;

  if ( brightness->priv->xrandr_has_hw )
  {
    ret = xfpm_brightness_xrand_up (brightness, new_level);
  }
#ifdef ENABLE_POLKIT
  else if ( brightness->priv->helper_has_hw )
  {
    ret = xfpm_brightness_helper_up (brightness, new_level);
  }
#endif
  return ret;
}

gboolean xfpm_brightness_down (XfpmBrightness *brightness, gint32 *new_level)
{
  gboolean ret = FALSE;

  if ( brightness->priv->xrandr_has_hw )
  {
    ret = xfpm_brightness_xrand_down (brightness, new_level);
    if ( ret )
      ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, new_level);
  }
#ifdef ENABLE_POLKIT
  else if ( brightness->priv->helper_has_hw )
  {
    ret = xfpm_brightness_helper_down (brightness, new_level);
  }
#endif
  return ret;
}

gboolean xfpm_brightness_has_hw (XfpmBrightness *brightness)
{
  return brightness->priv->xrandr_has_hw || brightness->priv->helper_has_hw;
}

gint32 xfpm_brightness_get_max_level (XfpmBrightness *brightness)
{
  return brightness->priv->max_level;
}

gboolean xfpm_brightness_get_level  (XfpmBrightness *brightness, gint32 *level)
{
  gboolean ret = FALSE;

  if ( brightness->priv->xrandr_has_hw )
    ret = xfpm_brightness_xrandr_get_level (brightness, brightness->priv->output, level);
#ifdef ENABLE_POLKIT
  else if ( brightness->priv->helper_has_hw )
    ret = xfpm_brightness_helper_get_level (brightness, level);
#endif

  return ret;
}

gboolean xfpm_brightness_set_level (XfpmBrightness *brightness, gint32 level)
{
  gboolean ret = FALSE;

  if (brightness->priv->xrandr_has_hw )
    ret = xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, level);
#ifdef ENABLE_POLKIT
  else if ( brightness->priv->helper_has_hw )
    ret = xfpm_brightness_helper_set_level (brightness, level);
#endif

  return ret;
}

gboolean xfpm_brightness_set_step_count (XfpmBrightness *brightness, guint32 count, gboolean exponential)
{
  gboolean ret = FALSE;

  if ( xfpm_brightness_has_hw (brightness) ) {
    guint32 delta;

    if ( count < 2 )
      count = 2;
    delta = brightness->priv->max_level - brightness->priv->min_level;
    brightness->priv->use_exp_step = exponential;
    brightness->priv->step = (delta < (count * 2)) ? 1 : (delta / count);
    brightness->priv->exp_step = powf (delta, 1.0 / count);
    ret = TRUE;
  }

  return ret;
}

gboolean xfpm_brightness_dim_down (XfpmBrightness *brightness)
{
  gboolean ret = FALSE;

  if (brightness->priv->xrandr_has_hw )
    ret = xfpm_brightness_xrandr_set_level (brightness, brightness->priv->output, brightness->priv->min_level);
#ifdef ENABLE_POLKIT
  else if ( brightness->priv->helper_has_hw )
    ret = xfpm_brightness_helper_set_level (brightness, brightness->priv->min_level);
#endif

  return ret;
}

gboolean xfpm_brightness_get_switch (XfpmBrightness *brightness, gint *brightness_switch)
{
  gboolean ret = FALSE;

#ifdef ENABLE_POLKIT
  if ( brightness->priv->helper_has_hw )
    ret = xfpm_brightness_helper_get_switch (brightness, brightness_switch);
#endif

  return ret;
}

gboolean xfpm_brightness_set_switch (XfpmBrightness *brightness, gint brightness_switch)
{
  gboolean ret = FALSE;

#ifdef ENABLE_POLKIT
  if ( brightness->priv->helper_has_hw )
    ret = xfpm_brightness_helper_set_switch (brightness, brightness_switch);
#endif

  return ret;
}
