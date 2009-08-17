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

#ifndef __XFPM_SESSION_H
#define __XFPM_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_SESSION        (xfpm_session_get_type () )
#define XFPM_SESSION(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_SESSION, XfpmSession))
#define XFPM_IS_SESSION(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_SESSION))

typedef struct XfpmSessionPrivate XfpmSessionPrivate;

typedef struct
{
    GObject         		 parent;
    XfpmSessionPrivate     	*priv;
    
} XfpmSession;

typedef struct
{
    GObjectClass 		 parent_class;
    
    void                        (*session_die)		      (XfpmSession *session);
    
} XfpmSessionClass;

GType  		      		 xfpm_session_get_type        (void) G_GNUC_CONST;

XfpmSession       		*xfpm_session_new             (void);

void				 xfpm_session_real_init	      (XfpmSession *session);

void			         xfpm_session_set_client_id   (XfpmSession *session,
							       const gchar *client_id);
							    
/* This is used to change the restart Style */
void				 xfpm_session_quit	      (XfpmSession *session);

gboolean                         xfpm_session_shutdown        (XfpmSession *session);

gboolean 			 xfpm_session_reboot 	      (XfpmSession *session);

gboolean			 xfpm_session_ask_shutdown    (XfpmSession *session);

G_END_DECLS

#endif /* __XFPM_SESSION_H */
