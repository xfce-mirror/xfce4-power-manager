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

#include "xfpm-xfconf.h"

#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-enum-glib.h"
#include "common/xfpm-enum-types.h"
#include "common/xfpm-enum.h"

#include <libxfce4util/libxfce4util.h>

static void
xfpm_xfconf_finalize (GObject *object);

struct XfpmXfconfPrivate
{
  XfconfChannel *channel;
  GValue *values;
};

enum
{
  PROP_0,
  PROP_GENERAL_NOTIFICATION,
  PROP_LOCK_SCREEN_ON_SLEEP,
  PROP_CRITICAL_LEVEL,
  PROP_SHOW_BRIGHTNESS_POPUP,
  PROP_HANDLE_BRIGHTNESS_KEYS,
  PROP_BRIGHTNESS_STEP_COUNT,
  PROP_BRIGHTNESS_EXPONENTIAL,
  PROP_TRAY_ICON,
  PROP_CRITICAL_BATTERY_ACTION,
  PROP_POWER_BUTTON,
  PROP_HIBERNATE_BUTTON,
  PROP_SLEEP_BUTTON,
  PROP_BATTERY_BUTTON,
  PROP_LID_ACTION_ON_AC,
  PROP_LID_ACTION_ON_BATTERY,
  PROP_BRIGHTNESS_LEVEL_ON_AC,
  PROP_BRIGHTNESS_LEVEL_ON_BATTERY,
  PROP_BRIGHTNESS_SLIDER_MIN_LEVEL,

  PROP_ENABLE_DPMS,
  PROP_DPMS_SLEEP_ON_AC,
  PROP_DPMS_OFF_ON_AC,
  PROP_DPMS_SLEEP_ON_BATTERY,
  PROP_DPMS_OFF_ON_BATTERY,
  PROP_DPMS_SLEEP_MODE,

  PROP_IDLE_ON_AC,
  PROP_IDLE_ON_BATTERY,
  PROP_IDLE_SLEEP_MODE_ON_AC,
  PROP_IDLE_SLEEP_MODE_ON_BATTERY,
  PROP_DIM_ON_AC_TIMEOUT,
  PROP_DIM_ON_BATTERY_TIMEOUT,
  PROP_LOGIND_HANDLE_POWER_KEY,
  PROP_LOGIND_HANDLE_SUSPEND_KEY,
  PROP_LOGIND_HANDLE_HIBERNATE_KEY,
  PROP_LOGIND_HANDLE_LID_SWITCH,
  PROP_HEARTBEAT_COMMAND,

  PROP_PROFILE_ON_AC,
  PROP_PROFILE_ON_BATTERY,

  PROP_SHOW_PANEL_LABEL,
  PROP_SHOW_PRESENTATION_INDICATOR,

  N_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (XfpmXfconf, xfpm_xfconf, G_TYPE_OBJECT)

static void
xfpm_xfconf_set_property (GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
  XfpmXfconf *conf = XFPM_XFCONF (object);
  GValue *dst = conf->priv->values + prop_id;

  if (!G_IS_VALUE (dst))
  {
    g_value_init (dst, pspec->value_type);
    g_param_value_set_default (pspec, dst);
  }

  if (g_param_values_cmp (pspec, value, dst) != 0)
  {
    g_value_copy (value, dst);
    g_object_notify (object, pspec->name);
  }
}

static void
xfpm_xfconf_get_property (GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
  XfpmXfconf *conf = XFPM_XFCONF (object);
  GValue *src = conf->priv->values + prop_id;

  if (G_VALUE_HOLDS (src, pspec->value_type))
    g_value_copy (src, value);
  else
    g_param_value_set_default (pspec, value);
}

static void
xfpm_xfconf_load (XfpmXfconf *conf)
{
  guint nspecs;
  GParamSpec **specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (conf), &nspecs);
  GValue value = G_VALUE_INIT;

  for (guint i = 0; i < nspecs; i++)
  {
    gchar *prop_name;
    prop_name = g_strdup_printf ("%s%s", XFPM_PROPERTIES_PREFIX, specs[i]->name);
    g_value_init (&value, specs[i]->value_type);

    if (conf->priv->channel != NULL)
    {
      if (!xfconf_channel_get_property (conf->priv->channel, prop_name, &value))
      {
        XFPM_DEBUG ("Using default configuration for %s", specs[i]->name);
        g_param_value_set_default (specs[i], &value);
      }
    }
    else
    {
      XFPM_DEBUG ("Using default configuration for %s", specs[i]->name);
      g_param_value_set_default (specs[i], &value);
    }
    g_free (prop_name);
    g_object_set_property (G_OBJECT (conf), specs[i]->name, &value);
    g_value_unset (&value);
  }

  g_free (specs);
}

static void
xfpm_xfconf_property_changed_cb (XfconfChannel *channel,
                                 gchar *property,
                                 GValue *value,
                                 XfpmXfconf *conf)
{
  /*FIXME: Set default for this key*/
  if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
    return;

  if (!g_str_has_prefix (property, XFPM_PROPERTIES_PREFIX)
      || strlen (property) <= strlen (XFPM_PROPERTIES_PREFIX))
    return;

  /* We handle presentation mode in xfpm-power directly */
  if (g_strcmp0 (property, XFPM_PROPERTIES_PREFIX PRESENTATION_MODE) == 0)
    return;

  /* We handle brightness switch in xfpm-backlight directly */
  if (g_strcmp0 (property, XFPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH) == 0
      || g_strcmp0 (property, XFPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH_RESTORE_ON_EXIT) == 0)
    return;

  XFPM_DEBUG ("Property modified: %s\n", property);

  g_object_set_property (G_OBJECT (conf), property + strlen (XFPM_PROPERTIES_PREFIX), value);
}

static void
xfpm_xfconf_class_init (XfpmXfconfClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = xfpm_xfconf_set_property;
  object_class->get_property = xfpm_xfconf_get_property;
  object_class->finalize = xfpm_xfconf_finalize;

  /**
   * XfpmXfconf::general-notification
   **/
  g_object_class_install_property (object_class,
                                   PROP_GENERAL_NOTIFICATION,
                                   g_param_spec_boolean (GENERAL_NOTIFICATION,
                                                         NULL, NULL,
                                                         DEFAULT_GENERAL_NOTIFICATION,
                                                         G_PARAM_READWRITE));
  /**
   * XfpmXfconf::lock-screen-suspend-hibernate
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOCK_SCREEN_ON_SLEEP,
                                   g_param_spec_boolean (LOCK_SCREEN_SUSPEND_HIBERNATE,
                                                         NULL, NULL,
                                                         DEFAULT_LOCK_SCREEN_SUSPEND_HIBERNATE,
                                                         G_PARAM_READWRITE));

  /**
   * XfpmXfconf::critical-power-level
   **/
  g_object_class_install_property (object_class,
                                   PROP_CRITICAL_LEVEL,
                                   g_param_spec_uint (CRITICAL_POWER_LEVEL,
                                                      NULL, NULL,
                                                      MIN_CRITICAL_POWER_LEVEL,
                                                      MAX_CRITICAL_POWER_LEVEL,
                                                      DEFAULT_CRITICAL_POWER_LEVEL,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::show-brightness-popup
   **/
  g_object_class_install_property (object_class,
                                   PROP_SHOW_BRIGHTNESS_POPUP,
                                   g_param_spec_boolean (SHOW_BRIGHTNESS_POPUP,
                                                         NULL, NULL,
                                                         DEFAULT_SHOW_BRIGHTNESS_POPUP,
                                                         G_PARAM_READWRITE));

  /**
   * XfpmXfconf::handle-brightness-keys
   **/
  g_object_class_install_property (object_class,
                                   PROP_HANDLE_BRIGHTNESS_KEYS,
                                   g_param_spec_boolean (HANDLE_BRIGHTNESS_KEYS,
                                                         NULL, NULL,
                                                         DEFAULT_HANDLE_BRIGHTNESS_KEYS,
                                                         G_PARAM_READWRITE));

  /**
   * XfpmXfconf::brightness-step-count
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_STEP_COUNT,
                                   g_param_spec_uint (BRIGHTNESS_STEP_COUNT,
                                                      NULL, NULL,
                                                      MIN_BRIGHTNESS_STEP_COUNT,
                                                      MAX_BRIGHTNESS_STEP_COUNT,
                                                      DEFAULT_BRIGHTNESS_STEP_COUNT,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::brightness-exponential
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_EXPONENTIAL,
                                   g_param_spec_boolean (BRIGHTNESS_EXPONENTIAL,
                                                         NULL, NULL,
                                                         DEFAULT_BRIGHTNESS_EXPONENTIAL,
                                                         G_PARAM_READWRITE));

  /**
   * XfpmXfconf::show-tray-icon
   **/
  g_object_class_install_property (object_class,
                                   PROP_TRAY_ICON,
                                   g_param_spec_uint (SHOW_TRAY_ICON,
                                                      NULL, NULL,
                                                      0,
                                                      N_SHOW_ICONS - 1,
                                                      DEFAULT_SHOW_TRAY_ICON,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::critical-battery-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_CRITICAL_BATTERY_ACTION,
                                   g_param_spec_uint (CRITICAL_POWER_ACTION,
                                                      NULL, NULL,
                                                      0,
                                                      N_XFPM_SHUTDOWN_REQUESTS - 1,
                                                      XFPM_DO_NOTHING,
                                                      G_PARAM_READWRITE));
  /**
   * XfpmXfconf::power-switch-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_POWER_BUTTON,
                                   g_param_spec_uint (POWER_BUTTON_ACTION,
                                                      NULL, NULL,
                                                      0,
                                                      N_XFPM_SHUTDOWN_REQUESTS - 1,
                                                      DEFAULT_POWER_BUTTON_ACTION,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::sleep-switch-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_SLEEP_BUTTON,
                                   g_param_spec_uint (SLEEP_BUTTON_ACTION,
                                                      NULL, NULL,
                                                      0,
                                                      N_XFPM_SHUTDOWN_REQUESTS - 1,
                                                      DEFAULT_SLEEP_BUTTON_ACTION,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::hibernate-switch-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_HIBERNATE_BUTTON,
                                   g_param_spec_uint (HIBERNATE_BUTTON_ACTION,
                                                      NULL, NULL,
                                                      0,
                                                      N_XFPM_SHUTDOWN_REQUESTS - 1,
                                                      DEFAULT_HIBERNATE_BUTTON_ACTION,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::battery-switch-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_BATTERY_BUTTON,
                                   g_param_spec_uint (BATTERY_BUTTON_ACTION,
                                                      NULL, NULL,
                                                      0,
                                                      N_XFPM_SHUTDOWN_REQUESTS - 1,
                                                      DEFAULT_BATTERY_BUTTON_ACTION,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::lid-action-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_LID_ACTION_ON_AC,
                                   g_param_spec_uint (LID_ACTION_ON_AC,
                                                      NULL, NULL,
                                                      0,
                                                      N_LID_TRIGGERS - 1,
                                                      DEFAULT_LID_ACTION_ON_AC,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::brightness-level-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_LEVEL_ON_AC,
                                   g_param_spec_uint (BRIGHTNESS_LEVEL_ON_AC,
                                                      NULL, NULL,
                                                      1,
                                                      100,
                                                      DEFAULT_BRIGHTNESS_LEVEL_ON_AC,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::brightness-level-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_LEVEL_ON_BATTERY,
                                   g_param_spec_uint (BRIGHTNESS_LEVEL_ON_BATTERY,
                                                      NULL, NULL,
                                                      1,
                                                      100,
                                                      DEFAULT_BRIGHTNESS_LEVEL_ON_BATTERY,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::lid-action-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_LID_ACTION_ON_BATTERY,
                                   g_param_spec_uint (LID_ACTION_ON_BATTERY,
                                                      NULL, NULL,
                                                      0,
                                                      N_LID_TRIGGERS - 1,
                                                      DEFAULT_LID_ACTION_ON_BATTERY,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::dpms-enabled
   **/
  g_object_class_install_property (object_class,
                                   PROP_ENABLE_DPMS,
                                   g_param_spec_boolean (DPMS_ENABLED,
                                                         NULL, NULL,
                                                         DEFAULT_DPMS_ENABLED,
                                                         G_PARAM_READWRITE));
  /**
   * XfpmXfconf::dpms-on-ac-sleep
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_SLEEP_ON_AC,
                                   g_param_spec_uint (DPMS_ON_AC_SLEEP,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT,
                                                      DEFAULT_DPMS_ON_AC_SLEEP,
                                                      G_PARAM_READWRITE));
  /**
   * XfpmXfconf::dpms-on-ac-off
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_OFF_ON_AC,
                                   g_param_spec_uint (DPMS_ON_AC_OFF,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT,
                                                      DEFAULT_DPMS_ON_AC_OFF,
                                                      G_PARAM_READWRITE));
  /**
   * XfpmXfconf::dpms-on-battery-sleep
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_SLEEP_ON_BATTERY,
                                   g_param_spec_uint (DPMS_ON_BATTERY_SLEEP,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT,
                                                      DEFAULT_DPMS_ON_BATTERY_SLEEP,
                                                      G_PARAM_READWRITE));
  /**
   * XfpmXfconf::dpms-on-battery-off
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_OFF_ON_BATTERY,
                                   g_param_spec_uint (DPMS_ON_BATTERY_OFF,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT,
                                                      DEFAULT_DPMS_ON_BATTERY_OFF,
                                                      G_PARAM_READWRITE));
  /**
   * XfpmXfconf::dpms-sleep-mode
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_SLEEP_MODE,
                                   g_param_spec_string (DPMS_SLEEP_MODE,
                                                        NULL, NULL,
                                                        DEFAULT_DPMS_SLEEP_MODE,
                                                        G_PARAM_READWRITE));

  /**
   * XfpmXfconf::inactivity-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_IDLE_ON_AC,
                                   g_param_spec_uint (INACTIVITY_ON_AC,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT,
                                                      DEFAULT_INACTIVITY_ON_AC,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::inactivity-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_IDLE_ON_BATTERY,
                                   g_param_spec_uint (INACTIVITY_ON_BATTERY,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT,
                                                      DEFAULT_INACTIVITY_ON_BATTERY,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::inactivity-sleep-mode-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_IDLE_SLEEP_MODE_ON_BATTERY,
                                   g_param_spec_uint (INACTIVITY_SLEEP_MODE_ON_BATTERY,
                                                      NULL, NULL,
                                                      XFPM_DO_SUSPEND,
                                                      XFPM_DO_HIBERNATE,
                                                      DEFAULT_INACTIVITY_SLEEP_MODE_ON_BATTERY,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::inactivity-sleep-mode-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_IDLE_SLEEP_MODE_ON_AC,
                                   g_param_spec_uint (INACTIVITY_SLEEP_MODE_ON_AC,
                                                      NULL, NULL,
                                                      XFPM_DO_SUSPEND,
                                                      XFPM_DO_HIBERNATE,
                                                      DEFAULT_INACTIVITY_SLEEP_MODE_ON_AC,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::brightness-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_DIM_ON_AC_TIMEOUT,
                                   g_param_spec_uint (BRIGHTNESS_ON_AC,
                                                      NULL, NULL,
                                                      MIN_BRIGHTNESS_ON_AC,
                                                      G_MAXUINT,
                                                      DEFAULT_BRIGHTNESS_ON_AC,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::brightness-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_DIM_ON_BATTERY_TIMEOUT,
                                   g_param_spec_uint (BRIGHTNESS_ON_BATTERY,
                                                      NULL, NULL,
                                                      MIN_BRIGHTNESS_ON_BATTERY,
                                                      G_MAXUINT,
                                                      DEFAULT_BRIGHTNESS_ON_BATTERY,
                                                      G_PARAM_READWRITE));

  /**
   * XfpmXfconf::profile-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_PROFILE_ON_AC,
                                   g_param_spec_string (PROFILE_ON_AC,
                                                        NULL, NULL,
                                                        DEFAULT_PROFILE_ON_AC,
                                                        G_PARAM_READWRITE));
  /**
   * XfpmXfconf::profile-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_PROFILE_ON_BATTERY,
                                   g_param_spec_string (PROFILE_ON_BATTERY,
                                                        NULL, NULL,
                                                        DEFAULT_PROFILE_ON_BATTERY,
                                                        G_PARAM_READWRITE));


  /**
   * XfpmXfconf::brightness-slider-min-level
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_SLIDER_MIN_LEVEL,
                                   g_param_spec_int (BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                     NULL, NULL,
                                                     MIN_BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                     G_MAXINT,
                                                     DEFAULT_BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                     G_PARAM_READWRITE));

  /**
   * XfpmXfconf::logind-handle-power-key
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOGIND_HANDLE_POWER_KEY,
                                   g_param_spec_boolean (LOGIND_HANDLE_POWER_KEY,
                                                         NULL, NULL,
                                                         DEFAULT_LOGIND_HANDLE_POWER_KEY,
                                                         G_PARAM_READWRITE));

  /**
   * XfpmXfconf::logind-handle-suspend-key
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOGIND_HANDLE_SUSPEND_KEY,
                                   g_param_spec_boolean (LOGIND_HANDLE_SUSPEND_KEY,
                                                         NULL, NULL,
                                                         DEFAULT_LOGIND_HANDLE_SUSPEND_KEY,
                                                         G_PARAM_READWRITE));

  /**
   * XfpmXfconf::logind-handle-hibernate-key
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOGIND_HANDLE_HIBERNATE_KEY,
                                   g_param_spec_boolean (LOGIND_HANDLE_HIBERNATE_KEY,
                                                         NULL, NULL,
                                                         DEFAULT_LOGIND_HANDLE_HIBERNATE_KEY,
                                                         G_PARAM_READWRITE));

  /**
   * XfpmXfconf::logind-handle-lid-switch
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOGIND_HANDLE_LID_SWITCH,
                                   g_param_spec_boolean (LOGIND_HANDLE_LID_SWITCH,
                                                         NULL, NULL,
                                                         DEFAULT_LOGIND_HANDLE_LID_SWITCH,
                                                         G_PARAM_READWRITE));

  /**
   * XfpmXfconf::heartbeat-command
   **/
  g_object_class_install_property (object_class,
                                   PROP_HEARTBEAT_COMMAND,
                                   g_param_spec_string (HEARTBEAT_COMMAND,
                                                        NULL, NULL,
                                                        DEFAULT_HEARTBEAT_COMMAND,
                                                        G_PARAM_READWRITE));

  /**
   * XfpmXfconf::show-panel-label
   **/
  g_object_class_install_property (object_class,
                                   PROP_SHOW_PANEL_LABEL,
                                   g_param_spec_int (SHOW_PANEL_LABEL,
                                                     NULL, NULL,
                                                     0,
                                                     N_PANEL_LABELS - 1,
                                                     DEFAULT_SHOW_PANEL_LABEL,
                                                     G_PARAM_READWRITE));

  /**
   * XfpmXfconf::show-presentation-indicator
   **/
  g_object_class_install_property (object_class,
                                   PROP_SHOW_PRESENTATION_INDICATOR,
                                   g_param_spec_boolean (SHOW_PRESENTATION_INDICATOR,
                                                         NULL, NULL,
                                                         DEFAULT_SHOW_PRESENTATION_INDICATOR,
                                                         G_PARAM_READWRITE));
}

static void
xfpm_xfconf_init (XfpmXfconf *conf)
{
  GError *error = NULL;

  conf->priv = xfpm_xfconf_get_instance_private (conf);
  conf->priv->values = g_new0 (GValue, N_PROPERTIES);

  if (!xfconf_init (&error))
  {
    if (error)
    {
      g_critical ("xfconf_init failed: %s", error->message);
      g_error_free (error);
    }
  }
  else
  {
    conf->priv->channel = xfconf_channel_get (XFPM_CHANNEL);
    g_signal_connect_object (conf->priv->channel, "property-changed",
                             G_CALLBACK (xfpm_xfconf_property_changed_cb), conf, 0);
  }
  xfpm_xfconf_load (conf);
}

static void
xfpm_xfconf_finalize (GObject *object)
{
  XfpmXfconf *conf = XFPM_XFCONF (object);

  for (guint i = 0; i < N_PROPERTIES; i++)
  {
    if (G_IS_VALUE (conf->priv->values + i))
      g_value_unset (conf->priv->values + i);
  }

  g_free (conf->priv->values);
  if (conf->priv->channel != NULL)
    xfconf_shutdown ();

  G_OBJECT_CLASS (xfpm_xfconf_parent_class)->finalize (object);
}

XfpmXfconf *
xfpm_xfconf_new (void)
{
  static gpointer xfpm_xfconf_object = NULL;

  if (G_LIKELY (xfpm_xfconf_object != NULL))
  {
    g_object_ref (xfpm_xfconf_object);
  }
  else
  {
    xfpm_xfconf_object = g_object_new (XFPM_TYPE_XFCONF, NULL);
    g_object_add_weak_pointer (xfpm_xfconf_object, &xfpm_xfconf_object);
  }

  return XFPM_XFCONF (xfpm_xfconf_object);
}

XfconfChannel *
xfpm_xfconf_get_channel (XfpmXfconf *conf)
{
  return conf->priv->channel;
}
