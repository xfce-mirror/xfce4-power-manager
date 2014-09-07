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

#include "../power-manager-button.h"

/* plugin structure */
typedef struct
{
    Plugin *plugin;

    /* panel widgets */
    GtkWidget       *ebox;
    GtkWidget       *power_manager_button;
}
PowerManagerPlugin;


/* prototypes */
static int lxde_power_manager_plugin_construct (Plugin *p, char **fp);
static void lxde_power_manager_plugin_configuration_changed(Plugin *p);

PluginClass lxde_power_manager_plugin_class = {
    PLUGINCLASS_VERSIONING,
    type : "lxde_power_manager",
    name : N_("Power Manager Plugin"),
    version: PACKAGE_VERSION,
    description : N_("Display the battery levels of your devices and control the brightness of your display"),
    one_per_system : FALSE,
    expand_available : FALSE,
    constructor : lxde_power_manager_plugin_construct,
    destructor  : NULL,
    config : NULL,
    save : NULL,
    panel_configuration_changed : lxde_power_manager_plugin_configuration_changed
};


static PowerManagerPlugin *
lxde_power_manager_plugin_new (Plugin *plugin)
{
    PowerManagerPlugin *power_manager_plugin;

    /* allocate memory for the plugin structure */
    power_manager_plugin = g_new0 (PowerManagerPlugin, 1);

    /* pointer to plugin */
    power_manager_plugin->plugin = plugin;

    /* create some panel ebox */
    power_manager_plugin->ebox = gtk_event_box_new ();
    gtk_widget_show (power_manager_plugin->ebox);
    gtk_event_box_set_visible_window (GTK_EVENT_BOX(power_manager_plugin->ebox), FALSE);

    power_manager_plugin->power_manager_button = power_manager_button_new ();
    power_manager_button_show(POWER_MANAGER_BUTTON(power_manager_plugin->power_manager_button));
    gtk_container_add (GTK_CONTAINER (power_manager_plugin->ebox), power_manager_plugin->power_manager_button);

    return power_manager_plugin;
}


static int
lxde_power_manager_plugin_construct (Plugin *plugin, char **fp)
{
    PowerManagerPlugin *power_manager_plugin;

    /* create the plugin */
    power_manager_plugin = lxde_power_manager_plugin_new (plugin);

    /* add the ebox to the panel */
    plugin->pwid = power_manager_plugin->ebox;
    plugin->priv = power_manager_plugin;

    return 1;
}

static void
lxde_power_manager_plugin_configuration_changed(Plugin *p)
{
    PowerManagerPlugin *power_manager_plugin = p->priv;

    /* Determine orientation and size */
    GtkOrientation orientation = (p->panel->orientation == GTK_ORIENTATION_VERTICAL) ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;

    int size = (orientation == GTK_ORIENTATION_VERTICAL) ? p->panel->width : p->panel->height;

    if ( orientation == GTK_ORIENTATION_HORIZONTAL )
        gtk_widget_set_size_request (p->pwid, -1, size);
    else
        gtk_widget_set_size_request (p->pwid, size, -1);

    /* update the button's width */
    power_manager_button_set_width (POWER_MANAGER_BUTTON(power_manager_plugin->power_manager_button), p->panel->icon_size);
}
