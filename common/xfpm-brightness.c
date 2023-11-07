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

#include <math.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "xfpm-brightness.h"
#include "xfpm-debug.h"

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include "xfpm-brightness-x11.h"
#endif
#ifdef HAVE_POLKIT
#include "xfpm-brightness-polkit.h"
#endif

#define get_instance_private(instance) ((XfpmBrightnessPrivate *) \
  xfpm_brightness_get_instance_private (XFPM_BRIGHTNESS (instance)))

typedef struct _XfpmBrightnessPrivate
{
  gint32 min_level;
  gint32 max_level;
  gint32 step;
  gboolean use_exp_step;
  gfloat exp_step;
} XfpmBrightnessPrivate;



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (XfpmBrightness, xfpm_brightness, G_TYPE_OBJECT)



static void
xfpm_brightness_class_init (XfpmBrightnessClass *klass)
{
}

static void
xfpm_brightness_init (XfpmBrightness *brightness)
{
}



XfpmBrightness *
xfpm_brightness_new (void)
{
  XfpmBrightness *brightness = NULL;
  XfpmBrightnessPrivate *priv;

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
  {
    brightness = g_object_new (XFPM_TYPE_BRIGHTNESS_X11, NULL);
    priv = get_instance_private (brightness);
    if (!XFPM_BRIGHTNESS_GET_CLASS (brightness)->setup (brightness, &priv->min_level, &priv->max_level))
    {
      g_object_unref (brightness);
      brightness = NULL;
    }
  }
#endif
#ifdef HAVE_POLKIT
  if (brightness == NULL)
  {
    brightness = g_object_new (XFPM_TYPE_BRIGHTNESS_POLKIT, NULL);
    priv = get_instance_private (brightness);
    if (!XFPM_BRIGHTNESS_GET_CLASS (brightness)->setup (brightness, &priv->min_level, &priv->max_level))
    {
      g_object_unref (brightness);
      brightness = NULL;
    }
  }
#endif

  if (brightness == NULL)
  {
    XFPM_DEBUG ("No brightness controls available");
    return NULL;
  }

  xfpm_brightness_set_step_count (brightness, 10, FALSE);

  return brightness;
}

gint32
xfpm_brightness_get_max_level (XfpmBrightness *brightness)
{
  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), 0);
  return get_instance_private (brightness)->max_level;
}

void
xfpm_brightness_set_step_count (XfpmBrightness *brightness,
                                guint32 count,
                                gboolean exponential)
{
  XfpmBrightnessPrivate *priv = get_instance_private (brightness);
  guint32 delta;

  g_return_if_fail (XFPM_BRIGHTNESS (brightness));

  if (count < 2)
    count = 2;

  delta = priv->max_level - priv->min_level;
  priv->use_exp_step = exponential;
  priv->step = (delta < (count * 2)) ? 1 : (delta / count);
  priv->exp_step = powf (delta, 1.0 / count);
}

/**
 * Returns the next decrement in screen brightness, given the current level. If
 * the decrement would be below 0, then 0 is returned instead. Note that it is
 * left to callers to define their own lower limits; for an example, see
 * decrease_brightness in power-manager-button, which clamps the value above 0
 * to avoid errant mouse scroll events from blanking the display panel entirely.
 */
gint32
xfpm_brightness_dec (XfpmBrightness *brightness,
                     gint32 level)
{
  XfpmBrightnessPrivate *priv = get_instance_private (brightness);
  gint32 new_level;

  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), 0);

  if (priv->use_exp_step)
  {
    new_level = roundf (level / priv->exp_step);
    if (new_level == level)
      --new_level;
  }
  else
  {
    new_level = level - priv->step;
  }

  return MAX (0, new_level);
}

/**
 * Returns the next increment in screen brightness, given the current level. If
 * the increment would be above the maximum brightness, the maximum brightness
 * is returned.
 */
gint32
xfpm_brightness_inc (XfpmBrightness *brightness,
                     gint32 level)
{
  XfpmBrightnessPrivate *priv = get_instance_private (brightness);
  gint32 new_level;

  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), 0);

  if (priv->use_exp_step)
  {
    new_level = roundf (level * priv->exp_step);
    if (new_level == level)
      ++new_level;
  }
  else
  {
    new_level = level + priv->step;
  }

  return MIN (xfpm_brightness_get_max_level (brightness), new_level);
}



gboolean
xfpm_brightness_get_level (XfpmBrightness *brightness,
                           gint32 *level)
{
  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);
  return XFPM_BRIGHTNESS_GET_CLASS (brightness)->get_level (brightness, level);
}

gboolean
xfpm_brightness_set_level (XfpmBrightness *brightness,
                           gint32 level)
{
  XfpmBrightnessPrivate *priv = get_instance_private (brightness);

  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);

  if (level < priv->min_level || level > priv->max_level)
    return FALSE;

  return XFPM_BRIGHTNESS_GET_CLASS (brightness)->set_level (brightness, level);
}

gboolean
xfpm_brightness_get_switch (XfpmBrightness *brightness,
                            gint *_switch)
{
  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);
  return XFPM_BRIGHTNESS_GET_CLASS (brightness)->get_switch (brightness, _switch);
}

gboolean
xfpm_brightness_set_switch (XfpmBrightness *brightness,
                            gint _switch)
{
  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);
  return XFPM_BRIGHTNESS_GET_CLASS (brightness)->set_switch (brightness, _switch);
}
