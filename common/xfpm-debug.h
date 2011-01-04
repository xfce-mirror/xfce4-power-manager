/*
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

#ifndef __XFPM_DEBUG_H
#define __XFPM_DEBUG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <glib.h>

G_BEGIN_DECLS

#if defined(G_HAVE_ISO_VARARGS)

#define XFPM_DEBUG(...)\
    xfpm_debug (__func__, __FILE__, __LINE__, __VA_ARGS__)

#define XFPM_WARNING(...)\
    xfpm_warn (__func__, __FILE__, __LINE__, __VA_ARGS__)

#define XFPM_DEBUG_ENUM(_value, _type, ...)\
    xfpm_debug_enum (__func__, __FILE__, __LINE__, _value, _type, __VA_ARGS__)

					 
void		xfpm_debug_enum         (const gchar *func,
					 const gchar *file,
					 gint line,
					 gint v_enum,
					 GType type,
					 const gchar *format,
					 ...) __attribute__((format (printf,6,7)));

void		xfpm_debug 		(const char *func,
					 const char *file,
					 int line,
					 const char *format,
					 ...) __attribute__((format (printf,4,5)));

void		xfpm_warn 		(const char *func,
					 const char *file,
					 int line,
					 const char *format,
					 ...) __attribute__((format (printf,4,5)));

#else

#define XFPM_DEBUG(...)
#define XFPM_WARNING(...)
#define XFPM_DEBUG_ENUM(_value, _type, ...)

#endif

void		xfpm_debug_init		(gboolean debug);

G_END_DECLS

#endif /* __XFPM_DEBUG_H */
