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

#include <glib.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "common/xfpm-common.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-brightness.h"

#include "brightness-button.h"

static void brightness_button_finalize   (GObject *object);

#define BRIGHTNESS_BUTTON_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), BRIGHTNESS_TYPE_BUTTON, BrightnessButtonPrivate))

struct BrightnessButtonPrivate
{
    XfcePanelPlugin *plugin;
    
    XfpmBrightness  *brightness;
    
    GtkWidget       *popup;
    GtkWidget       *range;
    GtkWidget       *plus;
    GtkWidget       *minus;
    GtkWidget       *image;
    gboolean         popup_open;
};

enum
{
    PROP_0,
    PROP_PLUGIN
};

G_DEFINE_TYPE (BrightnessButton, brightness_button, GTK_TYPE_BUTTON)

static void
brightness_button_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
    BrightnessButton *button = BRIGHTNESS_BUTTON (object);
    switch (prop_id)
    {
	case PROP_PLUGIN:
	    button->priv->plugin = XFCE_PANEL_PLUGIN (g_object_ref (g_value_get_object (value)));
	    break;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

/*
 * The Code for handling grab/ungrab events are taken from GtkScaleButton.
 * GTK - The GIMP Toolkit
 * Copyright (C) 2005 Ronald S. Bultje
 * Copyright (C) 2006, 2007 Christian Persch
 * Copyright (C) 2006 Jan Arne Petersen
 * Copyright (C) 2005-2007 Red Hat, Inc.
 */
static void
brightness_button_grab_notify (BrightnessButton *button, gboolean was_grabbed)
{
    GdkDisplay *display;
    if (was_grabbed != FALSE)
	return;

    if (!GTK_WIDGET_HAS_GRAB (button->priv->popup))
	return;

    if (gtk_widget_is_ancestor (gtk_grab_get_current (), button->priv->popup))
	return;
  
    display = gtk_widget_get_display (button->priv->popup);
    gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
    gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
    gtk_grab_remove (button->priv->popup);
    gtk_widget_hide (button->priv->popup);
    button->priv->popup_open = FALSE;
}

static void
brightness_button_popup_grab_notify (GtkWidget *widget, gboolean was_grabbed, BrightnessButton *button)
{
    brightness_button_grab_notify (button, was_grabbed);
}

static void
brightness_button_range_grab_notify (GtkWidget *widget, gboolean was_grabbed, BrightnessButton *button)
{
    brightness_button_grab_notify (button, was_grabbed);
}

static gboolean
brightness_button_popup_broken_event (GtkWidget *widget, gboolean was_grabbed, BrightnessButton *button)
{
    brightness_button_grab_notify (button, FALSE);
    return FALSE;
}

static void
brightness_button_release_grab (BrightnessButton *button, GdkEventButton *event)
{
    GdkEventButton *e;
    GdkDisplay *display;

    display = gtk_widget_get_display (GTK_WIDGET (button));
    gdk_display_keyboard_ungrab (display, event->time);
    gdk_display_pointer_ungrab (display, event->time);
    gtk_grab_remove (button->priv->popup);

    gtk_widget_hide (button->priv->popup);

    e = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
    e->window = GTK_WIDGET (button)->window;
    e->type = GDK_BUTTON_RELEASE;
    gtk_widget_event (GTK_WIDGET (button), (GdkEvent *) e);
    e->window = event->window;
    gdk_event_free ((GdkEvent *) e);
    button->priv->popup_open = FALSE;
}

static gboolean
brightness_button_popup_button_press_event (GtkWidget *widget, GdkEventButton *ev, BrightnessButton *button)
{
    if ( ev->type == GDK_BUTTON_PRESS )
    {
	brightness_button_release_grab (button, ev);
	return TRUE;
    }
    return FALSE;
}

static gboolean
brightness_button_popup_key_release_event (GtkWidget *widget, GdkEventKey *ev, gpointer data)
{
    
    return FALSE;
}

static void
brightness_button_set_tooltip (BrightnessButton *button)
{
    gboolean has_hw;
    
    has_hw = xfpm_brightness_has_hw (button->priv->brightness);
    
    if ( has_hw )
	gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Control your LCD brightness"));
    else
	gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("No device found"));
}

static gboolean
brightness_button_popup_win (GtkWidget *widget, GdkEvent *ev, guint32 ev_time)
{
    gint x, y, orientation;
    gint current_level = 0;
    GdkDisplay *display;
    GdkScreen *screen;
    BrightnessButton *button;
    XfceScreenPosition pos;
    gboolean has_hw;
    
    button = BRIGHTNESS_BUTTON (widget);
    
    has_hw = xfpm_brightness_has_hw (button->priv->brightness);
    
    if ( !has_hw ) 
	return FALSE;
    
    display = gtk_widget_get_display (widget);
    screen = gtk_widget_get_screen (widget);
    
    gtk_window_set_screen (GTK_WINDOW (button->priv->popup), screen);
    
    gtk_widget_show_all (button->priv->popup);
    
    gtk_grab_add (button->priv->popup);

    if (gdk_pointer_grab (button->priv->popup->window, TRUE,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK, NULL, NULL, ev_time)
	  != GDK_GRAB_SUCCESS)
    {
	gtk_grab_remove (button->priv->popup);
	gtk_widget_hide (button->priv->popup);
	return FALSE;
    }

    if (gdk_keyboard_grab (button->priv->popup->window, TRUE, ev_time) != GDK_GRAB_SUCCESS)
    {
	gdk_display_pointer_ungrab (display, ev_time);
	gtk_grab_remove (button->priv->popup);
	gtk_widget_hide (button->priv->popup);
	return FALSE;
    }

    gtk_widget_grab_focus (button->priv->popup);
    gtk_widget_grab_focus (button->priv->range);
    
    /* Position */
    gdk_window_get_origin (widget->window, &x, &y);

    pos = xfce_panel_plugin_get_screen_position (button->priv->plugin);
    orientation = xfce_panel_plugin_get_orientation (button->priv->plugin);
    
    /* top */
    if ( pos == XFCE_SCREEN_POSITION_NW_H || 
	 pos == XFCE_SCREEN_POSITION_N    ||
	 pos == XFCE_SCREEN_POSITION_NE_H )
    {
	x += widget->allocation.x
		+ widget->allocation.width/2;
	y += widget->allocation.height;
	x -= button->priv->popup->allocation.width/2;
    }
    /* left */
    else if ( pos == XFCE_SCREEN_POSITION_NW_V ||
	      pos == XFCE_SCREEN_POSITION_W    ||
	      pos == XFCE_SCREEN_POSITION_SW_V )
    {
	y += widget->allocation.y
		+ widget->allocation.height/2;
	x += widget->allocation.width;
	y -= button->priv->popup->allocation.height/2;
    }
    /* right */
    else if ( pos == XFCE_SCREEN_POSITION_NE_V ||
	      pos == XFCE_SCREEN_POSITION_E    ||
	      pos == XFCE_SCREEN_POSITION_SE_V )
    {
	y += widget->allocation.y
		+ widget->allocation.height/2;
	x -= button->priv->popup->allocation.width;
	y -= button->priv->popup->allocation.height/2;
    }
    /* bottom */
    else if ( pos == XFCE_SCREEN_POSITION_SW_H ||
	      pos == XFCE_SCREEN_POSITION_S    ||
	      pos == XFCE_SCREEN_POSITION_SE_H )
    {
	x += widget->allocation.x
		+ widget->allocation.width/2;
	y -= button->priv->popup->allocation.height;
	x -= button->priv->popup->allocation.width/2;
    }
    else if ( pos == XFCE_SCREEN_POSITION_FLOATING_H )
    {
	x += widget->allocation.x
		+ widget->allocation.width/2;
	x -= button->priv->popup->allocation.width/2;
	if ( y > button->priv->popup->allocation.height )
	    y -= button->priv->popup->allocation.height;
	else 
	     y += widget->allocation.height;
    }
    else if ( pos == XFCE_SCREEN_POSITION_FLOATING_V )
    {
	y -= button->priv->popup->allocation.height/2;
	y += widget->allocation.y
		+ widget->allocation.height/2;
	if ( x < button->priv->popup->allocation.width )
	    x += widget->allocation.width;
	else
	    x -= button->priv->popup->allocation.width;
    }
    else
    {
	brightness_button_release_grab (button, (GdkEventButton *)ev);
	g_return_val_if_reached (FALSE);
    }
   
    gtk_window_move (GTK_WINDOW(button->priv->popup), x, y);
    TRACE("Displaying window on x=%d y=%d", x, y);
    xfpm_brightness_get_level (button->priv->brightness, &current_level);
    
    gtk_range_set_value (GTK_RANGE(button->priv->range), current_level);
    button->priv->popup_open = TRUE;
    return TRUE;
}

static gboolean
brightness_button_press_event (GtkWidget *widget, GdkEventButton *ev)
{
    return brightness_button_popup_win (widget, (GdkEvent *) ev, ev->time);
}

static void
minus_clicked (GtkWidget *widget, BrightnessButton *button)
{
    gint level, max_level;
    
    max_level = xfpm_brightness_get_max_level (button->priv->brightness);
    level = (gint ) gtk_range_get_value (GTK_RANGE (button->priv->range));
    
    if ( level != 0 )
	gtk_range_set_value (GTK_RANGE (button->priv->range), level - 1);
}

static void
plus_clicked (GtkWidget *widget, BrightnessButton *button)
{
    gint level, max_level;
    
    max_level = xfpm_brightness_get_max_level (button->priv->brightness);
    level = (gint ) gtk_range_get_value (GTK_RANGE (button->priv->range));
    
    if ( level != max_level )
	gtk_range_set_value (GTK_RANGE (button->priv->range), level + 1);
}

static void
range_value_changed (GtkWidget *widget, BrightnessButton *button)
{
    gint range_level, hw_level;
    
    range_level = (gint) gtk_range_get_value (GTK_RANGE (button->priv->range));
    
    xfpm_brightness_get_level (button->priv->brightness, &hw_level);
    
    if ( hw_level != range_level )
    {
	xfpm_brightness_set_level (button->priv->brightness, range_level);
    }
}

static void
brightness_button_create_popup (BrightnessButton *button)
{
    GtkWidget *box;
    GtkOrientation orientation;
    gint max_level;
    gboolean has_hw;
    
    has_hw = xfpm_brightness_has_hw (button->priv->brightness);
    
    if ( !has_hw )
	return;
	
    max_level = xfpm_brightness_get_max_level (button->priv->brightness);
     
    button->priv->popup = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_decorated (GTK_WINDOW(button->priv->popup), FALSE);
    
    g_signal_connect (button->priv->popup, "grab-notify",
		      G_CALLBACK (brightness_button_popup_grab_notify), button);
    g_signal_connect (button->priv->popup, "grab-broken-event",
		      G_CALLBACK (brightness_button_popup_broken_event), button);
    g_signal_connect (button->priv->popup, "key_release_event",
		      G_CALLBACK (brightness_button_popup_key_release_event), button);
    g_signal_connect (button->priv->popup , "button_press_event",
		      G_CALLBACK (brightness_button_popup_button_press_event), button);
		      
    orientation = xfce_panel_plugin_get_orientation (button->priv->plugin);

    if ( orientation == GTK_ORIENTATION_VERTICAL)
	box = gtk_hbox_new (FALSE, 2);
    else
	box = gtk_vbox_new (FALSE, 2);
    
    button->priv->minus = gtk_button_new_with_label ("-");
    gtk_button_set_relief (GTK_BUTTON(button->priv->minus), GTK_RELIEF_NONE);
    g_signal_connect (button->priv->minus, "clicked",
		      G_CALLBACK (minus_clicked), button);
    
    if ( orientation == GTK_ORIENTATION_VERTICAL )
    {
	button->priv->range = gtk_hscale_new_with_range (0, max_level, 1);
	gtk_widget_set_size_request (button->priv->range, 100, -1);
    }
    else
    {
	button->priv->range = gtk_vscale_new_with_range (0, max_level, 1);
	gtk_widget_set_size_request (button->priv->range, -1, 100);
    }
    gtk_range_set_inverted (GTK_RANGE(button->priv->range), TRUE);
    gtk_scale_set_draw_value (GTK_SCALE(button->priv->range), FALSE);
    
    g_signal_connect (button->priv->range, "grab-notify",
		      G_CALLBACK (brightness_button_range_grab_notify), button);
    g_signal_connect (button->priv->range, "value-changed",
		      G_CALLBACK (range_value_changed), button);
  
    button->priv->plus = gtk_button_new_with_label ("+");
    gtk_button_set_relief (GTK_BUTTON(button->priv->plus), GTK_RELIEF_NONE);
    g_signal_connect (button->priv->plus, "clicked",
		      G_CALLBACK (plus_clicked), button);

    gtk_box_pack_start (GTK_BOX(box), button->priv->plus, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(box), button->priv->range, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(box), button->priv->minus, FALSE, FALSE, 0);
    
    gtk_window_set_type_hint (GTK_WINDOW(button->priv->popup), GDK_WINDOW_TYPE_HINT_UTILITY );
    
    gtk_container_add (GTK_CONTAINER(button->priv->popup), box);
}

static void
brightness_button_up (BrightnessButton *button)
{
    gint level;
    gint max_level;
    
    xfpm_brightness_get_level (button->priv->brightness, &level);
    max_level = xfpm_brightness_get_max_level (button->priv->brightness);
    
    if ( level != max_level )
    {
	plus_clicked (NULL, button);
    }
}

static void
brightness_button_down (BrightnessButton *button)
{
    gint level;
    xfpm_brightness_get_level (button->priv->brightness, &level);
    
    if ( level != 0 )
    {
	minus_clicked (NULL, button);
    }
}

static gboolean
brightness_button_scroll_event (GtkWidget *widget, GdkEventScroll *ev)
{
    gboolean hw_found;
    BrightnessButton *button;
    
    button = BRIGHTNESS_BUTTON (widget);
    
    hw_found = xfpm_brightness_has_hw (button->priv->brightness);
    
    if ( !hw_found )
	return FALSE;
	
    if ( ev->direction == GDK_SCROLL_UP )
    {
	brightness_button_up (button);
	return TRUE;
    }
    else if ( ev->direction == GDK_SCROLL_DOWN )
    {
	brightness_button_down (button);
	return TRUE;
    }
    return FALSE;
}

static void
brightness_button_class_init (BrightnessButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = brightness_button_finalize;
    object_class->set_property = brightness_button_set_property;
    
    widget_class->button_press_event = brightness_button_press_event;
    widget_class->scroll_event = brightness_button_scroll_event;

    g_object_class_install_property (object_class,
				     PROP_PLUGIN,
				     g_param_spec_object ("plugin",
							  NULL,
							  NULL,
							  XFCE_TYPE_PANEL_PLUGIN,
							  G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_WRITABLE));

    g_type_class_add_private (klass, sizeof (BrightnessButtonPrivate));
}

static void
brightness_button_init (BrightnessButton *button)
{
    button->priv = BRIGHTNESS_BUTTON_GET_PRIVATE (button);
    
    button->priv->brightness = xfpm_brightness_new ();
    xfpm_brightness_setup (button->priv->brightness);
    
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
}

static void
brightness_button_finalize (GObject *object)
{
    BrightnessButton *button;

    button = BRIGHTNESS_BUTTON (object);

    g_object_unref (button->priv->brightness);
    g_object_unref (button->priv->plugin);

    G_OBJECT_CLASS (brightness_button_parent_class)->finalize (object);
}

GtkWidget *
brightness_button_new (XfcePanelPlugin *plugin)
{
    BrightnessButton *button = NULL;
    button = g_object_new (BRIGHTNESS_TYPE_BUTTON, "plugin", plugin, NULL);
    return GTK_WIDGET (button);
}

static void
destroy_popup (BrightnessButton *button)
{
    if ( GTK_IS_WIDGET (button->priv->range) )
	gtk_widget_destroy (button->priv->range);
	
    if ( GTK_IS_WIDGET (button->priv->minus) )
	gtk_widget_destroy (button->priv->minus);
	
    if ( GTK_IS_WIDGET (button->priv->plus) )
	gtk_widget_destroy (button->priv->plus);
	
    if ( GTK_IS_WIDGET (button->priv->popup) )
	gtk_widget_destroy (button->priv->popup);
}

static gboolean
brightness_button_set_icon (BrightnessButton *button, gint width)
{
    gboolean hw_found;
    GdkPixbuf *pixbuf;
    const gchar *icon_name;
    
    hw_found = xfpm_brightness_has_hw (button->priv->brightness);
    
    icon_name = hw_found ? XFPM_DISPLAY_BRIGHTNESS_ICON : XFPM_DISPLAY_BRIGHTNESS_INVALID_ICON;
    
    pixbuf = xfce_themed_icon_load (icon_name, width);
    
    if ( pixbuf )
    {
	gtk_image_set_from_pixbuf (GTK_IMAGE (button->priv->image), pixbuf);
	g_object_unref (pixbuf);
	return TRUE;
    }
    return FALSE;
}

static gboolean
brightness_button_size_changed_cb (XfcePanelPlugin *plugin, gint size, BrightnessButton *button)
{
    gint width = size -2 - 2* MAX(GTK_WIDGET(button)->style->xthickness,
				  GTK_WIDGET(button)->style->xthickness);
				 
    gtk_widget_set_size_request (GTK_WIDGET(plugin), size, size);
    return brightness_button_set_icon (button, width);
}

static void
reload_activated (GtkWidget *widget, BrightnessButton *button)
{
    gint size;
    
    destroy_popup (button);
    xfpm_brightness_setup (button->priv->brightness);
    brightness_button_create_popup (button);
    brightness_button_set_tooltip (button);
    
    size = xfce_panel_plugin_get_size (button->priv->plugin);
    brightness_button_size_changed_cb (button->priv->plugin, size, button);
}

static void
brightness_button_free_data_cb (XfcePanelPlugin *plugin, BrightnessButton *button)
{
    destroy_popup (button);
    gtk_widget_destroy (GTK_WIDGET (button));
}

static void
brightness_button_orientation_changed_cb (XfcePanelPlugin *plugin, GtkOrientation or, BrightnessButton *button)
{
    destroy_popup (button);
    brightness_button_create_popup (button);
}

void brightness_button_show (BrightnessButton *button)
{
    GtkWidget *mi;
    
    g_return_if_fail (BRIGHTNESS_IS_BUTTON (button));
    
    gtk_container_add (GTK_CONTAINER (button->priv->plugin),
		       GTK_WIDGET (button));
		       
    xfce_panel_plugin_add_action_widget (button->priv->plugin, GTK_WIDGET (button));
    
    button->priv->image = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (button), button->priv->image);
    
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_REFRESH, NULL);
    gtk_widget_show (mi);
    
    g_signal_connect (mi, "activate", 
		      G_CALLBACK (reload_activated), button);
		      
    xfce_panel_plugin_menu_insert_item (button->priv->plugin, GTK_MENU_ITEM (mi));
    
    g_signal_connect (button->priv->plugin, "size-changed",
		      G_CALLBACK (brightness_button_size_changed_cb), button);
		      
    g_signal_connect (button->priv->plugin, "orientation_changed", 
		      G_CALLBACK (brightness_button_orientation_changed_cb), button);
		      
    g_signal_connect (button->priv->plugin, "free-data",
		      G_CALLBACK (brightness_button_free_data_cb), button);
    
    g_signal_connect (button->priv->plugin, "about",
		      G_CALLBACK (xfpm_about), _("Brightness plugin"));
    
    xfce_panel_plugin_menu_show_about (button->priv->plugin);
    
    gtk_widget_show_all (GTK_WIDGET(button));
    brightness_button_create_popup (button);
    brightness_button_set_tooltip (button);
}
