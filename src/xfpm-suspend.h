/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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
#ifndef __XFPM_SUSPEND_H
#define __XFPM_SUSPEND_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_SUSPEND        (xfpm_suspend_get_type () )
#define XFPM_SUSPEND(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_SUSPEND, XfpmSuspend))
#define XFPM_IS_SUSPEND(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_SUSPEND))

typedef struct XfpmSuspendPrivate XfpmSuspendPrivate;

typedef struct
{
    GObject parent;
    XfpmSuspendPrivate *priv;
} XfpmSuspend;

typedef struct
{
    GObjectClass parent_class;
} XfpmSuspendClass;

typedef enum
{
 SUDO_NOT_INITIAZED,
 SUDO_AVAILABLE,
 SUDO_FAILED
} HelperState;

typedef enum
{
 XFPM_ASK_0 = 0,
 XFPM_SUSPEND,
 XFPM_HIBERNATE,
} XfpmActionType;

typedef enum
{
 PASSWORD_RETRY,
 PASSWORD_SUCCEED,
 PASSWORD_FAILED
} XfpmPassState;

GType xfpm_suspend_get_type (void) G_GNUC_CONST;

XfpmSuspend *xfpm_suspend_get (void);

gboolean xfpm_suspend_can_suspend   (void);
gboolean xfpm_suspend_can_hibernate (void);

gboolean xfpm_suspend_password_required (XfpmSuspend *suspend);

gboolean xfpm_suspend_sudo_try_action (XfpmSuspend       *suspend,
                                       XfpmActionType     type,
                                       GError           **error);

XfpmPassState xfpm_suspend_sudo_send_password (XfpmSuspend *suspend,
                                               const gchar *password);

HelperState xfpm_suspend_sudo_get_state (XfpmSuspend *suspend);

G_END_DECLS

#endif /* __XFPM_SUSPEND_H */
