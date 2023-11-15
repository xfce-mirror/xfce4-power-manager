/*
 *
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdk.h>

#include <libxfce4util/libxfce4util.h>

#include "xfpm-common.h"

#include "xfpm-dpms.h"
#include "xfpm-xfconf.h"
#include "xfpm-config.h"
#include "xfpm-debug.h"


static void xfpm_dpms_finalize   (GObject *object);

struct XfpmDpmsPrivate
{
  XfpmXfconf      *conf;

  gboolean         dpms_capable;
  gboolean         inhibited;

  gboolean         on_battery;

  gulong           switch_off_timeout_id;
  gulong           switch_on_timeout_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (XfpmDpms, xfpm_dpms, G_TYPE_OBJECT)

static void
xfpm_dpms_set_timeouts (XfpmDpms *dpms, guint16 standby, guint16 suspend, guint off)
{
  CARD16 x_standby = 0 , x_suspend = 0, x_off = 0;

  DPMSGetTimeouts (gdk_x11_get_default_xdisplay(), &x_standby, &x_suspend, &x_off);

  if ( standby != x_standby || suspend != x_suspend || off != x_off )
  {
    XFPM_DEBUG ("Settings dpms: standby=%d suspend=%d off=%d\n", standby, suspend, off);
    DPMSSetTimeouts (gdk_x11_get_default_xdisplay(), standby,
                     suspend,
                     off );
  }
}

/*
 * Disable DPMS
 */
static void
xfpm_dpms_disable (XfpmDpms *dpms)
{
  BOOL state;
  CARD16 power_level;

  if (!DPMSInfo (gdk_x11_get_default_xdisplay(), &power_level, &state) )
    g_warning ("Cannot get DPMSInfo");

  if ( state )
    DPMSDisable (gdk_x11_get_default_xdisplay());
}

/*
 * Enable DPMS
 */
static void
xfpm_dpms_enable (XfpmDpms *dpms)
{
  BOOL state;
  CARD16 power_level;

  if (!DPMSInfo (gdk_x11_get_default_xdisplay(), &power_level, &state) )
    g_warning ("Cannot get DPMSInfo");

  if ( !state )
    DPMSEnable (gdk_x11_get_default_xdisplay());
}

static void
xfpm_dpms_get_enabled (XfpmDpms *dpms, gboolean *dpms_enabled)
{
  g_object_get (G_OBJECT (dpms->priv->conf),
                DPMS_ENABLED_CFG, dpms_enabled,
                NULL);
}

static void
xfpm_dpms_get_sleep_mode (XfpmDpms *dpms, gboolean *ret_standby_mode)
{
  gchar *sleep_mode;

  g_object_get (G_OBJECT (dpms->priv->conf),
                DPMS_SLEEP_MODE, &sleep_mode,
                NULL);

  if ( !g_strcmp0 (sleep_mode, "Standby"))
    *ret_standby_mode = TRUE;
  else
    *ret_standby_mode = FALSE;

  g_free (sleep_mode);
}

static void
xfpm_dpms_get_configuration_timeouts (XfpmDpms *dpms, guint16 *ret_sleep, guint16 *ret_off )
{
  guint sleep_time, off_time;

  g_object_get (G_OBJECT (dpms->priv->conf),
                dpms->priv->on_battery ? ON_BATT_DPMS_SLEEP : ON_AC_DPMS_SLEEP, &sleep_time,
                dpms->priv->on_battery ? ON_BATT_DPMS_OFF : ON_AC_DPMS_OFF, &off_time,
                NULL);

  *ret_sleep = sleep_time * 60;
  *ret_off =  off_time * 60;
}

void
xfpm_dpms_refresh (XfpmDpms *dpms)
{
  gboolean enabled;
  guint16 off_timeout;
  guint16 sleep_timeout;
  gboolean sleep_mode;

  if (!dpms->priv->dpms_capable)
    return;

  if ( dpms->priv->inhibited)
  {
    xfpm_dpms_disable (dpms);
    return;
  }

  xfpm_dpms_get_enabled (dpms, &enabled);

  if ( !enabled )
  {
    xfpm_dpms_disable (dpms);
    return;
  }

  xfpm_dpms_enable (dpms);
  xfpm_dpms_get_configuration_timeouts (dpms, &sleep_timeout, &off_timeout);
  xfpm_dpms_get_sleep_mode (dpms, &sleep_mode);

  if (sleep_mode == TRUE )
  {
    xfpm_dpms_set_timeouts     (dpms,
                                sleep_timeout,
                                0,
                                off_timeout);
  }
  else
  {
    xfpm_dpms_set_timeouts     (dpms,
                                0,
                                sleep_timeout,
                                off_timeout );
  }
}

static void
xfpm_dpms_settings_changed_cb (GObject *obj, GParamSpec *spec, XfpmDpms *dpms)
{
  if ( g_str_has_prefix (spec->name, "dpms"))
  {
    XFPM_DEBUG ("Configuration changed");
    xfpm_dpms_refresh (dpms);
  }
}

static void
xfpm_dpms_class_init(XfpmDpmsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = xfpm_dpms_finalize;
}

/*
 * Check if the display is DPMS capabale if not do nothing.
 */
static void
xfpm_dpms_init(XfpmDpms *dpms)
{
  dpms->priv = xfpm_dpms_get_instance_private(dpms);

  dpms->priv->dpms_capable = DPMSCapable (gdk_x11_get_default_xdisplay());
  dpms->priv->switch_off_timeout_id = 0;
  dpms->priv->switch_on_timeout_id = 0;

  if ( dpms->priv->dpms_capable )
  {
    dpms->priv->conf    = xfpm_xfconf_new  ();

    g_signal_connect_object (dpms->priv->conf, "notify",
                             G_CALLBACK (xfpm_dpms_settings_changed_cb), dpms, 0);

    xfpm_dpms_refresh (dpms);
  }
  else
  {
    g_warning ("Monitor is not DPMS capable");
  }
}

static void
xfpm_dpms_finalize(GObject *object)
{
  XfpmDpms *dpms;

  dpms = XFPM_DPMS (object);

  if (dpms->priv->conf != NULL)
    g_object_unref (dpms->priv->conf);

  G_OBJECT_CLASS(xfpm_dpms_parent_class)->finalize(object);
}

XfpmDpms *
xfpm_dpms_new (void)
{
  static gpointer xfpm_dpms_object = NULL;

  if ( G_LIKELY (xfpm_dpms_object != NULL ) )
  {
    g_object_ref (xfpm_dpms_object);
  }
  else
  {
    xfpm_dpms_object = g_object_new (XFPM_TYPE_DPMS, NULL);
    g_object_add_weak_pointer (xfpm_dpms_object, &xfpm_dpms_object);
  }

  return XFPM_DPMS (xfpm_dpms_object);
}

gboolean
xfpm_dpms_capable (XfpmDpms *dpms)
{
  g_return_val_if_fail (XFPM_IS_DPMS(dpms), FALSE);

  return dpms->priv->dpms_capable;
}

void
xfpm_dpms_force_level (XfpmDpms *dpms, CARD16 level)
{
  CARD16 current_level;
  BOOL current_state;

  XFPM_DEBUG ("start");

  if ( !dpms->priv->dpms_capable )
    goto out;

  if ( G_UNLIKELY (!DPMSInfo (gdk_x11_get_default_xdisplay (), &current_level, &current_state)) )
  {
    g_warning ("Cannot get DPMSInfo");
    goto out;
  }

  if ( !current_state )
  {
    XFPM_DEBUG ("DPMS is disabled");
    goto out;
  }

  if ( current_level != level )
  {
    XFPM_DEBUG ("Forcing DPMS mode %d", level);

    if ( !DPMSForceLevel (gdk_x11_get_default_xdisplay (), level ) )
    {
      g_warning ("Cannot set Force DPMS level %d", level);
      goto out;
    }

    if ( level == DPMSModeOn )
      XResetScreenSaver (gdk_x11_get_default_xdisplay ());

    XSync (gdk_x11_get_default_xdisplay (), FALSE);
  }
  else
  {
    XFPM_DEBUG ("No need to change DPMS mode, current_level=%d requested_level=%d", current_level, level);
  }

  out:
    ;
}

gboolean
xfpm_dpms_is_inhibited (XfpmDpms *dpms)
{
  return dpms->priv->inhibited;
}

void
xfpm_dpms_inhibit (XfpmDpms *dpms, gboolean inhibit)
{
  if ( dpms->priv->inhibited == inhibit )
    return;

  dpms->priv->inhibited = inhibit;
  xfpm_dpms_refresh (dpms);
  XFPM_DEBUG ("dpms inhibited %s", inhibit ? "TRUE" : "FALSE");
}

void
xfpm_dpms_set_on_battery (XfpmDpms *dpms, gboolean on_battery)
{
  if ( dpms->priv->on_battery == on_battery )
    return;

  dpms->priv->on_battery = on_battery;
  xfpm_dpms_refresh (dpms);
  XFPM_DEBUG ("dpms on battery %s", on_battery ? "TRUE" : "FALSE");
}
