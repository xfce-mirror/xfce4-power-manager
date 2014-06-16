/*
 * * Copyright (C) 2014 Eric Koegel <eric@xfce.org>
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

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#ifdef XFCE_PLUGIN
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#endif

#include "../battery-button.h"

/* plugin structure */
typedef struct
{
    XfcePanelPlugin *plugin;

    /* panel widgets */
    GtkWidget       *ebox;
    GtkWidget       *battery_button;
}
BatteryPlugin;


/* prototypes */
static void battery_plugin_construct (XfcePanelPlugin *plugin);
/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER (battery_plugin_construct);


static BatteryPlugin *
battery_plugin_new (XfcePanelPlugin *plugin)
{
    BatteryPlugin *battery_plugin;

    /* allocate memory for the plugin structure */
    battery_plugin = panel_slice_new0 (BatteryPlugin);

    /* pointer to plugin */
    battery_plugin->plugin = plugin;

    /* create some panel ebox */
    battery_plugin->ebox = gtk_event_box_new ();
    gtk_widget_show (battery_plugin->ebox);
    gtk_event_box_set_visible_window (GTK_EVENT_BOX(battery_plugin->ebox), FALSE);

    battery_plugin->battery_button = battery_button_new (plugin);
    battery_button_show(BATTERY_BUTTON(battery_plugin->battery_button));
    gtk_container_add (GTK_CONTAINER (battery_plugin->ebox), battery_plugin->battery_button);

    return battery_plugin;
}


static void
battery_plugin_construct (XfcePanelPlugin *plugin)
{
    BatteryPlugin *battery_plugin;

    /* create the plugin */
    battery_plugin = battery_plugin_new (plugin);

    /* add the ebox to the panel */
    gtk_container_add (GTK_CONTAINER (plugin), battery_plugin->ebox);
}
