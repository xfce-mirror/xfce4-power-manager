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

#ifndef __XFPM_DEBUG_H
#define __XFPM_DEBUG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

G_BEGIN_DECLS

#ifdef DEBUG

static gchar *content = NULL;
static GValue __value__ = { 0, };

#define XFPM_DEBUG_ENUM(_text, _value, _type) 				\
    g_value_init (&__value__, _type);					\
    g_value_set_enum (&__value__, _value);				\
    content = g_strdup_value_contents (&__value__);			\
    TRACE ("%s : %s", _text, content);					\
    g_value_unset (&__value__);						\
    g_free (content);
    
#define XFPM_DEBUG_ENUM_FULL(_value, _type, ...)			\
    g_value_init (&__value__, _type);					\
    g_value_set_enum (&__value__, _value);				\
    content = g_strdup_value_contents (&__value__);			\
    fprintf(stderr, "TRACE[%s:%d] %s(): ",__FILE__,__LINE__,__func__);  \
    fprintf(stderr, __VA_ARGS__);					\
    fprintf(stderr, ": %s", content);					\
    fprintf(stderr, "\n");						\
    g_value_unset (&__value__);						\
    g_free (content);
    
#else

#define XFPM_DEBUG_ENUM(_text, _value, _type)
#define XFPM_DEBUG_ENUM_FULL(_value, _type, ...)			

#endif

G_END_DECLS

#endif /* __XFPM_DEBUG_H */
