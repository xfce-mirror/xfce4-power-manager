/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include "xfpm-battery.h"
#include "xfpm-dpms.h"
#include "xfpm-errors.h"
#include "xfpm-idle.h"
#include "xfpm-inhibit.h"
#include "xfpm-notify.h"
#include "xfpm-polkit.h"
#include "xfpm-power.h"
#include "xfpm-suspend.h"
#include "xfpm-xfconf.h"

#include "common/xfpm-brightness.h"
#include "common/xfpm-common.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-enum-types.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"
#include "libdbus/xfpm-dbus.h"

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <upower.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif

static void
xfpm_power_finalize (GObject *object);

static void
xfpm_power_get_property (GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec);

static void
xfpm_power_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec);

static void
xfpm_power_change_presentation_mode (XfpmPower *power,
                                     gboolean presentation_mode);

static void
xfpm_power_toggle_screensaver (XfpmPower *power);

static void
xfpm_power_dbus_class_init (XfpmPowerClass *klass);
static void
xfpm_power_dbus_init (XfpmPower *power);
static void
xfpm_power_can_suspend (XfpmPower *power,
                        gboolean *can_suspend,
                        gboolean *auth_suspend);
static void
xfpm_power_can_hibernate (XfpmPower *power,
                          gboolean *can_hibernate,
                          gboolean *auth_hibernate);
static void
xfpm_power_can_hybrid_sleep (XfpmPower *power,
                             gboolean *can_hybrid_sleep,
                             gboolean *auth_hybrid_sleep);

struct XfpmPowerPrivate
{
  GDBusConnection *bus;

  UpClient *upower;

  GHashTable *hash;


  XfceSystemd *systemd;
  XfceConsolekit *console;
  XfpmInhibit *inhibit;
  XfpmXfconf *conf;

  XfpmBatteryCharge overall_state;
  gboolean critical_action_done;

  XfpmDpms *dpms;
  XfceScreensaver *screensaver;
  gboolean presentation_mode;
  gboolean inhibited;

  XfpmNotify *notify;
#ifdef HAVE_POLKIT
  XfpmPolkit *polkit;
#endif

  /* Properties */
  gboolean on_low_battery;
  gboolean lid_is_present;
  gboolean lid_is_closed;
  gboolean on_battery;
  gchar *daemon_version;

  /**
   * Warning dialog to use when notification daemon
   * doesn't support actions.
   **/
  GtkWidget *dialog;
};

enum
{
  PROP_0,
  PROP_ON_LOW_BATTERY,
  PROP_ON_BATTERY,
  PROP_AUTH_SUSPEND,
  PROP_AUTH_HIBERNATE,
  PROP_AUTH_HYBRID_SLEEP,
  PROP_CAN_SUSPEND,
  PROP_CAN_HIBERNATE,
  PROP_CAN_HYBRID_SLEEP,
  PROP_HAS_LID,
  PROP_LID_IS_CLOSED,
  PROP_PRESENTATION_MODE,
  N_PROPERTIES
};

enum
{
  ON_BATTERY_CHANGED,
  LOW_BATTERY_CHANGED,
  LID_CHANGED,
  WAKING_UP,
  SLEEPING,
  ASK_SHUTDOWN,
  SHUTDOWN,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (XfpmPower, xfpm_power, G_TYPE_OBJECT)


static void
xfpm_power_check_power (XfpmPower *power,
                        gboolean on_battery)
{
  if (on_battery != power->priv->on_battery)
  {
    GList *list;

    power->priv->on_battery = on_battery;
    g_signal_emit (G_OBJECT (power), signals[ON_BATTERY_CHANGED], 0, power->priv->on_battery);

    if (power->priv->dpms != NULL)
      xfpm_dpms_set_on_battery (power->priv->dpms, on_battery);

    /* Dismiss critical notifications on battery state changes */
    xfpm_notify_close_critical (power->priv->notify);

    list = g_hash_table_get_values (power->priv->hash);
    for (GList *lp = list; lp != NULL; lp = lp->next)
    {
      g_object_set (G_OBJECT (lp->data),
                    "ac-online", !on_battery,
                    NULL);
    }
    g_list_free (list);
  }
}

static void
xfpm_power_check_lid (XfpmPower *power,
                      gboolean present,
                      gboolean closed)
{
  power->priv->lid_is_present = present;

  if (power->priv->lid_is_present)
  {
    if (closed != power->priv->lid_is_closed)
    {
      power->priv->lid_is_closed = closed;
      g_signal_emit (G_OBJECT (power), signals[LID_CHANGED], 0, power->priv->lid_is_closed);
    }
  }
}

/*
 * Get the properties on org.freedesktop.DeviceKit.Power
 *
 * DaemonVersion      's'
 * CanSuspend'        'b'
 * CanHibernate'      'b'
 * OnBattery'         'b'
 * OnLowBattery'      'b'
 * LidIsClosed'       'b'
 * LidIsPresent'      'b'
 */
static void
xfpm_power_get_properties (XfpmPower *power)
{
  gboolean on_battery = FALSE;
  gboolean lid_is_closed = FALSE;
  gboolean lid_is_present = FALSE;

  if (power->priv->upower != NULL)
    g_object_get (power->priv->upower,
                  "on-battery", &on_battery,
                  "lid-is-closed", &lid_is_closed,
                  "lid-is-present", &lid_is_present,
                  NULL);

  xfpm_power_check_lid (power, lid_is_present, lid_is_closed);
  xfpm_power_check_power (power, on_battery);
}

static void
xfpm_power_report_error (XfpmPower *power,
                         const gchar *error,
                         const gchar *icon_name)
{
  xfpm_notify_show_notification (power->priv->notify,
                                 _("Power Manager"),
                                 error,
                                 icon_name,
                                 XFPM_NOTIFY_CRITICAL);
}

static void
xfpm_power_sleep (XfpmPower *power,
                  const gchar *sleep_time,
                  gboolean force)
{
  GError *error = NULL;
  gboolean lock_screen;
  XfpmBrightness *brightness;
  gint32 brightness_level = 0;

  if ((power->priv->presentation_mode || power->priv->inhibited) && !force)
  {
    GtkWidget *dialog;
    gboolean ret;

    dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO,
                                     _("An application is currently disabling the automatic sleep. "
                                       "Doing this action now may damage the working state of this application.\n"
                                       "Are you sure you want to hibernate the system?"));
    ret = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    if (!ret || ret == GTK_RESPONSE_NO)
      return;
  }

  g_signal_emit (G_OBJECT (power), signals[SLEEPING], 0);

  /* Get the current brightness level so we can use it after we suspend */
  brightness = xfpm_brightness_new ();
  if (brightness != NULL)
    xfpm_brightness_get_level (brightness, &brightness_level);

  g_object_get (G_OBJECT (power->priv->conf),
                LOCK_SCREEN_SUSPEND_HIBERNATE, &lock_screen,
                NULL);

  if (lock_screen)
  {
    if (!xfce_screensaver_lock (power->priv->screensaver) && !force)
    {
      GtkWidget *dialog;
      gboolean ret;

      dialog = gtk_message_dialog_new (NULL,
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_YES_NO,
                                       _("None of the screen lock tools ran "
                                         "successfully, the screen will not "
                                         "be locked.\n"
                                         "Do you still want to continue to "
                                         "suspend the system?"));
      ret = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (!ret || ret == GTK_RESPONSE_NO)
      {
        if (brightness != NULL)
          g_object_unref (brightness);
        return;
      }
    }
  }

  if (g_strcmp0 (sleep_time, "Hibernate") == 0)
  {
    if (power->priv->systemd != NULL)
    {
      if (xfce_systemd_can_hibernate (power->priv->systemd, NULL, NULL, NULL)
          && !xfce_systemd_hibernate (power->priv->systemd, TRUE, &error))
      {
        g_warning ("Failed to hibernate via systemd: %s", error->message);
      }
    }
    else if (xfce_consolekit_can_hibernate (power->priv->console, NULL, NULL, NULL)
             && !xfce_consolekit_hibernate (power->priv->console, TRUE, &error))
    {
      g_warning ("Failed to hibernate via ConsoleKit: %s", error->message);
    }

    if (error != NULL && xfpm_suspend_try_action (XFPM_HIBERNATE))
      g_clear_error (&error);
  }
  else if (g_strcmp0 (sleep_time, "Suspend") == 0)
  {
    if (power->priv->systemd != NULL)
    {
      if (xfce_systemd_can_suspend (power->priv->systemd, NULL, NULL, NULL)
          && !xfce_systemd_suspend (power->priv->systemd, TRUE, &error))
      {
        g_warning ("Failed to suspend via systemd: %s", error->message);
      }
    }
    else if (xfce_consolekit_can_suspend (power->priv->console, NULL, NULL, NULL)
             && !xfce_consolekit_suspend (power->priv->console, TRUE, &error))
    {
      g_warning ("Failed to suspend via ConsoleKit: %s", error->message);
    }

    if (error != NULL && xfpm_suspend_try_action (XFPM_SUSPEND))
      g_clear_error (&error);
  }
  else if (g_strcmp0 (sleep_time, "HybridSleep") == 0)
  {
    if (power->priv->systemd != NULL)
    {
      if (xfce_systemd_can_hybrid_sleep (power->priv->systemd, NULL, NULL, NULL)
          && !xfce_systemd_hybrid_sleep (power->priv->systemd, TRUE, &error))
      {
        g_warning ("Failed to hybrid sleep via systemd: %s", error->message);
      }
    }
    else if (xfce_consolekit_can_hybrid_sleep (power->priv->console, NULL, NULL, NULL)
             && !xfce_consolekit_hybrid_sleep (power->priv->console, TRUE, &error))
    {
      g_warning ("Failed to hybrid sleep via ConsoleKit: %s", error->message);
    }

    if (error != NULL && xfpm_suspend_try_action (XFPM_HYBRID_SLEEP))
      g_clear_error (&error);
  }
  else
  {
    g_warn_if_reached ();
  }

  if (error)
  {
    if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY))
    {
      XFPM_DEBUG ("D-Bus time out, but should be harmless");
    }
    else
    {
      xfpm_power_report_error (power, error->message, "dialog-error");
      g_error_free (error);
    }
  }

  g_signal_emit (G_OBJECT (power), signals[WAKING_UP], 0);

  /* Check/update any changes while we slept */
  xfpm_power_get_properties (power);

  /* Restore the brightness level from before we suspended */
  if (brightness != NULL)
  {
    xfpm_brightness_set_level (brightness, brightness_level);
    g_object_unref (brightness);
  }
}

static void
xfpm_power_hibernate_clicked (XfpmPower *power)
{
  gtk_widget_destroy (power->priv->dialog);
  power->priv->dialog = NULL;
  xfpm_power_sleep (power, "Hibernate", TRUE);
}

static void
xfpm_power_suspend_clicked (XfpmPower *power)
{
  gtk_widget_destroy (power->priv->dialog);
  power->priv->dialog = NULL;
  xfpm_power_sleep (power, "Suspend", TRUE);
}

static void
xfpm_power_hybrid_sleep_clicked (XfpmPower *power)
{
  gtk_widget_destroy (power->priv->dialog);
  power->priv->dialog = NULL;
  xfpm_power_sleep (power, "HybridSleep", TRUE);
}

static void
xfpm_power_shutdown_clicked (XfpmPower *power)
{
  gtk_widget_destroy (power->priv->dialog);
  power->priv->dialog = NULL;
  g_signal_emit (G_OBJECT (power), signals[SHUTDOWN], 0);
}

static XfpmBatteryCharge
xfpm_power_get_current_charge_state (XfpmPower *power)
{
  GList *list;
  XfpmBatteryCharge max_charge_status = XFPM_BATTERY_CHARGE_UNKNOWN;

  list = g_hash_table_get_values (power->priv->hash);
  for (GList *lp = list; lp != NULL; lp = lp->next)
  {
    XfpmBatteryCharge battery_charge;
    UpDeviceKind type;
    gboolean power_supply;

    g_object_get (G_OBJECT (lp->data),
                  "charge-status", &battery_charge,
                  "device-type", &type,
                  "ac-online", &power_supply,
                  NULL);
    if ((type != UP_DEVICE_KIND_BATTERY && type != UP_DEVICE_KIND_UPS) || power_supply)
      continue;

    max_charge_status = MAX (max_charge_status, battery_charge);
  }
  g_list_free (list);

  return max_charge_status;
}

static void
xfpm_power_notify_action_callback (NotifyNotification *n,
                                   gchar *action,
                                   XfpmPower *power)
{
  if (g_strcmp0 (action, "Shutdown") == 0)
    g_signal_emit (G_OBJECT (power), signals[SHUTDOWN], 0);
  else
    xfpm_power_sleep (power, action, TRUE);
}

static void
xfpm_power_add_actions_to_notification (XfpmPower *power,
                                        NotifyNotification *n)
{
  gboolean can_method, auth_method;

  xfpm_power_can_hibernate (power, &can_method, &auth_method);
  if (can_method && auth_method)
  {
    xfpm_notify_add_action_to_notification (
      power->priv->notify,
      n,
      "Hibernate",
      _("Hibernate the system"),
      (NotifyActionCallback) xfpm_power_notify_action_callback,
      power);
  }

  xfpm_power_can_suspend (power, &can_method, &auth_method);
  if (can_method && auth_method)
  {
    xfpm_notify_add_action_to_notification (
      power->priv->notify,
      n,
      "Suspend",
      _("Suspend the system"),
      (NotifyActionCallback) xfpm_power_notify_action_callback,
      power);
  }

  xfpm_power_can_hybrid_sleep (power, &can_method, &auth_method);
  if (can_method && auth_method)
  {
    xfpm_notify_add_action_to_notification (
      power->priv->notify,
      n,
      "HybridSleep",
      _("Hybrid sleep the system"),
      (NotifyActionCallback) xfpm_power_notify_action_callback,
      power);
  }

  if (power->priv->systemd != NULL)
  {
    xfce_systemd_can_power_off (power->priv->systemd, &can_method, &auth_method, NULL);
  }
  else
  {
    xfce_consolekit_can_power_off (power->priv->console, &can_method, &auth_method, NULL);
  }

  if (can_method && auth_method)
  {
    xfpm_notify_add_action_to_notification (
      power->priv->notify,
      n,
      "Shutdown",
      _("Shutdown the system"),
      (NotifyActionCallback) xfpm_power_notify_action_callback,
      power);
  }
}

static void
xfpm_power_show_critical_action_notification (XfpmPower *power,
                                              XfpmBattery *battery)
{
  const gchar *message;
  NotifyNotification *n;

  message = _("System is running on low power. "\
               "Save your work to avoid losing data");

  n = xfpm_notify_new_notification (power->priv->notify,
                                    _("Power Manager"),
                                    message,
                                    xfpm_battery_get_icon_name (battery),
                                    XFPM_NOTIFY_CRITICAL);

  xfpm_power_add_actions_to_notification (power, n);
  xfpm_notify_critical (power->priv->notify, n);
}

static void
xfpm_power_close_critical_dialog (XfpmPower *power)
{
  gtk_widget_destroy (power->priv->dialog);
  power->priv->dialog = NULL;
}

static void
critical_dialog_add_button (GtkDialog *dialog,
                            const gchar *label,
                            GCallback callback,
                            XfpmPower *power)
{
  GtkWidget *button = gtk_button_new_with_label (label);
  gtk_dialog_add_action_widget (dialog, button, GTK_RESPONSE_NONE);
  g_signal_connect_object (button, "clicked", callback, power, G_CONNECT_SWAPPED);

  /* we don't want the user to unintentionally trigger an action when this dialog is shown */
  gtk_widget_set_can_focus (button, FALSE);
}

static void
xfpm_power_show_critical_action_gtk (XfpmPower *power)
{
  GtkWidget *dialog;
  GtkWidget *content_area;
  const gchar *message;
  gboolean can_method, auth_method;

  message = _("System is running on low power. "\
              "Save your work to avoid losing data");

  dialog = gtk_dialog_new_with_buttons (_("Power Manager"), NULL, GTK_DIALOG_MODAL,
                                        NULL, NULL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  gtk_box_pack_start (GTK_BOX (content_area), gtk_label_new (message),
                      TRUE, TRUE, 8);

  xfpm_power_can_hibernate (power, &can_method, &auth_method);
  if (can_method && auth_method)
    critical_dialog_add_button (GTK_DIALOG (dialog), _("Hibernate"),
                                G_CALLBACK (xfpm_power_hibernate_clicked), power);

  xfpm_power_can_suspend (power, &can_method, &auth_method);
  if (can_method && auth_method)
    critical_dialog_add_button (GTK_DIALOG (dialog), _("Suspend"),
                                G_CALLBACK (xfpm_power_suspend_clicked), power);

  xfpm_power_can_hybrid_sleep (power, &can_method, &auth_method);
  if (can_method && auth_method)
    critical_dialog_add_button (GTK_DIALOG (dialog), _("Hybrid Sleep"),
                                G_CALLBACK (xfpm_power_hybrid_sleep_clicked), power);

  if (power->priv->systemd != NULL)
  {
    xfce_systemd_can_power_off (power->priv->systemd, &can_method, &auth_method, NULL);
  }
  else
  {
    xfce_consolekit_can_power_off (power->priv->console, &can_method, &auth_method, NULL);
  }

  if (can_method && auth_method)
    critical_dialog_add_button (GTK_DIALOG (dialog), _("Shutdown"),
                                G_CALLBACK (xfpm_power_shutdown_clicked), power);

  critical_dialog_add_button (GTK_DIALOG (dialog), _("Cancel"),
                              G_CALLBACK (xfpm_power_close_critical_dialog), power);

  g_signal_connect_object (dialog, "destroy",
                           G_CALLBACK (xfpm_power_close_critical_dialog), power, G_CONNECT_SWAPPED);
  if (power->priv->dialog)
  {
    gtk_widget_destroy (power->priv->dialog);
    power->priv->dialog = NULL;
  }
  power->priv->dialog = dialog;
  gtk_widget_show_all (dialog);
}

static void
xfpm_power_show_critical_action (XfpmPower *power,
                                 XfpmBattery *battery)
{
  gboolean supports_actions;

  g_object_get (G_OBJECT (power->priv->notify),
                "actions", &supports_actions,
                NULL);

  if (supports_actions)
    xfpm_power_show_critical_action_notification (power, battery);
  else
    xfpm_power_show_critical_action_gtk (power);
}

static void
xfpm_power_process_critical_action (XfpmPower *power,
                                    XfpmShutdownRequest req)
{
  if (req == XFPM_DO_SUSPEND)
    xfpm_power_sleep (power, "Suspend", TRUE);
  else if (req == XFPM_DO_HIBERNATE)
    xfpm_power_sleep (power, "Hibernate", TRUE);
  else if (req == XFPM_DO_HYBRID_SLEEP)
    xfpm_power_sleep (power, "HybridSleep", TRUE);
  else if (req == XFPM_DO_SHUTDOWN)
    g_signal_emit (G_OBJECT (power), signals[SHUTDOWN], 0);
  else
    g_warn_if_reached ();
}

static void
xfpm_power_system_on_critical_power (XfpmPower *power,
                                     XfpmBattery *battery)
{
  XfpmShutdownRequest critical_action;

  g_object_get (G_OBJECT (power->priv->conf),
                CRITICAL_POWER_ACTION, &critical_action,
                NULL);

  XFPM_DEBUG ("System is running on low power");
  XFPM_DEBUG_ENUM (critical_action, XFPM_TYPE_SHUTDOWN_REQUEST, "Critical battery action");

  if (critical_action == XFPM_DO_NOTHING)
  {
    xfpm_power_show_critical_action (power, battery);
  }
  else
  {
    if (!power->priv->critical_action_done)
    {
      power->priv->critical_action_done = TRUE;
      xfpm_power_process_critical_action (power, critical_action);
    }
    else
    {
      xfpm_power_show_critical_action (power, battery);
    }
  }
}

static void
xfpm_power_battery_charge_changed_cb (XfpmBattery *battery,
                                      XfpmPower *power)
{
  gboolean notify;
  XfpmBatteryCharge battery_charge;
  XfpmBatteryCharge current_charge;

  battery_charge = xfpm_battery_get_charge (battery);
  current_charge = xfpm_power_get_current_charge_state (power);

  XFPM_DEBUG_ENUM (current_charge, XFPM_TYPE_BATTERY_CHARGE, "Current system charge status");

  if (current_charge == power->priv->overall_state)
    return;

  if (current_charge >= XFPM_BATTERY_CHARGE_LOW)
    power->priv->critical_action_done = FALSE;

  power->priv->overall_state = current_charge;

  if (current_charge == XFPM_BATTERY_CHARGE_CRITICAL && power->priv->on_battery)
  {
    xfpm_power_system_on_critical_power (power, battery);

    power->priv->on_low_battery = TRUE;
    g_signal_emit (G_OBJECT (power), signals[LOW_BATTERY_CHANGED], 0, power->priv->on_low_battery);
    return;
  }

  if (power->priv->on_low_battery)
  {
    power->priv->on_low_battery = FALSE;
    g_signal_emit (G_OBJECT (power), signals[LOW_BATTERY_CHANGED], 0, power->priv->on_low_battery);
  }

  g_object_get (G_OBJECT (power->priv->conf),
                GENERAL_NOTIFICATION, &notify,
                NULL);

  if (power->priv->on_battery)
  {
    if (current_charge == XFPM_BATTERY_CHARGE_LOW)
    {
      if (notify)
        xfpm_notify_show_notification (power->priv->notify,
                                       _("Power Manager"), _("System is running on low power"),
                                       xfpm_battery_get_icon_name (battery),
                                       XFPM_NOTIFY_NORMAL);
    }
    else if (battery_charge == XFPM_BATTERY_CHARGE_LOW)
    {
      if (notify)
      {
        gchar *msg;
        gchar *time_str;

        const gchar *battery_name = xfpm_battery_get_battery_name (battery);

        time_str = xfpm_battery_get_time_left (battery);

        msg = g_strdup_printf (_("Your %s charge level is low\nEstimated time left %s"), battery_name, time_str);


        xfpm_notify_show_notification (power->priv->notify,
                                       _("Power Manager"),
                                       msg,
                                       xfpm_battery_get_icon_name (battery),
                                       XFPM_NOTIFY_NORMAL);
        g_free (msg);
        g_free (time_str);
      }
    }
  }

  /*Current charge is okay now, then close the dialog*/
  if (power->priv->dialog)
  {
    gtk_widget_destroy (power->priv->dialog);
    power->priv->dialog = NULL;
  }
}

static void
xfpm_power_add_device (UpDevice *device,
                       XfpmPower *power)
{
  guint device_type = UP_DEVICE_KIND_UNKNOWN;
  const gchar *object_path = up_device_get_object_path (device);

  /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
  g_object_get (device,
                "kind", &device_type,
                NULL);

  XFPM_DEBUG ("'%s' device added", up_device_kind_to_string (device_type));

  if (device_type == UP_DEVICE_KIND_BATTERY
      || device_type == UP_DEVICE_KIND_UPS
      || device_type == UP_DEVICE_KIND_MOUSE
      || device_type == UP_DEVICE_KIND_KEYBOARD
      || device_type == UP_DEVICE_KIND_PHONE)
  {
    GtkWidget *battery;
    XFPM_DEBUG ("Battery device type '%s' detected at: %s",
                up_device_kind_to_string (device_type), object_path);
    battery = g_object_ref_sink (xfpm_battery_new ());

    xfpm_battery_monitor_device (XFPM_BATTERY (battery),
                                 object_path,
                                 device_type);
    g_hash_table_insert (power->priv->hash, g_strdup (object_path), battery);

    g_signal_connect_object (battery, "battery-charge-changed",
                             G_CALLBACK (xfpm_power_battery_charge_changed_cb), power, 0);
  }
}

static void
xfpm_power_get_power_devices (XfpmPower *power)
{
  GPtrArray *array = up_client_get_devices2 (power->priv->upower);
  guint i;

  if (array)
  {
    for (i = 0; i < array->len; i++)
    {
      UpDevice *device = g_ptr_array_index (array, i);
      const gchar *object_path = up_device_get_object_path (device);
      XFPM_DEBUG ("Power device detected at : %s", object_path);
      xfpm_power_add_device (device, power);
    }
    g_ptr_array_free (array, TRUE);
  }
}

static void
xfpm_power_remove_device (XfpmPower *power,
                          const gchar *object_path)
{
  g_hash_table_remove (power->priv->hash, object_path);
}

static void
xfpm_power_inhibit_changed_cb (XfpmInhibit *inhibit,
                               gboolean is_inhibit,
                               XfpmPower *power)
{
  /* no change, exit */
  if (power->priv->inhibited == is_inhibit)
    return;

  power->priv->inhibited = is_inhibit;

  XFPM_DEBUG ("inhibited %s, presentation_mode %s",
              power->priv->inhibited ? "TRUE" : "FALSE",
              power->priv->presentation_mode ? "TRUE" : "FALSE");

  /* either inhibition already occurred, or we don't want to remove it yet */
  if (power->priv->presentation_mode)
    return;

  xfce_screensaver_inhibit (power->priv->screensaver, is_inhibit);
  xfpm_power_toggle_screensaver (power);
  if (power->priv->dpms != NULL)
    xfpm_dpms_set_inhibited (power->priv->dpms, is_inhibit);
}

static void
xfpm_power_changed_cb (UpClient *upower,
                       GParamSpec *pspec,
                       XfpmPower *power)
{
  xfpm_power_get_properties (power);
}

static void
xfpm_power_device_added_cb (UpClient *upower,
                            UpDevice *device,
                            XfpmPower *power)
{
  xfpm_power_add_device (device, power);
}

static void
xfpm_power_device_removed_cb (UpClient *upower,
                              const gchar *object_path,
                              XfpmPower *power)
{
  xfpm_power_remove_device (power, object_path);
}

static void
xfpm_power_class_init (XfpmPowerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xfpm_power_finalize;

  object_class->get_property = xfpm_power_get_property;
  object_class->set_property = xfpm_power_set_property;

  signals[ON_BATTERY_CHANGED] = g_signal_new ("on-battery-changed",
                                              XFPM_TYPE_POWER,
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (XfpmPowerClass, on_battery_changed),
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__BOOLEAN,
                                              G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[LOW_BATTERY_CHANGED] = g_signal_new ("low-battery-changed",
                                               XFPM_TYPE_POWER,
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET (XfpmPowerClass, low_battery_changed),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__BOOLEAN,
                                               G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[LID_CHANGED] = g_signal_new ("lid-changed",
                                       XFPM_TYPE_POWER,
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET (XfpmPowerClass, lid_changed),
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__BOOLEAN,
                                       G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[WAKING_UP] = g_signal_new ("waking-up",
                                     XFPM_TYPE_POWER,
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (XfpmPowerClass, waking_up),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0, G_TYPE_NONE);

  signals[SLEEPING] = g_signal_new ("sleeping",
                                    XFPM_TYPE_POWER,
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (XfpmPowerClass, sleeping),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE, 0, G_TYPE_NONE);

  signals[ASK_SHUTDOWN] = g_signal_new ("ask-shutdown",
                                        XFPM_TYPE_POWER,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (XfpmPowerClass, ask_shutdown),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE, 0, G_TYPE_NONE);

  signals[SHUTDOWN] = g_signal_new ("shutdown",
                                    XFPM_TYPE_POWER,
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (XfpmPowerClass, shutdown),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE, 0, G_TYPE_NONE);

#define XFPM_PARAM_FLAGS (G_PARAM_READWRITE \
                          | G_PARAM_CONSTRUCT \
                          | G_PARAM_STATIC_NAME \
                          | G_PARAM_STATIC_NICK \
                          | G_PARAM_STATIC_BLURB)

  g_object_class_install_property (object_class,
                                   PROP_ON_BATTERY,
                                   g_param_spec_boolean ("on-battery",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_ON_LOW_BATTERY,
                                   g_param_spec_boolean ("on-low-battery",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_AUTH_SUSPEND,
                                   g_param_spec_boolean ("auth-suspend",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_AUTH_HIBERNATE,
                                   g_param_spec_boolean ("auth-hibernate",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_AUTH_HYBRID_SLEEP,
                                   g_param_spec_boolean ("auth-hybrid-sleep",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_CAN_HIBERNATE,
                                   g_param_spec_boolean ("can-hibernate",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_CAN_HYBRID_SLEEP,
                                   g_param_spec_boolean ("can-hybrid-sleep",
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
                                   PROP_HAS_LID,
                                   g_param_spec_boolean ("has-lid",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_LID_IS_CLOSED,
                                   g_param_spec_boolean ("lid-is-closed",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_PRESENTATION_MODE,
                                   g_param_spec_boolean (PRESENTATION_MODE,
                                                         NULL, NULL,
                                                         DEFAULT_PRESENTATION_MODE,
                                                         XFPM_PARAM_FLAGS));
#undef XFPM_PARAM_FLAGS

  xfpm_power_dbus_class_init (klass);
}

static void
xfpm_power_init (XfpmPower *power)
{
  GError *error = NULL;

  power->priv = xfpm_power_get_instance_private (power);

  power->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  power->priv->lid_is_present = FALSE;
  power->priv->lid_is_closed = FALSE;
  power->priv->on_battery = FALSE;
  power->priv->on_low_battery = FALSE;
  power->priv->daemon_version = NULL;
  power->priv->dialog = NULL;
  power->priv->overall_state = XFPM_BATTERY_CHARGE_OK;
  power->priv->critical_action_done = FALSE;

  power->priv->dpms = xfpm_dpms_new ();

  power->priv->presentation_mode = FALSE;

  power->priv->inhibit = xfpm_inhibit_new ();
  power->priv->notify = xfpm_notify_new ();
  power->priv->conf = xfpm_xfconf_new ();
  power->priv->upower = up_client_new ();
  power->priv->screensaver = xfce_screensaver_new ();

  power->priv->systemd = NULL;
  power->priv->console = NULL;
  if (LOGIND_RUNNING ())
    power->priv->systemd = xfce_systemd_get ();
  else
    power->priv->console = xfce_consolekit_get ();

#ifdef HAVE_POLKIT
  power->priv->polkit = xfpm_polkit_get ();
#endif

  g_signal_connect_object (power->priv->inhibit, "has-inhibit-changed",
                           G_CALLBACK (xfpm_power_inhibit_changed_cb), power, 0);

  power->priv->bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

  if (error)
  {
    g_critical ("Unable to connect to the system bus : %s", error->message);
    g_error_free (error);
    goto out;
  }

  if (power->priv->upower != NULL)
  {
    g_signal_connect_object (power->priv->upower, "device-added", G_CALLBACK (xfpm_power_device_added_cb), power, 0);
    g_signal_connect_object (power->priv->upower, "device-removed", G_CALLBACK (xfpm_power_device_removed_cb), power, 0);
    g_signal_connect_object (power->priv->upower, "notify", G_CALLBACK (xfpm_power_changed_cb), power, 0);
    xfpm_power_get_power_devices (power);
  }

  xfpm_power_get_properties (power);

out:
  xfpm_power_dbus_init (power);

  /*
   * Emit org.freedesktop.PowerManagement session signals on startup
   */
  g_signal_emit (G_OBJECT (power), signals[ON_BATTERY_CHANGED], 0, power->priv->on_battery);
}

static void
xfpm_power_get_property (GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  XfpmPower *power = XFPM_POWER (object);
  gboolean bool_value;

  switch (prop_id)
  {
    case PROP_ON_BATTERY:
      g_value_set_boolean (value, power->priv->on_battery);
      break;
    case PROP_AUTH_HIBERNATE:
      xfpm_power_can_hibernate (power, NULL, &bool_value);
      g_value_set_boolean (value, bool_value);
      break;
    case PROP_AUTH_HYBRID_SLEEP:
      xfpm_power_can_hybrid_sleep (power, NULL, &bool_value);
      g_value_set_boolean (value, bool_value);
      break;
    case PROP_AUTH_SUSPEND:
      xfpm_power_can_suspend (power, NULL, &bool_value);
      g_value_set_boolean (value, bool_value);
      break;
    case PROP_CAN_SUSPEND:
      xfpm_power_can_suspend (power, &bool_value, NULL);
      g_value_set_boolean (value, bool_value);
      break;
    case PROP_CAN_HIBERNATE:
      xfpm_power_can_hibernate (power, &bool_value, NULL);
      g_value_set_boolean (value, bool_value);
      break;
    case PROP_CAN_HYBRID_SLEEP:
      xfpm_power_can_hybrid_sleep (power, &bool_value, NULL);
      g_value_set_boolean (value, bool_value);
      break;
    case PROP_HAS_LID:
      g_value_set_boolean (value, power->priv->lid_is_present);
      break;
    case PROP_LID_IS_CLOSED:
      g_value_set_boolean (value, power->priv->lid_is_closed);
      break;
    case PROP_PRESENTATION_MODE:
      g_value_set_boolean (value, power->priv->presentation_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
xfpm_power_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  XfpmPower *power = XFPM_POWER (object);

  switch (prop_id)
  {
    case PROP_PRESENTATION_MODE:
      xfpm_power_change_presentation_mode (power, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
xfpm_power_finalize (GObject *object)
{
  XfpmPower *power;

  power = XFPM_POWER (object);

  g_free (power->priv->daemon_version);

  g_object_unref (power->priv->inhibit);
  g_object_unref (power->priv->notify);
  g_object_unref (power->priv->conf);
  g_object_unref (power->priv->screensaver);
  if (power->priv->upower != NULL)
    g_object_unref (power->priv->upower);

  if (power->priv->systemd != NULL)
    g_object_unref (power->priv->systemd);
  if (power->priv->console != NULL)
    g_object_unref (power->priv->console);

  g_object_unref (power->priv->bus);

  g_hash_table_destroy (power->priv->hash);

#ifdef HAVE_POLKIT
  g_object_unref (power->priv->polkit);
#endif

  if (power->priv->dpms != NULL)
    g_object_unref (power->priv->dpms);

  G_OBJECT_CLASS (xfpm_power_parent_class)->finalize (object);
}

static XfpmPower *
xfpm_power_new (void)
{
  XfpmPower *power = XFPM_POWER (g_object_new (XFPM_TYPE_POWER, NULL));

  xfconf_g_property_bind (xfpm_xfconf_get_channel (power->priv->conf),
                          XFPM_PROPERTIES_PREFIX PRESENTATION_MODE, G_TYPE_BOOLEAN,
                          G_OBJECT (power), PRESENTATION_MODE);

  return power;
}

XfpmPower *
xfpm_power_get (void)
{
  static gpointer xfpm_power_object = NULL;

  if (G_LIKELY (xfpm_power_object != NULL))
  {
    g_object_ref (xfpm_power_object);
  }
  else
  {
    xfpm_power_object = xfpm_power_new ();
    g_object_add_weak_pointer (xfpm_power_object, &xfpm_power_object);
  }

  return XFPM_POWER (xfpm_power_object);
}

void
xfpm_power_suspend (XfpmPower *power,
                    gboolean force)
{
  xfpm_power_sleep (power, "Suspend", force);
}

void
xfpm_power_hibernate (XfpmPower *power,
                      gboolean force)
{
  xfpm_power_sleep (power, "Hibernate", force);
}

void
xfpm_power_hybrid_sleep (XfpmPower *power,
                         gboolean force)
{
  xfpm_power_sleep (power, "HybridSleep", force);
}

gboolean
xfpm_power_has_battery (XfpmPower *power)
{
  GList *list;
  gboolean ret = FALSE;

  list = g_hash_table_get_values (power->priv->hash);
  for (GList *lp = list; lp != NULL; lp = lp->next)
  {
    UpDeviceKind type;
    type = xfpm_battery_get_device_type (XFPM_BATTERY (lp->data));
    if (type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS)
    {
      ret = TRUE;
      break;
    }
  }
  g_list_free (list);

  return ret;
}

static void
xfpm_power_toggle_screensaver (XfpmPower *power)
{
#ifdef ENABLE_X11
  Display *display;
  static int timeout = -2, interval, prefer_blanking, allow_exposures;

  if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    return;

  display = gdk_x11_get_default_xdisplay ();

  /* Presentation mode or inhibited disables blanking */
  if (power->priv->presentation_mode || power->priv->inhibited)
  {
    if (timeout == -2)
      XGetScreenSaver (display, &timeout, &interval, &prefer_blanking, &allow_exposures);

    XFPM_DEBUG ("Disabling screensaver, timeout stored: %d", timeout);
    XSetScreenSaver (display, 0, interval, prefer_blanking, allow_exposures);
    XSync (display, FALSE);
  }
  else if (timeout != -2)
  {
    XFPM_DEBUG ("Enabling screensaver, timeout restored: %d", timeout);
    XSetScreenSaver (display, timeout, interval, prefer_blanking, allow_exposures);
    XSync (display, FALSE);
    timeout = -2;
  }
#endif
}

static void
xfpm_power_change_presentation_mode (XfpmPower *power,
                                     gboolean presentation_mode)
{
  /* no change, exit */
  if (power->priv->presentation_mode == presentation_mode)
    return;

  power->priv->presentation_mode = presentation_mode;

  XFPM_DEBUG ("inhibited %s, presentation_mode %s",
              power->priv->inhibited ? "TRUE" : "FALSE",
              power->priv->presentation_mode ? "TRUE" : "FALSE");

  /* either inhibition already occurred, or we don't want to remove it yet */
  if (power->priv->inhibited)
    return;

  xfce_screensaver_inhibit (power->priv->screensaver, presentation_mode);
  xfpm_power_toggle_screensaver (power);
  if (power->priv->dpms != NULL)
    xfpm_dpms_set_inhibited (power->priv->dpms, presentation_mode);
}

gboolean
xfpm_power_is_inhibited (XfpmPower *power)
{
  g_return_val_if_fail (XFPM_IS_POWER (power), FALSE);

  return power->priv->presentation_mode || power->priv->inhibited;
}


/*
 *
 * DBus server implementation for org.freedesktop.PowerManagement
 *
 */
static gboolean
xfpm_power_dbus_shutdown (XfpmPower *power,
                          GDBusMethodInvocation *invocation,
                          gpointer user_data);

static gboolean
xfpm_power_dbus_reboot (XfpmPower *power,
                        GDBusMethodInvocation *invocation,
                        gpointer user_data);

static gboolean
xfpm_power_dbus_hibernate (XfpmPower *power,
                           GDBusMethodInvocation *invocation,
                           gpointer user_data);

static gboolean
xfpm_power_dbus_suspend (XfpmPower *power,
                         GDBusMethodInvocation *invocation,
                         gpointer user_data);

static gboolean
xfpm_power_dbus_hybrid_sleep (XfpmPower *power,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data);

static gboolean
xfpm_power_dbus_can_reboot (XfpmPower *power,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data);

static gboolean
xfpm_power_dbus_can_shutdown (XfpmPower *power,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data);

static gboolean
xfpm_power_dbus_can_hibernate (XfpmPower *power,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data);

static gboolean
xfpm_power_dbus_can_suspend (XfpmPower *power,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data);

static gboolean
xfpm_power_dbus_can_hybrid_sleep (XfpmPower *power,
                                  GDBusMethodInvocation *invocation,
                                  gpointer user_data);

static gboolean
xfpm_power_dbus_get_on_battery (XfpmPower *power,
                                GDBusMethodInvocation *invocation,
                                gpointer user_data);

static gboolean
xfpm_power_dbus_get_low_battery (XfpmPower *power,
                                 GDBusMethodInvocation *invocation,
                                 gpointer user_data);

#include "org.freedesktop.PowerManagement.h"

static void
xfpm_power_dbus_class_init (XfpmPowerClass *klass)
{
}

static void
xfpm_power_dbus_init (XfpmPower *power)
{
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  XfpmPowerManagement *power_dbus;

  power_dbus = xfpm_power_management_skeleton_new ();
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (power_dbus),
                                    bus,
                                    "/org/freedesktop/PowerManagement",
                                    NULL);

  g_signal_connect_object (power_dbus,
                           "handle-shutdown",
                           G_CALLBACK (xfpm_power_dbus_shutdown),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-reboot",
                           G_CALLBACK (xfpm_power_dbus_reboot),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-hibernate",
                           G_CALLBACK (xfpm_power_dbus_hibernate),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-suspend",
                           G_CALLBACK (xfpm_power_dbus_suspend),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-hybrid-sleep",
                           G_CALLBACK (xfpm_power_dbus_hybrid_sleep),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-can-reboot",
                           G_CALLBACK (xfpm_power_dbus_can_reboot),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-can-shutdown",
                           G_CALLBACK (xfpm_power_dbus_can_shutdown),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-can-hibernate",
                           G_CALLBACK (xfpm_power_dbus_can_hibernate),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-can-suspend",
                           G_CALLBACK (xfpm_power_dbus_can_suspend),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-can-hybrid-sleep",
                           G_CALLBACK (xfpm_power_dbus_can_hybrid_sleep),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-get-on-battery",
                           G_CALLBACK (xfpm_power_dbus_get_on_battery),
                           power, G_CONNECT_SWAPPED);
  g_signal_connect_object (power_dbus,
                           "handle-get-low-battery",
                           G_CALLBACK (xfpm_power_dbus_get_low_battery),
                           power, G_CONNECT_SWAPPED);
}

static gboolean
xfpm_power_dbus_shutdown (XfpmPower *power,
                          GDBusMethodInvocation *invocation,
                          gpointer user_data)
{
  GError *error = NULL;
  gboolean can_shutdown, auth_shutdown;

  if (power->priv->systemd != NULL)
  {
    xfce_systemd_can_power_off (power->priv->systemd, &can_shutdown, &auth_shutdown, NULL);
  }
  else
  {
    xfce_consolekit_can_power_off (power->priv->console, &can_shutdown, &auth_shutdown, NULL);
  }

  if (!can_shutdown || !auth_shutdown)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           XFPM_ERROR,
                                           XFPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if (power->priv->systemd != NULL)
    xfce_systemd_power_off (power->priv->systemd, TRUE, &error);
  else
    xfce_consolekit_power_off (power->priv->console, TRUE, &error);

  if (error)
  {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_error_free (error);
  }
  else
  {
    xfpm_power_management_complete_shutdown (user_data, invocation);
  }

  return TRUE;
}

static gboolean
xfpm_power_dbus_reboot (XfpmPower *power,
                        GDBusMethodInvocation *invocation,
                        gpointer user_data)
{
  GError *error = NULL;
  gboolean can_reboot, auth_reboot;

  if (power->priv->systemd != NULL)
  {
    xfce_systemd_can_reboot (power->priv->systemd, &can_reboot, &auth_reboot, NULL);
  }
  else
  {
    xfce_consolekit_can_reboot (power->priv->console, &can_reboot, &auth_reboot, NULL);
  }

  if (!can_reboot || !auth_reboot)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           XFPM_ERROR,
                                           XFPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if (power->priv->systemd != NULL)
    xfce_systemd_reboot (power->priv->systemd, TRUE, &error);
  else
    xfce_consolekit_reboot (power->priv->console, TRUE, &error);

  if (error)
  {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_error_free (error);
  }
  else
  {
    xfpm_power_management_complete_reboot (user_data, invocation);
  }

  return TRUE;
}

static gboolean
xfpm_power_dbus_hibernate (XfpmPower *power,
                           GDBusMethodInvocation *invocation,
                           gpointer user_data)
{
  gboolean can_hibernate, auth_hibernate;

  xfpm_power_can_hibernate (power, &can_hibernate, &auth_hibernate);

  if (!auth_hibernate)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           XFPM_ERROR,
                                           XFPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if (!can_hibernate)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           XFPM_ERROR,
                                           XFPM_ERROR_NO_HARDWARE_SUPPORT,
                                           _("Hibernate not supported"));
    return TRUE;
  }

  xfpm_power_sleep (power, "Hibernate", FALSE);

  xfpm_power_management_complete_hibernate (user_data, invocation);

  return TRUE;
}

static gboolean
xfpm_power_dbus_suspend (XfpmPower *power,
                         GDBusMethodInvocation *invocation,
                         gpointer user_data)
{
  gboolean can_suspend, auth_suspend;

  xfpm_power_can_suspend (power, &can_suspend, &auth_suspend);

  if (!auth_suspend)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           XFPM_ERROR,
                                           XFPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if (!can_suspend)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           XFPM_ERROR,
                                           XFPM_ERROR_NO_HARDWARE_SUPPORT,
                                           _("Suspend not supported"));
    return TRUE;
  }

  xfpm_power_sleep (power, "Suspend", FALSE);

  xfpm_power_management_complete_suspend (user_data, invocation);

  return TRUE;
}

static gboolean
xfpm_power_dbus_hybrid_sleep (XfpmPower *power,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data)
{
  gboolean can_hybrid_sleep, auth_hybrid_sleep;

  xfpm_power_can_hybrid_sleep (power, &can_hybrid_sleep, &auth_hybrid_sleep);

  if (!auth_hybrid_sleep)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           XFPM_ERROR,
                                           XFPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if (!can_hybrid_sleep)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           XFPM_ERROR,
                                           XFPM_ERROR_NO_HARDWARE_SUPPORT,
                                           _("Hybrid sleep not supported"));
    return TRUE;
  }

  xfpm_power_sleep (power, "HybridSleep", FALSE);

  xfpm_power_management_complete_hybrid_sleep (user_data, invocation);

  return TRUE;
}

static gboolean
xfpm_power_dbus_can_reboot (XfpmPower *power,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data)
{
  gboolean can_reboot;

  if (power->priv->systemd != NULL)
  {
    xfce_systemd_can_reboot (power->priv->systemd, &can_reboot, NULL, NULL);
  }
  else
  {
    xfce_consolekit_can_reboot (power->priv->console, &can_reboot, NULL, NULL);
  }

  xfpm_power_management_complete_can_reboot (user_data,
                                             invocation,
                                             can_reboot);

  return TRUE;
}

static gboolean
xfpm_power_dbus_can_shutdown (XfpmPower *power,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data)
{
  gboolean can_shutdown;

  if (power->priv->systemd != NULL)
  {
    xfce_systemd_can_power_off (power->priv->systemd, &can_shutdown, NULL, NULL);
  }
  else
  {
    xfce_consolekit_can_power_off (power->priv->console, &can_shutdown, NULL, NULL);
  }

  xfpm_power_management_complete_can_shutdown (user_data,
                                               invocation,
                                               can_shutdown);

  return TRUE;
}

static gboolean
xfpm_power_dbus_can_hibernate (XfpmPower *power,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data)
{
  gboolean can_hibernate;
  xfpm_power_can_hibernate (power, &can_hibernate, NULL);
  xfpm_power_management_complete_can_hibernate (user_data,
                                                invocation,
                                                can_hibernate);
  return TRUE;
}

static gboolean
xfpm_power_dbus_can_suspend (XfpmPower *power,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
  gboolean can_suspend;
  xfpm_power_can_suspend (power, &can_suspend, NULL);
  xfpm_power_management_complete_can_suspend (user_data,
                                              invocation,
                                              can_suspend);

  return TRUE;
}

static gboolean
xfpm_power_dbus_can_hybrid_sleep (XfpmPower *power,
                                  GDBusMethodInvocation *invocation,
                                  gpointer user_data)
{
  gboolean can_hybrid_sleep;
  xfpm_power_can_hybrid_sleep (power, &can_hybrid_sleep, NULL);
  xfpm_power_management_complete_can_hybrid_sleep (user_data,
                                                   invocation,
                                                   can_hybrid_sleep);

  return TRUE;
}

static gboolean
xfpm_power_dbus_get_on_battery (XfpmPower *power,
                                GDBusMethodInvocation *invocation,
                                gpointer user_data)
{
  xfpm_power_management_complete_get_on_battery (user_data,
                                                 invocation,
                                                 power->priv->on_battery);

  return TRUE;
}

static gboolean
xfpm_power_dbus_get_low_battery (XfpmPower *power,
                                 GDBusMethodInvocation *invocation,
                                 gpointer user_data)
{
  xfpm_power_management_complete_get_low_battery (user_data,
                                                  invocation,
                                                  power->priv->on_low_battery);

  return TRUE;
}

static void
xfpm_power_can_suspend (XfpmPower *power,
                        gboolean *can_suspend,
                        gboolean *auth_suspend)
{
  if (power->priv->systemd != NULL)
  {
    if (xfce_systemd_can_suspend (power->priv->systemd, can_suspend, auth_suspend, NULL))
      return;
  }
  else if (xfce_consolekit_can_suspend (power->priv->console, can_suspend, auth_suspend, NULL))
  {
    return;
  }

  if (can_suspend != NULL)
    *can_suspend = xfpm_suspend_can_suspend ();
  if (auth_suspend != NULL)
#ifdef ENABLE_POLKIT
    *auth_suspend = xfpm_polkit_check_auth (power->priv->polkit, POLKIT_AUTH_SUSPEND_XFPM);
#else
    *auth_suspend = TRUE;
#endif
}

static void
xfpm_power_can_hibernate (XfpmPower *power,
                          gboolean *can_hibernate,
                          gboolean *auth_hibernate)
{
  if (power->priv->systemd != NULL)
  {
    if (xfce_systemd_can_hibernate (power->priv->systemd, can_hibernate, auth_hibernate, NULL))
      return;
  }
  else if (xfce_consolekit_can_hibernate (power->priv->console, can_hibernate, auth_hibernate, NULL))
  {
    return;
  }

  if (can_hibernate != NULL)
    *can_hibernate = xfpm_suspend_can_hibernate ();
  if (auth_hibernate != NULL)
#ifdef ENABLE_POLKIT
    *auth_hibernate = xfpm_polkit_check_auth (power->priv->polkit, POLKIT_AUTH_HIBERNATE_XFPM);
#else
    *auth_hibernate = TRUE;
#endif
}

static void
xfpm_power_can_hybrid_sleep (XfpmPower *power,
                             gboolean *can_hybrid_sleep,
                             gboolean *auth_hybrid_sleep)
{
  if (power->priv->systemd != NULL)
  {
    if (xfce_systemd_can_hybrid_sleep (power->priv->systemd, can_hybrid_sleep, auth_hybrid_sleep, NULL))
      return;
  }
  else if (xfce_consolekit_can_hybrid_sleep (power->priv->console, can_hybrid_sleep, auth_hybrid_sleep, NULL))
  {
    return;
  }

  if (can_hybrid_sleep != NULL)
    *can_hybrid_sleep = xfpm_suspend_can_hybrid_sleep ();
  if (auth_hybrid_sleep != NULL)
#ifdef ENABLE_POLKIT
    *auth_hybrid_sleep = xfpm_polkit_check_auth (power->priv->polkit, POLKIT_AUTH_HYBRID_SLEEP_XFPM);
#else
    *auth_hybrid_sleep = TRUE;
#endif
}
