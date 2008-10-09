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

#ifndef __XFPM_DPMS_H
#define __XFPM_DPMS_H

#include <glib-object.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_DPMS

G_BEGIN_DECLS

#define XFPM_TYPE_DPMS      (xfpm_dpms_get_type())
#define XFPM_DPMS(o)        (G_TYPE_CHECK_INSTANCE_CAST((o),XFPM_TYPE_DPMS,XfpmDpms))

typedef struct XfpmDpmsPrivate XfpmDpmsPrivate;

typedef struct 
{
    GObject parent;
    XfpmDpmsPrivate *priv;
    
    gboolean ac_adapter_present;
    gboolean force_dpms;
    gboolean dpms_enabled;
    
    guint on_ac_standby_timeout;
    guint on_ac_suspend_timeout;
    guint on_ac_off_timeout;
    guint on_batt_standby_timeout;
    guint on_batt_suspend_timeout;
    guint on_batt_off_timeout;
    
} XfpmDpms;

typedef struct
{
    GObjectClass parent_class;

} XfpmDpmsClass;

GType      xfpm_dpms_get_type    ();
XfpmDpms  *xfpm_dpms_new         (void);
gboolean   xfpm_dpms_capable     (XfpmDpms *dpms);

G_END_DECLS

#endif /* HAVE_DPMS */

#endif /* __XFPM_DPMS_H */
