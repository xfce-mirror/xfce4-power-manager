/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#include "xfpm-errors.h"

static GQuark xfpm_error_quark = 0;

GQuark
xfpm_get_error_quark (void)
{
    if (xfpm_error_quark == 0)
	xfpm_error_quark = g_quark_from_static_string ("xfpm-error-quark");
	
    return xfpm_error_quark;
}

GType
xfpm_error_get_type (void)
{
    static GType type = 0;
    
    if (!type)
    {
	static const GEnumValue values[] = 
	{
	    { XFPM_ERROR_UNKNOWN, "XFPM_ERROR_UNKNOWN", "Unknown" },
	    { XFPM_ERROR_PERMISSION_DENIED, "XFPM_ERROR_PERMISSION_DENIED", "PermissionDenied" },
	    { XFPM_ERROR_SUSPEND_NOT_SUPPORTED, "XFPM_ERROR_SUSPEND_NOT_SUPPORTED", "SuspendNotSupported" },
	    { XFPM_ERROR_HIBERNATE_NOT_SUPPORTED, "XFPM_ERROR_HIBERNATE_NOT_SUPPORTED", "HibernateNotSupported" },
	    { XFPM_ERROR_SUSPEND_HYBRID_NOT_SUPPORTED, "XFPM_ERROR_SUSPEND_HYBRID_NOT_SUPPORTED", "SuspendHybridNotSupported" },
	    { XFPM_ERROR_INVALID_COOKIE, "XFPM_ERROR_INVALID_COOKIE", "InvalidCookie" },
	    { XFPM_ERROR_INVALID_ARGUMENTS, "XFPM_ERROR_INVALID_ARGUMENTS", "InvalidArguments" },
	    { XFPM_ERROR_HAL_DISCONNECTED, "XFPM_ERROR_HAL_DISCONNECTED", "HalDisconnected" },
	    { XFPM_ERROR_SLEEP_FAILED, "XFPM_ERROR_SLEEP_FAILED", "SleepFailed" },
	    { 0, NULL, NULL }
	};
	
	type = g_enum_register_static ("XfpmError", values);
    }
    return type;
}
