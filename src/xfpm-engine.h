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

#ifndef __XFPM_ENGINE_H
#define __XFPM_ENGINE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_ENGINE        (xfpm_engine_get_type () )
#define XFPM_ENGINE(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_ENGINE, XfpmEngine))
#define XFPM_IS_ENGINE(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_ENGINE))

typedef struct XfpmEnginePrivate XfpmEnginePrivate;

typedef struct
{
    GObject		  parent;
    XfpmEnginePrivate	 *priv;
    
} XfpmEngine;

typedef struct
{
    GObjectClass 	  parent_class;
    
    void                  (*on_battery_changed)       (XfpmEngine *engine,
						       gboolean    on_battery);
    
} XfpmEngineClass;

GType        	  	  xfpm_engine_get_type        (void) G_GNUC_CONST;
XfpmEngine       	 *xfpm_engine_new             (void);

void              	  xfpm_engine_get_info        (XfpmEngine *engine,
						       gboolean *system_laptop,
						       gboolean *user_privilege,
						       gboolean *can_suspend,
						       gboolean *can_hibernate,
						       gboolean *has_lcd_brightness,
						       gboolean *has_lid);
G_END_DECLS

#endif /* __XFPM_ENGINE_H */
