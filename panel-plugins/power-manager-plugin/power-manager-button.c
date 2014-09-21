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
#include "common/xfpm-config.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"
#include "common/xfpm-brightness.h"

#include "power-manager-button.h"
#include "scalemenuitem.h"


#define SET_LEVEL_TIMEOUT (50)
#define SAFE_SLIDER_MIN_LEVEL (5)

#define POWER_MANAGER_BUTTON_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), POWER_MANAGER_TYPE_BUTTON, PowerManagerButtonPrivate))

struct PowerManagerButtonPrivate
{
#ifdef XFCE_PLUGIN
    XfcePanelPlugin *plugin;
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
    /* Some laptops (and mostly newer ones with intel graphics) can turn off the
     * backlight completely. If the user is not careful and sets the brightness
     * very low using the slider, he might not be able to see the screen contents
     * anymore. Brightness keys do not work on every laptop, so it's better to use
     * a safe default minimum level that the user can change via the settings
     * editor if desired.
     */
    gint32           brightness_min_level;

    /* filter range value changed events for snappier UI feedback */
    guint            set_level_timeout;
};

typedef struct
{
    GdkPixbuf   *pix;               /* Icon */
    GtkWidget   *img;               /* Icon image in the menu */
    gchar       *details;           /* Description of the device + state */
    gchar       *object_path;       /* UpDevice object path */
    UpDevice    *device;            /* Pointer to the UpDevice */
    gulong       changed_signal_id; /* device changed callback id */
    gulong       expose_signal_id;  /* expose-event callback id */
    GtkWidget   *menu_item;         /* The device's item on the menu (if shown) */
} BatteryDevice;

typedef enum
{
    PROP_0 = 0,
    PROP_BRIGHTNESS_MIN_LEVEL,
} POWER_MANAGER_BUTTON_PROPERTIES;


G_DEFINE_TYPE (PowerManagerButton, power_manager_button, GTK_TYPE_TOGGLE_BUTTON)

static void power_manager_button_finalize   (GObject *object);
static GList* find_device_in_list (PowerManagerButton *button, const gchar *object_path);
static gboolean power_manager_button_device_icon_expose (GtkWidget *img, GdkEventExpose *event, gpointer userdata);
static gboolean power_manager_button_set_icon (PowerManagerButton *button);
static gboolean power_manager_button_press_event (GtkWidget *widget, GdkEventButton *event);
static void power_manager_button_show_menu (PowerManagerButton *button);
static gboolean power_manager_button_menu_add_device (PowerManagerButton *button, BatteryDevice *battery_device, gboolean append);
static void increase_brightness (PowerManagerButton *button);
static void decrease_brightness (PowerManagerButton *button);
static void battery_device_remove_pix (BatteryDevice *battery_device);


static BatteryDevice*
get_display_device (PowerManagerButton *button)
{
    GList *item = NULL;
    gdouble highest_percentage = 0;
    BatteryDevice *display_device = NULL;

    TRACE("entering");

    g_return_val_if_fail ( POWER_MANAGER_IS_BUTTON(button), NULL );

    if (button->priv->display_device)
    {
        item = find_device_in_list (button, up_device_get_object_path (button->priv->display_device));
        if (item)
        {
            return item->data;
        }
    }

    /* We want to find the battery or ups device with the highest percentage
     * and use that to get our tooltip from */
    for (item = g_list_first (button->priv->devices); item != NULL; item = g_list_next (item))
    {
        BatteryDevice *battery_device = item->data;
        guint type = 0;
        gdouble percentage;

        if (!battery_device->device || !UP_IS_DEVICE(battery_device->device))
        {
            continue;
        }

        g_object_get (battery_device->device,
                      "kind", &type,
                      "percentage", &percentage,
                      NULL);

        if ( type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS)
        {
            if ( highest_percentage < percentage )
            {
                display_device = battery_device;
                highest_percentage = percentage;
            }
        }
    }

    return display_device;
}

static void
power_manager_button_set_tooltip (PowerManagerButton *button)
{
    BatteryDevice *display_device = get_display_device (button);

    TRACE("entering");

    if (!GTK_IS_WIDGET (button))
    {
        g_critical ("power_manager_button_set_tooltip: !GTK_IS_WIDGET (button)");
        return;
    }

    if ( display_device )
    {
        /* if we have something, display it */
        if( display_device->details )
        {
            gtk_widget_set_tooltip_markup (GTK_WIDGET (button), display_device->details);
            return;
        }
    }

    /* Odds are this is a desktop without any batteries attached */
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Display battery levels for attached devices"));
}

static GList*
find_device_in_list (PowerManagerButton *button, const gchar *object_path)
{
    GList *item = NULL;

    TRACE("entering");

    g_return_val_if_fail ( POWER_MANAGER_IS_BUTTON(button), NULL );

    for (item = g_list_first (button->priv->devices); item != NULL; item = g_list_next (item))
    {
        BatteryDevice *battery_device = item->data;
        if (battery_device == NULL)
        {
            DBG("!battery_device");
            continue;
        }

        if (g_strcmp0 (battery_device->object_path, object_path) == 0)
            return item;
    }

    return NULL;
}

static gboolean
power_manager_button_device_icon_expose (GtkWidget *img, GdkEventExpose *event, gpointer userdata)
{
    cairo_t *cr;
    UpDevice *device = NULL;
    guint type = 0, state = 0;
    gdouble percentage;
    gint height, width;
    gdouble min_height = 2;
    PangoLayout *layout = NULL;
    PangoRectangle ink_extent, log_extent;

    TRACE("entering");

    /* sanity checks */
    if (!img || !GTK_IS_WIDGET (img))
        return FALSE;

    if (UP_IS_DEVICE (userdata))
    {
        device = UP_DEVICE(userdata);

        g_object_get (device,
                      "kind", &type,
                      "state", &state,
                      "percentage", &percentage,
                      NULL);

        /* Don't draw the progressbar for Battery and UPS */
        if (type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS)
            return FALSE;
    }
    else
    {
        /* If the UpDevice hasn't fully updated yet it then we'll want
         * a question mark for sure. */
        state = UP_DEVICE_STATE_UNKNOWN;
    }

    cr = gdk_cairo_create (img->window);
    width = img->allocation.width;
    height = img->allocation.height;

    if (state != UP_DEVICE_STATE_UNKNOWN)
    {
        /* Draw the trough of the progressbar */
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_line_width (cr, 1.0);
        cairo_rectangle (cr, width - 3.5, img->allocation.y + 1.5, 5, height - 2);
        cairo_set_source_rgb (cr, 0.87, 0.87, 0.87);
        cairo_fill_preserve (cr);
        cairo_set_source_rgb (cr, 0.53, 0.54, 0.52);
        cairo_stroke (cr);

        /* Draw the fill of the progressbar
           Use yellow for 20% and below, green for 100%, red for 5% and below and blue for the rest */
        cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

        if ((height * (percentage / 100)) > min_height)
           min_height = (height - 3) * (percentage / 100);

        cairo_rectangle (cr, width - 3, img->allocation.y + height - min_height - 1, 4, min_height);
        if (percentage > 5 && percentage < 20)
            cairo_set_source_rgb (cr, 0.93, 0.83, 0.0);
        else if (percentage > 20 && percentage < 100)
            cairo_set_source_rgb (cr, 0.2, 0.4, 0.64);
        else if (percentage == 100)
            cairo_set_source_rgb (cr, 0.45, 0.82, 0.08);
        else
            cairo_set_source_rgb (cr, 0.94, 0.16, 0.16);
        cairo_fill (cr);

        cairo_rectangle (cr, width - 2.5, img->allocation.y + 2.5, 3, height - 4);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.75);
        cairo_stroke (cr);
    }
    else
    {
        /* Draw a bubble with a question mark for devices with unknown state */
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_line_width (cr, 1.0);
        cairo_arc(cr, width - 4.5, img->allocation.y + 6.5, 6, 0, 2*3.14159);
        cairo_set_source_rgb (cr, 0.2, 0.54, 0.9);
        cairo_fill_preserve (cr);
        cairo_set_source_rgb (cr, 0.1, 0.37, 0.6);
        cairo_stroke (cr);

        layout = gtk_widget_create_pango_layout (GTK_WIDGET (img), "?");
        pango_layout_set_font_description (layout, pango_font_description_from_string ("Sans Bold 9"));
        pango_layout_get_pixel_extents (layout, &ink_extent, &log_extent);
        cairo_move_to (cr, (width - 5.5) - (log_extent.width / 2), (img->allocation.y + 5.5) - (log_extent.height / 2));
        cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
        pango_cairo_show_layout (cr, layout);
    }

    cairo_destroy (cr);
    if (layout)
        g_object_unref (layout);
    return FALSE;
}


static void
power_manager_button_update_device_icon_and_details (PowerManagerButton *button, UpDevice *device)
{
    GList *item;
    BatteryDevice *battery_device, *display_device;
    const gchar *object_path = up_device_get_object_path(device);
    gchar *details, *icon_name;
    GdkPixbuf *pix;
    guint type = 0;

    TRACE("entering for %s", object_path);

    g_return_if_fail ( POWER_MANAGER_IS_BUTTON (button) );

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

    /* If we had an image before, remove it and the callback */
    battery_device_remove_pix(battery_device);

    battery_device->pix = pix;

    /* Get the display device, which may now be this one */
    display_device = get_display_device (button);
    if ( battery_device == display_device)
    {
	DBG("this is the display device, updating");
	/* it is! update the panel button */
	g_free(button->priv->panel_icon_name);
	button->priv->panel_icon_name = icon_name;
	power_manager_button_set_icon (button);
	/* update tooltip */
	power_manager_button_set_tooltip (button);
    }

    /* If the menu is being displayed, update it */
    if (button->priv->menu && battery_device->menu_item)
    {
        gtk_menu_item_set_label (GTK_MENU_ITEM (battery_device->menu_item), details);

        /* update the image, keep track of the signal ids and the img
         * so we can disconnect it later */
        battery_device->img = gtk_image_new_from_pixbuf(battery_device->pix);
        g_object_ref (battery_device->img);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(battery_device->menu_item), battery_device->img);
        battery_device->expose_signal_id = g_signal_connect_after (G_OBJECT (battery_device->img),
                                                                   "expose-event",
                                                                   G_CALLBACK (power_manager_button_device_icon_expose),
                                                                   device);
    }
}

static void
#if UP_CHECK_VERSION(0, 99, 0)
device_changed_cb (UpDevice *device, GParamSpec *pspec, PowerManagerButton *button)
#else
device_changed_cb (UpDevice *device, PowerManagerButton *button)
#endif
{
    power_manager_button_update_device_icon_and_details (button, device);
}

static void
power_manager_button_add_device (UpDevice *device, PowerManagerButton *button)
{
    BatteryDevice *battery_device;
    guint type = 0;
    const gchar *object_path = up_device_get_object_path(device);
    gulong signal_id;

    TRACE("entering for %s", object_path);

    g_return_if_fail ( POWER_MANAGER_IS_BUTTON (button ) );

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
    battery_device->changed_signal_id = signal_id;
    battery_device->device = g_object_ref(device);

    /* add it to the list */
    button->priv->devices = g_list_append (button->priv->devices, battery_device);

    /* Add the icon and description for the device */
    power_manager_button_update_device_icon_and_details (button, device);

    /* If the menu is being shown, add this new device to it */
    if (button->priv->menu)
    {
	power_manager_button_menu_add_device (button, battery_device, FALSE);
    }
}

/* This function unrefs the pix and img from the battery device and
 * disconnects the expose-event callback on the img.
 */
static void
battery_device_remove_pix (BatteryDevice *battery_device)
{
    TRACE("entering");

    if (battery_device == NULL)
        return;

    if (G_IS_OBJECT(battery_device->pix))
    {
        if (GTK_IS_WIDGET(battery_device->img))
        {
            if (battery_device->expose_signal_id != 0)
            {
                g_signal_handler_disconnect (battery_device->img, battery_device->expose_signal_id);
                battery_device->expose_signal_id = 0;
            }
            g_object_unref (battery_device->img);
            battery_device->img = NULL;
        }
        battery_device->pix = NULL;
    }
}

static void
power_manager_button_remove_device (PowerManagerButton *button, const gchar *object_path)
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

    g_free(battery_device->details);
    g_free(battery_device->object_path);

    if (battery_device->device != NULL && UP_IS_DEVICE(battery_device->device))
    {
        /* disconnect the signal handler if we were using it */
        if (battery_device->changed_signal_id != 0)
            g_signal_handler_disconnect (battery_device->device, battery_device->changed_signal_id);
        battery_device->changed_signal_id = 0;

        g_object_unref (battery_device->device);
        battery_device->device = NULL;
    }

    /* remove it item and free the battery device */
    button->priv->devices = g_list_delete_link (button->priv->devices, item);
}

static void
device_added_cb (UpClient *upower, UpDevice *device, PowerManagerButton *button)
{
    power_manager_button_add_device (device, button);
}

#if UP_CHECK_VERSION(0, 99, 0)
static void
device_removed_cb (UpClient *upower, const gchar *object_path, PowerManagerButton *button)
{
    power_manager_button_remove_device (button, object_path);
}
#else
static void
device_removed_cb (UpClient *upower, UpDevice *device, PowerManagerButton *button)
{
    const gchar *object_path = up_device_get_object_path(device);
    power_manager_button_remove_device (button, object_path);
}
#endif

static void
power_manager_button_add_all_devices (PowerManagerButton *button)
{
#if !UP_CHECK_VERSION(0, 99, 0)
    /* the device-add callback is called for each device */
    up_client_enumerate_devices_sync(button->priv->upower, NULL, NULL);
#else
    GPtrArray *array = NULL;
    guint i;

    button->priv->display_device = up_client_get_display_device (button->priv->upower);
    power_manager_button_add_device (button->priv->display_device, button);

    array = up_client_get_devices(button->priv->upower);

    if ( array )
    {
	for ( i = 0; i < array->len; i++)
	{
	    UpDevice *device = g_ptr_array_index (array, i);

	    power_manager_button_add_device (device, button);
	}
	g_ptr_array_free (array, TRUE);
    }
#endif
}

static void
brightness_up (PowerManagerButton *button)
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
brightness_down (PowerManagerButton *button)
{
    gint32 level;
    xfpm_brightness_get_level (button->priv->brightness, &level);

    if ( level > button->priv->brightness_min_level )
    {
        decrease_brightness (button);
    }
}

static gboolean
power_manager_button_scroll_event (GtkWidget *widget, GdkEventScroll *ev)
{
    gboolean hw_found;
    PowerManagerButton *button;

    button = POWER_MANAGER_BUTTON (widget);

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
set_brightness_min_level(PowerManagerButton *button, gint32 new_brightness_level)
{
    gint32 max_level = xfpm_brightness_get_max_level (button->priv->brightness);

    /* sanity check */
    if (new_brightness_level > max_level)
        new_brightness_level = -1;

    /* -1 = auto, we set the step value to a hopefully sane default */
    if (new_brightness_level == -1)
    {
        button->priv->brightness_min_level = (max_level > 100) ? SAFE_SLIDER_MIN_LEVEL : 0;
    }
    else
    {
        button->priv->brightness_min_level = new_brightness_level;
    }

    DBG("button->priv->brightness_min_level : %d", button->priv->brightness_min_level);

    /* update the range if it's being shown */
    if (button->priv->range)
    {
        gtk_range_set_range (GTK_RANGE(button->priv->range), button->priv->brightness_min_level, max_level);
    }
}

static void
power_manager_button_set_property(GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    PowerManagerButton *button;

    button = POWER_MANAGER_BUTTON (object);

    switch(property_id)
    {
        case PROP_BRIGHTNESS_MIN_LEVEL:
            set_brightness_min_level (button, g_value_get_int(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
power_manager_button_get_property(GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    PowerManagerButton *button;

    button = POWER_MANAGER_BUTTON (object);

    switch(property_id)
    {
        case PROP_BRIGHTNESS_MIN_LEVEL:
            g_value_set_int(value, button->priv->brightness_min_level);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
power_manager_button_class_init (PowerManagerButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = power_manager_button_finalize;
    object_class->set_property = power_manager_button_set_property;
    object_class->get_property = power_manager_button_get_property;

    widget_class->button_press_event = power_manager_button_press_event;
    widget_class->scroll_event = power_manager_button_scroll_event;

    g_type_class_add_private (klass, sizeof (PowerManagerButtonPrivate));

#define XFPM_PARAM_FLAGS  (G_PARAM_READWRITE \
                           | G_PARAM_CONSTRUCT \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)

    /* We allow and default to -1 only so that we can automagically set a
     * sane value if the user hasn't selected one already */
    g_object_class_install_property(object_class, PROP_BRIGHTNESS_MIN_LEVEL,
                                    g_param_spec_int(BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                     BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                     BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                     -1, G_MAXINT32, -1,
                                                     XFPM_PARAM_FLAGS));
#undef XFPM_PARAM_FLAGS
}

static void
power_manager_button_init (PowerManagerButton *button)
{
    GError *error = NULL;

    button->priv = POWER_MANAGER_BUTTON_GET_PRIVATE (button);

    gtk_widget_set_can_default (GTK_WIDGET (button), FALSE);
    gtk_widget_set_can_focus (GTK_WIDGET (button), FALSE);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);

    button->priv->brightness = xfpm_brightness_new ();
    xfpm_brightness_setup (button->priv->brightness);
    button->priv->set_level_timeout = 0;

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
power_manager_button_finalize (GObject *object)
{
    PowerManagerButton *button;

    button = POWER_MANAGER_BUTTON (object);

    g_free(button->priv->panel_icon_name);

    if (button->priv->set_level_timeout)
    {
        g_source_remove(button->priv->set_level_timeout);
        button->priv->set_level_timeout = 0;
    }

    g_signal_handlers_disconnect_by_data (button->priv->upower, button);

#ifdef XFCE_PLUGIN
    g_object_unref (button->priv->plugin);
#endif

    G_OBJECT_CLASS (power_manager_button_parent_class)->finalize (object);
}

GtkWidget *
#ifdef XFCE_PLUGIN
power_manager_button_new (XfcePanelPlugin *plugin)
#endif
#ifdef LXDE_PLUGIN
power_manager_button_new (void)
#endif
{
    PowerManagerButton *button = NULL;
    button = g_object_new (POWER_MANAGER_TYPE_BUTTON, NULL, NULL);

#ifdef XFCE_PLUGIN
    button->priv->plugin = XFCE_PANEL_PLUGIN (g_object_ref (plugin));
#endif

    xfconf_g_property_bind(button->priv->channel,
                           PROPERTIES_PREFIX BRIGHTNESS_SLIDER_MIN_LEVEL, G_TYPE_INT,
                           G_OBJECT(button), BRIGHTNESS_SLIDER_MIN_LEVEL);

    return GTK_WIDGET (button);
}

static gboolean
power_manager_button_set_icon (PowerManagerButton *button)
{
    GdkPixbuf *pixbuf;

    DBG("icon_width %d", button->priv->panel_icon_width);

    pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                       button->priv->panel_icon_name,
                                       button->priv->panel_icon_width,
                                       GTK_ICON_LOOKUP_GENERIC_FALLBACK,
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
power_manager_button_set_width (PowerManagerButton *button, gint width)
{
    g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));

    button->priv->panel_icon_width = width;

    power_manager_button_set_icon (button);
}

static gboolean
power_manager_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
    PowerManagerButton *button = POWER_MANAGER_BUTTON (widget);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
    power_manager_button_show_menu (button);
    return TRUE;
}

#ifdef XFCE_PLUGIN
static gboolean
power_manager_button_size_changed_cb (XfcePanelPlugin *plugin, gint size, PowerManagerButton *button)
{
    gint width;
    gint xthickness;
    gint ythickness;

    g_return_val_if_fail (POWER_MANAGER_IS_BUTTON (button), FALSE);
    g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), FALSE);

    xthickness = gtk_widget_get_style(GTK_WIDGET(button))->xthickness;
    ythickness = gtk_widget_get_style(GTK_WIDGET(button))->ythickness;
    size /= xfce_panel_plugin_get_nrows (plugin);
    width = size - 2* MAX (xthickness, ythickness);

    gtk_widget_set_size_request (GTK_WIDGET(plugin), size + xthickness, size + ythickness);
    button->priv->panel_icon_width = width;

    return power_manager_button_set_icon (button);
}

static void
power_manager_button_style_set_cb (XfcePanelPlugin *plugin, GtkStyle *prev_style, PowerManagerButton *button)
{
    gtk_widget_reset_rc_styles (GTK_WIDGET (plugin));
    power_manager_button_size_changed_cb (plugin, xfce_panel_plugin_get_size (plugin), button);
}

static void
power_manager_button_free_data_cb (XfcePanelPlugin *plugin, PowerManagerButton *button)
{
    gtk_widget_destroy (GTK_WIDGET (button));
}
#endif

static void
help_cb (GtkMenuItem *menuitem, gpointer user_data)
{
#if LIBXFCE4UI_CHECK_VERSION(4, 11, 1)
    xfce_dialog_show_help_with_version (NULL, "xfce4-power-manager", "start", NULL, XFPM_VERSION_SHORT);
#else
    xfce_dialog_show_help (NULL, "xfce4-power-manager", "start", NULL);
#endif
}

void
power_manager_button_show (PowerManagerButton *button)
{
    GtkWidget *mi;

    g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));

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
                      G_CALLBACK (power_manager_button_size_changed_cb), button);

    g_signal_connect (button->priv->plugin, "style-set",
                      G_CALLBACK (power_manager_button_style_set_cb), button);

    g_signal_connect (button->priv->plugin, "free-data",
                      G_CALLBACK (power_manager_button_free_data_cb), button);
#endif

    gtk_widget_show_all (GTK_WIDGET(button));
    power_manager_button_set_tooltip (button);

    /* Add all the devcies currently attached to the system */
    power_manager_button_add_all_devices (button);
}

static void
menu_destroyed_cb(GtkMenuShell *menu, gpointer user_data)
{
    PowerManagerButton *button = POWER_MANAGER_BUTTON (user_data);

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
    PowerManagerButton *button = POWER_MANAGER_BUTTON (user_data);
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
    PowerManagerButton *button = POWER_MANAGER_BUTTON (user_data);
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

static gboolean
power_manager_button_menu_add_device (PowerManagerButton *button, BatteryDevice *battery_device, gboolean append)
{
    GtkWidget *mi, *label;
    guint type = 0;

    g_return_val_if_fail (POWER_MANAGER_IS_BUTTON (button), FALSE);

    /* We need a menu to attach it to */
    g_return_val_if_fail (button->priv->menu, FALSE);

    if (UP_IS_DEVICE(battery_device->device))
    {
        g_object_get (battery_device->device,
                      "kind", &type,
                      NULL);

        /* Don't add the display device or line power to the menu */
        if (type == UP_DEVICE_KIND_LINE_POWER || battery_device->device == button->priv->display_device)
        {
            DBG("filtering device from menu (display or line power device)");
            return FALSE;
        }
    }

    mi = gtk_image_menu_item_new_with_label(battery_device->details);
    /* Make the menu item be bold and multi-line */
    label = gtk_bin_get_child(GTK_BIN(mi));
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);

    /* add the image */
    battery_device->img = gtk_image_new_from_pixbuf(battery_device->pix);
    g_object_ref (battery_device->img);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), battery_device->img);

    /* keep track of the menu item in the battery_device so we can update it */
    battery_device->menu_item = mi;
    g_signal_connect(G_OBJECT(mi), "destroy", G_CALLBACK(menu_item_destroyed_cb), button);
    battery_device->expose_signal_id = g_signal_connect_after (G_OBJECT (battery_device->img),
                                                               "expose-event",
                                                               G_CALLBACK (power_manager_button_device_icon_expose),
                                                               battery_device->device);

    /* Active calls xfpm settings with the device's id to display details */
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menu_item_activate_cb), button);

    /* Add it to the menu */
    gtk_widget_show(mi);
    if (append)
	gtk_menu_shell_append(GTK_MENU_SHELL(button->priv->menu), mi);
    else
	gtk_menu_shell_prepend(GTK_MENU_SHELL(button->priv->menu), mi);

    return TRUE;
}

static void
decrease_brightness (PowerManagerButton *button)
{
    gint32 level;

    TRACE("entering");

    if ( !xfpm_brightness_has_hw (button->priv->brightness) )
        return;

    xfpm_brightness_get_level (button->priv->brightness, &level);

    if ( level > button->priv->brightness_min_level )
    {
        xfpm_brightness_down (button->priv->brightness, &level);
        if (button->priv->range)
            gtk_range_set_value (GTK_RANGE (button->priv->range), level);
    }
}

static void
increase_brightness (PowerManagerButton *button)
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

static gboolean
brightness_set_level_with_timeout (PowerManagerButton *button)
{
    gint32 range_level, hw_level;

    TRACE("entering");

    range_level = (gint32) gtk_range_get_value (GTK_RANGE (button->priv->range));

    xfpm_brightness_get_level (button->priv->brightness, &hw_level);

    if ( hw_level != range_level )
    {
        xfpm_brightness_set_level (button->priv->brightness, range_level);
    }

    if (button->priv->set_level_timeout)
    {
        g_source_remove(button->priv->set_level_timeout);
        button->priv->set_level_timeout = 0;
    }

    return FALSE;
}

static void
range_value_changed_cb (PowerManagerButton *button, GtkWidget *widget)
{
    TRACE("entering");

    if (button->priv->set_level_timeout)
        return;

    button->priv->set_level_timeout =
        g_timeout_add(SET_LEVEL_TIMEOUT,
                      (GSourceFunc) brightness_set_level_with_timeout, button);
}

static void
range_scroll_cb (GtkWidget *widget, GdkEvent *event, PowerManagerButton *button)
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
range_show_cb (GtkWidget *widget, PowerManagerButton *button)
{
    TRACE("entering");
    /* Release these grabs they will cause a lockup if pkexec is called
     * for the brightness helper */
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
    gtk_grab_remove(widget);
}

static void
power_manager_button_show_menu (PowerManagerButton *button)
{
    GtkWidget *menu, *mi, *img = NULL;
    GdkScreen *gscreen;
    GList *item;
    gboolean show_separator_flag = FALSE;
    gint32 max_level, current_level = 0;

    TRACE("entering");

    g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));

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

        if (power_manager_button_menu_add_device (button, battery_device, TRUE))
        {
            /* If we add an item to the menu, show the separator */
            show_separator_flag = TRUE;
        }
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

        max_level = xfpm_brightness_get_max_level (button->priv->brightness);

        mi = scale_menu_item_new_with_range (button->priv->brightness_min_level, max_level, 1);

        /* attempt to load and display the brightness icon */
        pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                       XFPM_DISPLAY_BRIGHTNESS_ICON,
                                       32,
                                       GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                       NULL);
        if (pix)
        {
            img = gtk_image_new_from_pixbuf (pix);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(mi), img);
        }

        scale_menu_item_set_description_label (SCALE_MENU_ITEM(mi), _("<b>Display brightness</b>"));

        /* range slider */
        button->priv->range = scale_menu_item_get_scale (SCALE_MENU_ITEM (mi));

        /* update the slider to the current brightness level */
        xfpm_brightness_get_level (button->priv->brightness, &current_level);
        gtk_range_set_value (GTK_RANGE(button->priv->range), current_level);

        g_signal_connect_swapped (mi, "value-changed", G_CALLBACK (range_value_changed_cb), button);
        g_signal_connect (mi, "scroll-event", G_CALLBACK (range_scroll_cb), button);
        g_signal_connect (menu, "show", G_CALLBACK (range_show_cb), button);

        gtk_widget_show_all (mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }

    /* Presentation mode checkbox */
    mi = gtk_check_menu_item_new_with_mnemonic (_("Presentation _mode"));
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    xfconf_g_property_bind(button->priv->channel,
                           PROPERTIES_PREFIX PRESENTATION_MODE,
                           G_TYPE_BOOLEAN, G_OBJECT(mi), "active");

    /* Power manager settings */
    mi = gtk_menu_item_new_with_mnemonic (_("_Power manager settings..."));
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
#ifdef XFCE_PLUGIN
                    button->priv->plugin,
#else
                    NULL,
#endif
                    0,
                    gtk_get_current_event_time ());
}
