/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
 * * Copyright (C) 2019 Kacper Piwiński
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

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#define WINDOWING_IS_X11() GDK_IS_X11_DISPLAY (gdk_display_get_default ())
#else
#define WINDOWING_IS_X11() FALSE
#endif
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include <gio/gunixfdlist.h>

#include <libnotify/notify.h>

#include "xfpm-power.h"
#include "xfpm-dbus.h"
#include "xfpm-dpms.h"
#include "xfpm-manager.h"
#include "xfpm-button.h"
#include "xfpm-backlight.h"
#include "xfpm-kbd-backlight.h"
#include "xfpm-inhibit.h"
#include "xfpm-idle.h"
#include "xfpm-config.h"
#include "xfpm-debug.h"
#include "xfpm-xfconf.h"
#include "xfpm-errors.h"
#include "xfpm-common.h"
#include "xfpm-enum.h"
#include "xfpm-enum-glib.h"
#include "xfpm-enum-types.h"
#include "xfpm-dbus-monitor.h"
#include "xfpm-ppd.h"
#include "../panel-plugins/power-manager-plugin/power-manager-button.h"

static void xfpm_manager_finalize   (GObject *object);
static void xfpm_manager_set_property(GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec);
static void xfpm_manager_get_property(GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec);

static void xfpm_manager_dbus_class_init (XfpmManagerClass *klass);
static void xfpm_manager_dbus_init   (XfpmManager *manager);

static gboolean xfpm_manager_quit (XfpmManager *manager);

static void xfpm_manager_show_tray_icon (XfpmManager *manager);
static void xfpm_manager_hide_tray_icon (XfpmManager *manager);

#define SLEEP_KEY_TIMEOUT 6.0f

struct XfpmManagerPrivate
{
  GDBusConnection    *session_bus;
  GDBusConnection    *system_bus;

  XfceSMClient       *client;

  XfpmPower          *power;
  XfpmButton         *button;
  XfpmXfconf         *conf;
  XfpmBacklight      *backlight;
  XfpmKbdBacklight   *kbd_backlight;
  XfceConsolekit     *console;
  XfceSystemd        *systemd;
  XfpmDBusMonitor    *monitor;
  XfpmInhibit        *inhibit;
  XfpmPPD            *ppd;
  XfceScreensaver    *screensaver;
  XfpmIdle           *idle;
  GtkStatusIcon      *adapter_icon;
  GtkWidget          *power_button;
  gint                show_tray_icon;

  XfpmDpms           *dpms;

  GTimer         *timer;

  gboolean          inhibited;
  gboolean          session_managed;

  gint                inhibit_fd;
};

enum
{
  PROP_0 = 0,
  PROP_SHOW_TRAY_ICON
};

G_DEFINE_TYPE_WITH_PRIVATE (XfpmManager, xfpm_manager, G_TYPE_OBJECT)

static void
xfpm_manager_class_init (XfpmManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = xfpm_manager_finalize;
  object_class->set_property = xfpm_manager_set_property;
  object_class->get_property = xfpm_manager_get_property;

#define XFPM_PARAM_FLAGS  (G_PARAM_READWRITE \
                     | G_PARAM_CONSTRUCT \
                     | G_PARAM_STATIC_NAME \
                     | G_PARAM_STATIC_NICK \
                     | G_PARAM_STATIC_BLURB)

  g_object_class_install_property (object_class, PROP_SHOW_TRAY_ICON,
                             g_param_spec_int (SHOW_TRAY_ICON_CFG,
                                               SHOW_TRAY_ICON_CFG,
                                               SHOW_TRAY_ICON_CFG,
                                               0, 5, 0,
                                               XFPM_PARAM_FLAGS));
#undef XFPM_PARAM_FLAGS
}

static void
xfpm_manager_init (XfpmManager *manager)
{
  manager->priv = xfpm_manager_get_instance_private (manager);

  manager->priv->timer = g_timer_new ();

  notify_init ("xfce4-power-manager");
}

static void
xfpm_manager_finalize (GObject *object)
{
  XfpmManager *manager;

  manager = XFPM_MANAGER(object);

  if ( manager->priv->session_bus )
    g_object_unref (manager->priv->session_bus);

  if ( manager->priv->system_bus )
    g_object_unref (manager->priv->system_bus);

  g_object_unref (manager->priv->power);
  g_object_unref (manager->priv->button);
  g_object_unref (manager->priv->conf);
  g_object_unref (manager->priv->client);
  if ( manager->priv->systemd != NULL )
    g_object_unref (manager->priv->systemd);
  if ( manager->priv->console != NULL )
    g_object_unref (manager->priv->console);
  g_object_unref (manager->priv->monitor);
  g_object_unref (manager->priv->inhibit);
  g_object_unref (manager->priv->ppd);
  if (manager->priv->idle != NULL)
    g_object_unref (manager->priv->idle);

  g_timer_destroy (manager->priv->timer);

  if (manager->priv->dpms != NULL)
    g_object_unref (manager->priv->dpms);

  if (manager->priv->backlight != NULL)
    g_object_unref (manager->priv->backlight);

  g_object_unref (manager->priv->kbd_backlight);

  G_OBJECT_CLASS (xfpm_manager_parent_class)->finalize (object);
}

static void
xfpm_manager_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  XfpmManager *manager = XFPM_MANAGER(object);
  gint new_value;

  switch(property_id) {
    case PROP_SHOW_TRAY_ICON:
      new_value = g_value_get_int (value);
      if (new_value != manager->priv->show_tray_icon)
      {
        manager->priv->show_tray_icon = new_value;
        if (new_value > 0)
        {
          if (WINDOWING_IS_X11 ())
            xfpm_manager_show_tray_icon (manager);
        }
        else
        {
          if (WINDOWING_IS_X11 ())
            xfpm_manager_hide_tray_icon (manager);
        }
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
}

static void
xfpm_manager_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
  XfpmManager *manager = XFPM_MANAGER(object);

  switch(property_id) {
    case PROP_SHOW_TRAY_ICON:
      g_value_set_int (value, manager->priv->show_tray_icon);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
}

static void
xfpm_manager_release_names (XfpmManager *manager)
{
  xfpm_dbus_release_name (manager->priv->session_bus,
                          "org.xfce.PowerManager");

  xfpm_dbus_release_name (manager->priv->session_bus,
                          "org.freedesktop.PowerManagement");
}

static gboolean
xfpm_manager_quit (XfpmManager *manager)
{
  XFPM_DEBUG ("Exiting");

  xfpm_manager_release_names (manager);

  if (manager->priv->inhibit_fd >= 0)
    close (manager->priv->inhibit_fd);

  gtk_main_quit ();
  return TRUE;
}

static void
xfpm_manager_system_bus_connection_changed_cb (XfpmDBusMonitor *monitor, gboolean connected, XfpmManager *manager)
{
  if ( connected == TRUE )
  {
    XFPM_DEBUG ("System bus connection changed to TRUE, restarting the power manager");
    xfpm_manager_quit (manager);
    g_spawn_command_line_async ("xfce4-power-manager", NULL);
  }
}

static gboolean
xfpm_manager_reserve_names (XfpmManager *manager)
{
  if ( !xfpm_dbus_register_name (manager->priv->session_bus,
                                 "org.xfce.PowerManager") ||
       !xfpm_dbus_register_name (manager->priv->session_bus,
                                 "org.freedesktop.PowerManagement") )
  {
    g_warning ("Unable to reserve bus name: Maybe any already running instance?\n");

    g_object_unref (G_OBJECT (manager));
    gtk_main_quit ();

    return FALSE;
  }
  return TRUE;
}

static void
xfpm_manager_shutdown (XfpmManager *manager)
{
  GError *error = NULL;

  if (manager->priv->systemd != NULL)
    xfce_systemd_power_off (manager->priv->systemd, TRUE, &error);
  else
    xfce_consolekit_power_off (manager->priv->console, TRUE, &error);

  if ( error )
  {
    g_warning ("Failed to shutdown the system : %s", error->message);
    g_error_free (error);
    /* Try with the session then */
    if ( manager->priv->session_managed )
      xfce_sm_client_request_shutdown (manager->priv->client, XFCE_SM_CLIENT_SHUTDOWN_HINT_HALT);
  }
}

static void
xfpm_manager_ask_shutdown (XfpmManager *manager)
{
  if ( manager->priv->session_managed )
  xfce_sm_client_request_shutdown (manager->priv->client, XFCE_SM_CLIENT_SHUTDOWN_HINT_ASK);
}

static void
xfpm_manager_sleep_request (XfpmManager *manager, XfpmShutdownRequest req, gboolean force)
{
  switch (req)
  {
    case XFPM_DO_NOTHING:
      break;
    case XFPM_DO_SUSPEND:
      xfpm_power_suspend (manager->priv->power, force);
      break;
    case XFPM_DO_HIBERNATE:
      xfpm_power_hibernate (manager->priv->power, force);
      break;
    case XFPM_DO_SHUTDOWN:
      xfpm_manager_shutdown (manager);
      break;
    case XFPM_ASK:
      xfpm_manager_ask_shutdown (manager);
      break;
    default:
      g_warn_if_reached ();
      break;
  }
}

static void
xfpm_manager_reset_sleep_timer (XfpmManager *manager)
{
  g_timer_reset (manager->priv->timer);
}

static void
xfpm_manager_button_pressed_cb (XfpmButton *bt, XfpmButtonKey type, XfpmManager *manager)
{
  XfpmShutdownRequest req = XFPM_DO_NOTHING;

  XFPM_DEBUG_ENUM (type, XFPM_TYPE_BUTTON_KEY, "Received button press event");

  if ( type == BUTTON_MON_BRIGHTNESS_DOWN || type == BUTTON_MON_BRIGHTNESS_UP )
    return;

  if ( type == BUTTON_KBD_BRIGHTNESS_DOWN || type == BUTTON_KBD_BRIGHTNESS_UP )
    return;

  if ( type == BUTTON_POWER_OFF )
  {
    g_object_get (G_OBJECT (manager->priv->conf),
                  POWER_SWITCH_CFG, &req,
                  NULL);
  }
  else if ( type == BUTTON_SLEEP )
  {
    g_object_get (G_OBJECT (manager->priv->conf),
                  SLEEP_SWITCH_CFG, &req,
                  NULL);
  }
  else if ( type == BUTTON_HIBERNATE )
    {
    g_object_get (G_OBJECT (manager->priv->conf),
                  HIBERNATE_SWITCH_CFG, &req,
                  NULL);
  }
  else if ( type == BUTTON_BATTERY )
  {
    g_object_get (G_OBJECT (manager->priv->conf),
                  BATTERY_SWITCH_CFG, &req,
                  NULL);
  }
  else
  {
    g_return_if_reached ();
  }

  XFPM_DEBUG_ENUM (req, XFPM_TYPE_SHUTDOWN_REQUEST, "Shutdown request : ");

  if ( req == XFPM_ASK )
    xfpm_manager_ask_shutdown (manager);
  else
  {
    if ( g_timer_elapsed (manager->priv->timer, NULL) > SLEEP_KEY_TIMEOUT )
    {
      g_timer_reset (manager->priv->timer);
      xfpm_manager_sleep_request (manager, req, FALSE);
    }
  }
}

static void
xfpm_manager_lid_changed_cb (XfpmPower *power, gboolean lid_is_closed, XfpmManager *manager)
{
  XfpmLidTriggerAction action;
  gboolean on_battery, logind_handle_lid_switch;

  if (manager->priv->systemd != NULL)
  {
    g_object_get (G_OBJECT (manager->priv->conf),
                  LOGIND_HANDLE_LID_SWITCH, &logind_handle_lid_switch,
                  NULL);

    if (logind_handle_lid_switch)
      return;
  }

  g_object_get (G_OBJECT (power),
                "on-battery", &on_battery,
                NULL);

  g_object_get (G_OBJECT (manager->priv->conf),
                on_battery ? LID_SWITCH_ON_BATTERY_CFG : LID_SWITCH_ON_AC_CFG, &action,
                NULL);

  if ( lid_is_closed )
  {
    XFPM_DEBUG_ENUM (action, XFPM_TYPE_LID_TRIGGER_ACTION, "LID close event");

    if ( action == LID_TRIGGER_DPMS )
    {
      if (manager->priv->dpms != NULL && !xfpm_is_multihead_connected ())
        xfpm_dpms_set_mode (manager->priv->dpms, XFPM_DPMS_MODE_OFF);
    }
    else if ( action == LID_TRIGGER_LOCK_SCREEN )
    {
      if ( !xfpm_is_multihead_connected () )
      {
        if (!xfce_screensaver_lock (manager->priv->screensaver))
        {
          xfce_dialog_show_error (NULL, NULL,
                                  _("None of the screen lock tools ran "
                                    "successfully, the screen will not "
                                    "be locked."));
        }
      }
    }
    else if ( action != LID_TRIGGER_NOTHING )
    {
      /*
       * Force sleep here as lid is closed and no point of asking the
       * user for confirmation in case of an application is inhibiting
       * the power manager.
       */
      xfpm_manager_sleep_request (manager, (XfpmShutdownRequest) action, TRUE);
    }
  }
  else
  {
    XFPM_DEBUG_ENUM (action, XFPM_TYPE_LID_TRIGGER_ACTION, "LID opened");

    if (manager->priv->dpms != NULL && action != LID_TRIGGER_NOTHING)
      xfpm_dpms_set_mode (manager->priv->dpms, XFPM_DPMS_MODE_ON);
  }
}

static void
xfpm_manager_inhibit_changed_cb (XfpmInhibit *inhibit, gboolean inhibited, XfpmManager *manager)
{
  manager->priv->inhibited = inhibited;
}

static void
xfpm_manager_alarm_timeout_cb (XfpmIdle *idle, XfpmAlarmId id, XfpmManager *manager)
{
  if (xfpm_power_is_in_presentation_mode (manager->priv->power) == TRUE)
    return;

  XFPM_DEBUG ("Alarm inactivity timeout id %d", id);

  if (id == XFPM_ALARM_ID_INACTIVITY_ON_AC || id == XFPM_ALARM_ID_INACTIVITY_ON_BATTERY)
  {
    XfpmShutdownRequest sleep_mode = XFPM_DO_NOTHING;
    gboolean on_battery;

    if ( manager->priv->inhibited )
    {
      XFPM_DEBUG ("Idle sleep alarm timeout, but power manager is currently inhibited, action ignored");
      return;
    }

    if (id == XFPM_ALARM_ID_INACTIVITY_ON_AC)
      g_object_get (G_OBJECT (manager->priv->conf),
                    INACTIVITY_SLEEP_MODE_ON_AC, &sleep_mode,
                    NULL);
    else
      g_object_get (G_OBJECT (manager->priv->conf),
                    INACTIVITY_SLEEP_MODE_ON_BATTERY, &sleep_mode,
                    NULL);

    g_object_get (G_OBJECT (manager->priv->power),
                  "on-battery", &on_battery,
                  NULL);

    if ((id == XFPM_ALARM_ID_INACTIVITY_ON_AC && !on_battery)
        || (id ==  XFPM_ALARM_ID_INACTIVITY_ON_BATTERY && on_battery))
      xfpm_manager_sleep_request (manager, sleep_mode, FALSE);
  }
}

static void
xfpm_manager_set_idle_alarm_on_ac (XfpmManager *manager)
{
  guint on_ac;

  g_object_get (G_OBJECT (manager->priv->conf),
                ON_AC_INACTIVITY_TIMEOUT, &on_ac,
                NULL);

  if ( on_ac == 14 )
    XFPM_DEBUG ("setting inactivity sleep timeout on ac to never");
  else
    XFPM_DEBUG ("setting inactivity sleep timeout on ac to %d", on_ac);

  if ( on_ac == 14 )
  {
    xfpm_idle_alarm_remove (manager->priv->idle, XFPM_ALARM_ID_INACTIVITY_ON_AC);
  }
  else
  {
    xfpm_idle_alarm_add (manager->priv->idle, XFPM_ALARM_ID_INACTIVITY_ON_AC, on_ac * 1000 * 60);
  }
}

static void
xfpm_manager_set_idle_alarm_on_battery (XfpmManager *manager)
{
  guint on_battery;

  g_object_get (G_OBJECT (manager->priv->conf),
                ON_BATTERY_INACTIVITY_TIMEOUT, &on_battery,
                NULL);

  if ( on_battery == 14 )
    XFPM_DEBUG ("setting inactivity sleep timeout on battery to never");
  else
    XFPM_DEBUG ("setting inactivity sleep timeout on battery to %d", on_battery);

  if ( on_battery == 14 )
  {
    xfpm_idle_alarm_remove (manager->priv->idle, XFPM_ALARM_ID_INACTIVITY_ON_BATTERY);
  }
  else
  {
    xfpm_idle_alarm_add (manager->priv->idle, XFPM_ALARM_ID_INACTIVITY_ON_BATTERY, on_battery * 1000 * 60);
  }
}

static void
xfpm_manager_on_battery_changed_cb (XfpmPower *power, gboolean on_battery, XfpmManager *manager)
{
  xfpm_idle_alarm_reset_all (manager->priv->idle);
}

static gchar*
xfpm_manager_get_systemd_events(XfpmManager *manager)
{
  GSList *events = NULL, *current_event;
  gchar *what = g_strdup ("");
  gboolean logind_handle_power_key, logind_handle_suspend_key, logind_handle_hibernate_key, logind_handle_lid_switch;

  g_object_get (G_OBJECT (manager->priv->conf),
                LOGIND_HANDLE_POWER_KEY, &logind_handle_power_key,
                LOGIND_HANDLE_SUSPEND_KEY, &logind_handle_suspend_key,
                LOGIND_HANDLE_HIBERNATE_KEY, &logind_handle_hibernate_key,
                LOGIND_HANDLE_LID_SWITCH, &logind_handle_lid_switch,
                NULL);

  if (!logind_handle_power_key)
    events = g_slist_append(events, "handle-power-key");
  if (!logind_handle_suspend_key)
    events = g_slist_append(events, "handle-suspend-key");
  if (!logind_handle_hibernate_key)
    events = g_slist_append(events, "handle-hibernate-key");
  if (!logind_handle_lid_switch)
    events = g_slist_append(events, "handle-lid-switch");

  if (events != NULL)
  {
    g_free(what);
    current_event = events;

    what = g_strdup ( (gchar *) current_event->data );
    while ((current_event = g_slist_next (current_event)))
    {
      gchar *what_temp = g_strconcat (what, ":", (gchar *) current_event->data, NULL);
      g_free(what);
      what = what_temp;
    }

    g_slist_free(events);
  }

  return what;
}

static gint
xfpm_manager_inhibit_sleep_systemd (XfpmManager *manager)
{
  GDBusConnection *bus_connection;
  GVariant *reply;
  GError *error = NULL;
  GUnixFDList *fd_list = NULL;
  gint fd = -1;
  char *what = xfpm_manager_get_systemd_events(manager);
  const char *who = "xfce4-power-manager";
  const char *why = "xfce4-power-manager handles these events";
  const char *mode = "block";

  if (manager->priv->systemd == NULL)
    return -1;

  if (g_strcmp0 (what, "") == 0)
    return -1;

  XFPM_DEBUG ("Inhibiting systemd sleep: %s", what);

  bus_connection = manager->priv->system_bus;
  if (!xfpm_dbus_name_has_owner (bus_connection, "org.freedesktop.login1"))
    return -1;

  reply = g_dbus_connection_call_with_unix_fd_list_sync (bus_connection,
                                                         "org.freedesktop.login1",
                                                         "/org/freedesktop/login1",
                                                         "org.freedesktop.login1.Manager",
                                                         "Inhibit",
                                                         g_variant_new ("(ssss)",
                                                                        what, who, why, mode),
                                                         G_VARIANT_TYPE ("(h)"),
                                                         G_DBUS_CALL_FLAGS_NONE,
                                                         -1,
                                                         NULL,
                                                         &fd_list,
                                                         NULL,
                                                         &error);

  if (!reply)
  {
    if (error)
    {
      g_warning ("Unable to inhibit systemd sleep: %s", error->message);
      g_error_free (error);
    }
    return -1;
  }

  g_variant_unref (reply);

  fd = g_unix_fd_list_get (fd_list, 0, &error);
  if (fd == -1)
  {
    g_warning ("Inhibit() reply parsing failed: %s", error->message);
  }

  g_object_unref (fd_list);

  if (error)
    g_error_free (error);

  g_free (what);

  return fd;
}

static void
xfpm_manager_systemd_events_changed (XfpmManager *manager)
{
  if (manager->priv->inhibit_fd >= 0)
    close (manager->priv->inhibit_fd);

  if (manager->priv->system_bus)
    manager->priv->inhibit_fd = xfpm_manager_inhibit_sleep_systemd (manager);
}

static void
xfpm_manager_tray_update_tooltip (PowerManagerButton *button, XfpmManager *manager)
{
  g_return_if_fail (XFPM_IS_MANAGER (manager));
  g_return_if_fail (POWER_MANAGER_IS_BUTTON (manager->priv->power_button));
  g_return_if_fail (GTK_IS_STATUS_ICON (manager->priv->adapter_icon));

  XFPM_DEBUG ("updating tooltip");

  if (power_manager_button_get_tooltip (POWER_MANAGER_BUTTON(manager->priv->power_button)) == NULL)
    return;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_tooltip_markup (manager->priv->adapter_icon, power_manager_button_get_tooltip (POWER_MANAGER_BUTTON(manager->priv->power_button)));
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
xfpm_manager_tray_update_icon (PowerManagerButton *button, XfpmManager *manager)
{
  g_return_if_fail (XFPM_IS_MANAGER (manager));
  g_return_if_fail (POWER_MANAGER_IS_BUTTON (manager->priv->power_button));

  XFPM_DEBUG ("updating icon");

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_from_icon_name (manager->priv->adapter_icon, power_manager_button_get_icon_name (POWER_MANAGER_BUTTON(manager->priv->power_button)));
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
xfpm_manager_show_tray_menu (GtkStatusIcon *icon, guint button, guint activate_time, XfpmManager *manager)
{
  power_manager_button_show_menu (POWER_MANAGER_BUTTON(manager->priv->power_button));
}

static void
xfpm_manager_show_tray_icon (XfpmManager *manager)
{
  if (manager->priv->adapter_icon != NULL)
  {
    XFPM_DEBUG ("tray icon already being shown");
    return;
  }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  manager->priv->adapter_icon = gtk_status_icon_new ();
  manager->priv->power_button = g_object_ref_sink (power_manager_button_new ());
G_GNUC_END_IGNORE_DEPRECATIONS

  XFPM_DEBUG ("Showing tray icon");

  /* send a show event to startup the button */
  power_manager_button_show (POWER_MANAGER_BUTTON(manager->priv->power_button));

  /* initial update the tray icon + tooltip */
  xfpm_manager_tray_update_icon (POWER_MANAGER_BUTTON(manager->priv->power_button), manager);
  xfpm_manager_tray_update_tooltip (POWER_MANAGER_BUTTON(manager->priv->power_button), manager);

  /* Listen to the tooltip and icon changes */
  g_signal_connect (G_OBJECT(manager->priv->power_button), "tooltip-changed",   G_CALLBACK(xfpm_manager_tray_update_tooltip), manager);
  g_signal_connect (G_OBJECT(manager->priv->power_button), "icon-name-changed", G_CALLBACK(xfpm_manager_tray_update_icon),    manager);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_visible (manager->priv->adapter_icon, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS

  g_signal_connect (manager->priv->adapter_icon, "popup-menu", G_CALLBACK (xfpm_manager_show_tray_menu), manager);
}

static void
xfpm_manager_hide_tray_icon (XfpmManager *manager)
{
  if (manager->priv->adapter_icon == NULL)
    return;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_visible (manager->priv->adapter_icon, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS

    /* disconnect from all the signals */
  g_signal_handlers_disconnect_by_func (G_OBJECT(manager->priv->power_button), G_CALLBACK(xfpm_manager_tray_update_tooltip), manager);
  g_signal_handlers_disconnect_by_func (G_OBJECT(manager->priv->power_button), G_CALLBACK(xfpm_manager_tray_update_icon),    manager);
  g_signal_handlers_disconnect_by_func (G_OBJECT(manager->priv->adapter_icon), G_CALLBACK(xfpm_manager_show_tray_menu),      manager);

  g_object_unref (manager->priv->power_button);
  g_object_unref (manager->priv->adapter_icon);

  manager->priv->power_button = NULL;
  manager->priv->adapter_icon = NULL;
}

XfpmManager *
xfpm_manager_new (GDBusConnection *bus, const gchar *client_id)
{
  XfpmManager *manager = NULL;
  GError *error = NULL;
  gchar *current_dir;

  const gchar *restart_command[] =
  {
    "xfce4-power-manager",
    "--restart",
    NULL
  };

  manager = g_object_new (XFPM_TYPE_MANAGER, NULL);

  manager->priv->session_bus = bus;

  current_dir = g_get_current_dir ();
  manager->priv->client = xfce_sm_client_get_full (XFCE_SM_CLIENT_RESTART_NORMAL,
                                                   XFCE_SM_CLIENT_PRIORITY_DEFAULT,
                                                   client_id,
                                                   current_dir,
                                                   restart_command,
                                                   SYSCONFDIR "/xdg/autostart/" PACKAGE_NAME ".desktop");

  g_free (current_dir);

  manager->priv->session_managed = xfce_sm_client_connect (manager->priv->client, &error);

  if ( error )
  {
    g_warning ("Unable to connect to session manager : %s", error->message);
    g_error_free (error);
  }
  else
  {
    g_signal_connect_swapped (manager->priv->client, "quit",
                              G_CALLBACK (xfpm_manager_quit), manager);
  }

  xfpm_manager_dbus_class_init (XFPM_MANAGER_GET_CLASS (manager));
  xfpm_manager_dbus_init (manager);

  return manager;
}

void xfpm_manager_start (XfpmManager *manager)
{
  GError *error = NULL;

  if ( !xfpm_manager_reserve_names (manager) )
  goto out;

  manager->priv->power = xfpm_power_get ();
  manager->priv->button = xfpm_button_new ();
  manager->priv->conf = xfpm_xfconf_new ();
  manager->priv->screensaver = xfce_screensaver_new ();
  manager->priv->console = NULL;
  manager->priv->systemd = NULL;

  if ( LOGIND_RUNNING () )
    manager->priv->systemd = xfce_systemd_get ();
  else
    manager->priv->console = xfce_consolekit_get ();

  manager->priv->monitor = xfpm_dbus_monitor_new ();
  manager->priv->inhibit = xfpm_inhibit_new ();
  manager->priv->ppd = xfpm_ppd_new ();
  manager->priv->idle = xfpm_idle_new ();

    /* Don't allow systemd to handle power/suspend/hibernate buttons
     * and lid-switch */
  manager->priv->system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (manager->priv->system_bus)
    manager->priv->inhibit_fd = xfpm_manager_inhibit_sleep_systemd (manager);
  else
  {
    g_warning ("Unable connect to system bus: %s", error->message);
    g_clear_error (&error);
  }

  if (manager->priv->idle != NULL)
  {
    g_signal_connect (manager->priv->idle, "alarm-expired",
                      G_CALLBACK (xfpm_manager_alarm_timeout_cb), manager);
    g_signal_connect_swapped (manager->priv->conf, "notify::" ON_AC_INACTIVITY_TIMEOUT,
                              G_CALLBACK (xfpm_manager_set_idle_alarm_on_ac), manager);
    g_signal_connect_swapped (manager->priv->conf, "notify::" ON_BATTERY_INACTIVITY_TIMEOUT,
                              G_CALLBACK (xfpm_manager_set_idle_alarm_on_battery), manager);
    xfpm_manager_set_idle_alarm_on_ac (manager);
    xfpm_manager_set_idle_alarm_on_battery (manager);
  }
  g_signal_connect_swapped (manager->priv->conf, "notify::" LOGIND_HANDLE_POWER_KEY,
                            G_CALLBACK (xfpm_manager_systemd_events_changed), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" LOGIND_HANDLE_SUSPEND_KEY,
                            G_CALLBACK (xfpm_manager_systemd_events_changed), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" LOGIND_HANDLE_HIBERNATE_KEY,
                            G_CALLBACK (xfpm_manager_systemd_events_changed), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" LOGIND_HANDLE_LID_SWITCH,
                            G_CALLBACK (xfpm_manager_systemd_events_changed), manager);

  g_signal_connect (manager->priv->inhibit, "has-inhibit-changed",
                    G_CALLBACK (xfpm_manager_inhibit_changed_cb), manager);
  g_signal_connect (manager->priv->monitor, "system-bus-connection-changed",
                    G_CALLBACK (xfpm_manager_system_bus_connection_changed_cb), manager);

  manager->priv->backlight = xfpm_backlight_new ();

  manager->priv->kbd_backlight = xfpm_kbd_backlight_new ();

  manager->priv->dpms = xfpm_dpms_new ();

  g_signal_connect (manager->priv->button, "button_pressed",
                    G_CALLBACK (xfpm_manager_button_pressed_cb), manager);

  g_signal_connect (manager->priv->power, "lid-changed",
                    G_CALLBACK (xfpm_manager_lid_changed_cb), manager);

  g_signal_connect (manager->priv->power, "on-battery-changed",
                    G_CALLBACK (xfpm_manager_on_battery_changed_cb), manager);

  g_signal_connect_swapped (manager->priv->power, "waking-up",
                            G_CALLBACK (xfpm_manager_reset_sleep_timer), manager);

  g_signal_connect_swapped (manager->priv->power, "sleeping",
                            G_CALLBACK (xfpm_manager_reset_sleep_timer), manager);

  g_signal_connect_swapped (manager->priv->power, "ask-shutdown",
                            G_CALLBACK (xfpm_manager_ask_shutdown), manager);

  g_signal_connect_swapped (manager->priv->power, "shutdown",
                            G_CALLBACK (xfpm_manager_shutdown), manager);

  xfconf_g_property_bind (xfpm_xfconf_get_channel (manager->priv->conf),
                                                   XFPM_PROPERTIES_PREFIX SHOW_TRAY_ICON_CFG,
                                                   G_TYPE_INT,
                                                   G_OBJECT(manager),
                                                   SHOW_TRAY_ICON_CFG);
out:
  ;
}

void xfpm_manager_stop (XfpmManager *manager)
{
  XFPM_DEBUG ("Stopping");
  g_return_if_fail (XFPM_IS_MANAGER (manager));
  xfpm_manager_quit (manager);
}

GHashTable *xfpm_manager_get_config (XfpmManager *manager)
{
  GHashTable *hash;

  guint16 mapped_buttons;
  gboolean auth_hibernate = FALSE;
  gboolean auth_suspend = FALSE;
  gboolean can_suspend = FALSE;
  gboolean can_hibernate = FALSE;
  gboolean has_sleep_button = FALSE;
  gboolean has_hibernate_button = FALSE;
  gboolean has_power_button = FALSE;
  gboolean has_battery_button = FALSE;
  gboolean has_battery = TRUE;
  gboolean has_lcd_brightness = manager->priv->backlight != NULL;
  gboolean can_shutdown = TRUE;
  gboolean auth_shutdown = TRUE;
  gboolean has_lid = FALSE;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (manager->priv->systemd != NULL)
  {
    xfce_systemd_can_power_off (manager->priv->systemd, &can_shutdown, &auth_shutdown, NULL);
  }
  else
  {
    xfce_consolekit_can_power_off (manager->priv->console, &can_shutdown, &auth_shutdown, NULL);
  }

  g_object_get (G_OBJECT (manager->priv->power),
                "auth-suspend", &auth_suspend,
                "auth-hibernate", &auth_hibernate,
                "can-suspend", &can_suspend,
                "can-hibernate", &can_hibernate,
                "has-lid", &has_lid,
                NULL);

  has_battery = xfpm_power_has_battery (manager->priv->power);
  mapped_buttons = xfpm_button_get_mapped (manager->priv->button);

  if ( mapped_buttons & SLEEP_KEY )
    has_sleep_button = TRUE;
  if ( mapped_buttons & HIBERNATE_KEY )
    has_hibernate_button = TRUE;
  if ( mapped_buttons & POWER_KEY )
    has_power_button = TRUE;
  if ( mapped_buttons & BATTERY_KEY )
    has_battery_button = TRUE;

  g_hash_table_insert (hash, g_strdup ("sleep-button"), g_strdup (xfpm_bool_to_string (has_sleep_button)));
  g_hash_table_insert (hash, g_strdup ("power-button"), g_strdup (xfpm_bool_to_string (has_power_button)));
  g_hash_table_insert (hash, g_strdup ("hibernate-button"), g_strdup (xfpm_bool_to_string (has_hibernate_button)));
  g_hash_table_insert (hash, g_strdup ("battery-button"), g_strdup (xfpm_bool_to_string (has_battery_button)));
  g_hash_table_insert (hash, g_strdup ("auth-suspend"), g_strdup (xfpm_bool_to_string (auth_suspend)));
  g_hash_table_insert (hash, g_strdup ("auth-hibernate"), g_strdup (xfpm_bool_to_string (auth_hibernate)));
  g_hash_table_insert (hash, g_strdup ("can-suspend"), g_strdup (xfpm_bool_to_string (can_suspend)));
  g_hash_table_insert (hash, g_strdup ("can-hibernate"), g_strdup (xfpm_bool_to_string (can_hibernate)));
  g_hash_table_insert (hash, g_strdup ("can-shutdown"), g_strdup (xfpm_bool_to_string (can_shutdown && auth_shutdown)));

  g_hash_table_insert (hash, g_strdup ("has-battery"), g_strdup (xfpm_bool_to_string (has_battery)));
  g_hash_table_insert (hash, g_strdup ("has-lid"), g_strdup (xfpm_bool_to_string (has_lid)));

  g_hash_table_insert (hash, g_strdup ("has-brightness"), g_strdup (xfpm_bool_to_string (has_lcd_brightness)));

  return hash;
}

/*
 *
 * DBus server implementation
 *
 */
static gboolean xfpm_manager_dbus_quit       (XfpmManager *manager,
                                              GDBusMethodInvocation *invocation,
                                              gpointer user_data);

static gboolean xfpm_manager_dbus_restart     (XfpmManager *manager,
                                               GDBusMethodInvocation *invocation,
                                               gpointer user_data);

static gboolean xfpm_manager_dbus_get_config (XfpmManager *manager,
                                              GDBusMethodInvocation *invocation,
                                              gpointer user_data);

static gboolean xfpm_manager_dbus_get_info   (XfpmManager *manager,
                                              GDBusMethodInvocation *invocation,
                                              gpointer user_data);

#include "xfce-power-manager-dbus.h"

static void
xfpm_manager_dbus_class_init (XfpmManagerClass *klass)
{
}

static void
xfpm_manager_dbus_init (XfpmManager *manager)
{
  XfpmPowerManager *manager_dbus;
  manager_dbus = xfpm_power_manager_skeleton_new ();
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (manager_dbus),
                                    manager->priv->session_bus,
                                    "/org/xfce/PowerManager",
                                    NULL);

  g_signal_connect_swapped (manager_dbus,
                            "handle-quit",
                            G_CALLBACK (xfpm_manager_dbus_quit),
                            manager);
  g_signal_connect_swapped (manager_dbus,
                            "handle-restart",
                            G_CALLBACK (xfpm_manager_dbus_restart),
                            manager);
  g_signal_connect_swapped (manager_dbus,
                            "handle-get-config",
                            G_CALLBACK (xfpm_manager_dbus_get_config),
                            manager);
  g_signal_connect_swapped (manager_dbus,
                            "handle-get-info",
                            G_CALLBACK (xfpm_manager_dbus_get_info),
                            manager);
}

static gboolean
xfpm_manager_dbus_quit (XfpmManager *manager,
                        GDBusMethodInvocation *invocation,
                        gpointer user_data)
{
  XFPM_DEBUG("Quit message received\n");

  xfpm_manager_quit (manager);

  xfpm_power_manager_complete_quit (user_data, invocation);

  return TRUE;
}

static gboolean
xfpm_manager_dbus_restart (XfpmManager *manager,
                           GDBusMethodInvocation *invocation,
                           gpointer user_data)
{
  XFPM_DEBUG("Restart message received");

  xfpm_manager_quit (manager);

  g_spawn_command_line_async ("xfce4-power-manager", NULL);

  xfpm_power_manager_complete_restart (user_data, invocation);

  return TRUE;
}

static void hash_to_variant (gpointer key, gpointer value, gpointer user_data)
{
  g_variant_builder_add (user_data, "{ss}", key, value);
}

static gboolean
xfpm_manager_dbus_get_config (XfpmManager *manager,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data)
{
  GHashTable *config;
  GVariantBuilder builder;
  GVariant *variant;

  config = xfpm_manager_get_config (manager);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));

  g_hash_table_foreach (config, hash_to_variant, &builder);

  g_hash_table_unref (config);

  variant = g_variant_builder_end (&builder);

  xfpm_power_manager_complete_get_config (user_data,
                                      invocation,
                                      variant);

  return TRUE;
}

static gboolean
xfpm_manager_dbus_get_info (XfpmManager *manager,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data)
{

  xfpm_power_manager_complete_get_info (user_data,
                                        invocation,
                                        PACKAGE,
                                        VERSION,
                                        "Xfce-goodies");

  return TRUE;
}
