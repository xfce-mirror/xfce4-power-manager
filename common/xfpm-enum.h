 /*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

#ifndef __XFPM_ENUM_H
#define __XFPM_ENUM_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
    SYSTEM_LAPTOP,
    SYSTEM_DESKTOP,
    SYSTEM_SERVER,
    SYSTEM_UNKNOWN

} SystemFormFactor;

typedef enum
{
    LID_KEY   	        	= (1 << 0),
    BRIGHTNESS_KEY_UP   	= (1 << 1),
    BRIGHTNESS_KEY_DOWN  	= (1 << 2),
    SLEEP_KEY 	        	= (1 << 3),
    HIBERNATE_KEY	        = (1 << 4),
    POWER_KEY 	        	= (1 << 5),
    KBD_BRIGHTNESS_KEY_UP	= (1 << 6),
    KBD_BRIGHTNESS_KEY_DOWN	= (1 << 7)

} XfpmKeys;

typedef enum
{
    CPU_UNKNOWN         = (1 << 0),
    CPU_POWERSAVE	= (1 << 1),
    CPU_ONDEMAND	= (1 << 2),
    CPU_PERFORMANCE	= (1 << 3)

} XfpmCpuGovernor;

G_END_DECLS

#endif /*__XFPM_ENUM_H */
