/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <dbus/dbus-glib.h>

#include "libxfpm/hal-manager.h"
#include "libxfpm/hal-device.h"
#include "libxfpm/xfpm-common.h"

typedef struct
{
    DBusGConnection  *bus;
    
    XfcePanelPlugin  *plugin;
    gint              max_level;
    gint              current_level;
    gboolean          hw_found;
    
    gboolean          open;
    DBusGProxy       *proxy;

    GtkWidget        *button;
    
    GtkWidget        *scale;
    GtkWidget        *image;
    GtkWidget 	     *win;
    GtkWidget        *plus;
    GtkWidget        *minus;

} brightness_t;

static gint 
brightness_plugin_get_level (brightness_t *brightness)
{
    GError *error = NULL;
    gint level = 0;
    gboolean ret;
    
    ret = dbus_g_proxy_call (brightness->proxy, "GetBrightness", &error,
	 		     G_TYPE_INVALID,
			     G_TYPE_INT, &level,
			     G_TYPE_INVALID);

    if ( error )
    {
	g_critical ("Error getting brightness level: %s\n", error->message);
	g_error_free (error);
    }
    return level;
}

static gboolean
brightness_plugin_set_level (brightness_t *brightness, gint level)
{
    GError *error = NULL;
    gboolean ret;
    gint dummy;
    
    ret = dbus_g_proxy_call (brightness->proxy, "SetBrightness", &error,
			     G_TYPE_INT, level,
			     G_TYPE_INVALID,
			     G_TYPE_INT, &dummy,
			     G_TYPE_INVALID );
    if ( error )
    {
	g_critical ("Error setting brightness level: %s\n", error->message);
	g_error_free (error);
    }
    return ret;
}

static void
brightness_plugin_get_device (brightness_t *brightness)
{
    HalManager *manager;
    HalDevice *device;
    gchar **udis = NULL;
    
    //FIXME Don't connect blindly
    brightness->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
    
    manager = hal_manager_new ();
    
    udis = hal_manager_find_device_by_capability (manager, "laptop_panel");
    
    if (!udis || !udis[0] )
    {
	TRACE ("No laptop panel found on the system");
	brightness->hw_found = FALSE;
	goto out;
    }
    
    device = hal_device_new ();
    hal_device_set_udi (device, udis[0]);
    
    brightness->max_level =
	hal_device_get_property_int (device, "laptop_panel.num_levels") -1;
	
    TRACE("Laptop panel %s with max level %d", udis[0], brightness->max_level);
    
    brightness->proxy = dbus_g_proxy_new_for_name (brightness->bus,
		   			           "org.freedesktop.Hal",
						   udis[0],
						   "org.freedesktop.Hal.Device.LaptopPanel");
    brightness->hw_found = TRUE;
    brightness->current_level = brightness_plugin_get_level (brightness);
    
    g_object_unref (device);

out:
    g_object_unref (manager);
}

static void 
brightness_plugin_button_press_cb (GtkWidget *widget, brightness_t *plugin)
{
    gint x, y, orientation;
    GdkDisplay *display;
    GdkScreen *screen;
    XfceScreenPosition pos;
    
    if ( plugin->open )
    {
	gtk_widget_hide (plugin->win);
	plugin->open = FALSE;
	return;
    }
    
    display = gtk_widget_get_display (plugin->button);
    screen = gtk_widget_get_screen (plugin->button);
    
    gtk_window_set_screen (GTK_WINDOW(plugin->win), screen);
    gdk_window_get_origin (plugin->button->window, &x, &y);
    gtk_widget_show_all (plugin->win);

    pos = xfce_panel_plugin_get_screen_position (plugin->plugin);
    orientation = xfce_panel_plugin_get_orientation (plugin->plugin);
    
    /* top */
    if ( pos == XFCE_SCREEN_POSITION_NW_H || 
	 pos == XFCE_SCREEN_POSITION_N    ||
	 pos == XFCE_SCREEN_POSITION_NE_H )
    {
	x += plugin->button->allocation.x
		+ plugin->button->allocation.width/2;
	y += plugin->button->allocation.height;
	x -= plugin->win->allocation.width/2;
    }
    /* left */
    else if ( pos == XFCE_SCREEN_POSITION_NW_V ||
	      pos == XFCE_SCREEN_POSITION_W    ||
	      pos == XFCE_SCREEN_POSITION_SW_V )
    {
	y += plugin->button->allocation.y
		+ plugin->button->allocation.height/2;
	x += plugin->button->allocation.width;
	y -= plugin->win->allocation.height/2;
    }
    /* right */
    else if ( pos == XFCE_SCREEN_POSITION_NE_V ||
	      pos == XFCE_SCREEN_POSITION_E    ||
	      pos == XFCE_SCREEN_POSITION_SE_V )
    {
	y += plugin->button->allocation.y
		+ plugin->button->allocation.height/2;
	x -= plugin->win->allocation.width;
	y -= plugin->win->allocation.height/2;
    }
    /* bottom */
    else if ( pos == XFCE_SCREEN_POSITION_SW_H ||
	      pos == XFCE_SCREEN_POSITION_S    ||
	      pos == XFCE_SCREEN_POSITION_SE_H )
    {
	x += plugin->button->allocation.x
		+ plugin->button->allocation.width/2;
	y -= plugin->win->allocation.height;
	x -= plugin->win->allocation.width/2;
    }
    /* floating */
    else if ( pos == XFCE_SCREEN_POSITION_FLOATING_H ||
	      pos == XFCE_SCREEN_POSITION_FLOATING_V )
    {
	g_warning ("Floating position");
	return;
    }
    else return;
   
    gtk_window_move (GTK_WINDOW(plugin->win), x, y);
    plugin->current_level = brightness_plugin_get_level (plugin);
    
    gtk_range_set_value (GTK_RANGE(plugin->scale), plugin->current_level);
    
    plugin->open = TRUE;
}

static void
plus_clicked (GtkWidget *widget, brightness_t *plugin)
{
    gint level = (gint)gtk_range_get_value (GTK_RANGE(plugin->scale));
    
    if ( level != plugin->max_level )
	gtk_range_set_value (GTK_RANGE(plugin->scale), level + 1);
}

static void
minus_clicked (GtkWidget *widget, brightness_t *plugin)
{
    gint level = (gint)gtk_range_get_value (GTK_RANGE(plugin->scale));
    
    if ( level != 0 )
	gtk_range_set_value (GTK_RANGE(plugin->scale), level - 1);
}

static void
scale_value_changed (GtkRange *range, brightness_t *plugin)
{
    gint range_level = (gint)gtk_range_get_value (range);
    gint hw_level = brightness_plugin_get_level (plugin);
    
    if ( hw_level != range_level )
    {
	if (!brightness_plugin_set_level (plugin, range_level))
	{
	    g_warning("Failed to set brightness level\n");
	}
    }
}

static void
brightness_plugin_destroy_popup (brightness_t *plugin)
{
    if ( plugin->win != NULL)
    {
	gtk_widget_destroy (plugin->win);
	plugin->win = NULL;
    }
}

static void
brightness_plugin_create_popup (brightness_t *plugin)
{
    GtkWidget *box;
    GtkOrientation orientation;
    
    plugin->win = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_decorated (GTK_WINDOW(plugin->win), FALSE);
    
    orientation = xfce_panel_plugin_get_orientation (plugin->plugin);

    if ( orientation == GTK_ORIENTATION_VERTICAL)
	box = gtk_hbox_new (FALSE, 2);
    else
	box = gtk_vbox_new (FALSE, 2);
    
    plugin->minus = gtk_button_new_with_label ("-");
    gtk_button_set_relief (GTK_BUTTON(plugin->minus), GTK_RELIEF_NONE);
    
    if ( orientation == GTK_ORIENTATION_VERTICAL )
    {
	plugin->scale = gtk_hscale_new_with_range (0, plugin->max_level, 1);
	gtk_widget_set_size_request (plugin->scale, 100, -1);
    }
    else
    {
	plugin->scale = gtk_vscale_new_with_range (0, plugin->max_level, 1);
	gtk_widget_set_size_request (plugin->scale, -1, 100);
    }
    gtk_range_set_inverted (GTK_RANGE(plugin->scale), TRUE);
    gtk_scale_set_draw_value (GTK_SCALE(plugin->scale), FALSE);
  
    
    plugin->plus = gtk_button_new_with_label ("+");
    gtk_button_set_relief (GTK_BUTTON(plugin->plus), GTK_RELIEF_NONE);

    gtk_box_pack_start (GTK_BOX(box), plugin->plus, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(box), plugin->scale, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(box), plugin->minus, FALSE, FALSE, 0);
    
    
    plugin->win = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_type_hint (GTK_WINDOW(plugin->win), GDK_WINDOW_TYPE_HINT_UTILITY );
    
    gtk_container_add (GTK_CONTAINER(plugin->win), box);
}

static gboolean
brightness_plugin_set_icon (brightness_t *brightness, gint width)
{
    GdkPixbuf *pixbuf;
    const gchar *icon_name;
    
    icon_name = brightness->hw_found ? "gpm-brightness-lcd" : "gpm-brightness-lcd-invalid";
    pixbuf = xfce_themed_icon_load (icon_name, width);
    
    if ( pixbuf )
    {
	gtk_image_set_from_pixbuf (GTK_IMAGE(brightness->image), pixbuf);
	g_object_unref (pixbuf);
	return TRUE;
    }
    return FALSE;
}

static gboolean
brightness_plugin_size_changed_cb (XfcePanelPlugin *plugin, gint size, brightness_t *brightness)
{
    gint width = size -2 - 2* MAX(brightness->button->style->xthickness,
				 brightness->button->style->xthickness);
				 
    gtk_widget_set_size_request (GTK_WIDGET(plugin), size, size);
    return brightness_plugin_set_icon (brightness, width);
}


static void
brightness_plugin_construct_popup (brightness_t *plugin)
{
    brightness_plugin_create_popup (plugin);
    plugin->open = FALSE;

    g_signal_connect (plugin->button, "clicked",
		      G_CALLBACK (brightness_plugin_button_press_cb), plugin);
		      
    g_signal_connect (plugin->plus, "clicked",
		      G_CALLBACK (plus_clicked), plugin);
		      
    g_signal_connect (plugin->minus, "clicked",
		      G_CALLBACK (minus_clicked), plugin);

    g_signal_connect (plugin->scale, "value-changed",
		      G_CALLBACK(scale_value_changed), plugin);
}

static void
brightness_plugin_orientation_changed_cb (XfcePanelPlugin *plugin, 
					  GtkOrientation orientation, 
					  brightness_t *brightness)
{
    brightness_plugin_destroy_popup (brightness);
    brightness_plugin_construct_popup (brightness);
    
}

static void brightness_plugin_free_data_cb (XfcePanelPlugin *plugin, brightness_t *brightness)
{
    if ( brightness->win )
	gtk_widget_destroy (brightness->win);
	
    if ( brightness->bus)
	dbus_g_connection_unref (brightness->bus);
	
    if ( brightness->proxy )
	g_object_unref (brightness->proxy);
	
    g_free (brightness);
}

static void
brightness_plugin_construct (brightness_t *plugin)
{
    plugin->image = gtk_image_new ();
    plugin->button = gtk_toggle_button_new ();
    
    gtk_container_add (GTK_CONTAINER(plugin->button), plugin->image);
    
    gtk_button_set_relief (GTK_BUTTON(plugin->button), GTK_RELIEF_NONE);
    
    gtk_container_add (GTK_CONTAINER(plugin->plugin), plugin->button);
    
    xfce_panel_plugin_add_action_widget (plugin->plugin, plugin->button);

    gtk_widget_show_all (plugin->button);
}

static void
register_brightness_plugin (XfcePanelPlugin *plugin)
{
    brightness_t *brightness;
    
    brightness = g_new0 (brightness_t, 1); 
    
    brightness->plugin = plugin;
    
    brightness_plugin_construct (brightness);
    brightness_plugin_get_device (brightness);
    
    if ( brightness->hw_found )
    {
	brightness_plugin_construct_popup (brightness);
	brightness_plugin_set_level (brightness, 9);
	gtk_widget_set_tooltip_text (brightness->button, _("Control your LCD brightness level"));
    }
    else
    {
	gtk_widget_set_tooltip_text (brightness->button, _("No device found"));
    }
    
    g_signal_connect (plugin, "free-data",
		      G_CALLBACK(brightness_plugin_free_data_cb), brightness);
		      
    g_signal_connect (plugin, "size-changed",
		      G_CALLBACK(brightness_plugin_size_changed_cb), brightness);
		      
    g_signal_connect (plugin, "orientation-changed",
		      G_CALLBACK(brightness_plugin_orientation_changed_cb), brightness);
    xfce_panel_plugin_menu_show_about(plugin);
    g_signal_connect (plugin, "about", G_CALLBACK(xfpm_about), _("Brightness plugin"));

}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(register_brightness_plugin);
