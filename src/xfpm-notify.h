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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __XFPM_NOTIFY_H
#define __XFPM_NOTIFY_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

G_BEGIN_DECLS

#define XFPM_TYPE_NOTIFY (xfpm_notify_get_type ())
#define XFPM_NOTIFY(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_NOTIFY, XfpmNotify))
#define XFPM_IS_NOTIFY(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_NOTIFY))

typedef enum
{
  XFPM_NOTIFY_LOW = 0,
  XFPM_NOTIFY_NORMAL,
  XFPM_NOTIFY_CRITICAL
} XfpmNotifyUrgency;

typedef struct XfpmNotifyPrivate XfpmNotifyPrivate;

typedef struct
{
  GObject parent;
  XfpmNotifyPrivate *priv;
} XfpmNotify;

typedef struct
{
  GObjectClass parent_class;
} XfpmNotifyClass;

GType
xfpm_notify_get_type (void) G_GNUC_CONST;
XfpmNotify *
xfpm_notify_new (void);
void
xfpm_notify_show_notification (XfpmNotify *notify,
                               const gchar *title,
                               const gchar *text,
                               const gchar *icon_name,
                               XfpmNotifyUrgency urgency);
NotifyNotification *
xfpm_notify_new_notification (XfpmNotify *notify,
                              const gchar *title,
                              const gchar *text,
                              const gchar *icon_name,
                              XfpmNotifyUrgency urgency) G_GNUC_MALLOC;
void
xfpm_notify_add_action_to_notification (XfpmNotify *notify,
                                        NotifyNotification *n,
                                        const gchar *id,
                                        const gchar *action_label,
                                        NotifyActionCallback callback,
                                        gpointer data);
void
xfpm_notify_present_notification (XfpmNotify *notify,
                                  NotifyNotification *n);
void
xfpm_notify_critical (XfpmNotify *notify,
                      NotifyNotification *n);
void
xfpm_notify_close_critical (XfpmNotify *notify);
void
xfpm_notify_close_normal (XfpmNotify *notify);

G_END_DECLS

#endif /* __XFPM_NOTIFY_H */
