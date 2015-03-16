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


struct _XfpmSettingsAppPrivate
{
    XfpmPowerManager *manager;
    XfconfChannel    *channel;
    gboolean          debug;
    Window            socket_id;
    gchar            *device_id;
};

static void xfpm_settings_app_class_init (XfpmSettingsAppClass *class);
static void xfpm_settings_app_init       (XfpmSettingsApp *app);
static void xfpm_settings_app_activate   (GApplication *app);


G_DEFINE_TYPE(XfpmSettingsApp, xfpm_settings_app, GTK_TYPE_APPLICATION);


#define XFPM_SETTINGS_APP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_SETTINGS_APP, XfpmSettingsAppPrivate))


static void
xfpm_settings_app_init (XfpmSettingsApp *app)
{
    XfpmSettingsAppPrivate *priv = XFPM_SETTINGS_APP_GET_PRIVATE (app);

    const GOptionEntry option_entries[] = {
      { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &priv->socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
      { "device-id", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &priv->device_id, N_("Display a specific device by UpDevice object path"), N_("UpDevice object path") },
      { "debug",    '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &priv->debug, N_("Enable debugging"), NULL },
      { NULL, },
    };

    g_application_add_main_option_entries (G_APPLICATION (app), option_entries);
}

static void
xfpm_settings_app_activate (GApplication *app)
{
    XfpmSettingsAppPrivate *priv = XFPM_SETTINGS_APP_GET_PRIVATE (app);

    GError           *error = NULL;
    GtkWidget        *dialog;
    GHashTable       *hash;
    GVariant         *config;
    GVariantIter     *iter;
    gchar            *key, *value;

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
    gboolean has_lid;
    gboolean start_xfpm_if_not_running;

    priv->manager = xfpm_power_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
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


    while ( !xfpm_power_manager_call_get_config_sync (priv->manager, &config, NULL, NULL) )
    {
        GtkWidget *startw;

        startw = gtk_message_dialog_new (NULL,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_YES_NO,
                                         _("Xfce4 Power Manager is not running, do you want to launch it now?"));
        start_xfpm_if_not_running = gtk_dialog_run (GTK_DIALOG (startw));
        gtk_widget_destroy (startw);

        if ( start_xfpm_if_not_running ) 
        {
            g_spawn_command_line_async("xfce4-power-manager",NULL);
            /* wait 2 seconds for xfpm to startup */
            g_usleep ( 2 * 1000000 );
        }
        else
        {
            /* continue without starting xfpm, this will probably error out */
            break;
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


    priv->channel = xfconf_channel_new(XFPM_CHANNEL_CFG);

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
    can_shutdown = xfpm_string_to_bool (g_hash_table_lookup (hash, "can-shutdown"));

    dialog = xfpm_settings_dialog_new (priv->channel, auth_suspend, auth_hibernate,
                                       can_suspend, can_hibernate, can_shutdown, has_battery, has_lcd_brightness,
                                       has_lid, has_sleep_button, has_hibernate_button, has_power_button,
                                       priv->socket_id, priv->device_id);

    g_hash_table_destroy (hash);

    gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (dialog));
}

static void
xfpm_settings_app_class_init (XfpmSettingsAppClass *class)
{
    G_APPLICATION_CLASS (class)->activate = xfpm_settings_app_activate;

    g_type_class_add_private (class, sizeof (XfpmSettingsAppPrivate));
}

XfpmSettingsApp *
xfpm_settings_app_new ()
{
    return g_object_new (XFPM_TYPE_SETTINGS_APP,
                         "application-id", "org.xfce.PowerManager.Settings",
                         "flags", G_APPLICATION_FLAGS_NONE,
                         NULL);
}
