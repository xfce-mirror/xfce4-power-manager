/*
 * Copyright (C) 2012 Nick Schermer <nick@xfce.org>
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
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus-glib.h>

#include "xfpm-sm-client.h"
#include "xfpm-dbus-monitor.h"



static void xfpm_sm_client_finalize (GObject *object);



struct _XfpmSMClientClass
{
  GObjectClass __parent__;
};

struct _XfpmSMClient
{
  GObject  __parent__;
};



G_DEFINE_TYPE (XfpmSMClient, xfpm_sm_client, G_TYPE_OBJECT)



static void
xfpm_sm_client_class_init (XfpmSMClientClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = xfpm_sm_client_finalize;
}

static void
xfpm_sm_client_init (XfpmSMClient *sm_client)
{

}

static void
xfpm_sm_client_finalize (GObject *object)
{
    G_OBJECT_CLASS (xfpm_sm_client_parent_class)->finalize (object);
}

XfpmSMClient *
xfpm_sm_client_new (void)
{
    static gpointer sm_client = NULL;

    if (G_LIKELY (sm_client != NULL))
    {
        g_object_ref (sm_client);
    }
    else
    {
        sm_client = g_object_new (XFPM_TYPE_SM_CLIENT, NULL);
        g_object_add_weak_pointer (sm_client, &sm_client);
    }

    return XFPM_SM_CLIENT (sm_client);
}

gboolean
xfpm_sm_client_can_shutdown (XfpmSMClient *sm_client,
                             gboolean *can_shutdown,
                             GError **error)
{
    g_return_val_if_fail (XFPM_IS_SM_CLIENT (sm_client), FALSE);
    g_return_val_if_fail (can_shutdown != NULL, FALSE);

    return TRUE;
}

gboolean
xfpm_sm_client_can_restart (XfpmSMClient *sm_client,
                            gboolean *can_restart,
                            GError **error)
{
    g_return_val_if_fail (XFPM_IS_SM_CLIENT (sm_client), FALSE);
    g_return_val_if_fail (can_restart != NULL, FALSE);

    return TRUE;
}

gboolean
xfpm_sm_client_shutdown (XfpmSMClient *sm_client,
                         GError **error)
{
    g_return_val_if_fail (XFPM_IS_SM_CLIENT (sm_client), FALSE);

    return TRUE;
}

gboolean
xfpm_sm_client_restart (XfpmSMClient *sm_client,
                        GError **error)
{
    g_return_val_if_fail (XFPM_IS_SM_CLIENT (sm_client), FALSE);

    return TRUE;
}
