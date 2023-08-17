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

#include <gio/gio.h>

#include "xfpm-config.h"
#include "xfpm-xfconf.h"
#include "xfpm-power.h"
#include "xfpm-ppd-common.h"
#include "xfpm-ppd.h"

static void xfpm_ppd_finalize (GObject *object);

static void xfpm_ppd_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec);

static void xfpm_ppd_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec);

struct XfpmPPDPrivate
{
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

G_DEFINE_TYPE_WITH_PRIVATE (XfpmPPD, xfpm_ppd, G_TYPE_OBJECT)

static void
xfpm_ppd_set_active_profile (XfpmPPD *ppd, const gchar *profile)
{
  GVariant *var = NULL;

  g_return_if_fail (XFPM_IS_PPD (ppd));
  g_return_if_fail (profile != NULL);

  var = g_dbus_proxy_call_sync (ppd->priv->proxy,
                                "org.freedesktop.DBus.Properties.Set",
                                g_variant_new ("(ssv)", "net.hadess.PowerProfiles", "ActiveProfile",
                                               g_variant_new_string (profile)),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, NULL, NULL);

  if ( var != NULL )
    g_variant_unref (var);
}

static void
xfpm_ppd_on_battery_changed_cb (XfpmPower *power, gboolean on_battery, XfpmPPD *ppd)
{
  xfpm_ppd_set_active_profile (ppd, on_battery ? ppd->priv->profile_on_battery : ppd->priv->profile_on_ac);
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
                                   g_param_spec_string ("profile-on-ac",
                                                        NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_PROFILE_ON_BATTERY,
                                   g_param_spec_string ("profile-on-battery",
                                                        NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE));
}

static void xfpm_ppd_init (XfpmPPD *ppd)
{
  ppd->priv = xfpm_ppd_get_instance_private (ppd);

  ppd->priv->conf = xfpm_xfconf_new ();
  ppd->priv->power = xfpm_power_get ();

  ppd->priv->proxy = xfpm_ppd_g_dbus_proxy_new ();

  if ( ppd->priv->proxy == NULL )
  {
    g_warning ("Unable to get the interface, net.hadess.PowerProfiles");
    return;
  }

  xfconf_g_property_bind (xfpm_xfconf_get_channel (ppd->priv->conf),
                          XFPM_PROPERTIES_PREFIX PROFILE_ON_AC, G_TYPE_STRING,
                          G_OBJECT (ppd), PROFILE_ON_AC);

  xfconf_g_property_bind (xfpm_xfconf_get_channel (ppd->priv->conf),
                          XFPM_PROPERTIES_PREFIX PROFILE_ON_BATTERY, G_TYPE_STRING,
                          G_OBJECT (ppd), PROFILE_ON_BATTERY);

  g_signal_connect (ppd->priv->power, "on-battery-changed",
                    G_CALLBACK (xfpm_ppd_on_battery_changed_cb), ppd);
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
      g_value_set_string (value, ppd->priv->profile_on_ac);
      break;
    case PROP_PROFILE_ON_BATTERY:
      g_value_set_string (value, ppd->priv->profile_on_battery);
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
  g_object_get (ppd->priv->power, "on-battery", &on_battery, NULL);

  switch (prop_id)
  {
    case PROP_PROFILE_ON_AC:
      ppd->priv->profile_on_ac = g_strdup (g_value_get_string (value));
      if ( !on_battery )
        xfpm_ppd_set_active_profile (ppd, ppd->priv->profile_on_ac);
      break;
    case PROP_PROFILE_ON_BATTERY:
      ppd->priv->profile_on_battery = g_strdup (g_value_get_string (value));
      if ( on_battery )
        xfpm_ppd_set_active_profile (ppd, ppd->priv->profile_on_battery);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
xfpm_ppd_finalize (GObject *object)
{
  XfpmPPD *ppd = NULL;

  ppd = XFPM_PPD (object);

  if ( ppd->priv->conf != NULL )
    g_object_unref (ppd->priv->conf);

  if ( ppd->priv->power != NULL )
    g_object_unref (ppd->priv->power);

  if ( ppd->priv->proxy != NULL )
    g_object_unref (ppd->priv->proxy);

  G_OBJECT_CLASS (xfpm_ppd_parent_class)->finalize (object);
}

XfpmPPD *
xfpm_ppd_new (void)
{
  XfpmPPD *ppd = NULL;
  ppd = g_object_new (XFPM_TYPE_PPD, NULL);
  return g_object_new (XFPM_TYPE_PPD, NULL);
}
