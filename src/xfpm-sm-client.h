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

#ifndef __XFPM_CONSOLE_KIT_H
#define __XFPM_CONSOLE_KIT_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _XfpmSMClientClass XfpmSMClientClass;
typedef struct _XfpmSMClient      XfpmSMClient;

#define XFPM_TYPE_SM_CLIENT            (xfpm_sm_client_get_type ())
#define XFPM_SM_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFPM_TYPE_SM_CLIENT, XfpmSMClient))
#define XFPM_SM_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFPM_TYPE_SM_CLIENT, XfpmSMClientClass))
#define XFPM_IS_SM_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFPM_TYPE_SM_CLIENT))
#define XFPM_IS_SM_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFPM_TYPE_SM_CLIENT))
#define XFPM_SM_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFPM_TYPE_SM_CLIENT, XfpmSMClientClass))

GType               xfpm_sm_client_get_type             (void) G_GNUC_CONST;

XfpmSMClient        *xfpm_sm_client_new                 (void);

gboolean            xfpm_sm_client_can_shutdown         (XfpmSMClient *sm_client,
                                                         gboolean *can_shutdown,
                                                         GError **error);

gboolean            xfpm_sm_client_can_restart          (XfpmSMClient *sm_client,
                                                         gboolean *can_restart,
                                                         GError **error);

gboolean            xfpm_sm_client_shutdown             (XfpmSMClient *sm_client,
                                                         GError **error);

gboolean            xfpm_sm_client_restart               (XfpmSMClient *sm_client,
                                                          GError **error);

G_END_DECLS

#endif /* __XFPM_CONSOLE_KIT_H */
