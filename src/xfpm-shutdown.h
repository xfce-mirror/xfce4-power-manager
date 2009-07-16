/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#ifndef __XFPM_SHUTDOWN_H
#define __XFPM_SHUTDOWN_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_SHUTDOWN        (xfpm_shutdown_get_type () )
#define XFPM_SHUTDOWN(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_SHUTDOWN, XfpmShutdown))
#define XFPM_IS_SHUTDOWN(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_SHUTDOWN))

typedef enum
{
    SYSTEM_CAN_HIBERNATE         =  (1<<0),
    SYSTEM_CAN_SUSPEND           =  (1<<1),
    
} SystemPowerManagement;

typedef struct XfpmShutdownPrivate XfpmShutdownPrivate;

typedef struct
{
    GObject		  parent;
    XfpmShutdownPrivate	 *priv;
	
} XfpmShutdown;

typedef struct
{
    GObjectClass 	  parent_class;
    
    void		  (*waking_up)			(XfpmShutdown *shutdown);
	
} XfpmShutdownClass;

GType        		  xfpm_shutdown_get_type        (void) G_GNUC_CONST;
XfpmShutdown       	 *xfpm_shutdown_new             (void);

gboolean                  xfpm_shutdown_add_callback    (XfpmShutdown *shutdown,
							 GSourceFunc func,
							 gboolean lock_screen,
							 gpointer data);
							 

void                      xfpm_shutdown			(XfpmShutdown *shutdown,
							 GError **error);

void                      xfpm_reboot			(XfpmShutdown *shutdown,
							 GError **error);

void                      xfpm_hibernate                (XfpmShutdown *shutdown,
							 GError **error);

void                      xfpm_suspend                  (XfpmShutdown *shutdown,
							 GError **error);

void                      xfpm_shutdown_ask             (XfpmShutdown *shutdown);

void			  xfpm_shutdown_reload          (XfpmShutdown *shutdown);

G_END_DECLS

#endif /* __XFPM_SHUTDOWN_H */
