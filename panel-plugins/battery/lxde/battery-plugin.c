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

#include "../battery-button.h"

/* plugin structure */
typedef struct
{
    Plugin *plugin;

    /* panel widgets */
    GtkWidget       *ebox;
    GtkWidget       *battery_button;
}
BatteryPlugin;


/* prototypes */
static int battery_plugin_construct (Plugin *p, char **fp);
static void battery_plugin_configuration_changed(Plugin *p);

PluginClass lxdebattery_plugin_class = {
    PLUGINCLASS_VERSIONING,
    type : "lxdebattery",
    name : N_("Battery indicator plugin"),
    version: PACKAGE_VERSION,
    description : N_("Display the battery levels of your devices"),
    one_per_system : FALSE,
    expand_available : FALSE,
    constructor : battery_plugin_construct,
    destructor  : NULL,
    config : NULL,
    save : NULL,
    panel_configuration_changed : battery_plugin_configuration_changed
};


static BatteryPlugin *
battery_plugin_new (Plugin *plugin)
{
    BatteryPlugin *battery_plugin;

    /* allocate memory for the plugin structure */
    battery_plugin = g_new0 (BatteryPlugin, 1);

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


static int
battery_plugin_construct (Plugin *plugin, char **fp)
{
    BatteryPlugin *battery_plugin;

    /* create the plugin */
    battery_plugin = battery_plugin_new (plugin);

    /* add the ebox to the panel */
    plugin->pwid = battery_plugin->ebox;
    plugin->priv = battery_plugin;

    return 1;
}

static void
battery_plugin_configuration_changed(Plugin *p)
{
    BatteryPlugin *battery_plugin = p->priv;

    /* Determine orientation and size */
    GtkOrientation orientation = (p->panel->orientation == GTK_ORIENTATION_VERTICAL) ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;

    int size = (orientation == GTK_ORIENTATION_VERTICAL) ? p->panel->width : p->panel->height;

    if ( orientation == GTK_ORIENTATION_HORIZONTAL )
        gtk_widget_set_size_request (p->pwid, -1, size);
    else
        gtk_widget_set_size_request (p->pwid, size, -1);

    /* update the button's width */
    battery_button_set_width (BATTERY_BUTTON(battery_plugin->battery_button), p->panel->icon_size);
}
