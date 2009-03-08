/*
 * * Copyright (C) 2008-2009 Ali <aliov@xfce.org>
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

#ifndef __XFPM_STRING_H
#define __XFPM_STRING_H

#include <glib.h>
#include <string.h>

gint 		xfpm_strcmp 		(const gchar *str1,
					 const gchar *str2);

gboolean        xfpm_strequal		(const gchar *str1,
					 const gchar *str2);
const gchar    *xfpm_bool_to_string     (gboolean value);

gboolean        xfpm_string_to_bool     (const gchar *string);


#endif /* XFPM_STRING_H */
