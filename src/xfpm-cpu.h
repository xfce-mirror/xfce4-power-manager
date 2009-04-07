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

#ifndef __XFPM_CPU_H
#define __XFPM_CPU_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_CPU        (xfpm_cpu_get_type () )
#define XFPM_CPU(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_CPU, XfpmCpu))
#define XFPM_IS_CPU(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_CPU))

typedef struct XfpmCpuPrivate XfpmCpuPrivate;

typedef struct
{
    GObject		 parent;
    XfpmCpuPrivate	 *priv;
    
} XfpmCpu;

typedef struct
{
    GObjectClass parent_class;
    
} XfpmCpuClass;

GType          xfpm_cpu_get_type        (void) G_GNUC_CONST;
XfpmCpu       *xfpm_cpu_new             (void);
void           xfpm_cpu_reload          (XfpmCpu *cpu);

G_END_DECLS

#endif /* __XFPM_CPU_H */
