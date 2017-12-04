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
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <glib.h>
#include <gio/gio.h>

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfpm-settings-app.h"
#include "xfpm-settings.h"
#include "xfpm-debug.h"
#include "xfpm-config.h"
#include "xfpm-common.h"


struct _XfpmSettingsAppPrivate
{
    gboolean          debug;
    Window            socket_id;
    gchar            *device_id;
};

static void xfpm_settings_app_launch     (GApplication *app);

static void activate_socket              (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);
static void activate_device              (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);
static void activate_debug               (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);
static void activate_window              (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);
static void activate_quit                (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);

G_DEFINE_TYPE(XfpmSettingsApp, xfpm_settings_app, GTK_TYPE_APPLICATION);


#define XFPM_SETTINGS_APP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_SETTINGS_APP, XfpmSettingsAppPrivate))


static void
xfpm_settings_app_init (XfpmSettingsApp *app)
{
    const GOptionEntry option_entries[] = {
      { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,    NULL, N_("Settings manager socket"), N_("SOCKET ID") },
      { "device-id", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, NULL, N_("Display a specific device by UpDevice object path"), N_("UpDevice object path") },
      { "debug",    '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   NULL, N_("Enable debugging"), NULL },
      { "version",   'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   NULL, N_("Display version information"), NULL },
      { "quit",      'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   NULL, N_("Cause xfce4-power-manager-settings to quit"), NULL },
      { NULL, },
    };

    g_application_add_main_option_entries (G_APPLICATION (app), option_entries);
}

static void
xfpm_settings_app_startup (GApplication *app)
{
    const GActionEntry action_entries[] = {
      { "socket-id", activate_socket, "i"  },
      { "device-id", activate_device, "s"  },
      { "debug",     activate_debug,  NULL },
      { "activate",  activate_window, NULL },
      { "quit",      activate_quit,   NULL },
    };

    TRACE ("entering");

    g_action_map_add_action_entries (G_ACTION_MAP (app),
                                     action_entries,
                                     G_N_ELEMENTS (action_entries),
                                     app);

    /* keep the app running until we've launched our window */
    g_application_hold (app);

    /* let the parent class do it's startup as well */
    G_APPLICATION_CLASS(xfpm_settings_app_parent_class)->startup(app);
}

static void
xfpm_settings_app_activate (GApplication *app)
{
    TRACE ("entering");
}

static void
xfpm_settings_app_launch (GApplication *app)
{
    XfpmSettingsAppPrivate *priv = XFPM_SETTINGS_APP_GET_PRIVATE (app);

    XfpmPowerManager *manager;
    XfconfChannel    *channel;
    GError           *error = NULL;
    GtkWidget        *dialog;
    GHashTable       *hash;
    GVariant         *config;
    GVariantIter     *iter;
    gchar            *key, *value;
    GList            *windows;

    gboolean has_battery;
    gboolean auth_suspend;
    gboolean auth_hibernate;
    gboolean can_suspend;
    gboolean can_hibernate;
    gboolean can_shutdown;
    gboolean has_lcd_brightness;
    gboolean has_sleep_button;
    gboolean has_hibernate_button;
    gboolean has_power_button;
    gboolean has_battery_button;
    gboolean has_lid;
    gint     start_xfpm_if_not_running;

    TRACE ("entering");

    windows = gtk_application_get_windows (GTK_APPLICATION (app));

    if (windows != NULL)
    {
        XFPM_DEBUG ("window already opened");

        gdk_notify_startup_complete ();

        if (priv->device_id != NULL)
        {
            xfpm_settings_show_device_id (priv->device_id);
        }

        return;
    }

    manager = xfpm_power_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_BUS_NAME_OWNER_FLAGS_NONE,
                                                         "org.xfce.PowerManager",
                                                         "/org/xfce/PowerManager",
                                                         NULL,
                                                         &error);

    if (error != NULL)
    {
        g_critical("xfpm_power_manager_proxy_new_sync failed: %s\n", error->message);
        xfce_dialog_show_warning (NULL,
                                 _("Xfce Power Manager"),
                                 "%s",
                                 _("Failed to connect to power manager"));
        g_clear_error (&error);
        return;
    }


    while ( !xfpm_power_manager_call_get_config_sync (manager, &config, NULL, NULL) )
    {
        GtkWidget *startw;

        startw = gtk_message_dialog_new (NULL,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_YES_NO,
                                         _("Xfce4 Power Manager is not running, do you want to launch it now?"));
        start_xfpm_if_not_running = gtk_dialog_run (GTK_DIALOG (startw));
        gtk_widget_destroy (startw);

        if (start_xfpm_if_not_running == GTK_RESPONSE_YES)
        {
            GAppInfo *app_info;
            GError   *error = NULL;

            app_info = g_app_info_create_from_commandline ("xfce4-power-manager", "Xfce4 Power Manager",
                                                           G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION, NULL);
            if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
                if (error != NULL) {
                  g_warning ("xfce4-power-manager could not be launched. %s", error->message);
                  g_error_free (error);
                }
            }
            /* wait 2 seconds for xfpm to startup */
            g_usleep ( 2 * 1000000 );
        }
        else
        {
            /* exit without starting xfpm */
            return;
        }
    }

    if ( !xfconf_init(&error) )
    {
        g_critical("xfconf init failed: %s using default settings\n", error->message);
        xfce_dialog_show_warning (NULL,
                                  _("Xfce Power Manager"),
                                  "%s",
                                  _("Failed to load power manager configuration, using defaults"));
        g_clear_error (&error);
    }


    channel = xfconf_channel_new(XFPM_CHANNEL_CFG);

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
    auth_suspend = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-suspend"));
    auth_hibernate = xfpm_string_to_bool (g_hash_table_lookup (hash, "auth-hibernate"));
    has_lcd_brightness = xfpm_string_to_bool (g_hash_table_lookup (hash, "has-brightness"));
    has_sleep_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "sleep-button"));
    has_power_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "power-button"));
    has_hibernate_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "hibernate-button"));
    has_battery_button = xfpm_string_to_bool (g_hash_table_lookup (hash, "battery-button"));
    can_shutdown = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-shutdown"));

    DBG("socket_id %i", (int)priv->socket_id);
    DBG("device id %s", priv->device_id);

    dialog = xfpm_settings_dialog_new (channel, auth_suspend, auth_hibernate,
                                       can_suspend, can_hibernate, can_shutdown, has_battery, has_lcd_brightness,
                                       has_lid, has_sleep_button, has_hibernate_button, has_power_button, has_battery_button,
                                       priv->socket_id, priv->device_id, GTK_APPLICATION (app));

    g_hash_table_destroy (hash);

    gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (dialog));
    g_application_release (app);

    g_object_unref (manager);
}

static void
activate_socket (GSimpleAction  *action,
                 GVariant       *parameter,
                 gpointer        data)
{
    XfpmSettingsApp *app = XFPM_SETTINGS_APP (data);
    XfpmSettingsAppPrivate *priv = XFPM_SETTINGS_APP_GET_PRIVATE (app);

    TRACE ("entering");

    priv->socket_id = g_variant_get_int32 (parameter);

    xfpm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_device (GSimpleAction  *action,
                 GVariant       *parameter,
                 gpointer        data)
{
    XfpmSettingsApp *app = XFPM_SETTINGS_APP (data);
    XfpmSettingsAppPrivate *priv = XFPM_SETTINGS_APP_GET_PRIVATE (app);

    TRACE ("entering");

    priv->device_id = g_strdup(g_variant_get_string (parameter, NULL));

    xfpm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_debug (GSimpleAction  *action,
                GVariant       *parameter,
                gpointer        data)
{
    XfpmSettingsApp *app = XFPM_SETTINGS_APP (data);
    XfpmSettingsAppPrivate *priv = XFPM_SETTINGS_APP_GET_PRIVATE (app);

    TRACE ("entering");

    priv->debug = TRUE;

    xfpm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_window (GSimpleAction  *action,
                 GVariant       *parameter,
                 gpointer        data)
{
    XfpmSettingsApp *app = XFPM_SETTINGS_APP (data);

    TRACE ("entering");

    xfpm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_quit (GSimpleAction  *action,
               GVariant       *parameter,
               gpointer        data)
{
    GtkApplication *app = GTK_APPLICATION (data);
    GList *windows;

    TRACE ("entering");

    windows = gtk_application_get_windows (app);

    if (windows)
    {
        /* Remove our window if we've attahced one */
        gtk_application_remove_window (app, GTK_WINDOW (windows->data));
    }
}

static gboolean
xfpm_settings_app_local_options (GApplication *g_application,
                                 GVariantDict *options)
{
    TRACE ("entering");

    /* --version */
    if (g_variant_dict_contains (options, "version"))
    {
        g_print(_("This is %s version %s, running on Xfce %s.\n"), PACKAGE,
                VERSION, xfce_version_string());
        g_print(_("Built with GTK+ %d.%d.%d, linked with GTK+ %d.%d.%d."),
                GTK_MAJOR_VERSION,GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                gtk_major_version, gtk_minor_version, gtk_micro_version);
        g_print("\n");

        return 0;
    }

    /* This will call xfpm_settings_app_startup if it needs to */
    g_application_register (g_application, NULL, NULL);

    /* --debug */
    if (g_variant_dict_contains (options, "debug"))
    {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "debug", NULL);
        return 0;
    }

    /* --socket-id */
    if (g_variant_dict_contains (options, "socket-id") || g_variant_dict_contains (options, "s"))
    {
        GVariant *var;

        var = g_variant_dict_lookup_value (options, "socket-id", G_VARIANT_TYPE_INT32);

        g_action_group_activate_action(G_ACTION_GROUP(g_application), "socket-id", var);
        return 0;
    }

    /* --device-id */
    if (g_variant_dict_contains (options, "device-id") || g_variant_dict_contains (options, "d"))
    {
        GVariant *var;

        var = g_variant_dict_lookup_value (options, "device-id", G_VARIANT_TYPE_STRING);

        g_action_group_activate_action(G_ACTION_GROUP(g_application), "device-id", var);
        return 0;
    }

    /* --quit */
    if (g_variant_dict_contains (options, "quit") || g_variant_dict_contains (options, "q"))
    {
        g_action_group_activate_action(G_ACTION_GROUP(g_application), "quit", NULL);
        return 0;
    }

    /* default action */
    g_action_group_activate_action(G_ACTION_GROUP(g_application), "activate", NULL);

    return 0;
}

static void
xfpm_settings_app_class_init (XfpmSettingsAppClass *class)
{
    GApplicationClass *gapplication_class = G_APPLICATION_CLASS (class);

    gapplication_class->handle_local_options = xfpm_settings_app_local_options;
    gapplication_class->startup              = xfpm_settings_app_startup;
    gapplication_class->activate             = xfpm_settings_app_activate;

    g_type_class_add_private (class, sizeof (XfpmSettingsAppPrivate));
}

XfpmSettingsApp *
xfpm_settings_app_new (void)
{
    return g_object_new (XFPM_TYPE_SETTINGS_APP,
                         "application-id", "org.xfce.PowerManager.Settings",
                         "flags", G_APPLICATION_FLAGS_NONE,
                         NULL);
}
