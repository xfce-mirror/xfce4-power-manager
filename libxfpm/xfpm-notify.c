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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include <libnotify/notify.h>

#include "xfpm-common.h"
#include "xfpm-notify.h"

static void xfpm_notify_finalize   (GObject *object);

static NotifyNotification * xfpm_notify_new_notification_internal (const gchar *title, 
								   const gchar *message, 
								   const gchar *icon_name, 
								   guint timeout, 
								   XfpmNotifyUrgency urgency, 
								   GtkStatusIcon *icon) G_GNUC_MALLOC;

#define XFPM_NOTIFY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_NOTIFY, XfpmNotifyPrivate))

struct XfpmNotifyPrivate
{
    NotifyNotification *notification;
    NotifyNotification *critical;
};

static gpointer xfpm_notify_object = NULL;

G_DEFINE_TYPE(XfpmNotify, xfpm_notify, G_TYPE_OBJECT)

static void
xfpm_notify_class_init(XfpmNotifyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_notify_finalize;


    g_type_class_add_private(klass,sizeof(XfpmNotifyPrivate));
}

static void
xfpm_notify_init(XfpmNotify *notify)
{
    notify->priv = XFPM_NOTIFY_GET_PRIVATE(notify);
    
    notify->priv->notification = NULL;
    notify->priv->critical = NULL;
}

static void
xfpm_notify_finalize(GObject *object)
{
    XfpmNotify *notify;

    notify = XFPM_NOTIFY(object);
    
    xfpm_notify_close_normal (notify);
    xfpm_notify_close_critical (notify);
    
    G_OBJECT_CLASS(xfpm_notify_parent_class)->finalize(object);
}

static void
xfpm_notify_set_notification_icon (NotifyNotification *n, const gchar *icon_name )
{
    GdkPixbuf *pix = xfpm_load_icon (icon_name, 48);
    
    if ( pix )
    {
	notify_notification_set_icon_from_pixbuf (n, 
						  pix);
	g_object_unref ( G_OBJECT(pix));
    }
}

static NotifyNotification *
xfpm_notify_new_notification_internal (const gchar *title, const gchar *message,
				       const gchar *icon_name, guint timeout,
				       XfpmNotifyUrgency urgency, GtkStatusIcon *icon)
{
    NotifyNotification *n;
    
    n = notify_notification_new (title, message, NULL, NULL);
    
    if ( icon_name )
    	xfpm_notify_set_notification_icon (n, icon_name);
	
    if ( icon )
    	notify_notification_attach_to_status_icon (n, icon);
	
    notify_notification_set_urgency (n, (NotifyUrgency)urgency);
    notify_notification_set_timeout (n, timeout);
    
    return n;
}

static void
xfpm_notify_closed_cb (NotifyNotification *n, XfpmNotify *notify)
{
    notify->priv->notification = NULL;
    g_object_unref (G_OBJECT (n));
}

static void
xfpm_notify_close_critical_cb (NotifyNotification *n, XfpmNotify *notify)
{
    notify->priv->critical = NULL;
    g_object_unref (G_OBJECT (n));
}

static gboolean
xfpm_notify_show (NotifyNotification *n)
{
    notify_notification_show (n, NULL);
    return FALSE;
}

static void
xfpm_notify_close_notification (XfpmNotify *notify )
{
    if ( notify->priv->notification )
    {
    	if (!notify_notification_close (notify->priv->notification, NULL))
	    g_warning ("Failed to close notification\n");
	
	g_object_unref (G_OBJECT(notify->priv->notification) );
	notify->priv->notification  = NULL;
    }
}

XfpmNotify *
xfpm_notify_new (void)
{
    if ( xfpm_notify_object != NULL )
    {
	g_object_ref (xfpm_notify_object);
    }
    else
    {
	xfpm_notify_object = g_object_new (XFPM_TYPE_NOTIFY, NULL);
	g_object_add_weak_pointer (xfpm_notify_object, &xfpm_notify_object);
    }
    return XFPM_NOTIFY (xfpm_notify_object);
}

void xfpm_notify_show_notification (XfpmNotify *notify, const gchar *title,
				    const gchar *text,  const gchar *icon_name,
				    gint timeout, gboolean simple,
				    XfpmNotifyUrgency urgency, GtkStatusIcon *icon)
{
    NotifyNotification *n;
    
    if ( !simple )
        xfpm_notify_close_notification (notify);
    
    n = xfpm_notify_new_notification_internal (title, 
				               text, icon_name, 
					       timeout, urgency, 
					       icon);
					       
    xfpm_notify_present_notification (notify, n, simple);
}

NotifyNotification *xfpm_notify_new_notification (XfpmNotify *notify,
						  const gchar *title,
						  const gchar *text,
						  const gchar *icon_name,
						  guint timeout,
						  XfpmNotifyUrgency urgency,
						  GtkStatusIcon *icon)
{
    NotifyNotification *n = xfpm_notify_new_notification_internal (title, 
							           text, icon_name, 
								   timeout, urgency, 
								   icon);
    return n;
}

void xfpm_notify_add_action_to_notification (XfpmNotify *notify, NotifyNotification *n,
					    const gchar *id, const gchar *action_label,
					    NotifyActionCallback callback, gpointer data)
{
    g_return_if_fail (XFPM_IS_NOTIFY(notify));
    
    notify_notification_add_action (n, id, action_label,
				   (NotifyActionCallback)callback,
				    data, NULL);
    
}

void xfpm_notify_present_notification (XfpmNotify *notify, NotifyNotification *n, gboolean simple)
{
    g_return_if_fail (XFPM_IS_NOTIFY(notify));
    
    if ( !simple )
        xfpm_notify_close_notification (notify);
    
    if ( !simple )
    {
	g_signal_connect (G_OBJECT(n),"closed",
			G_CALLBACK(xfpm_notify_closed_cb), notify);
	notify->priv->notification = n;
    }
    
    g_idle_add ((GSourceFunc) xfpm_notify_show, n);
}

void xfpm_notify_critical (XfpmNotify *notify, NotifyNotification *n)
{
    g_return_if_fail (XFPM_IS_NOTIFY (notify));

    xfpm_notify_close_critical (notify);
    
    notify->priv->critical = n;
    
    g_signal_connect (G_OBJECT (n), "closed", 
		      G_CALLBACK (xfpm_notify_close_critical_cb), notify);
		      
    g_idle_add ((GSourceFunc) xfpm_notify_show, n);
}

void xfpm_notify_close_critical (XfpmNotify *notify)
{
    g_return_if_fail (XFPM_IS_NOTIFY (notify));
    
    if ( notify->priv->critical )
    {
    	if (!notify_notification_close (notify->priv->critical, NULL))
	    g_warning ("Failed to close notification\n");
	
	g_object_unref (G_OBJECT(notify->priv->critical) );
	notify->priv->critical  = NULL;
    }
}

void xfpm_notify_close_normal  (XfpmNotify *notify)
{
    g_return_if_fail (XFPM_IS_NOTIFY (notify));
    
    xfpm_notify_close_notification (notify);
}
