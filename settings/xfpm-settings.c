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

#include <xfconf/xfconf.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfpm-common.h"
#include "xfpm-icons.h"
#include "xfpm-debug.h"

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

static  gboolean  lcd_brightness = FALSE;

/*
 * GtkBuilder callbacks
 */
void	    brightness_level_on_ac		   (GtkSpinButton *w,
						    XfconfChannel *channel);

void 	    brightness_level_on_battery 	   (GtkSpinButton *w,  
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

void        power_save_toggled_cb                  (GtkWidget *w, 
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

void        on_spindown_hdd_changed		   (GtkWidget *w,
						    XfconfChannel *channel);

void        _cursor_changed_cb 			   (GtkTreeView *view, 
						    gpointer data);



void brightness_level_on_ac (GtkSpinButton *w,  XfconfChannel *channel)
{
    guint val = (guint) gtk_spin_button_get_value (w);
    
    if (!xfconf_channel_set_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_AC, val) )
    {
	g_critical ("Unable to set value %u for property %s\n", val, BRIGHTNESS_LEVEL_ON_AC);
    }
}

void brightness_level_on_battery (GtkSpinButton *w,  XfconfChannel *channel)
{
     guint val = (guint) gtk_spin_button_get_value (w);
    
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
power_save_toggled_cb (GtkWidget *w, XfconfChannel *channel)
{
    gboolean val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(w));
    
    if (!xfconf_channel_set_bool (channel, PROPERTIES_PREFIX POWER_SAVE_ON_BATTERY, val) )
    {
	g_critical ("Cannot set value for property %s\n", POWER_SAVE_ON_BATTERY);
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

void on_spindown_hdd_changed	(GtkWidget *w, XfconfChannel *channel)
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
    
    if ( !xfconf_channel_set_uint (channel, PROPERTIES_PREFIX SPIN_DOWN_HDD, value) )
    {
	g_critical ("Unable to set value for property %s", SPIN_DOWN_HDD);
    }
}

static void
xfpm_settings_on_battery (XfconfChannel *channel, gboolean auth_hibernate, 
			  gboolean auth_suspend, gboolean can_shutdown, 
			  gboolean can_suspend, gboolean can_hibernate, 
			  gboolean has_lcd_brightness, gboolean has_lid,
			  gboolean devkit_disk, gboolean can_spin_down)
{
    gboolean valid;
    gint list_value;
    gint val;
    gboolean save_power;
    GtkListStore *list_store;
    GtkTreeIter iter;
    GtkWidget *power_save;
    GtkWidget *inact;
    GtkWidget *battery_critical;
    GtkWidget *lid;
    GtkWidget *label;
    GtkWidget *brg;
    GtkWidget *brg_level;
    GtkWidget *spin_down_hdd;

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
    
    power_save = GTK_WIDGET (gtk_builder_get_object (xml, "power-save"));
    save_power = xfconf_channel_get_bool (channel, PROPERTIES_PREFIX POWER_SAVE_ON_BATTERY, TRUE);
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(power_save), save_power);
    
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
	gtk_list_store_set (list_store, &iter, 0, _("Nothing"), 1, LID_TRIGGER_NOTHING, -1);
	
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
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (lid), XFPM_DO_NOTHING);
	
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
     * 
     * Brightness on battery power
     */
    brg = GTK_WIDGET (gtk_builder_get_object (xml ,"brg-on-battery"));
    brg_level = GTK_WIDGET (gtk_builder_get_object (xml ,"brg-level-on-battery"));
    if ( has_lcd_brightness )
    {
	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_ON_BATTERY, 120);
	
	gtk_range_set_value (GTK_RANGE(brg), val);
	
	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_BATTERY, 20);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (brg_level), val);
	
    }
    else
    {
	gtk_widget_set_sensitive (GTK_WIDGET (brg), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (brg_level), FALSE);
    }
#ifndef HAVE_DPMS
    if ( !has_lcd_brightness )
    {
	gtk_notebook_remove_page (GTK_NOTEBOOK (nt), 1);
    }
#endif

    spin_down_hdd = GTK_WIDGET (gtk_builder_get_object (xml, "spin-down-hdd"));

    if ( can_spin_down && devkit_disk )
    {
	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);

	gtk_combo_box_set_model (GTK_COMBO_BOX(spin_down_hdd), GTK_TREE_MODEL(list_store));

	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Never"), 1, SPIN_DOWN_HDD_NEVER, -1);

	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("On battery"), 1, SPIN_DOWN_HDD_ON_BATTERY, -1);

	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Plugged in"), 1, SPIN_DOWN_HDD_PLUGGED_IN, -1);

	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, _("Always"), 1, SPIN_DOWN_HDD_ALWAYS, -1);

	gtk_combo_box_set_active (GTK_COMBO_BOX (spin_down_hdd), 0);

	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX SPIN_DOWN_HDD, SPIN_DOWN_HDD_NEVER);

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
    gtk_widget_set_sensitive (spin_down_hdd, can_spin_down);
    
    if ( !devkit_disk )
    {
	gtk_widget_hide (spin_down_hdd);
    }
    else if ( !can_spin_down )
    {
	gtk_widget_set_tooltip_text (spin_down_hdd, _("Spinning down hard disks permission denied"));
    }
}

static void
xfpm_settings_on_ac (XfconfChannel *channel, gboolean auth_suspend, 
		     gboolean auth_hibernate, gboolean can_suspend, 
		     gboolean can_hibernate, gboolean has_lcd_brightness, 
		     gboolean has_lid, gboolean devkit_disk, gboolean can_spin_down)
{
    GtkWidget *inact;
    GtkWidget *lid;
    GtkWidget *brg;
    GtkWidget *brg_level;
    GtkWidget *spin_down_hdd;
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
	gtk_list_store_set (list_store, &iter, 0, _("Nothing"), 1, LID_TRIGGER_NOTHING, -1);
	
	if ( can_suspend )
	{
	    gtk_list_store_append(list_store, &iter);
	    gtk_list_store_set (list_store, &iter, 0, _("Suspend"), 1, LID_TRIGGER_SUSPEND, -1);
	}
	
	if ( can_hibernate )
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
     * 
     * Brightness on AC power
     */
    brg = GTK_WIDGET (gtk_builder_get_object (xml ,"brg-on-ac"));
    brg_level = GTK_WIDGET (gtk_builder_get_object (xml ,"brg-level-on-ac"));
    if ( has_lcd_brightness )
    {
	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_ON_AC, 9);
	
	gtk_range_set_value (GTK_RANGE(brg), val);
	
	val = xfconf_channel_get_uint (channel, PROPERTIES_PREFIX BRIGHTNESS_LEVEL_ON_AC, 80);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (brg_level), val);
	
    }
    else
    {
	gtk_widget_set_sensitive (GTK_WIDGET (brg), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (brg_level), FALSE);
    }
#ifndef HAVE_DPMS
    if ( !has_lcd_brightness )
    {
	gtk_notebook_remove_page (GTK_NOTEBOOK (GTK_WIDGET (gtk_builder_get_object (xml, "on-ac-notebook"))), 1);
    }
#endif

    spin_down_hdd = GTK_WIDGET (gtk_builder_get_object (xml, "spin-down-hdd"));
    /*
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spin_down), 
				  xfconf_channel_get_bool (channel, PROPERTIES_PREFIX SPIN_DOWN_ON_AC, FALSE)); */
				  
    gtk_widget_set_sensitive (spin_down_hdd, can_spin_down);
    
    if ( !devkit_disk )
    {
	gtk_widget_hide (spin_down_hdd);
    }
    else if ( !can_spin_down )
    {
	gtk_widget_set_tooltip_text (spin_down_hdd, _("Spinning down hard disks permission denied"));
    }

}

static void
xfpm_settings_general (XfconfChannel *channel, gboolean auth_hibernate, 
		       gboolean auth_suspend, gboolean can_shutdown,  
		       gboolean can_suspend, gboolean can_hibernate,
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
xfpm_settings_advanced (XfconfChannel *channel, gboolean system_laptop, 
		        gboolean auth_hibernate, gboolean auth_suspend,
			gboolean can_suspend, gboolean can_hibernate)
{
    guint val;
    gchar *str;
    GtkWidget *critical_level;
    GtkWidget *lock;
    GtkWidget *label;
    GtkWidget *sleep_dpms_mode;
    GtkWidget *network_manager_sleep;
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
    label = GTK_WIDGET (gtk_builder_get_object (xml, "system-sleepmode-label"));
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
    if ( system_laptop )
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

    /*
     * Network Manager Sleep for suspend/hibernate
     */
    network_manager_sleep = GTK_WIDGET (gtk_builder_get_object (xml, "network-manager-sleep"));

#ifdef WITH_NETWORK_MANAGER
    val = xfconf_channel_get_bool (channel, PROPERTIES_PREFIX NETWORK_MANAGER_SLEEP, TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(network_manager_sleep), val);

    xfconf_g_property_bind(channel, PROPERTIES_PREFIX NETWORK_MANAGER_SLEEP,
                           G_TYPE_BOOLEAN, G_OBJECT(network_manager_sleep),
                           "active");
#else
    gtk_widget_hide (network_manager_sleep);
#endif
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
    DBG("response %d", response);
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
xfpm_settings_dialog_new (XfconfChannel *channel, gboolean system_laptop, 
			  gboolean auth_hibernate, gboolean auth_suspend, 
			  gboolean can_shutdown, gboolean can_suspend, 
			  gboolean can_hibernate, gboolean has_lcd_brightness, 
			  gboolean has_lid, gboolean has_sleep_button, 
			  gboolean has_hibernate_button, gboolean has_power_button,
			  gboolean devkit_disk, gboolean can_spin_down, 
			  GdkNativeWindow id)
{
    GtkWidget *plug;
    GtkWidget *dialog;
    GtkWidget *plugged_box;
    GError *error = NULL;

    XFPM_DEBUG ("system_laptop=%s auth_hibernate=%s  auth_suspend=%s can_shutdown=%s can_suspend=%s can_hibernate=%s has_lcd_brightness=%s has_lid=%s "\
           "has_sleep_button=%s has_hibernate_button=%s has_power_button=%s can_spin_down=%s",
	  xfpm_bool_to_string (system_laptop), xfpm_bool_to_string (auth_hibernate), 
	  xfpm_bool_to_string (can_shutdown), xfpm_bool_to_string (auth_suspend),
	  xfpm_bool_to_string (can_suspend), xfpm_bool_to_string (can_hibernate),
	  xfpm_bool_to_string (has_lcd_brightness), xfpm_bool_to_string (has_lid),
	  xfpm_bool_to_string (has_sleep_button), xfpm_bool_to_string (has_hibernate_button),
	  xfpm_bool_to_string (has_power_button), xfpm_bool_to_string (can_spin_down) );

    xml = xfpm_builder_new_from_string (xfpm_settings_ui, &error);
    
    if ( G_UNLIKELY (error) )
    {
	xfce_dialog_show_error (NULL, error, "%s", _("Check your power manager installation"));
	g_error ("%s", error->message);
    }
    
    lcd_brightness = has_lcd_brightness;
    
    on_battery_dpms_sleep = GTK_WIDGET (gtk_builder_get_object (xml, "sleep-dpms-on-battery"));
    on_battery_dpms_off = GTK_WIDGET (gtk_builder_get_object (xml, "off-dpms-on-battery"));
    on_ac_dpms_sleep = GTK_WIDGET (gtk_builder_get_object (xml, "sleep-dpms-on-ac"));
    on_ac_dpms_off = GTK_WIDGET (gtk_builder_get_object (xml, "off-dpms-on-ac"));

    dialog = GTK_WIDGET (gtk_builder_get_object (xml, "xfpm-settings-dialog"));
    nt = GTK_WIDGET (gtk_builder_get_object (xml, "main-notebook"));
    
    xfpm_settings_on_ac (channel, 
			 auth_hibernate, 
			 auth_suspend, 
			 can_suspend, 
			 can_hibernate, 
			 has_lcd_brightness, 
			 has_lid,
			 devkit_disk,
			 can_spin_down);
    
    if ( system_laptop )
	xfpm_settings_on_battery (channel, 
				  auth_hibernate, 
				  auth_suspend, 
				  can_shutdown, 
				  can_suspend, 
				  can_hibernate, 
				  has_lcd_brightness, 
				  has_lid,
				  devkit_disk,
				  can_spin_down);
    
    xfpm_settings_general   (channel, auth_hibernate, auth_suspend, can_shutdown, can_suspend, can_hibernate,
			     has_sleep_button, has_hibernate_button, has_power_button );
			     
    xfpm_settings_advanced  (channel, system_laptop, auth_hibernate, auth_suspend, can_suspend, can_hibernate);
    
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
    
    return dialog;
}
