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

#include "xfpm-dbus-messages.h"

static
DBusConnection *xfpm_dbus_get_connection() 
{
    DBusError error;
    DBusConnection *connection;

    dbus_error_init(&error);
    
    connection = dbus_bus_get(DBUS_BUS_SESSION,&error);
    
    if ( !connection ) 
    {
        g_printerr("Dbus connection error: %s\n",error.message);
        dbus_error_free(&error);
        return NULL; 
    }
    else 
    {
        return connection;
    }               
 
    
}    

static DBusMessage *xfpm_dbus_new_signal(const gchar *signal)
{
    DBusMessage *message;    
    message = dbus_message_new_signal(XFPM_PM_ROOT,
                                      XFPM_PM_IFACE,
                                      signal);
    if ( !message )
    {
        g_critical("Failed to create dbus message\n");
        return NULL;
    }    
    else
    {
        return message;
    }    
}    

DBusMessage *xfpm_dbus_new_message(const gchar *service  ,const gchar *remote_object,
                                   const gchar *interface,const gchar *method)
{       
    DBusMessage *message;
    message = dbus_message_new_method_call(service,
                                           remote_object,
                                           interface,
                                           method);
	if (!message) {
	    g_printerr("Cannot create DBus message out of memmory\n");
		return NULL;
	}	

    return message;
}

gboolean xfpm_dbus_send_message(const char *signal)
{
    DBusConnection *connection;
    DBusMessage *message;
    
    connection = xfpm_dbus_get_connection();
    if ( !connection )
    {
        return FALSE;
    }
    
    message = xfpm_dbus_new_signal(signal);
    
    if (!message)
    {
        return FALSE;
    }
        
    gboolean ret =
    dbus_connection_send(connection,
                         message,
                         NULL);
        
    dbus_message_unref(message);
    dbus_connection_unref(connection);
    if ( ret == FALSE )
    {
        g_critical("Failed to send message \n");
        return FALSE;
    } else
    {
        return TRUE;
    }         
                     
}  

gboolean xfpm_dbus_send_message_with_reply (const char *signal,gint *get_reply) {
    
    DBusConnection *connection;
    DBusMessage *message;
    DBusPendingCall *pend;

    connection = xfpm_dbus_get_connection();
    if ( !connection )
    {
        return FALSE;
    }
    
    message = xfpm_dbus_new_signal(signal);
    
    if (!message)
    {
        dbus_connection_unref(connection);
        return FALSE;
    }
    
                
    if(!dbus_connection_send_with_reply(connection,
                                        message,
                                        &pend,
                                        100))
    {
        dbus_message_unref(message);
        dbus_connection_unref(connection);
        return FALSE;
    }   
         
    dbus_pending_call_block(pend);
    if ( !pend )
    {
        dbus_message_unref(message);
        dbus_connection_unref(connection);
        return FALSE;
    }
        
    DBusMessage *reply = dbus_pending_call_steal_reply(pend);
    
    dbus_pending_call_unref(pend);
    
    if ( reply != NULL ) 
    {
        if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
        {
            *get_reply = 1;
        } else 
        {
            *get_reply = 0;
        }
        dbus_message_unref(reply);
    } else     
    {
        *get_reply = 0;
        
    }    
  
    dbus_message_unref(message);
    dbus_connection_unref(connection);
    return TRUE;
}  
