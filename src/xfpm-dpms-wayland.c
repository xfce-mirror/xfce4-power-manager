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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkwayland.h>

#include "xfpm-dpms-wayland.h"
#include "xfpm-idle.h"
#include "common/xfpm-debug.h"
#include "protocols/wlr-output-power-management-unstable-v1-client.h"

static void       xfpm_dpms_wayland_finalize            (GObject          *object);
static void       xfpm_dpms_wayland_set_mode            (XfpmDpms         *dpms,
                                                         XfpmDpmsMode      mode);
static void       xfpm_dpms_wayland_set_enabled         (XfpmDpms         *dpms,
                                                         gboolean          enabled);
static void       xfpm_dpms_wayland_set_timeouts        (XfpmDpms         *dpms,
                                                         gboolean          standby,
                                                         guint             sleep_timemout,
                                                         guint             off_timemout);

static void registry_global (void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
static void registry_global_remove (void *data, struct wl_registry *registry, uint32_t id);
static void power_mode (void *data, struct zwlr_output_power_v1 *wl_power, uint32_t wl_mode);
static void power_failed (void *data, struct zwlr_output_power_v1 *wl_power);

struct _XfpmDpmsWayland
{
  XfpmDpms __parent__;

  struct wl_registry *wl_registry;
  struct zwlr_output_power_manager_v1 *wl_manager;
  GList *powers;
  XfpmIdle *idle;
};

typedef struct _Power
{
  XfpmDpmsWayland *dpms;
  struct zwlr_output_power_v1 *wl_power;
  XfpmDpmsMode mode;
  gchar *model;
} Power;

static const struct wl_registry_listener registry_listener =
{
  .global = registry_global,
  .global_remove = registry_global_remove,
};

static const struct zwlr_output_power_v1_listener power_listener =
{
  .mode = power_mode,
  .failed = power_failed,
};



G_DEFINE_FINAL_TYPE (XfpmDpmsWayland, xfpm_dpms_wayland, XFPM_TYPE_DPMS)



static void
xfpm_dpms_wayland_class_init (XfpmDpmsWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  XfpmDpmsClass *dpms_class = XFPM_DPMS_CLASS (klass);

  object_class->finalize = xfpm_dpms_wayland_finalize;

  dpms_class->set_mode = xfpm_dpms_wayland_set_mode;
  dpms_class->set_enabled = xfpm_dpms_wayland_set_enabled;
  dpms_class->set_timeouts = xfpm_dpms_wayland_set_timeouts;
}

static void
monitor_added (GdkDisplay *display,
               GdkMonitor *monitor,
               XfpmDpmsWayland *dpms)
{
  Power *power = g_new0 (Power, 1);
  struct wl_output *wl_output = gdk_wayland_monitor_get_wl_output (monitor);
  struct zwlr_output_power_v1 *wl_power = zwlr_output_power_manager_v1_get_output_power (dpms->wl_manager, wl_output);
  zwlr_output_power_v1_add_listener (wl_power, &power_listener, power);
  power->dpms = dpms;
  power->wl_power = wl_power;
  power->model = g_strdup (gdk_monitor_get_model (monitor));
  dpms->powers = g_list_prepend (dpms->powers, power);
  wl_display_roundtrip (gdk_wayland_display_get_wl_display (display));
}

static void
idle_alarm_expired (XfpmIdle *idle,
                    XfpmAlarmId id,
                    XfpmDpmsWayland *dpms)
{
  xfpm_dpms_wayland_set_mode (XFPM_DPMS (dpms), XFPM_DPMS_MODE_OFF);
}

static void
idle_reset (XfpmIdle *idle,
            XfpmDpmsWayland *dpms)
{
  xfpm_dpms_wayland_set_mode (XFPM_DPMS (dpms), XFPM_DPMS_MODE_ON);
}

static void
xfpm_dpms_wayland_init (XfpmDpmsWayland *dpms)
{
  GdkDisplay *display = gdk_display_get_default ();
  struct wl_display *wl_display = gdk_wayland_display_get_wl_display (display);
  gint n_monitors;

  dpms->wl_registry = wl_display_get_registry (wl_display);
  wl_registry_add_listener (dpms->wl_registry, &registry_listener, dpms);
  wl_display_roundtrip (wl_display);
  if (dpms->wl_manager == NULL)
  {
    g_warning ("wlr-output-power-management protocol unsupported: DPMS features won't work");
    return;
  }

  g_signal_connect_object (display, "monitor-added", G_CALLBACK (monitor_added), dpms, 0);
  n_monitors = gdk_display_get_n_monitors (display);
  for (gint n = 0; n < n_monitors; n++)
    monitor_added (display, gdk_display_get_monitor (display, n), dpms);

  dpms->idle = xfpm_idle_new ();
  if (dpms->idle != NULL)
  {
    g_signal_connect_object (dpms->idle, "alarm-expired", G_CALLBACK (idle_alarm_expired), dpms, 0);
    g_signal_connect_object (dpms->idle, "reset", G_CALLBACK (idle_reset), dpms, 0);
  }
}

static void
power_free (gpointer data)
{
  Power *power = data;
  zwlr_output_power_v1_destroy (power->wl_power);
  g_free (power->model);
  g_free (power);
}

static void
xfpm_dpms_wayland_finalize (GObject *object)
{
  XfpmDpmsWayland *dpms = XFPM_DPMS_WAYLAND (object);

  if (dpms->wl_manager != NULL)
  {
    g_list_free_full (dpms->powers, power_free);
    zwlr_output_power_manager_v1_destroy (dpms->wl_manager);
    if (dpms->idle != NULL)
      g_object_unref (dpms->idle);
  }
  wl_registry_destroy (dpms->wl_registry);

  G_OBJECT_CLASS (xfpm_dpms_wayland_parent_class)->finalize (object);
}

static uint32_t
mode_to_wl_mode (XfpmDpmsMode mode)
{
  switch (mode)
  {
    case XFPM_DPMS_MODE_OFF:
    case XFPM_DPMS_MODE_SUSPEND:
    case XFPM_DPMS_MODE_STANDBY:
      return 0;
    case XFPM_DPMS_MODE_ON:
      return 1;
    default:
      g_warn_if_reached ();
      return 0;
  }
}

static XfpmDpmsMode
wl_mode_to_mode (uint32_t wl_mode)
{
  switch (wl_mode)
  {
    case 0:
      return XFPM_DPMS_MODE_OFF;
    case 1:
      return XFPM_DPMS_MODE_ON;
    default:
      g_warn_if_reached ();
      return XFPM_DPMS_MODE_OFF;
  }
}

static void
xfpm_dpms_wayland_set_mode (XfpmDpms *_dpms,
                            XfpmDpmsMode mode)
{
  XfpmDpmsWayland *dpms = XFPM_DPMS_WAYLAND (_dpms);
  for (GList *lp = dpms->powers; lp != NULL; lp = lp->next)
  {
    Power *power = lp->data;
    if (mode != power->mode)
    {
      XFPM_DEBUG ("Setting DPMS mode %d for output %s", mode, power->model);
      zwlr_output_power_v1_set_mode (power->wl_power, mode_to_wl_mode (mode));
    }
    else
    {
      XFPM_DEBUG ("DPMS mode %d already set for output %s", mode, power->model);
    }
  }
}

static void
xfpm_dpms_wayland_set_enabled (XfpmDpms *_dpms,
                               gboolean enabled)
{
  XfpmDpmsWayland *dpms = XFPM_DPMS_WAYLAND (_dpms);
  if (!enabled)
    xfpm_idle_alarm_remove (dpms->idle, XFPM_ALARM_ID_DPMS);
}

static void
xfpm_dpms_wayland_set_timeouts (XfpmDpms *_dpms,
                                gboolean standby,
                                guint sleep_timeout,
                                guint off_timeout)
{
  XfpmDpmsWayland *dpms = XFPM_DPMS_WAYLAND (_dpms);
  if (off_timeout == 0)
    xfpm_idle_alarm_remove (dpms->idle, XFPM_ALARM_ID_DPMS);
  else
    xfpm_idle_alarm_add (dpms->idle, XFPM_ALARM_ID_DPMS, off_timeout * 1000);
}



static void
registry_global (void *data,
                 struct wl_registry *registry,
                 uint32_t id,
                 const char *interface,
                 uint32_t version)
{
  XfpmDpmsWayland *dpms = data;
  if (g_strcmp0 (zwlr_output_power_manager_v1_interface.name, interface) == 0)
    dpms->wl_manager = wl_registry_bind (dpms->wl_registry, id, &zwlr_output_power_manager_v1_interface,
                                         MIN ((uint32_t) zwlr_output_power_manager_v1_interface.version, version));
}

static void
registry_global_remove (void *data,
                        struct wl_registry *registry,
                        uint32_t id)
{
}

static void
power_mode (void *data,
            struct zwlr_output_power_v1 *wl_power,
            uint32_t wl_mode)
{
  Power *power = data;
  power->mode = wl_mode_to_mode (wl_mode);
  XFPM_DEBUG ("DPMS mode %d advertised", power->mode);
}

static void
power_failed (void *data,
              struct zwlr_output_power_v1 *wl_power)
{
  Power *power = data;
  XFPM_DEBUG ("DPMS controller of output %s died for some reason", power->model);
  power->dpms->powers = g_list_remove (power->dpms->powers, power);
  power_free (power);
}



XfpmDpms *
xfpm_dpms_wayland_new (void)
{
  XfpmDpms *dpms = g_object_new (XFPM_TYPE_DPMS_WAYLAND, NULL);
  if (XFPM_DPMS_WAYLAND (dpms)->wl_manager == NULL)
  {
    g_object_unref (dpms);
    return NULL;
  }

  return dpms;
}
