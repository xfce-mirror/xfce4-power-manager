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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xfpm-backlight.h"
#include "xfpm-button.h"
#include "xfpm-idle.h"
#include "xfpm-notify.h"
#include "xfpm-power.h"
#include "xfpm-xfconf.h"

#include "common/xfpm-brightness.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-enum-types.h"
#include "common/xfpm-icons.h"

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

  NotifyNotification *n;

  gboolean on_battery;

  gint32 last_level;
  gint32 max_level;

  gint brightness_switch;
  gint brightness_switch_save;
  gboolean brightness_switch_initialized;

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
  gboolean ret;

  if (!xfpm_power_is_inhibited (backlight->priv->power))
  {
    gint32 dim_level;

    g_object_get (G_OBJECT (backlight->priv->conf),
                  backlight->priv->on_battery ? BRIGHTNESS_LEVEL_ON_BATTERY : BRIGHTNESS_LEVEL_ON_AC, &dim_level,
                  NULL);

    ret = xfpm_brightness_get_level (backlight->priv->brightness, &backlight->priv->last_level);

    if (!ret)
    {
      XFPM_DEBUG ("Unable to get current brightness level");
      return;
    }

    dim_level = dim_level * backlight->priv->max_level / 100;

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

static gboolean
xfpm_backlight_destroy_popup (gpointer data)
{
  XfpmBacklight *backlight;

  backlight = XFPM_BACKLIGHT (data);

  if (backlight->priv->n)
  {
    g_object_unref (backlight->priv->n);
    backlight->priv->n = NULL;
  }

  return FALSE;
}

static void
xfpm_backlight_show_notification (XfpmBacklight *backlight,
                                  gfloat value)
{
  gchar *summary;
  /* generate a human-readable summary for the notification */
  summary = g_strdup_printf (_("Brightness: %.0f percent"), value);

  /* create the notification on demand */
  if (backlight->priv->n == NULL)
  {
    backlight->priv->n = xfpm_notify_new_notification (backlight->priv->notify,
                                                       _("Power Manager"),
                                                       summary,
                                                       XFPM_DISPLAY_BRIGHTNESS_ICON,
                                                       XFPM_NOTIFY_NORMAL);
  }
  else
  {
    notify_notification_update (backlight->priv->n,
                                _("Power Manager"),
                                summary,
                                XFPM_DISPLAY_BRIGHTNESS_ICON);
  }
  g_free (summary);

  /* add the brightness value to the notification */
  notify_notification_set_hint (backlight->priv->n, "value", g_variant_new_int32 (value));

  /* show the notification */
  notify_notification_show (backlight->priv->n, NULL);
}

static void
xfpm_backlight_show (XfpmBacklight *backlight, gint level)
{
  gfloat value;

  XFPM_DEBUG ("Level %u", level);

  value = (gfloat) 100 * level / backlight->priv->max_level;
  xfpm_backlight_show_notification (backlight, value);
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
  gboolean ret = TRUE;
  gboolean handle_brightness_keys, show_popup;
  guint brightness_step_count;
  gboolean brightness_exponential;

  XFPM_DEBUG_ENUM (type, XFPM_TYPE_BUTTON_KEY, "Received button press event");

  /* this check is required; we are notified about keys used by other functions
   * e.g. keyboard brightness, sleep key. */
  if (type != BUTTON_MON_BRIGHTNESS_UP && type != BUTTON_MON_BRIGHTNESS_DOWN)
    return;

  g_object_get (G_OBJECT (backlight->priv->conf),
                HANDLE_BRIGHTNESS_KEYS, &handle_brightness_keys,
                SHOW_BRIGHTNESS_POPUP, &show_popup,
                BRIGHTNESS_STEP_COUNT, &brightness_step_count,
                BRIGHTNESS_EXPONENTIAL, &brightness_exponential,
                NULL);

  backlight->priv->block = TRUE;

  /* optionally, handle updating the level and setting the screen brightness */
  if (handle_brightness_keys)
  {
    xfpm_brightness_set_step_count (backlight->priv->brightness,
                                    brightness_step_count,
                                    brightness_exponential);
    if (type == BUTTON_MON_BRIGHTNESS_UP)
    {
      xfpm_brightness_increase (backlight->priv->brightness);
    }
    else
    {
      xfpm_brightness_decrease (backlight->priv->brightness);
    }
  }

  /* get the current brightness level */
  ret = xfpm_brightness_get_level (backlight->priv->brightness, &level);

  /* optionally, show the result in a popup (even if it did not change) */
  if (ret && show_popup)
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
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_SWITCH_SAVE,
                                   g_param_spec_int (BRIGHTNESS_SWITCH_RESTORE_ON_EXIT,
                                                     NULL, NULL,
                                                     MIN_BRIGHTNESS_SWITCH, MAX_BRIGHTNESS_SWITCH, DEFAULT_BRIGHTNESS_SWITCH_RESTORE_ON_EXIT,
                                                     G_PARAM_READWRITE));
}

static void
xfpm_backlight_init (XfpmBacklight *backlight)
{
  backlight->priv = xfpm_backlight_get_instance_private (backlight);

  backlight->priv->brightness = xfpm_brightness_new ();

  backlight->priv->notify = NULL;
  backlight->priv->idle = NULL;
  backlight->priv->conf = NULL;
  backlight->priv->button = NULL;
  backlight->priv->power = NULL;
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
    backlight->priv->max_level = xfpm_brightness_get_max_level (backlight->priv->brightness);
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
                               G_CALLBACK (xfpm_backlight_alarm_timeout_cb), backlight, 0);
      g_signal_connect_object (backlight->priv->idle, "reset",
                               G_CALLBACK (xfpm_backlight_reset_cb), backlight, 0);
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
                             G_CALLBACK (xfpm_backlight_button_pressed_cb), backlight, 0);
    g_signal_connect_object (backlight->priv->power, "on-battery-changed",
                             G_CALLBACK (xfpm_backlight_on_battery_changed_cb), backlight, 0);

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

  xfpm_backlight_destroy_popup (backlight);

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
