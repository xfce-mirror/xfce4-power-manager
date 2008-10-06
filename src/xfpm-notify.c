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

#include "xfpm-common.h"
#include "xfpm-notify.h"

#ifdef HAVE_LIBNOTIFY
static NotifyNotification *
xfpm_notify_new(const char *title,const char *message,
                const gchar *icon_name,GtkStatusIcon *icon)
{
    NotifyNotification *n;
    if ( icon != NULL ) 
	{
	    n = notify_notification_new_with_status_icon(title,message,icon_name,icon);
	}
	else
	{
	    n = notify_notification_new(title,message,NULL,NULL);
    
	    if ( icon_name != NULL ) {
		    GdkPixbuf *pixbuf = xfpm_load_icon(icon_name,60);	
		    if (pixbuf) {
			    notify_notification_set_icon_from_pixbuf(n,pixbuf);
			    g_object_unref(G_OBJECT(pixbuf));
		    }    
	    }		
	}
	return n;
}

static gboolean
xfpm_notify_send_notification(gpointer data)
{
    NotifyNotification *n = data;
    notify_notification_show(n,NULL);	
	g_object_unref(n);				
	return FALSE;
}

static gboolean
xfpm_notify_send_notification_with_action(gpointer data)
{
    NotifyNotification *n = data;
    notify_notification_show(n,NULL);	
    /* The action callback should call g_object_unref on NotifyNotification object */
	return FALSE;
}

void 
xfpm_notify_simple(const gchar *title,const gchar *message,guint timeout,
                   NotifyUrgency urgency,GtkStatusIcon *icon,
                   const gchar *icon_name,guint8 timeout_to_show) 
{
	
	NotifyNotification *n;
	n = xfpm_notify_new(title,message,icon_name,icon);
	    
	notify_notification_set_urgency(n,urgency);
	notify_notification_set_timeout(n,timeout);
	if ( timeout_to_show != 0 )
	{
	    g_timeout_add_seconds(timeout_to_show,xfpm_notify_send_notification,n);
    }
    else
    {
        xfpm_notify_send_notification(n);
    }
}	

void xfpm_notify_with_action(const gchar *title,
							 const gchar *message,
							 guint timeout,
							 NotifyUrgency urgency,
							 GtkStatusIcon *icon,
							 const gchar *icon_name,
							 const gchar *action_label,
							 guint8 timeout_to_show,
							 NotifyActionCallback notify_callback,
							 gpointer user_data) {
										  	
	NotifyNotification *n;
	n = xfpm_notify_new(title,message,icon_name,icon);
	
	notify_notification_set_urgency(n,urgency);
	notify_notification_set_timeout(n,timeout);
	
	notify_notification_add_action(n,"ok",action_label,
								  (NotifyActionCallback)notify_callback,
								  user_data,NULL);
								  
	if ( timeout_to_show != 0 )
	{
	    g_timeout_add_seconds(timeout_to_show,xfpm_notify_send_notification_with_action,n);
    }
    else
    {
        xfpm_notify_send_notification_with_action(n);
    }							  
}										  	

#endif
