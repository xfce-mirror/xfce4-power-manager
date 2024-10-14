/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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
#include "config.h"
#endif

#include "xfpm-settings.h"

#include "common/xfpm-common.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-enum-glib.h"
#include "common/xfpm-enum.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"
#include "common/xfpm-ppd-common.h"
#include "data/interfaces/xfpm-settings_ui.h"
#include "src/xfpm-backlight.h"
#include "src/xfpm-power.h"

#include <cairo-gobject.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <upower.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#define WINDOWING_IS_X11() GDK_IS_X11_DISPLAY (gdk_display_get_default ())
#else
#define WINDOWING_IS_X11() FALSE
#endif

static GtkApplication *app = NULL;
static GtkBuilder *xml = NULL;
static GtkWidget *nt = NULL;

static GtkWidget *on_battery_dpms_sleep = NULL;
static GtkWidget *on_battery_dpms_off = NULL;
static GtkWidget *on_ac_dpms_sleep = NULL;
static GtkWidget *on_ac_dpms_off = NULL;
static GtkWidget *sideview = NULL; /* Sidebar tree view - all devices are in the sideview */
static GtkWidget *device_details_notebook = NULL; /* Displays the details of a deivce */
static GtkWidget *brightness_step_count = NULL;
static GtkWidget *brightness_exponential = NULL;

static gboolean lcd_brightness = FALSE;
static gchar *starting_device_id = NULL;
static UpClient *upower = NULL;

static gint devices_page_num;


enum
{
  COL_SIDEBAR_ICON,
  COL_SIDEBAR_NAME,
  COL_SIDEBAR_INT,
  COL_SIDEBAR_BATTERY_DEVICE, /* Pointer to the UpDevice */
  COL_SIDEBAR_OBJECT_PATH, /* UpDevice object path */
  COL_SIDEBAR_SIGNAL_ID, /* device changed callback id */
  COL_SIDEBAR_VIEW, /* Pointer to GtkTreeView of the devcie details */
  NCOLS_SIDEBAR
};

enum
{
  XFPM_DEVICE_INFO_NAME,
  XFPM_DEVICE_INFO_VALUE,
  XFPM_DEVICE_INFO_LAST
};

static void
update_label (GtkWidget *label,
              GtkWidget *scale,
              gchar *(*format) (gint))
{
  gint value = (gint) gtk_range_get_value (GTK_RANGE (scale));

  gchar *formatted_value = format (value);
  gtk_label_set_text (GTK_LABEL (label), formatted_value);
  g_free (formatted_value);
}

static void
update_label_cb (GtkWidget *scale,
                 GtkWidget *label)
{
  update_label (label, scale, g_object_get_data (G_OBJECT (scale), "format-callback"));
}

static void
combo_box_changed_cb (GtkWidget *combo_box,
                      XfconfChannel *channel)
{
  GtkTreeModel *model;
  GtkTreeIter selected_row;
  gint value = 0;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &selected_row))
    return;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
  gtk_tree_model_get (model, &selected_row, 1, &value, -1);
  xfconf_channel_set_uint (channel, g_object_get_data (G_OBJECT (combo_box), "xfconf-property"), value);
}

static void
set_combo_box_active_by_value (guint new_value,
                               GtkComboBox *combo_box)
{
  GtkTreeModel *list_store;
  GtkTreeIter iter;
  gboolean valid;
  guint list_value;

  list_store = gtk_combo_box_get_model (combo_box);

  for (valid = gtk_tree_model_get_iter_first (list_store, &iter);
       valid;
       valid = gtk_tree_model_iter_next (list_store, &iter))
  {
    gtk_tree_model_get (list_store, &iter, 1, &list_value, -1);
    if (new_value == list_value)
    {
      gtk_combo_box_set_active_iter (combo_box, &iter);
      break;
    }
  }
}

static void
combo_box_xfconf_property_changed_cb (XfconfChannel *channel,
                                      char *property,
                                      GValue *value,
                                      GtkWidget *combo_box)
{
  if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
    set_combo_box_active_by_value (GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (combo_box), "default-value")),
                                   GTK_COMBO_BOX (combo_box));
  else
    set_combo_box_active_by_value (g_value_get_uint (value), GTK_COMBO_BOX (combo_box));
}

static void
view_cursor_changed_cb (GtkTreeView *view,
                        gpointer *user_data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter selected_row;
  gint int_data = 0;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  if (!gtk_tree_selection_get_selected (sel, &model, &selected_row))
    return;

  gtk_tree_model_get (model, &selected_row,
                      COL_SIDEBAR_INT, &int_data,
                      -1);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (device_details_notebook), int_data);
}

/*
 * Format value of GtkRange used with DPMS
 */
static gchar *
format_minutes_value_cb (gint value)
{
  if (value == 0)
    return g_strdup (_("Never"));

  if (value == 1)
    return g_strdup (_("One minute"));

  return g_strdup_printf ("%d %s", value, _("minutes"));
}

/*
 * Format value of GtkRange used with Brightness
 */
static gchar *
format_brightness_value_cb (gint value)
{
  gint min, sec;

  if (value < 1)
    return g_strdup (_("Never"));

  /* value > 6 */
  min = value / 6;
  sec = 10 * (value % 6);

  if (min == 0)
    return g_strdup_printf ("%d %s", sec, _("seconds"));
  else if (min == 1)
    if (sec == 0)
      return g_strdup_printf ("%s", _("One minute"));
    else
      return g_strdup_printf ("%s %d %s", _("One minute"), sec, _("seconds"));
  else if (sec == 0)
    return g_strdup_printf ("%d %s", min, _("minutes"));
  else
    return g_strdup_printf ("%d %s %d %s", min, _("minutes"), sec, _("seconds"));
}

static gchar *
format_brightness_percentage_cb (gint value)
{
  return g_strdup_printf ("%d %s", value, _("%"));
}

static void
brg_scale_xfconf_property_changed_cb (XfconfChannel *channel,
                                      char *property,
                                      GValue *value,
                                      GtkWidget *scale)
{
  guint val;
  if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
    val = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (scale), "default-value"));
  else
    val = g_value_get_uint (value);

  gtk_range_set_value (GTK_RANGE (scale), val / 10);
}

static gboolean
dpms_toggled_cb (GtkWidget *w,
                 gboolean is_active,
                 XfconfChannel *channel)
{
  gtk_widget_set_sensitive (on_ac_dpms_off, is_active);
  gtk_widget_set_sensitive (on_ac_dpms_sleep, is_active);
  gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (xml, "dpms-sleep-label")), is_active);
  gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (xml, "dpms-off-label")), is_active);

  if (GTK_IS_WIDGET (on_battery_dpms_off))
  {
    gtk_widget_set_sensitive (on_battery_dpms_off, is_active);
    gtk_widget_set_sensitive (on_battery_dpms_sleep, is_active);
  }

  return FALSE;
}

static void
sleep_value_changed_cb (GtkWidget *scale,
                        GtkWidget *label)
{
  gboolean on_ac = scale == on_ac_dpms_sleep;
  GtkWidget *off_scale = on_ac ? on_ac_dpms_off : on_battery_dpms_off;
  gint sleep_value = gtk_range_get_value (GTK_RANGE (scale));
  gint off_value = gtk_range_get_value (GTK_RANGE (off_scale));

  if (off_value != 0 && sleep_value >= off_value)
  {
    gtk_range_set_value (GTK_RANGE (off_scale), sleep_value + 1);
  }

  if (lcd_brightness)
  {
    GtkWidget *brg = GTK_WIDGET (gtk_builder_get_object (
      xml, on_ac ? "brightness-inactivity-on-ac" : "brightness-inactivity-on-battery"));
    gint min_value = on_ac ? MIN_BRIGHTNESS_ON_AC : MIN_BRIGHTNESS_ON_BATTERY;
    gint brightness_value = MAX (min_value, 10 * (gint) gtk_range_get_value (GTK_RANGE (brg)));

    if (sleep_value * 60 <= brightness_value && brightness_value != min_value)
    {
      gtk_range_set_value (GTK_RANGE (brg), 0);
    }
  }

  update_label (label, scale, format_minutes_value_cb);
}

static void
off_value_changed_cb (GtkWidget *scale,
                      GtkWidget *label)
{
  gboolean on_ac = scale == on_ac_dpms_off;
  GtkWidget *sleep_scale = on_ac ? on_ac_dpms_sleep : on_battery_dpms_sleep;
  gint off_value = gtk_range_get_value (GTK_RANGE (scale));
  gint sleep_value = gtk_range_get_value (GTK_RANGE (sleep_scale));

  if (sleep_value != 0 && off_value <= sleep_value)
  {
    gtk_range_set_value (GTK_RANGE (sleep_scale), off_value - 1);
  }

  update_label (label, scale, format_minutes_value_cb);
}

static void
brightness_value_changed_cb (GtkWidget *scale,
                             XfconfChannel *channel)
{
  gboolean on_ac = (scale == GTK_WIDGET (gtk_builder_get_object (xml, "brightness-inactivity-on-ac")));
  GtkWidget *dpms_scale = on_ac ? on_ac_dpms_sleep : on_battery_dpms_sleep;
  GtkWidget *label = GTK_WIDGET (gtk_builder_get_object (
    xml, on_ac ? "brightness-inactivity-on-ac-label" : "brightness-inactivity-on-battery-label"));
  gint min_value = on_ac ? MIN_BRIGHTNESS_ON_AC : MIN_BRIGHTNESS_ON_BATTERY;
  const gchar *property = on_ac ? XFPM_PROPERTIES_PREFIX BRIGHTNESS_ON_AC : XFPM_PROPERTIES_PREFIX BRIGHTNESS_ON_BATTERY;
  gint value = MAX (min_value, 10 * (gint) gtk_range_get_value (GTK_RANGE (scale)));
  gint dpms_sleep = gtk_range_get_value (GTK_RANGE (dpms_scale));

  if (value != min_value && dpms_sleep != 0 && dpms_sleep * 60 <= value)
  {
    gtk_range_set_value (GTK_RANGE (dpms_scale), (value / 60) + 1);
  }

  xfconf_channel_set_uint (channel, property, value);
  update_label (label, scale, format_brightness_value_cb);
}

static gboolean
handle_brightness_keys_toggled_cb (GtkWidget *w,
                                   gboolean is_active,
                                   XfconfChannel *channel)
{
  gtk_widget_set_sensitive (brightness_step_count, is_active);
  gtk_widget_set_sensitive (brightness_exponential, is_active);
  gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (xml, "brightness-step-count-label")), is_active);

  return FALSE;
}

static void
xfpm_settings_power_supply (XfconfChannel *channel,
                            gboolean on_ac,
                            GDBusProxy *profiles_proxy,
                            gboolean auth_suspend,
                            gboolean auth_hibernate,
                            gboolean auth_hybrid_sleep,
                            gboolean can_suspend,
                            gboolean can_hibernate,
                            gboolean can_hybrid_sleep,
                            gboolean has_lcd_brightness,
                            gboolean has_lid)
{
  gboolean handle_dpms;
  GtkWidget *inact_timeout, *inact_action, *label, *dpms;
  GtkWidget *lid;
  GtkWidget *brg;
  GtkWidget *brg_level;
  GtkWidget *power_profile;
  GtkListStore *list_store;
  GtkTreeIter iter;
  guint val, default_val;
  GSList *profiles;
  const gchar *widget_id, *property;

  /*
   * Inactivity sleep mode
   */
  list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
  widget_id = on_ac ? "system-sleep-mode-on-ac" : "system-sleep-mode-on-battery";
  inact_action = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
  gtk_combo_box_set_model (GTK_COMBO_BOX (inact_action), GTK_TREE_MODEL (list_store));

  if (can_suspend)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, XFPM_DO_SUSPEND, -1);
  }
  else if (!auth_suspend)
  {
    gtk_widget_set_tooltip_text (inact_action, _("Suspend operation not permitted"));
  }
  else
  {
    gtk_widget_set_tooltip_text (inact_action, _("Suspend operation not supported"));
  }

  if (can_hibernate)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, XFPM_DO_HIBERNATE, -1);
  }
  else if (!auth_hibernate)
  {
    gtk_widget_set_tooltip_text (inact_action, _("Hibernate operation not permitted"));
  }
  else
  {
    gtk_widget_set_tooltip_text (inact_action, _("Hibernate operation not supported"));
  }

  if (can_hybrid_sleep)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Hybrid Sleep"), 1, XFPM_DO_HYBRID_SLEEP, -1);
  }
  else if (!auth_hybrid_sleep)
  {
    gtk_widget_set_tooltip_text (inact_action, _("Hybrid sleep operation not permitted"));
  }
  else
  {
    gtk_widget_set_tooltip_text (inact_action, _("Hybrid sleep operation not supported"));
  }

  property = on_ac ? XFPM_PROPERTIES_PREFIX INACTIVITY_SLEEP_MODE_ON_AC : XFPM_PROPERTIES_PREFIX INACTIVITY_SLEEP_MODE_ON_BATTERY;
  default_val = on_ac ? DEFAULT_INACTIVITY_SLEEP_MODE_ON_AC : DEFAULT_INACTIVITY_SLEEP_MODE_ON_BATTERY;
  gtk_combo_box_set_active (GTK_COMBO_BOX (inact_action), 0);
  val = xfconf_channel_get_uint (channel, property, default_val);
  set_combo_box_active_by_value (val, GTK_COMBO_BOX (inact_action));

  g_object_set_data_full (G_OBJECT (inact_action), "xfconf-property", g_strdup (property), g_free);
  g_signal_connect (inact_action, "changed", G_CALLBACK (combo_box_changed_cb), channel);
  property = on_ac ? "property-changed::" XFPM_PROPERTIES_PREFIX INACTIVITY_SLEEP_MODE_ON_AC
                   : "property-changed::" XFPM_PROPERTIES_PREFIX INACTIVITY_SLEEP_MODE_ON_BATTERY;
  g_object_set_data (G_OBJECT (inact_action), "default-value", GUINT_TO_POINTER (default_val));
  g_signal_connect (channel, property, G_CALLBACK (combo_box_xfconf_property_changed_cb), inact_action);

  /*
   * Inactivity timeout
   */
  widget_id = on_ac ? "system-sleep-inactivity-on-ac" : "system-sleep-inactivity-on-battery";
  inact_timeout = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));

  if (!can_suspend && !can_hibernate && !can_hybrid_sleep)
  {
    gtk_widget_set_sensitive (inact_timeout, FALSE);
    gtk_widget_set_tooltip_text (inact_timeout, _("Hibernate and suspend operations not supported"));
  }
  else if (!auth_suspend && !auth_hibernate && !auth_hybrid_sleep)
  {
    gtk_widget_set_sensitive (inact_timeout, FALSE);
    gtk_widget_set_tooltip_text (inact_timeout, _("Hibernate and suspend operations not permitted"));
  }

  property = on_ac ? XFPM_PROPERTIES_PREFIX INACTIVITY_ON_AC : XFPM_PROPERTIES_PREFIX INACTIVITY_ON_BATTERY;
  default_val = on_ac ? DEFAULT_INACTIVITY_ON_AC : DEFAULT_INACTIVITY_ON_BATTERY;
  gtk_range_set_value (GTK_RANGE (inact_timeout), default_val);
  xfconf_g_property_bind (channel, property, G_TYPE_UINT, gtk_range_get_adjustment (GTK_RANGE (inact_timeout)), "value");
  g_object_set_data (G_OBJECT (inact_timeout), "format-callback", format_minutes_value_cb);

  widget_id = on_ac ? "system-sleep-inactivity-on-ac-label" : "system-sleep-inactivity-on-battery-label";
  label = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
  g_signal_connect (inact_timeout, "value-changed", G_CALLBACK (update_label_cb), label);
  update_label (label, inact_timeout, format_minutes_value_cb);

  /*
   * DPMS settings
   */
  handle_dpms = xfconf_channel_get_bool (channel, XFPM_PROPERTIES_PREFIX DPMS_ENABLED, DEFAULT_DPMS_ENABLED);

  property = on_ac ? XFPM_PROPERTIES_PREFIX DPMS_ON_AC_SLEEP : XFPM_PROPERTIES_PREFIX DPMS_ON_BATTERY_SLEEP;
  dpms = on_ac ? on_ac_dpms_sleep : on_battery_dpms_sleep;
  gtk_widget_set_sensitive (dpms, handle_dpms);
  xfconf_g_property_bind (channel, property, G_TYPE_UINT, gtk_range_get_adjustment (GTK_RANGE (dpms)), "value");

  widget_id = on_ac ? "dpms-sleep-on-ac-label" : "dpms-sleep-on-battery-label";
  label = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
  g_signal_connect (dpms, "value-changed", G_CALLBACK (sleep_value_changed_cb), label);
  update_label (label, dpms, format_minutes_value_cb);

  property = on_ac ? XFPM_PROPERTIES_PREFIX DPMS_ON_AC_OFF : XFPM_PROPERTIES_PREFIX DPMS_ON_BATTERY_OFF;
  dpms = on_ac ? on_ac_dpms_off : on_battery_dpms_off;
  gtk_widget_set_sensitive (dpms, handle_dpms);
  xfconf_g_property_bind (channel, property, G_TYPE_UINT, gtk_range_get_adjustment (GTK_RANGE (dpms)), "value");

  widget_id = on_ac ? "dpms-off-on-ac-label" : "dpms-off-on-battery-label";
  label = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
  g_signal_connect (dpms, "value-changed", G_CALLBACK (off_value_changed_cb), label);
  update_label (label, dpms, format_minutes_value_cb);

  /*
   * Lid switch settings
   */
  widget_id = on_ac ? "lid-on-ac-combo" : "lid-on-battery-combo";
  lid = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
  if (has_lid)
  {
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);

    gtk_combo_box_set_model (GTK_COMBO_BOX (lid), GTK_TREE_MODEL (list_store));

    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Do nothing"), 1, LID_TRIGGER_NOTHING, -1);

    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Switch off display"), 1, LID_TRIGGER_DPMS, -1);

    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Lock screen"), 1, LID_TRIGGER_LOCK_SCREEN, -1);

    if (can_suspend && auth_suspend)
    {
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, LID_TRIGGER_SUSPEND, -1);
    }

    if (can_hibernate && auth_hibernate)
    {
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, LID_TRIGGER_HIBERNATE, -1);
    }

    if (can_hybrid_sleep && auth_hybrid_sleep)
    {
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, 0, _("Hybrid Sleep"), 1, LID_TRIGGER_HYBRID_SLEEP, -1);
    }

    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Shutdown"), 1, LID_TRIGGER_SHUTDOWN, -1);

    property = on_ac ? XFPM_PROPERTIES_PREFIX LID_ACTION_ON_AC : XFPM_PROPERTIES_PREFIX LID_ACTION_ON_BATTERY;
    default_val = on_ac ? DEFAULT_LID_ACTION_ON_AC : DEFAULT_LID_ACTION_ON_BATTERY;
    gtk_combo_box_set_active (GTK_COMBO_BOX (lid), 0);
    val = xfconf_channel_get_uint (channel, property, default_val);
    set_combo_box_active_by_value (val, GTK_COMBO_BOX (lid));

    g_object_set_data_full (G_OBJECT (lid), "xfconf-property", g_strdup (property), g_free);
    g_signal_connect (lid, "changed", G_CALLBACK (combo_box_changed_cb), channel);
    property = on_ac ? "property-changed::" XFPM_PROPERTIES_PREFIX LID_ACTION_ON_AC
                     : "property-changed::" XFPM_PROPERTIES_PREFIX LID_ACTION_ON_BATTERY;
    g_object_set_data (G_OBJECT (lid), "default-value", GUINT_TO_POINTER (default_val));
    g_signal_connect (channel, property, G_CALLBACK (combo_box_xfconf_property_changed_cb), lid);
  }
  else
  {
    widget_id = on_ac ? "lid-action-label1" : "lid-action-label";
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, widget_id)));
    gtk_widget_hide (lid);
    widget_id = on_ac ? "lid-plugged-in-header" : "lid-on-battery-header";
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, widget_id)));
  }

  /*
   * Power profile
   */
  widget_id = on_ac ? "power-profile-on-ac" : "power-profile-on-battery";
  power_profile = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
  if (profiles_proxy != NULL && (profiles = xfpm_ppd_get_profiles (profiles_proxy)) != NULL)
  {
    list_store = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_combo_box_set_model (GTK_COMBO_BOX (power_profile), GTK_TREE_MODEL (list_store));

    for (GSList *l = profiles; l != NULL; l = l->next)
    {
      gchar *profile = l->data;

      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, 0, profile, -1);
      if (g_strcmp0 (profile, on_ac ? DEFAULT_PROFILE_ON_AC : DEFAULT_PROFILE_ON_BATTERY) == 0)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (power_profile), &iter);
    }

    property = on_ac ? XFPM_PROPERTIES_PREFIX PROFILE_ON_AC : XFPM_PROPERTIES_PREFIX PROFILE_ON_BATTERY;
    xfconf_g_property_bind (channel, property, G_TYPE_STRING, power_profile, "active-id");
    g_slist_free_full (profiles, g_free);
  }
  else
  {
    widget_id = on_ac ? "power-profile-label-on-ac" : "power-profile-label-on-battery";
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, widget_id)));
    gtk_widget_hide (power_profile);
  }

  /*
   * Brightness
   */
  widget_id = on_ac ? "brightness-inactivity-on-ac" : "brightness-inactivity-on-battery";
  brg = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
  widget_id = on_ac ? "brightness-level-on-ac" : "brightness-level-on-battery";
  brg_level = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));

  if (has_lcd_brightness)
  {
    property = on_ac ? XFPM_PROPERTIES_PREFIX BRIGHTNESS_ON_AC : XFPM_PROPERTIES_PREFIX BRIGHTNESS_ON_BATTERY;
    default_val = on_ac ? DEFAULT_BRIGHTNESS_ON_AC : DEFAULT_BRIGHTNESS_ON_BATTERY;
    val = xfconf_channel_get_uint (channel, property, default_val);
    gtk_range_set_value (GTK_RANGE (brg), val / 10);
    g_object_set_data (G_OBJECT (brg), "default-value", GUINT_TO_POINTER (default_val));
    property = on_ac ? "property-changed::" XFPM_PROPERTIES_PREFIX BRIGHTNESS_ON_AC
                     : "property-changed::" XFPM_PROPERTIES_PREFIX BRIGHTNESS_ON_BATTERY;
    g_signal_connect (channel, property, G_CALLBACK (brg_scale_xfconf_property_changed_cb), brg);

    widget_id = on_ac ? "brightness-inactivity-on-ac-label" : "brightness-inactivity-on-battery-label";
    label = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
    g_signal_connect (brg, "value-changed", G_CALLBACK (brightness_value_changed_cb), channel);
    update_label (label, brg, format_brightness_value_cb);

    property = on_ac ? XFPM_PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_AC : XFPM_PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_BATTERY;
    xfconf_g_property_bind (channel, property, G_TYPE_UINT, gtk_range_get_adjustment (GTK_RANGE (brg_level)), "value");
    g_object_set_data (G_OBJECT (brg_level), "format-callback", format_brightness_percentage_cb);

    widget_id = on_ac ? "brightness-level-on-ac-label" : "brightness-level-on-battery-label";
    label = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
    g_signal_connect (brg_level, "value-changed", G_CALLBACK (update_label_cb), label);
    update_label (label, brg_level, format_brightness_percentage_cb);
  }
  else
  {
    gtk_widget_hide (brg);
    gtk_widget_hide (brg_level);
  }
}

static void
add_button_combo (XfconfChannel *channel,
                  const gchar *widget_id,
                  const gchar *property,
                  guint default_value,
                  gboolean auth_suspend,
                  gboolean auth_hibernate,
                  gboolean auth_hybrid_sleep,
                  gboolean can_suspend,
                  gboolean can_hibernate,
                  gboolean can_hybrid_sleep,
                  gboolean can_shutdown)
{
  GtkListStore *list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
  GtkWidget *combo = GTK_WIDGET (gtk_builder_get_object (xml, widget_id));
  gchar *property_changed = g_strconcat ("property-changed::", property, NULL);
  GtkTreeIter iter;
  guint value;

  gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (list_store));
  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter, 0, _("Do nothing"), 1, XFPM_DO_NOTHING, -1);

  if (can_suspend && auth_suspend)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, XFPM_DO_SUSPEND, -1);
  }

  if (can_hibernate && auth_hibernate)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, XFPM_DO_HIBERNATE, -1);
  }

  if (can_hybrid_sleep && auth_hybrid_sleep)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Hybrid Sleep"), 1, XFPM_DO_HYBRID_SLEEP, -1);
  }

  if (can_shutdown)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Shutdown"), 1, XFPM_DO_SHUTDOWN, -1);
  }

  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter, 0, _("Ask"), 1, XFPM_ASK, -1);

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
  value = xfconf_channel_get_uint (channel, property, default_value);
  set_combo_box_active_by_value (value, GTK_COMBO_BOX (combo));

  g_object_set_data (G_OBJECT (combo), "default-value", GUINT_TO_POINTER (default_value));
  g_signal_connect (channel, property_changed, G_CALLBACK (combo_box_xfconf_property_changed_cb), combo);
  g_object_set_data_full (G_OBJECT (combo), "xfconf-property", g_strdup (property), g_free);
  g_signal_connect (combo, "changed", G_CALLBACK (combo_box_changed_cb), channel);

  g_free (property_changed);
}

static void
xfpm_settings_general (XfconfChannel *channel,
                       gboolean auth_suspend,
                       gboolean auth_hibernate,
                       gboolean auth_hybrid_sleep,
                       gboolean can_suspend,
                       gboolean can_hibernate,
                       gboolean can_hybrid_sleep,
                       gboolean can_shutdown,
                       gboolean has_sleep_button,
                       gboolean has_hibernate_button,
                       gboolean has_power_button,
                       gboolean has_battery_button)
{
  GtkWidget *switch_widget;
  gboolean valid;

  if (has_power_button)
  {
    add_button_combo (channel, "button-power-combo", XFPM_PROPERTIES_PREFIX POWER_BUTTON_ACTION,
                      DEFAULT_POWER_BUTTON_ACTION, auth_suspend, auth_hibernate, auth_hybrid_sleep,
                      can_suspend, can_hibernate, can_hybrid_sleep, can_shutdown);
  }
  else
  {
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "button-power-combo")));
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "button-power-label")));
  }

  if (has_sleep_button)
  {
    add_button_combo (channel, "button-sleep-combo", XFPM_PROPERTIES_PREFIX SLEEP_BUTTON_ACTION,
                      DEFAULT_SLEEP_BUTTON_ACTION, auth_suspend, auth_hibernate, auth_hybrid_sleep,
                      can_suspend, can_hibernate, can_hybrid_sleep, FALSE);
  }
  else
  {
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "button-sleep-combo")));
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "button-sleep-label")));
  }

  if (has_hibernate_button)
  {
    add_button_combo (channel, "button-hibernate-combo", XFPM_PROPERTIES_PREFIX HIBERNATE_BUTTON_ACTION,
                      DEFAULT_HIBERNATE_BUTTON_ACTION, auth_suspend, auth_hibernate, auth_hybrid_sleep,
                      can_suspend, can_hibernate, can_hybrid_sleep, FALSE);
  }
  else
  {
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "button-hibernate-combo")));
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "button-hibernate-label")));
  }

  if (has_battery_button)
  {
    add_button_combo (channel, "button-battery-combo", XFPM_PROPERTIES_PREFIX BATTERY_BUTTON_ACTION,
                      DEFAULT_BATTERY_BUTTON_ACTION, auth_suspend, auth_hibernate, auth_hybrid_sleep,
                      can_suspend, can_hibernate, can_hybrid_sleep, FALSE);
  }
  else
  {
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "button-battery-combo")));
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "button-battery-label")));
  }

  /*
   * Brightness
   */
  switch_widget = GTK_WIDGET (gtk_builder_get_object (xml, "handle-brightness-keys"));
  gtk_switch_set_active (GTK_SWITCH (switch_widget), DEFAULT_HANDLE_BRIGHTNESS_KEYS);
  xfconf_g_property_bind (channel, XFPM_PROPERTIES_PREFIX HANDLE_BRIGHTNESS_KEYS,
                          G_TYPE_BOOLEAN, switch_widget, "active");
  g_signal_connect (switch_widget, "state-set", G_CALLBACK (handle_brightness_keys_toggled_cb), channel);

  brightness_step_count = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-step-count-spin"));
  gtk_widget_set_tooltip_text (brightness_step_count,
                               _("Number of brightness steps available using keys"));

  xfconf_g_property_bind (channel, XFPM_PROPERTIES_PREFIX BRIGHTNESS_STEP_COUNT,
                          G_TYPE_UINT, brightness_step_count, "value");

  brightness_exponential = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-exponential"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (brightness_exponential), DEFAULT_BRIGHTNESS_EXPONENTIAL);
  xfconf_g_property_bind (channel, XFPM_PROPERTIES_PREFIX BRIGHTNESS_EXPONENTIAL,
                          G_TYPE_BOOLEAN, brightness_exponential, "active");

  valid = xfconf_channel_get_bool (channel, XFPM_PROPERTIES_PREFIX HANDLE_BRIGHTNESS_KEYS, DEFAULT_HANDLE_BRIGHTNESS_KEYS);
  gtk_widget_set_sensitive (brightness_step_count, valid);
  gtk_widget_set_sensitive (brightness_exponential, valid);
  gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (xml, "brightness-step-count-label")), valid);

  /*
   * Appearance
   */
  switch_widget = GTK_WIDGET (gtk_builder_get_object (xml, "show-notifications"));
  gtk_switch_set_active (GTK_SWITCH (switch_widget), DEFAULT_GENERAL_NOTIFICATION);
  xfconf_g_property_bind (channel, XFPM_PROPERTIES_PREFIX GENERAL_NOTIFICATION,
                          G_TYPE_BOOLEAN, switch_widget, "active");

  switch_widget = GTK_WIDGET (gtk_builder_get_object (xml, "show-systray"));
  gtk_switch_set_active (GTK_SWITCH (switch_widget), DEFAULT_SHOW_TRAY_ICON);
  xfconf_g_property_bind (channel, XFPM_PROPERTIES_PREFIX SHOW_TRAY_ICON,
                          G_TYPE_BOOLEAN, switch_widget, "active");
}

static void
xfpm_settings_others (XfconfChannel *channel,
                      gboolean auth_suspend,
                      gboolean auth_hibernate,
                      gboolean auth_hybrid_sleep,
                      gboolean can_suspend,
                      gboolean can_hibernate,
                      gboolean can_hybrid_sleep,
                      gboolean can_shutdown,
                      gboolean has_battery)
{
  GtkWidget *critical_level;
  GtkWidget *lock;
  GtkWidget *dpms;

  /*
   * Battery critical
   */
  if (has_battery)
  {
    GtkTreeIter iter;
    GtkTreeModel *model;
    const gchar *action;

    critical_level = GTK_WIDGET (gtk_builder_get_object (xml, "critical-power-level-spin"));
    gtk_widget_set_tooltip_text (critical_level, _("When all the power sources of the computer reach this charge level"));
    xfconf_g_property_bind (channel, XFPM_PROPERTIES_PREFIX CRITICAL_POWER_LEVEL, G_TYPE_UINT, critical_level, "value");

    add_button_combo (channel, "critical-power-action-combo", XFPM_PROPERTIES_PREFIX CRITICAL_POWER_ACTION,
                      DEFAULT_CRITICAL_POWER_ACTION, auth_suspend, auth_hibernate, auth_hybrid_sleep,
                      can_suspend, can_hibernate, can_hybrid_sleep, can_shutdown);

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (gtk_builder_get_object (xml, "critical-power-action-combo")));
    gtk_tree_model_get_iter_first (model, &iter);
    do
    {
      XfpmShutdownRequest request;
      gtk_tree_model_get (model, &iter, 1, &request, -1);
      if (request == XFPM_DO_NOTHING)
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, _("Notify"), -1);
      if (request == XFPM_ASK && !gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
        break;
    } while (gtk_tree_model_iter_next (model, &iter));

    action = upower != NULL ? up_client_get_critical_action (upower) : "Unknown";
    if (g_strcmp0 (action, "Ignore") == 0)
    {
      gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "critical-warning-button")));
    }
    else
    {
      GtkWidget *label = GTK_WIDGET (gtk_builder_get_object (xml, "critical-warning-label"));
      gchar *text = g_strdup_printf (_("Make sure you set a sufficiently high value here, otherwise the system will trigger its own action before xfce4-power-manager, without being able to prevent it or know exactly when it will do so. The action triggered by the system in this case will be: %s."), action);
      gtk_label_set_text (GTK_LABEL (label), text);
      g_free (text);
    }
  }
  else
  {
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "critical-power-frame")));
  }

  /*
   * Lock screen for suspend/hibernate
   */
  lock = GTK_WIDGET (gtk_builder_get_object (xml, "lock-screen"));

  if (!can_suspend && !can_hibernate && !can_hybrid_sleep)
  {
    gtk_widget_set_sensitive (lock, FALSE);
    gtk_widget_set_tooltip_text (lock, _("Hibernate and suspend operations not supported"));
  }
  else if (!auth_suspend && !auth_hibernate && !auth_hybrid_sleep)
  {
    gtk_widget_set_sensitive (lock, FALSE);
    gtk_widget_set_tooltip_text (lock, _("Hibernate and suspend operations not permitted"));
  }

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (lock), DEFAULT_LOCK_SCREEN_SUSPEND_HIBERNATE);
  xfconf_g_property_bind (channel, XFPM_PROPERTIES_PREFIX LOCK_SCREEN_SUSPEND_HIBERNATE, G_TYPE_BOOLEAN, lock, "active");

  /*
   * Global dpms settings (enable/disable)
   */
  dpms = GTK_WIDGET (gtk_builder_get_object (xml, "handle-dpms"));
  gtk_switch_set_state (GTK_SWITCH (dpms), DEFAULT_DPMS_ENABLED);
  xfconf_g_property_bind (channel, XFPM_PROPERTIES_PREFIX DPMS_ENABLED, G_TYPE_BOOLEAN, dpms, "active");
  g_signal_connect (dpms, "state-set", G_CALLBACK (dpms_toggled_cb), channel);
}

/* Call gtk_tree_iter_free when done with the tree iter */
static GtkTreeIter *
find_device_in_tree (const gchar *object_path)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (!sideview)
    return NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (sideview));

  if (!model)
    return NULL;

  if (gtk_tree_model_get_iter_first (model, &iter))
  {
    do
    {
      gchar *path = NULL;
      gtk_tree_model_get (model, &iter, COL_SIDEBAR_OBJECT_PATH, &path, -1);

      if (g_strcmp0 (path, object_path) == 0)
      {
        g_free (path);
        return gtk_tree_iter_copy (&iter);
      }

      g_free (path);
    } while (gtk_tree_model_iter_next (model, &iter));
  }

  return NULL;
}

/* Call gtk_tree_iter_free when done with the tree iter */
static GtkTreeIter *
find_device_info_name_in_tree (GtkTreeView *view,
                               const gchar *device_info_name)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (!view)
    return NULL;

  model = gtk_tree_view_get_model (view);

  if (!model)
    return NULL;

  if (gtk_tree_model_get_iter_first (model, &iter))
  {
    do
    {
      gchar *name = NULL;
      gtk_tree_model_get (model, &iter, XFPM_DEVICE_INFO_NAME, &name, -1);

      if (g_strcmp0 (name, device_info_name) == 0)
      {
        g_free (name);
        return gtk_tree_iter_copy (&iter);
      }

      g_free (name);
    } while (gtk_tree_model_iter_next (model, &iter));
  }

  return NULL;
}

static gchar *
xfpm_info_get_energy_property (gdouble energy,
                               const gchar *unit)
{
  gchar *val = NULL;

  val = g_strdup_printf ("%.1f %s", energy, unit);

  return val;
}

static void
update_device_info_value_for_name (GtkTreeView *view,
                                   GtkListStore *list_store,
                                   const gchar *name,
                                   const gchar *value)
{
  GtkTreeIter *iter;

  g_return_if_fail (GTK_IS_TREE_VIEW (view));
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (name != NULL);
  /* Value can be NULL */

  XFPM_DEBUG ("updating  name %s with value %s", name, value);

  iter = find_device_info_name_in_tree (view, name);
  if (iter == NULL)
  {
    /* The row doesn't exist yet, add it */
    GtkTreeIter new_iter;
    gtk_list_store_append (list_store, &new_iter);
    iter = gtk_tree_iter_copy (&new_iter);
  }

  if (value != NULL)
  {
    gtk_list_store_set (list_store, iter,
                        XFPM_DEVICE_INFO_NAME, name,
                        XFPM_DEVICE_INFO_VALUE, value,
                        -1);
  }
  else
  {
    /* The value no longer applies, remove the row */
    gtk_list_store_remove (list_store, iter);
  }

  gtk_tree_iter_free (iter);
}

static void
update_sideview_icon (UpDevice *device,
                      gint scale_factor)
{
  GtkListStore *list_store;
  GtkTreeIter *iter;
  GdkPixbuf *pix;
  cairo_surface_t *surface = NULL;
  guint type = 0;
  gchar *name = NULL, *icon_name = NULL, *model = NULL, *vendor = NULL;
  const gchar *object_path = up_device_get_object_path (device);

  list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (sideview)));

  iter = find_device_in_tree (object_path);

  /* quit if device doesn't exist in the sidebar */
  if (!iter)
    return;

  /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
  g_object_get (device,
                "kind", &type,
                "vendor", &vendor,
                "model", &model,
                NULL);


  if (upower != NULL)
  {
    name = get_device_description (upower, device);
    icon_name = get_device_icon_name (upower, device, FALSE);
  }

  pix = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (),
                                            icon_name,
                                            48,
                                            scale_factor,
                                            GTK_ICON_LOOKUP_USE_BUILTIN
                                              | GTK_ICON_LOOKUP_FORCE_SIZE,
                                            NULL);
  if (G_LIKELY (pix != NULL))
  {
    surface = gdk_cairo_surface_create_from_pixbuf (pix, scale_factor, NULL);
    g_object_unref (pix);
  }

  gtk_list_store_set (list_store, iter,
                      COL_SIDEBAR_ICON, surface,
                      COL_SIDEBAR_NAME, name,
                      -1);

  if (G_LIKELY (surface != NULL))
  {
    cairo_surface_destroy (surface);
  }

  g_free (name);
  g_free (icon_name);

  gtk_tree_iter_free (iter);
}

static void
update_device_details (UpDevice *device)
{
  GtkTreeView *view;
  GtkListStore *list_store;
  GtkTreeIter *sideview_iter;
  gchar *str;
  guint type = 0, tech = 0;
  gdouble energy_full_design = -1.0, energy_full = -1.0, energy_empty = -1.0, energy_rate = -1.0, voltage = -1.0, percent = -1.0;
  gboolean p_supply = FALSE;
  gchar *model = NULL, *vendor = NULL, *serial = NULL;
  const gchar *battery_type = NULL;
  const gchar *object_path = up_device_get_object_path (device);

  sideview_iter = find_device_in_tree (object_path);

  /* quit if device doesn't exist in the sidebar */
  if (sideview_iter == NULL)
    return;

  gtk_tree_model_get (gtk_tree_view_get_model (GTK_TREE_VIEW (sideview)), sideview_iter,
                      COL_SIDEBAR_VIEW, &view,
                      -1);

  list_store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

  /**
   * Add/Update Device information:
   **/
  /*Device*/
  update_device_info_value_for_name (view,
                                     list_store,
                                     _("Device"),
                                     g_str_has_prefix (object_path, UPOWER_PATH_DEVICE) ? object_path + strlen (UPOWER_PATH_DEVICE) : object_path);

  /*Type*/
  /* hack, this depends on XFPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
  g_object_get (device,
                "kind", &type,
                "power-supply", &p_supply,
                "model", &model,
                "vendor", &vendor,
                "serial", &serial,
                "technology", &tech,
                "energy-full-design", &energy_full_design,
                "energy-full", &energy_full,
                "energy-empty", &energy_empty,
                "energy-rate", &energy_rate,
                "voltage", &voltage,
                "percentage", &percent,
                NULL);

  if (type != UP_DEVICE_KIND_UNKNOWN)
  {
    battery_type = xfpm_power_translate_device_type (type);
    update_device_info_value_for_name (view, list_store, _("Type"), battery_type);
  }

  update_device_info_value_for_name (view,
                                     list_store,
                                     _("PowerSupply"),
                                     p_supply ? _("True") : _("False"));

  if (type != UP_DEVICE_KIND_LINE_POWER)
  {
    /*Model*/
    if (model && strlen (model) > 0)
    {
      update_device_info_value_for_name (view, list_store, _("Model"), model);
    }

    update_device_info_value_for_name (view, list_store, _("Technology"), xfpm_power_translate_technology (tech));

    /*Percentage*/
    if (percent >= 0)
    {
      str = g_strdup_printf ("%d%%", (guint) percent);

      update_device_info_value_for_name (view, list_store, _("Current charge"), str);

      g_free (str);
    }

    if (energy_full_design > 0)
    {
      /* TRANSLATORS: Unit here is Watt hour*/
      str = xfpm_info_get_energy_property (energy_full_design, _("Wh"));

      update_device_info_value_for_name (view, list_store, _("Fully charged (design)"), str);

      g_free (str);
    }

    if (energy_full > 0)
    {
      gchar *str2;

      /* TRANSLATORS: Unit here is Watt hour*/
      str = xfpm_info_get_energy_property (energy_full, _("Wh"));
      str2 = g_strdup_printf ("%s (%d%%)", str, (guint) (energy_full / energy_full_design * 100));

      update_device_info_value_for_name (view, list_store, _("Fully charged"), str2);

      g_free (str);
      g_free (str2);
    }

    if (energy_empty > 0)
    {
      /* TRANSLATORS: Unit here is Watt hour*/
      str = xfpm_info_get_energy_property (energy_empty, _("Wh"));

      update_device_info_value_for_name (view, list_store, _("Energy empty"), str);

      g_free (str);
    }

    if (energy_rate > 0)
    {
      /* TRANSLATORS: Unit here is Watt*/
      str = xfpm_info_get_energy_property (energy_rate, _("W"));

      update_device_info_value_for_name (view, list_store, _("Energy rate"), str);

      g_free (str);
    }

    if (voltage > 0)
    {
      /* TRANSLATORS: Unit here is Volt*/
      str = xfpm_info_get_energy_property (voltage, _("V"));

      update_device_info_value_for_name (view, list_store, _("Voltage"), str);

      g_free (str);
    }

    if (vendor && strlen (vendor) > 0)
    {
      update_device_info_value_for_name (view, list_store, _("Vendor"), vendor);
    }

    if (serial && strlen (serial) > 0)
    {
      update_device_info_value_for_name (view, list_store, _("Serial"), serial);
    }
  }

  update_sideview_icon (device, gtk_widget_get_scale_factor (GTK_WIDGET (view)));
  gtk_widget_show_all (GTK_WIDGET (view));
}

static void
device_changed_cb (UpDevice *device,
                   GParamSpec *pspec,
                   gpointer user_data)
{
  update_device_details (device);
}

static void
add_device (UpDevice *device)
{
  GtkTreeIter iter, *device_iter;
  GtkListStore *sideview_store, *devices_store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;
  GtkWidget *frame, *view;
  const gchar *object_path = up_device_get_object_path (device);
  gulong signal_id;
  guint index;
  static gboolean first_run = TRUE;

  /* don't add the same device twice */
  device_iter = find_device_in_tree (object_path);
  if (device_iter)
  {
    gtk_tree_iter_free (device_iter);
    return;
  }

  /* Make sure the devices tab is shown */
  gtk_widget_show (gtk_notebook_get_nth_page (GTK_NOTEBOOK (nt), devices_page_num));

  signal_id = g_signal_connect_object (device, "notify", G_CALLBACK (device_changed_cb), sideview, 0);

  sideview_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (sideview)));

  index = gtk_notebook_get_n_pages (GTK_NOTEBOOK (device_details_notebook));

  /* Create the page that the update_device_details will update/replace */
  frame = gtk_frame_new (NULL);
  view = gtk_tree_view_new ();
  gtk_container_add (GTK_CONTAINER (frame), view);
  gtk_widget_show_all (frame);
  gtk_notebook_append_page (GTK_NOTEBOOK (device_details_notebook), frame, NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

  /* Create the list store that the devices view will display */
  devices_store = gtk_list_store_new (XFPM_DEVICE_INFO_LAST, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (devices_store));

  /* Create the headers for this item in the device details tab */
  renderer = gtk_cell_renderer_text_new ();

  /*Device Attribute*/
  col = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (col, renderer, FALSE);
  gtk_tree_view_column_set_attributes (col, renderer, "text", XFPM_DEVICE_INFO_NAME, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

  /*Device Attribute Value*/
  col = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (col, renderer, FALSE);
  gtk_tree_view_column_set_attributes (col, renderer, "text", XFPM_DEVICE_INFO_VALUE, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

  /* Add the new device to the sidebar */
  gtk_list_store_append (sideview_store, &iter);
  gtk_list_store_set (sideview_store, &iter,
                      COL_SIDEBAR_INT, index,
                      COL_SIDEBAR_BATTERY_DEVICE, device,
                      COL_SIDEBAR_OBJECT_PATH, object_path,
                      COL_SIDEBAR_SIGNAL_ID, signal_id,
                      COL_SIDEBAR_VIEW, view,
                      -1);

  /* Add the icon and description for the device */
  update_device_details (device);

  /* See if we're to select this device, for it to be selected,
   * the starting_device_id must be unset and the this is the first
   * time add_device is called (i.e. select the first device) or
   * our current device matches starting_device_id. */
  if ((starting_device_id == NULL && first_run) || (g_strcmp0 (object_path, starting_device_id) == 0))
  {
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sideview));

    gtk_tree_selection_select_iter (selection, &iter);
    view_cursor_changed_cb (GTK_TREE_VIEW (sideview), NULL);
  }

  first_run = FALSE;
}

static void
remove_device (const gchar *object_path)
{
  GtkTreeIter *iter;
  GtkListStore *list_store;
  gulong signal_id;
  UpDevice *device;

  iter = find_device_in_tree (object_path);

  if (iter == NULL)
    return;

  list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (sideview)));

  gtk_tree_model_get (GTK_TREE_MODEL (list_store), iter,
                      COL_SIDEBAR_SIGNAL_ID, &signal_id,
                      COL_SIDEBAR_BATTERY_DEVICE, &device,
                      -1);

  if (device)
  {
    g_signal_handler_disconnect (device, signal_id);
    g_object_unref (device);
  }

  gtk_list_store_remove (list_store, iter);

  /* If there are no devices left, hide the devices tab */
  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), iter))
    gtk_widget_hide (gtk_notebook_get_nth_page (GTK_NOTEBOOK (nt), devices_page_num));
}

static void
device_added_cb (UpClient *upclient,
                 UpDevice *device,
                 gpointer user_data)
{
  add_device (device);
}

static void
device_removed_cb (UpClient *upclient,
                   const gchar *object_path,
                   gpointer user_data)
{
  remove_device (object_path);
}

static void
add_all_devices (void)
{
  GPtrArray *array = up_client_get_devices2 (upower);
  guint i;

  if (array)
  {
    for (i = 0; i < array->len; i++)
    {
      UpDevice *device = g_ptr_array_index (array, i);

      add_device (device);
    }
    g_ptr_array_free (array, TRUE);
  }
}

static void
settings_create_devices_list (void)
{
  upower = up_client_new ();

  if (upower != NULL)
  {
    g_signal_connect_object (upower, "device-added", G_CALLBACK (device_added_cb), sideview, 0);
    g_signal_connect_object (upower, "device-removed", G_CALLBACK (device_removed_cb), sideview, 0);
    add_all_devices ();
  }
}

static void
settings_quit (GtkWidget *widget,
               XfconfChannel *channel)
{
  g_object_unref (channel);
  xfconf_shutdown ();
  gtk_widget_destroy (widget);
  if (upower != NULL)
    g_object_unref (upower);

  /* initiate the quit action on the application so it terminates */
  g_action_group_activate_action (G_ACTION_GROUP (app), "quit", NULL);
}

static void
dialog_response_cb (GtkDialog *dialog,
                    gint response,
                    XfconfChannel *channel)
{
  switch (response)
  {
    case GTK_RESPONSE_HELP:
      xfce_dialog_show_help_with_version (NULL, "xfce4-power-manager", "start", NULL, XFPM_VERSION_SHORT);
      break;

    case GTK_RESPONSE_REJECT:
    {
      GError *error = NULL;
      if (!g_spawn_command_line_async ("xfce4-screensaver-preferences", &error))
      {
        g_warning ("Unable to start screensaver preferences: %s", error->message);
        g_error_free (error);
      }
      break;
    }

    default:
      settings_quit (GTK_WIDGET (dialog), channel);
      break;
  }
}

#ifdef ENABLE_X11
static void
delete_event_cb (GtkWidget *plug,
                 GdkEvent *ev,
                 XfconfChannel *channel)
{
  settings_quit (plug, channel);
}
#endif

GtkWidget *
xfpm_settings_dialog_new (XfconfChannel *channel,
                          gboolean auth_suspend,
                          gboolean auth_hibernate,
                          gboolean auth_hybrid_sleep,
                          gboolean can_suspend,
                          gboolean can_hibernate,
                          gboolean can_hybrid_sleep,
                          gboolean can_shutdown,
                          gboolean has_battery,
                          gboolean has_lcd_brightness,
                          gboolean has_lid,
                          gboolean has_sleep_button,
                          gboolean has_hibernate_button,
                          gboolean has_power_button,
                          gboolean has_battery_button,
#ifdef ENABLE_X11
                          Window id,
#endif
                          gchar *device_id,
                          GtkApplication *gtk_app)
{
  GtkWidget *dialog;
  GtkWidget *viewport;
  GtkWidget *hbox;
  GtkWidget *stack;
  GtkStyleContext *context;
  GtkListStore *list_store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;
  GError *error = NULL;
  GtkCssProvider *css_provider;
  GDBusProxy *profiles_proxy = xfpm_ppd_g_dbus_proxy_new ();
  gchar *path;

  XFPM_DEBUG ("auth_suspend=%s auth_hibernate=%s auth_hybrid_sleep=%s "
              "can_suspend=%s can_hibernate=%s can_hybrid_sleep=%s can_shutdown=%s "
              "has_battery=%s has_lcd_brightness=%s has_lid=%s has_sleep_button=%s "
              "has_hibernate_button=%s has_power_button=%s has_battery_button=%s",
              xfpm_bool_to_string (auth_suspend), xfpm_bool_to_string (auth_hibernate),
              xfpm_bool_to_string (auth_hybrid_sleep), xfpm_bool_to_string (can_suspend),
              xfpm_bool_to_string (can_hibernate), xfpm_bool_to_string (can_hybrid_sleep),
              xfpm_bool_to_string (can_shutdown), xfpm_bool_to_string (has_battery),
              xfpm_bool_to_string (has_lcd_brightness), xfpm_bool_to_string (has_lid),
              xfpm_bool_to_string (has_sleep_button), xfpm_bool_to_string (has_hibernate_button),
              xfpm_bool_to_string (has_power_button), xfpm_bool_to_string (has_battery_button));

  xml = xfpm_builder_new_from_string (xfpm_settings_ui, &error);

  if (G_UNLIKELY (error))
  {
    xfce_dialog_show_error (NULL, error, "%s", _("Check your power manager installation"));
    g_error ("%s", error->message);
  }

  path = g_find_program_in_path ("xfce4-screensaver-preferences");
  if (path != NULL)
  {
    gtk_widget_show (GTK_WIDGET (gtk_builder_get_object (xml, "screensaver-button")));
    g_free (path);
  }

  lcd_brightness = has_lcd_brightness;
  starting_device_id = device_id;

  on_battery_dpms_sleep = GTK_WIDGET (gtk_builder_get_object (xml, "dpms-sleep-on-battery"));
  on_battery_dpms_off = GTK_WIDGET (gtk_builder_get_object (xml, "dpms-off-on-battery"));
  on_ac_dpms_sleep = GTK_WIDGET (gtk_builder_get_object (xml, "dpms-sleep-on-ac"));
  on_ac_dpms_off = GTK_WIDGET (gtk_builder_get_object (xml, "dpms-off-on-ac"));

  dialog = GTK_WIDGET (gtk_builder_get_object (xml, "xfpm-settings-dialog"));
  nt = GTK_WIDGET (gtk_builder_get_object (xml, "main-notebook"));

  /* Set Gtk style */
  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider,
                                   ".xfce4-scale-label { padding-bottom: 0; }",
                                   -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  /* Devices listview */
  sideview = gtk_tree_view_new ();
  list_store = gtk_list_store_new (NCOLS_SIDEBAR,
                                   CAIRO_GOBJECT_TYPE_SURFACE, /* COL_SIDEBAR_ICON */
                                   G_TYPE_STRING, /* COL_SIDEBAR_NAME */
                                   G_TYPE_INT, /* COL_SIDEBAR_INT */
                                   G_TYPE_OBJECT, /* COL_SIDEBAR_BATTERY_DEVICE */
                                   G_TYPE_STRING, /* COL_SIDEBAR_OBJECT_PATH */
                                   G_TYPE_ULONG, /* COL_SIDEBAR_SIGNAL_ID */
                                   G_TYPE_POINTER /* COL_SIDEBAR_VIEW */
  );

  gtk_tree_view_set_model (GTK_TREE_VIEW (sideview), GTK_TREE_MODEL (list_store));

  col = gtk_tree_view_column_new ();

  renderer = gtk_cell_renderer_pixbuf_new ();

  gtk_tree_view_column_pack_start (col, renderer, FALSE);
  gtk_tree_view_column_set_attributes (col, renderer, "surface", 0, NULL);

  /* The device label */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (col, renderer, FALSE);
  gtk_tree_view_column_set_attributes (col, renderer, "markup", 1, NULL);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (sideview), FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (sideview), col);

  g_signal_connect (sideview, "cursor-changed", G_CALLBACK (view_cursor_changed_cb), NULL);

  /* Pack the content of the devices tab */
  device_details_notebook = gtk_notebook_new ();

  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (device_details_notebook), FALSE);
  context = gtk_widget_get_style_context (GTK_WIDGET (device_details_notebook));
  gtk_style_context_add_class (context, "frame");
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);

  viewport = gtk_viewport_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (viewport), sideview);
  gtk_box_pack_start (GTK_BOX (hbox), viewport, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), device_details_notebook, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
  devices_page_num = gtk_notebook_append_page (GTK_NOTEBOOK (nt), hbox, gtk_label_new (_("Devices")));

  gtk_widget_show_all (sideview);
  gtk_widget_show_all (viewport);
  gtk_widget_show_all (hbox);
  gtk_widget_hide (gtk_notebook_get_nth_page (GTK_NOTEBOOK (nt), devices_page_num));

  settings_create_devices_list ();

  xfpm_settings_power_supply (channel, TRUE, profiles_proxy, auth_suspend, auth_hibernate, auth_hybrid_sleep,
                              can_suspend, can_hibernate, can_hybrid_sleep, has_lcd_brightness, has_lid);

  if (has_battery)
  {
    xfpm_settings_power_supply (channel, FALSE, profiles_proxy, auth_suspend, auth_hibernate, auth_hybrid_sleep,
                                can_suspend, can_hibernate, can_hybrid_sleep, has_lcd_brightness, has_lid);
    if (upower != NULL && !up_client_get_on_battery (upower))
    {
      GtkWidget *widget;
      stack = GTK_WIDGET (gtk_builder_get_object (xml, "system-stack"));
      widget = gtk_stack_get_child_by_name (GTK_STACK (stack), "page1");
      gtk_stack_set_visible_child (GTK_STACK (stack), widget);
      stack = GTK_WIDGET (gtk_builder_get_object (xml, "display-stack"));
      widget = gtk_stack_get_child_by_name (GTK_STACK (stack), "page1");
      gtk_stack_set_visible_child (GTK_STACK (stack), widget);
    }
  }
  else
  {
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "critical-power-frame")));
    stack = GTK_WIDGET (gtk_builder_get_object (xml, "system-stack"));
    gtk_widget_hide (gtk_stack_get_child_by_name (GTK_STACK (stack), "page0"));
    stack = GTK_WIDGET (gtk_builder_get_object (xml, "display-stack"));
    gtk_widget_hide (gtk_stack_get_child_by_name (GTK_STACK (stack), "page0"));
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "system-stack-switcher")));
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "display-stack-switcher")));
  }

  xfpm_settings_general (channel, auth_suspend, auth_hibernate, auth_hybrid_sleep, can_suspend, can_hibernate,
                         can_hybrid_sleep, can_shutdown, has_sleep_button, has_hibernate_button,
                         has_power_button, has_battery_button);

  xfpm_settings_others (channel, auth_suspend, auth_hibernate, auth_hybrid_sleep, can_suspend, can_hibernate,
                        can_hybrid_sleep, can_shutdown, has_battery);

  if (!has_lcd_brightness)
  {
    GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object (xml, "label4"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-level-label"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-inactivity-label"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-level-vbox"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-inactivity-vbox"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "label6"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-level-label1"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-inactivity-label1"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-level-vbox1"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-inactivity-vbox1"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "handle-brightness-keys"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "handle-brightness-keys-label"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "hbox4"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "brightness-step-count-label"));
    gtk_widget_hide (widget);
  }

  if (!WINDOWING_IS_X11 ())
  {
    GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object (xml, "buttons-frame"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "hbox21"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "dpms-sleep-label"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "dpms-sleep-box"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "dpms-sleep-label1"));
    gtk_widget_hide (widget);
    widget = GTK_WIDGET (gtk_builder_get_object (xml, "dpms-sleep-box1"));
    gtk_widget_hide (widget);
  }

#ifdef ENABLE_X11
  if (id != 0 && WINDOWING_IS_X11 ())
  {
    GtkWidget *plugged_box = GTK_WIDGET (gtk_builder_get_object (xml, "plug-child"));
    GtkWidget *parent = gtk_widget_get_parent (plugged_box);
    GtkWidget *plug = gtk_plug_new (id);

    gtk_widget_show (plug);
    if (parent)
    {
      g_object_ref (plugged_box);
      gtk_container_remove (GTK_CONTAINER (parent), plugged_box);
      gtk_container_add (GTK_CONTAINER (plug), plugged_box);
      g_object_unref (plugged_box);
    }

    g_signal_connect (plug, "delete-event", G_CALLBACK (delete_event_cb), channel);
    gdk_notify_startup_complete ();
  }
  else
#endif
  {
    g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), channel);
    gtk_widget_show (dialog);
  }

  gtk_builder_connect_signals (xml, channel);

  /* If we passed in a device to display, show the devices tab now, otherwise hide it */
  if (device_id != NULL)
  {
    gtk_widget_show (gtk_notebook_get_nth_page (GTK_NOTEBOOK (nt), devices_page_num));
    gtk_notebook_set_current_page (GTK_NOTEBOOK (nt), devices_page_num);
  }

  /* keep a pointer to the GtkApplication instance so we can signal a
   * quit message */
  app = gtk_app;

  if (profiles_proxy != NULL)
    g_object_unref (profiles_proxy);

  return dialog;
}

void
xfpm_settings_show_device_id (gchar *device_id)
{
  GtkTreeIter *device_iter;

  if (device_id == NULL)
    return;

  gtk_widget_show (gtk_notebook_get_nth_page (GTK_NOTEBOOK (nt), devices_page_num));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (nt), devices_page_num);

  XFPM_DEBUG ("device_id %s", device_id);

  device_iter = find_device_in_tree (device_id);
  if (device_iter)
  {
    GtkTreeSelection *selection;

    XFPM_DEBUG ("device found");

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sideview));

    gtk_tree_selection_select_iter (selection, device_iter);
    view_cursor_changed_cb (GTK_TREE_VIEW (sideview), NULL);
    gtk_tree_iter_free (device_iter);
  }
}
