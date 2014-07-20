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

#include <glib.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <dbus/dbus-glib.h>
#include <upower.h>
#include <xfconf/xfconf.h>

#include "common/xfpm-common.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"
#include "common/xfpm-brightness.h"

#include "battery-button.h"

#define BATTERY_BUTTON_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), BATTERY_TYPE_BUTTON, BatteryButtonPrivate))

struct BatteryButtonPrivate
{
#ifdef XFCE_PLUGIN
    XfcePanelPlugin *plugin;
#endif
#ifdef LXDE_PLUGIN
    Plugin *plugin;
#endif
    XfconfChannel   *channel;

    UpClient        *upower;

    /* A list of BatteryDevices  */
    GList           *devices;

    /* The left-click popup menu, if one is being displayed */
    GtkWidget       *menu;

    /* The actual panel icon image */
    GtkWidget       *panel_icon_image;
    /* Keep track of icon name to redisplay during size changes */
    gchar           *panel_icon_name;
    /* Keep track of the last icon size for use during updates */
    gint             panel_icon_width;

    /* Upower 0.99 has a display device that can be used for the
     * panel image and tooltip description */
    UpDevice        *display_device;

    XfpmBrightness  *brightness;

    /* display brightness slider widget */
    GtkWidget       *range;
};

typedef struct
{
    GdkPixbuf   *pix;          /* Icon */
    gchar       *details;      /* Description of the device + state */
    gchar       *object_path;  /* UpDevice object path */
    UpDevice    *device;       /* Pointer to the UpDevice */
    gulong       signal_id;    /* device changed callback id */
    GtkWidget   *menu_item;    /* The device's item on the menu (if shown) */
} BatteryDevice;

G_DEFINE_TYPE (BatteryButton, battery_button, GTK_TYPE_TOGGLE_BUTTON)

static void battery_button_finalize   (GObject *object);
static GList* find_device_in_list (BatteryButton *button, const gchar *object_path);
static gboolean battery_button_set_icon (BatteryButton *button);
static gboolean battery_button_press_event (GtkWidget *widget, GdkEventButton *event);
static void battery_button_show_menu (BatteryButton *button);
static void battery_button_menu_add_device (BatteryButton *button, BatteryDevice *battery_device, gboolean append);
static void increase_brightness (BatteryButton *button);
static void decrease_brightness (BatteryButton *button);



static BatteryDevice*
get_display_device (BatteryButton *button)
{
    GList *item = NULL;
    gdouble highest_percentage = 0;
    BatteryDevice *display_device = NULL;

    if (button->priv->display_device)
    {
	item = find_device_in_list (button, up_device_get_object_path (button->priv->display_device));
	if (item)
	    return item->data;
    }

    /* We want to find the battery or ups device with the highest percentage
     * and use that to get our tooltip from */
    for (item = g_list_first (button->priv->devices); item != NULL; item = g_list_next (item))
    {
	BatteryDevice *battery_device = item->data;
	guint type = 0;
	gdouble percentage;

	g_object_get (battery_device->device,
		      "kind", &type,
		      "percentage", &percentage,
		      NULL);

	if ( type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS)
	{
	    if ( highest_percentage < percentage )
	    {
		/* found something better */
		display_device = battery_device;
		highest_percentage = percentage;
	    }
	}
    }

    return display_device;
}

static void
battery_button_set_tooltip (BatteryButton *button)
{
    BatteryDevice *display_device = get_display_device (button);

    if ( display_device )
    {
	/* if we have something, display it */
	gtk_widget_set_tooltip_markup (GTK_WIDGET (button), display_device->details);
    }
    else
    {
	/* how did we get here? */
	gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Display battery levels for attached devices"));
    }
}

static GList*
find_device_in_list (BatteryButton *button, const gchar *object_path)
{
    GList *item = NULL;

    TRACE("entering");

    g_return_val_if_fail ( BATTERY_IS_BUTTON(button), NULL );

    for (item = g_list_first (button->priv->devices); item != NULL; item = g_list_next (item))
    {
	BatteryDevice *battery_device = item->data;
	if (g_strcmp0 (battery_device->object_path, object_path) == 0)
	    return item;
    }

    return NULL;
}

static void
battery_button_update_device_icon_and_details (BatteryButton *button, UpDevice *device)
{
    GList *item;
    BatteryDevice *battery_device, *display_device;
    const gchar *object_path = up_device_get_object_path(device);
    gchar *details, *icon_name;
    GdkPixbuf *pix;
    guint type = 0;

    TRACE("entering for %s", object_path);

    g_return_if_fail ( BATTERY_IS_BUTTON (button) );

    item = find_device_in_list (button, object_path);

    if (item == NULL)
	return;

    battery_device = item->data;

    /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
    g_object_get (device,
		  "kind", &type,
		   NULL);

    icon_name = get_device_icon_name (button->priv->upower, device);
    details = get_device_description(button->priv->upower, device);

    pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
				    icon_name,
				    32,
				    GTK_ICON_LOOKUP_USE_BUILTIN,
				    NULL);

    if (battery_device->details)
	g_free(battery_device->details);
    battery_device->details = details;

    if (battery_device->pix)
	g_object_unref (battery_device->pix);
    battery_device->pix = pix;

    /* Get the display device, which may now be this one */
    display_device = get_display_device (button);
    if ( battery_device == display_device)
    {
	DBG("this is the display device, updating");
	/* it is! update the panel button */
	g_free(button->priv->panel_icon_name);
	button->priv->panel_icon_name = icon_name;
	battery_button_set_icon (button);
	/* update tooltip */
	battery_button_set_tooltip (button);
    }

    /* If the menu is being displayed, update it */
    if (button->priv->menu && battery_device->menu_item)
    {
	GtkWidget *img;

	gtk_menu_item_set_label (GTK_MENU_ITEM (battery_device->menu_item), details);

        /* update the image */
        img = gtk_image_new_from_pixbuf(battery_device->pix);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(battery_device->menu_item), img);
    }
}

static void
#if UP_CHECK_VERSION(0, 99, 0)
device_changed_cb (UpDevice *device, GParamSpec *pspec, BatteryButton *button)
#else
device_changed_cb (UpDevice *device, BatteryButton *button)
#endif
{
    battery_button_update_device_icon_and_details (button, device);
}

static void
battery_button_add_device (UpDevice *device, BatteryButton *button)
{
    BatteryDevice *battery_device;
    guint type = 0;
    const gchar *object_path = up_device_get_object_path(device);
    gulong signal_id;

    TRACE("entering for %s", object_path);

    g_return_if_fail ( BATTERY_IS_BUTTON (button ) );

    /* don't add the same device twice */
    if ( find_device_in_list (button, object_path) )
	return;

    battery_device = g_new0 (BatteryDevice, 1);

    /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
    g_object_get (device,
		  "kind", &type,
		   NULL);


#if UP_CHECK_VERSION(0, 99, 0)
    signal_id = g_signal_connect (device, "notify", G_CALLBACK (device_changed_cb), button);
#else
    signal_id = g_signal_connect (device, "changed", G_CALLBACK (device_changed_cb), button);
#endif

    /* populate the struct */
    battery_device->object_path = g_strdup (object_path);
    battery_device->signal_id = signal_id;
    battery_device->device = device;

    /* add it to the list */
    button->priv->devices = g_list_append (button->priv->devices, battery_device);

    /* Add the icon and description for the device */
    battery_button_update_device_icon_and_details (button, device);

    /* If the menu is being shown, add this new device to it */
    if (button->priv->menu)
    {
	battery_button_menu_add_device (button, battery_device, FALSE);
    }
}

static void
battery_button_remove_device (BatteryButton *button, const gchar *object_path)
{
    GList *item;
    BatteryDevice *battery_device;

    TRACE("entering for %s", object_path);

    item = find_device_in_list (button, object_path);

    if (item == NULL)
	return;

    battery_device = item->data;

    /* If it is being shown in the menu, remove it */
    if(battery_device->menu_item && button->priv->menu)
        gtk_container_remove(GTK_CONTAINER(button->priv->menu), battery_device->menu_item);

    if (battery_device->pix)
	g_object_unref (battery_device->pix);

    g_free(battery_device->details);
    g_free(battery_device->object_path);

    if (battery_device->device)
    {
	if (battery_device->signal_id)
	    g_signal_handler_disconnect (battery_device->device, battery_device->signal_id);
	g_object_unref (battery_device->device);
    }

    /* remove it item and free the battery device */
    button->priv->devices = g_list_delete_link (button->priv->devices, item);
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

    button->priv->display_device = up_client_get_display_device (button->priv->upower);
    battery_button_add_device (button->priv->display_device, button);

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
brightness_up (BatteryButton *button)
{
    gint32 level;
    gint32 max_level;

    xfpm_brightness_get_level (button->priv->brightness, &level);
    max_level = xfpm_brightness_get_max_level (button->priv->brightness);

    if ( level < max_level )
    {
        increase_brightness (button);
    }
}

static void
brightness_down (BatteryButton *button)
{
    gint32 level;
    xfpm_brightness_get_level (button->priv->brightness, &level);

    if ( level > 0 )
    {
        decrease_brightness (button);
    }
}

static gboolean
battery_button_scroll_event (GtkWidget *widget, GdkEventScroll *ev)
{
    gboolean hw_found;
    BatteryButton *button;

    button = BATTERY_BUTTON (widget);

    hw_found = xfpm_brightness_has_hw (button->priv->brightness);

    if ( !hw_found )
        return FALSE;

    if ( ev->direction == GDK_SCROLL_UP )
    {
        brightness_up (button);
        return TRUE;
    }
    else if ( ev->direction == GDK_SCROLL_DOWN )
    {
        brightness_down (button);
        return TRUE;
    }
    return FALSE;
}

static void
battery_button_class_init (BatteryButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = battery_button_finalize;

    widget_class->button_press_event = battery_button_press_event;
    widget_class->scroll_event = battery_button_scroll_event;

    g_type_class_add_private (klass, sizeof (BatteryButtonPrivate));
}

static void
battery_button_init (BatteryButton *button)
{
    GError *error = NULL;

    button->priv = BATTERY_BUTTON_GET_PRIVATE (button);

    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

    button->priv->brightness = xfpm_brightness_new ();
    xfpm_brightness_setup (button->priv->brightness);

    button->priv->upower  = up_client_new ();
    if ( !xfconf_init (&error) )
    {
        g_critical ("xfconf_init failed: %s\n", error->message);
        g_error_free (error);
    }
    else
    {
        button->priv->channel = xfconf_channel_get ("xfce4-power-manager");
    }

    /* Sane defaults for the panel icon */
    button->priv->panel_icon_name = g_strdup(XFPM_AC_ADAPTER_ICON);
    button->priv->panel_icon_width = 24;

    g_signal_connect (button->priv->upower, "device-added", G_CALLBACK (device_added_cb), button);
    g_signal_connect (button->priv->upower, "device-removed", G_CALLBACK (device_removed_cb), button);
}

static void
battery_button_finalize (GObject *object)
{
    BatteryButton *button;

    button = BATTERY_BUTTON (object);

    g_free(button->priv->panel_icon_name);

    g_signal_handlers_disconnect_by_data (button->priv->upower, button);

#ifdef XFCE_PLUGIN
    g_object_unref (button->priv->plugin);
#endif

    G_OBJECT_CLASS (battery_button_parent_class)->finalize (object);
}

GtkWidget *
#ifdef XFCE_PLUGIN
battery_button_new (XfcePanelPlugin *plugin)
#endif
#ifdef LXDE_PLUGIN
battery_button_new (Plugin *plugin)
#endif
{
    BatteryButton *button = NULL;
    button = g_object_new (BATTERY_TYPE_BUTTON, NULL, NULL);

#ifdef XFCE_PLUGIN
    button->priv->plugin = XFCE_PANEL_PLUGIN (g_object_ref (plugin));
#endif
#ifdef LXDE_PLUGIN
    button->priv->plugin = plugin;
#endif

    return GTK_WIDGET (button);
}

static gboolean
battery_button_set_icon (BatteryButton *button)
{
    GdkPixbuf *pixbuf;

    DBG("icon_width %d", button->priv->panel_icon_width);

    pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                       button->priv->panel_icon_name,
                                       button->priv->panel_icon_width,
                                       GTK_ICON_LOOKUP_FORCE_SIZE,
                                       NULL);

    if ( pixbuf )
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (button->priv->panel_icon_image), pixbuf);
        g_object_unref (pixbuf);
        return TRUE;
    }

    return FALSE;
}

void
battery_button_set_width (BatteryButton *button, gint width)
{
    g_return_if_fail (BATTERY_IS_BUTTON (button));

    button->priv->panel_icon_width = width;

    battery_button_set_icon (button);
}

static gboolean
battery_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
    BatteryButton *button = BATTERY_BUTTON (widget);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
    battery_button_show_menu (button);
    return TRUE;
}

#ifdef XFCE_PLUGIN
static gboolean
battery_button_size_changed_cb (XfcePanelPlugin *plugin, gint size, BatteryButton *button)
{
    gint width;

    g_return_val_if_fail (BATTERY_IS_BUTTON (button), FALSE);
    g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), FALSE);


    size /= xfce_panel_plugin_get_nrows (plugin);
    width = size -2 - 2* MAX(gtk_widget_get_style(GTK_WIDGET(button))->xthickness,
                             gtk_widget_get_style(GTK_WIDGET(button))->ythickness);

    gtk_widget_set_size_request (GTK_WIDGET(plugin), size, size);
    button->priv->panel_icon_width = width;

    return battery_button_set_icon (button);
}

static void
battery_button_style_set_cb (XfcePanelPlugin *plugin, GtkStyle *prev_style, BatteryButton *button)
{
    gtk_widget_reset_rc_styles (GTK_WIDGET (plugin));
    battery_button_size_changed_cb (plugin, xfce_panel_plugin_get_size (plugin), button);
}

static void
battery_button_free_data_cb (XfcePanelPlugin *plugin, BatteryButton *button)
{
    gtk_widget_destroy (GTK_WIDGET (button));
}
#endif

static void
help_cb (GtkMenuItem *menuitem, gpointer user_data)
{
    xfce_dialog_show_help (NULL, "xfce4-power-manager", "start", NULL);
}

void
battery_button_show (BatteryButton *button)
{
    GtkWidget *mi;

    g_return_if_fail (BATTERY_IS_BUTTON (button));

#ifdef XFCE_PLUGIN
    xfce_panel_plugin_add_action_widget (button->priv->plugin, GTK_WIDGET (button));
    xfce_panel_plugin_set_small (button->priv->plugin, TRUE);
#endif

    button->priv->panel_icon_image = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (button), button->priv->panel_icon_image);

    /* help dialog */
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_HELP, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect (mi, "activate", G_CALLBACK (help_cb), button);

#ifdef XFCE_PLUGIN
    xfce_panel_plugin_menu_insert_item (button->priv->plugin, GTK_MENU_ITEM (mi));

    g_signal_connect (button->priv->plugin, "size-changed",
                      G_CALLBACK (battery_button_size_changed_cb), button);

    g_signal_connect (button->priv->plugin, "style-set",
                      G_CALLBACK (battery_button_style_set_cb), button);

    g_signal_connect (button->priv->plugin, "free-data",
                      G_CALLBACK (battery_button_free_data_cb), button);
#endif

    gtk_widget_show_all (GTK_WIDGET(button));
    battery_button_set_tooltip (button);

    /* Add all the devcies currently attached to the system */
    battery_button_add_all_devices (button);
}

static void
menu_destroyed_cb(GtkMenuShell *menu, gpointer user_data)
{
    BatteryButton *button = BATTERY_BUTTON (user_data);

    TRACE("entering");

    /* menu destroyed, range slider is gone */
    button->priv->range = NULL;

    /* untoggle panel icon */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);

    button->priv->menu = NULL;
}

static void
menu_item_destroyed_cb(GtkWidget *object, gpointer user_data)
{
    BatteryButton *button = BATTERY_BUTTON (user_data);
    GList *item;

    for (item = g_list_first (button->priv->devices); item != NULL; item = g_list_next (item))
    {
        BatteryDevice *battery_device = item->data;

        if (battery_device->menu_item == object)
        {
            battery_device->menu_item = NULL;
            return;
        }
    }
}

static void
menu_item_activate_cb(GtkWidget *object, gpointer user_data)
{
    BatteryButton *button = BATTERY_BUTTON (user_data);
    GList *item;

    for (item = g_list_first (button->priv->devices); item != NULL; item = g_list_next (item))
    {
        BatteryDevice *battery_device = item->data;

        if (battery_device->menu_item == object)
        {
            /* Call xfpm settings with the device id */
            xfpm_preferences_device_id (battery_device->object_path);
            return;
        }
    }
}

static void
battery_button_menu_add_device (BatteryButton *button, BatteryDevice *battery_device, gboolean append)
{
    GtkWidget *mi, *label, *img;
    guint type = 0;

    /* We need a menu to attach it to */
    g_return_if_fail (button->priv->menu);

    g_object_get (battery_device->device,
		  "kind", &type,
		  NULL);

    /* Don't add the display device or line power to the menu */
    if (type == UP_DEVICE_KIND_LINE_POWER || battery_device->device == button->priv->display_device)
	return;

    mi = gtk_image_menu_item_new_with_label(battery_device->details);
    /* Make the menu item be bold and multi-line */
    label = gtk_bin_get_child(GTK_BIN(mi));
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);

    /* add the image */
    img = gtk_image_new_from_pixbuf(battery_device->pix);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);

    /* keep track of the menu item in the battery_device so we can update it */
    battery_device->menu_item = mi;
    g_signal_connect(G_OBJECT(mi), "destroy", G_CALLBACK(menu_item_destroyed_cb), button);

    /* Active calls xfpm settings with the device's id to display details */
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menu_item_activate_cb), button);

    /* Add it to the menu */
    gtk_widget_show(mi);
    if (append)
	gtk_menu_shell_append(GTK_MENU_SHELL(button->priv->menu), mi);
    else
	gtk_menu_shell_prepend(GTK_MENU_SHELL(button->priv->menu), mi);
}

static void
decrease_brightness (BatteryButton *button)
{
    gint32 level;

    TRACE("entering");

    if ( !xfpm_brightness_has_hw (button->priv->brightness) )
        return;

    xfpm_brightness_get_level (button->priv->brightness, &level);

    if ( level > 0 )
    {
        xfpm_brightness_down (button->priv->brightness, &level);
        if (button->priv->range)
            gtk_range_set_value (GTK_RANGE (button->priv->range), level);
    }
}

static void
increase_brightness (BatteryButton *button)
{
    gint32 level, max_level;

    TRACE("entering");

    if ( !xfpm_brightness_has_hw (button->priv->brightness) )
        return;

    max_level = xfpm_brightness_get_max_level (button->priv->brightness);
    xfpm_brightness_get_level (button->priv->brightness, &level);

    if ( level < max_level )
    {
        xfpm_brightness_up (button->priv->brightness, &level);
        if (button->priv->range)
            gtk_range_set_value (GTK_RANGE (button->priv->range), level);
    }
}

static void
range_value_changed_cb (GtkWidget *widget, BatteryButton *button)
{
    gint32 range_level, hw_level;

    TRACE("entering");

    range_level = (gint32) gtk_range_get_value (GTK_RANGE (button->priv->range));

    xfpm_brightness_get_level (button->priv->brightness, &hw_level);

    if ( hw_level != range_level )
    {
        xfpm_brightness_set_level (button->priv->brightness, range_level);
    }
}

static void
range_scroll_cb (GtkWidget *widget, GdkEvent *event, BatteryButton *button)
{
    GdkEventScroll *scroll_event;

    TRACE("entering");

    scroll_event = (GdkEventScroll*)event;

    if (scroll_event->direction == GDK_SCROLL_UP)
        increase_brightness (button);
    else if (scroll_event->direction == GDK_SCROLL_DOWN)
        decrease_brightness (button);
}

static void
range_show_cb (GtkWidget *widget, BatteryButton *button)
{
    TRACE("entering");
    /* Release these grabs they will cause a lockup if pkexec is called
     * for the brightness helper */
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
    gtk_grab_remove(widget);
}

static void
battery_button_show_menu (BatteryButton *button)
{
    GtkWidget *menu, *mi, *vbox, *hbox, *img = NULL, *label;
    GdkScreen *gscreen;
    GList *item;
    gboolean show_separator_flag = FALSE;
    gint32 max_level, current_level = 0;

    if(gtk_widget_has_screen(GTK_WIDGET(button)))
        gscreen = gtk_widget_get_screen(GTK_WIDGET(button));
    else
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());

    menu = gtk_menu_new ();
    gtk_menu_set_screen(GTK_MENU(menu), gscreen);
    /* keep track of the menu while it's being displayed */
    button->priv->menu = menu;
    g_signal_connect(GTK_MENU_SHELL(menu), "deactivate", G_CALLBACK(menu_destroyed_cb), button);

    for (item = g_list_first (button->priv->devices); item != NULL; item = g_list_next (item))
    {
        BatteryDevice *battery_device = item->data;

        battery_button_menu_add_device (button, battery_device, TRUE);
        /* If we add an item to the menu, show the separator */
        show_separator_flag = TRUE;
    }

    if (show_separator_flag)
    {
        /* separator */
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }

    /* Display brightness slider - show if there's hardware support for it */
    if ( xfpm_brightness_has_hw (button->priv->brightness) )
    {
        GdkPixbuf *pix;

        mi = gtk_image_menu_item_new ();
        /* attempt to load and display the brightness icon */
        pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                       XFPM_DISPLAY_BRIGHTNESS_ICON,
                                       button->priv->panel_icon_width,
                                       GTK_ICON_LOOKUP_FORCE_SIZE,
                                       NULL);
        if (pix)
        {
            img = gtk_image_new_from_pixbuf (pix);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(mi), img);
        }

        vbox = gtk_vbox_new (FALSE, 2);
        hbox = gtk_hbox_new (FALSE, 2);

        /* Make a display brightness label and place it above the
         * brightness slider */
        label = gtk_label_new (_("Display Brightness"));
        /* align left */
        gtk_misc_set_alignment (GTK_MISC(label), 0, 0);

        max_level = xfpm_brightness_get_max_level (button->priv->brightness);

        /* range slider */
        button->priv->range = gtk_hscale_new_with_range (0, max_level, 1);
        gtk_widget_set_size_request (button->priv->range, 100, -1);
        gtk_range_set_inverted (GTK_RANGE(button->priv->range), FALSE);
        gtk_scale_set_draw_value (GTK_SCALE(button->priv->range), FALSE);
        gtk_widget_set_can_focus (button->priv->range, TRUE);

        /* update the slider to the current brightness level */
        xfpm_brightness_get_level (button->priv->brightness, &current_level);
        gtk_range_set_value (GTK_RANGE(button->priv->range), current_level);
        g_signal_connect (button->priv->range, "value-changed", G_CALLBACK (range_value_changed_cb), button);
        g_signal_connect (mi, "scroll-event", G_CALLBACK (range_scroll_cb), button);
        g_signal_connect (menu, "show", G_CALLBACK (range_show_cb), button);

        /* pack the label and slider */
        gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
        /* pack in the brightness slider */
        gtk_box_pack_start (GTK_BOX(hbox), button->priv->range, TRUE, TRUE, 0);
        /* add it to the menu */
        gtk_container_add (GTK_CONTAINER (mi), vbox);
        gtk_widget_show_all (mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }

    /* Presentation mode checkbox */
    mi = gtk_check_menu_item_new_with_mnemonic (_("Presentation _mode"));
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    xfconf_g_property_bind(button->priv->channel,
                           "/xfce4-power-manager/presentation-mode",
                           G_TYPE_BOOLEAN, G_OBJECT(mi), "active");

    /* Power manager settings */
    mi = gtk_menu_item_new_with_mnemonic (_("_Power manager..."));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(xfpm_preferences), NULL);

    gtk_menu_popup (GTK_MENU (menu),
                    NULL,
                    NULL,
#ifdef XFCE_PLUGIN
                    xfce_panel_plugin_position_menu,
#else
                    NULL,
#endif
                    button->priv->plugin,
                    0,
                    gtk_get_current_event_time ());
}
