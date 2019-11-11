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

#ifndef __XFPM_INHIBIT_H
#define __XFPM_INHIBIT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_INHIBIT        (xfpm_inhibit_get_type () )
#define XFPM_INHIBIT(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_INHIBIT, XfpmInhibit))
#define XFPM_IS_INHIBIT(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_INHIBIT))

typedef struct XfpmInhibitPrivate XfpmInhibitPrivate;

typedef struct
{
    GObject               parent;
    XfpmInhibitPrivate   *priv;
} XfpmInhibit;

typedef struct
{
    GObjectClass     parent_class;

    /* signals */
    void            (*has_inhibit_changed)       (XfpmInhibit *inhibit,
                                                  gboolean is_inhibit);
    void            (*inhibitors_list_changed)   (XfpmInhibit *inhibit,
                                                  gboolean is_inhibit);
} XfpmInhibitClass;

GType              xfpm_inhibit_get_type         (void) G_GNUC_CONST;
GType              xfpm_inhibit_error_get_type   (void) G_GNUC_CONST;
GQuark             xfpm_inhibit_get_error_quark  ();
XfpmInhibit       *xfpm_inhibit_new              (void);
const gchar      **xfpm_inhibit_get_inhibit_list (XfpmInhibit *inhibit);

G_END_DECLS

#endif /* __XFPM_INHIBIT_H */
