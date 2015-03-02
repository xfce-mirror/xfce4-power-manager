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
#include <libxfce4util/libxfce4util.h>
#endif

#include "../power-manager-button.h"

/* plugin structure */
typedef struct
{
    XfcePanelPlugin *plugin;

    /* panel widgets */
    GtkWidget       *ebox;
    GtkWidget       *power_manager_button;
}
PowerManagerPlugin;


/* prototypes */
static void power_manager_plugin_construct (XfcePanelPlugin *plugin);
/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER (power_manager_plugin_construct);


static PowerManagerPlugin *
power_manager_plugin_new (XfcePanelPlugin *plugin)
{
    PowerManagerPlugin *power_manager_plugin;

    /* allocate memory for the plugin structure */
    power_manager_plugin = panel_slice_new0 (PowerManagerPlugin);

    /* pointer to plugin */
    power_manager_plugin->plugin = plugin;

    /* create some panel ebox */
    power_manager_plugin->ebox = gtk_event_box_new ();
    gtk_widget_show (power_manager_plugin->ebox);
    gtk_event_box_set_visible_window (GTK_EVENT_BOX(power_manager_plugin->ebox), FALSE);

    power_manager_plugin->power_manager_button = power_manager_button_new (plugin);
    power_manager_button_show(POWER_MANAGER_BUTTON(power_manager_plugin->power_manager_button));
    gtk_container_add (GTK_CONTAINER (power_manager_plugin->ebox), power_manager_plugin->power_manager_button);

    return power_manager_plugin;
}


static void
power_manager_plugin_construct (XfcePanelPlugin *plugin)
{
    PowerManagerPlugin *power_manager_plugin;

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    /* create the plugin */
    power_manager_plugin = power_manager_plugin_new (plugin);

    /* add the ebox to the panel */
    gtk_container_add (GTK_CONTAINER (plugin), power_manager_plugin->ebox);
}
