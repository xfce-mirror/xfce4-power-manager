/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "brightness-button.h"

/* plugin structure */
typedef struct
{
    XfcePanelPlugin *plugin;

    /* panel widgets */
    GtkWidget       *ebox;
    GtkWidget       *brightness_button;
}
BrightnessPlugin;

/* prototypes */
static void brightness_plugin_construct (XfcePanelPlugin *plugin);


/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER (brightness_plugin_construct);


static BrightnessPlugin *
brightness_plugin_new (XfcePanelPlugin *plugin)
{
    BrightnessPlugin *brightness_plugin;

    /* allocate memory for the plugin structure */
    brightness_plugin = panel_slice_new0 (BrightnessPlugin);

    /* pointer to plugin */
    brightness_plugin->plugin = plugin;

    /* pointer to plugin */
    brightness_plugin->plugin = plugin;

    /* create some panel ebox */
    brightness_plugin->ebox = gtk_event_box_new ();
    gtk_widget_show (brightness_plugin->ebox);
    gtk_event_box_set_visible_window (GTK_EVENT_BOX(brightness_plugin->ebox), FALSE);

    brightness_plugin->brightness_button = brightness_button_new (plugin);
    brightness_button_show(BRIGHTNESS_BUTTON(brightness_plugin->brightness_button));
    gtk_container_add (GTK_CONTAINER (brightness_plugin->ebox), brightness_plugin->brightness_button);

    return brightness_plugin;
}


static void
brightness_plugin_construct (XfcePanelPlugin *plugin)
{
    BrightnessPlugin *brightness_plugin;

    /* create the plugin */
    brightness_plugin = brightness_plugin_new (plugin);

    /* add the ebox to the panel */
    gtk_container_add (GTK_CONTAINER (plugin), brightness_plugin->ebox);
}
