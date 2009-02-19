/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <aliov@xfce.org>
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

#ifndef __XFPM_DPMS_SPINS_H
#define __XFPM_DPMS_SPINS_H

#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_DPMS

G_BEGIN_DECLS

#define XFPM_TYPE_DPMS_SPINS    (xfpm_dpms_spins_get_type())
#define XFPM_DPMS_SPINS(o)      (G_TYPE_CHECK_INSTANCE_CAST((o),XFPM_TYPE_DPMS_SPINS,XfpmDpmsSpins))
#define XFPM_IS_DPMS_SPINS(o)   (G_TYPE_CHECK_INSTANCE_TYPE((o),XFPM_TYPE_DPMS_SPINS))

typedef struct XfpmDpmsSpinsPrivate XfpmDpmsSpinsPrivate;

typedef struct 
{
    GtkTable parent;
    XfpmDpmsSpinsPrivate *priv;

} XfpmDpmsSpins;

typedef struct 
{
    GtkTableClass parent_class;
    void         (*dpms_value_changed)       (XfpmDpmsSpins *dpms_spins,
                                              gint spin_1,
                                              gint spin_2,
                                              gint spin_3);
    
} XfpmDpmsSpinsClass;

GType          xfpm_dpms_spins_get_type           (void);
GtkWidget     *xfpm_dpms_spins_new                (void);
void           xfpm_dpms_spins_set_default_values (XfpmDpmsSpins *spins,
                                                   guint spin_1,
                                                   guint spin_2,
                                                   guint spin_3);    
void           xfpm_dpms_spins_set_active         (XfpmDpmsSpins *spins,
                                                   gboolean active);    
G_END_DECLS
#endif /*HAVE DPMS */

#endif /* __XFPM_DPMS_SPINS_H */
