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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "xfpm-debug.h"

static gboolean enable_debug = FALSE;

#if defined(G_HAVE_ISO_VARARGS)

void
xfpm_debug (const char *func, const char *file, int line, const char *format, ...)
{
    va_list args;
    
    if ( !enable_debug )
	return;

    va_start (args, format);

    fprintf (stdout, "TRACE[%s:%d] %s(): ", file, line, func);
    vfprintf (stdout, format, args);
    fprintf (stdout, "\n");
    
    va_end (args);

}

void
xfpm_warn (const char *func, const char *file, int line, const char *format, ...)
{
    va_list args;

    if ( !enable_debug )
	return;

    va_start (args, format);
    
    fprintf(stdout, "TRACE[%s:%d] %s(): ", file, line, func);
    fprintf (stdout, "***WARNING***: ");
    vfprintf (stdout, format, args);
    fprintf (stdout, "\n");
    va_end (args);
}

void xfpm_debug_enum (const gchar *func, const gchar *file, gint line,
		      gint v_enum, GType type, const gchar *format, ...)
{
    va_list args;
    gchar *buffer;
    
    gchar *content = NULL;
    GValue __value__ = { 0, };
    
    if ( !enable_debug )
	return;
    
    g_value_init (&__value__, type);
    g_value_set_enum (&__value__, v_enum);
    
    content = g_strdup_value_contents (&__value__);
    
    va_start (args, format);
    g_vasprintf (&buffer, format, args);
    va_end (args);
	
    fprintf(stdout, "TRACE[%s:%d] %s(): ", file, line, func);
    fprintf(stdout, "%s: %s", buffer, content);
    fprintf(stdout, "\n");
    
    g_value_unset (&__value__);	
    g_free (content);
    g_free (buffer);
}

#endif /*defined(G_HAVE_ISO_VARARGS)*/

void xfpm_debug_init (gboolean debug)
{
    enable_debug = debug;
}
