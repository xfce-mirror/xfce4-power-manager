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

#include <glib.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <dbus/dbus-glib.h>
#include <upower.h>

#include "common/xfpm-common.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"

#include "battery-button.h"

static void battery_button_finalize   (GObject *object);
static gchar* get_device_description (UpClient *upower, UpDevice *device);
static GtkTreeIter* find_device_in_tree (BatteryButton *button, const gchar *object_path);
static gboolean battery_button_set_icon (BatteryButton *button);

#define BATTERY_BUTTON_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), BATTERY_TYPE_BUTTON, BatteryButtonPrivate))

struct BatteryButtonPrivate
{
    XfcePanelPlugin *plugin;

    UpClient        *upower;

    GtkWidget       *popup;
    GtkWidget       *image;
    GtkWidget       *treeview;
    gboolean         popup_open;
    gchar           *panel_icon_name;
    gint             panel_icon_width;
};

enum
{
    PROP_0,
    PROP_PLUGIN
};

enum
{
    DEVICE_INFO_NAME,
    DEVICE_INFO_VALUE,
    DEVICE_INFO_LAST
};

enum
{
    COL_ICON,
    COL_NAME,
    COL_OBJ_PATH,
    COL_OBJ_DEVICE_POINTER,
    COL_OBJ_SIGNAL_ID,
    NCOLS
};

G_DEFINE_TYPE (BatteryButton, battery_button, GTK_TYPE_BUTTON)

static void
battery_button_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
    BatteryButton *button = BATTERY_BUTTON (object);
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
battery_button_grab_notify (BatteryButton *button, gboolean was_grabbed)
{
    GdkDisplay *display;
    if (was_grabbed != FALSE)
	return;

    if (!gtk_widget_has_grab (button->priv->popup))
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
battery_button_popup_grab_notify (GtkWidget *widget, gboolean was_grabbed, BatteryButton *button)
{
    battery_button_grab_notify (button, was_grabbed);
}

static gboolean
battery_button_popup_broken_event (GtkWidget *widget, gboolean was_grabbed, BatteryButton *button)
{
    battery_button_grab_notify (button, FALSE);
    return FALSE;
}

static void
battery_button_release_grab (BatteryButton *button, GdkEventButton *event)
{
    GdkEventButton *e;
    GdkDisplay *display;

    display = gtk_widget_get_display (GTK_WIDGET (button));
    gdk_display_keyboard_ungrab (display, event->time);
    gdk_display_pointer_ungrab (display, event->time);
    gtk_grab_remove (button->priv->popup);

    gtk_widget_hide (button->priv->popup);

    e = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
    e->window = gtk_widget_get_window (GTK_WIDGET (button));
    e->type = GDK_BUTTON_RELEASE;
    gtk_widget_event (GTK_WIDGET (button), (GdkEvent *) e);
    e->window = event->window;
    gdk_event_free ((GdkEvent *) e);
    button->priv->popup_open = FALSE;
}

static gboolean
battery_button_popup_button_press_event (GtkWidget *widget, GdkEventButton *ev, BatteryButton *button)
{
    if ( ev->type == GDK_BUTTON_PRESS )
    {
	battery_button_release_grab (button, ev);
	return TRUE;
    }
    return FALSE;
}

static gboolean
battery_button_popup_key_release_event (GtkWidget *widget, GdkEventKey *ev, gpointer data)
{

    return FALSE;
}

static void
battery_button_set_tooltip (BatteryButton *button)
{
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Display battery levels for attached devices"));
}

static gboolean
battery_button_popup_win (GtkWidget *widget, GdkEvent *ev, guint32 ev_time)
{
    gint x, y;
    GdkDisplay *display;
    GdkScreen *screen;
    BatteryButton *button;
    XfceScreenPosition pos;
    GtkAllocation widget_allocation, popup_allocation;

    button = BATTERY_BUTTON (widget);

    display = gtk_widget_get_display (widget);
    screen = gtk_widget_get_screen (widget);

    gtk_window_set_screen (GTK_WINDOW (button->priv->popup), screen);

    gtk_widget_show_all (button->priv->popup);

    gtk_grab_add (button->priv->popup);

    if (gdk_pointer_grab (gtk_widget_get_window (button->priv->popup), TRUE,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK, NULL, NULL, ev_time)
	  != GDK_GRAB_SUCCESS)
    {
	gtk_grab_remove (button->priv->popup);
	gtk_widget_hide (button->priv->popup);
	return FALSE;
    }

    if (gdk_keyboard_grab (gtk_widget_get_window (button->priv->popup), TRUE, ev_time) != GDK_GRAB_SUCCESS)
    {
	gdk_display_pointer_ungrab (display, ev_time);
	gtk_grab_remove (button->priv->popup);
	gtk_widget_hide (button->priv->popup);
	return FALSE;
    }

    gtk_widget_grab_focus (button->priv->popup);

    /* Position */
    gdk_window_get_origin (gtk_widget_get_window (widget), &x, &y);

    pos = xfce_panel_plugin_get_screen_position (button->priv->plugin);

    gtk_widget_get_allocation (widget, &widget_allocation);
    gtk_widget_get_allocation (button->priv->popup, &popup_allocation);


    if ( pos == XFCE_SCREEN_POSITION_N )
    {
	x += widget_allocation.x + widget_allocation.width/2;
	y += widget_allocation.height;
	x -= popup_allocation.width/2;
    }
    else if ( pos == XFCE_SCREEN_POSITION_NE_H )
    {
	x += widget_allocation.x + widget_allocation.width;
	y += widget_allocation.height;
	x -= popup_allocation.width;
    }
    else if ( pos == XFCE_SCREEN_POSITION_NW_H )
    {
	x += widget_allocation.x + widget_allocation.width/2;
	y += widget_allocation.height;
    }
    else if ( pos == XFCE_SCREEN_POSITION_NW_V ||
	      pos == XFCE_SCREEN_POSITION_W )
    {
	y += widget_allocation.y + widget_allocation.height/2;
	x += widget_allocation.width;
    }
    else if ( pos == XFCE_SCREEN_POSITION_SW_V )
    {
	y += widget_allocation.y + widget_allocation.height/2;
	x += widget_allocation.width;
	y -= popup_allocation.height;
    }
    else if ( pos == XFCE_SCREEN_POSITION_NE_V )
    {
	x += widget_allocation.x + widget_allocation.width;
	y += widget_allocation.height;
	x -= popup_allocation.width;
    }
    else if ( pos == XFCE_SCREEN_POSITION_E    ||
	      pos == XFCE_SCREEN_POSITION_SE_V )
    {
	y += widget_allocation.y
		+ widget_allocation.height/2;
	x -= popup_allocation.width;
	y -= popup_allocation.height/2;
    }
    else if ( pos == XFCE_SCREEN_POSITION_SW_H )
    {
	x += widget_allocation.x + widget_allocation.width/2;
	y -= popup_allocation.height;
    }
    else if ( pos == XFCE_SCREEN_POSITION_SE_H )
    {
	x += widget_allocation.x + widget_allocation.width;
	y -= popup_allocation.height;
	x -= popup_allocation.width;
    }
    else if ( pos == XFCE_SCREEN_POSITION_S )
    {
	x += widget_allocation.x + widget_allocation.width/2;
	y -= popup_allocation.height;
	x -= popup_allocation.width/2;
    }
    else if ( pos == XFCE_SCREEN_POSITION_FLOATING_H )
    {
	x += widget_allocation.x + widget_allocation.width/2;
	x -= popup_allocation.width/2;
	if ( y > popup_allocation.height )
	    y -= popup_allocation.height;
	else
	     y += widget_allocation.height;
    }
    else if ( pos == XFCE_SCREEN_POSITION_FLOATING_V )
    {
	y -= popup_allocation.height/2;
	y += widget_allocation.y + widget_allocation.height/2;
	if ( x < popup_allocation.width )
	    x += widget_allocation.width;
	else
	    x -= popup_allocation.width;
    }
    else
    {
	battery_button_release_grab (button, (GdkEventButton *)ev);
	g_return_val_if_reached (FALSE);
    }

    gtk_window_move (GTK_WINDOW(button->priv->popup), x, y);
    TRACE("Displaying window on x=%d y=%d", x, y);

    button->priv->popup_open = TRUE;
    return TRUE;
}

static gboolean
battery_button_press_event (GtkWidget *widget, GdkEventButton *ev)
{
    return battery_button_popup_win (widget, (GdkEvent *) ev, ev->time);
}

static gchar*
get_device_description (UpClient *upower, UpDevice *device)
{
    gchar *tip = NULL;
    gchar *est_time_str = NULL;
    guint type = 0, state = 0;
    gchar *model = NULL, *vendor = NULL;
    gboolean online;
    gboolean present;
    gdouble percentage;
    guint64 time_to_empty, time_to_full;

    /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
    g_object_get (device,
		  "kind", &type,
		  "vendor", &vendor,
		  "model", &model,
		  "state", &state,
		  "is-present", &present,
		  "percentage", &percentage,
		  "time-to-empty", &time_to_empty,
		  "time-to-full", &time_to_full,
		  "online", &online,
		   NULL);


    if (type == UP_DEVICE_KIND_LINE_POWER)
    {
	if ( online )
	{
	    tip = g_strdup_printf(_("<b>Plugged In</b>\t"));
	}
	else
	{
	    tip = g_strdup_printf(_("<b>On Battery</b>\t"));
	}

	return tip;
    }

    if ( state == UP_DEVICE_STATE_FULLY_CHARGED )
    {
	if ( time_to_empty > 0 )
	{
	    est_time_str = xfpm_battery_get_time_string (time_to_empty);
	    tip = g_strdup_printf (_("<b>%s %s</b>\t\nFully charged (%0.0f%%).\t\nProvides %s runtime\t"),
				   vendor, model,
				   percentage,
				   est_time_str);
	    g_free (est_time_str);
	}
	else
	{
	    tip = g_strdup_printf (_("<b>%s %s</b>\t\nFully charged (%0.0f%%).\t"),
				   vendor, model,
				   percentage);
	}
    }
    else if ( state == UP_DEVICE_STATE_CHARGING )
    {
	if ( time_to_full != 0 )
	{
	    est_time_str = xfpm_battery_get_time_string (time_to_full);
	    tip = g_strdup_printf (_("<b>%s %s</b>\t\nCharging (%0.0f%%).\t\n%s until fully charged.\t"),
				   vendor, model,
				   percentage,
				   est_time_str);
	    g_free (est_time_str);
	}
	else
	{
	    tip = g_strdup_printf (_("<b>%s %s</b>\t\nCharging (%0.0f%%).\t"),
				   vendor, model,
				   percentage);
	}
    }
    else if ( state == UP_DEVICE_STATE_DISCHARGING )
    {
	if ( time_to_empty != 0 )
	{
	    est_time_str = xfpm_battery_get_time_string (time_to_empty);
	    tip = g_strdup_printf (_("<b>%s %s</b>\t\nDischarging (%0.0f%%).\t\nEstimated time left is %s.\t"),
				   vendor, model,
				   percentage,
				   est_time_str);
	    g_free (est_time_str);
	}
	else
	{
	    tip = g_strdup_printf (_("<b>%s %s</b>\t\nDischarging (%0.0f%%).\t"),
				   vendor, model,
				   percentage);
	}

    }
    else if ( state == UP_DEVICE_STATE_PENDING_CHARGE )
    {
	tip = g_strdup_printf (_("<b>%s %s</b>\t\nWaiting to discharge (%0.0f%%).\t"),
			       vendor, model,
			       percentage);
    }
    else if ( state == UP_DEVICE_STATE_PENDING_DISCHARGE )
    {
	tip = g_strdup_printf (_("<b>%s %s</b>\t\nWaiting to charge (%0.0f%%).\t"),
	                       vendor, model,
			       percentage);
    }
    else if ( state == UP_DEVICE_STATE_EMPTY )
    {
	tip = g_strdup_printf (_("<b>%s %s</b>\t\nis empty\t"),
	                       vendor, model);
    }
    else
    {
	/* unknown device state, just display the percentage */
	tip = g_strdup_printf (_("<b>%s %s</b>\t\nis at (%0.0f%%).\t"),
			       vendor, model,
			       percentage);
    }

    return tip;
}

/* Call gtk_tree_iter_free when done with the tree iter */
static GtkTreeIter*
find_device_in_tree (BatteryButton *button, const gchar *object_path)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    TRACE("entering");

    g_return_val_if_fail ( BATTERY_IS_BUTTON(button), NULL );

    if ( !button->priv->treeview )
	return NULL;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(button->priv->treeview));

    if (!model)
	return NULL;

    if(gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gchar *path = NULL;
            gtk_tree_model_get(model, &iter, COL_OBJ_PATH, &path, -1);

            if(g_strcmp0(path, object_path) == 0) {
                g_free(path);
                return gtk_tree_iter_copy(&iter);
            }

            g_free(path);
        } while(gtk_tree_model_iter_next(model, &iter));
    }

    return NULL;
}

static void
#if UP_CHECK_VERSION(0, 99, 0)
device_changed_cb (UpDevice *device, GParamSpec *pspec, BatteryButton *button)
#else
device_changed_cb (UpDevice *device, BatteryButton *button)
#endif
{
    GtkTreeIter *iter;
    const gchar *object_path = up_device_get_object_path(device);
    gchar *details, *icon_name;
    GdkPixbuf *pix;
    guint type = 0;

    TRACE("entering for %s", object_path);

    g_return_if_fail ( BATTERY_IS_BUTTON (button) );

    iter = find_device_in_tree (button, object_path);

    if (iter == NULL)
	return;

    /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
    g_object_get (device,
		  "kind", &type,
		   NULL);

    icon_name = get_device_icon_name (button->priv->upower, device);
    details = get_device_description(button->priv->upower, device);

    pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
				    icon_name,
				    48,
				    GTK_ICON_LOOKUP_USE_BUILTIN,
				    NULL);

    gtk_list_store_set (GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(button->priv->treeview))), iter,
			COL_NAME, details,
			COL_ICON, pix,
			-1);

    gtk_tree_iter_free (iter);

    if ( type == UP_DEVICE_KIND_LINE_POWER )
    {
	/* Update the panel icon */
	button->priv->panel_icon_name = icon_name;
	battery_button_set_icon (button);
    }
}

static void
battery_button_add_device (UpDevice *device, BatteryButton *button)
{
    GtkListStore *list_store;
    GtkTreeIter iter, *device_iter;
    GdkPixbuf *pix;
    guint type = 0;
    gchar *details, *icon_name;
    const gchar *object_path = up_device_get_object_path(device);
    gulong signal_id;

    TRACE("entering for %s", object_path);

    g_return_if_fail ( BATTERY_IS_BUTTON (button ) );

    /* don't add the same device twice */
    device_iter = find_device_in_tree (button, object_path);
    if (device_iter)
    {
	gtk_tree_iter_free (device_iter);
	return;
    }

    /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
    g_object_get (device,
		  "kind", &type,
		   NULL);

    icon_name = get_device_icon_name (button->priv->upower, device);
    details = get_device_description(button->priv->upower, device);

    list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (button->priv->treeview)));

    pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
				    icon_name,
				    48,
				    GTK_ICON_LOOKUP_USE_BUILTIN,
				    NULL);

#if UP_CHECK_VERSION(0, 99, 0)
    signal_id = g_signal_connect (device, "notify", G_CALLBACK (device_changed_cb), button);
#else
    signal_id = g_signal_connect (device, "changed", G_CALLBACK (device_changed_cb), button);
#endif

    if ( type == UP_DEVICE_KIND_LINE_POWER )
    {
	/* The PC's plugged in status shows up first */
	gtk_list_store_prepend (list_store, &iter);

	/* Update the panel icon */
	button->priv->panel_icon_name = icon_name;
	battery_button_set_icon (button);
    }
    else
    {
	gtk_list_store_append (list_store, &iter);
    }
    gtk_list_store_set (list_store, &iter,
			COL_ICON, pix,
			COL_NAME, details,
			COL_OBJ_PATH, object_path,
			COL_OBJ_SIGNAL_ID, signal_id,
			COL_OBJ_DEVICE_POINTER, device,
			-1);

    if ( pix )
	g_object_unref (pix);
}

static void
battery_button_remove_device (BatteryButton *button, const gchar *object_path)
{
    GtkTreeIter *iter;
    GtkListStore *list_store;
    gulong signal_id;
    UpDevice *device;

    TRACE("entering for %s", object_path);

    iter = find_device_in_tree (button, object_path);

    if (iter == NULL)
	return;

    list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(button->priv->treeview)));

    gtk_tree_model_get (GTK_TREE_MODEL(list_store), iter,
			COL_OBJ_SIGNAL_ID, &signal_id,
			COL_OBJ_DEVICE_POINTER, &device,
			-1);

    gtk_list_store_remove (list_store, iter);

    gtk_tree_iter_free (iter);

    if (device)
	g_signal_handler_disconnect (device, signal_id);
}

static void
device_added_cb (UpClient *upower, UpDevice *device, BatteryButton *button)
{
    battery_button_add_device (device, button);
}

#if UP_CHECK_VERSION(0, 99, 0)
static void
device_removed_cb (UpClient *upower, const gchar *object_path, BatteryButton *button)
{
    battery_button_remove_device (button, object_path);
}
#else
static void
device_removed_cb (UpClient *upower, UpDevice *device, BatteryButton *button)
{
    const gchar *object_path = up_device_get_object_path(device);
    battery_button_remove_device (button, object_path);
}
#endif


static void
battery_button_add_all_devices (BatteryButton *button)
{
#if !UP_CHECK_VERSION(0, 99, 0)
    /* the device-add callback is called for each device */
    up_client_enumerate_devices_sync(button->priv->upower, NULL, NULL);
#else
    GPtrArray *array = NULL;
    guint i;

    array = up_client_get_devices(button->priv->upower);

    if ( array )
    {
	for ( i = 0; i < array->len; i++)
	{
	    UpDevice *device = g_ptr_array_index (array, i);

	    battery_button_add_device (device, button);
	}
	g_ptr_array_free (array, TRUE);
    }
#endif
}

static void
battery_button_create_popup (BatteryButton *button)
{
    GtkOrientation orientation;
    GtkWidget *box;
    GtkListStore *list_store;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;

    button->priv->popup = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_decorated (GTK_WINDOW(button->priv->popup), FALSE);

    g_signal_connect (button->priv->popup, "grab-notify",
		      G_CALLBACK (battery_button_popup_grab_notify), button);
    g_signal_connect (button->priv->popup, "grab-broken-event",
		      G_CALLBACK (battery_button_popup_broken_event), button);
    g_signal_connect (button->priv->popup, "key_release_event",
		      G_CALLBACK (battery_button_popup_key_release_event), button);
    g_signal_connect (button->priv->popup , "button_press_event",
		      G_CALLBACK (battery_button_popup_button_press_event), button);

    orientation = xfce_panel_plugin_get_orientation (button->priv->plugin);

    if ( orientation == GTK_ORIENTATION_VERTICAL)
	box = gtk_hbox_new (FALSE, 1);
    else
	box = gtk_vbox_new (FALSE, 1);

    gtk_container_add (GTK_CONTAINER (button->priv->popup), box);

    button->priv->treeview = gtk_tree_view_new ();
    list_store = gtk_list_store_new (5, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_POINTER);

    gtk_tree_view_set_model (GTK_TREE_VIEW (button->priv->treeview), GTK_TREE_MODEL (list_store));

    /* turn off alternating row colors, themes will probably override this */
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (button->priv->treeview), FALSE);
    col = gtk_tree_view_column_new ();

    renderer = gtk_cell_renderer_pixbuf_new ();

    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "pixbuf", 0, NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "markup", 1, NULL);

    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (button->priv->treeview), FALSE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (button->priv->treeview), col);

    gtk_box_pack_start (GTK_BOX (box), button->priv->treeview, TRUE, TRUE, 0);

    gtk_window_set_type_hint (GTK_WINDOW(button->priv->popup), GDK_WINDOW_TYPE_HINT_UTILITY );

    battery_button_add_all_devices (button);

    gtk_widget_show_all (box);
}

static void
battery_button_class_init (BatteryButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = battery_button_finalize;
    object_class->set_property = battery_button_set_property;

    widget_class->button_press_event = battery_button_press_event;

    g_object_class_install_property (object_class,
				     PROP_PLUGIN,
				     g_param_spec_object ("plugin",
							  NULL,
							  NULL,
							  XFCE_TYPE_PANEL_PLUGIN,
							  G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_WRITABLE));

    g_type_class_add_private (klass, sizeof (BatteryButtonPrivate));
}

static void
battery_button_init (BatteryButton *button)
{
    button->priv = BATTERY_BUTTON_GET_PRIVATE (button);

    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

    button->priv->upower  = up_client_new ();

    g_signal_connect (button->priv->upower, "device-added", G_CALLBACK (device_added_cb), button);
    g_signal_connect (button->priv->upower, "device-removed", G_CALLBACK (device_removed_cb), button);
}

static void
battery_button_finalize (GObject *object)
{
    BatteryButton *button;

    button = BATTERY_BUTTON (object);

    g_signal_handlers_disconnect_by_data (button->priv->upower, button);

    g_object_unref (button->priv->plugin);

    G_OBJECT_CLASS (battery_button_parent_class)->finalize (object);
}

GtkWidget *
battery_button_new (XfcePanelPlugin *plugin)
{
    BatteryButton *button = NULL;
    button = g_object_new (BATTERY_TYPE_BUTTON, "plugin", plugin, NULL);
    return GTK_WIDGET (button);
}

static gboolean
battery_button_set_icon (BatteryButton *button)
{
    GdkPixbuf *pixbuf;

    pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                       button->priv->panel_icon_name,
                                       button->priv->panel_icon_width,
                                       GTK_ICON_LOOKUP_FORCE_SIZE,
                                       NULL);

    if ( pixbuf )
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (button->priv->image), pixbuf);
        g_object_unref (pixbuf);
        return TRUE;
    }

    return FALSE;
}

static void
destroy_popup (BatteryButton *button)
{
    if ( GTK_IS_WIDGET (button->priv->popup) )
	gtk_widget_destroy (button->priv->popup);
}

static gboolean
battery_button_size_changed_cb (XfcePanelPlugin *plugin, gint size, BatteryButton *button)
{
    gint width = size -2 - 2* MAX(gtk_widget_get_style(GTK_WIDGET(button))->xthickness,
                                  gtk_widget_get_style(GTK_WIDGET(button))->xthickness);

    gtk_widget_set_size_request (GTK_WIDGET(plugin), size, size);
    button->priv->panel_icon_width = width;

    return battery_button_set_icon (button);
}

static void
battery_button_free_data_cb (XfcePanelPlugin *plugin, BatteryButton *button)
{
    destroy_popup (button);
    gtk_widget_destroy (GTK_WIDGET (button));
}

static void
help_cb (GtkMenuItem *menuitem, gpointer user_data)
{
    BatteryButton *button = BATTERY_BUTTON (user_data);
    xfce_dialog_show_help (GTK_WINDOW (button->priv->popup), "xfce4-power-manager", "start", NULL);
}

void battery_button_show (BatteryButton *button)
{
    GtkWidget *mi;

    g_return_if_fail (BATTERY_IS_BUTTON (button));

    xfce_panel_plugin_add_action_widget (button->priv->plugin, GTK_WIDGET (button));

    button->priv->image = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (button), button->priv->image);

    /* help dialog */
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_HELP, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate", G_CALLBACK (help_cb), button);

    xfce_panel_plugin_menu_insert_item (button->priv->plugin, GTK_MENU_ITEM (mi));

    /* preferences dialog */
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate",G_CALLBACK (xfpm_preferences), NULL);

    xfce_panel_plugin_menu_insert_item (button->priv->plugin, GTK_MENU_ITEM (mi));

    g_signal_connect (button->priv->plugin, "size-changed",
		      G_CALLBACK (battery_button_size_changed_cb), button);

    g_signal_connect (button->priv->plugin, "free-data",
		      G_CALLBACK (battery_button_free_data_cb), button);

    gtk_widget_show_all (GTK_WIDGET(button));
    battery_button_create_popup (button);
    battery_button_set_tooltip (button);
}
