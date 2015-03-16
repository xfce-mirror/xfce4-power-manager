/* -*- c-basic-offset: 4 -*- vi:set ts=4 sts=4 sw=4:
 * * Copyright (C) 2015 Xfce Development Team <xfce4-dev@xfce.org>
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

#include <gtk/gtk.h>

#include "xfce-power-manager-dbus.h"


#ifndef __XFPM_SETTINGS_APP_H
#define __XFPM_SETTINGS_APP_H

G_BEGIN_DECLS


#define XFPM_TYPE_SETTINGS_APP            (xfpm_settings_app_get_type())
#define XFPM_SETTINGS_APP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), XFPM_TYPE_SETTINGS_APP, XfpmSettingsApp))
#define XFPM_SETTINGS_APP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), XFPM_TYPE_SETTINGS_APP, XfpmSettingsAppClass))
#define XFPM_IS_SETTINGS_APP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFPM_TYPE_SETTINGS_APP))
#define XFPM_SETTINGS_APP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), XFPM_TYPE_SETTINGS_APP, XfpmSettingsAppClass))

typedef struct _XfpmSettingsApp         XfpmSettingsApp;
typedef struct _XfpmSettingsAppClass    XfpmSettingsAppClass;
typedef struct _XfpmSettingsAppPrivate  XfpmSettingsAppPrivate;

struct _XfpmSettingsApp
{
    GtkApplication               parent;
    XfpmSettingsAppPrivate      *priv;
};

struct _XfpmSettingsAppClass
{
    GtkApplicationClass    parent_class;
};


GType                xfpm_settings_app_get_type        (void) G_GNUC_CONST;

XfpmSettingsApp     *xfpm_settings_app_new             (void);


G_END_DECLS

#endif /* __XFPM_SETTINGS_APP_H */
