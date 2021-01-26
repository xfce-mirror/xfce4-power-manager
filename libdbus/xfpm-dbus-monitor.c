/* * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include <libxfce4util/libxfce4util.h>

#include "xfpm-dbus.h"

#include "xfpm-dbus-monitor.h"
#include "xfpm-dbus-marshal.h"

static void xfpm_dbus_monitor_finalize   (GObject *object);

#define XFPM_DBUS_MONITOR_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_DBUS_MONITOR, XfpmDBusMonitorPrivate))

struct XfpmDBusMonitorPrivate
{
  GDBusConnection *system_bus;
  GDBusConnection *session_bus;

  guint            system_signal;
  guint            session_signal;

  GPtrArray       *names_array;
  GPtrArray       *services_array;
};

typedef struct
{
  gchar   *name;
  GBusType     bus_type;
} XfpmWatchData;

enum
{
  UNIQUE_NAME_LOST,
  SERVICE_CONNECTION_CHANGED,
  SYSTEM_BUS_CONNECTION_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (XfpmDBusMonitor, xfpm_dbus_monitor, G_TYPE_OBJECT)

static void
xfpm_dbus_monitor_free_watch_data (gpointer data)
{
  XfpmWatchData *watch_data = data;

  g_free (watch_data->name);
  g_free (watch_data);
}

static XfpmWatchData *
xfpm_dbus_monitor_get_watch_data (GPtrArray *array, const gchar *name, GBusType bus_type)
{
  XfpmWatchData *data;
  guint i;

  for ( i = 0; i < array->len; i++)
  {
    data = g_ptr_array_index (array, i);
    if ( !g_strcmp0 (data->name, name) && data->bus_type == bus_type )
      return data;
  }
  return NULL;
}

static void
xfpm_dbus_monitor_unique_connection_name_lost (XfpmDBusMonitor *monitor, GBusType bus_type, const gchar *name)
{
  XfpmWatchData *watch;
  guint i = 0;

  for ( i = 0; i < monitor->priv->names_array->len; i++ )
  {
    watch = g_ptr_array_index (monitor->priv->names_array, i);

    if ( !g_strcmp0 (watch->name, name) && bus_type == watch->bus_type )
    {
      g_signal_emit (G_OBJECT(monitor), signals [UNIQUE_NAME_LOST], 0,
         watch->name, bus_type == G_BUS_TYPE_SESSION ? TRUE : FALSE);
      g_ptr_array_remove (monitor->priv->names_array, watch);
      xfpm_dbus_monitor_free_watch_data (watch);
    }
  }
}

static void
xfpm_dbus_monitor_service_connection_changed (XfpmDBusMonitor *monitor, GBusType bus_type,
                                              const gchar *name, gboolean connected)
{
  XfpmWatchData *watch;
  guint i;

  for ( i = 0; i < monitor->priv->services_array->len; i++)
  {
    watch = g_ptr_array_index (monitor->priv->services_array, i);

    if ( !g_strcmp0 (watch->name, name) && watch->bus_type == bus_type)
    {
      g_signal_emit (G_OBJECT (monitor), signals [SERVICE_CONNECTION_CHANGED], 0,
                     name, connected, bus_type == G_BUS_TYPE_SESSION ? TRUE : FALSE);
    }
  }
}

static void
xfpm_dbus_monitor_name_owner_changed (XfpmDBusMonitor *monitor, const gchar *name,
                                      const gchar *prev, const gchar *new, GBusType bus_type)
{
  if ( strlen (prev) != 0 )
  {
    xfpm_dbus_monitor_unique_connection_name_lost (monitor, bus_type, prev);

    /* Connection has name */
    if ( strlen (name) != 0 )
      xfpm_dbus_monitor_service_connection_changed (monitor, bus_type, name, FALSE);
  }
  else if ( strlen (name) != 0 && strlen (new) != 0)
  {
    xfpm_dbus_monitor_service_connection_changed (monitor, bus_type, name, TRUE);
  }
}

static void
xfpm_dbus_monitor_session_name_owner_changed_cb (GDBusConnection *connection, const gchar *sender,
                                                 const gchar *object_path, const gchar *interface_name,
                                                 const gchar *signal_name, GVariant *parameters,
                                                 gpointer monitor)
{
    const gchar *name, *prev, *new;

    g_variant_get (parameters,
                   "(&s&s&s)",
                   &name,
                   &prev,
                   &new);

    xfpm_dbus_monitor_name_owner_changed (monitor, name, prev, new, G_BUS_TYPE_SESSION);
}

static void
xfpm_dbus_monitor_system_name_owner_changed_cb  (GDBusConnection *connection, const gchar *sender,
             const gchar *object_path, const gchar *interface_name,
             const gchar *signal_name, GVariant *parameters,
             gpointer monitor)
{
    const gchar *name, *prev, *new;

    g_variant_get (parameters,
                   "(&s&s&s)",
                   &name,
                   &prev,
                   &new);

    xfpm_dbus_monitor_name_owner_changed (monitor, name, prev, new, G_BUS_TYPE_SYSTEM);
}

static void
xfpm_dbus_monitor_session (XfpmDBusMonitor *monitor)
{
    monitor->priv->session_signal =
        g_dbus_connection_signal_subscribe (monitor->priv->session_bus,
              "org.freedesktop.DBus",
              "org.freedesktop.DBus",
                                            "NameOwnerChanged",
              "/org/freedesktop/DBus",
              NULL,
              G_DBUS_SIGNAL_FLAGS_NONE,
              xfpm_dbus_monitor_session_name_owner_changed_cb,
              monitor, NULL);
}

static void
xfpm_dbus_monitor_system (XfpmDBusMonitor *monitor)
{
    monitor->priv->system_signal =
        g_dbus_connection_signal_subscribe (monitor->priv->system_bus,
              "org.freedesktop.DBus",
              "org.freedesktop.DBus",
                                            "NameOwnerChanged",
              "/org/freedesktop/DBus",
              NULL,
              G_DBUS_SIGNAL_FLAGS_NONE,
              xfpm_dbus_monitor_system_name_owner_changed_cb,
              monitor, NULL);
}

static void
xfpm_dbus_monitor_class_init (XfpmDBusMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  signals [UNIQUE_NAME_LOST] =
    g_signal_new ("unique-name-lost",
                  XFPM_TYPE_DBUS_MONITOR,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfpmDBusMonitorClass, unique_name_lost),
                  NULL, NULL,
                  _xfpm_dbus_marshal_VOID__STRING_BOOLEAN,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING, G_TYPE_BOOLEAN);

  signals [SERVICE_CONNECTION_CHANGED] =
    g_signal_new ("service-connection-changed",
                  XFPM_TYPE_DBUS_MONITOR,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfpmDBusMonitorClass, service_connection_changed),
                  NULL, NULL,
                  _xfpm_dbus_marshal_VOID__STRING_BOOLEAN_BOOLEAN,
                  G_TYPE_NONE, 3, G_TYPE_STRING,
                  G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

  signals [SYSTEM_BUS_CONNECTION_CHANGED] =
    g_signal_new ("system-bus-connection-changed",
                  XFPM_TYPE_DBUS_MONITOR,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfpmDBusMonitorClass, system_bus_connection_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  object_class->finalize = xfpm_dbus_monitor_finalize;
}

static void
xfpm_dbus_monitor_init (XfpmDBusMonitor *monitor)
{
  monitor->priv = xfpm_dbus_monitor_get_instance_private (monitor);

  monitor->priv->names_array = g_ptr_array_new_with_free_func (xfpm_dbus_monitor_free_watch_data);
  monitor->priv->services_array = g_ptr_array_new_with_free_func (xfpm_dbus_monitor_free_watch_data);

  monitor->priv->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  monitor->priv->system_bus  = g_bus_get_sync (G_BUS_TYPE_SYSTEM,  NULL, NULL);

  xfpm_dbus_monitor_session (monitor);
  xfpm_dbus_monitor_system  (monitor);
}

static void
xfpm_dbus_monitor_finalize (GObject *object)
{
  XfpmDBusMonitor *monitor;

  monitor = XFPM_DBUS_MONITOR (object);

  g_dbus_connection_signal_unsubscribe (monitor->priv->session_bus, monitor->priv->session_signal);

  g_dbus_connection_signal_unsubscribe (monitor->priv->system_bus, monitor->priv->system_signal);

  g_object_unref (monitor->priv->system_bus);
  g_object_unref (monitor->priv->session_bus);

  g_ptr_array_free (monitor->priv->names_array, TRUE);
  g_ptr_array_free (monitor->priv->services_array, TRUE);

  G_OBJECT_CLASS (xfpm_dbus_monitor_parent_class)->finalize (object);
}

XfpmDBusMonitor *
xfpm_dbus_monitor_new (void)
{
  static gpointer xfpm_dbus_monitor_object = NULL;

  if ( G_LIKELY (xfpm_dbus_monitor_object != NULL) )
  {
    g_object_ref (xfpm_dbus_monitor_object);
  }
  else
  {
    xfpm_dbus_monitor_object = g_object_new (XFPM_TYPE_DBUS_MONITOR, NULL);
    g_object_add_weak_pointer (xfpm_dbus_monitor_object, &xfpm_dbus_monitor_object);
  }

  return XFPM_DBUS_MONITOR (xfpm_dbus_monitor_object);
}

gboolean xfpm_dbus_monitor_add_unique_name (XfpmDBusMonitor *monitor, GBusType bus_type, const gchar *unique_name)
{
  XfpmWatchData *watch;

  g_return_val_if_fail (XFPM_IS_DBUS_MONITOR (monitor), FALSE);
  g_return_val_if_fail (unique_name != NULL, FALSE);

  /* We have it already */
  if ( xfpm_dbus_monitor_get_watch_data (monitor->priv->names_array, unique_name, bus_type) )
    return FALSE;

  watch = g_new0 (XfpmWatchData , 1);
  watch->name = g_strdup (unique_name);
  watch->bus_type = bus_type;

  g_ptr_array_add (monitor->priv->names_array, watch);
  return TRUE;
}

void xfpm_dbus_monitor_remove_unique_name (XfpmDBusMonitor *monitor, GBusType bus_type, const gchar *unique_name)
{
  XfpmWatchData *watch;

  g_return_if_fail (XFPM_IS_DBUS_MONITOR (monitor));

  watch = xfpm_dbus_monitor_get_watch_data (monitor->priv->names_array, unique_name, bus_type);

  if ( watch )
  {
    g_ptr_array_remove (monitor->priv->names_array, watch);
    xfpm_dbus_monitor_free_watch_data (watch);
  }
}

gboolean xfpm_dbus_monitor_add_service (XfpmDBusMonitor *monitor, GBusType bus_type, const gchar *service_name)
{
  XfpmWatchData *watch;

  g_return_val_if_fail (XFPM_IS_DBUS_MONITOR (monitor), FALSE);

  if ( xfpm_dbus_monitor_get_watch_data (monitor->priv->services_array, service_name, bus_type ) )
    return FALSE;

  watch = g_new0 (XfpmWatchData , 1);
  watch->name = g_strdup (service_name);
  watch->bus_type = bus_type;

  g_ptr_array_add (monitor->priv->services_array, watch);

  return TRUE;
}

void xfpm_dbus_monitor_remove_service (XfpmDBusMonitor *monitor, GBusType bus_type, const gchar *service_name)
{
  XfpmWatchData *watch;

  g_return_if_fail (XFPM_IS_DBUS_MONITOR (monitor));

  watch = xfpm_dbus_monitor_get_watch_data (monitor->priv->services_array, service_name, bus_type);

  if ( watch )
  {
    g_ptr_array_remove (monitor->priv->services_array, watch);
    xfpm_dbus_monitor_free_watch_data (watch);
  }
}
