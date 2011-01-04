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

#ifndef __XFPM_XFCONF_H
#define __XFPM_XFCONF_H

#include <glib-object.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

#define XFPM_TYPE_XFCONF        (xfpm_xfconf_get_type () )
#define XFPM_XFCONF(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_XFCONF, XfpmXfconf))
#define XFPM_IS_XFCONF(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_XFCONF))

typedef struct  XfpmXfconfPrivate XfpmXfconfPrivate;

typedef struct
{
    GObject		  parent;
    XfpmXfconfPrivate    *priv;
    
} XfpmXfconf;

typedef struct
{
    GObjectClass	  parent_class;
    
} XfpmXfconfClass;

GType        		  xfpm_xfconf_get_type           	(void) G_GNUC_CONST;

XfpmXfconf       	 *xfpm_xfconf_new                 	(void);

XfconfChannel 		 *xfpm_xfconf_get_channel		(XfpmXfconf *conf);

G_END_DECLS

#endif /* __XFPM_XFCONF_H */
