/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libxfce4util/libxfce4util.h>

#include <glib.h>

#include "libxfpm/hal-monitor.h"
#include "libxfpm/hal-device.h"
#include "libxfpm/hal-monitor.h"

#include "libxfpm/xfpm-string.h"
#include "xfpm-shutdown.h"
#include "xfpm-errors.h"

/* Init */
static void xfpm_shutdown_class_init (XfpmShutdownClass *klass);
static void xfpm_shutdown_init       (XfpmShutdown *shutdown);
static void xfpm_shutdown_finalize   (GObject *object);

static void xfpm_shutdown_get_property (GObject *object,
				        guint prop_id,
				        GValue *value,
				        GParamSpec *pspec);

#define XFPM_SHUTDOWN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_SHUTDOWN, XfpmShutdownPrivate))

struct XfpmShutdownPrivate
{
    DBusGConnection *bus;
    HalMonitor      *monitor;

    gboolean         connected;
    gboolean         can_suspend;
    gboolean         can_hibernate;
    gboolean         caller_privilege;
    
    gboolean         block_shutdown;
};

enum
{
    PROP_0,
    PROP_CALLER_PRIVILEGE,
    PROP_CAN_SUSPEND,
    PROP_CAN_HIBERNATE,
};

static gpointer xfpm_shutdown_object = NULL;

G_DEFINE_TYPE(XfpmShutdown, xfpm_shutdown, G_TYPE_OBJECT)

static gboolean
xfpm_shutdown_check_interface (XfpmShutdown *shutdown, const gchar *interface)
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
    	dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(shutdown->priv->bus),
						   message, 2000, &error);
    dbus_message_unref (message);
    
    if ( reply ) dbus_message_unref (reply);
    
    if ( dbus_error_is_set(&error) )
    {
	if ( !g_strcmp0 (error.name, "org.freedesktop.DBus.Error.UnknownMethod") )
        {
            dbus_error_free(&error);
	    return TRUE;
        }
    }
    return FALSE;
}

static void
xfpm_shutdown_power_management_check (XfpmShutdown *shutdown)
{
    HalDevice *device;
    device = hal_device_new ();
    hal_device_set_udi (device,  "/org/freedesktop/Hal/devices/computer");
    
    shutdown->priv->caller_privilege =
	xfpm_shutdown_check_interface (shutdown,  "org.freedesktop.Hal.Device.SystemPowerManagement");
    
    shutdown->priv->can_suspend   = hal_device_get_property_bool (device, "power_management.can_suspend");
    shutdown->priv->can_hibernate = hal_device_get_property_bool (device, "power_management.can_hibernate");
    g_object_unref (device);
}

static void
xfpm_shutdown_connection_changed_cb (HalMonitor *monitor, gboolean connected, XfpmShutdown *shutdown)
{
    TRACE ("Hal connection changed=%s", xfpm_bool_to_string (connected));
    shutdown->priv->connected = connected;
}

static void
xfpm_shutdown_class_init(XfpmShutdownClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = xfpm_shutdown_get_property;
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
							 
    object_class->finalize = xfpm_shutdown_finalize;
    g_type_class_add_private(klass,sizeof(XfpmShutdownPrivate));
}

static void
xfpm_shutdown_init (XfpmShutdown *shutdown)
{
    GError *error = NULL;
    shutdown->priv = XFPM_SHUTDOWN_GET_PRIVATE(shutdown);
    
    shutdown->priv->bus 			= NULL;
    shutdown->priv->connected			= FALSE;
    shutdown->priv->can_suspend    		= FALSE;
    shutdown->priv->can_hibernate  		= FALSE;
    shutdown->priv->caller_privilege        	= FALSE;
    
    shutdown->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Unable to get system bus %s.", error->message);
	g_error_free (error);
	return;
    }
    
    shutdown->priv->monitor = hal_monitor_new ();
    g_signal_connect (shutdown->priv->monitor, "connection-changed",
		      G_CALLBACK (xfpm_shutdown_connection_changed_cb), shutdown);
    shutdown->priv->connected = hal_monitor_get_connected (shutdown->priv->monitor);
    
    xfpm_shutdown_power_management_check (shutdown);
}

static void xfpm_shutdown_get_property (GObject *object,
				        guint prop_id,
				        GValue *value,
				        GParamSpec *pspec)
{
    XfpmShutdown *shutdown;
    shutdown = XFPM_SHUTDOWN (object);

    switch(prop_id)
    {
	case PROP_CALLER_PRIVILEGE:
	    g_value_set_boolean (value, shutdown->priv->caller_privilege );
	    break;
	case PROP_CAN_SUSPEND:
	    g_value_set_boolean (value, shutdown->priv->can_suspend );
	    break;
	case PROP_CAN_HIBERNATE:
	    g_value_set_boolean (value, shutdown->priv->can_hibernate);
	    break;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
	    break;
    }
}

static void
xfpm_shutdown_finalize(GObject *object)
{
    XfpmShutdown *shutdown;

    shutdown = XFPM_SHUTDOWN(object);
    
    if ( shutdown->priv->bus )
	dbus_g_connection_unref (shutdown->priv->bus);	

    if ( shutdown->priv->monitor )
	g_object_unref (shutdown->priv->monitor);
	
    G_OBJECT_CLASS(xfpm_shutdown_parent_class)->finalize(object);
}

gboolean xfpm_shutdown_internal (DBusConnection *bus, const gchar *shutdown, GError **gerror)
{
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
    
    if ( !g_strcmp0("Suspend", shutdown ) )
    {
	gint seconds = 0;
    	dbus_message_append_args (message, DBUS_TYPE_INT32, &seconds, DBUS_TYPE_INVALID);
    }
    
    dbus_error_init (&error);
    
    reply = dbus_connection_send_with_reply_and_block (bus,
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

static const gchar *
_filter_error_message(const gchar *error)
{
    if( xfpm_strequal ("No back-end for your operating system", error))
    {
        return _("No back-end for your operating system");
    }
    else if ( xfpm_strequal ("No hibernate script found", error) )
    {
        return _("No hibernate script found");
    }
    else if ( xfpm_strequal ("No suspend script found", error) )
    {
	return _("No suspend script found");
    }
    else if ( xfpm_strequal ("No suspend method found", error) )
    {
        return _("No suspend method found");
    }
    else if ( xfpm_strequal ("No hibernate method found", error))
    {
        return _("No hibernate method found");
    }
    else if ( xfpm_strequal ("Out of memory", error) )
    {
	return _("Out of memory");
    }
    else if ( xfpm_strequal ("System failed to sleep", error ) )
    {
	return _("System failed to sleep");
    }
    return NULL;
}

XfpmShutdown *
xfpm_shutdown_new(void)
{
    if ( xfpm_shutdown_object != NULL )
    {
	g_object_ref (xfpm_shutdown_object);
    }
    else
    {
	xfpm_shutdown_object = g_object_new (XFPM_TYPE_SHUTDOWN, NULL);
	g_object_add_weak_pointer (xfpm_shutdown_object, &xfpm_shutdown_object);
    }
    return XFPM_SHUTDOWN (xfpm_shutdown_object);
}

gboolean                  xfpm_shutdown_add_callback    (XfpmShutdown *shutdown,
							 GSourceFunc func,
							 guint timeout,
							 gpointer data)
{
    g_return_val_if_fail (XFPM_IS_SHUTDOWN (shutdown), FALSE);
    
    if (shutdown->priv->block_shutdown)
	return FALSE;
	
    g_timeout_add_seconds (timeout, func, data);
    shutdown->priv->block_shutdown = TRUE;
    return TRUE;
}

void xfpm_shutdown	(XfpmShutdown *shutdown, GError **error)
{
    g_return_if_fail (XFPM_IS_SHUTDOWN(shutdown));
    
    if ( G_UNLIKELY (shutdown->priv->connected == FALSE) )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_HAL_DISCONNECTED, _("HAL daemon is currently not connected"));
	return;
    }
    
    xfpm_shutdown_internal (dbus_g_connection_get_connection(shutdown->priv->bus), "Shutdown", NULL);
    shutdown->priv->block_shutdown = FALSE;
}

void xfpm_hibernate (XfpmShutdown *shutdown, GError **error)
{
    GError *error_internal = NULL;
    const gchar *error_message;
    
    g_return_if_fail (XFPM_IS_SHUTDOWN(shutdown));
    
    if ( G_UNLIKELY (shutdown->priv->connected == FALSE) )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_HAL_DISCONNECTED, _("HAL daemon is currently not connected"));
	return;
    }

    xfpm_shutdown_internal (dbus_g_connection_get_connection(shutdown->priv->bus), "Hibernate", &error_internal);
    shutdown->priv->block_shutdown = FALSE;
    
    if ( error_internal )
    {
	g_warning ("%s", error_internal->message);
	error_message = _filter_error_message (error_internal->message);
	if ( error_message )
	{
	    g_set_error (error, XFPM_ERROR, XFPM_ERROR_SLEEP_FAILED, "%s", error_message);
	}
	g_error_free (error_internal);
    }
}

void xfpm_suspend (XfpmShutdown *shutdown, GError **error)
{
    GError *error_internal = NULL;
    const gchar *error_message;
    
    g_return_if_fail (XFPM_IS_SHUTDOWN(shutdown));
    
    if ( G_UNLIKELY (shutdown->priv->connected == FALSE) )
    {
	g_set_error (error, XFPM_ERROR, XFPM_ERROR_HAL_DISCONNECTED, _("HAL daemon is currently not connected"));
	return;
    }
    
    xfpm_shutdown_internal (dbus_g_connection_get_connection(shutdown->priv->bus), "Suspend", &error_internal);
    shutdown->priv->block_shutdown = FALSE;
     
    if ( error_internal )
    {
	g_warning ("%s", error_internal->message);
	error_message = _filter_error_message (error_internal->message);
	if ( error_message )
	{
	    g_set_error ( error, XFPM_ERROR, XFPM_ERROR_SLEEP_FAILED, "%s", error_message);
	}
	g_error_free (error_internal);
    }
}
