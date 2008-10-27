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

#ifndef __XFPM_AC_ADAPTER_H
#define __XFPM_AC_ADAPTER_H

#include <glib.h>
#include <gtk/gtk.h>

#define XFPM_TYPE_AC_ADAPTER    (xfpm_ac_adapter_get_type())
#define XFPM_AC_ADAPTER(o)      (G_TYPE_CHECK_INSTANCE_CAST(o,XFPM_TYPE_AC_ADAPTER,XfpmAcAdapter))
#define XFPM_IS_AC_ADAPTER(o)   (G_TYPE_CHECK_INSTANCE_TYPE(o,XFPM_TYPE_AC_ADAPTER))

typedef struct XfpmAcAdapterPrivate XfpmAcAdapterPrivate;

typedef struct
{
    GtkStatusIcon parent;
    XfpmAcAdapterPrivate *priv;
    
} XfpmAcAdapter;

typedef struct
{
    GtkStatusIconClass parent_class;
    
    /* signals */
    void   (*ac_adapter_changed)  (XfpmAcAdapter *xfpm_adapter,
                                   gboolean present,
                                   gboolean state_ok);
                                   
    void  (*adapter_action_request) (XfpmAcAdapter *adapter,
                                     XfpmActionRequest action,
                                     gboolean critical);                               
} XfpmAcAdapterClass;

GType          xfpm_ac_adapter_get_type       (void) G_GNUC_CONST;
GtkStatusIcon *xfpm_ac_adapter_new            (gboolean visible);
void           xfpm_ac_adapter_monitor        (XfpmAcAdapter *adapter,
                                               SystemFormFactor factor);
void           xfpm_ac_adapter_set_sleep_info (XfpmAcAdapter *adapter,
                                               guint8 sleep_info);
#endif
