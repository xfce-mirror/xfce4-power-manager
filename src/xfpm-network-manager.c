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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gio/gio.h>

#include "xfpm-network-manager.h"

/*
 * Inform the Network Manager when we do suspend/hibernate
 */
gboolean 	xfpm_network_manager_sleep  	(gboolean sleep)
{
#ifdef WITH_NETWORK_MANAGER

    GDBusConnection *bus   = NULL;
    GDBusProxy      *proxy = NULL;
    GError          *error = NULL;

    bus = g_bus_get_sync ( G_BUS_TYPE_SYSTEM, NULL, &error);

    if ( error )
    {
	g_warning("%s", error->message);
	g_error_free (error);
	return FALSE;
    }

    proxy = g_dbus_proxy_new_sync (bus,
				   G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
				   G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
				   NULL,
				   "org.freedesktop.NetworkManager",
				   "/org/freedesktop/NetworkManager",
				   "org.freedesktop.NetworkManager",
				   NULL,
				   NULL);

    if (!proxy)
    {
	g_warning ("Failed to create proxy for Network Manager interface");
	return FALSE;
    }

    g_dbus_proxy_call (proxy, "Sleep", g_variant_new("(b)", sleep),
                       G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    g_object_unref (G_OBJECT(proxy));
    g_object_unref (bus);

    /* Sleep 0.5 second to allow the nm applet to disconnect*/
    g_usleep (500000);

#endif /* WITH_NETWORK_MANAGER */
    return TRUE;
}
