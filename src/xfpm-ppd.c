/*
 * * Copyright (C) 2023 Elliot <BlindRepublic@mailo.com>
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

#include "xfpm-power.h"
#include "xfpm-ppd.h"
#include "xfpm-xfconf.h"

#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-ppd-common.h"

#include <gio/gio.h>

static void
xfpm_ppd_finalize (GObject *object);

static void
xfpm_ppd_get_property (GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec);

static void
xfpm_ppd_set_property (GObject *object,
                       guint prop_id,
                       const GValue *value,
                       GParamSpec *pspec);

struct _XfpmPPD
{
  GObject parent;

  XfpmXfconf *conf;
  XfpmPower *power;

  GDBusProxy *proxy;

  gchar *profile_on_ac;
  gchar *profile_on_battery;
};

enum
{
  PROP_0,
  PROP_PROFILE_ON_AC,
  PROP_PROFILE_ON_BATTERY,
  N_PROPERTIES
};

G_DEFINE_FINAL_TYPE (XfpmPPD, xfpm_ppd, G_TYPE_OBJECT)

static void
xfpm_ppd_set_active_profile (XfpmPPD *ppd,
                             const gchar *profile)
{
  GVariant *var = NULL;
  GError *error = NULL;

  g_return_if_fail (XFPM_IS_PPD (ppd));
  g_return_if_fail (profile != NULL);

  var = g_dbus_proxy_call_sync (ppd->proxy,
                                "org.freedesktop.DBus.Properties.Set",
                                g_variant_new ("(ssv)", "net.hadess.PowerProfiles", "ActiveProfile",
                                               g_variant_new_string (profile)),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, NULL, &error);

  if (var != NULL)
    g_variant_unref (var);

  if (error != NULL)
  {
    XFPM_DEBUG ("Failed to set active power profile : %s", error->message);
    g_error_free (error);
  }
}

static void
xfpm_ppd_on_battery_changed (XfpmPower *power,
                             gboolean on_battery,
                             XfpmPPD *ppd)
{
  xfpm_ppd_set_active_profile (ppd, on_battery ? ppd->profile_on_battery : ppd->profile_on_ac);
}

static void
xfpm_ppd_class_init (XfpmPPDClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xfpm_ppd_finalize;
  object_class->get_property = xfpm_ppd_get_property;
  object_class->set_property = xfpm_ppd_set_property;

  g_object_class_install_property (object_class,
                                   PROP_PROFILE_ON_AC,
                                   g_param_spec_string (PROFILE_ON_AC,
                                                        NULL, NULL,
                                                        DEFAULT_PROFILE_ON_AC,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_PROFILE_ON_BATTERY,
                                   g_param_spec_string (PROFILE_ON_BATTERY,
                                                        NULL, NULL,
                                                        DEFAULT_PROFILE_ON_BATTERY,
                                                        G_PARAM_READWRITE));
}

static void
xfpm_ppd_init (XfpmPPD *ppd)
{
  if ((ppd->proxy = xfpm_ppd_g_dbus_proxy_new ()) == NULL)
    return;

  ppd->conf = xfpm_xfconf_new ();
  ppd->power = xfpm_power_get ();

  xfconf_g_property_bind (xfpm_xfconf_get_channel (ppd->conf),
                          XFPM_PROPERTIES_PREFIX PROFILE_ON_AC, G_TYPE_STRING,
                          G_OBJECT (ppd), PROFILE_ON_AC);

  xfconf_g_property_bind (xfpm_xfconf_get_channel (ppd->conf),
                          XFPM_PROPERTIES_PREFIX PROFILE_ON_BATTERY, G_TYPE_STRING,
                          G_OBJECT (ppd), PROFILE_ON_BATTERY);

  g_signal_connect (ppd->power, "on-battery-changed", G_CALLBACK (xfpm_ppd_on_battery_changed), ppd);
}

static void
xfpm_ppd_get_property (GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec)
{
  XfpmPPD *ppd = XFPM_PPD (object);

  switch (prop_id)
  {
    case PROP_PROFILE_ON_AC:
      g_value_set_string (value, ppd->profile_on_ac);
      break;

    case PROP_PROFILE_ON_BATTERY:
      g_value_set_string (value, ppd->profile_on_battery);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
xfpm_ppd_set_property (GObject *object,
                       guint prop_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
  XfpmPPD *ppd = XFPM_PPD (object);
  gboolean on_battery;

  /* Modify property and apply profile if necessary */
  g_object_get (ppd->power, "on-battery", &on_battery, NULL);

  switch (prop_id)
  {
    case PROP_PROFILE_ON_AC:
      g_free (ppd->profile_on_ac);
      ppd->profile_on_ac = g_value_dup_string (value);
      if (!on_battery)
        xfpm_ppd_set_active_profile (ppd, ppd->profile_on_ac);
      break;

    case PROP_PROFILE_ON_BATTERY:
      g_free (ppd->profile_on_battery);
      ppd->profile_on_battery = g_value_dup_string (value);
      if (on_battery)
        xfpm_ppd_set_active_profile (ppd, ppd->profile_on_battery);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
xfpm_ppd_finalize (GObject *object)
{
  XfpmPPD *ppd = XFPM_PPD (object);

  if (ppd->conf != NULL)
    g_object_unref (ppd->conf);

  if (ppd->power != NULL)
  {
    g_signal_handlers_disconnect_by_func (ppd->power, xfpm_ppd_on_battery_changed, ppd);
    g_object_unref (ppd->power);
  }

  if (ppd->proxy != NULL)
    g_object_unref (ppd->proxy);

  g_free (ppd->profile_on_ac);
  g_free (ppd->profile_on_battery);

  G_OBJECT_CLASS (xfpm_ppd_parent_class)->finalize (object);
}

XfpmPPD *
xfpm_ppd_new (void)
{
  return g_object_new (XFPM_TYPE_PPD, NULL);
}
