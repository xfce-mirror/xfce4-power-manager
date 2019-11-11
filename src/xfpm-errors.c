/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include <gio/gio.h>

#include "xfpm-errors.h"

GQuark
xfpm_get_error_quark (void)
{
  static volatile gsize xfpm_error_quark = 0;
  if (xfpm_error_quark == 0)
  {
    static const GDBusErrorEntry values[] =
    {
      { XFPM_ERROR_UNKNOWN, "org.xfce.PowerManager.Error.Unknown" },
      { XFPM_ERROR_PERMISSION_DENIED, "org.xfce.PowerManager.Error.PermissionDenied" },
      { XFPM_ERROR_NO_HARDWARE_SUPPORT, "org.xfce.PowerManager.Error.NoHardwareSupport" },
      { XFPM_ERROR_COOKIE_NOT_FOUND, "org.xfce.PowerManager.Error.CookieNotFound" },
      { XFPM_ERROR_INVALID_ARGUMENTS, "org.xfce.PowerManager.Error.InvalidArguments" },
      { XFPM_ERROR_SLEEP_FAILED, "org.xfce.PowerManager.Error.SleepFailed" },
    };

    g_dbus_error_register_error_domain ("xfpm-error-quark",
                                        &xfpm_error_quark,
                                        values,
                                        G_N_ELEMENTS (values));
  }

  return (GQuark) xfpm_error_quark;
}
