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

#include "xfpm-dbus.h"

gboolean
xfpm_dbus_name_has_owner (GDBusConnection *connection, const gchar *name)
{
    GError *error = NULL;
    const gchar *owner;
    gboolean ret;
    GVariant *var;

    var = g_dbus_connection_call_sync (connection,
                                       "org.freedesktop.DBus",  /* name */
                                       "/org/freedesktop/DBus", /* object path */
                                       "org.freedesktop.DBus",  /* interface */
                                       "GetNameOwner",
                                       g_variant_new ("(s)", name),
                                       G_VARIANT_TYPE ("(s)"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,           /* timeout */
                                       NULL,
                                       &error);
    if (var)
	g_variant_get (var, "(&s)", &owner);
    ret = (owner != NULL);
    g_variant_unref (var);
    
    if ( error )
    {
        g_warning("Failed to get name owner: %s\n",error->message);
        g_error_free(error);
        return FALSE;
    }
    
    return ret;
}

gboolean xfpm_dbus_register_name(GDBusConnection *connection, const gchar *name)
{
    GError *error = NULL;
    guint32 ret;
    GVariant *var;
    
    var = g_dbus_connection_call_sync (connection,
                                       "org.freedesktop.DBus",  /* bus name */
                                       "/org/freedesktop/DBus", /* object path */
                                       "org.freedesktop.DBus",  /* interface name */
                                       "RequestName",           /* method name */
                                       g_variant_new ("(su)",
                                                      name,
                                                      0x4),     /* DBUS_NAME_FLAG_DO_NOT_QUEUE */
                                       G_VARIANT_TYPE ("(u)"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &error);

    if (var)
        g_variant_get (var, "(u)", &ret);
    g_variant_unref (var);
    if ( error )
    {
	g_warning("Error: %s\n",error->message);
	g_error_free(error);
	return FALSE;
    }
    
    if ( ret == 1 ) /* DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER */
    {
	return TRUE;
    }
    
    return FALSE;
}

gboolean xfpm_dbus_release_name(GDBusConnection *connection, const gchar *name)
{
    GError *error = NULL;
    GVariant *var;
    
    var = g_dbus_connection_call_sync (connection,
                                       "org.freedesktop.DBus",  /* bus name */
                                       "/org/freedesktop/DBus", /* object path */
                                       "org.freedesktop.DBus",  /* interface name */
                                       "ReleaseName",           /* method name */
                                       g_variant_new ("(s)", name),
                                       G_VARIANT_TYPE ("(u)"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &error);
    g_variant_unref (var);
    
    if ( error )
    {
        g_warning("Error: %s\n",error->message);
        g_error_free(error);
        return FALSE;
    }
    
    return TRUE;
}
