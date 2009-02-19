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

#ifndef __XFPM_SETTINGS_H
#define __XFPM_SETTINGS_H

GtkWidget *xfpm_settings_new(XfconfChannel *channel,
                             gboolean is_laptop,
                             guint8 power_management,
                             gboolean dpms_capable,
                             guint8 govs,
                             guint8 switch_buttons,
                             gboolean lcd,
                             gboolean ups_found,
                             guint32 socket_id);


#endif /* __XFPM_SETTINGS_H */
