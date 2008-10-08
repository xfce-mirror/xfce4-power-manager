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

#ifndef __XFPM_LID_SWITCH_H
#define __XFPM_LID_SWITCH_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_LID_SWITCH  (xfpm_lid_switch_get_type())
#define XFPM_LID_SWITCH(o)    (G_TYPE_CHECK_INSTANCE_CAST(o,XFPM_TYPE_LID_SWITCH,XfpmLidSwitch)) 

typedef struct XfpmLidSwitchPrivate XfpmLidSwitchPrivate;

typedef struct
{
    GObject parent;
    XfpmLidSwitchPrivate *priv;
    
} XfpmLidSwitch;


typedef struct
{
    GObjectClass parent_class;
} XfpmLidSwitchClass;

GType          xfpm_lid_switch_get_type(void) G_GNUC_CONST;
XfpmLidSwitch *xfpm_lid_switch_new(void);

G_END_DECLS

#endif
