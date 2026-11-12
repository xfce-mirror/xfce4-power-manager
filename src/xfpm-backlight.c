/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
 * * Copyright (C) 2019 Kacper Piwiński
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

#include "xfpm-backlight.h"
#include "xfpm-button.h"
#include "xfpm-idle.h"
#include "xfpm-power.h"
#include "xfpm-xfconf.h"

#include "common/xfpm-brightness.h"
#include "common/xfpm-common.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-enum-types.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-notify.h"

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

static void
xfpm_backlight_finalize (GObject *object);

static void
xfpm_backlight_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec);

static void
xfpm_backlight_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec);

struct XfpmBacklightPrivate
{
  XfpmBrightness *brightness;
  XfpmPower *power;
  XfpmIdle *idle;
  XfpmXfconf *conf;
  XfpmButton *button;
  XfpmNotify *notify;
  NotifyNotification *brightness_notification;

  gboolean on_battery;

  gint brightness_switch;
  gint brightness_switch_save;
  gboolean brightness_switch_initialized;

  gint32 last_level;
  gboolean dimmed;
  gboolean block;
};

enum
{
  PROP_0,
  PROP_BRIGHTNESS_SWITCH,
  PROP_BRIGHTNESS_SWITCH_SAVE,
  N_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (XfpmBacklight, xfpm_backlight, G_TYPE_OBJECT)


static void
xfpm_backlight_dim_brightness (XfpmBacklight *backlight)
{
  if (!xfpm_power_is_inhibited (backlight->priv->power))
  {
    if (!xfpm_brightness_get_level (backlight->priv->brightness, &backlight->priv->last_level))
    {
      XFPM_DEBUG ("Unable to get current brightness level");
      return;
    }

    guint dim_percent;
    g_object_get (G_OBJECT (backlight->priv->conf),
                  backlight->priv->on_battery ? BRIGHTNESS_LEVEL_ON_BATTERY : BRIGHTNESS_LEVEL_ON_AC, &dim_percent,
                  NULL);
    gint32 min_level = xfpm_brightness_get_min_level (backlight->priv->brightness);
    gint32 max_level = xfpm_brightness_get_max_level (backlight->priv->brightness);
    gint32 dim_level = min_level + dim_percent * (max_level - min_level) / 100;

    /**
     * Only reduce if the current level is brighter than
     * the configured dim_level
     **/
    if (backlight->priv->last_level > dim_level)
    {
      XFPM_DEBUG ("Current brightness level before dimming : %d, new %d", backlight->priv->last_level, dim_level);
      backlight->priv->dimmed = xfpm_brightness_set_level (backlight->priv->brightness, dim_level);
    }
  }
}

static void
xfpm_backlight_show (XfpmBacklight *backlight, gint32 level)
{
  XFPM_DEBUG ("Level %d", level);

  gint32 min_level = xfpm_brightness_get_min_level (backlight->priv->brightness);
  gint32 max_level = xfpm_brightness_get_max_level (backlight->priv->brightness);
  gfloat value = 100 * (level - min_level) / (max_level - min_level);

  xfpm_notify_show_brightness_notification (backlight->priv->notify,
                                            &backlight->priv->brightness_notification,
                                            _("Brightness: %.0f%%"),
                                            XFPM_DISPLAY_BRIGHTNESS_ICON,
                                            value);
}

static void
xfpm_backlight_alarm_timeout_cb (XfpmIdle *idle,
                                 XfpmAlarmId id,
                                 XfpmBacklight *backlight)
{
  backlight->priv->block = FALSE;

  if (id == XFPM_ALARM_ID_BRIGHTNESS_ON_AC && !backlight->priv->on_battery)
    xfpm_backlight_dim_brightness (backlight);
  else if (id == XFPM_ALARM_ID_BRIGHTNESS_ON_BATTERY && backlight->priv->on_battery)
    xfpm_backlight_dim_brightness (backlight);
}

static void
xfpm_backlight_reset_cb (XfpmIdle *idle,
                         XfpmBacklight *backlight)
{
  if (backlight->priv->dimmed)
  {
    if (!backlight->priv->block)
    {
      XFPM_DEBUG ("Alarm reset, setting level to %d", backlight->priv->last_level);
      xfpm_brightness_set_level (backlight->priv->brightness, backlight->priv->last_level);
    }
    backlight->priv->dimmed = FALSE;
  }
}

/* applies the step count and the minimum level of the brightness keys and the
 * panel slider; the step count goes first, the default minimum depends on it */
static void
xfpm_backlight_update_levels (XfpmBacklight *backlight)
{
  guint brightness_step_count;
  gboolean brightness_exponential;
  gint brightness_min_level;

  g_object_get (G_OBJECT (backlight->priv->conf),
                BRIGHTNESS_STEP_COUNT, &brightness_step_count,
                BRIGHTNESS_EXPONENTIAL, &brightness_exponential,
                BRIGHTNESS_SLIDER_MIN_LEVEL, &brightness_min_level,
                NULL);

  xfpm_brightness_set_step_count (backlight->priv->brightness,
                                  brightness_step_count,
                                  brightness_exponential);
  xfpm_brightness_set_min_level (backlight->priv->brightness, brightness_min_level);
}

/**
 * This callback is responsible for two functionalities in response to a user
 * pressing the brightness key:
 *
 *   - changing screen brightness;
 *   - displaying a notification with the updated brightness.
 *
 * It is possible for these functions to be enabled independently; this would
 * be desirable if, for example, a user has another tool configured to manage
 * the backlight brightness which does not provide any visual feedback.
 */
static void
xfpm_backlight_button_pressed_cb (XfpmButton *button,
                                  XfpmButtonKey type,
                                  XfpmBacklight *backlight)
{
  gint32 level;
  gboolean handle_brightness_keys;

  XFPM_DEBUG_ENUM (type, XFPM_TYPE_BUTTON_KEY, "Received button press event");

  /* this check is required; we are notified about keys used by other functions
   * e.g. keyboard brightness, sleep key. */
  if (type != BUTTON_MON_BRIGHTNESS_UP && type != BUTTON_MON_BRIGHTNESS_DOWN)
    return;

  g_object_get (G_OBJECT (backlight->priv->conf),
                HANDLE_BRIGHTNESS_KEYS, &handle_brightness_keys,
                NULL);

  backlight->priv->block = TRUE;

  /* optionally, handle updating the level and setting the screen brightness */
  if (handle_brightness_keys)
  {
    xfpm_backlight_update_levels (backlight);
    if (type == BUTTON_MON_BRIGHTNESS_UP)
    {
      xfpm_brightness_increase (backlight->priv->brightness);
    }
    else
    {
      xfpm_brightness_decrease (backlight->priv->brightness);
    }
  }

  /* show the result in a popup (even if it did not change) */
  if (xfpm_brightness_get_level (backlight->priv->brightness, &level))
    xfpm_backlight_show (backlight, level);
}

static void
xfpm_backlight_handle_brightness_keys_changed (XfpmBacklight *backlight)
{
  gboolean handle_keys;

  g_object_get (G_OBJECT (backlight->priv->conf),
                HANDLE_BRIGHTNESS_KEYS, &handle_keys,
                NULL);

  XFPM_DEBUG ("handle_brightness_keys changed to %s", handle_keys ? "TRUE" : "FALSE");

  xfpm_button_set_handle_brightness_keys (backlight->priv->button, handle_keys);
}

static void
xfpm_backlight_brightness_on_ac_settings_changed (XfpmBacklight *backlight)
{
  guint timeout_on_ac;

  g_object_get (G_OBJECT (backlight->priv->conf),
                BRIGHTNESS_ON_AC, &timeout_on_ac,
                NULL);

  XFPM_DEBUG ("Alarm on ac timeout changed %u", timeout_on_ac);

  if (timeout_on_ac == MIN_BRIGHTNESS_ON_AC)
  {
    xfpm_idle_alarm_remove (backlight->priv->idle, XFPM_ALARM_ID_BRIGHTNESS_ON_AC);
  }
  else
  {
    xfpm_idle_alarm_add (backlight->priv->idle, XFPM_ALARM_ID_BRIGHTNESS_ON_AC, timeout_on_ac * 1000);
  }
}

static void
xfpm_backlight_brightness_on_battery_settings_changed (XfpmBacklight *backlight)
{
  guint timeout_on_battery;

  g_object_get (G_OBJECT (backlight->priv->conf),
                BRIGHTNESS_ON_BATTERY, &timeout_on_battery,
                NULL);

  XFPM_DEBUG ("Alarm on battery timeout changed %u", timeout_on_battery);

  if (timeout_on_battery == MIN_BRIGHTNESS_ON_BATTERY)
  {
    xfpm_idle_alarm_remove (backlight->priv->idle, XFPM_ALARM_ID_BRIGHTNESS_ON_BATTERY);
  }
  else
  {
    xfpm_idle_alarm_add (backlight->priv->idle, XFPM_ALARM_ID_BRIGHTNESS_ON_BATTERY, timeout_on_battery * 1000);
  }
}


static void
xfpm_backlight_on_battery_changed_cb (XfpmPower *power,
                                      gboolean on_battery,
                                      XfpmBacklight *backlight)
{
  backlight->priv->on_battery = on_battery;
}

static void
xfpm_backlight_class_init (XfpmBacklightClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = xfpm_backlight_get_property;
  object_class->set_property = xfpm_backlight_set_property;
  object_class->finalize = xfpm_backlight_finalize;

  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_SWITCH,
                                   g_param_spec_int (BRIGHTNESS_SWITCH,
                                                     NULL, NULL,
                                                     MIN_BRIGHTNESS_SWITCH, MAX_BRIGHTNESS_SWITCH, DEFAULT_BRIGHTNESS_SWITCH,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_SWITCH_SAVE,
                                   g_param_spec_int (BRIGHTNESS_SWITCH_RESTORE_ON_EXIT,
                                                     NULL, NULL,
                                                     MIN_BRIGHTNESS_SWITCH, MAX_BRIGHTNESS_SWITCH, DEFAULT_BRIGHTNESS_SWITCH_RESTORE_ON_EXIT,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
xfpm_backlight_init (XfpmBacklight *backlight)
{
  backlight->priv = xfpm_backlight_get_instance_private (backlight);

  backlight->priv->brightness = xfpm_brightness_new ();

  backlight->priv->idle = NULL;
  backlight->priv->conf = NULL;
  backlight->priv->button = NULL;
  backlight->priv->power = NULL;
  backlight->priv->notify = NULL;
  backlight->priv->brightness_notification = NULL;
  backlight->priv->dimmed = FALSE;
  backlight->priv->block = FALSE;
  backlight->priv->brightness_switch_initialized = FALSE;

  if (backlight->priv->brightness != NULL)
  {
    gboolean handle_keys;

    backlight->priv->idle = xfpm_idle_new ();
    backlight->priv->conf = xfpm_xfconf_new ();
    backlight->priv->button = xfpm_button_new ();
    backlight->priv->power = xfpm_power_get ();
    backlight->priv->notify = xfpm_notify_new ();
    backlight->priv->brightness_switch = DEFAULT_BRIGHTNESS_SWITCH;

    xfconf_g_property_bind (xfpm_xfconf_get_channel (backlight->priv->conf),
                            XFPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH, G_TYPE_INT,
                            G_OBJECT (backlight), BRIGHTNESS_SWITCH);

    xfpm_brightness_get_switch (backlight->priv->brightness, &backlight->priv->brightness_switch);
    backlight->priv->brightness_switch_initialized = TRUE;

    /*
     * If power manager has crashed last time, the brightness switch
     * saved value will be set to the original value. In that case, we
     * will use this saved value instead of the one found at the
     * current startup so the setting is restored properly.
     */
    backlight->priv->brightness_switch_save =
      xfconf_channel_get_int (xfpm_xfconf_get_channel (backlight->priv->conf),
                              XFPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH_RESTORE_ON_EXIT,
                              DEFAULT_BRIGHTNESS_SWITCH_RESTORE_ON_EXIT);

    if (backlight->priv->brightness_switch_save == MIN_BRIGHTNESS_SWITCH)
    {
      if (!xfconf_channel_set_int (xfpm_xfconf_get_channel (backlight->priv->conf),
                                   XFPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH_RESTORE_ON_EXIT,
                                   backlight->priv->brightness_switch))
        g_critical ("Cannot set value for property %s", BRIGHTNESS_SWITCH_RESTORE_ON_EXIT);

      backlight->priv->brightness_switch_save = backlight->priv->brightness_switch;
    }
    else
    {
      XFPM_DEBUG ("It seems the kernel brightness switch handling value was "
                  "not restored properly on exit last time, xfce4-power-manager "
                  "will try to restore it this time.");
    }

    /* check whether to change the brightness switch */
    handle_keys = xfconf_channel_get_bool (xfpm_xfconf_get_channel (backlight->priv->conf),
                                           XFPM_PROPERTIES_PREFIX HANDLE_BRIGHTNESS_KEYS,
                                           DEFAULT_HANDLE_BRIGHTNESS_KEYS);
    backlight->priv->brightness_switch = handle_keys ? 0 : 1;
    g_object_set (G_OBJECT (backlight),
                  BRIGHTNESS_SWITCH,
                  backlight->priv->brightness_switch,
                  NULL);
    xfpm_button_set_handle_brightness_keys (backlight->priv->button, handle_keys);
    g_signal_connect_object (backlight->priv->conf, "notify::" HANDLE_BRIGHTNESS_KEYS,
                             G_CALLBACK (xfpm_backlight_handle_brightness_keys_changed),
                             backlight, G_CONNECT_SWAPPED);

    if (backlight->priv->idle != NULL)
    {
      g_signal_connect_object (backlight->priv->idle, "alarm-expired",
                               G_CALLBACK (xfpm_backlight_alarm_timeout_cb), backlight, G_CONNECT_DEFAULT);
      g_signal_connect_object (backlight->priv->idle, "reset",
                               G_CALLBACK (xfpm_backlight_reset_cb), backlight, G_CONNECT_DEFAULT);
      g_signal_connect_object (backlight->priv->conf, "notify::" BRIGHTNESS_ON_AC,
                               G_CALLBACK (xfpm_backlight_brightness_on_ac_settings_changed),
                               backlight, G_CONNECT_SWAPPED);
      g_signal_connect_object (backlight->priv->conf, "notify::" BRIGHTNESS_ON_BATTERY,
                               G_CALLBACK (xfpm_backlight_brightness_on_battery_settings_changed),
                               backlight, G_CONNECT_SWAPPED);
      xfpm_backlight_brightness_on_ac_settings_changed (backlight);
      xfpm_backlight_brightness_on_battery_settings_changed (backlight);
    }
    g_signal_connect_object (backlight->priv->button, "button-pressed",
                             G_CALLBACK (xfpm_backlight_button_pressed_cb), backlight, G_CONNECT_DEFAULT);
    g_signal_connect_object (backlight->priv->power, "on-battery-changed",
                             G_CALLBACK (xfpm_backlight_on_battery_changed_cb), backlight, G_CONNECT_DEFAULT);

    g_object_get (G_OBJECT (backlight->priv->power),
                  "on-battery", &backlight->priv->on_battery,
                  NULL);
    xfpm_brightness_get_level (backlight->priv->brightness, &backlight->priv->last_level);
  }
}

static void
xfpm_backlight_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
  XfpmBacklight *backlight = XFPM_BACKLIGHT (object);

  switch (prop_id)
  {
    case PROP_BRIGHTNESS_SWITCH:
      g_value_set_int (value, backlight->priv->brightness_switch);
      break;
    case PROP_BRIGHTNESS_SWITCH_SAVE:
      g_value_set_int (value, backlight->priv->brightness_switch_save);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
xfpm_backlight_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
  XfpmBacklight *backlight = XFPM_BACKLIGHT (object);
  gboolean ret;

  switch (prop_id)
  {
    case PROP_BRIGHTNESS_SWITCH:
      backlight->priv->brightness_switch = g_value_get_int (value);
      if (!backlight->priv->brightness_switch_initialized)
        break;
      ret = xfpm_brightness_set_switch (backlight->priv->brightness,
                                        backlight->priv->brightness_switch);
      if (!ret)
        XFPM_DEBUG ("Unable to set the kernel brightness switch parameter to %d.",
                    backlight->priv->brightness_switch);
      else
        XFPM_DEBUG ("Set kernel brightness switch to %d",
                    backlight->priv->brightness_switch);

      break;
    case PROP_BRIGHTNESS_SWITCH_SAVE:
      backlight->priv->brightness_switch_save = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
xfpm_backlight_finalize (GObject *object)
{
  XfpmBacklight *backlight;

  backlight = XFPM_BACKLIGHT (object);

  if (backlight->priv->idle)
    g_object_unref (backlight->priv->idle);

  if (backlight->priv->conf)
  {
    /* restore video module brightness switch setting */
    if (backlight->priv->brightness_switch_save != MIN_BRIGHTNESS_SWITCH)
    {
      gboolean ret = xfpm_brightness_set_switch (backlight->priv->brightness,
                                                 backlight->priv->brightness_switch_save);
      /* unset the xfconf saved value after the restore */
      if (!xfconf_channel_set_int (xfpm_xfconf_get_channel (backlight->priv->conf),
                                   XFPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH_RESTORE_ON_EXIT, MIN_BRIGHTNESS_SWITCH))
        g_critical ("Cannot set value for property %s", BRIGHTNESS_SWITCH_RESTORE_ON_EXIT);

      if (ret)
      {
        backlight->priv->brightness_switch = backlight->priv->brightness_switch_save;
        XFPM_DEBUG ("Restored brightness switch value to: %d", backlight->priv->brightness_switch);
      }
      else
        XFPM_DEBUG ("Unable to restore the kernel brightness switch parameter to its original value, "
                    "still resetting the saved value.");
    }
    g_object_unref (backlight->priv->conf);
  }

  if (backlight->priv->brightness)
    g_object_unref (backlight->priv->brightness);

  if (backlight->priv->button)
    g_object_unref (backlight->priv->button);

  if (backlight->priv->power)
    g_object_unref (backlight->priv->power);

  if (backlight->priv->notify)
    g_object_unref (backlight->priv->notify);

  if (backlight->priv->brightness_notification)
    g_object_unref (backlight->priv->brightness_notification);

  G_OBJECT_CLASS (xfpm_backlight_parent_class)->finalize (object);
}

XfpmBacklight *
xfpm_backlight_new (void)
{
  XfpmBacklight *backlight = g_object_new (XFPM_TYPE_BACKLIGHT, NULL);
  if (backlight->priv->brightness == NULL)
  {
    g_object_unref (backlight);
    return NULL;
  }

  return backlight;
}

/**
 * xfpm_backlight_get_percent:
 * @backlight: a #XfpmBacklight
 * @percent: (out): the brightness in percent of the range from the minimum
 *   to the maximum level
 *
 * While the screen is dimmed because the user is idle, gives the level that is
 * set when the user comes back.
 *
 * Returns: %TRUE on success
 **/
gboolean
xfpm_backlight_get_percent (XfpmBacklight *backlight,
                            gint *percent)
{
  gint32 level;
  gint32 min_level;
  gint32 max_level;

  g_return_val_if_fail (XFPM_IS_BACKLIGHT (backlight), FALSE);
  g_return_val_if_fail (percent != NULL, FALSE);

  xfpm_backlight_update_levels (backlight);
  min_level = xfpm_brightness_get_min_level (backlight->priv->brightness);
  max_level = xfpm_brightness_get_max_level (backlight->priv->brightness);

  /* while dimmed, the level the user comes back to */
  if (backlight->priv->dimmed)
    level = backlight->priv->last_level;
  else if (!xfpm_brightness_get_level (backlight->priv->brightness, &level))
    return FALSE;

  *percent = 100 * (level - min_level) / (max_level - min_level);
  return TRUE;
}

/**
 * xfpm_backlight_set_percent:
 * @backlight: a #XfpmBacklight
 * @percent: the brightness in percent of the range from the minimum to the
 *   maximum level, 0-100
 *
 * Sets the brightness without a notification, for other programs such as a
 * schedule. 0 is the minimum level the brightness keys and the panel slider
 * use. While the screen is dimmed because the user is idle, the level is only
 * remembered and set when the user comes back.
 *
 * Returns: %TRUE on success
 **/
gboolean
xfpm_backlight_set_percent (XfpmBacklight *backlight,
                            gint percent)
{
  gint32 min_level;
  gint32 max_level;
  gint32 level;

  g_return_val_if_fail (XFPM_IS_BACKLIGHT (backlight), FALSE);
  g_return_val_if_fail (percent >= 0 && percent <= 100, FALSE);

  xfpm_backlight_update_levels (backlight);

  min_level = xfpm_brightness_get_min_level (backlight->priv->brightness);
  max_level = xfpm_brightness_get_max_level (backlight->priv->brightness);
  level = min_level + percent * (max_level - min_level) / 100;

  XFPM_DEBUG ("Brightness %d%% requested, level %d%s", percent, level,
              backlight->priv->dimmed ? " (dimmed, set on activity)" : "");

  if (backlight->priv->dimmed)
  {
    backlight->priv->last_level = level;
    return TRUE;
  }

  return xfpm_brightness_set_level (backlight->priv->brightness, level);
}
