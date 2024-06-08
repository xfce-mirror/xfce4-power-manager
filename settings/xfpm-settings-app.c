/* -*- c-basic-offset: 4 -*- vi:set ts=4 sts=4 sw=4:
 * * Copyright (C) 2015 Xfce Development Team <xfce4-dev@xfce.org>
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
#include "config.h"
#endif

#include "xfpm-settings-app.h"
#include "xfpm-settings.h"

#include "common/xfpm-common.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <gtk/gtkx.h>
#endif


struct _XfpmSettingsAppPrivate
{
  gboolean debug;
#ifdef ENABLE_X11
  Window socket_id;
#endif
  gchar *device_id;
  guint abort_id;
  XfpmPowerManager *manager;
};

static void
xfpm_settings_app_launch (GApplication *app);

static void
activate_socket (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer data);
static void
activate_device (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer data);
static void
activate_debug (GSimpleAction *action,
                GVariant *parameter,
                gpointer data);
static void
activate_window (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer data);
static void
activate_quit (GSimpleAction *action,
               GVariant *parameter,
               gpointer data);

G_DEFINE_TYPE_WITH_PRIVATE (XfpmSettingsApp, xfpm_settings_app, GTK_TYPE_APPLICATION);



static void
xfpm_settings_app_init (XfpmSettingsApp *app)
{
  const GOptionEntry option_entries[] = {
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, NULL, N_ ("Settings manager socket"), N_ ("SOCKET ID") },
    { "device-id", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, NULL, N_ ("Display a specific device by UpDevice object path"), N_ ("UpDevice object path") },
    { "debug", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_ ("Enable debugging"), NULL },
    { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_ ("Display version information"), NULL },
    { "quit", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, NULL, N_ ("Cause xfce4-power-manager-settings to quit"), NULL },
    { NULL },
  };

  g_application_add_main_option_entries (G_APPLICATION (app), option_entries);
}

static void
xfpm_settings_app_startup (GApplication *app)
{
  const GActionEntry action_entries[] = {
    { "socket-id", activate_socket, "i" },
    { "device-id", activate_device, "s" },
    { "debug", activate_debug, NULL },
    { "activate", activate_window, NULL },
    { "quit", activate_quit, NULL },
  };

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   app);

  /* let the parent class do it's startup as well */
  G_APPLICATION_CLASS (xfpm_settings_app_parent_class)->startup (app);
}

static void
xfpm_settings_app_activate (GApplication *app)
{
}

static void
xfpm_settings_app_shutdown (GApplication *app)
{
  XfpmSettingsAppPrivate *priv = xfpm_settings_app_get_instance_private (XFPM_SETTINGS_APP (app));

  if (priv->abort_id != 0)
    g_source_remove (priv->abort_id);
  if (priv->manager != NULL)
    g_object_unref (priv->manager);

  G_APPLICATION_CLASS (xfpm_settings_app_parent_class)->shutdown (app);
}

static gboolean
abort_xfpm_startup (gpointer data)
{
  XfpmSettingsApp *app = data;
  XfpmSettingsAppPrivate *priv = xfpm_settings_app_get_instance_private (app);

  g_critical ("Failed to start xfce4-power-manager, timeout reached");
  xfce_dialog_show_warning (NULL, _("Xfce Power Manager"),
                            "%s", _("Failed to connect to power manager"));
  g_application_release (G_APPLICATION (app));
  priv->abort_id = 0;

  return FALSE;
}

static void
get_config (XfpmPowerManager *manager,
            GParamSpec *pspec,
            XfpmSettingsApp *app)
{
  XfpmSettingsAppPrivate *priv = xfpm_settings_app_get_instance_private (app);
  XfconfChannel *channel;
  GError *error = NULL;
  GtkWidget *dialog;
  GHashTable *hash;
  GVariant *config;
  GVariantIter *iter;
  gchar *key, *value;
  gboolean has_battery;
  gboolean auth_suspend;
  gboolean auth_hibernate;
  gboolean auth_hybrid_sleep;
  gboolean can_suspend;
  gboolean can_hibernate;
  gboolean can_hybrid_sleep;
  gboolean can_shutdown;
  gboolean has_lcd_brightness;
  gboolean has_sleep_button;
  gboolean has_hibernate_button;
  gboolean has_power_button;
  gboolean has_battery_button;
  gboolean has_lid;

  g_signal_handlers_disconnect_by_func (manager, get_config, app);
  g_application_release (G_APPLICATION (app));
  if (priv->abort_id != 0)
  {
    g_source_remove (priv->abort_id);
    priv->abort_id = 0;
  }

  if (!xfpm_power_manager_call_get_config_sync (manager, &config, NULL, &error))
  {
    g_critical ("xfpm_power_manager_call_get_config_sync failed: %s", error->message);
    xfce_dialog_show_warning (NULL, _("Xfce Power Manager"),
                              "%s", _("Failed to connect to power manager"));
    g_error_free (error);
    return;
  }

  channel = xfconf_channel_new (XFPM_CHANNEL);
  xfpm_debug_init (priv->debug);

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_variant_get (config, "a{ss}", &iter);
  while (g_variant_iter_next (iter, "{ss}", &key, &value))
  {
    g_hash_table_insert (hash, key, value);
  }
  g_variant_iter_free (iter);
  g_variant_unref (config);

  has_battery = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-battery"));
  has_lid = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-lid"));
  can_suspend = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-suspend"));
  can_hibernate = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-hibernate"));
  can_hybrid_sleep = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-hybrid-sleep"));
  auth_suspend = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-suspend"));
  auth_hibernate = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-hibernate"));
  auth_hybrid_sleep = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-hybrid-sleep"));
  has_lcd_brightness = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-brightness"));
  has_sleep_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "sleep-button"));
  has_power_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "power-button"));
  has_hibernate_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "hibernate-button"));
  has_battery_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "battery-button"));
  can_shutdown = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-shutdown"));

#ifdef ENABLE_X11
  XFPM_DEBUG ("socket_id %i", (int) priv->socket_id);
#endif
  XFPM_DEBUG ("device id %s", priv->device_id);

  dialog = xfpm_settings_dialog_new (channel, auth_suspend, auth_hibernate, auth_hybrid_sleep,
                                     can_suspend, can_hibernate, can_hybrid_sleep, can_shutdown, has_battery, has_lcd_brightness,
                                     has_lid, has_sleep_button, has_hibernate_button, has_power_button, has_battery_button,
#ifdef ENABLE_X11
                                     priv->socket_id,
#endif
                                     priv->device_id, GTK_APPLICATION (app));

  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (dialog));
  g_hash_table_destroy (hash);
}

static void
xfpm_settings_app_launch (GApplication *app)
{
  XfpmSettingsAppPrivate *priv = xfpm_settings_app_get_instance_private (XFPM_SETTINGS_APP (app));
  XfpmPowerManager *manager;
  GError *error = NULL;
  GList *windows = gtk_application_get_windows (GTK_APPLICATION (app));
  gchar *owner;

  if (windows != NULL)
  {
    XFPM_DEBUG ("window already opened");

    gtk_window_present (windows->data);
    gdk_notify_startup_complete ();

    if (priv->device_id != NULL)
    {
      xfpm_settings_show_device_id (priv->device_id);
    }

    return;
  }

  if (!xfconf_init (&error))
  {
    g_critical ("xfconf init failed: %s", error->message);
    xfce_dialog_show_warning (NULL, _("Xfce Power Manager"),
                              "%s", _("Failed to load power manager configuration"));
    g_error_free (error);
    return;
  }

  manager = xfpm_power_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       "org.xfce.PowerManager",
                                                       "/org/xfce/PowerManager",
                                                       NULL,
                                                       &error);
  if (error != NULL)
  {
    g_critical ("xfpm_power_manager_proxy_new_sync failed: %s", error->message);
    xfce_dialog_show_warning (NULL, _("Xfce Power Manager"),
                              "%s", _("Failed to connect to power manager"));
    g_error_free (error);
    return;
  }

  owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (manager));
  if (owner != NULL)
  {
    g_application_hold (G_APPLICATION (app));
    get_config (manager, NULL, XFPM_SETTINGS_APP (app));
    g_free (owner);
  }
  else
  {
    GtkWidget *dialog = gtk_message_dialog_new (
      NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
      _("Xfce4 Power Manager is not running, do you want to launch it now?"));
    g_signal_connect_after (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES)
    {
      g_signal_connect (manager, "notify::g-name-owner", G_CALLBACK (get_config), app);
      g_application_hold (G_APPLICATION (app));

      if (!g_spawn_command_line_async ("xfce4-power-manager", &error))
      {
        g_critical ("Failed to start xfce4-power-manager: %s", error->message);
        xfce_dialog_show_warning (NULL, _("Xfce Power Manager"),
                                  "%s", _("Failed to connect to power manager"));
        g_application_release (G_APPLICATION (app));
        g_error_free (error);
      }
      else
      {
        priv->abort_id = g_timeout_add_seconds (5, abort_xfpm_startup, app);
      }
    }
  }
}

static void
activate_socket (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer data)
{
  XfpmSettingsApp *app = XFPM_SETTINGS_APP (data);
#ifdef ENABLE_X11
  XfpmSettingsAppPrivate *priv = xfpm_settings_app_get_instance_private (app);
  priv->socket_id = g_variant_get_int32 (parameter);
#endif

  xfpm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_device (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer data)
{
  XfpmSettingsApp *app = XFPM_SETTINGS_APP (data);
  XfpmSettingsAppPrivate *priv = xfpm_settings_app_get_instance_private (app);

  priv->device_id = g_strdup (g_variant_get_string (parameter, NULL));

  xfpm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_debug (GSimpleAction *action,
                GVariant *parameter,
                gpointer data)
{
  XfpmSettingsApp *app = XFPM_SETTINGS_APP (data);
  XfpmSettingsAppPrivate *priv = xfpm_settings_app_get_instance_private (app);

  priv->debug = TRUE;

  xfpm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_window (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer data)
{
  XfpmSettingsApp *app = XFPM_SETTINGS_APP (data);

  xfpm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_quit (GSimpleAction *action,
               GVariant *parameter,
               gpointer data)
{
  GtkApplication *app = GTK_APPLICATION (data);
  GList *windows;

  windows = gtk_application_get_windows (app);

  if (windows)
  {
    /* Remove our window if we've attached one */
    gtk_application_remove_window (app, GTK_WINDOW (windows->data));
  }
}

static gboolean
xfpm_settings_app_local_options (GApplication *g_application,
                                 GVariantDict *options)
{
  /* --version */
  if (g_variant_dict_contains (options, "version"))
  {
    g_print (_("This is %s version %s, running on Xfce %s.\n"),
             PACKAGE, VERSION, xfce_version_string ());
    g_print (_("Built with GTK+ %d.%d.%d, linked with GTK+ %d.%d.%d."),
             GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
             gtk_major_version, gtk_minor_version, gtk_micro_version);
    g_print ("\n");

    return 0;
  }

  /* embed settings instead of presenting existing window, do this before registering */
  if (g_variant_dict_contains (options, "socket-id") || g_variant_dict_contains (options, "s"))
    g_application_set_flags (g_application, G_APPLICATION_REPLACE);

  /* This will call xfpm_settings_app_startup if it needs to */
  g_application_register (g_application, NULL, NULL);

  /* --debug */
  if (g_variant_dict_contains (options, "debug"))
  {
    g_action_group_activate_action (G_ACTION_GROUP (g_application), "debug", NULL);
    return 0;
  }

  /* --socket-id */
  if (g_variant_dict_contains (options, "socket-id") || g_variant_dict_contains (options, "s"))
  {
    GVariant *var;

    var = g_variant_dict_lookup_value (options, "socket-id", G_VARIANT_TYPE_INT32);

    g_action_group_activate_action (G_ACTION_GROUP (g_application), "socket-id", var);
    return 0;
  }

  /* --device-id */
  if (g_variant_dict_contains (options, "device-id") || g_variant_dict_contains (options, "d"))
  {
    GVariant *var;

    var = g_variant_dict_lookup_value (options, "device-id", G_VARIANT_TYPE_STRING);

    g_action_group_activate_action (G_ACTION_GROUP (g_application), "device-id", var);
    return 0;
  }

  /* --quit */
  if (g_variant_dict_contains (options, "quit") || g_variant_dict_contains (options, "q"))
  {
    g_action_group_activate_action (G_ACTION_GROUP (g_application), "quit", NULL);
    return 0;
  }

  /* default action */
  g_action_group_activate_action (G_ACTION_GROUP (g_application), "activate", NULL);

  return 0;
}

static void
xfpm_settings_app_class_init (XfpmSettingsAppClass *class)
{
  GApplicationClass *gapplication_class = G_APPLICATION_CLASS (class);

  gapplication_class->handle_local_options = xfpm_settings_app_local_options;
  gapplication_class->startup = xfpm_settings_app_startup;
  gapplication_class->activate = xfpm_settings_app_activate;
  gapplication_class->shutdown = xfpm_settings_app_shutdown;
}

XfpmSettingsApp *
xfpm_settings_app_new (void)
{
  return g_object_new (XFPM_TYPE_SETTINGS_APP,
                       "application-id", "org.xfce.PowerManager.Settings",
                       "flags", G_APPLICATION_ALLOW_REPLACEMENT,
                       NULL);
}
