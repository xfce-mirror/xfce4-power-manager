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

#ifndef __XFPM_ERRORS_H
#define __XFPM_ERRORS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_ERROR	(xfpm_error_get_type  ())
#define XFPM_ERROR      (xfpm_get_error_quark ())


typedef enum
{
    XFPM_ERROR_UNKNOWN = 0,
    XFPM_ERROR_PERMISSION_DENIED,
    XFPM_ERROR_NO_HARDWARE_SUPPORT,
    XFPM_ERROR_COOKIE_NOT_FOUND,
    XFPM_ERROR_INVALID_ARGUMENTS,
    XFPM_ERROR_SLEEP_FAILED
    
} XfpmError;

GType	xfpm_error_get_type  (void) G_GNUC_CONST;
GQuark  xfpm_get_error_quark (void);


G_END_DECLS

#endif /*__XFPM_ERRORS_H */
