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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

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
    GDBusConnection *bus;
    GDBusProxy      *proxy;
    
    XfpmDBusMonitor *monitor;
    
    gboolean	     can_shutdown;
    gboolean	     can_restart;
    gboolean         can_suspend;
    gboolean         can_hibernate;
};

enum
{
    PROP_0,
    PROP_CAN_RESTART,
    PROP_CAN_SHUTDOWN,
    PROP_CAN_SUSPEND,
    PROP_CAN_HIBERNATE
};

G_DEFINE_TYPE (XfpmConsoleKit, xfpm_console_kit, G_TYPE_OBJECT)

static void
xfpm_console_kit_get_info (XfpmConsoleKit *console)
{
    GError *error = NULL;
    gchar *tmp = NULL;
    GVariant *var;

    var = g_dbus_proxy_call_sync (console->priv->proxy, "CanStop",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  &error);

    if (var)
	g_variant_get (var,
		       "(b)",
		       &console->priv->can_shutdown);
    g_variant_unref (var);
    if ( error )
    {
	g_warning ("'CanStop' method failed : %s", error->message);
	g_clear_error (&error);
    }
    
    var = g_dbus_proxy_call_sync (console->priv->proxy, "CanRestart",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  &error);

    if (var)
	g_variant_get (var,
		       "(b)",
		       &console->priv->can_restart);
    g_variant_unref (var);
    if ( error )
    {
	g_warning ("'CanRestart' method failed : %s", error->message);
	g_clear_error (&error);
    }

    /* start with FALSE */
    console->priv->can_suspend = FALSE;

    var = g_dbus_proxy_call_sync (console->priv->proxy, "CanSuspend",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  &error);

    if ( error )
    {
	g_debug ("'CanSuspend' method failed : %s", error->message);
	g_clear_error (&error);
    }
    else
    {
	g_variant_get (var,
		       "(&s)",
		       &tmp);
	if (g_strcmp0 (tmp, "yes") == 0 || g_strcmp0 (tmp, "challenge") == 0)
	{
	    console->priv->can_suspend = TRUE;
	}
	g_variant_unref (var);
    }

    /* start with FALSE */
    console->priv->can_hibernate = FALSE;

    var = g_dbus_proxy_call_sync (console->priv->proxy, "CanHibernate",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  &error);

    if ( error )
    {
	g_debug ("'CanHibernate' method failed : %s", error->message);
	g_clear_error (&error);
    }
    else
    {
	g_variant_get (var,
		       "(&s)",
		       &tmp);
	if (g_strcmp0 (tmp, "yes") == 0 || g_strcmp0 (tmp, "challenge") == 0)
	{
	    console->priv->can_hibernate = TRUE;
	}
	g_variant_unref (var);
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

    g_object_class_install_property (object_class,
                                     PROP_CAN_SUSPEND,
                                     g_param_spec_boolean ("can-suspend",
                                                           NULL, NULL,
                                                           FALSE,
                                                           G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_CAN_HIBERNATE,
                                     g_param_spec_boolean ("can-hibernate",
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
    
    console->priv->bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    
    if ( error )
    {
	g_critical ("Unable to get system bus connection : %s", error->message);
	g_error_free (error);
	return;
    }
    
    console->priv->proxy = g_dbus_proxy_new_sync (console->priv->bus,
						  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
						  G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
						  NULL,
						  "org.freedesktop.ConsoleKit",
						  "/org/freedesktop/ConsoleKit/Manager",
						  "org.freedesktop.ConsoleKit.Manager",
						  NULL,
						  NULL);

    if ( !console->priv->proxy )
    {
	g_warning ("Unable to create proxy for 'org.freedesktop.ConsoleKit'");
	return;
    }
    
    xfpm_console_kit_get_info (console);
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
	case PROP_CAN_SUSPEND:
	    g_value_set_boolean (value, console->priv->can_suspend);
	    break;
	case PROP_CAN_HIBERNATE:
	    g_value_set_boolean (value, console->priv->can_hibernate);
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
	g_object_unref (console->priv->bus);
	
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
    GVariant *var;

    g_return_if_fail (console->priv->proxy != NULL );
    
    var = g_dbus_proxy_call_sync (console->priv->proxy, "Stop",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  error);

    g_variant_unref (var);
}

void xfpm_console_kit_reboot (XfpmConsoleKit *console, GError **error)
{
    GVariant *var;

    g_return_if_fail (console->priv->proxy != NULL );
    
    var = g_dbus_proxy_call_sync (console->priv->proxy, "Restart",
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  error);

    g_variant_unref (var);
}

void
xfpm_console_kit_suspend (XfpmConsoleKit *console,
                          GError        **error)
{
    GVariant *var;

    g_return_if_fail (console->priv->proxy != NULL );

    var = g_dbus_proxy_call_sync (console->priv->proxy, "Suspend",
                                  g_variant_new ("(b)", TRUE),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  error);

    g_variant_unref (var);
}

void
xfpm_console_kit_hibernate (XfpmConsoleKit *console,
                            GError        **error)
{
    GVariant *var;

    g_return_if_fail (console->priv->proxy != NULL );

    var = g_dbus_proxy_call_sync (console->priv->proxy, "Hibernate",
                                  g_variant_new ("(b)", TRUE),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  error);

    g_variant_unref (var);
}
