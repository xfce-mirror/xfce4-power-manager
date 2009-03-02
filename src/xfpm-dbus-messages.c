/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <aliov@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfpm-dbus-messages.h"
#include "xfpm-debug.h"


gboolean
xfpm_dbus_name_has_owner(DBusConnection *connection,const gchar *service)
{
    DBusError error;
    dbus_error_init(&error);
    
    gboolean ret = dbus_bus_name_has_owner(connection,service,&error);
    
    if ( dbus_error_is_set(&error) )
    {
        XFPM_DEBUG("Failed to get name owner: %s\n",error.message);
        dbus_error_free(&error);
        return FALSE;
    }
    
    if ( ret == FALSE )
    {
        XFPM_DEBUG("%s is not running \n",service);
    }
    
    return ret;
}

static
DBusConnection *xfpm_dbus_get_connection(DBusBusType type) 
{
    DBusError error;
    DBusConnection *connection;

    dbus_error_init(&error);
    
    connection = dbus_bus_get(type,&error);
    
    if ( !connection ) 
    {
        XFPM_DEBUG("Dbus connection error: %s\n",error.message);
        dbus_error_free(&error);
        return NULL; 
    }
    else 
    {
        return connection;
    }               
}    

void
xfpm_dbus_send_nm_message   (const gchar *signal)
{
    DBusConnection *connection;
    DBusMessage *message;
    
    connection = xfpm_dbus_get_connection(DBUS_BUS_SYSTEM);
    if ( !connection )
    {
        return;
    }
    
    if ( !xfpm_dbus_name_has_owner(connection,NM_SERVICE) )
    {
        dbus_connection_unref(connection);
        return;
    }
    
    message = dbus_message_new_method_call(NM_SERVICE,NM_PATH,NM_INTERFACE,signal);
    
    if (!message)
    {
        return;
    }
        
    gboolean ret =
    dbus_connection_send(connection,
                         message,
                         NULL);
        
    dbus_message_unref(message);
    dbus_connection_unref(connection);
    if ( ret == FALSE )
    {
        XFPM_DEBUG("Failed to send message \n");
        return;
    } else
    {
        return;
    }         
}

gboolean xfpm_dbus_register_name(DBusConnection *connection, const gchar *name)
{
    DBusError error;
    
    dbus_error_init(&error);
    
    int ret =
	dbus_bus_request_name(connection, name , 0, &error);
	
	if ( dbus_error_is_set(&error) )
	{
		printf("Error: %s\n",error.message);
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
    
    dbus_error_init(&error);
    
    int ret =
    dbus_bus_release_name(connection, name, &error);
    
    if ( dbus_error_is_set(&error) )
    {
        printf("Error: %s\n",error.message);
        dbus_error_free(&error);
        return FALSE;
    }
    
    if ( ret == -1 ) return FALSE;
    
    return TRUE;
}
