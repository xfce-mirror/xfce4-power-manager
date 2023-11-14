/*
 * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

#include "xfpm-dpms.h"
#include "xfpm-xfconf.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include "xfpm-dpms-x11.h"
#endif
#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include "xfpm-dpms-wayland.h"
#endif

#define get_instance_private(instance) ((XfpmDpmsPrivate *) \
  xfpm_dpms_get_instance_private (XFPM_DPMS (instance)))

static void       xfpm_dpms_finalize       (GObject       *object);

typedef struct _XfpmDpmsPrivate
{
  XfpmXfconf *conf;
  gboolean inhibited;
  gboolean on_battery;
} XfpmDpmsPrivate;



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (XfpmDpms, xfpm_dpms, G_TYPE_OBJECT)



static void
xfpm_dpms_class_init (XfpmDpmsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xfpm_dpms_finalize;
}

static void
xfpm_dpms_init (XfpmDpms *dpms)
{
}

static void
xfpm_dpms_finalize (GObject *object)
{
  XfpmDpmsPrivate *priv = get_instance_private (object);

  if (priv->conf != NULL)
    g_object_unref (priv->conf);

  G_OBJECT_CLASS (xfpm_dpms_parent_class)->finalize (object);
}



static void
refresh (XfpmDpms *dpms)
{
  XfpmDpmsPrivate *priv = get_instance_private (dpms);
  gboolean enabled;
  guint off_timeout;
  guint sleep_timeout;
  gchar *sleep_mode;

  if (priv->inhibited)
  {
    XFPM_DPMS_GET_CLASS (dpms)->set_enabled (dpms, FALSE);
    return;
  }

  g_object_get (priv->conf, DPMS_ENABLED_CFG, &enabled, NULL);
  if (!enabled)
  {
    XFPM_DPMS_GET_CLASS (dpms)->set_enabled (dpms, FALSE);
    return;
  }

  g_object_get (priv->conf,
                priv->on_battery ? ON_BATT_DPMS_SLEEP : ON_AC_DPMS_SLEEP, &sleep_timeout,
                priv->on_battery ? ON_BATT_DPMS_OFF : ON_AC_DPMS_OFF, &off_timeout,
                NULL);
  g_object_get (G_OBJECT (priv->conf), DPMS_SLEEP_MODE, &sleep_mode, NULL);

  XFPM_DPMS_GET_CLASS (dpms)->set_enabled (dpms, TRUE);
  XFPM_DPMS_GET_CLASS (dpms)->set_timeouts (dpms, g_strcmp0 (sleep_mode, "Standby") == 0, sleep_timeout * 60, off_timeout * 60);

  g_free (sleep_mode);
}

static void
settings_changed (GObject *object,
                  GParamSpec *pspec,
                  XfpmDpms *dpms)
{
  if (g_str_has_prefix (pspec->name, "dpms"))
  {
    XFPM_DEBUG ("Configuration changed");
    refresh (dpms);
  }
}

XfpmDpms *
xfpm_dpms_new (void)
{
  static gpointer singleton = NULL;
  static gboolean tried = FALSE;

  if (singleton != NULL)
  {
    g_object_ref (singleton);
  }
  else if (!tried)
  {
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      singleton = xfpm_dpms_x11_new ();
      tried = TRUE;
    }
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      singleton = xfpm_dpms_wayland_new ();
      tried = TRUE;
    }
#endif
    if (singleton != NULL)
    {
      XfpmDpmsPrivate *priv = get_instance_private (singleton);
      priv->conf = xfpm_xfconf_new ();
      g_signal_connect_object (priv->conf, "notify", G_CALLBACK (settings_changed), singleton, 0);
      refresh (singleton);
      g_object_add_weak_pointer (singleton, &singleton);
    }
    else if (!tried)
    {
      g_critical ("DPMS is not supported on this windowing environment");
      tried = TRUE;
    }
  }

  return singleton;
}

void
xfpm_dpms_set_inhibited (XfpmDpms *dpms,
                         gboolean inhibited)
{
  XfpmDpmsPrivate *priv = get_instance_private (dpms);

  g_return_if_fail (XFPM_IS_DPMS (dpms));

  if (inhibited == priv->inhibited)
    return;

  priv->inhibited = inhibited;
  refresh (dpms);
  XFPM_DEBUG ("DPMS inhibited: %s", inhibited ? "TRUE" : "FALSE");
}

void
xfpm_dpms_set_on_battery (XfpmDpms *dpms,
                          gboolean on_battery)
{
  XfpmDpmsPrivate *priv = get_instance_private (dpms);

  g_return_if_fail (XFPM_IS_DPMS (dpms));

  if (on_battery == priv->on_battery)
    return;

  priv->on_battery = on_battery;
  refresh (dpms);
  XFPM_DEBUG ("DPMS on battery: %s", on_battery ? "TRUE" : "FALSE");
}



void
xfpm_dpms_set_mode (XfpmDpms *dpms,
                    XfpmDpmsMode mode)
{
  g_return_if_fail (XFPM_IS_DPMS (dpms));
  XFPM_DPMS_GET_CLASS (dpms)->set_mode (dpms, mode);
}
