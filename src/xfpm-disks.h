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

#ifndef __XFPM_DISKS_H
#define __XFPM_DISKS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_DISKS        (xfpm_disks_get_type () )
#define XFPM_DISKS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_DISKS, XfpmDisks))
#define XFPM_IS_DISKS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_DISKS))

typedef struct XfpmDisksPrivate XfpmDisksPrivate;

typedef struct
{
    GObject         	  parent;
    XfpmDisksPrivate     *priv;
    
} XfpmDisks;

typedef struct
{
    GObjectClass 	  parent_class;
    
} XfpmDisksClass;

GType        		  xfpm_disks_get_type        (void) G_GNUC_CONST;

XfpmDisks                *xfpm_disks_new             (void);

gboolean		  xfpm_disks_get_can_spin    (XfpmDisks *disks);

gboolean		  xfpm_disks_kit_is_running  (XfpmDisks *disks);

G_END_DECLS

#endif /* __XFPM_DISKS_H */
