/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <ali.slackware@gmail.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XFPM_DRIVER_H
#define __XFPM_DRIVER_H

#include <glib-object.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xfpm-hal.h"
#include "xfpm-enums.h"

#ifdef HAVE_DPMS
#include "xfpm-dpms.h"
#endif

G_BEGIN_DECLS

#define XFPM_TYPE_DRIVER   (xfpm_driver_get_type())
#define XFPM_DRIVER(o)     (G_TYPE_CHECK_INSTANCE_CAST((o),XFPM_TYPE_DRIVER,XfpmDriver))
#define XFPM_IS_DRIVER(o)  (G_TYPE_CHECK_INSTANCE_TYPE((o),XFPM_TYPE_DRIVER))

typedef struct XfpmDriverPrivate XfpmDriverPrivate;

typedef struct 
{
    GObject parent;
    XfpmDriverPrivate *priv;
    
} XfpmDriver;

typedef struct 
{
    GObjectClass parent_class;
    
} XfpmDriverClass;

GType         xfpm_driver_get_type              (void);
XfpmDriver   *xfpm_driver_new                   (void);
gboolean      xfpm_driver_monitor               (XfpmDriver *drv);

G_END_DECLS

#endif /* __XFPM_DRIVER_H */
