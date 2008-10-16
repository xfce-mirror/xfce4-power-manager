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

#ifndef __XFPM_DBUS_MESSAGES
#define __XFPM_DBUS_MESSAGES

#define XFPM_PM_ROOT  "/xfpm/power/manager"
#define XFPM_PM_IFACE "xfpm.power.manager"

#define NM_SERVICE	    "org.freedesktop.NetworkManager"
#define NM_PATH	        "/org/freedesktop/NetworkManager"
#define NM_INTERFACE	"org.freedesktop.NetworkManager"

#include <glib.h>
#include <dbus/dbus.h>

DBusMessage *xfpm_dbus_new_message(const gchar *service  ,const gchar *remote_object,
                                   const gchar *interface,const gchar *method);
                                   
gboolean xfpm_dbus_send_message            (const gchar *signal);
gboolean xfpm_dbus_send_message_with_reply (const gchar *signal,gint *get_reply);
void     xfpm_dbus_send_nm_message         (const gchar *signal);

#endif /* __XFPM_DBUS_MESSAGES */
