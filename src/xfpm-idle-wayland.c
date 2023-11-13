/*
 * Copyright (C) 2023 Gaël Bonithon <gael@xfce.org>
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
#include <config.h>
#endif

#include <gdk/gdkwayland.h>

#include "xfpm-idle-wayland.h"
#include "protocols/ext-idle-notify-v1-client.h"

static void     xfpm_idle_wayland_finalize           (GObject        *object);
static void     xfpm_idle_wayland_alarm_reset_all    (XfpmIdle       *idle);
static void     xfpm_idle_wayland_alarm_add          (XfpmIdle       *idle,
                                                      XfpmAlarmId     id,
                                                      guint           timeout);
static void     xfpm_idle_wayland_alarm_remove       (XfpmIdle       *idle,
                                                      XfpmAlarmId     id);

static void registry_global (void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
static void registry_global_remove (void *data, struct wl_registry *registry, uint32_t id);
static void notification_idled (void *data, struct ext_idle_notification_v1 *notification);
static void notification_resumed (void *data, struct ext_idle_notification_v1 *notification);

struct _XfpmIdleWayland
{
  XfpmIdle __parent__;

  struct wl_registry *wl_registry;
  struct ext_idle_notifier_v1 *wl_notifier;
  GHashTable *alarms;
};

typedef struct _Alarm
{
  XfpmIdleWayland *idle;
  struct ext_idle_notification_v1 *wl_notification;
  XfpmAlarmId id;
  guint timeout;
} Alarm;

static const struct wl_registry_listener registry_listener =
{
  .global = registry_global,
  .global_remove = registry_global_remove,
};

static const struct ext_idle_notification_v1_listener notification_listener =
{
  .idled = notification_idled,
  .resumed = notification_resumed,
};



G_DEFINE_FINAL_TYPE (XfpmIdleWayland, xfpm_idle_wayland, XFPM_TYPE_IDLE)



static void
xfpm_idle_wayland_class_init (XfpmIdleWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  XfpmIdleClass *idle_class = XFPM_IDLE_CLASS (klass);

  object_class->finalize = xfpm_idle_wayland_finalize;

  idle_class->alarm_reset_all = xfpm_idle_wayland_alarm_reset_all;
  idle_class->alarm_add = xfpm_idle_wayland_alarm_add;
  idle_class->alarm_remove = xfpm_idle_wayland_alarm_remove;
}

static void
alarm_free (gpointer data)
{
  Alarm *alarm = data;
  ext_idle_notification_v1_destroy (alarm->wl_notification);
  g_free (alarm);
}

static void
xfpm_idle_wayland_init (XfpmIdleWayland *idle)
{
  struct wl_display *wl_display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());
  idle->wl_registry = wl_display_get_registry (wl_display);
  wl_registry_add_listener (idle->wl_registry, &registry_listener, idle);
  wl_display_roundtrip (wl_display);
  if (idle->wl_notifier == NULL)
  {
    g_warning ("ext-idle-notify-v1 protocol unsupported: most power manager features won't work");
    return;
  }

  idle->alarms = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, alarm_free);
}

static void
xfpm_idle_wayland_finalize (GObject *object)
{
  XfpmIdleWayland *idle = XFPM_IDLE_WAYLAND (object);

  if (idle->wl_notifier != NULL)
  {
    g_hash_table_destroy (idle->alarms);
    ext_idle_notifier_v1_destroy (idle->wl_notifier);
  }
  wl_registry_destroy (idle->wl_registry);

  G_OBJECT_CLASS (xfpm_idle_wayland_parent_class)->finalize (object);
}

static Alarm *
alarm_new (XfpmIdleWayland *idle,
           XfpmAlarmId id,
           guint timeout)
{
  struct wl_seat *wl_seat = gdk_wayland_seat_get_wl_seat (gdk_display_get_default_seat (gdk_display_get_default ()));
  Alarm *alarm = g_new0 (Alarm, 1);
  alarm->idle = idle;
  alarm->id = id;
  alarm->timeout = timeout;
  alarm->wl_notification = ext_idle_notifier_v1_get_idle_notification (idle->wl_notifier, timeout, wl_seat);
  ext_idle_notification_v1_add_listener (alarm->wl_notification, &notification_listener, alarm);
  return alarm;
}

static void
xfpm_idle_wayland_alarm_reset_all (XfpmIdle *_idle)
{
  XfpmIdleWayland *idle = XFPM_IDLE_WAYLAND (_idle);
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, idle->alarms);
  while (g_hash_table_iter_next (&iter, NULL, &value))
  {
    Alarm *alarm = value;
    g_hash_table_iter_replace (&iter, alarm_new (idle, alarm->id, alarm->timeout));
  }
}

static void
xfpm_idle_wayland_alarm_add (XfpmIdle *_idle,
                             XfpmAlarmId id,
                             guint timeout)
{
  XfpmIdleWayland *idle = XFPM_IDLE_WAYLAND (_idle);
  g_hash_table_insert (idle->alarms, GINT_TO_POINTER (id), alarm_new (idle, id, timeout));
}

static void
xfpm_idle_wayland_alarm_remove (XfpmIdle *_idle,
                                XfpmAlarmId id)
{
  XfpmIdleWayland *idle = XFPM_IDLE_WAYLAND (_idle);
  g_hash_table_remove (idle->alarms, GINT_TO_POINTER (id));
}

static void
registry_global (void *data,
                 struct wl_registry *registry,
                 uint32_t id,
                 const char *interface,
                 uint32_t version)
{
  XfpmIdleWayland *idle = data;
  if (g_strcmp0 (ext_idle_notifier_v1_interface.name, interface) == 0)
    idle->wl_notifier = wl_registry_bind (idle->wl_registry, id, &ext_idle_notifier_v1_interface,
                                          MIN ((uint32_t) ext_idle_notifier_v1_interface.version, version));
}

static void
registry_global_remove (void *data,
                        struct wl_registry *registry,
                        uint32_t id)
{
}

static void
notification_idled (void *data,
                    struct ext_idle_notification_v1 *notification)
{
  Alarm *alarm = data;
  g_signal_emit_by_name (alarm->idle, "alarm-expired", alarm->id);
}

static void
notification_resumed (void *data,
                      struct ext_idle_notification_v1 *notification)
{
  Alarm *alarm = data;
  g_signal_emit_by_name (alarm->idle, "reset");
}




XfpmIdle *
xfpm_idle_wayland_new (void)
{
  XfpmIdle *idle = g_object_new (XFPM_TYPE_IDLE_WAYLAND, NULL);
  if (XFPM_IDLE_WAYLAND (idle)->alarms == NULL)
  {
    g_object_unref (idle);
    return NULL;
  }

  return idle;
}
