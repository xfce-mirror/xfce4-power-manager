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

#ifndef __XFPM_IDLE_H
#define __XFPM_IDLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_IDLE        (xfpm_idle_get_type () )
#define XFPM_IDLE(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_IDLE, XfpmIdle))
#define XFPM_IS_IDLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_IDLE))

enum
{
    TIMEOUT_INPUT = 0,
    TIMEOUT_BRIGHTNESS_ON_AC,
    TIMEOUT_BRIGHTNESS_ON_BATTERY,
    TIMEOUT_INACTIVITY_ON_AC,
    TIMEOUT_INACTIVITY_ON_BATTERY
};

typedef struct XfpmIdlePrivate XfpmIdlePrivate;

typedef struct
{
    GObject		  parent;
    XfpmIdlePrivate	 *priv;
    
} XfpmIdle;

typedef struct
{
    GObjectClass 	  parent_class;
    
    void                 (*alarm_timeout)	    (XfpmIdle *idle,
						     guint id);
						     
    void		 (*reset)		    (XfpmIdle *idle);
    
} XfpmIdleClass;

GType        		  xfpm_idle_get_type        (void) G_GNUC_CONST;
XfpmIdle       		 *xfpm_idle_new             (void);

gboolean                  xfpm_idle_set_alarm       (XfpmIdle *idle,
						     guint id,
						     guint timeout);
						     
void                      xfpm_idle_alarm_reset_all (XfpmIdle *idle);

gboolean                  xfpm_idle_free_alarm      (XfpmIdle *idle,
						     guint id);
G_END_DECLS

#endif /* __XFPM_IDLE_H */
