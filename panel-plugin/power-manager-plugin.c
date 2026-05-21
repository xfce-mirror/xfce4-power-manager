/*
 * * Copyright (C) 2014 Eric Koegel <eric@xfce.org>
 * * Copyright (C) 2019 Kacper Piwiński
 * * Copyright (C) 2024 Andrzej Radecki <andrzejr@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "power-manager-button.h"
#include "power-manager-config.h"
#include "power-manager-dialog.h"
#include "power-manager-plugin.h"

#include "common/xfpm-brightness.h"
#include "common/xfpm-common.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-enum-glib.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>



static void
power_manager_plugin_construct (XfcePanelPlugin *panel_plugin);
static void
power_manager_plugin_free_data (XfcePanelPlugin *panel_plugin);
static gboolean
power_manager_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                                   gint size);
static void
power_manager_plugin_mode_changed (XfcePanelPlugin *panel_plugin,
                                   XfcePanelPluginMode mode);
static void
power_manager_plugin_style_updated (GtkWidget *widget);
static void
power_manager_plugin_configure (XfcePanelPlugin *panel_plugin);
static void
power_manager_plugin_about (XfcePanelPlugin *panel_plugin);



struct _PowerManagerPlugin
{
  XfcePanelPlugin __parent__;

  PowerManagerButton *button;
  PowerManagerDialog *dialog;
  PowerManagerConfig *config;
  GDBusProxy *proxy;
};



XFCE_PANEL_DEFINE_PLUGIN (PowerManagerPlugin, power_manager_plugin)



static void
power_manager_plugin_class_init (PowerManagerPluginClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  XfcePanelPluginClass *plugin_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->style_updated = power_manager_plugin_style_updated;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = power_manager_plugin_construct;
  plugin_class->free_data = power_manager_plugin_free_data;
  plugin_class->size_changed = power_manager_plugin_size_changed;
  plugin_class->mode_changed = power_manager_plugin_mode_changed;
  plugin_class->configure_plugin = power_manager_plugin_configure;
  plugin_class->about = power_manager_plugin_about;
}



static void
power_manager_plugin_init (PowerManagerPlugin *plugin)
{
}



static void
get_config_call_ready (GObject *source_object,
                       GAsyncResult *res,
                       gpointer data)
{
  GError *error = NULL;
  GVariant *value = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (value != NULL)
  {
    GVariantIter *iter;
    const gchar *key, *val;
    g_variant_get (value, "(a{ss})", &iter);
    while (g_variant_iter_next (iter, "{&s&s}", &key, &val))
    {
      if (g_strcmp0 (key, "debug-enabled") == 0)
      {
        xfpm_debug_init (xfpm_string_to_bool (val));
        break;
      }
    }
    g_variant_iter_free (iter);
    g_variant_unref (value);
  }
  else
  {
    g_warning ("Error calling org.xfce.Power.Manager.GetConfig: %s", error->message);
    g_error_free (error);
  }
}

static void
g_name_owner_changed (GDBusProxy *proxy)
{
  gchar *name = g_dbus_proxy_get_name_owner (proxy);
  if (name != NULL)
  {
    g_dbus_proxy_call (proxy, "GetConfig", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, get_config_call_ready, NULL);
    g_free (name);
  }
}

static void
xfpm_proxy_ready (GObject *source_object,
                  GAsyncResult *res,
                  gpointer data)
{
  PowerManagerPlugin *plugin = data;
  GError *error = NULL;

  plugin->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (plugin->proxy != NULL)
  {
    g_signal_connect (plugin->proxy, "notify::g-name-owner", G_CALLBACK (g_name_owner_changed), NULL);
    g_name_owner_changed (plugin->proxy);
  }
  else
  {
    g_warning ("Error getting org.xfce.Power.Manager proxy: %s", error->message);
    g_error_free (error);
  }
}

static void
power_manager_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  PowerManagerPlugin *plugin = POWER_MANAGER_PLUGIN (panel_plugin);

  xfce_panel_plugin_menu_show_configure (panel_plugin);
  xfce_panel_plugin_menu_show_about (panel_plugin);

  xfce_panel_plugin_set_small (panel_plugin, TRUE);

  /* setup transation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  plugin->config = power_manager_config_new (plugin);

  /* instantiate a button box */
  plugin->button = power_manager_button_new (plugin, plugin->config);
  gtk_container_add (GTK_CONTAINER (plugin), GTK_WIDGET (plugin->button));
  power_manager_button_show (plugin->button);

  /* enable/disable debug logs following --debug xfpm option */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.xfce.PowerManager",
                            "/org/xfce/PowerManager",
                            "org.xfce.Power.Manager",
                            NULL,
                            xfpm_proxy_ready,
                            plugin);
}



static void
power_manager_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  PowerManagerPlugin *plugin = POWER_MANAGER_PLUGIN (panel_plugin);

  if (plugin->dialog != NULL)
    g_object_unref (plugin->dialog);

  g_object_unref (plugin->config);
  if (plugin->proxy != NULL)
    g_object_unref (plugin->proxy);
}



static void
power_manager_plugin_mode_changed (XfcePanelPlugin *panel_plugin,
                                   XfcePanelPluginMode mode)
{
  GtkWidget *widget = GTK_WIDGET (panel_plugin);

  gtk_widget_queue_resize (widget);
}



static gboolean
power_manager_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                                   gint size)
{
  PowerManagerPlugin *plugin = POWER_MANAGER_PLUGIN (panel_plugin);
  GtkWidget *widget = GTK_WIDGET (plugin);

  power_manager_button_set_icon (plugin->button);
  gtk_widget_queue_resize (widget);

  return TRUE;
}



static void
power_manager_plugin_style_updated (GtkWidget *widget)
{
  gtk_widget_reset_style (widget);
  gtk_widget_queue_resize (widget);
}



static void
power_manager_plugin_configure (XfcePanelPlugin *panel_plugin)
{
  PowerManagerPlugin *plugin = POWER_MANAGER_PLUGIN (panel_plugin);
  g_return_if_fail (POWER_MANAGER_IS_PLUGIN (plugin));

  if (plugin->dialog == NULL)
    plugin->dialog = power_manager_dialog_new (plugin, plugin->config);
  power_manager_dialog_show (plugin->dialog, gtk_widget_get_screen (GTK_WIDGET (plugin)));
}


static void
power_manager_plugin_about (XfcePanelPlugin *panel_plugin)
{
  xfpm_about ("org.xfce.powermanager");
}
