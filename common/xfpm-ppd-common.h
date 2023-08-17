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

#ifndef __XFPM_PPD_COMMON_H
#define __XFPM_PPD_COMMON_H

#include <gio/gio.h>

G_BEGIN_DECLS

GDBusProxy            *xfpm_ppd_g_dbus_proxy_new   (void);
GSList                *xfpm_ppd_get_profiles       (void);

G_END_DECLS

#endif
