/*
 * * Copyright (C) 2008-2009 Ali <aliov@xfce.org>
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
#include <glade/glade.h>

#include <xfconf/xfconf.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "libxfpm/xfpm-common.h"
#include "libxfpm/xfpm-string.h"

#include "xfpm-settings_glade.h"

#include "xfpm-config.h"

static 	GladeXML *xml 				= NULL;
#ifdef HAVE_DPMS
static  GtkWidget *on_battery_dpms_sleep 	= NULL;
static  GtkWidget *on_battery_dpms_off  	= NULL;
static  GtkWidget *on_ac_dpms_sleep 		= NULL;
static  GtkWidget *on_ac_dpms_off 		= NULL;
static  GtkWidget *sleep_dpms_mode 		= NULL;
static  GtkWidget *suspend_dpms_mode		= NULL;
#endif

/*
 * Callback settings 
 */
static void
battery_critical_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gint value = 0;
    
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (w), &selected_row))
	return;
	
    model = gtk_combo_box_get_model (GTK_COMBO_BOX(w));
   
    gtk_tree_model_get(model,
                       &selected_row,
                       1,
                       &value,
                       -1);
		       
    if (!xfconf_channel_set_string (channel, CRITICAL_BATT_ACTION_CFG, xfpm_int_to_shutdown_string(value)) )
    {
	g_critical ("Cannot set value for property %s\n", CRITICAL_BATT_ACTION_CFG);
    }
}

static void
set_show_tray_icon_cb (GtkWidget *w, XfconfChannel *channel)
{
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gint value = 0;
    
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (w), &selected_row))
	return;
	
    model = gtk_combo_box_get_model (GTK_COMBO_BOX(w));
   
    gtk_tree_model_get(model,
                       &selected_row,
                       1,
                       &value,
                       -1);
    
    if (!xfconf_channel_set_uint (channel, SHOW_TRAY_ICON_CFG, value) )
    {
	g_critical ("Cannot set value for property %s\n", SHOW_TRAY_ICON_CFG);
    }
}

static void
set_sleep_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gint value = 0;
    
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (w), &selected_row))
	return;
	
    model = gtk_combo_box_get_model (GTK_COMBO_BOX(w));
   
    gtk_tree_model_get(model,
                       &selected_row,
                       1,
                       &value,
                       -1);
    
    if (!xfconf_channel_set_string (channel, SLEEP_SWITCH_CFG, xfpm_int_to_shutdown_string(value) ) )
    {
	g_critical ("Cannot set value for property %s\n", SLEEP_SWITCH_CFG);
    }
}

static void
power_save_toggled_cb (GtkWidget *w, XfconfChannel *channel)
{
    gboolean val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(w));
    
    if (!xfconf_channel_set_bool (channel, POWER_SAVE_ON_BATTERY, val) )
    {
	g_critical ("Cannot set value for property %s\n", POWER_SAVE_ON_BATTERY);
    }
}

static void
notify_toggled_cb (GtkWidget *w, XfconfChannel *channel)
{
    gboolean val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(w));
    
    if (!xfconf_channel_set_bool (channel, GENERAL_NOTIFICATION_CFG, val) )
    {
	g_critical ("Cannot set value for property %s\n", GENERAL_NOTIFICATION_CFG);
    }
}

#ifdef HAVE_DPMS
static void
set_dpms_sleep_mode (GtkWidget *w, XfconfChannel *channel)
{
    if (!xfconf_channel_set_string (channel, DPMS_SLEEP_MODE, "sleep") )
    {
	g_critical ("Cannot set value sleep for property %s\n", DPMS_SLEEP_MODE);
    }
}

static void
set_dpms_suspend_mode (GtkWidget *w, XfconfChannel *channel)
{
    if (!xfconf_channel_set_string (channel, DPMS_SLEEP_MODE, "suspend") )
    {
	g_critical ("Cannot set value sleep for property %s\n", DPMS_SLEEP_MODE);
    }
}

static void
dpms_toggled_cb (GtkWidget *w, XfconfChannel *channel)
{
    gboolean val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(w));
    
    xfconf_channel_set_bool (channel, DPMS_ENABLED_CFG, val);
    
    gtk_widget_set_sensitive (on_ac_dpms_off, val);
    gtk_widget_set_sensitive (on_ac_dpms_sleep, val);
    
    if ( GTK_IS_WIDGET (on_battery_dpms_off ) )
    {
	gtk_widget_set_sensitive (on_battery_dpms_off, val);
    	gtk_widget_set_sensitive (on_battery_dpms_sleep, val);
    }
}

static void
sleep_on_battery_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    gint off_value    = (gint)gtk_range_get_value (GTK_RANGE(on_battery_dpms_off));
    gint sleep_value  = (gint)gtk_range_get_value (GTK_RANGE(w));
    
    if ( off_value != 0 )
    {
	if ( sleep_value >= off_value )
	{
	    gtk_range_set_value (GTK_RANGE(on_battery_dpms_off), sleep_value + 1 );
	}
    }
    
    if (!xfconf_channel_set_uint (channel, ON_BATT_DPMS_SLEEP, sleep_value))
    {
	g_critical ("Cannot set value for property %s\n", ON_BATT_DPMS_SLEEP);
    }
}

static void
off_on_battery_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    gint off_value    = (gint)gtk_range_get_value (GTK_RANGE(w));
    gint sleep_value  = (gint)gtk_range_get_value (GTK_RANGE(on_battery_dpms_sleep));
    
    if ( sleep_value != 0 )
    {
	if ( off_value <= sleep_value )
	{
	    gtk_range_set_value (GTK_RANGE(on_battery_dpms_sleep), off_value -1 );
	}
    }
    
    if (!xfconf_channel_set_uint (channel, ON_BATT_DPMS_OFF, off_value))
    {
	g_critical ("Cannot set value for property %s\n", ON_BATT_DPMS_OFF);
    }
    
}

static void
sleep_on_ac_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    gint off_value    = (gint)gtk_range_get_value (GTK_RANGE(on_ac_dpms_off));
    gint sleep_value  = (gint)gtk_range_get_value (GTK_RANGE(w));
    
    if ( off_value > 60 || sleep_value > 60 )
    	return;
	
    if ( off_value != 0 )
    {
	if ( sleep_value >= off_value )
	{
	    gtk_range_set_value (GTK_RANGE(on_ac_dpms_off), sleep_value + 1 );
	}
    }

    if (!xfconf_channel_set_uint (channel, ON_AC_DPMS_SLEEP, sleep_value))
    {
	g_critical ("Cannot set value for property %s\n", ON_AC_DPMS_SLEEP);
    }
}

static void
off_on_ac_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    gint off_value    = (gint)gtk_range_get_value (GTK_RANGE(w));
    gint sleep_value  = (gint)gtk_range_get_value (GTK_RANGE(on_ac_dpms_sleep));
    
    if ( off_value > 60 || sleep_value > 60 )
    	return;
    
    if ( sleep_value != 0 )
    {
	if ( off_value <= sleep_value )
	{
	    gtk_range_set_value (GTK_RANGE(on_ac_dpms_sleep), off_value -1 );
	}
    }

    if (!xfconf_channel_set_uint (channel, ON_AC_DPMS_OFF, off_value))
    {
	g_critical ("Cannot set value for property %s\n", ON_AC_DPMS_OFF);
    }
}

/*
 * Format value of GtkRange used with DPMS
 */
static gchar *
format_dpms_value_cb (GtkScale *scale, gdouble value)
{
    if ( (int)value == 0 )
    	return g_strdup _("Never");
        
    return g_strdup_printf ("%d %s", (int)value, _("Minutes"));
}
#endif

static void
on_battery_lid_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gint value = 0;
    
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (w), &selected_row))
	return;
	
    model = gtk_combo_box_get_model (GTK_COMBO_BOX(w));
   
    gtk_tree_model_get(model,
                       &selected_row,
                       1,
                       &value,
                       -1);
    
    if (!xfconf_channel_set_string (channel, LID_SWITCH_ON_BATTERY_CFG, xfpm_int_to_shutdown_string(value)) )
    {
	g_critical ("Cannot set value for property %s\n", LID_SWITCH_ON_BATTERY_CFG);
    }
}

static void
on_ac_lid_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gint value = 0;
    
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (w), &selected_row))
	return;
	
    model = gtk_combo_box_get_model (GTK_COMBO_BOX(w));
   
    gtk_tree_model_get(model,
                       &selected_row,
                       1,
                       &value,
                       -1);
		       
    if (!xfconf_channel_set_string (channel, LID_SWITCH_ON_AC_CFG, xfpm_int_to_shutdown_string(value)) )
    {
	g_critical ("Cannot set value for property %s\n", LID_SWITCH_ON_AC_CFG);
    }
} 

static void
xfpm_settings_on_battery (XfconfChannel *channel)
{
    gint val;
    GtkWidget *battery_critical = glade_xml_get_widget (xml, "battery-critical-combox");
    GtkListStore *list_store;
    GtkTreeIter iter;
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    
    gtk_combo_box_set_model (GTK_COMBO_BOX(battery_critical), GTK_TREE_MODEL(list_store));
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Nothing"), 1, 0, -1);
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, 1, -1);
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, 2, -1);
    
    g_signal_connect (battery_critical, "changed", 
		      G_CALLBACK(battery_critical_changed_cb), channel);
		      
    gchar *str = xfconf_channel_get_string (channel, CRITICAL_BATT_ACTION_CFG, "Nothing");
    
    val = xfpm_shutdown_string_to_int (str);
    
    if ( val == -1 || val == 3 /*we don't do shutdown here */) 
    {
	g_warning ("Invalid value %s for property %s\n", str, CRITICAL_BATT_ACTION_CFG);
	gtk_combo_box_set_active (GTK_COMBO_BOX(battery_critical), 0);
    }
    else
	gtk_combo_box_set_active (GTK_COMBO_BOX(battery_critical), val);
	
    g_free(str);
    
    GtkWidget *power_save = glade_xml_get_widget (xml, "power-save");
    gboolean save_power = xfconf_channel_get_bool (channel, POWER_SAVE_ON_BATTERY, TRUE);
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(power_save), save_power);
    g_signal_connect (power_save, "toggled",
		      G_CALLBACK(power_save_toggled_cb), channel);
	
    
    /*
     * DPMS settings when running on battery power
     */
#ifdef HAVE_DPMS
    GtkWidget *dpms_frame_on_battery = glade_xml_get_widget (xml, "dpms-on-battery-frame");
    gtk_widget_show (GTK_WIDGET(dpms_frame_on_battery));
    
    on_battery_dpms_sleep = glade_xml_get_widget (xml, "sleep-dpms-on-battery");
  
    val = xfconf_channel_get_uint (channel, ON_BATT_DPMS_SLEEP, 3);
    gtk_range_set_value (GTK_RANGE(on_battery_dpms_sleep), val);
    
    g_signal_connect (on_battery_dpms_sleep, "value-changed",
		      G_CALLBACK(sleep_on_battery_value_changed_cb), channel);
		      
    g_signal_connect (on_battery_dpms_sleep, "format-value", 
		      G_CALLBACK(format_dpms_value_cb), NULL);
		      
    on_battery_dpms_off = glade_xml_get_widget (xml, "off-dpms-on-battery");
    
    val = xfconf_channel_get_uint (channel, ON_BATT_DPMS_OFF, 5);
    gtk_range_set_value (GTK_RANGE(on_battery_dpms_off), val);
    
    g_signal_connect (on_battery_dpms_off, "value-changed",
		      G_CALLBACK(off_on_battery_value_changed_cb), channel);
    g_signal_connect (on_battery_dpms_off, "format-value", 
		      G_CALLBACK(format_dpms_value_cb), NULL);
#endif

     /*
     * Lid switch settings on battery
     */
    GtkWidget *lid = glade_xml_get_widget (xml, "on-battery-lid");
    
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    
    gtk_combo_box_set_model (GTK_COMBO_BOX(lid), GTK_TREE_MODEL(list_store));
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Nothing"), 1, 0, -1);
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, 1, -1);
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, 2, -1);
    
    g_signal_connect (lid, "changed", 
		      G_CALLBACK(on_battery_lid_changed_cb), channel);
		      
    str = xfconf_channel_get_string (channel, LID_SWITCH_ON_BATTERY_CFG, "Nothing");
    
    val = xfpm_shutdown_string_to_int (str);
    
    if ( val == -1 || val == 3 /*we don't do shutdown here */) 
    {
	g_warning ("Invalid value %s for property %s\n", str, LID_SWITCH_ON_BATTERY_CFG);
	gtk_combo_box_set_active (GTK_COMBO_BOX(lid), 0);
    }
    else
	gtk_combo_box_set_active (GTK_COMBO_BOX(lid), val);
	
    g_free (str);
    
}

static void
xfpm_settings_on_ac (XfconfChannel *channel)
{
    guint val;
#ifdef HAVE_DPMS
    /*
     * DPMS settings when running on AC power 
     */
    GtkWidget *dpms_frame_on_ac = glade_xml_get_widget (xml, "dpms-on-ac-frame");
    gtk_widget_show (GTK_WIDGET(dpms_frame_on_ac));
    
    on_ac_dpms_sleep = glade_xml_get_widget (xml, "sleep-dpms-on-ac");
  
    val = xfconf_channel_get_uint (channel, ON_AC_DPMS_SLEEP, 10);
    gtk_range_set_value (GTK_RANGE(on_ac_dpms_sleep), val);
    
    g_signal_connect (on_ac_dpms_sleep, "value-changed",
		      G_CALLBACK(sleep_on_ac_value_changed_cb), channel);
		      
    g_signal_connect (on_ac_dpms_sleep, "format-value", 
		      G_CALLBACK(format_dpms_value_cb), NULL);
		      
    on_ac_dpms_off = glade_xml_get_widget (xml, "off-dpms-on-ac");
    
    val = xfconf_channel_get_uint (channel, ON_AC_DPMS_OFF, 15);
    gtk_range_set_value (GTK_RANGE(on_ac_dpms_off), val);
    
    g_signal_connect (on_ac_dpms_off, "value-changed",
		      G_CALLBACK(off_on_ac_value_changed_cb), channel);
    g_signal_connect (on_ac_dpms_off, "format-value", 
		      G_CALLBACK(format_dpms_value_cb), NULL);
#endif

    /*
     * Lid switch settings on AC power
     */
    GtkListStore *list_store;
    GtkTreeIter iter;
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    
    GtkWidget *lid = glade_xml_get_widget (xml, "on-ac-lid");
    gtk_combo_box_set_model (GTK_COMBO_BOX(lid), GTK_TREE_MODEL(list_store));
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Nothing"), 1, 0, -1);
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, 1, -1);
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, 2, -1);
    
    g_signal_connect (lid, "changed", 
		      G_CALLBACK(on_ac_lid_changed_cb), channel);
		      
    gchar *str = xfconf_channel_get_string (channel, LID_SWITCH_ON_AC_CFG, "Nothing");
    
    val = xfpm_shutdown_string_to_int (str);
    
    if ( val == -1 || val == 3 /*we don't do shutdown here */) 
    {
	g_warning ("Invalid value %s for property %s\n", str, LID_SWITCH_ON_AC_CFG);
	gtk_combo_box_set_active (GTK_COMBO_BOX(lid), 0);
    }
    else
	gtk_combo_box_set_active (GTK_COMBO_BOX(lid), val);
	
    g_free (str);
}

static void
xfpm_settings_general (XfconfChannel *channel)
{
    /*
     *  Tray icon settings
     */
    GtkListStore *list_store;
    GtkTreeIter iter;
    list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    
    GtkWidget *tray = glade_xml_get_widget (xml, "tray-combox");
    gtk_combo_box_set_model (GTK_COMBO_BOX(tray), GTK_TREE_MODEL(list_store));

    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Always show icon"), 1, 0, -1);

    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("When battery is present"), 1, 1, -1);
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("When battery is charging or discharging"), 1, 2, -1);
    
    guint show_tray = xfconf_channel_get_uint (channel, SHOW_TRAY_ICON_CFG, 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX(tray), show_tray);
    g_signal_connect (tray, "changed",
		      G_CALLBACK(set_show_tray_icon_cb), channel);
    gboolean val;
#ifdef HAVE_DPMS
    /*
     * Global dpms settings (enable/disable)
     */
    GtkWidget *dpms = glade_xml_get_widget (xml, "enable-dpms");
   
    g_signal_connect (dpms, "toggled",
		      G_CALLBACK(dpms_toggled_cb), channel);
		      
    val = xfconf_channel_get_bool (channel, DPMS_ENABLED_CFG, TRUE);
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(dpms), val);
#endif

    /*
     * Sleep button 
     */
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    GtkWidget *sleep = glade_xml_get_widget (xml, "sleep-combox");
    
    gtk_combo_box_set_model (GTK_COMBO_BOX(sleep), GTK_TREE_MODEL(list_store));

    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Nothing"), 1, 0, -1);
    
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, 1, -1);
    
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, 2, -1);
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Shutdown"), 1, 3, -1);

    g_signal_connect (sleep, "changed",
		      G_CALLBACK(set_sleep_changed_cb), channel);
    
    gchar *default_sleep_value = xfconf_channel_get_string (channel, SLEEP_SWITCH_CFG, "Nothing");
    gint   sleep_val_int = xfpm_shutdown_string_to_int (default_sleep_value );
    if ( sleep_val_int == -1) 
    {
	g_warning ("Invalid value %s for property %s\n", default_sleep_value, SLEEP_SWITCH_CFG);
	gtk_combo_box_set_active (GTK_COMBO_BOX(sleep), 0);
    }
    else
	gtk_combo_box_set_active (GTK_COMBO_BOX(sleep), sleep_val_int);
    
    g_free (default_sleep_value);
    
    
    /*
     * Enable/Disable Notification
     */
    
    GtkWidget *notify = glade_xml_get_widget (xml, "notification");
    val = xfconf_channel_get_bool (channel, GENERAL_NOTIFICATION_CFG, TRUE);
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(notify), val);
    
    g_signal_connect (notify, "toggled",
		      G_CALLBACK(notify_toggled_cb), channel);
}

static void
xfpm_settings_advanced (XfconfChannel *channel )
{
#ifdef HAVE_DPMS
    sleep_dpms_mode = glade_xml_get_widget (xml, "sleep-dpms-mode");
    suspend_dpms_mode = glade_xml_get_widget (xml, "suspend-dpms-mode");
    g_signal_connect (sleep_dpms_mode, "toggled",
		      G_CALLBACK(set_dpms_sleep_mode), channel);
    g_signal_connect (suspend_dpms_mode, "toggled",
		      G_CALLBACK(set_dpms_suspend_mode), channel);
		      
    gchar *str = xfconf_channel_get_string (channel, DPMS_SLEEP_MODE, "sleep");
    if ( xfpm_strequal (str, "sleep" ) )
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(sleep_dpms_mode), TRUE);
    else if ( xfpm_strequal (str, "suspend") )
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(suspend_dpms_mode), TRUE);
    else 
    {
	g_critical ("Invalid value %s for property %s\n", str, DPMS_SLEEP_MODE );
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(sleep_dpms_mode), TRUE);
    }
    
    g_free (str);
    
    
#endif
}

static void
_cursor_changed_cb(GtkIconView *view,gpointer data)
{
    GtkTreeSelection *sel;
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gint int_data = 0;

    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW(view));

    gtk_tree_selection_get_selected (sel, &model, &selected_row);

    gtk_tree_model_get(model,
                       &selected_row,
                       2,
                       &int_data,
                       -1);

    if ( int_data >= 0 && int_data <= 3 )
    {
	GtkWidget *nt = glade_xml_get_widget (xml, "main-notebook");
        gtk_notebook_set_current_page(GTK_NOTEBOOK(nt), int_data);
    }
}

static void
xfpm_settings_tree_view (XfconfChannel *channel)
{
    GtkWidget *view = glade_xml_get_widget (xml, "treeview");
    GdkPixbuf *pix;
    GtkListStore *list_store;
    GtkTreeIter iter;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    gint i = 0;
    
    list_store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);

    gtk_tree_view_set_model (GTK_TREE_VIEW(view), GTK_TREE_MODEL(list_store));
    
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view),TRUE);
    col = gtk_tree_view_column_new();
    //gtk_tree_view_column_set_spacing(col, 10);

    renderer = gtk_cell_renderer_pixbuf_new();
    
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", 0, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "text", 1, NULL);
    
    gtk_tree_view_append_column(GTK_TREE_VIEW(view),col);
    
    /*General settings */
    pix = xfpm_load_icon("preferences-system", 48); 
    
    gtk_list_store_append(list_store, &iter);
    
    if ( pix )
    {
	    gtk_list_store_set (list_store, &iter, 0, pix, 1, _("General"), 2, i, -1);
	    g_object_unref(pix);
    }
    else
    {
	    gtk_list_store_set (list_store, &iter, 1, _("General"), 2, i, -1);
    }
    i++;
    
    /* ON ac power */
    pix = xfpm_load_icon("gpm-ac-adapter", 48); 
    gtk_list_store_append(list_store, &iter);
    if ( pix )
    {
	    gtk_list_store_set(list_store, &iter, 0, pix, 1, _("On AC"), 2, i, -1);
	    g_object_unref(pix);
    }
    else
    {
	    gtk_list_store_set(list_store, &iter, 1, _("On AC"), 2, i, -1);
    }
    i++;
    
    
    pix = xfpm_load_icon("battery", 48); 
    gtk_list_store_append(list_store, &iter);
    if ( pix )
    {
	    gtk_list_store_set(list_store, &iter, 0, pix, 1, _("On Battery"), 2, i, -1);
	    g_object_unref(pix);
    }
    else
    {
	    gtk_list_store_set(list_store, &iter, 1, _("On Battery"), 2, i, -1);
    }
    i++;
    
    pix = xfpm_load_icon("applications-other", 48); 
    gtk_list_store_append(list_store, &iter);
    if ( pix )
    {
	    gtk_list_store_set(list_store, &iter, 0, pix, 1, _("Advanced"), 2, i, -1);
	    g_object_unref(pix);
    }
    else
    {
	    gtk_list_store_set(list_store, &iter, 1, _("Advance"), 2, i, -1);
    }
    
    
    GtkTreeSelection *sel;
    GtkTreePath *path;

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

    path = gtk_tree_path_new_from_string("0");

    gtk_tree_selection_select_path(sel, path);
    gtk_tree_path_free(path);

    g_signal_connect(view,"cursor-changed",G_CALLBACK(_cursor_changed_cb),NULL);
}

static void dialog_response_cb (GtkDialog *dialog, gint response, gpointer data)
{
    XfconfChannel *channel = (XfconfChannel *)data;
    
    switch(response)
    {
	case GTK_RESPONSE_HELP:
	    xfpm_help();
	    break;
	default:
	    g_object_unref(G_OBJECT(channel));
	    break;
    }
}

GtkWidget *
xfpm_settings_dialog_new (XfconfChannel *channel)
{
    GtkWidget *dialog;
    
    xml = glade_xml_new_from_buffer (xfpm_settings_glade,
				     xfpm_settings_glade_length,
				     "xfpm-settings-dialog", NULL);
    
    dialog = glade_xml_get_widget (xml, "xfpm-settings-dialog");

    xfpm_settings_on_ac     (channel);
    xfpm_settings_on_battery (channel);
    xfpm_settings_tree_view (channel);
    xfpm_settings_general   (channel);
    xfpm_settings_advanced  (channel);

    g_signal_connect (dialog, "response", G_CALLBACK(dialog_response_cb), channel);
    
    return dialog;
}
