/*
 * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
 * Copyright (C) 2011      Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __XFPM_DBUS_SERVICE_H__
#define __XFPM_DBUS_SERVICE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _XfpmDBusServiceClass XfpmDBusServiceClass;
typedef struct _XfpmDBusService      XfpmDBusService;

#define XFPM_TYPE_DBUS_SERVICE             (xfpm_dbus_service_get_type ())
#define XFPM_DBUS_SERVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFPM_TYPE_DBUS_SERVICE, XfpmDBusService))
#define XFPM_DBUS_SERVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), XFPM_TYPE_DBUS_SERVICE, XfpmDBusServiceClass))
#define XFPM_IS_DBUS_SERVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFPM_TYPE_DBUS_SERVICE))
#define XFPM_IS_DBUS_SERVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), XFPM_TYPE_DBUS_BRIGDE))
#define XFPM_DBUS_SERVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), XFPM_TYPE_DBUS_SERVICE, XfpmDBusServicetClass))

GType xfpm_dbus_service_get_type (void) G_GNUC_CONST;

XfpmDBusService *xfpm_dbus_service_new (void);

G_END_DECLS

#endif /* !__XFPM_DBUS_SERVICE_H__ */
