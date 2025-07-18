/*
 * * Copyright (C) 2014 Eric Koegel <eric@xfce.org>
 * * Copyright (C) 2019 Kacper Piwiński
 * * Copyright (C) 2024 Andrzej Radecki <andrzejr@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "power-manager-config.h"
#include "power-manager-plugin.h"

#include "common/xfpm-brightness.h"
#include "common/xfpm-common.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-enum-glib.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"

#include <xfconf/xfconf.h>



static void
power_manager_config_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec);
static void
power_manager_config_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec);
static void
power_manager_config_finalize (GObject *object);



enum
{
  PROP_0,
  PROP_SHOW_PANEL_LABEL,
  PROP_PRESENTATION_MODE,
  PROP_SHOW_PRESENTATION_INDICATOR,
};


struct _PowerManagerConfig
{
  GObject __parent__;

  PowerManagerPlugin *plugin;
  XfconfChannel *channel;

  gint show_panel_label;
  gboolean presentation_mode;
  gboolean show_presentation_indicator;
};


G_DEFINE_TYPE (PowerManagerConfig, power_manager_config, G_TYPE_OBJECT)



static void
power_manager_config_class_init (PowerManagerConfigClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = power_manager_config_set_property;
  gobject_class->get_property = power_manager_config_get_property;
  gobject_class->finalize = power_manager_config_finalize;

  g_object_class_install_property (gobject_class, PROP_SHOW_PANEL_LABEL,
                                   g_param_spec_int (SHOW_PANEL_LABEL,
                                                     NULL, NULL,
                                                     0, N_PANEL_LABELS - 1, DEFAULT_SHOW_PANEL_LABEL,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PRESENTATION_MODE,
                                   g_param_spec_boolean (PRESENTATION_MODE,
                                                         NULL, NULL,
                                                         DEFAULT_PRESENTATION_MODE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SHOW_PRESENTATION_INDICATOR,
                                   g_param_spec_boolean (SHOW_PRESENTATION_INDICATOR,
                                                         NULL, NULL,
                                                         DEFAULT_SHOW_PRESENTATION_INDICATOR,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}



static void
power_manager_config_init (PowerManagerConfig *config)
{
}



static void
power_manager_config_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
  PowerManagerConfig *config = POWER_MANAGER_CONFIG (object);

  switch (prop_id)
  {
    case PROP_SHOW_PANEL_LABEL:
      g_value_set_int (value, config->show_panel_label);
      break;

    case PROP_PRESENTATION_MODE:
      g_value_set_boolean (value, config->presentation_mode);
      break;

    case PROP_SHOW_PRESENTATION_INDICATOR:
      g_value_set_boolean (value, config->show_presentation_indicator);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}



static void
power_manager_config_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
  PowerManagerConfig *config = POWER_MANAGER_CONFIG (object);

  switch (prop_id)
  {
    case PROP_SHOW_PANEL_LABEL:
      config->show_panel_label = g_value_get_int (value);
      break;

    case PROP_PRESENTATION_MODE:
      config->presentation_mode = g_value_get_boolean (value);
      break;

    case PROP_SHOW_PRESENTATION_INDICATOR:
      config->show_presentation_indicator = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
power_manager_config_finalize (GObject *object)
{
  PowerManagerConfig *config = POWER_MANAGER_CONFIG (object);

  if (config->channel != NULL)
    xfconf_shutdown ();

  G_OBJECT_CLASS (power_manager_config_parent_class)->finalize (object);
}



PowerManagerConfig *
power_manager_config_new (PowerManagerPlugin *plugin)
{
  PowerManagerConfig *config;
  XfconfChannel *channel;
  gchar *property;
  GError *error = NULL;

  config = g_object_new (POWER_MANAGER_TYPE_CONFIG, NULL);
  config->plugin = plugin;

  if (xfconf_init (&error))
  {
    channel = xfconf_channel_get (XFPM_CHANNEL);

    config->channel = channel;

    property = g_strconcat (XFPM_PROPERTIES_PREFIX, SHOW_PANEL_LABEL, NULL);
    xfconf_g_property_bind (channel, property, G_TYPE_INT, config, SHOW_PANEL_LABEL);
    g_free (property);

    property = g_strconcat (XFPM_PROPERTIES_PREFIX, PRESENTATION_MODE, NULL);
    xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, PRESENTATION_MODE);
    g_free (property);

    property = g_strconcat (XFPM_PROPERTIES_PREFIX, SHOW_PRESENTATION_INDICATOR, NULL);
    xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, SHOW_PRESENTATION_INDICATOR);
    g_free (property);
  }
  else
  {
    g_critical ("xfconf_init failed: %s", error->message);
    g_error_free (error);
  }

  return config;
}


gint
power_manager_config_get_show_panel_label (PowerManagerConfig *config)
{
  g_return_val_if_fail (POWER_MANAGER_IS_CONFIG (config), 0);

  return config->show_panel_label;
}

gboolean
power_manager_config_get_presentation_mode (PowerManagerConfig *config)
{
  g_return_val_if_fail (POWER_MANAGER_IS_CONFIG (config), FALSE);

  return config->presentation_mode;
}

gboolean
power_manager_config_get_show_presentation_indicator (PowerManagerConfig *config)
{
  g_return_val_if_fail (POWER_MANAGER_IS_CONFIG (config), FALSE);

  return config->show_presentation_indicator;
}
