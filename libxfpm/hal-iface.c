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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>


#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "xfpm-string.h"

#include "hal-iface.h"

/* Init */
static void hal_iface_class_init (HalIfaceClass *klass);
static void hal_iface_init       (HalIface *iface);
static void hal_iface_finalize   (GObject *object);

static void hal_iface_get_property(GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec);

static gpointer  hal_iface_object = NULL;

#define HAL_IFACE_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_IFACE, HalIfacePrivate))

struct HalIfacePrivate
{
    DBusGConnection 	*bus;
    
    gboolean             connected;
    gboolean             can_suspend;
    gboolean             can_hibernate;
    gboolean             cpu_freq_iface_can_be_used;
    gboolean             caller_privilege;
    
};

enum
{
    PROP_0,
    PROP_CALLER_PRIVILEGE,
    PROP_CAN_SUSPEND,
    PROP_CAN_HIBERNATE,
    PROP_CPU_FREQ_IFACE_CAN_BE_USED
};
    
G_DEFINE_TYPE(HalIface, hal_iface, G_TYPE_OBJECT)

static void
hal_iface_class_init(HalIfaceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->get_property = hal_iface_get_property;

    g_object_class_install_property(object_class,
				    PROP_CALLER_PRIVILEGE,
				    g_param_spec_boolean("caller-privilege",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));
							 
    g_object_class_install_property(object_class,
				    PROP_CAN_SUSPEND,
				    g_param_spec_boolean("can-suspend",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));
							 
    g_object_class_install_property(object_class,
				    PROP_CAN_HIBERNATE,
				    g_param_spec_boolean("can-hibernate",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));
							 
    g_object_class_install_property(object_class,
				    PROP_CPU_FREQ_IFACE_CAN_BE_USED,
				    g_param_spec_boolean("cpu-freq-iface",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

    object_class->finalize = hal_iface_finalize;

    g_type_class_add_private(klass,sizeof(HalIfacePrivate));
}

static void
hal_iface_init(HalIface *iface)
{
    iface->priv = HAL_IFACE_GET_PRIVATE(iface);
    
    iface->priv->bus 			= NULL;
    iface->priv->connected		= FALSE;
    iface->priv->can_suspend    	= FALSE;
    iface->priv->can_hibernate  	= FALSE;
    iface->priv->caller_privilege       = FALSE;
    iface->priv->cpu_freq_iface_can_be_used = FALSE;
}

static void hal_iface_get_property(GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
    HalIface *iface;
    iface = HAL_IFACE(object);

    switch(prop_id)
    {
	case PROP_CALLER_PRIVILEGE:
	    g_value_set_boolean (value, iface->priv->caller_privilege );
	    break;
	case PROP_CAN_SUSPEND:
	    g_value_set_boolean (value, iface->priv->can_suspend );
	    break;
	case PROP_CAN_HIBERNATE:
	    g_value_set_boolean (value, iface->priv->can_hibernate);
	    break;
	case PROP_CPU_FREQ_IFACE_CAN_BE_USED:
	    g_value_set_boolean (value, iface->priv->cpu_freq_iface_can_be_used);
	    break;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
	    break;
    }
}

static void
hal_iface_finalize(GObject *object)
{
    HalIface *iface;

    iface = HAL_IFACE(object);

    G_OBJECT_CLASS(hal_iface_parent_class)->finalize(object);
}

HalIface *
hal_iface_new(void)
{
    if ( hal_iface_object != NULL )
    {
	g_object_ref (hal_iface_object);
    }
    else
    {
	hal_iface_object = g_object_new (HAL_TYPE_IFACE, NULL);
	g_object_add_weak_pointer (hal_iface_object, &hal_iface_object);
    }
    
    return HAL_IFACE (hal_iface_object);
}

static gboolean
hal_iface_check_interface (HalIface *iface, const gchar *interface)
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
    	dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(iface->priv->bus),
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
get_property_bool (HalIface *iface, const gchar *property)
{
    GError *error = NULL;
    gboolean ret;
    DBusGProxy *proxy = dbus_g_proxy_new_for_name (iface->priv->bus, 
						   "org.freedesktop.Hal",
					           "/org/freedesktop/Hal/devices/computer",
					           "org.freedesktop.Hal.Device");
    dbus_g_proxy_call (proxy, "GetPropertyBoolean", &error,
		       G_TYPE_STRING, property,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &ret,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_critical ("Unable to get bool property: %s\n", error->message);
	g_error_free (error);
    }
    
    g_object_unref (proxy);
    
    return ret;
}

static void
hal_iface_power_management_check (HalIface *iface)
{
    iface->priv->caller_privilege =
	hal_iface_check_interface (iface,  "org.freedesktop.Hal.Device.SystemPowerManagement");
    
    iface->priv->can_suspend   = get_property_bool (iface, "power_management.can_suspend");
    iface->priv->can_hibernate = get_property_bool (iface, "power_management.can_hibernate");
}

static void
hal_iface_cpu_check (HalIface *iface)
{
    iface->priv->cpu_freq_iface_can_be_used  = 
	hal_iface_check_interface (iface, "org.freedesktop.Hal.Device.CPUFreq");
}

gboolean
hal_iface_connect (HalIface *iface)
{
    g_return_val_if_fail (HAL_IS_IFACE (iface), FALSE);
    
    GError *error = NULL;
    
    iface->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Unable to get system bus %s.", error->message);
	g_error_free (error);
	return FALSE;
    }
    
    iface->priv->connected = TRUE;
    hal_iface_power_management_check (iface);
    hal_iface_cpu_check (iface);
    
    return TRUE;
}

gboolean hal_iface_shutdown (HalIface *iface, const gchar *shutdown, GError **gerror)
{
    g_return_val_if_fail (HAL_IS_IFACE(iface), FALSE);
    g_return_val_if_fail (iface->priv->connected, FALSE);
    g_return_val_if_fail (iface->priv->caller_privilege, FALSE);
    
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
    
    reply = dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(iface->priv->bus),
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

gchar **hal_iface_get_cpu_governors (HalIface *iface, GError **error)
{
    gchar **governors = NULL;
    
    g_return_val_if_fail (HAL_IS_IFACE(iface), NULL);
    g_return_val_if_fail (iface->priv->connected, NULL);
    g_return_val_if_fail (iface->priv->caller_privilege, NULL);
    g_return_val_if_fail (iface->priv->cpu_freq_iface_can_be_used, NULL);
    
    DBusGProxy *proxy = dbus_g_proxy_new_for_name (iface->priv->bus,
						   "org.freedesktop.Hal",
						   "/org/freedesktop/Hal/devices/computer",
						   "org.freedesktop.Hal.Device.CPUFreq");
    if ( !proxy )
    {
	g_critical ("Failed to create proxy");
	goto out;
    }
    
    dbus_g_proxy_call (proxy, "GetCPUFreqAvailableGovernors", error,
		       G_TYPE_INVALID,
		       G_TYPE_STRV, &governors,
		       G_TYPE_INVALID);
    
    g_object_unref (proxy);
    
out:
    return governors;
}
							  
gchar *hal_iface_get_cpu_current_governor (HalIface *iface, GError **error)
{
    gchar *governor;
    
    g_return_val_if_fail (HAL_IS_IFACE(iface), NULL);
    g_return_val_if_fail (iface->priv->connected, NULL);
    g_return_val_if_fail (iface->priv->caller_privilege, NULL);
    g_return_val_if_fail (iface->priv->cpu_freq_iface_can_be_used, NULL);
    
    DBusGProxy *proxy = dbus_g_proxy_new_for_name (iface->priv->bus,
						   "org.freedesktop.Hal",
						   "/org/freedesktop/Hal/devices/computer",
						   "org.freedesktop.Hal.Device.CPUFreq");
    if ( !proxy )
    {
	g_critical ("Failed to create proxy");
	goto out;
    }
    
    dbus_g_proxy_call (proxy, "GetCPUFreqGovernor", error,
		       G_TYPE_INVALID,
		       G_TYPE_STRING, &governor,
		       G_TYPE_INVALID);
    
    g_object_unref (proxy);
    
out:
    return governor;
}
							
gboolean hal_iface_set_cpu_governor (HalIface *iface, const gchar *governor, GError **error)
{
    g_return_val_if_fail (HAL_IS_IFACE(iface), FALSE);
    g_return_val_if_fail (iface->priv->connected, FALSE);
    g_return_val_if_fail (iface->priv->caller_privilege, FALSE);
    g_return_val_if_fail (iface->priv->cpu_freq_iface_can_be_used, FALSE);
    
    DBusGProxy *proxy = dbus_g_proxy_new_for_name (iface->priv->bus,
						   "org.freedesktop.Hal",
						   "/org/freedesktop/Hal/devices/computer",
						   "org.freedesktop.Hal.Device.CPUFreq");
    if ( !proxy )
    {
	g_critical ("Failed to create proxy");
	goto out;
    }
    
    dbus_g_proxy_call (proxy, "SetCPUFreqGovernor", error,
		       G_TYPE_STRING, &governor,
		       G_TYPE_INVALID,
		       G_TYPE_INVALID);
    
    g_object_unref (proxy);
    
out:
    
    return FALSE;
}

void hal_iface_free_string_array (gchar **array)
{
    gint i;

    if (array == NULL)	
	    return;
    
    for (i=0; array[i]; i++) 
	g_free (array[i]);
    
    g_free (array);
}
