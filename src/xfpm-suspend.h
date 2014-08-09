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
#ifndef __XFPM_SUSPEND_H
#define __XFPM_SUSPEND_H

#include <glib-object.h>

typedef enum
{
 XFPM_ASK_0 = 0,
 XFPM_SUSPEND,
 XFPM_HIBERNATE,
} XfpmActionType;

gboolean xfpm_suspend_can_suspend   (void);
gboolean xfpm_suspend_can_hibernate (void);

gboolean xfpm_suspend_try_action (XfpmActionType     type);

#endif /* __XFPM_SUSPEND_H */
