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

#include "xfpm-dbus.h"

gboolean
xfpm_dbus_name_has_owner (DBusConnection *connection, const gchar *name)
{
    DBusError error;
    gboolean ret;
    
    dbus_error_init (&error);
    
    ret = dbus_bus_name_has_owner(connection, name, &error);
    
    if ( dbus_error_is_set(&error) )
    {
        g_warning("Failed to get name owner: %s\n",error.message);
        dbus_error_free(&error);
        return FALSE;
    }
    
    return ret;
}

gboolean xfpm_dbus_register_name(DBusConnection *connection, const gchar *name)
{
    DBusError error;
    int ret;
    
    dbus_error_init(&error);
    
    ret =
	dbus_bus_request_name(connection,
			      name,
			      DBUS_NAME_FLAG_DO_NOT_QUEUE,
			      &error);
	
    if ( dbus_error_is_set(&error) )
    {
	g_warning("Error: %s\n",error.message);
	dbus_error_free(&error);
	return FALSE;
    }
    
    if ( ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
    {
	return TRUE;
    }
    
    return FALSE;
}

gboolean xfpm_dbus_release_name(DBusConnection *connection, const gchar *name)
{
    DBusError error;
    int ret;
    
    dbus_error_init(&error);
    
    ret =
	dbus_bus_release_name(connection,
			      name,
			      &error);
    
    if ( dbus_error_is_set(&error) )
    {
        g_warning("Error: %s\n",error.message);
        dbus_error_free(&error);
        return FALSE;
    }
    
    if ( ret == -1 ) return FALSE;
    
    return TRUE;
}


GHashTable *xfpm_dbus_get_interface_properties (DBusGProxy *proxy_prop, const gchar *iface_name)
{
    gboolean ret;
    GError *error = NULL;
    GHashTable *props = NULL;

    props = NULL;

    ret = dbus_g_proxy_call (proxy_prop, "GetAll", &error,
			     G_TYPE_STRING, iface_name,
			     G_TYPE_INVALID,
			     dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props,
			     G_TYPE_INVALID);
			    
    if (!ret) 
    {
	g_warning ("Unable to get interface properties for : %s : %s", iface_name, error->message);
	g_error_free (error);
    }
    
    return props;
}

GValue xfpm_dbus_get_interface_property (DBusGProxy *proxy, const gchar *iface_name, const gchar *prop_name)
{
    gboolean ret;
    GError *error = NULL;
    GValue value = { 0, };

    ret = dbus_g_proxy_call (proxy, "Get", &error,
			     G_TYPE_STRING, iface_name,
			     G_TYPE_STRING, prop_name,
			     G_TYPE_INVALID,
			     G_TYPE_VALUE, &value, G_TYPE_INVALID);
							
    if (!ret) 
    {
	g_warning ("Unable to get property %s on interface  %s : %s", prop_name, iface_name, error->message);
	g_error_free (error);
    }
    
    return value;
}
