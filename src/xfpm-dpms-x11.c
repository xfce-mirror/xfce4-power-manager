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

#include <gdk/gdkx.h>
#include <X11/extensions/dpms.h>

#include "xfpm-dpms-x11.h"
#include "common/xfpm-debug.h"

static void       xfpm_dpms_x11_set_mode            (XfpmDpms         *dpms,
                                                     XfpmDpmsMode      mode);
static void       xfpm_dpms_x11_set_enabled         (XfpmDpms         *dpms,
                                                     gboolean          enabled);
static void       xfpm_dpms_x11_set_timeouts        (XfpmDpms         *dpms,
                                                     gboolean          standby,
                                                     guint             sleep_timemout,
                                                     guint             off_timemout);

struct _XfpmDpmsX11
{
  XfpmDpms __parent__;
};



G_DEFINE_FINAL_TYPE (XfpmDpmsX11, xfpm_dpms_x11, XFPM_TYPE_DPMS)



static void
xfpm_dpms_x11_class_init (XfpmDpmsX11Class *klass)
{
  XfpmDpmsClass *dpms_class = XFPM_DPMS_CLASS (klass);

  dpms_class->set_mode = xfpm_dpms_x11_set_mode;
  dpms_class->set_enabled = xfpm_dpms_x11_set_enabled;
  dpms_class->set_timeouts = xfpm_dpms_x11_set_timeouts;
}

static void
xfpm_dpms_x11_init (XfpmDpmsX11 *dpms)
{
}

static CARD16
mode_to_x_mode (XfpmDpmsMode mode)
{
  switch (mode)
  {
    case XFPM_DPMS_MODE_OFF:
      return DPMSModeOff;
    case XFPM_DPMS_MODE_SUSPEND:
      return DPMSModeSuspend;
    case XFPM_DPMS_MODE_STANDBY:
      return DPMSModeStandby;
    case XFPM_DPMS_MODE_ON:
      return DPMSModeOn;
    default:
      g_warn_if_reached ();
      return DPMSModeOff;
  }
}

static void
xfpm_dpms_x11_set_mode (XfpmDpms *dpms,
                        XfpmDpmsMode mode)
{
  Display *display = gdk_x11_get_default_xdisplay ();
  CARD16 x_mode;
  BOOL x_enabled;

  if (!DPMSInfo (display, &x_mode, &x_enabled))
  {
    g_warning ("Cannot get DPMSInfo");
    return;
  }

  if (!x_enabled)
  {
    XFPM_DEBUG ("DPMS is disabled");
    return;
  }

  if (mode != x_mode)
  {
    XFPM_DEBUG ("Setting DPMS mode %d", mode);

    if (!DPMSForceLevel (display, mode_to_x_mode (mode)))
    {
      g_warning ("Cannot set DPMS mode");
      return;
    }

    if (mode == XFPM_DPMS_MODE_ON)
      XResetScreenSaver (display);

    XSync (display, FALSE);
  }
  else
  {
    XFPM_DEBUG ("DPMS mode %d already set", mode);
  }
}

static void
xfpm_dpms_x11_set_enabled (XfpmDpms *dpms,
                           gboolean enabled)
{
  Display *display = gdk_x11_get_default_xdisplay ();
  BOOL x_enabled;
  CARD16 mode;

  if (!DPMSInfo (display, &mode, &x_enabled))
  {
    g_warning ("Cannot get DPMSInfo");
    return;
  }

  if (x_enabled && !enabled)
    DPMSDisable (display);
  else if (!x_enabled && enabled)
    DPMSEnable (display);
}

static void
xfpm_dpms_x11_set_timeouts (XfpmDpms *dpms,
                            gboolean standby,
                            guint sleep_timeout,
                            guint off_timeout)
{
  Display *display = gdk_x11_get_default_xdisplay ();
  CARD16 x_standby = 0, x_suspend = 0, x_off = 0;

  DPMSGetTimeouts (display, &x_standby, &x_suspend, &x_off);
  if (x_standby != sleep_timeout || x_suspend != sleep_timeout || x_off != off_timeout)
  {
    x_standby = standby ? sleep_timeout : 0;
    x_suspend = standby ? 0 : sleep_timeout;
    XFPM_DEBUG ("Setting DPMS timeouts: standby=%d suspend=%d off=%d\n", x_standby, x_suspend, off_timeout);
    DPMSSetTimeouts (display, x_standby, x_suspend, off_timeout);
  }
}



XfpmDpms *
xfpm_dpms_x11_new (void)
{
  if (!DPMSCapable (gdk_x11_get_default_xdisplay ()))
  {
    g_warning ("Display is not DPMS capable");
    return NULL;
  }

  return g_object_new (XFPM_TYPE_DPMS_X11, NULL);
}
