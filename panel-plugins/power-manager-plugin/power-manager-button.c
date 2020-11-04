/*
 * * Copyright (C) 2014 Eric Koegel <eric@xfce.org>
 * * Copyright (C) 2019 Kacper Piwiński
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
#include <upower.h>
#include <xfconf/xfconf.h>

#include "common/xfpm-common.h"
#include "common/xfpm-config.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"
#include "common/xfpm-brightness.h"
#include "common/xfpm-debug.h"
#ifdef XFPM_SYSTRAY
#include "src/xfpm-inhibit.h"
#endif

#include "power-manager-button.h"
#include "scalemenuitem.h"


#define SET_LEVEL_TIMEOUT (50)
#define SAFE_SLIDER_MIN_LEVEL (5)
#define PANEL_DEFAULT_ICON ("battery-full-charged")
#define PANEL_DEFAULT_ICON_SYMBOLIC ("battery-full-charged-symbolic")
#define PRESENTATION_MODE_ICON ("x-office-presentation-symbolic")

struct PowerManagerButtonPrivate
{
#ifdef XFCE_PLUGIN
  XfcePanelPlugin *plugin;
  GDBusProxy      *inhibit_proxy;
#else
  XfpmInhibit     *inhibit;
#endif

  XfconfChannel   *channel;

  UpClient        *upower;

  /* A list of BatteryDevices  */
  GList           *devices;

  /* The left-click popup menu, if one is being displayed */
  GtkWidget       *menu;

  /* The actual panel icon image */
  GtkWidget       *panel_icon_image;
  GtkWidget       *panel_presentation_mode;
  GtkWidget       *panel_label;
  GtkWidget       *hbox;
  /* Keep track of icon name to redisplay during size changes */
  gchar           *panel_icon_name;
  gchar           *panel_fallback_icon_name;
  /* Keep track of the last icon size for use during updates */
  gint             panel_icon_width;
  /* Keep track of the tooltip */
  gchar           *tooltip;

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

  gint             show_panel_label;
  gboolean         presentation_mode;
  gboolean         show_presentation_indicator;

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
  PROP_SHOW_PANEL_LABEL,
  PROP_PRESENTATION_MODE,
  PROP_SHOW_PRESENTATION_INDICATOR,
} POWER_MANAGER_BUTTON_PROPERTIES;

enum {
  SIG_ICON_NAME_CHANGED = 0,
  SIG_TOOLTIP_CHANGED,
  SIG_N_SIGNALS,
};

static guint __signals[SIG_N_SIGNALS] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (PowerManagerButton, power_manager_button, GTK_TYPE_TOGGLE_BUTTON)

static void       power_manager_button_finalize                         (GObject *object);
static GList*     find_device_in_list                                   (PowerManagerButton *button,
                                                                         const gchar *object_path);
static gboolean   power_manager_button_device_icon_draw                 (GtkWidget *img,
                                                                         cairo_t *cr,
                                                                         gpointer userdata);
static void       power_manager_button_set_icon                         (PowerManagerButton *button);
static void       power_manager_button_set_label                        (PowerManagerButton *button,
                                                                         gdouble percentage,
                                                                         guint64 time_to_empty_or_full);
#ifdef XFCE_PLUGIN
static void       power_manager_button_toggle_presentation_mode         (GtkMenuItem *mi,
                                                                         GtkSwitch *sw);
static void       power_manager_button_update_presentation_indicator    (PowerManagerButton *button);
#endif
static void       power_manager_button_update_label                     (PowerManagerButton *button,
                                                                         UpDevice *device);
static gboolean   power_manager_button_press_event                      (GtkWidget *widget,
                                                                         GdkEventButton *event);
static gboolean   power_manager_button_menu_add_device                  (PowerManagerButton *button,
                                                                         BatteryDevice *battery_device,
                                                                         gboolean append);
static void       increase_brightness                                   (PowerManagerButton *button);
static void       decrease_brightness                                   (PowerManagerButton *button);
static void       battery_device_remove_pix                             (BatteryDevice *battery_device);


static BatteryDevice*
get_display_device (PowerManagerButton *button)
{
  GList *item = NULL;
  gdouble highest_percentage = 0;
  BatteryDevice *display_device = NULL;

  TRACE("entering");

  g_return_val_if_fail (POWER_MANAGER_IS_BUTTON(button), NULL);

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

    if (type == UP_DEVICE_KIND_BATTERY || type == UP_DEVICE_KIND_UPS)
    {
      if (highest_percentage < percentage)
      {
        display_device = battery_device;
        highest_percentage = percentage;
      }
    }
  }

  return display_device;
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

static void
power_manager_button_set_icon (PowerManagerButton *button)
{
  g_return_if_fail (GTK_IS_WIDGET (button->priv->panel_presentation_mode));

  if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default(), button->priv->panel_icon_name))
    gtk_image_set_from_icon_name (GTK_IMAGE (button->priv->panel_icon_image), button->priv->panel_icon_name, GTK_ICON_SIZE_BUTTON);
  else
    gtk_image_set_from_icon_name (GTK_IMAGE (button->priv->panel_icon_image), button->priv->panel_fallback_icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (button->priv->panel_icon_image), button->priv->panel_icon_width);

  gtk_image_set_pixel_size (GTK_IMAGE (button->priv->panel_presentation_mode), button->priv->panel_icon_width);

  /* Notify others the icon name changed */
  g_signal_emit (button, __signals[SIG_ICON_NAME_CHANGED], 0);
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

  if (button->priv->tooltip != NULL)
  {
    g_free (button->priv->tooltip);
    button->priv->tooltip = NULL;
  }

  if (display_device)
  {
    /* if we have something, display it */
    if( display_device->details )
    {
      button->priv->tooltip = g_strdup(display_device->details);
      gtk_widget_set_tooltip_markup (GTK_WIDGET (button), display_device->details);
      /* Tooltip changed! */
      g_signal_emit (button, __signals[SIG_TOOLTIP_CHANGED], 0);
      return;
    }
  }

  /* Odds are this is a desktop without any batteries attached */
  button->priv->tooltip = g_strdup (_("Display battery levels for attached devices"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), button->priv->tooltip);
  /* Tooltip changed! */
  g_signal_emit (button, __signals[SIG_TOOLTIP_CHANGED], 0);
}

const gchar *
power_manager_button_get_icon_name (PowerManagerButton *button)
{
  return button->priv->panel_icon_name;
}

const gchar *
power_manager_button_get_tooltip (PowerManagerButton *button)
{
  return button->priv->tooltip;
}

static void
power_manager_button_set_label (PowerManagerButton *button, gdouble percentage,
                                guint64 time_to_empty_or_full)
{
  gchar *label_string = NULL;
  gint   hours;
  gint   minutes;
  gchar *remaining_time = NULL;

  /* Create the short timestring in the format hh:mm */
  minutes = (int) ( ( time_to_empty_or_full / 60.0 ) + 0.5 );
  if (minutes < 60)
  {
    if (minutes < 10)
      remaining_time = g_strdup_printf ("0:0%d", minutes);
    else
      remaining_time = g_strdup_printf ("0:%d", minutes);
  }
  else
  {
    hours = minutes / 60;
    minutes = minutes % 60;
    if (minutes < 10)
      remaining_time = g_strdup_printf ("%d:0%d", hours, minutes);
    else
      remaining_time = g_strdup_printf ("%d:%d", hours, minutes);
  }

  /* Set the label accordingly or hide it if the battery is full */
  if (button->priv->show_panel_label == 1)
    label_string = g_strdup_printf ("%d%%", (int) percentage);
  else if (button->priv->show_panel_label == 2)
    label_string = g_strdup_printf ("%s", remaining_time);
  else if (button->priv->show_panel_label == 3)
    label_string = g_strdup_printf ("(%s, %d%%)", remaining_time, (int) percentage);

  gtk_label_set_text (GTK_LABEL (button->priv->panel_label), label_string);

  g_free (label_string);
  g_free (remaining_time);
}

static gboolean
power_manager_button_device_icon_draw (GtkWidget *img, cairo_t *cr, gpointer userdata)
{
  UpDevice *device = NULL;
  guint type = 0, state = 0;
  gdouble percentage;
  gint height, width;
  gdouble min_height = 2;
  PangoLayout *layout = NULL;
  PangoRectangle ink_extent, log_extent;
  GtkAllocation allocation;

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

    /* Don't draw the progressbar for Battery */
    if (type == UP_DEVICE_KIND_BATTERY)
      return FALSE;
  }
  else
  {
    /* If the UpDevice hasn't fully updated yet it then we'll want
     * a question mark for sure. */
    state = UP_DEVICE_STATE_UNKNOWN;
  }

  gtk_widget_get_allocation (img, &allocation);

  width = allocation.width;
  height = allocation.height;

  if (state != UP_DEVICE_STATE_UNKNOWN)
  {
    /* Draw the trough of the progressbar */
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_line_width (cr, 1.0);
    cairo_rectangle (cr, width - 3.5, allocation.y + 1.5, 5, height - 2);
    cairo_set_source_rgb (cr, 0.87, 0.87, 0.87);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, 0.53, 0.54, 0.52);
    cairo_stroke (cr);

    /* Draw the fill of the progressbar
       Use yellow for 20% and below, green for 100%, red for 5% and below and blue for the rest */
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    if ((height * (percentage / 100)) > min_height)
       min_height = (height - 3) * (percentage / 100);

    cairo_rectangle (cr, width - 3, allocation.y + height - min_height - 1, 4, min_height);
    if (percentage > 5 && percentage < 20)
        cairo_set_source_rgb (cr, 0.93, 0.83, 0.0);
    else if (percentage > 20 && percentage < 100)
        cairo_set_source_rgb (cr, 0.2, 0.4, 0.64);
    else if (percentage == 100)
        cairo_set_source_rgb (cr, 0.45, 0.82, 0.08);
    else
        cairo_set_source_rgb (cr, 0.94, 0.16, 0.16);
    cairo_fill (cr);

    cairo_rectangle (cr, width - 2.5, allocation.y + 2.5, 3, height - 4);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.75);
    cairo_stroke (cr);
  }
  else
  {
    /* Draw a bubble with a question mark for devices with unknown state */
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_line_width (cr, 1.0);
    cairo_arc(cr, width - 4.5, allocation.y + 6.5, 6, 0, 2*3.14159);
    cairo_set_source_rgb (cr, 0.2, 0.54, 0.9);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, 0.1, 0.37, 0.6);
    cairo_stroke (cr);

    layout = gtk_widget_create_pango_layout (GTK_WIDGET (img), "?");
    pango_layout_set_font_description (layout, pango_font_description_from_string ("Sans Bold 9"));
    pango_layout_get_pixel_extents (layout, &ink_extent, &log_extent);
    cairo_move_to (cr, (width - 5.5) - (log_extent.width / 2), (allocation.y + 5.5) - (log_extent.height / 2));
    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
    pango_cairo_show_layout (cr, layout);
  }

  if (layout)
    g_object_unref (layout);

  return FALSE;
}


static void
power_manager_button_update_device_icon_and_details (PowerManagerButton *button, UpDevice *device)
{
  GList          *item;
  BatteryDevice  *battery_device;
  BatteryDevice  *display_device;
  const gchar    *object_path = up_device_get_object_path(device);
  gchar          *details;
  gchar          *icon_name;
  gchar          *menu_icon_name;
  GdkPixbuf      *pix = NULL;

  XFPM_DEBUG("entering for %s", object_path);

  if (!POWER_MANAGER_IS_BUTTON (button))
    return;

  item = find_device_in_list (button, object_path);

  if (item == NULL)
    return;

  battery_device = item->data;

  icon_name = get_device_icon_name (button->priv->upower, device, TRUE);
  menu_icon_name = get_device_icon_name (button->priv->upower, device, FALSE);
  details = get_device_description (button->priv->upower, device);

  /* If UPower doesn't give us an icon, just use the default */
  if (g_strcmp0(menu_icon_name, "") == 0)
  {
    /* ignore empty icon names */
    g_free (menu_icon_name);
    menu_icon_name = NULL;
  }

  if (menu_icon_name == NULL)
    menu_icon_name = g_strdup (PANEL_DEFAULT_ICON);

  pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                  menu_icon_name,
                                  32,
                                  GTK_ICON_LOOKUP_USE_BUILTIN,
                                  NULL);

  if (battery_device->details)
    g_free (battery_device->details);

  battery_device->details = details;

  /* If we had an image before, remove it and the callback */
  battery_device_remove_pix(battery_device);

  battery_device->pix = pix;

  /* Get the display device, which may now be this one */
  display_device = get_display_device (button);
  if (battery_device == display_device)
  {
    DBG("this is the display device, updating");
    /* update the icon */
    g_free (button->priv->panel_icon_name);
#ifdef XFCE_PLUGIN
    button->priv->panel_icon_name = g_strdup_printf ("%s-%s", icon_name, "symbolic");
    button->priv->panel_fallback_icon_name = g_strdup_printf ("%s-%s", menu_icon_name, "symbolic");
#else
    button->priv->panel_icon_name = g_strdup (icon_name);
    button->priv->panel_fallback_icon_name = g_strdup (menu_icon_name);
#endif
    power_manager_button_set_icon (button);
    /* update the tooltip */
    power_manager_button_set_tooltip (button);
    /* update the label */
    power_manager_button_update_label (button, device);
  }
  g_free (icon_name);
  g_free (menu_icon_name);

  /* If the menu is being displayed, update it */
  if (button->priv->menu && battery_device->menu_item)
  {
    gtk_menu_item_set_label (GTK_MENU_ITEM (battery_device->menu_item), details);

    /* update the image, keep track of the signal ids and the img
     * so we can disconnect it later */
    battery_device->img = gtk_image_new_from_pixbuf (battery_device->pix);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(battery_device->menu_item), battery_device->img);
G_GNUC_END_IGNORE_DEPRECATIONS
    battery_device->expose_signal_id = g_signal_connect_after (G_OBJECT (battery_device->img),
                                                               "draw",
                                                               G_CALLBACK (power_manager_button_device_icon_draw),
                                                               device);
  }
}

static void
device_changed_cb (UpDevice *device, GParamSpec *pspec, PowerManagerButton *button)
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

  XFPM_DEBUG("entering for %s", object_path);

  g_return_if_fail (POWER_MANAGER_IS_BUTTON (button ));

  /* don't add the same device twice */
  if (find_device_in_list (button, object_path))
    return;

  battery_device = g_new0 (BatteryDevice, 1);

  /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
  g_object_get (device,
                "kind", &type,
                NULL);

  signal_id = g_signal_connect (device, "notify", G_CALLBACK (device_changed_cb), button);

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

  if (G_IS_OBJECT (battery_device->pix))
  {
    if (GTK_IS_WIDGET (battery_device->img))
    {
      if (battery_device->expose_signal_id != 0)
      {
          g_signal_handler_disconnect (battery_device->img, battery_device->expose_signal_id);
          battery_device->expose_signal_id = 0;
      }
      g_object_unref (battery_device->img);
      battery_device->img = NULL;
    }
    g_object_unref (battery_device->pix);
    battery_device->pix = NULL;
  }
}

static void
remove_battery_device (PowerManagerButton *button, BatteryDevice *battery_device)
{
  g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));
  g_return_if_fail (battery_device != NULL);

  /* If it is being shown in the menu, remove it */
  if (battery_device->menu_item && button->priv->menu)
    gtk_container_remove (GTK_CONTAINER (button->priv->menu), battery_device->menu_item);

  g_free (battery_device->details);
  g_free (battery_device->object_path);

  battery_device_remove_pix (battery_device);

  if (battery_device->device != NULL && UP_IS_DEVICE(battery_device->device))
  {
    /* disconnect the signal handler if we were using it */
    if (battery_device->changed_signal_id != 0)
      g_signal_handler_disconnect (battery_device->device, battery_device->changed_signal_id);
    battery_device->changed_signal_id = 0;

    g_object_unref (battery_device->device);
    battery_device->device = NULL;
  }

  g_free (battery_device);
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

  /* Remove its resources */
  remove_battery_device (button, battery_device);

  /* remove it item and free the battery device */
  button->priv->devices = g_list_delete_link (button->priv->devices, item);
}

static void
device_added_cb (UpClient *upower, UpDevice *device, PowerManagerButton *button)
{
  power_manager_button_add_device (device, button);
}

static void
device_removed_cb (UpClient *upower, const gchar *object_path, PowerManagerButton *button)
{
  power_manager_button_remove_device (button, object_path);
}


static void
power_manager_button_add_all_devices (PowerManagerButton *button)
{
  GPtrArray *array = NULL;
  guint i;

  button->priv->display_device = up_client_get_display_device (button->priv->upower);
  power_manager_button_add_device (button->priv->display_device, button);

#if UP_CHECK_VERSION(0, 99, 8)
  array = up_client_get_devices2 (button->priv->upower);
#else
  array = up_client_get_devices (button->priv->upower);
#endif

  if (array)
  {
    for (i = 0; i < array->len; i++)
    {
      UpDevice *device = g_ptr_array_index (array, i);

      power_manager_button_add_device (device, button);
    }
    g_ptr_array_free (array, TRUE);
  }
}

static void
power_manager_button_remove_all_devices (PowerManagerButton *button)
{
  GList *item = NULL;

  TRACE("entering");

  g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));

  for (item = g_list_first (button->priv->devices); item != NULL; item = g_list_next (item))
  {
    BatteryDevice *battery_device = item->data;
    if (battery_device == NULL)
    {
      DBG("!battery_device");
      continue;
    }

    /* Remove its resources */
    remove_battery_device (button, battery_device);
  }
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

  if (!hw_found)
    return FALSE;

  if (ev->direction == GDK_SCROLL_UP)
  {
    brightness_up (button);
    return TRUE;
  }
  else if (ev->direction == GDK_SCROLL_DOWN)
  {
    brightness_down (button);
    return TRUE;
  }
  return FALSE;
}

static void
set_brightness_min_level (PowerManagerButton *button, gint32 new_brightness_level)
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
    gtk_range_set_range (GTK_RANGE (button->priv->range), button->priv->brightness_min_level, max_level);
  }
}

static void
power_manager_button_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
  PowerManagerButton *button;

  button = POWER_MANAGER_BUTTON (object);

  switch (property_id)
  {
    case PROP_BRIGHTNESS_MIN_LEVEL:
      set_brightness_min_level (button, g_value_get_int (value));
      break;
#ifdef XFCE_PLUGIN
    case PROP_SHOW_PANEL_LABEL:
      button->priv->show_panel_label = g_value_get_int (value);
      power_manager_button_update_label (button, button->priv->display_device);
      break;
    case PROP_PRESENTATION_MODE:
      button->priv->presentation_mode = g_value_get_boolean (value);
      if (GTK_IS_WIDGET (button->priv->panel_presentation_mode))
          power_manager_button_update_presentation_indicator (button);
      break;
    case PROP_SHOW_PRESENTATION_INDICATOR:
      button->priv->show_presentation_indicator = g_value_get_boolean (value);
      if (GTK_IS_WIDGET (button->priv->panel_presentation_mode))
          power_manager_button_update_presentation_indicator (button);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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
      g_value_set_int (value, button->priv->brightness_min_level);
      break;
#ifdef XFCE_PLUGIN
    case PROP_SHOW_PANEL_LABEL:
      g_value_set_int (value, button->priv->show_panel_label);
      break;
    case PROP_PRESENTATION_MODE:
      g_value_set_boolean (value, button->priv->presentation_mode);
      break;
    case PROP_SHOW_PRESENTATION_INDICATOR:
      g_value_set_boolean (value, button->priv->show_presentation_indicator);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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

  __signals[SIG_TOOLTIP_CHANGED] = g_signal_new ("tooltip-changed",
                                                 POWER_MANAGER_TYPE_BUTTON,
                                                 G_SIGNAL_RUN_LAST,
                                                 G_STRUCT_OFFSET(PowerManagerButtonClass,
                                                                 tooltip_changed),
                                                 NULL, NULL,
                                                 g_cclosure_marshal_VOID__VOID,
                                                 G_TYPE_NONE, 0);

  __signals[SIG_ICON_NAME_CHANGED] = g_signal_new ("icon-name-changed",
                                                   POWER_MANAGER_TYPE_BUTTON,
                                                   G_SIGNAL_RUN_LAST,
                                                   G_STRUCT_OFFSET(PowerManagerButtonClass,
                                                                   icon_name_changed),
                                                   NULL, NULL,
                                                   g_cclosure_marshal_VOID__VOID,
                                                   G_TYPE_NONE, 0);

#define XFPM_PARAM_FLAGS  (G_PARAM_READWRITE \
                           | G_PARAM_CONSTRUCT \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)

  /* We allow and default to -1 only so that we can automagically set a
   * sane value if the user hasn't selected one already */
  g_object_class_install_property (object_class, PROP_BRIGHTNESS_MIN_LEVEL,
                                   g_param_spec_int(BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                    BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                    BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                    -1, G_MAXINT32, -1,
                                                    XFPM_PARAM_FLAGS));

  g_object_class_install_property (object_class, PROP_SHOW_PANEL_LABEL,
                                   g_param_spec_int (SHOW_PANEL_LABEL,
                                                     NULL, NULL,
                                                     0, G_MAXINT16, 3,
                                                     XFPM_PARAM_FLAGS));

  g_object_class_install_property (object_class, PROP_PRESENTATION_MODE,
                                   g_param_spec_boolean (PRESENTATION_MODE,
                                                         NULL, NULL,
                                                         FALSE,
                                                         XFPM_PARAM_FLAGS));

  g_object_class_install_property (object_class, PROP_SHOW_PRESENTATION_INDICATOR,
                                   g_param_spec_boolean (SHOW_PRESENTATION_INDICATOR,
                                                         NULL, NULL,
                                                         FALSE,
                                                         XFPM_PARAM_FLAGS));
#undef XFPM_PARAM_FLAGS
}

#ifdef XFCE_PLUGIN
static void
inhibit_proxy_ready_cb (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
  GError *error = NULL;
  PowerManagerButton *button = POWER_MANAGER_BUTTON (user_data);

  button->priv->inhibit_proxy = g_dbus_proxy_new_finish (res, &error);
  if (error != NULL)
  {
    g_warning ("error getting inhibit proxy: %s", error->message);
    g_clear_error (&error);
  }
}
#endif

static void
power_manager_button_init (PowerManagerButton *button)
{
  GError *error = NULL;
  GtkCssProvider *css_provider;

  button->priv = power_manager_button_get_instance_private (button);

  gtk_widget_set_can_default (GTK_WIDGET (button), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
#if !GTK_CHECK_VERSION (3, 20, 0)
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
#else
  gtk_widget_set_focus_on_click (GTK_WIDGET (button), FALSE);
#endif
  gtk_widget_set_name (GTK_WIDGET (button), "xfce4-power-manager-plugin");

  button->priv->brightness = xfpm_brightness_new ();
  xfpm_brightness_setup (button->priv->brightness);
  button->priv->set_level_timeout = 0;

  button->priv->upower  = up_client_new ();
  if ( !xfconf_init (&error) )
  {
    if (error)
    {
      g_critical ("xfconf_init failed: %s\n", error->message);
      g_error_free (error);
    }
  }
  else
  {
    button->priv->channel = xfconf_channel_get (XFPM_CHANNEL);
  }

#ifdef XFCE_PLUGIN
  g_dbus_proxy_new (g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL),
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    "org.freedesktop.PowerManagement",
                    "/org/freedesktop/PowerManagement/Inhibit",
                    "org.freedesktop.PowerManagement.Inhibit",
                    NULL,
                    inhibit_proxy_ready_cb,
                    button);
#else
  button->priv->inhibit = xfpm_inhibit_new ();
#endif

  /* Sane defaults for the systray and panel icon */
#ifdef XFCE_PLUGIN
  button->priv->panel_icon_name = g_strdup (PANEL_DEFAULT_ICON_SYMBOLIC);
  button->priv->panel_fallback_icon_name = g_strdup (PANEL_DEFAULT_ICON_SYMBOLIC);
#else
  button->priv->panel_icon_name = g_strdup (PANEL_DEFAULT_ICON);
  button->priv->panel_fallback_icon_name = g_strdup (PANEL_DEFAULT_ICON);
#endif
  button->priv->panel_icon_width = 24;

  /* Sane default Gtk style */
  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider,
                                   "#xfce4-power-manager-plugin {"
                                   "padding: 1px;"
                                   "border-width: 1px;}",
                                   -1, NULL);
  gtk_style_context_add_provider (GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (button))),
                                  GTK_STYLE_PROVIDER (css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  /* Intercept scroll events */
  gtk_widget_add_events (GTK_WIDGET (button), GDK_SCROLL_MASK);

  g_signal_connect (button->priv->upower, "device-added", G_CALLBACK (device_added_cb), button);
  g_signal_connect (button->priv->upower, "device-removed", G_CALLBACK (device_removed_cb), button);
}

static void
power_manager_button_finalize (GObject *object)
{
  PowerManagerButton *button;

  DBG("entering");

  button = POWER_MANAGER_BUTTON (object);

  g_free (button->priv->panel_icon_name);
  g_free (button->priv->panel_fallback_icon_name);

  if (button->priv->set_level_timeout)
  {
    g_source_remove(button->priv->set_level_timeout);
    button->priv->set_level_timeout = 0;
  }

  g_signal_handlers_disconnect_by_data (button->priv->upower, button);

  power_manager_button_remove_all_devices (button);

#ifdef XFCE_PLUGIN
  g_object_unref (button->priv->plugin);
#endif

  G_OBJECT_CLASS (power_manager_button_parent_class)->finalize (object);
}

GtkWidget *
#ifdef XFCE_PLUGIN
power_manager_button_new (XfcePanelPlugin *plugin)
#endif
#ifdef XFPM_SYSTRAY
power_manager_button_new (void)
#endif
{
  PowerManagerButton *button = NULL;
  button = g_object_new (POWER_MANAGER_TYPE_BUTTON, NULL, NULL);

#ifdef XFCE_PLUGIN
  button->priv->plugin = XFCE_PANEL_PLUGIN (g_object_ref (plugin));
#endif

  xfconf_g_property_bind (button->priv->channel,
                          XFPM_PROPERTIES_PREFIX BRIGHTNESS_SLIDER_MIN_LEVEL, G_TYPE_INT,
                          G_OBJECT (button), BRIGHTNESS_SLIDER_MIN_LEVEL);
  xfconf_g_property_bind (button->priv->channel, XFPM_PROPERTIES_PREFIX SHOW_PANEL_LABEL, G_TYPE_INT,
                          G_OBJECT (button), SHOW_PANEL_LABEL);
  xfconf_g_property_bind (button->priv->channel, XFPM_PROPERTIES_PREFIX PRESENTATION_MODE, G_TYPE_BOOLEAN,
                          G_OBJECT (button), PRESENTATION_MODE);
  xfconf_g_property_bind (button->priv->channel, XFPM_PROPERTIES_PREFIX SHOW_PRESENTATION_INDICATOR, G_TYPE_BOOLEAN,
                          G_OBJECT (button), SHOW_PRESENTATION_INDICATOR);
  return GTK_WIDGET (button);
}

static gboolean
power_manager_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  PowerManagerButton *button = POWER_MANAGER_BUTTON (widget);

  if (event->button == 1 && !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
    power_manager_button_show_menu (button);
    return TRUE;
  }

  if (event->button == 2)
  {
    gboolean state;

    state = xfconf_channel_get_bool (button->priv->channel, XFPM_PROPERTIES_PREFIX PRESENTATION_MODE, FALSE);
    xfconf_channel_set_bool (button->priv->channel, XFPM_PROPERTIES_PREFIX PRESENTATION_MODE, !state);
    return TRUE;
  }

  return FALSE;
}

#ifdef XFCE_PLUGIN
static void
power_manager_button_size_changed_cb (XfcePanelPlugin *plugin, gint size, PowerManagerButton *button)
{
#if !LIBXFCE4PANEL_CHECK_VERSION (4, 13, 0)
  GtkStyleContext *context;
  GtkBorder padding, border;
  gint width;
  gint xthickness;
  gint ythickness;
#endif

  g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (size > 0);

  size /= xfce_panel_plugin_get_nrows (plugin);

#if LIBXFCE4PANEL_CHECK_VERSION (4, 13, 0)
  button->priv->panel_icon_width = xfce_panel_plugin_get_icon_size (plugin);
#else
  /* Calculate the size of the widget because the theme can override it */
  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_get_padding (context, gtk_widget_get_state_flags (GTK_WIDGET (button)), &padding);
  gtk_style_context_get_border (context, gtk_widget_get_state_flags (GTK_WIDGET (button)), &border);
  xthickness = padding.left + padding.right + border.left + border.right;
  ythickness = padding.top + padding.bottom + border.top + border.bottom;

  /* Calculate the size of the space left for the icon */
  width = size - 2 * MAX (xthickness, ythickness);

  /* Since symbolic icons are usually only provided in 16px we
   * try to be clever and use size steps */
  if (width <= 21)
    button->priv->panel_icon_width = 16;
  else if (width >=22 && width <= 29)
    button->priv->panel_icon_width = 24;
  else if (width >= 30 && width <= 40)
    button->priv->panel_icon_width = 32;
  else
    button->priv->panel_icon_width = width;
#endif

  /* resize the plugin */
  gtk_widget_set_size_request (GTK_WIDGET (plugin), size, size);
  power_manager_button_set_icon (button);

  /* resize the plugin button too */
  gtk_widget_set_size_request (GTK_WIDGET (button), -1, -1);
}

static void
power_manager_button_style_update_cb (XfcePanelPlugin *plugin, PowerManagerButton *button)
{
  gtk_widget_reset_style (GTK_WIDGET (plugin));
  power_manager_button_size_changed_cb (plugin, xfce_panel_plugin_get_size (plugin), button);
}

static void
power_manager_button_free_data_cb (XfcePanelPlugin *plugin, PowerManagerButton *button)
{
  gtk_widget_destroy (GTK_WIDGET (button));
}

static void
about_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  xfpm_about ("xfce4-power-manager");
}
#endif

void
power_manager_button_show (PowerManagerButton *button)
{
  GtkWidget *hbox;
  GtkStyleContext *context;
  GtkCssProvider *css_provider;

  g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));

#ifdef XFCE_PLUGIN
  xfce_panel_plugin_add_action_widget (button->priv->plugin, GTK_WIDGET (button));
  xfce_panel_plugin_set_small (button->priv->plugin, TRUE);
#endif

  button->priv->panel_icon_image = gtk_image_new ();
  button->priv->panel_presentation_mode = gtk_image_new_from_icon_name (PRESENTATION_MODE_ICON, GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (button->priv->panel_presentation_mode), button->priv->panel_icon_width);
  context = gtk_widget_get_style_context (button->priv->panel_presentation_mode);
  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider,
                                   ".presentation-mode { color: @warning_color; }",
                                   -1, NULL);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);
  gtk_style_context_add_class (context, "presentation-mode");
  button->priv->panel_label = gtk_label_new ("");
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (button->priv->panel_presentation_mode), TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (button->priv->panel_icon_image), TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (button->priv->panel_label), TRUE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (button), GTK_WIDGET (hbox));

#ifdef XFCE_PLUGIN
  xfce_panel_plugin_menu_show_about (button->priv->plugin);
  g_signal_connect (button->priv->plugin, "about", G_CALLBACK (about_cb), NULL);

  g_signal_connect (button->priv->plugin, "size-changed",
                    G_CALLBACK (power_manager_button_size_changed_cb), button);

  g_signal_connect (button->priv->plugin, "style-updated",
                    G_CALLBACK (power_manager_button_style_update_cb), button);

  g_signal_connect (button->priv->plugin, "free-data",
                    G_CALLBACK (power_manager_button_free_data_cb), button);
#endif

  gtk_widget_show_all (GTK_WIDGET(button));

  gtk_widget_set_visible (button->priv->panel_presentation_mode, button->priv->presentation_mode &&
                                                                 button->priv->show_presentation_indicator);
  power_manager_button_update_label (button, button->priv->display_device);
  power_manager_button_set_tooltip (button);

  /* Add all the devcies currently attached to the system */
  power_manager_button_add_all_devices (button);
}

#ifdef XFCE_PLUGIN
static void
power_manager_button_update_presentation_indicator (PowerManagerButton *button)
{
  gtk_image_set_pixel_size (GTK_IMAGE (button->priv->panel_presentation_mode), button->priv->panel_icon_width);

  gtk_widget_set_visible (button->priv->panel_presentation_mode, button->priv->presentation_mode &&
                                                                 button->priv->show_presentation_indicator);
}
#endif

static void
power_manager_button_update_label (PowerManagerButton *button, UpDevice *device)
{
  guint state;
  gdouble percentage;
  guint64 time_to_empty;
  guint64 time_to_full;

  if (!POWER_MANAGER_IS_BUTTON (button) || !UP_IS_DEVICE (device))
      return;

#ifdef XFCE_PLUGIN
  if (button->priv->show_panel_label <= 0 || button->priv->show_panel_label >3)
  {
    gtk_widget_hide (GTK_WIDGET (button->priv->panel_label));
    power_manager_button_size_changed_cb (button->priv->plugin,
                                          xfce_panel_plugin_get_size (button->priv->plugin),
                                          button);
    return;
  }
  else
    gtk_widget_show (GTK_WIDGET (button->priv->panel_label));
#endif

  g_object_get (device,
                "state", &state,
                "percentage", &percentage,
                "time-to-empty", &time_to_empty,
                "time-to-full", &time_to_full,
                NULL);

  /* Hide the label if the battery is fully charged,
   * if the state is unknown (no battery available)
     or if it's a desktop system */
  if (state == UP_DEVICE_STATE_CHARGING)
    power_manager_button_set_label (button, percentage, time_to_full);
  else if (state == UP_DEVICE_STATE_FULLY_CHARGED
           || state == UP_DEVICE_STATE_UNKNOWN
           || g_strcmp0 (button->priv->panel_icon_name, "ac-adapter-symbolic") == 0
           || g_strcmp0 (button->priv->panel_fallback_icon_name, "ac-adapter-symbolic") == 0)
    gtk_widget_hide (GTK_WIDGET (button->priv->panel_label));
  else
    power_manager_button_set_label (button, percentage, time_to_empty);
}

static void
menu_destroyed_cb(GtkMenuShell *menu, gpointer user_data)
{
  PowerManagerButton *button = POWER_MANAGER_BUTTON (user_data);

  TRACE("entering");

  /* menu destroyed, range slider is gone */
  button->priv->range = NULL;

  /* untoggle panel icon */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), FALSE);

  gtk_menu_detach (GTK_MENU (button->priv->menu));

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

  if (UP_IS_DEVICE (battery_device->device))
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
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  mi = gtk_image_menu_item_new_with_label(battery_device->details);
G_GNUC_END_IGNORE_DEPRECATIONS
  /* Make the menu item be bold and multi-line */
  label = gtk_bin_get_child (GTK_BIN (mi));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  /* add the image */
  battery_device->img = gtk_image_new_from_pixbuf (battery_device->pix);
  g_object_ref (battery_device->img);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), battery_device->img);
G_GNUC_END_IGNORE_DEPRECATIONS
  /* keep track of the menu item in the battery_device so we can update it */
  battery_device->menu_item = mi;
  g_signal_connect(G_OBJECT (mi), "destroy", G_CALLBACK (menu_item_destroyed_cb), button);
  battery_device->expose_signal_id = g_signal_connect_after (G_OBJECT (battery_device->img),
                                                             "draw",
                                                             G_CALLBACK (power_manager_button_device_icon_draw),
                                                             battery_device->device);

  /* Active calls xfpm settings with the device's id to display details */
  g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(menu_item_activate_cb), button);

  /* Add it to the menu */
  gtk_widget_show (mi);
  if (append)
    gtk_menu_shell_append (GTK_MENU_SHELL(button->priv->menu), mi);
  else
    gtk_menu_shell_prepend (GTK_MENU_SHELL(button->priv->menu), mi);

  return TRUE;
}

static void
add_inhibitor_to_menu (PowerManagerButton *button, const gchar *text)
{
  GtkWidget *mi, *img;

  /* Translators this is to display which app is inhibiting
   * power in the plugin menu. Example:
   * VLC is currently inhibiting power management
   */
  gchar *label = g_strdup_printf (_("%s is currently inhibiting power management"), text);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  mi = gtk_image_menu_item_new_with_label(label);
G_GNUC_END_IGNORE_DEPRECATIONS
  /* add the image */
  img = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_MENU);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_widget_set_can_focus (mi, FALSE);
  gtk_widget_show (mi);
  gtk_menu_shell_append (GTK_MENU_SHELL(button->priv->menu), mi);
  g_free (label);
}

#ifdef XFCE_PLUGIN
static void
display_inhibitors (PowerManagerButton *button, GtkWidget *menu)
{
  GtkWidget *separator_mi;
  gboolean needs_seperator = FALSE;

  g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));
  g_return_if_fail (GTK_IS_MENU (menu));

  if (button->priv->inhibit_proxy)
  {
    GVariant *reply;
    GError   *error = NULL;

    reply = g_dbus_proxy_call_sync (button->priv->inhibit_proxy,
                                    "GetInhibitors",
                                    g_variant_new ("()"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    1000,
                                    NULL,
                                    &error);

    if (reply != NULL)
    {
      GVariantIter *iter;
      gchar        *value;

      g_variant_get (reply, "(as)", &iter);

      if (g_variant_iter_n_children (iter) > 0)
      {
        needs_seperator = TRUE;
      }

      /* Add the list of programs to the menu */
      while (g_variant_iter_next (iter, "s", &value))
      {
        add_inhibitor_to_menu (button, value);
      }
      g_variant_iter_free (iter);
      g_variant_unref (reply);

    } else {
      g_warning ("failed calling GetInhibitors: %s", error->message);
      g_clear_error (&error);
    }

    if (needs_seperator)
    {
      /* add a separator */
      separator_mi = gtk_separator_menu_item_new ();
      gtk_widget_show (separator_mi);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator_mi);
    }
  }
}
#else
static void
display_inhibitors (PowerManagerButton *button, GtkWidget *menu)
{
  guint i;
  GtkWidget *separator_mi;
  const gchar **inhibitors;

  g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));
  g_return_if_fail (GTK_IS_MENU (menu));

  inhibitors = xfpm_inhibit_get_inhibit_list (button->priv->inhibit);
  if (inhibitors != NULL && inhibitors[0] != NULL)
  {
    for (i=0; inhibitors[i] != NULL; i++)
    {
      add_inhibitor_to_menu (button, inhibitors[i]);
    }

    /* add a separator */
    separator_mi = gtk_separator_menu_item_new ();
    gtk_widget_show (separator_mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator_mi);
  }

  g_free (inhibitors);
}
#endif

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

  if (!xfpm_brightness_has_hw (button->priv->brightness))
    return;

  max_level = xfpm_brightness_get_max_level (button->priv->brightness);
  xfpm_brightness_get_level (button->priv->brightness, &level);

  if (level < max_level)
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

  if (hw_level != range_level)
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
    g_timeout_add (SET_LEVEL_TIMEOUT,
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
#if !GTK_CHECK_VERSION (3, 20, 0)
  GdkDeviceManager* manager = gdk_display_get_device_manager (gdk_display_get_default());
  GdkDevice* pointer = gdk_device_manager_get_client_pointer (manager);
#else
  GdkSeat   *seat = gdk_display_get_default_seat (gdk_display_get_default());
  GdkDevice *pointer = gdk_seat_get_pointer (seat);
#endif

  TRACE("entering");
  /* Release these grabs as they will cause a lockup if pkexec is called
   * for the brightness helper */
  if (pointer)
  {
#if !GTK_CHECK_VERSION (3, 20, 0)
    gdk_device_ungrab (pointer, GDK_CURRENT_TIME);
#else
    gdk_seat_ungrab (seat);
#endif
  }

  gtk_grab_remove (widget);
}

#ifdef XFCE_PLUGIN
static void
power_manager_button_toggle_presentation_mode (GtkMenuItem *mi, GtkSwitch *sw)
{
  g_return_if_fail (GTK_IS_SWITCH (sw));

  gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
}
#endif


void
power_manager_button_show_menu (PowerManagerButton *button)
{
  GtkWidget *menu, *mi, *img = NULL;
#ifdef XFCE_PLUGIN
  GtkWidget *box, *label, *sw;
#endif
  GdkScreen *gscreen;
  GList *item;
  gboolean show_separator_flag = FALSE;
  gint32 max_level, current_level = 0;

  TRACE("entering");

  g_return_if_fail (POWER_MANAGER_IS_BUTTON (button));

  if (gtk_widget_has_screen (GTK_WIDGET (button)))
      gscreen = gtk_widget_get_screen(GTK_WIDGET(button));
  else
      gscreen = gdk_display_get_default_screen(gdk_display_get_default());

  menu = gtk_menu_new ();
  gtk_menu_set_screen(GTK_MENU(menu), gscreen);
  /* keep track of the menu while it's being displayed */
  button->priv->menu = menu;
  g_signal_connect(GTK_MENU_SHELL(menu), "deactivate", G_CALLBACK(menu_destroyed_cb), button);
  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (button), NULL);

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
    mi = gtk_separator_menu_item_new ();
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  }

  /* Display brightness slider - show if there's hardware support for it */
  if ( xfpm_brightness_has_hw (button->priv->brightness) )
  {
    guint brightness_step_count;
    gboolean brightness_exponential;

    max_level = xfpm_brightness_get_max_level (button->priv->brightness);

    /* Setup brightness steps */
    brightness_step_count =
      xfconf_channel_get_uint (button->priv->channel,
                               XFPM_PROPERTIES_PREFIX BRIGHTNESS_STEP_COUNT,
                               10);
    brightness_exponential =
      xfconf_channel_get_bool (button->priv->channel,
                               XFPM_PROPERTIES_PREFIX BRIGHTNESS_EXPONENTIAL,
                               FALSE);
    xfpm_brightness_set_step_count (button->priv->brightness,
                                    brightness_step_count,
                                    brightness_exponential);

    mi = scale_menu_item_new_with_range (button->priv->brightness_min_level, max_level, 1);

    scale_menu_item_set_description_label (SCALE_MENU_ITEM (mi), _("<b>Display brightness</b>"));

    /* range slider */
    button->priv->range = scale_menu_item_get_scale (SCALE_MENU_ITEM (mi));

    /* update the slider to the current brightness level */
    xfpm_brightness_get_level (button->priv->brightness, &current_level);
    gtk_range_set_value (GTK_RANGE (button->priv->range), current_level);

    g_signal_connect_swapped (mi, "value-changed", G_CALLBACK (range_value_changed_cb), button);
    g_signal_connect (mi, "scroll-event", G_CALLBACK (range_scroll_cb), button);
    g_signal_connect (menu, "show", G_CALLBACK (range_show_cb), button);

    /* load and display the brightness icon and force it to 32px size */
    img = gtk_image_new_from_icon_name (XFPM_DISPLAY_BRIGHTNESS_ICON, GTK_ICON_SIZE_DND);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(mi), img);
G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_image_set_pixel_size (GTK_IMAGE (img), 32);
    gtk_widget_show_all (mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
  }

  /* Presentation mode checkbox */
#ifdef XFCE_PLUGIN
  mi = gtk_menu_item_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  label = gtk_label_new_with_mnemonic (_("Presentation _mode"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  sw = gtk_switch_new ();
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), sw, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (mi), box);
  g_signal_connect (G_OBJECT (mi), "activate", G_CALLBACK (power_manager_button_toggle_presentation_mode), sw);
  g_object_bind_property (G_OBJECT (button), PRESENTATION_MODE,
                          G_OBJECT (sw), "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
#else
  mi = gtk_check_menu_item_new_with_mnemonic (_("Presentation _mode"));
  gtk_widget_set_sensitive (mi, TRUE);
  xfconf_g_property_bind (button->priv->channel,
                          XFPM_PROPERTIES_PREFIX PRESENTATION_MODE,
                          G_TYPE_BOOLEAN, G_OBJECT (mi), "active");
#endif
  gtk_widget_show_all (mi);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  /* Show any applications currently inhibiting now */
  display_inhibitors (button, menu);

  /* Power manager settings */
  mi = gtk_menu_item_new_with_mnemonic (_("_Settings..."));
  gtk_widget_show (mi);
  gtk_menu_shell_append (GTK_MENU_SHELL(menu), mi);
  g_signal_connect (G_OBJECT(mi), "activate", G_CALLBACK(xfpm_preferences), NULL);

#if GTK_CHECK_VERSION (3, 22, 0)
  gtk_menu_popup_at_widget (GTK_MENU (menu),
                            GTK_WIDGET (button),
#ifdef XFCE_PLUGIN
                            xfce_panel_plugin_get_orientation (button->priv->plugin) == GTK_ORIENTATION_VERTICAL
                            ? GDK_GRAVITY_WEST : GDK_GRAVITY_NORTH,
                            xfce_panel_plugin_get_orientation (button->priv->plugin) == GTK_ORIENTATION_VERTICAL
                            ? GDK_GRAVITY_EAST : GDK_GRAVITY_SOUTH,
#else
                            GDK_GRAVITY_NORTH,
                            GDK_GRAVITY_SOUTH,
#endif
                            NULL);
#else
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
#endif

#ifdef XFCE_PLUGIN
  xfce_panel_plugin_register_menu (button->priv->plugin,
                                   GTK_MENU (menu));
#endif
}
