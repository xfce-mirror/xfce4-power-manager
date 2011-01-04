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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus-glib.h>

#include "xfpm-console-kit.h"
#include "xfpm-dbus-monitor.h"


static void xfpm_console_kit_finalize   (GObject *object);

static void xfpm_console_kit_get_property (GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec);
	    
#define XFPM_CONSOLE_KIT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_CONSOLE_KIT, XfpmConsoleKitPrivate))

struct XfpmConsoleKitPrivate
{
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    
    XfpmDBusMonitor *monitor;
    
    gboolean	     can_shutdown;
    gboolean	     can_restart;
};

enum
{
    PROP_0,
    PROP_CAN_RESTART,
    PROP_CAN_SHUTDOWN
};

G_DEFINE_TYPE (XfpmConsoleKit, xfpm_console_kit, G_TYPE_OBJECT)

static void
xfpm_console_kit_get_info (XfpmConsoleKit *console)
{
    GError *error = NULL;
    
    dbus_g_proxy_call (console->priv->proxy, "CanStop", &error,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &console->priv->can_shutdown,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_warning ("'CanStop' method failed : %s", error->message);
	g_error_free (error);
	error = NULL;
    }
    
    dbus_g_proxy_call (console->priv->proxy, "CanRestart", &error,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &console->priv->can_restart,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_warning ("'CanRestart' method failed : %s", error->message);
	g_error_free (error);
	error = NULL;
    }
    
}

static void
xfpm_console_kit_class_init (XfpmConsoleKitClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_console_kit_finalize;

    object_class->get_property = xfpm_console_kit_get_property;
    
    g_object_class_install_property (object_class,
                                     PROP_CAN_RESTART,
                                     g_param_spec_boolean ("can-restart",
                                                           NULL, NULL,
                                                           FALSE,
                                                           G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_CAN_SHUTDOWN,
                                     g_param_spec_boolean ("can-shutdown",
                                                           NULL, NULL,
                                                           FALSE,
                                                           G_PARAM_READABLE));

    g_type_class_add_private (klass, sizeof (XfpmConsoleKitPrivate));
}

static void
xfpm_console_kit_init (XfpmConsoleKit *console)
{
    GError *error = NULL;
    
    console->priv = XFPM_CONSOLE_KIT_GET_PRIVATE (console);
    console->priv->can_shutdown = FALSE;
    console->priv->can_restart  = FALSE;
    
    console->priv->bus   = NULL;
    console->priv->proxy = NULL;
    
    console->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Unable to get system bus connection : %s", error->message);
	g_error_free (error);
	goto out;
    }
    
    console->priv->proxy = dbus_g_proxy_new_for_name_owner (console->priv->bus,
							    "org.freedesktop.ConsoleKit",
							    "/org/freedesktop/ConsoleKit/Manager",
							    "org.freedesktop.ConsoleKit.Manager",
							    NULL);
						      
    if ( !console->priv->proxy )
    {
	g_warning ("Unable to create proxy for 'org.freedesktop.ConsoleKit'");
	goto out;
    }
    
    xfpm_console_kit_get_info (console);
    
out:
    ;
}

static void xfpm_console_kit_get_property (GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec)
{
    XfpmConsoleKit *console;
    console = XFPM_CONSOLE_KIT (object);

    switch (prop_id)
    {
	case PROP_CAN_SHUTDOWN:
	    g_value_set_boolean (value, console->priv->can_shutdown);
	    break;
	case PROP_CAN_RESTART:
	    g_value_set_boolean (value, console->priv->can_restart);
	    break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
xfpm_console_kit_finalize (GObject *object)
{
    XfpmConsoleKit *console;

    console = XFPM_CONSOLE_KIT (object);
    
    if ( console->priv->bus )
	dbus_g_connection_unref (console->priv->bus);
	
    if ( console->priv->proxy )
	g_object_unref (console->priv->proxy);

    G_OBJECT_CLASS (xfpm_console_kit_parent_class)->finalize (object);
}

XfpmConsoleKit *
xfpm_console_kit_new (void)
{
    static gpointer console_obj = NULL;
    
    if ( G_LIKELY (console_obj != NULL ) )
    {
	g_object_ref (console_obj);
    }
    else
    {
	console_obj = g_object_new (XFPM_TYPE_CONSOLE_KIT, NULL);
	g_object_add_weak_pointer (console_obj, &console_obj);
    }
    
    return XFPM_CONSOLE_KIT (console_obj);
}

void xfpm_console_kit_shutdown (XfpmConsoleKit *console, GError **error)
{
    g_return_if_fail (console->priv->proxy != NULL );
    
    dbus_g_proxy_call (console->priv->proxy, "Stop", error,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &console->priv->can_shutdown,
		       G_TYPE_INVALID);
}

void xfpm_console_kit_reboot (XfpmConsoleKit *console, GError **error)
{
    g_return_if_fail (console->priv->proxy != NULL );
    
    dbus_g_proxy_call (console->priv->proxy, "Restart", error,
		       G_TYPE_INVALID,
		       G_TYPE_BOOLEAN, &console->priv->can_shutdown,
		       G_TYPE_INVALID);
    
}
