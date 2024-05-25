/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

#include "xfce-power-manager-dbus.h"
#include "xfpm-manager.h"

#include "common/xfpm-common.h"
#include "common/xfpm-debug.h"
#include "libdbus/xfpm-dbus.h"

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif

static void G_GNUC_NORETURN
show_version (void)
{
  g_print ("Xfce Power Manager %s\n", VERSION);
  exit (EXIT_SUCCESS);
}

static void
xfpm_quit_signal (gint sig,
                  gpointer data)
{
  XfpmManager *manager = (XfpmManager *) data;

  XFPM_DEBUG ("sig %d", sig);

  if (sig != SIGHUP)
    xfpm_manager_quit (manager);
}

static const gchar *
xfpm_bool_to_local_string (gboolean value)
{
  return value ? _("True") : _("False");
}

static void
xfpm_dump (GHashTable *hash)
{
  gboolean has_battery = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-battery"));
  gboolean auth_suspend = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-suspend"));
  gboolean auth_hibernate = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-hibernate"));
  gboolean auth_hybrid_sleep = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-hybrid-sleep"));
  gboolean can_suspend = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-suspend"));
  gboolean can_hibernate = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-hibernate"));
  gboolean can_hybrid_sleep = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-hybrid-sleep"));
  gboolean can_shutdown = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-shutdown"));
  gboolean has_lcd_brightness = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-brightness"));
  gboolean has_sleep_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "sleep-button"));
  gboolean has_hibernate_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "hibernate-button"));
  gboolean has_power_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "power-button"));
  gboolean has_battery_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "battery-button"));
  gboolean has_lid = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-lid"));

  g_print ("---------------------------------------------------\n");
  g_print ("       Xfce power manager version %s\n", VERSION);
#ifdef HAVE_POLKIT
  g_print (_("With policykit support\n"));
#else
  g_print (_("Without policykit support\n"));
#endif
  g_print ("---------------------------------------------------\n");
  /* see https://github.com/llvm/llvm-project/issues/55443#issuecomment-2077910853 */
  /* clang-format off */
  g_print ("%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n"
           "%s: %s\n",
           _("Can suspend"), xfpm_bool_to_local_string (can_suspend),
           _("Can hibernate"), xfpm_bool_to_local_string (can_hibernate),
           _("Can hybrid sleep"), xfpm_bool_to_local_string (can_hybrid_sleep),
           _("Authorized to suspend"), xfpm_bool_to_local_string (auth_suspend),
           _("Authorized to hibernate"), xfpm_bool_to_local_string (auth_hibernate),
           _("Authorized to hybrid sleep"), xfpm_bool_to_local_string (auth_hybrid_sleep),
           _("Authorized to shutdown"), xfpm_bool_to_local_string (can_shutdown),
           _("Has battery"), xfpm_bool_to_local_string (has_battery),
           _("Has brightness panel"), xfpm_bool_to_local_string (has_lcd_brightness),
           _("Has power button"), xfpm_bool_to_local_string (has_power_button),
           _("Has hibernate button"), xfpm_bool_to_local_string (has_hibernate_button),
           _("Has sleep button"), xfpm_bool_to_local_string (has_sleep_button),
           _("Has battery button"), xfpm_bool_to_local_string (has_battery_button),
           _("Has LID"), xfpm_bool_to_local_string (has_lid));
  /* clang-format on */
}

static void
xfpm_dump_remote (GDBusConnection *bus)
{
  GVariant *config;
  GVariantIter *iter;
  GHashTable *hash;
  gchar *key, *value;
  GError *error = NULL;
  XfpmPowerManager *proxy = xfpm_power_manager_proxy_new_sync (bus,
                                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                                                 | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                               "org.xfce.PowerManager",
                                                               "/org/xfce/PowerManager",
                                                               NULL,
                                                               &error);
  if (proxy == NULL)
  {
    g_critical ("%s", error->message);
    g_error_free (error);
    exit (EXIT_FAILURE);
  }
  else
  {
    xfpm_power_manager_call_get_config_sync (proxy, &config, NULL, &error);
    g_object_unref (proxy);
    if (error != NULL)
    {
      g_critical ("%s", error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }
  }

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_variant_get (config, "a{ss}", &iter);
  while (g_variant_iter_next (iter, "{ss}", &key, &value))
  {
    g_hash_table_insert (hash, key, value);
  }
  g_variant_iter_free (iter);
  g_variant_unref (config);

  xfpm_dump (hash);
  g_hash_table_destroy (hash);
}

static void G_GNUC_NORETURN
xfpm_start (GDBusConnection *bus,
            const gchar *client_id,
            gboolean dump)
{
  XfpmManager *manager;
  GError *error = NULL;

  XFPM_DEBUG ("Starting the power manager");

  manager = xfpm_manager_new (bus, client_id);
  if (manager == NULL)
    exit (EXIT_FAILURE);

  if (xfce_posix_signal_handler_init (&error))
  {
    xfce_posix_signal_handler_set_handler (SIGHUP,
                                           xfpm_quit_signal,
                                           manager, NULL);

    xfce_posix_signal_handler_set_handler (SIGINT,
                                           xfpm_quit_signal,
                                           manager, NULL);

    xfce_posix_signal_handler_set_handler (SIGTERM,
                                           xfpm_quit_signal,
                                           manager, NULL);
  }
  else if (error != NULL)
  {
    g_warning ("Unable to set up POSIX signal handlers: %s", error->message);
    g_error_free (error);
  }

  if (dump)
  {
    GHashTable *hash;
    hash = xfpm_manager_get_config (manager);
    xfpm_dump (hash);
    g_hash_table_destroy (hash);
  }


  gtk_main ();

  g_object_unref (manager);

  exit (EXIT_SUCCESS);
}

int
main (int argc,
      char **argv)
{
  GDBusConnection *bus;
  GError *error = NULL;
  XfpmPowerManager *proxy;
  GOptionContext *octx;

  gboolean run = FALSE;
  gboolean quit = FALSE;
  gboolean config = FALSE;
  gboolean version = FALSE;
  gboolean reload = FALSE;
  gboolean daemonize = FALSE;
  gboolean debug = FALSE;
  gboolean dump = FALSE;
  gchar *client_id = NULL;

  GOptionEntry option_entries[] = {
    { "run", 'r', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &run, NULL, NULL },
    { "daemon", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &daemonize, N_ ("Daemonize"), NULL },
    { "debug", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &debug, N_ ("Enable debugging"), NULL },
    { "dump", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &dump, N_ ("Dump all information"), NULL },
    { "restart", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &reload, N_ ("Restart the running instance of Xfce power manager"), NULL },
    { "customize", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &config, N_ ("Show the configuration dialog"), NULL },
    { "quit", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &quit, N_ ("Quit any running xfce power manager"), NULL },
    { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version, N_ ("Version information"), NULL },
    { "sm-client-id", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &client_id, NULL, NULL },
    { NULL },
  };

  /* Parse the options */
  octx = g_option_context_new ("");
  g_option_context_set_ignore_unknown_options (octx, TRUE);
  g_option_context_add_main_entries (octx, option_entries, NULL);
#ifdef ENABLE_X11
  g_option_context_add_group (octx, xfce_sm_client_get_option_group (argc, argv));
#endif
  /* We can't add the following command because it will invoke gtk_init
     before we have a chance to fork.
     g_option_context_add_group(octx, gtk_get_option_group(TRUE));
   */

  if (!g_option_context_parse (octx, &argc, &argv, &error))
  {
    g_printerr (_("Failed to parse arguments: %s\n"), error->message);
    g_error_free (error);
    g_option_context_free (octx);

    return EXIT_FAILURE;
  }

  g_option_context_free (octx);

  if (version)
    show_version ();

  /* Fork if needed */
  if (!dump && !debug && daemonize && daemon (0, 0))
  {
    g_critical ("Could not daemonize");
  }

  /* Initialize */
  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  g_set_application_name (PACKAGE_NAME);

  if (!gtk_init_check (&argc, &argv))
  {
    g_critical ("Unable to open display.");
    return EXIT_FAILURE;
  }

  xfpm_debug_init (debug);

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (bus == NULL)
  {
    xfce_dialog_show_error (NULL, error, "%s", _("Unable to get connection to the message bus session"));
    g_critical ("%s", error->message);
    return EXIT_FAILURE;
  }

  if (quit)
  {
    if (!xfpm_dbus_name_has_owner (bus, "org.xfce.PowerManager"))
    {
      g_print (_("Xfce power manager is not running"));
      g_print ("\n");
      return EXIT_SUCCESS;
    }
    else
    {
      proxy = xfpm_power_manager_proxy_new_sync (bus,
                                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                                   | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                 "org.xfce.PowerManager",
                                                 "/org/xfce/PowerManager",
                                                 NULL,
                                                 NULL);
      if (proxy == NULL)
      {
        g_critical ("Failed to get proxy");
        g_object_unref (bus);
        return EXIT_FAILURE;
      }

      xfpm_power_manager_call_quit_sync (proxy, NULL, &error);
      g_object_unref (proxy);
      if (error != NULL)
      {
        g_critical ("Failed to send quit message: %s", error->message);
        g_error_free (error);
        return EXIT_FAILURE;
      }
    }

    return EXIT_SUCCESS;
  }

  if (config)
  {
    g_spawn_command_line_async ("xfce4-power-manager-settings", &error);

    if (error != NULL)
    {
      g_critical ("Failed to execute xfce4-power-manager-settings: %s", error->message);
      g_error_free (error);
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  if (reload)
  {
    if (!xfpm_dbus_name_has_owner (bus, "org.xfce.PowerManager")
        && !xfpm_dbus_name_has_owner (bus, "org.freedesktop.PowerManagement"))
    {
      g_print (_("Xfce power manager is not running"));
      g_print ("\n");
      xfpm_start (bus, client_id, dump);
    }

    proxy = xfpm_power_manager_proxy_new_sync (bus,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES
                                                 | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                               "org.xfce.PowerManager",
                                               "/org/xfce/PowerManager",
                                               NULL,
                                               &error);
    if (proxy == NULL)
    {
      g_critical ("Failed to get proxy: %s", error->message);
      g_error_free (error);
      g_object_unref (bus);
      return EXIT_FAILURE;
    }

    if (!xfpm_power_manager_call_restart_sync (proxy, NULL, NULL))
    {
      g_critical ("Unable to send reload message");
      g_object_unref (proxy);
      g_object_unref (bus);
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  if (dump && xfpm_dbus_name_has_owner (bus, "org.xfce.PowerManager"))
  {
    xfpm_dump_remote (bus);
    return EXIT_SUCCESS;
  }

  if (xfpm_dbus_name_has_owner (bus, "org.freedesktop.PowerManagement"))
  {
    g_print ("%s: %s\n", _("Xfce Power Manager"), _("Another power manager is already running"));
  }
  else if (xfpm_dbus_name_has_owner (bus, "org.xfce.PowerManager"))
  {
    g_print (_("Xfce power manager is already running"));
    g_print ("\n");
  }
  else
  {
    xfpm_start (bus, client_id, dump);
  }

  return EXIT_SUCCESS;
}
