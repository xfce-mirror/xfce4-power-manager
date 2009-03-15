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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xfpm-xfconf.h"

/* Init */
static void xfpm_xfconf_class_init (XfpmXfconfClass *klass);
static void xfpm_xfconf_init       (XfpmXfconf *xfconf);
static void xfpm_xfconf_finalize   (GObject *object);

static gpointer xfpm_xfconf_object = NULL;

G_DEFINE_TYPE(XfpmXfconf, xfpm_xfconf, G_TYPE_OBJECT)

static void
xfpm_xfconf_class_init (XfpmXfconfClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = xfpm_xfconf_finalize;
}

static void
xfpm_xfconf_init (XfpmXfconf *xfconf)
{
    xfconf->channel = xfconf_channel_new ("xfce4-power-manager");
}

static void
xfpm_xfconf_finalize(GObject *object)
{
    XfpmXfconf *xfconf;

    xfconf = XFPM_XFCONF(object);
    
    if (xfconf->channel )
	g_object_unref (xfconf->channel);
    
    G_OBJECT_CLASS(xfpm_xfconf_parent_class)->finalize(object);
}

XfpmXfconf *
xfpm_xfconf_new(void)
{
    if ( xfpm_xfconf_object != NULL )
    {
	g_object_ref (xfpm_xfconf_object);
    } 
    else
    {
	xfpm_xfconf_object = g_object_new (XFPM_TYPE_XFCONF, NULL);
	g_object_add_weak_pointer (xfpm_xfconf_object, &xfpm_xfconf_object);
    }
    
    return XFPM_XFCONF (xfpm_xfconf_object);
}
