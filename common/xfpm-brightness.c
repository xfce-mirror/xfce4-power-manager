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
  gint32 hw_min_level;
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

  priv->hw_min_level = priv->min_level;
  xfpm_brightness_set_step_count (brightness, 10, FALSE);

  return brightness;
}

gint32
xfpm_brightness_get_min_level (XfpmBrightness *brightness)
{
  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), 0);
  return get_instance_private (brightness)->min_level;
}

/*
 * Some laptops (and mostly newer ones with intel graphics) can turn off the
 * backlight completely. If the user is not careful and sets the brightness
 * very low using the slider, he might not be able to see the screen contents
 * anymore. Brightness keys do not work on every laptop, so it's better to use
 * a safe default minimum level that the user can change via the settings
 * editor if desired.
 */
void
xfpm_brightness_set_min_level (XfpmBrightness *brightness,
                               gint32 level)
{
  XfpmBrightnessPrivate *priv = get_instance_private (brightness);
  gint32 max_min;

  g_return_if_fail (XFPM_BRIGHTNESS (brightness));

  /* -1 = auto, we set the minimum as 10% of delta */
  if (level == -1)
  {
    priv->min_level = priv->hw_min_level + MAX (priv->step, (priv->max_level - priv->hw_min_level) / 10);
    XFPM_DEBUG ("Setting default min brightness (%d) above hardware min (%d)", priv->min_level, priv->hw_min_level);
    return;
  }

  max_min = priv->max_level - priv->step;
  if (level < priv->hw_min_level || level > max_min)
  {
    XFPM_DEBUG ("Set min brightness (%d) clamped to admissible values [%d, %d]", level, priv->hw_min_level, max_min);
    priv->min_level = CLAMP (level, priv->hw_min_level, max_min);
    return;
  }

  XFPM_DEBUG ("Setting min brightness at %d", level);
  priv->min_level = level;
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

  delta = priv->max_level - priv->hw_min_level;
  priv->use_exp_step = exponential;
  priv->step = (delta < (count * 2)) ? 1 : (delta / count);
  priv->exp_step = powf (delta, 1.0 / count);
}

gboolean
xfpm_brightness_decrease (XfpmBrightness *brightness)
{
  XfpmBrightnessPrivate *priv = get_instance_private (brightness);
  gint32 level, new_level;

  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);

  if (!xfpm_brightness_get_level (brightness, &level))
    return FALSE;

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

  return xfpm_brightness_set_level (brightness, new_level);
}

gboolean
xfpm_brightness_increase (XfpmBrightness *brightness)
{
  XfpmBrightnessPrivate *priv = get_instance_private (brightness);
  gint32 level, new_level;

  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);

  if (!xfpm_brightness_get_level (brightness, &level))
    return FALSE;

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

  return xfpm_brightness_set_level (brightness, new_level);
}



gboolean
xfpm_brightness_get_level (XfpmBrightness *brightness,
                           gint32 *level)
{
  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);
  g_return_val_if_fail (level != NULL, FALSE);
  return XFPM_BRIGHTNESS_GET_CLASS (brightness)->get_level (brightness, level);
}

gboolean
xfpm_brightness_set_level (XfpmBrightness *brightness,
                           gint32 level)
{
  XfpmBrightnessPrivate *priv = get_instance_private (brightness);

  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);

  if (level < priv->min_level || level > priv->max_level)
  {
    XFPM_DEBUG ("Set brightness (%d) clamped to admissible values [%d, %d]", level, priv->min_level, priv->max_level);
    level = CLAMP (level, priv->min_level, priv->max_level);
  }

  return XFPM_BRIGHTNESS_GET_CLASS (brightness)->set_level (brightness, level);
}

gboolean
xfpm_brightness_get_switch (XfpmBrightness *brightness,
                            gint *_switch)
{
  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);
  g_return_val_if_fail (_switch != NULL, FALSE);
  return XFPM_BRIGHTNESS_GET_CLASS (brightness)->get_switch (brightness, _switch);
}

gboolean
xfpm_brightness_set_switch (XfpmBrightness *brightness,
                            gint _switch)
{
  g_return_val_if_fail (XFPM_BRIGHTNESS (brightness), FALSE);
  return XFPM_BRIGHTNESS_GET_CLASS (brightness)->set_switch (brightness, _switch);
}
