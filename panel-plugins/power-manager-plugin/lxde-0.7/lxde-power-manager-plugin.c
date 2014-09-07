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
    LXPanel *panel;

    /* panel widgets */
    GtkWidget       *ebox;
    GtkWidget       *power_manager_button;
}
PowerManagerPlugin;


/* prototypes */
static GtkWidget * power_manager_plugin_construct (LXPanel *panel, config_setting_t *settings);

/* register the plugin */
FM_DEFINE_MODULE(lxpanel_gtk, power_manager_plugin);

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
   .name = N_("Power Manager Plugin"),
   .description = N_("Display the battery levels of your devices and control the brightness of your display"),
   .new_instance = power_manager_plugin_construct
};

static GtkWidget *
power_manager_plugin_new (LXPanel *panel)
{
    PowerManagerPlugin *power_manager_plugin;

    /* allocate memory for the plugin structure */
    power_manager_plugin = g_new0 (PowerManagerPlugin, 1);

    /* pointer to panel */
    power_manager_plugin->panel = panel;

    /* create a panel ebox */
    power_manager_plugin->ebox = gtk_event_box_new ();
    gtk_widget_show (power_manager_plugin->ebox);
    gtk_event_box_set_visible_window (GTK_EVENT_BOX(power_manager_plugin->ebox), FALSE);

    power_manager_plugin->power_manager_button = power_manager_button_new ();
    power_manager_button_show(POWER_MANAGER_BUTTON(power_manager_plugin->power_manager_button));
    gtk_container_add (GTK_CONTAINER (power_manager_plugin->ebox), power_manager_plugin->power_manager_button);

    /* bind the plugin structure to the widget */
    lxpanel_plugin_set_data(power_manager_plugin->ebox, power_manager_plugin, g_free);

    return power_manager_plugin->ebox;
}


static GtkWidget *
power_manager_plugin_construct (LXPanel *panel, config_setting_t *settings)
{
    /* create the plugin */
    return power_manager_plugin_new (panel);
}
