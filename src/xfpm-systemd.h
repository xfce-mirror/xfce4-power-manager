/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
 * * Copyright (C) 2013 Andreas Müller <schnitzeltony@googlemail.com>
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

#ifndef __XFPM_SYSTEMD_H
#define __XFPM_SYSTEMD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define LOGIND_RUNNING() (access ("/run/systemd/seats/", F_OK) >= 0)

#define XFPM_TYPE_SYSTEMD            (xfpm_systemd_get_type () )
#define XFPM_SYSTEMD(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_SYSTEMD, XfpmSystemd))
#define XFPM_IS_SYSTEMD(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_SYSTEMD))

typedef struct XfpmSystemdPrivate XfpmSystemdPrivate;

typedef struct
{
    GObject                 parent;
    XfpmSystemdPrivate      *priv;

} XfpmSystemd;

typedef struct
{
    GObjectClass        parent_class;

} XfpmSystemdClass;

GType               xfpm_systemd_get_type   (void) G_GNUC_CONST;

XfpmSystemd         *xfpm_systemd_new   (void);

void                xfpm_systemd_shutdown   (XfpmSystemd *systemd,
                                             GError **error);

void                xfpm_systemd_reboot (XfpmSystemd *systemd,
                                         GError **error);

G_END_DECLS

#endif /* __XFPM_SYSTEMD_H */
