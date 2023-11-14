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

#include "xfpm-brightness-polkit.h"
#include "xfpm-debug.h"

static gboolean       xfpm_brightness_polkit_setup           (XfpmBrightness       *brightness,
                                                              gint32               *min_level,
                                                              gint32               *max_level);
static gboolean       xfpm_brightness_polkit_get_level       (XfpmBrightness       *brightness,
                                                              gint32               *current);
static gboolean       xfpm_brightness_polkit_set_level       (XfpmBrightness       *brightness,
                                                              gint32                level);
static gboolean       xfpm_brightness_polkit_get_switch      (XfpmBrightness       *brightness,
                                                              gint                 *_switch);
static gboolean       xfpm_brightness_polkit_set_switch      (XfpmBrightness       *brightness,
                                                              gint                  _switch);

struct _XfpmBrightnessPolkit
{
  XfpmBrightness __parent__;
};



G_DEFINE_FINAL_TYPE (XfpmBrightnessPolkit, xfpm_brightness_polkit, XFPM_TYPE_BRIGHTNESS)



static void
xfpm_brightness_polkit_class_init (XfpmBrightnessPolkitClass *klass)
{
  XfpmBrightnessClass *brightness_class = XFPM_BRIGHTNESS_CLASS (klass);

  brightness_class->setup = xfpm_brightness_polkit_setup;
  brightness_class->get_level = xfpm_brightness_polkit_get_level;
  brightness_class->set_level = xfpm_brightness_polkit_set_level;
  brightness_class->get_switch = xfpm_brightness_polkit_get_switch;
  brightness_class->set_switch = xfpm_brightness_polkit_set_switch;
}

static void
xfpm_brightness_polkit_init (XfpmBrightnessPolkit *brightness)
{
}



static gint
helper_get_value (const gchar *argument)
{
  GError *error = NULL;
  gchar *stdout_data = NULL;
  gint status;
  gint value = -1;
  gchar *command = g_strdup_printf (SBINDIR "/xfpm-power-backlight-helper --%s", argument);

  XFPM_DEBUG ("Executing command: %s", command);
  if (!g_spawn_command_line_sync (command, &stdout_data, NULL, &status, &error)
      || !g_spawn_check_wait_status (status, &error))
  {
    XFPM_DEBUG ("Failed to get value: %s", error->message);
    g_error_free (error);
    g_free (command);
    return value;
  }

#ifndef BACKEND_TYPE_FREEBSD
  if (stdout_data[0] == 'N')
    value = 0;
  else if (stdout_data[0] == 'Y')
    value = 1;
  else
    value = atoi (stdout_data);
#else
  value = atoi (stdout_data);
#endif

  g_free (command);
  g_free (stdout_data);
  return value;
}

static gboolean
xfpm_brightness_polkit_setup (XfpmBrightness *brightness,
                              gint32 *min_level,
                              gint32 *max_level)
{
  *min_level = 0;
  *max_level = helper_get_value ("get-max-brightness");
  XFPM_DEBUG ("get-max-brightness returned %i", *max_level);

  if (*max_level >=0)
  {
#ifdef BACKEND_TYPE_FREEBSD
    const gchar *controller = "sysctl";
#else
    const gchar *controller = "sysfs";
#endif
    XFPM_DEBUG ("Windowing environment specific brightness control not available, controlled by %s helper: min_level=%d max_level=%d",
                controller, *min_level, *max_level);
    return TRUE;
  }

  return FALSE;
}

static gboolean
xfpm_brightness_polkit_get_level (XfpmBrightness *brightness,
                                  gint32 *level)
{
  gint32 ret = helper_get_value ("get-brightness");
  XFPM_DEBUG ("get-brightness returned %i", ret);

  if (ret >= 0)
  {
    *level = ret;
    return TRUE;
  }

  return FALSE;
}

static gboolean
xfpm_brightness_polkit_set_level (XfpmBrightness *brightness,
                                  gint32 level)
{
  GError *error = NULL;
  gint status;
  gchar *command = g_strdup_printf ("pkexec " SBINDIR "/xfpm-power-backlight-helper --set-brightness %i", level);

  XFPM_DEBUG ("Executing command: %s", command);
  if (!g_spawn_command_line_sync (command, NULL, NULL, &status, &error)
      || !g_spawn_check_wait_status (status, &error))
  {
    XFPM_DEBUG ("Failed to set value: %s", error->message);
    g_error_free (error);
    g_free (command);
    return FALSE;
  }

  g_free (command);
  return TRUE;
}

static gboolean
xfpm_brightness_polkit_get_switch (XfpmBrightness *brightness,
                                   gint *_switch)
{
  gint ret = helper_get_value ("get-brightness-switch");
  XFPM_DEBUG ("get-brightness-switch returned %i", ret);

  if (ret >= 0)
  {
    *_switch = ret;
    return TRUE;
  }

  return FALSE;
}

static gboolean
xfpm_brightness_polkit_set_switch (XfpmBrightness *brightness,
                                   gint _switch)
{
  GError *error = NULL;
  gint status;
  gchar *command = g_strdup_printf ("pkexec " SBINDIR "/xfpm-power-backlight-helper --set-brightness-switch %i", _switch);

  if (!g_spawn_command_line_sync (command, NULL, NULL, &status, &error)
      || !g_spawn_check_wait_status (status, &error))
  {
    XFPM_DEBUG ("Failed to set brightness switch value: %s", error->message);
    g_error_free (error);
    g_free (command);
    return FALSE;
  }

  g_free (command);
  return TRUE;
}
