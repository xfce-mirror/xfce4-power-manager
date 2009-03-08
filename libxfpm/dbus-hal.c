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

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <malloc.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <glib.h>

#include "dbus-hal.h"
#include "xfpm-string.h"

/* Init */
static void dbus_hal_class_init (DbusHalClass *klass);
static void dbus_hal_init       (DbusHal *bus);
static void dbus_hal_finalize   (GObject *object);

static void dbus_hal_get_property(GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec);

#define DBUS_HAL_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), DBUS_TYPE_HAL, DbusHalPrivate))

struct DbusHalPrivate
{
    DBusGConnection *system_bus;
    
    gboolean         connected;
    
    gboolean         can_suspend;
    gboolean         can_hibernate;
    gboolean         power_management;
    
    gboolean         cpu_freq_can_be_used;
    
    guint            power_management_info;
};

enum
{
    PROP_0,
    PROP_POWER_MANAGEMENT_INFO,
};

G_DEFINE_TYPE(DbusHal, dbus_hal, G_TYPE_OBJECT)

static void
dbus_hal_class_init(DbusHalClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = dbus_hal_get_property;

    g_object_class_install_property(object_class,
                                    PROP_POWER_MANAGEMENT_INFO,
                                    g_param_spec_uint("power-management-info",
                                                      NULL, NULL,
                                                      0,
						      2,
						      0,
                                                      G_PARAM_READABLE));

    object_class->finalize = dbus_hal_finalize;

    g_type_class_add_private(klass,sizeof(DbusHalPrivate));
}

static void
dbus_hal_init(DbusHal *bus)
{
    bus->priv = DBUS_HAL_GET_PRIVATE(bus);
    
    bus->priv->system_bus    = NULL;
     
    bus->priv->can_suspend      = FALSE;
    bus->priv->can_hibernate    = FALSE;
    bus->priv->power_management = FALSE;
    
    bus->priv->power_management_info = 0;
}

static void dbus_hal_get_property (GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
    DbusHal *bus;
    bus = DBUS_HAL (object);
    
    switch (prop_id)
    {
    	case PROP_POWER_MANAGEMENT_INFO:
                g_value_set_uint (value, bus->priv->power_management_info);
                break;	
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
    }
    
}

static void
dbus_hal_finalize(GObject *object)
{
    DbusHal *bus;

    bus = DBUS_HAL(object);
    
    if ( bus->priv->system_bus )
    	dbus_g_connection_unref (bus->priv->system_bus);
	
    G_OBJECT_CLASS(dbus_hal_parent_class)->finalize(object);
}

static gboolean
dbus_hal_check_interface (DbusHal *bus, const gchar *interface )
{
    DBusMessage *message;
    DBusMessage *reply;
    DBusError error ;
    
    message = dbus_message_new_method_call ("org.freedesktop.Hal",
					    "/org/freedesktop/Hal",
					    interface,
					    "JustToCheck");
    
    if (!message)
    	return FALSE;
    
    dbus_error_init (&error);
    
    reply = 
    	dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(bus->priv->system_bus),
						   message, 2000, &error);
    dbus_message_unref (message);
    
    if ( reply ) dbus_message_unref (reply);
    
    if ( dbus_error_is_set(&error) )
    {
	if (!xfpm_strcmp(error.name,"org.freedesktop.DBus.Error.UnknownMethod"))
        {
            dbus_error_free(&error);
	    return TRUE;
        }
    }
    
    return FALSE;
}

static gboolean
dbus_hal_get_bool_property (DbusHal *bus, const gchar *property)
{
    DBusMessage *message, *reply = NULL;
    DBusError error;
    
    gboolean property_value;
    
    message = dbus_message_new_method_call ("org.freedesktop.Hal",
					    "/org/freedesktop/Hal/devices/computer",
					    "org.freedesktop.Hal.Device",
					    "GetPropertyBoolean");
    if (!message)
	return FALSE;
	    
    dbus_error_init (&error);
    
    dbus_message_append_args (message, 
			      DBUS_TYPE_STRING, &property, 
			      DBUS_TYPE_INVALID);
    
    reply = 
    	dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(bus->priv->system_bus),
						   message, 2000, &error);
    dbus_message_unref (message);
    
     if ( dbus_error_is_set(&error) )
    {
	g_critical("%s", error.message);
	dbus_error_free (&error);
	return FALSE;
    }    
    
    if ( !reply ) return FALSE;
    
    dbus_message_get_args (reply, NULL,
    			   DBUS_TYPE_BOOLEAN, &property_value,
			   DBUS_TYPE_INVALID);
    
   return property_value;
}

static void
dbus_hal_check (DbusHal *bus)
{
    bus->priv->can_suspend = dbus_hal_get_bool_property (bus, "power_management.can_suspend");
    bus->priv->can_hibernate = dbus_hal_get_bool_property (bus, "power_management.can_hibernate");
    
    bus->priv->power_management = 
    	dbus_hal_check_interface (bus, "org.freedesktop.Hal.Device.SystemPowerManagement");
    
    bus->priv->cpu_freq_can_be_used =
    	dbus_hal_check_interface (bus, "org.freedesktop.Hal.Device.CPUFreq");
	
}

DbusHal *
dbus_hal_new(void)
{
    DbusHal *bus = NULL;
    bus = g_object_new (DBUS_TYPE_HAL, NULL);
    
    GError *error = NULL;
    bus->priv->system_bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
    	g_critical("Unable to connect to the system dbus: %s", error->message);
	bus->priv->connected = FALSE;
	goto out;
    }
    bus->priv->connected = TRUE;
    
    dbus_hal_check (bus);
    
    if ( bus->priv->power_management && bus->priv->can_hibernate )
    	 bus->priv->power_management_info |= SYSTEM_CAN_HIBERNATE;
	
    if ( bus->priv->power_management && bus->priv->can_suspend )
    	 bus->priv->power_management_info |= SYSTEM_CAN_SUSPEND;
    
out:
    return bus;
}

gboolean dbus_hal_shutdown (DbusHal *bus, const gchar *shutdown, GError **gerror)
{
    g_return_val_if_fail (DBUS_IS_HAL(bus), FALSE);
    g_return_val_if_fail (bus->priv->connected, FALSE);
    
    DBusMessage *message, *reply = NULL;
    DBusError error;
    gint exit_code;
    
    message = dbus_message_new_method_call ("org.freedesktop.Hal",
					    "/org/freedesktop/Hal/devices/computer",
					    "org.freedesktop.Hal.Device.SystemPowerManagement",
					    shutdown);
    if ( !message )
    {
	g_set_error ( gerror, 0, 0, "Out of memory");
	return FALSE;
    }
    
    if ( xfpm_strequal("Suspend", shutdown ) )
    {
	gint seconds = 0;
    	dbus_message_append_args (message, DBUS_TYPE_INT32, &seconds, DBUS_TYPE_INVALID);
    }
    
    dbus_error_init (&error);
    
    reply = dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(bus->priv->system_bus),
						       message,
						       -1,
						       &error);
    dbus_message_unref (message);
    
    if ( dbus_error_is_set(&error) )
    {
	dbus_set_g_error (gerror, &error);
	return FALSE;
    }
    
    switch (dbus_message_get_type(reply) )
    {
	case DBUS_MESSAGE_TYPE_METHOD_RETURN:
	    dbus_message_get_args (reply, NULL,
				   DBUS_TYPE_INT32,
				   &exit_code,
				   DBUS_TYPE_INVALID);
	    dbus_message_unref (reply);
	    if ( exit_code == 0) return TRUE;
	    else
	    {
		g_set_error (gerror, 0, 0, "System failed to sleep");
		return FALSE;
	    }
	    break;
	    
	case DBUS_MESSAGE_TYPE_ERROR:
	    dbus_message_unref (reply);
	    g_set_error ( gerror, 0, 0, "Failed to sleep");
	    return FALSE;
	    break;
	default:
	    dbus_message_unref ( reply );
	    g_set_error ( gerror, 0, 0, "Failed to sleep");
	    return FALSE;
    }
    return TRUE;
}

gchar **dbus_hal_get_cpu_available_governors (DbusHal *bus, GError **gerror)
{
    g_return_val_if_fail (DBUS_IS_HAL(bus), NULL);
    g_return_val_if_fail (bus->priv->connected, NULL);
    g_return_val_if_fail (bus->priv->cpu_freq_can_be_used == TRUE, NULL);
    
    DBusMessage *message, *reply = NULL;
    DBusError error;
    char **govs = NULL;
    gint dummy = 0;
    
    message = dbus_message_new_method_call ("org.freedesktop.Hal",
					    "/org/freedesktop/Hal/devices/computer",
					    "org.freedesktop.Hal.Device.CPUFreq",
					    "GetCPUFreqAvailableGovernors");
					    
    if ( !message )
    {
	g_set_error ( gerror, 0, 0, "Out of memory");
	return NULL;
    }
    
    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(bus->priv->system_bus) ,
						       message,
						       -1,
						       &error);
    dbus_message_unref (message);
    
    if ( dbus_error_is_set (&error) )
    {
         dbus_set_g_error (gerror, &error);
         dbus_error_free (&error);
         return FALSE;
    }
    
    if ( !reply ) 
    {
        g_critical("No reply from HAL daemon to get available cpu governors\n");
	
        return NULL;
    }
    
    dbus_message_get_args(reply,NULL,
                          DBUS_TYPE_ARRAY,
			  DBUS_TYPE_STRING,
                          &govs,&dummy,
			  DBUS_TYPE_INVALID,
                          DBUS_TYPE_INVALID);
			  
    dbus_message_unref(reply);
    
    return govs;
}

gchar* dbus_hal_get_cpu_current_governor (DbusHal *bus, GError **gerror)
{
    g_return_val_if_fail (DBUS_IS_HAL(bus), FALSE);
    g_return_val_if_fail (bus->priv->connected, FALSE);
    g_return_val_if_fail (bus->priv->cpu_freq_can_be_used == TRUE, FALSE);
    
    DBusMessage *message, *reply = NULL;
    DBusError error;
    gchar *gov = NULL;
    
    message = dbus_message_new_method_call ("org.freedesktop.Hal",
					    "/org/freedesktop/Hal/devices/computer",
					    "org.freedesktop.Hal.Device.CPUFreq",
					    "GetCPUFreqGovernor");
    
    if ( !message )
    {
    	g_set_error ( gerror, 0, 0, "Out of memory");
	return NULL;
    }
    
    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(bus->priv->system_bus) ,
						       message,
						       -1,
						       &error);
    dbus_message_unref (message);
    
    if ( dbus_error_is_set (&error) )
    {
         dbus_set_g_error (gerror, &error);
         dbus_error_free (&error);
         return NULL;
    }
    
    if ( !reply ) 
    {
        g_critical("No reply from HAL daemon to get available cpu governors\n");
        return NULL;
    }
    
    dbus_message_get_args (reply, NULL, 
    			   DBUS_TYPE_STRING,
			   &gov,
			   DBUS_TYPE_INVALID);
			   
    dbus_message_unref (reply);
    
    return gov;
}

gboolean dbus_hal_set_cpu_governor (DbusHal *bus, const gchar *governor, GError **gerror)
{
    g_return_val_if_fail (DBUS_IS_HAL(bus), FALSE);
    g_return_val_if_fail (bus->priv->connected, FALSE);
    g_return_val_if_fail (bus->priv->cpu_freq_can_be_used == TRUE, FALSE);
    
    DBusMessage *message, *reply = NULL;
    DBusError error;
    
    message = dbus_message_new_method_call ("org.freedesktop.Hal",
					    "/org/freedesktop/Hal/devices/computer",
					    "org.freedesktop.Hal.Device.CPUFreq",
					    "SetCPUFreqGovernor");
    
    if ( !message )
    {
    	g_set_error ( gerror, 0, 0, "Out of memory");
	return FALSE;
    }
    
    dbus_message_append_args (message, DBUS_TYPE_STRING, &governor, DBUS_TYPE_INVALID);
    
    dbus_error_init(&error);
    
    reply = dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection (bus->priv->system_bus), 
						       message, 
						       -1, 
						       &error);
    
    dbus_message_unref(message);
    
    if ( dbus_error_is_set(&error) )
    {
        dbus_set_g_error(gerror,&error);
        dbus_error_free(&error);
        return FALSE;
    }

    if ( !reply ) 
    {
        g_critical("No reply from HAL daemon to set cpu governor");
        return FALSE;
    }
    
    dbus_message_unref(reply);
    
    return TRUE;
}

void dbus_hal_free_string_array (char **array)
{
    if ( !array )
    	return;
	
    dbus_free_string_array (array);
}
