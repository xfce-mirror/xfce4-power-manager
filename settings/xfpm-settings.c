/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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
#include <upower.h>

#include <xfconf/xfconf.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfpm-common.h"
#include "xfpm-icons.h"
#include "xfpm-debug.h"
#include "xfpm-power-common.h"
#include "xfpm-power.h"

#include "interfaces/xfpm-settings_ui.h"

#include "xfpm-settings.h"
#include "xfpm-config.h"
#include "xfpm-enum-glib.h"
#include "xfpm-enum.h"

#define BRIGHTNESS_DISABLED 	9

static 	GtkBuilder *xml 			= NULL;
static  GtkWidget  *nt				= NULL;

static  GtkWidget *on_battery_dpms_sleep 	= NULL;
static  GtkWidget *on_battery_dpms_off  	= NULL;
static  GtkWidget *on_ac_dpms_sleep 		= NULL;
static  GtkWidget *on_ac_dpms_off 		= NULL;
static  GtkWidget *sideview                 = NULL; /* Sidebar tree view - all devices are in the sideview */
static  GtkWidget *device_details_notebook  = NULL; /* Displays the details of a deivce */

static  gboolean  lcd_brightness = FALSE;
static  gchar *starting_device_id = NULL;
static  UpClient *upower = NULL;

enum
{
    COL_SIDEBAR_ICON,
    COL_SIDEBAR_NAME,
    COL_SIDEBAR_INT,
    COL_SIDEBAR_BATTERY_DEVICE, /* Pointer to the UpDevice */
    COL_SIDEBAR_OBJECT_PATH,    /* UpDevice object path */
    COL_SIDEBAR_SIGNAL_ID,      /* device changed callback id */
    COL_SIDEBAR_VIEW,           /* Pointer to GtkTreeView of the devcie details */
    NCOLS_SIDEBAR
};

enum
{
    XFPM_DEVICE_INFO_NAME,
    XFPM_DEVICE_INFO_VALUE,
    XFPM_DEVICE_INFO_LAST
};

/*
 * GtkBuilder callbacks
 */
void	    brightness_level_on_ac		   (GtkWidget *w,
						    XfconfChannel *channel);

void 	    brightness_level_on_battery 	   (GtkWidget *w,  
						    XfconfChannel *channel);

void	    battery_critical_changed_cb 	   (GtkWidget *w, 
						    XfconfChannel *channel);

void        inactivity_on_ac_value_changed_cb      (GtkWidget *widget, 
						    XfconfChannel *channel);

void        inactivity_on_battery_value_changed_cb (GtkWidget *widget, 
						    XfconfChannel *channel);

void        button_sleep_changed_cb                (GtkWidget *w, 
						    XfconfChannel *channel);

void        button_power_changed_cb                 (GtkWidget *w, 
						    XfconfChannel *channel);

void        button_hibernate_changed_cb            (GtkWidget *w, 
						    XfconfChannel *channel);

void        notify_toggled_cb                      (GtkWidget *w, 
						    XfconfChannel *channel);

void        on_sleep_mode_changed_cb      (GtkWidget *w,
						    XfconfChannel *channel);

void        on_sleep_dpms_mode_changed_cb      (GtkWidget *w,
						    XfconfChannel *channel);

void        dpms_toggled_cb                        (GtkWidget *w, 
						    XfconfChannel *channel);

void        sleep_on_battery_value_changed_cb      (GtkWidget *w, 
						    XfconfChannel *channel);

void        off_on_battery_value_changed_cb        (GtkWidget *w, 
						    XfconfChannel *channel);

void        sleep_on_ac_value_changed_cb           (GtkWidget *w, 
						    XfconfChannel *channel);

void        off_on_ac_value_changed_cb             (GtkWidget *w, 
						    XfconfChannel *channel);

gchar      *format_dpms_value_cb                   (GtkScale *scale, 
						    gdouble value,
						    gpointer data);

gchar      *format_inactivity_value_cb             (GtkScale *scale, 
						    gdouble value,
						    gpointer data);

gchar      *format_brightness_value_cb             (GtkScale *scale, 
						    gdouble value,
						    gpointer data);

gchar      *format_brightness_percentage_cb        (GtkScale *scale, 
						    gdouble value,
						    gpointer data);

void        brightness_on_battery_value_changed_cb (GtkWidget *w, 
						    XfconfChannel *channel);

void        brightness_on_ac_value_changed_cb      (GtkWidget *w, 
						    XfconfChannel *channel);

gboolean    critical_spin_output_cb                (GtkSpinButton *w, 
						    gpointer data);

void        on_battery_lid_changed_cb              (GtkWidget *w, 
						    XfconfChannel *channel);

void        on_ac_lid_changed_cb                   (GtkWidget *w, 
						    XfconfChannel *channel);

void        critical_level_value_changed_cb        (GtkSpinButton *w, 
						    XfconfChannel *channel);

void        lock_screen_toggled_cb                 (GtkWidget *w, 
						    XfconfChannel *channel);

static void view_cursor_changed_cb 		   (GtkTreeView *view,
						    gpointer *user_data);



void brightness_level_on_ac (GtkWidget *w,  XfconfChannel *channel)
{
    guint val = (guint) gtk_range_get_value (GTK_RANGE (w));
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_AC, val) )
    {
	g_critical ("Unable to set value %u for property %s\n", val, BRIGHTNESS_LEVEL_ON_AC);
    }
}

void brightness_level_on_battery (GtkWidget *w,  XfconfChannel *channel)
{
     guint val = (guint) gtk_range_get_value (GTK_RANGE (w));
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_BATTERY, val) )
    {
	g_critical ("Unable to set value %u for property %s\n", val, BRIGHTNESS_LEVEL_ON_BATTERY);
    }
}

void
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
		       
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX CRITICAL_BATT_ACTION_CFG, value) )
    {
	g_critical ("Cannot set value for property %s\n", CRITICAL_BATT_ACTION_CFG);
    }
}

void
inactivity_on_ac_value_changed_cb (GtkWidget *widget, XfconfChannel *channel)
{
    gint value    = (gint)gtk_range_get_value (GTK_RANGE (widget));
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX ON_AC_INACTIVITY_TIMEOUT, value))
    {
	g_critical ("Cannot set value for property %s\n", ON_AC_INACTIVITY_TIMEOUT);
    }
}

void
inactivity_on_battery_value_changed_cb (GtkWidget *widget, XfconfChannel *channel)
{
    gint value    = (gint)gtk_range_get_value (GTK_RANGE (widget));
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX ON_BATTERY_INACTIVITY_TIMEOUT, value))
    {
	g_critical ("Cannot set value for property %s\n", ON_BATTERY_INACTIVITY_TIMEOUT);
    }
}

void
button_sleep_changed_cb (GtkWidget *w, XfconfChannel *channel)
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
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX SLEEP_SWITCH_CFG, value ) )
    {
	g_critical ("Cannot set value for property %s\n", SLEEP_SWITCH_CFG);
    }
}

void
button_power_changed_cb (GtkWidget *w, XfconfChannel *channel)
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
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX POWER_SWITCH_CFG, value) )
    {
	g_critical ("Cannot set value for property %s\n", POWER_SWITCH_CFG);
    }
}

void
button_hibernate_changed_cb (GtkWidget *w, XfconfChannel *channel)
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
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX HIBERNATE_SWITCH_CFG, value ) )
    {
	g_critical ("Cannot set value for property %s\n", HIBERNATE_SWITCH_CFG);
    }
}

void
notify_toggled_cb (GtkWidget *w, XfconfChannel *channel)
{
    gboolean val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(w));
    
    if (!xfconf_channel_set_bool (channel, PROPERTIES_PREFIX GENERAL_NOTIFICATION_CFG, val) )
    {
	g_critical ("Cannot set value for property %s\n", GENERAL_NOTIFICATION_CFG);
    }
}

void
on_sleep_mode_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gchar *value = "";
    
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (w), &selected_row))
	return;

    model = gtk_combo_box_get_model (GTK_COMBO_BOX(w));

    gtk_tree_model_get(model,
                       &selected_row,
                       0,
                       &value,
                       -1);
    
    if (!xfconf_channel_set_string (channel, PROPERTIES_PREFIX INACTIVITY_SLEEP_MODE, value) )
    {
	g_critical ("Cannot set value for property %s\n", INACTIVITY_SLEEP_MODE);
    }
}

void
on_sleep_dpms_mode_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gchar *value = "";
    
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (w), &selected_row))
	return;

    model = gtk_combo_box_get_model (GTK_COMBO_BOX(w));

    gtk_tree_model_get(model,
                       &selected_row,
                       0,
                       &value,
                       -1);
    
    if (!xfconf_channel_set_string (channel, PROPERTIES_PREFIX DPMS_SLEEP_MODE, value) )
    {
	g_critical ("Cannot set value for property %s\n", DPMS_SLEEP_MODE);
    }
}

void
dpms_toggled_cb (GtkWidget *w, XfconfChannel *channel)
{
#ifdef HAVE_DPMS
    gboolean val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(w));
    
    xfconf_channel_set_bool (channel, PROPERTIES_PREFIX DPMS_ENABLED_CFG, val);
    
    gtk_widget_set_sensitive (on_ac_dpms_off, val);
    gtk_widget_set_sensitive (on_ac_dpms_sleep, val);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (xml, "sleep-dpms-mode")), val);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (xml, "dpms-mode-label")), val);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (xml, "sleep-display-label")), val);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (xml, "switch-off-display-label")), val);
    
    if ( GTK_IS_WIDGET (on_battery_dpms_off ) )
    {
	gtk_widget_set_sensitive (on_battery_dpms_off, val);
    	gtk_widget_set_sensitive (on_battery_dpms_sleep, val);
    }
#endif
}

void
sleep_on_battery_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
#ifdef HAVE_DPMS
    GtkWidget *brg;
    gint off_value    = (gint)gtk_range_get_value (GTK_RANGE (on_battery_dpms_off));
    gint sleep_value  = (gint)gtk_range_get_value (GTK_RANGE (w));
    gint brightness_value;
    
    if ( off_value != 0 )
    {
	if ( sleep_value >= off_value )
	{
	    gtk_range_set_value (GTK_RANGE(on_battery_dpms_off), sleep_value + 1 );
	}
    }
    
    if ( lcd_brightness )
    {
	brg = GTK_WIDGET (gtk_builder_get_object (xml, "brg-on-battery"));
	brightness_value = (gint) gtk_range_get_value (GTK_RANGE (brg));
	
	if ( sleep_value * 60 <= brightness_value && brightness_value != BRIGHTNESS_DISABLED)
	{
	    gtk_range_set_value (GTK_RANGE (brg), BRIGHTNESS_DISABLED);
	}
    }
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX ON_BATT_DPMS_SLEEP, sleep_value))
    {
	g_critical ("Cannot set value for property %s\n", ON_BATT_DPMS_SLEEP);
    }
#endif
}

void
off_on_battery_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
#ifdef HAVE_DPMS
    gint off_value    = (gint)gtk_range_get_value (GTK_RANGE(w));
    gint sleep_value  = (gint)gtk_range_get_value (GTK_RANGE(on_battery_dpms_sleep));
    
    if ( sleep_value != 0 )
    {
	if ( off_value <= sleep_value )
	{
	    gtk_range_set_value (GTK_RANGE(on_battery_dpms_sleep), off_value -1 );
	}
    }
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX ON_BATT_DPMS_OFF, off_value))
    {
	g_critical ("Cannot set value for property %s\n", ON_BATT_DPMS_OFF);
    }
#endif
}

void
sleep_on_ac_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
#ifdef HAVE_DPMS
    GtkWidget *brg;

    gint brightness_value;
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

    if ( lcd_brightness )
    {
	brg = GTK_WIDGET (gtk_builder_get_object (xml, "brg-on-ac"));
    
	brightness_value = (gint) gtk_range_get_value (GTK_RANGE (brg));
	
	if ( sleep_value * 60 <= brightness_value && brightness_value != BRIGHTNESS_DISABLED)
	{
	    gtk_range_set_value (GTK_RANGE (brg), BRIGHTNESS_DISABLED);
	}
    }

    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX ON_AC_DPMS_SLEEP, sleep_value))
    {
	g_critical ("Cannot set value for property %s\n", ON_AC_DPMS_SLEEP);
    }
#endif
}

void
off_on_ac_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
#ifdef HAVE_DPMS
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

    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX ON_AC_DPMS_OFF, off_value))
    {
	g_critical ("Cannot set value for property %s\n", ON_AC_DPMS_OFF);
    }
#endif
}

/*
 * Format value of GtkRange used with DPMS
 */
gchar *
format_dpms_value_cb (GtkScale *scale, gdouble value, gpointer data)
{
    if ( (gint)value == 0 )
    	return g_strdup (_("Never"));
    
    if ( (int)value == 1 )
	return g_strdup (_("One minute"));

    return g_strdup_printf ("%d %s", (int)value, _("Minutes"));
}


gchar *
format_inactivity_value_cb (GtkScale *scale, gdouble value, gpointer data)
{
    gint h, min;
    
    if ( (gint)value <= 14 )
	return g_strdup (_("Never"));
    else if ( (gint)value < 60 )
	return g_strdup_printf ("%d %s", (gint)value, _("Minutes"));
    else if ( (gint)value == 60)
	return g_strdup (_("One hour"));

    /* value > 60 */
    h = (gint)value/60;
    min = (gint)value%60;
    
    if ( h <= 1 )
	if ( min == 0 )      return g_strdup_printf ("%s", _("One hour"));
	else if ( min == 1 ) return g_strdup_printf ("%s %s", _("One hour"),  _("one minute"));
	else                 return g_strdup_printf ("%s %d %s", _("One hour"), min, _("minutes"));
    else 
	if ( min == 0 )      return g_strdup_printf ("%d %s", h, _("hours"));
	else if ( min == 1 ) return g_strdup_printf ("%d %s %s", h, _("hours"), _("one minute"));
	else                 return g_strdup_printf ("%d %s %d %s", h, _("hours"), min, _("minutes"));
}

/*
 * Format value of GtkRange used with Brightness
 */
gchar *
format_brightness_value_cb (GtkScale *scale, gdouble value, gpointer data)
{
    if ( (gint)value <= 9 )
    	return g_strdup (_("Never"));
        
    return g_strdup_printf ("%d %s", (int)value, _("Seconds"));
}

gchar *
format_brightness_percentage_cb (GtkScale *scale, gdouble value, gpointer data)
{
    return g_strdup_printf ("%d %s", (int)value, _("%"));
}

void
brightness_on_battery_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    gint value    = (gint)gtk_range_get_value (GTK_RANGE (w));
#ifdef HAVE_DPMS
    gint dpms_sleep = (gint) gtk_range_get_value (GTK_RANGE (on_battery_dpms_sleep) );

    if ( value != BRIGHTNESS_DISABLED )
    {
	if ( dpms_sleep != 0 && dpms_sleep * 60 <= value)
	{
	    gtk_range_set_value (GTK_RANGE (on_battery_dpms_sleep), (value / 60) + 1);
	}
    }
#endif
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_ON_BATTERY, value))
    {
	g_critical ("Cannot set value for property %s\n", BRIGHTNESS_ON_BATTERY);
    }
}

void
brightness_on_ac_value_changed_cb (GtkWidget *w, XfconfChannel *channel)
{
    gint value    = (gint)gtk_range_get_value (GTK_RANGE (w));
#ifdef HAVE_DPMS
    gint dpms_sleep = (gint) gtk_range_get_value (GTK_RANGE (on_ac_dpms_sleep) );

    if ( value != BRIGHTNESS_DISABLED )
    {
	if ( dpms_sleep != 0 && dpms_sleep * 60 <= value)
	{
	    gtk_range_set_value (GTK_RANGE (on_ac_dpms_sleep), (value / 60) + 1);
	}
    }
#endif
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_ON_AC, value))
    {
	g_critical ("Cannot set value for property %s\n", BRIGHTNESS_ON_AC);
    }
}

gboolean
critical_spin_output_cb (GtkSpinButton *w, gpointer data)
{
    gint val = (gint) gtk_spin_button_get_value (w);
    gchar *text = g_strdup_printf ("%d %%", val);
    
    gtk_entry_set_text (GTK_ENTRY(w), text);
    g_free (text);
    
    return TRUE;
}

void
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
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX LID_SWITCH_ON_BATTERY_CFG, value) )
    {
	g_critical ("Cannot set value for property %s\n", LID_SWITCH_ON_BATTERY_CFG);
    }
}

void
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
		       
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX LID_SWITCH_ON_AC_CFG, value) )
    {
	g_critical ("Cannot set value for property %s\n", LID_SWITCH_ON_AC_CFG);
    }
} 

void
critical_level_value_changed_cb (GtkSpinButton *w, XfconfChannel *channel)
{
    guint val = (guint) gtk_spin_button_get_value (w);
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX CRITICAL_POWER_LEVEL, val) )
    {
	g_critical ("Unable to set value %d for property %s\n", val, CRITICAL_POWER_LEVEL);
    }
}

void
lock_screen_toggled_cb (GtkWidget *w, XfconfChannel *channel)
{
    XfconfChannel *session_channel = xfconf_channel_get ("xfce4-session");
    gboolean val = (gint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(w));

    if ( !xfconf_channel_set_bool (session_channel, "/shutdown/LockScreen", val) )
    {
	g_critical ("Unable to set value for property %s\n", LOCK_SCREEN_ON_SLEEP);
    }

    if ( !xfconf_channel_set_bool (channel, PROPERTIES_PREFIX LOCK_SCREEN_ON_SLEEP, val) )
    {
	g_critical ("Unable to set value for property %s\n", LOCK_SCREEN_ON_SLEEP);
    }
}

static void
xfpm_settings_on_battery (XfconfChannel *channel, gboolean auth_suspend,
                          gboolean auth_hibernate, gboolean can_suspend,
                          gboolean can_hibernate, gboolean can_shutdown,
                          gboolean has_lcd_brightness, gboolean has_lid)
{
    gboolean valid;
    gint list_value;
    gint val;
    GtkListStore *list_store;
    GtkTreeIter iter;
    GtkWidget *inact;
    GtkWidget *battery_critical;
    GtkWidget *lid;
    GtkWidget *label;
    GtkWidget *brg;
    GtkWidget *brg_level;

    battery_critical = GTK_WIDGET (gtk_builder_get_object (xml, "battery-critical-combox"));
    
    inact = GTK_WIDGET (gtk_builder_get_object (xml, "inactivity-on-battery"));
    
    if ( !can_suspend && !can_hibernate )
    {
	gtk_widget_set_sensitive (inact, FALSE);
	gtk_widget_set_tooltip_text (inact, _("Hibernate and suspend operations not supported"));
    }
    else if ( !auth_suspend && !auth_hibernate )
    {
	gtk_widget_set_sensitive (inact, FALSE);
	gtk_widget_set_tooltip_text (inact, _("Hibernate and suspend operations not permitted"));
    }
    
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX ON_BATTERY_INACTIVITY_TIMEOUT, 14);
    gtk_range_set_value (GTK_RANGE (inact), val);
    
    
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    
    gtk_combo_box_set_model (GTK_COMBO_BOX(battery_critical), GTK_TREE_MODEL(list_store));
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Do nothing"), 1, XFPM_DO_NOTHING, -1);
    
    if ( can_suspend && auth_suspend )
    {
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, XFPM_DO_SUSPEND, -1);
    }
    
    if ( can_hibernate && auth_hibernate )
    {
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, XFPM_DO_HIBERNATE, -1);
    }

    if ( can_shutdown )
    {
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Shutdown"), 1, XFPM_DO_SHUTDOWN, -1);
    }
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Ask"), 1, XFPM_ASK, -1);
    
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX CRITICAL_BATT_ACTION_CFG, XFPM_DO_NOTHING);
    
    for ( valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
	  valid;
	  valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter) )
    {
	gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
			    1, &list_value, -1);
	if ( val == list_value )
	{
	    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (battery_critical), &iter);
	    break;
	}
    }
    
    /*
     * DPMS settings when running on battery power
     */
#ifdef HAVE_DPMS
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX ON_BATT_DPMS_SLEEP, 5);
    gtk_range_set_value (GTK_RANGE(on_battery_dpms_sleep), val);
    
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX ON_BATT_DPMS_OFF, 10);
    gtk_range_set_value (GTK_RANGE(on_battery_dpms_off), val);
#else
    gtk_widget_set_sensitive (GTK_WIDGET(on_battery_dpms_sleep), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(on_battery_dpms_off), FALSE);
#endif

    /*
     * Lid switch settings on battery
     */
    lid = GTK_WIDGET (gtk_builder_get_object (xml, "on-battery-lid"));
    if ( has_lid )
    {
	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	
	gtk_combo_box_set_model (GTK_COMBO_BOX(lid), GTK_TREE_MODEL(list_store));
	
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Switch off display"), 1, LID_TRIGGER_NOTHING, -1);
	
	if ( can_suspend && auth_suspend )
	{
	    gtk_list_store_append(list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, LID_TRIGGER_SUSPEND, -1);
	}
	
	if ( can_hibernate && auth_hibernate)
	{
	    gtk_list_store_append(list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, LID_TRIGGER_HIBERNATE, -1);
	}
	
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Lock screen"), 1, LID_TRIGGER_LOCK_SCREEN, -1);
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (lid), 0);
	
	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX LID_SWITCH_ON_BATTERY_CFG, LID_TRIGGER_LOCK_SCREEN);
	
	for ( valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
	      valid;
	      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter) )
	{
	    gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
				1, &list_value, -1);
	    if ( val == list_value )
	    {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (lid), &iter);
		break;
	    }
	} 
    }
    else
    {
	label = GTK_WIDGET (gtk_builder_get_object (xml, "on-battery-lid-label"));
	gtk_widget_hide (label);
	gtk_widget_hide (lid);
    }

    /*
     * Brightness on battery
     */
    if ( has_lcd_brightness )
    {
    brg = GTK_WIDGET (gtk_builder_get_object (xml ,"brg-on-battery"));
    brg_level = GTK_WIDGET (gtk_builder_get_object (xml ,"brg-level-on-battery"));
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_ON_BATTERY, 120);
    gtk_range_set_value (GTK_RANGE(brg), val);

    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_BATTERY, 20);
    gtk_range_set_value (GTK_RANGE (brg_level), val);

    }
    else
    {
    gtk_notebook_remove_page (GTK_NOTEBOOK (nt), 3);
    }

}

static void
xfpm_settings_on_ac (XfconfChannel *channel, gboolean auth_suspend,
                     gboolean auth_hibernate, gboolean can_suspend,
                     gboolean can_hibernate, gboolean has_lcd_brightness,
                     gboolean has_lid)
{
    GtkWidget *inact;
    GtkWidget *lid;
    GtkWidget *brg;
    GtkWidget *brg_level;
    GtkListStore *list_store;
    GtkTreeIter iter;
    guint val;
    gboolean valid;
    guint list_value;
    
    inact = GTK_WIDGET (gtk_builder_get_object (xml, "inactivity-on-ac"));
    
    if ( !can_suspend && !can_hibernate )
    {
	gtk_widget_set_sensitive (inact, FALSE);
	gtk_widget_set_tooltip_text (inact, _("Hibernate and suspend operations not supported"));
    }
    else  if ( !auth_suspend && !auth_hibernate )
    {
	gtk_widget_set_sensitive (inact, FALSE);
	gtk_widget_set_tooltip_text (inact, _("Hibernate and suspend operations not permitted"));
    }
    
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX ON_AC_INACTIVITY_TIMEOUT, 14);
    gtk_range_set_value (GTK_RANGE (inact), val);
   
#ifdef HAVE_DPMS
    /*
     * DPMS settings when running on AC power 
     */
    
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX ON_AC_DPMS_SLEEP, 10);
    gtk_range_set_value (GTK_RANGE (on_ac_dpms_sleep), val);
    
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX ON_AC_DPMS_OFF, 15);
    gtk_range_set_value (GTK_RANGE(on_ac_dpms_off), val);
#else
    gtk_widget_set_sensitive (GTK_WIDGET(on_ac_dpms_sleep),FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(on_ac_dpms_off),FALSE);
#endif
    /*
     * Lid switch settings on AC power
     */
    lid = GTK_WIDGET (gtk_builder_get_object (xml, "on-ac-lid"));
    if ( has_lid )
    {
	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	
	gtk_combo_box_set_model (GTK_COMBO_BOX(lid), GTK_TREE_MODEL(list_store));
	
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Switch off display"), 1, LID_TRIGGER_NOTHING, -1);
	
	if ( can_suspend && auth_suspend )
	{
	    gtk_list_store_append(list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, LID_TRIGGER_SUSPEND, -1);
	}
	
	if ( can_hibernate && auth_hibernate )
	{
	    gtk_list_store_append(list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, LID_TRIGGER_HIBERNATE, -1);
	}
	
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Lock screen"), 1, LID_TRIGGER_LOCK_SCREEN, -1);
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (lid), 0);
	
	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX LID_SWITCH_ON_AC_CFG, LID_TRIGGER_LOCK_SCREEN);
	for ( valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
	      valid;
	      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter) )
	{
	    gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
				1, &list_value, -1);
	    if ( val == list_value )
	    {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (lid), &iter);
		break;
	    }
	} 
    }
    else
    {
	GtkWidget *label;
	label = GTK_WIDGET (gtk_builder_get_object (xml, "on-ac-lid-label"));
	gtk_widget_hide (label);
	gtk_widget_hide (lid);
    }
    
	/*
	 * Brightness on AC power
	 */
	if ( has_lcd_brightness )
	{
	brg = GTK_WIDGET (gtk_builder_get_object (xml ,"brg-on-ac"));
	brg_level = GTK_WIDGET (gtk_builder_get_object (xml ,"brg-level-on-ac"));
	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_ON_AC, 9);
	gtk_range_set_value (GTK_RANGE(brg), val);

	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_AC, 80);
	gtk_range_set_value (GTK_RANGE (brg_level), val);

	}
	else
	{
	gtk_notebook_remove_page (GTK_NOTEBOOK (nt), 3);
	}

}

static void
xfpm_settings_general (XfconfChannel *channel, gboolean auth_suspend,
                       gboolean auth_hibernate, gboolean can_suspend,
                       gboolean can_hibernate, gboolean can_shutdown,
                       gboolean has_sleep_button, gboolean has_hibernate_button,
                       gboolean has_power_button)
{
    GtkWidget *power;
    GtkWidget *power_label;
    GtkWidget *hibernate;
    GtkWidget *hibernate_label;
    GtkWidget *sleep_w;
    GtkWidget *sleep_label;
    GtkWidget *notify;
    
    guint  value;
    guint list_value;
    gboolean valid;
    gboolean val;
    
    GtkWidget *dpms;
    GtkListStore *list_store;
    GtkTreeIter iter;

    dpms = GTK_WIDGET (gtk_builder_get_object (xml, "enable-dpms"));
#ifdef HAVE_DPMS
    /*
     * Global dpms settings (enable/disable)
     */
   
    val = xfconf_channel_get_bool (channel, PROPERTIES_PREFIX DPMS_ENABLED_CFG, TRUE);
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(dpms), val);
#else
    gtk_widget_hide (dpms);
#endif

    /*
     * Power button
     */
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    power = GTK_WIDGET (gtk_builder_get_object (xml, "power-combox"));
    power_label = GTK_WIDGET (gtk_builder_get_object (xml, "power-label"));
    
    if ( has_power_button )
    {
	gtk_combo_box_set_model (GTK_COMBO_BOX(power), GTK_TREE_MODEL(list_store));

	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Do nothing"), 1, XFPM_DO_NOTHING, -1);
	
	if ( can_suspend && auth_suspend)
	{
	    gtk_list_store_append (list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, XFPM_DO_SUSPEND, -1);
	}
	
	if ( can_hibernate && auth_hibernate )
	{
	    gtk_list_store_append (list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, XFPM_DO_HIBERNATE, -1);
	}
	
	if ( can_shutdown )
	{
	    gtk_list_store_append (list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Shutdown"), 1, XFPM_DO_SHUTDOWN, -1);
	}
	
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Ask"), 1, XFPM_ASK, -1);
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (power), 0);
	
	value = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX POWER_SWITCH_CFG, XFPM_DO_NOTHING);
	for ( valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
	      valid;
	      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter) )
	{
	    gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
				1, &list_value, -1);
	    if ( value == list_value )
	    {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (power), &iter);
		break;
	    }
	} 
    }
    else
    {
	gtk_widget_hide (power);
	gtk_widget_hide (power_label);
    }
    
    /*
     * Hibernate button
     */
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    hibernate = GTK_WIDGET (gtk_builder_get_object (xml, "hibernate-combox"));
    hibernate_label = GTK_WIDGET (gtk_builder_get_object (xml, "hibernate-label"));
    
    if (has_hibernate_button )
    {
	gtk_combo_box_set_model (GTK_COMBO_BOX(hibernate), GTK_TREE_MODEL(list_store));

	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Do nothing"), 1, XFPM_DO_NOTHING, -1);
	
	if ( can_suspend && auth_suspend)
	{
	    gtk_list_store_append (list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, XFPM_DO_SUSPEND, -1);
	}
	
	if ( can_hibernate && auth_hibernate )
	{
	    gtk_list_store_append (list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, XFPM_DO_HIBERNATE, -1);
	}
	
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Ask"), 1, XFPM_ASK, -1);
	
	value = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX HIBERNATE_SWITCH_CFG, XFPM_DO_NOTHING);
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (hibernate), 0);
	
	for ( valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
	      valid;
	      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter) )
	{
	    gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
				1, &list_value, -1);
	    if ( value == list_value )
	    {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (hibernate), &iter);
		break;
	    }
	} 
    }
    else
    {
	gtk_widget_hide (hibernate);
	gtk_widget_hide (hibernate_label);
    }

    /*
     * Sleep button 
     */
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    sleep_w = GTK_WIDGET (gtk_builder_get_object (xml, "sleep-combox"));
    sleep_label = GTK_WIDGET (gtk_builder_get_object (xml, "sleep-label"));
    
    if ( has_sleep_button )
    {
	gtk_combo_box_set_model (GTK_COMBO_BOX(sleep_w), GTK_TREE_MODEL(list_store));

	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Do nothing"), 1, XFPM_DO_NOTHING, -1);
	
	if ( can_suspend && auth_suspend )
	{
	    gtk_list_store_append (list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, XFPM_DO_SUSPEND, -1);
	}
	
	if ( can_hibernate && auth_hibernate)
	{
	    gtk_list_store_append (list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), 1, XFPM_DO_HIBERNATE, -1);
	}
	
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Ask"), 1, XFPM_ASK, -1);
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (sleep_w), 0);
	
	value = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX SLEEP_SWITCH_CFG, XFPM_DO_NOTHING);
	for ( valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
	      valid;
	      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter) )
	{
	    gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
				1, &list_value, -1);
	    if ( value == list_value )
	    {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (sleep_w), &iter);
		break;
	    }
	} 
    }
    else
    {
	gtk_widget_hide (sleep_w);
	gtk_widget_hide (sleep_label);
    }
    /*
     * Enable/Disable Notification
     */
    
    notify = GTK_WIDGET (gtk_builder_get_object (xml, "notification"));
    val = xfconf_channel_get_bool (channel, PROPERTIES_PREFIX GENERAL_NOTIFICATION_CFG, TRUE);
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(notify), val);
}

static void
xfpm_settings_advanced (XfconfChannel *channel, gboolean auth_suspend,
                        gboolean auth_hibernate, gboolean can_suspend,
                        gboolean can_hibernate, gboolean has_battery)
{
    guint val;
    gchar *str;
    GtkWidget *critical_level;
    GtkWidget *lock;
    GtkWidget *label;
    GtkWidget *sleep_dpms_mode;
    GtkWidget *sleep_mode;

    gchar *list_str;
    gboolean valid;
    GtkListStore *list_store;
    GtkTreeIter iter;

    /*
     * Computer sleep mode
     */
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    sleep_mode = GTK_WIDGET (gtk_builder_get_object (xml, "system-sleep-mode"));
    gtk_combo_box_set_model (GTK_COMBO_BOX(sleep_mode), GTK_TREE_MODEL(list_store));
    
    if ( can_suspend )
    {
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Suspend"), -1);
    }
    else if ( !auth_suspend )
    {
	gtk_widget_set_tooltip_text (sleep_mode, _("Suspend operation not permitted"));
    }
    else
    {
	gtk_widget_set_tooltip_text (sleep_mode, _("Suspend operation not supported"));
    }
    
    if ( can_hibernate )
    {
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Hibernate"), -1);
    }
    else if ( !auth_hibernate )
    {
	gtk_widget_set_tooltip_text (sleep_mode, _("Hibernate operation not permitted"));
    }
    else
    {
	gtk_widget_set_tooltip_text (sleep_mode, _("Hibernate operation not supported"));
    }

    gtk_combo_box_set_active (GTK_COMBO_BOX (sleep_mode), 0);

    str = xfconf_channel_get_string (channel, PROPERTIES_PREFIX INACTIVITY_SLEEP_MODE, "Suspend");
    for ( valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
	  valid;
	  valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter) )
    {
	gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
			    0, &list_str, -1);
	if ( g_strcmp0 (str, list_str) )
	{
	    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (sleep_mode), &iter);
	    break;
	}
    }
    g_free (str);

    /*
     * Display sleep mode
     */
    sleep_dpms_mode = GTK_WIDGET (gtk_builder_get_object (xml, "sleep-dpms-mode"));

#ifdef HAVE_DPMS
    list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    gtk_combo_box_set_model (GTK_COMBO_BOX(sleep_dpms_mode), GTK_TREE_MODEL(list_store));
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), -1);
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 0, _("Standby"), -1);

    gtk_combo_box_set_active (GTK_COMBO_BOX (sleep_dpms_mode), 0);

    str = xfconf_channel_get_string (channel, PROPERTIES_PREFIX DPMS_SLEEP_MODE, "Standby");

    for ( valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
	  valid;
	  valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter) )
    {
	gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter,
			    0, &list_str, -1);
	if ( g_strcmp0 (str, list_str) )
	{
	    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (sleep_dpms_mode), &iter);
	    break;
	}
    }

    g_free (str);
    
#else
    gtk_widget_hide (sleep_dpms_mode);
    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml, "dpms-mode-label")));
#endif

    /*
     * Critical battery level
     */
    critical_level = GTK_WIDGET (gtk_builder_get_object (xml, "critical-spin"));
    if ( has_battery )
    {
	gtk_widget_set_tooltip_text (critical_level, 
				     _("When all the power sources of the computer reach this charge level"));
    
	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX CRITICAL_POWER_LEVEL, 10);

	if ( val > 20 || val < 1)
	{
	    g_critical ("Value %d if out of range for property %s\n", val, CRITICAL_POWER_LEVEL);
	    gtk_spin_button_set_value (GTK_SPIN_BUTTON(critical_level), 10);
	}
	else
	    gtk_spin_button_set_value (GTK_SPIN_BUTTON(critical_level), val);
    }
    else
    {
	label = GTK_WIDGET (gtk_builder_get_object (xml, "critical-level-label" ));
	gtk_widget_hide (critical_level);
	gtk_widget_hide (label);
    }
	
    /*
     * Lock screen for suspend/hibernate
     */
    lock = GTK_WIDGET (gtk_builder_get_object (xml, "lock-screen"));
    
    if ( !can_suspend && !can_hibernate )
    {
	gtk_widget_set_sensitive (lock, FALSE);
	gtk_widget_set_tooltip_text (lock, _("Hibernate and suspend operations not supported"));
    }
    else if ( !auth_hibernate && !auth_suspend)
    {
	gtk_widget_set_sensitive (lock, FALSE);
	gtk_widget_set_tooltip_text (lock, _("Hibernate and suspend operations not permitted"));
    }
    
    val = xfconf_channel_get_bool (channel, PROPERTIES_PREFIX LOCK_SCREEN_ON_SLEEP, TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(lock), val);
}

/* Call gtk_tree_iter_free when done with the tree iter */
static GtkTreeIter*
find_device_in_tree (const gchar *object_path)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    if ( !sideview )
        return NULL;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(sideview));

    if (!model)
        return NULL;

    if(gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gchar *path = NULL;
            gtk_tree_model_get(model, &iter, COL_SIDEBAR_OBJECT_PATH, &path, -1);

            if(g_strcmp0(path, object_path) == 0) {
                g_free(path);
                return gtk_tree_iter_copy(&iter);
            }

            g_free(path);
        } while(gtk_tree_model_iter_next(model, &iter));
    }

    return NULL;
}

/* Call gtk_tree_iter_free when done with the tree iter */
static GtkTreeIter*
find_device_info_name_in_tree (GtkTreeView *view, const gchar *device_info_name)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    if ( !view )
        return NULL;

    model = gtk_tree_view_get_model(view);

    if (!model)
        return NULL;

    if(gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gchar *name = NULL;
            gtk_tree_model_get(model, &iter, XFPM_DEVICE_INFO_NAME, &name, -1);

            if(g_strcmp0(name, device_info_name) == 0) {
                g_free(name);
                return gtk_tree_iter_copy(&iter);
            }

            g_free(name);
        } while(gtk_tree_model_iter_next(model, &iter));
    }

    return NULL;
}

static gchar *
xfpm_info_get_energy_property (gdouble energy, const gchar *unit)
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

    g_return_if_fail (GTK_IS_TREE_VIEW(view));
    g_return_if_fail (GTK_IS_LIST_STORE(list_store));
    g_return_if_fail (name != NULL);
    /* Value can be NULL */

    DBG ("updating  name %s with value %s", name, value);

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
update_sideview_icon (UpDevice *device)
{
    GtkListStore *list_store;
    GtkTreeIter *iter;
    GdkPixbuf *pix;
    guint type = 0;
    gchar *name = NULL, *icon_name = NULL, *model = NULL, *vendor = NULL;
    const gchar *object_path = up_device_get_object_path(device);

    list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (sideview)));

    TRACE("entering for %s", object_path);

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


    name = get_device_description (upower, device);
    icon_name = get_device_icon_name (upower, device);

    pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                    icon_name,
                                    48,
                                    GTK_ICON_LOOKUP_USE_BUILTIN,
                                    NULL);

    gtk_list_store_set (list_store, iter,
                        COL_SIDEBAR_ICON, pix,
                        COL_SIDEBAR_NAME, name,
                        -1);

    if ( pix )
        g_object_unref (pix);

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
    gdouble energy_full_design = -1.0, energy_full = -1.0, energy_empty = -1.0, voltage = -1.0, percent = -1.0;
    gboolean p_supply = FALSE;
    gchar *model = NULL, *vendor = NULL, *serial = NULL;
    const gchar *battery_type = NULL;
    const gchar *object_path = up_device_get_object_path(device);

    TRACE("entering for %s", object_path);

    sideview_iter = find_device_in_tree (object_path);

    /* quit if device doesn't exist in the sidebar */
    if (sideview_iter == NULL)
        return;

    gtk_tree_model_get (gtk_tree_view_get_model(GTK_TREE_VIEW(sideview)), sideview_iter,
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
                                       p_supply == TRUE ? _("True") : _("False"));

    if ( type != UP_DEVICE_KIND_LINE_POWER )
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
            str = g_strdup_printf("%d%%", (guint) percent);

            update_device_info_value_for_name (view, list_store, _("Current charge"), str);

            g_free(str);
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
            str2 = g_strdup_printf ("%s (%d%%)", str, (guint) (energy_full / energy_full_design *100));

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

    update_sideview_icon (device);
    gtk_widget_show_all (GTK_WIDGET(view));
}

static void
#if UP_CHECK_VERSION(0, 99, 0)
device_changed_cb (UpDevice *device, GParamSpec *pspec, gpointer user_data)
#else
device_changed_cb (UpDevice *device, gpointer user_data)
#endif
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
    GtkWidget *view;
    const gchar *object_path = up_device_get_object_path(device);
    gulong signal_id;
    guint index;
    static gboolean first_run = TRUE;

    TRACE("entering for %s", object_path);

    /* don't add the same device twice */
    device_iter = find_device_in_tree (object_path);
    if (device_iter)
    {
        gtk_tree_iter_free (device_iter);
        return;
    }

#if UP_CHECK_VERSION(0, 99, 0)
    signal_id = g_signal_connect (device, "notify", G_CALLBACK (device_changed_cb), NULL);
#else
    signal_id = g_signal_connect (device, "changed", G_CALLBACK (device_changed_cb), NULL);
#endif

    sideview_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (sideview)));

    index = gtk_notebook_get_n_pages (GTK_NOTEBOOK (device_details_notebook));

    /* Create the page that the update_device_details will update/replace */
    view = gtk_tree_view_new ();
    gtk_notebook_append_page (GTK_NOTEBOOK (device_details_notebook), view, NULL);

    /* Create the list store that the devices view will display */
    devices_store = gtk_list_store_new (XFPM_DEVICE_INFO_LAST, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (devices_store));

    /* Create the headers for this item in the device details tab */
    renderer = gtk_cell_renderer_text_new ();

    /*Device Attribute*/
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "text", XFPM_DEVICE_INFO_NAME, NULL);
    gtk_tree_view_column_set_title (col, _("Attribute"));
    gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

    /*Device Attribute Value*/
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "text", XFPM_DEVICE_INFO_VALUE, NULL);
    gtk_tree_view_column_set_title (col, _("Value"));

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
    if ((starting_device_id == NULL && first_run == TRUE) ||
        (g_strcmp0 (object_path, starting_device_id) == 0))
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

    TRACE("entering for %s", object_path);

    iter = find_device_in_tree (object_path);

    if (iter == NULL)
        return;

    list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(sideview)));

    gtk_tree_model_get (GTK_TREE_MODEL(list_store), iter,
                        COL_SIDEBAR_SIGNAL_ID, &signal_id,
                        COL_SIDEBAR_BATTERY_DEVICE, &device,
                        -1);

    gtk_list_store_remove (list_store, iter);

    if (device)
        g_signal_handler_disconnect (device, signal_id);
}

static void
device_added_cb (UpClient *upclient, UpDevice *device, gpointer user_data)
{
    add_device (device);
}

#if UP_CHECK_VERSION(0, 99, 0)
static void
device_removed_cb (UpClient *upclient, const gchar *object_path, gpointer user_data)
{
    remove_device (object_path);
}
#else
static void
device_removed_cb (UpClient *upclient, UpDevice *device, gpointer user_data)
{
    const gchar *object_path = up_device_get_object_path(device);
    remove_device (object_path);
}
#endif

static void
add_all_devices (void)
{
#if !UP_CHECK_VERSION(0, 99, 0)
    /* the device-add callback is called for each device */
    up_client_enumerate_devices_sync(upower, NULL, NULL);
#else
    GPtrArray *array = NULL;
    guint i;

    array = up_client_get_devices(upower);

    if ( array )
    {
        for ( i = 0; i < array->len; i++)
        {
            UpDevice *device = g_ptr_array_index (array, i);

            add_device (device);
        }
        g_ptr_array_free (array, TRUE);
    }
#endif
}

static void
settings_create_devices_list (void)
{
    upower = up_client_new ();

    g_signal_connect (upower, "device-added", G_CALLBACK (device_added_cb), NULL);
    g_signal_connect (upower, "device-removed", G_CALLBACK (device_removed_cb), NULL);

    add_all_devices ();
}

static void
view_cursor_changed_cb (GtkTreeView *view, gpointer *user_data)
{
    GtkTreeSelection *sel;
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gint int_data = 0;

    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

    if ( !gtk_tree_selection_get_selected (sel, &model, &selected_row))
	return;

    gtk_tree_model_get(model,
                       &selected_row,
                       COL_SIDEBAR_INT,
                       &int_data,
                       -1);

    gtk_notebook_set_current_page (GTK_NOTEBOOK (device_details_notebook), int_data);
}

static void
settings_quit (GtkWidget *widget, XfconfChannel *channel)
{
    g_object_unref (channel);
    xfconf_shutdown();
    gtk_widget_destroy (widget);
    gtk_main_quit();
}

static void dialog_response_cb (GtkDialog *dialog, gint response, XfconfChannel *channel)
{
    switch(response)
    {
	case GTK_RESPONSE_HELP:
	    xfce_dialog_show_help (GTK_WINDOW (dialog), "xfce4-power-manager", "start", NULL);
	    break;
	default:
	    settings_quit (GTK_WIDGET (dialog), channel);
	    break;
    }
}

static void
delete_event_cb (GtkWidget *plug, GdkEvent *ev, XfconfChannel *channel)
{
    settings_quit (plug, channel);
}

GtkWidget *
xfpm_settings_dialog_new (XfconfChannel *channel, gboolean auth_suspend,
                          gboolean auth_hibernate, gboolean can_suspend,
                          gboolean can_hibernate, gboolean can_shutdown,
                          gboolean has_battery, gboolean has_lcd_brightness,
                          gboolean has_lid, gboolean has_sleep_button,
                          gboolean has_hibernate_button, gboolean has_power_button,
                          GdkNativeWindow id, gchar *device_id)
{
    GtkWidget *plug;
    GtkWidget *dialog;
    GtkWidget *plugged_box;
    GtkWidget *viewport;
    GtkWidget *hbox;
    GtkWidget *on_battery_blank, *on_ac_blank;
    GtkListStore *list_store;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    GError *error = NULL;
    guint val;

    XFPM_DEBUG ("auth_hibernate=%s auth_suspend=%s can_shutdown=%s can_suspend=%s can_hibernate=%s " \
                "has_battery=%s has_lcd_brightness=%s has_lid=%s has_sleep_button=%s " \
                "has_hibernate_button=%s has_power_button=%s",
      xfpm_bool_to_string (has_battery), xfpm_bool_to_string (auth_hibernate),
	  xfpm_bool_to_string (can_shutdown), xfpm_bool_to_string (auth_suspend),
	  xfpm_bool_to_string (can_suspend), xfpm_bool_to_string (can_hibernate),
	  xfpm_bool_to_string (has_lcd_brightness), xfpm_bool_to_string (has_lid),
	  xfpm_bool_to_string (has_sleep_button), xfpm_bool_to_string (has_hibernate_button),
	  xfpm_bool_to_string (has_power_button));

    xml = xfpm_builder_new_from_string (xfpm_settings_ui, &error);
    
    if ( G_UNLIKELY (error) )
    {
	xfce_dialog_show_error (NULL, error, "%s", _("Check your power manager installation"));
	g_error ("%s", error->message);
    }
    
    lcd_brightness = has_lcd_brightness;
    starting_device_id = device_id;
    
    on_battery_dpms_sleep = GTK_WIDGET (gtk_builder_get_object (xml, "sleep-dpms-on-battery"));
    on_battery_dpms_off = GTK_WIDGET (gtk_builder_get_object (xml, "off-dpms-on-battery"));
    on_ac_dpms_sleep = GTK_WIDGET (gtk_builder_get_object (xml, "sleep-dpms-on-ac"));
    on_ac_dpms_off = GTK_WIDGET (gtk_builder_get_object (xml, "off-dpms-on-ac"));

    on_battery_blank = GTK_WIDGET (gtk_builder_get_object (xml, "blank-on-battery"));
    val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX ON_BATTERY_BLANK, 10);
    gtk_range_set_value (GTK_RANGE (on_battery_blank), val);
    xfconf_g_property_bind (channel, PROPERTIES_PREFIX ON_BATTERY_BLANK,
                            G_TYPE_INT, gtk_range_get_adjustment (GTK_RANGE (on_battery_blank)),
                            "value");

    on_ac_blank = GTK_WIDGET (gtk_builder_get_object (xml, "blank-on-ac"));
    xfconf_g_property_bind (channel, PROPERTIES_PREFIX ON_AC_BLANK,
                            G_TYPE_INT, gtk_range_get_adjustment (GTK_RANGE (on_ac_blank)),
                            "value");

    dialog = GTK_WIDGET (gtk_builder_get_object (xml, "xfpm-settings-dialog"));
    nt = GTK_WIDGET (gtk_builder_get_object (xml, "main-notebook"));

    /* Devices listview */
    sideview = gtk_tree_view_new ();
    list_store = gtk_list_store_new (NCOLS_SIDEBAR,
                                     GDK_TYPE_PIXBUF, /* COL_SIDEBAR_ICON */
                                     G_TYPE_STRING,   /* COL_SIDEBAR_NAME */
                                     G_TYPE_INT,      /* COL_SIDEBAR_INT */
                                     G_TYPE_POINTER,  /* COL_SIDEBAR_BATTERY_DEVICE */
                                     G_TYPE_STRING,   /* COL_SIDEBAR_OBJECT_PATH */
                                     G_TYPE_ULONG,    /* COL_SIDEBAR_SIGNAL_ID */
                                     G_TYPE_POINTER   /* COL_SIDEBAR_VIEW */
                                     );

    gtk_tree_view_set_model (GTK_TREE_VIEW (sideview), GTK_TREE_MODEL (list_store));

    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (sideview),TRUE);
    col = gtk_tree_view_column_new ();

    renderer = gtk_cell_renderer_pixbuf_new ();

    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "pixbuf", 0, NULL);

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
    hbox = gtk_hbox_new (FALSE, 3);

    viewport = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (viewport), sideview);
    gtk_box_pack_start (GTK_BOX (hbox), viewport, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), device_details_notebook, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
    gtk_notebook_append_page (GTK_NOTEBOOK (nt), hbox, gtk_label_new (_("Devices")) );

    gtk_widget_show_all (sideview);
    gtk_widget_show_all (viewport);
    gtk_widget_show_all (hbox);

    settings_create_devices_list ();

    xfpm_settings_on_ac (channel,
                         auth_suspend,
                         auth_hibernate,
                         can_suspend,
                         can_hibernate,
                         has_lcd_brightness,
                         has_lid);

    if ( has_battery )
    xfpm_settings_on_battery (channel,
                              auth_suspend,
                              auth_hibernate,
                              can_suspend,
                              can_hibernate,
                              can_shutdown,
                              has_lcd_brightness,
                              has_lid);
    else
    {
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"display_onbattery")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"display_pluggedin")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"sleep-dpms-on-battery")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"off-dpms-on-battery")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"brg-on-battery")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"brg-level-on-battery")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"system_onbattery")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"system_pluggedin")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"inactivity-on-battery")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"on-battery-lid")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"on-battery-lid-label")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (xml ,"blank-on-battery")));
    }
    
    xfpm_settings_general (channel, auth_suspend, auth_hibernate, can_suspend, can_hibernate, can_shutdown,
                           has_sleep_button, has_hibernate_button, has_power_button );

    xfpm_settings_advanced (channel, auth_suspend, auth_hibernate, can_suspend, can_hibernate, has_battery);

    if ( id != 0 )
    {
	plugged_box = GTK_WIDGET (gtk_builder_get_object (xml, "plug-child"));
	plug = gtk_plug_new (id);
	gtk_widget_show (plug);
	gtk_widget_reparent (plugged_box, plug);
	g_signal_connect (plug, "delete-event", 
			  G_CALLBACK (delete_event_cb), channel);
	gdk_notify_startup_complete ();
    }
    else
    {
	g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), channel);
	gtk_widget_show (dialog);
    }
    
    gtk_builder_connect_signals (xml, channel);

    /* If we passed in a device to display, show the devices tab now */
    if (device_id != NULL)
    {
	/* Assuming the last page is the devices tab */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (nt), gtk_notebook_get_n_pages (GTK_NOTEBOOK(nt)) - 1);
    }

    return dialog;
}
