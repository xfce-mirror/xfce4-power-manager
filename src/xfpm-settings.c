/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <ali.slackware@gmail.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gtk/gtk.h>

#include <xfconf/xfconf.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfpm-settings.h"
#include "xfpm-enums.h"
#include "xfpm-common.h"

#ifdef HAVE_DPMS
#include "xfpm-dpms-spins.h"
#endif


/// Global Variable ///
static GtkWidget *nt;

static GtkWidget *performance_on_ac;
static GtkWidget *ondemand_on_ac;
static GtkWidget *powersave_on_ac;
static GtkWidget *conservative_on_ac;
static GtkWidget *userspace_on_ac;

static GtkWidget *performance_on_batt;
static GtkWidget *ondemand_on_batt;
static GtkWidget *powersave_on_batt;
static GtkWidget *conservative_on_batt;
static GtkWidget *userspace_on_batt;

#ifdef HAVE_DPMS
static GtkWidget *dpms_op;
static GtkWidget *on_batt_dpms;
static GtkWidget *on_ac_dpms;
#endif

/// Callback Setting Functions ///
static void
set_show_tray_icon_cb(GtkWidget *widget,XfconfChannel *channel)
{
    gboolean value = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    
    if(!xfconf_channel_set_uint(channel,SHOW_TRAY_ICON_CFG,value))
    {
        g_critical("Cannot set value %s\n",SHOW_TRAY_ICON_CFG);
    }
}

static void
set_battery_critical_charge_cb(GtkWidget *widget,XfconfChannel *channel)
{
    guint value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
    
    if(!xfconf_channel_set_uint(channel,CRITICAL_BATT_CFG,value))
    {
        g_critical("Cannot set value %s\n",CRITICAL_BATT_CFG);
    }
}

static void
set_critical_action_cb(GtkWidget *widget,XfconfChannel *channel)
{
    guint value = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    
    if(!xfconf_channel_set_uint(channel,CRITICAL_BATT_ACTION_CFG,value))
    {
        g_critical("Cannot set value %s\n",CRITICAL_BATT_ACTION_CFG);
    }
}

static void
set_power_button_action_cb(GtkWidget *widget,XfconfChannel *channel)
{
    guint value = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    
    if(!xfconf_channel_set_uint(channel,POWER_SWITCH_CFG,value))
    {
        g_critical("Cannot set value %s\n",POWER_SWITCH_CFG);
    }
}

static void
set_sleep_button_action_cb(GtkWidget *widget,XfconfChannel *channel)
{
    guint value = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    
    if(!xfconf_channel_set_uint(channel,SLEEP_SWITCH_CFG,value))
    {
        g_critical("Cannot set value %s\n",SLEEP_SWITCH_CFG);
    }
}

static void
set_lid_button_action_cb(GtkWidget *widget,XfconfChannel *channel)
{
    guint value = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    
    if(!xfconf_channel_set_uint(channel,LID_SWITCH_CFG,value))
    {
        g_critical("Cannot set value %s\n",LID_SWITCH_CFG);
    }
}

#ifdef HAVE_LIBNOTIFY
static void
set_battery_state_notification_cb(GtkWidget *widget,XfconfChannel *channel)
{
    gboolean value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    
    if(!xfconf_channel_set_bool(channel,BATT_STATE_NOTIFICATION_CFG,value))
    {
        g_critical("Cannot set value %s\n",BATT_STATE_NOTIFICATION_CFG);
    }
} 
#endif

#ifdef HAVE_DPMS
static void
set_dpms_cb(GtkWidget *widget,XfconfChannel *channel)
{
    gboolean value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    
    if(!xfconf_channel_set_bool(channel,DPMS_ENABLE_CFG,value))
    {
        g_critical("Cannot set value %s\n",DPMS_ENABLE_CFG);
    }
    
    xfpm_dpms_spins_set_active(XFPM_DPMS_SPINS(on_ac_dpms),value); 
    
    if ( GTK_IS_WIDGET(on_batt_dpms) )
    {
        xfpm_dpms_spins_set_active(XFPM_DPMS_SPINS(on_batt_dpms),value); 
    }
}

static void
set_dpms_on_battery_cb(GtkWidget *spin,guint value1,guint value2,
                      guint value3,XfconfChannel *channel)
{
    GPtrArray *arr = g_ptr_array_sized_new(3);
    GValue *val;
    
    val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_UINT);
    g_value_set_uint(val, value1);
    g_ptr_array_add(arr, val);
    
    val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_UINT);
    g_value_set_uint(val, value2);
    g_ptr_array_add(arr, val);
    
    val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_UINT);
    g_value_set_uint(val, value3);
    g_ptr_array_add(arr, val);
    
    if (!xfconf_channel_set_arrayv(channel,
                                  ON_BATT_DPMS_TIMEOUTS_CFG,
                                  arr) )
    {                              
        g_critical("Cannot set value for %s \n",ON_BATT_DPMS_TIMEOUTS_CFG);
        xfconf_array_free(arr);
        return;
    }
    xfconf_array_free(arr);
}

static void
set_dpms_on_ac_cb(GtkWidget *spin,guint value1,guint value2,
                  guint value3,XfconfChannel *channel)
{
    GPtrArray *arr = g_ptr_array_sized_new(3);
    GValue *val;
    
    val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_UINT);
    g_value_set_uint(val, value1);
    g_ptr_array_add(arr, val);
    
    val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_UINT);
    g_value_set_uint(val, value2);
    g_ptr_array_add(arr, val);
    
    val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_UINT);
    g_value_set_uint(val, value3);
    g_ptr_array_add(arr, val);
    
    if (!xfconf_channel_set_arrayv(channel,
                                  ON_AC_DPMS_TIMEOUTS_CFG,
                                  arr) )
    {                              
        g_critical("Cannot set value %s \n",ON_AC_DPMS_TIMEOUTS_CFG);
        xfconf_array_free(arr);
        return;
    }
    xfconf_array_free(arr);
}
#endif

static void
set_cpu_freq_scaling_cb(GtkWidget *widget,XfconfChannel *channel)
{
    gboolean value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    
    if(!xfconf_channel_set_bool(channel,CPU_FREQ_SCALING_CFG,value))
    {
        g_critical("Cannot set value %s\n",CPU_FREQ_SCALING_CFG);
    }
    gtk_widget_set_sensitive(ondemand_on_ac,value);
    gtk_widget_set_sensitive(performance_on_ac,value);
    gtk_widget_set_sensitive(powersave_on_ac,value);
    gtk_widget_set_sensitive(conservative_on_ac,value);
    gtk_widget_set_sensitive(userspace_on_ac,value);
    
    if ( GTK_IS_WIDGET(ondemand_on_batt) )  /* enough to check only one widget */ 
    {
        gtk_widget_set_sensitive(ondemand_on_batt,value);
        gtk_widget_set_sensitive(performance_on_batt,value);
        gtk_widget_set_sensitive(powersave_on_batt,value);
        gtk_widget_set_sensitive(conservative_on_batt,value);
        gtk_widget_set_sensitive(userspace_on_batt,value);
    }
}

static void
set_powersave_on_ac_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_AC_CPU_GOV_CFG,POWERSAVE))
    {
        g_critical("Cannot set value %s\n",ON_AC_CPU_GOV_CFG);
    }

}

static void
set_ondemand_on_ac_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_AC_CPU_GOV_CFG,ONDEMAND))
    {
        g_critical("Cannot set value %s\n",ON_AC_CPU_GOV_CFG);
    }

}

static void
set_performance_on_ac_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_AC_CPU_GOV_CFG,PERFORMANCE))
    {
        g_critical("Cannot set value %s\n",ON_AC_CPU_GOV_CFG);
    }

}

static void
set_conservative_on_ac_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_AC_CPU_GOV_CFG,CONSERVATIVE))
    {
        g_critical("Cannot set value %s\n",ON_AC_CPU_GOV_CFG);
    }

}

static void
set_userspace_on_ac_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_AC_CPU_GOV_CFG,USERSPACE))
    {
        g_critical("Cannot set value %s\n",ON_AC_CPU_GOV_CFG);
    }
}

static void
set_powersave_on_batt_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_BATT_CPU_GOV_CFG,POWERSAVE))
    {
        g_critical("Cannot set value %s\n",ON_BATT_CPU_GOV_CFG);
    }

}

static void
set_ondemand_on_batt_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_BATT_CPU_GOV_CFG,ONDEMAND))
    {
        g_critical("Cannot set value %s\n",ON_BATT_CPU_GOV_CFG);
    }

}

static void
set_performance_on_batt_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_BATT_CPU_GOV_CFG,PERFORMANCE))
    {
        g_critical("Cannot set value %s\n",ON_BATT_CPU_GOV_CFG);
    }

}

static void
set_conservative_on_batt_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_BATT_CPU_GOV_CFG,CONSERVATIVE))
    {
        g_critical("Cannot set value %s\n",ON_BATT_CPU_GOV_CFG);
    }

}
static void
set_userspace_on_batt_cb(GtkWidget *widget,XfconfChannel *channel)
{
    if(!xfconf_channel_set_uint(channel,ON_BATT_CPU_GOV_CFG,USERSPACE))
    {
        g_critical("Cannot set value %s\n",ON_BATT_CPU_GOV_CFG);
    }
}
/// End of Callback Setting Functions ///

/// Settings frames ///
static GtkWidget *
xfpm_settings_battery(XfconfChannel *channel, gboolean can_hibernate)
{
    GtkWidget *table;
    GtkWidget *frame,*align;
    GtkWidget *label;
    GtkWidget *critical_spin;
    GtkWidget *action;
    
    table = gtk_table_new(3,2,TRUE);
    gtk_widget_show(table);
    frame = xfce_create_framebox(_("Battery configuration"), &align);
    gtk_widget_show(frame);
    gtk_container_set_border_width(GTK_CONTAINER(frame),BORDER);
    
    label = gtk_label_new(_("Consider battery charge critical"));
    gtk_widget_show(label);
    gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,0,1);
    
    critical_spin = gtk_spin_button_new_with_range(1,15,1);
    gtk_widget_show(critical_spin);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(critical_spin),
                              xfconf_channel_get_uint(channel,CRITICAL_BATT_CFG,12));
    g_signal_connect(critical_spin,"value-changed",
                    G_CALLBACK(set_battery_critical_charge_cb),channel);
    gtk_table_attach(GTK_TABLE(table),critical_spin,1,2,0,1,GTK_SHRINK,GTK_SHRINK,0,0);
    
    label = gtk_label_new(_("When battery charge level is critical do"));
    gtk_widget_show(label);
    gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,1,2);
    
    action = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(action),_("Nothing"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(action),_("Shutdown"));
    /* FIXME */
    if ( can_hibernate )
    {
        gtk_combo_box_append_text(GTK_COMBO_BOX(action),_("Hibernate"));
    }                           
                               
    gtk_combo_box_set_active(GTK_COMBO_BOX(action),
                            xfconf_channel_get_uint(channel,CRITICAL_BATT_ACTION_CFG,0));
    gtk_widget_show(action);
    g_signal_connect(action,"changed",G_CALLBACK(set_critical_action_cb),channel);
    gtk_table_attach(GTK_TABLE(table),action,1,2,1,2,GTK_SHRINK,GTK_SHRINK,0,0);
    
#ifdef HAVE_LIBNOTIFY
    GtkWidget *notify_bt;        
    label = gtk_label_new(_("Enable battery state notification"));
    gtk_widget_show(label);
    gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,2,3);
    notify_bt = gtk_check_button_new();
    gtk_widget_show(notify_bt);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(notify_bt),
                                 xfconf_channel_get_bool(channel,BATT_STATE_NOTIFICATION_CFG,TRUE));                    
    g_signal_connect(notify_bt,"toggled",G_CALLBACK(set_battery_state_notification_cb),channel); 
    gtk_table_attach(GTK_TABLE(table),notify_bt,1,2,2,3,GTK_SHRINK,GTK_SHRINK,0,0);                             
#endif        
    
    gtk_container_add(GTK_CONTAINER(align),table);
    return frame;
    
}

static GtkWidget *
xfpm_settings_cpu_on_ac_adapter(XfconfChannel *channel,guint8 *govs,const gchar *label)
{
    GtkWidget *frame;
    GtkWidget *align;
    GtkWidget *vbox;
    
    GSList *list;
    guint current_governor = xfconf_channel_get_uint(channel,ON_AC_CPU_GOV_CFG,ONDEMAND);
    gboolean enable = xfconf_channel_get_bool(channel,CPU_FREQ_SCALING_CFG,TRUE);
    
    frame = xfce_create_framebox(label, &align);
    gtk_container_set_border_width(GTK_CONTAINER(frame),BORDER);
    gtk_widget_show(frame);
    
    vbox = gtk_vbox_new(FALSE,BORDER);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(align),vbox);
    
    performance_on_ac = gtk_radio_button_new_with_label(NULL,_("Performance"));
    gtk_box_pack_start (GTK_BOX (vbox), performance_on_ac, FALSE, FALSE, 0);
    
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(performance_on_ac));
    
    ondemand_on_ac = gtk_radio_button_new_with_label(list,_("Ondemand"));
    gtk_box_pack_start (GTK_BOX (vbox), ondemand_on_ac, FALSE, FALSE, 0);
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(ondemand_on_ac));
    
    userspace_on_ac = gtk_radio_button_new_with_label(list,_("Userspace"));
    gtk_box_pack_start (GTK_BOX (vbox), userspace_on_ac, FALSE, FALSE, 0);
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(userspace_on_ac));
    
    powersave_on_ac = gtk_radio_button_new_with_label(list,_("Powersave"));
    gtk_box_pack_start (GTK_BOX (vbox), powersave_on_ac, FALSE, FALSE, 0); 
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(powersave_on_ac));
    
    conservative_on_ac = gtk_radio_button_new_with_label(list,_("Conservative"));
    gtk_box_pack_start (GTK_BOX (vbox), conservative_on_ac, FALSE, FALSE, 0); 
    
    if ( govs[0]  == 1 ) 
    {
        gtk_widget_set_sensitive(powersave_on_ac,enable);
        g_signal_connect(powersave_on_ac,"pressed",G_CALLBACK(set_powersave_on_ac_cb),channel);
        gtk_widget_show(powersave_on_ac);
        if ( current_governor == POWERSAVE )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(powersave_on_ac),TRUE);
        }
    }
    if ( govs[1]  == 1 ) 
    {
        gtk_widget_set_sensitive(ondemand_on_ac,enable);
        g_signal_connect(ondemand_on_ac,"pressed",G_CALLBACK(set_ondemand_on_ac_cb),channel);
        gtk_widget_show(ondemand_on_ac);
        if ( current_governor == ONDEMAND )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ondemand_on_ac),TRUE);
        }
    }
    if ( govs[2]  == 1 ) 
    {
        gtk_widget_set_sensitive(performance_on_ac,enable);
        g_signal_connect(performance_on_ac,"pressed",G_CALLBACK(set_performance_on_ac_cb),channel);
        gtk_widget_show(performance_on_ac);
        if ( current_governor == PERFORMANCE )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(performance_on_ac),TRUE);
        }
    }
    if ( govs[3]  == 1 ) 
    {
        gtk_widget_set_sensitive(conservative_on_ac,enable);
        g_signal_connect(conservative_on_ac,"pressed",G_CALLBACK(set_conservative_on_ac_cb),channel);
        gtk_widget_show(conservative_on_ac);
        if ( current_governor == CONSERVATIVE )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(conservative_on_ac),TRUE);
        }
    }
    if ( govs[4]  == 1 ) 
    {
        gtk_widget_set_sensitive(userspace_on_ac,enable);
        g_signal_connect(userspace_on_ac,"pressed",G_CALLBACK(set_userspace_on_ac_cb),channel);
        gtk_widget_show(userspace_on_ac);
        if ( current_governor == USERSPACE )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(userspace_on_ac),TRUE);
        }
    }
    
    return frame;
}

static GtkWidget *
xfpm_settings_cpu_on_battery_power(XfconfChannel *channel,guint8 *govs)
{
    GtkWidget *frame;
    GtkWidget *align;
    GtkWidget *vbox;
    
    GSList *list;
    guint current_governor = xfconf_channel_get_uint(channel,ON_BATT_CPU_GOV_CFG,ONDEMAND);
    gboolean enable = xfconf_channel_get_bool(channel,CPU_FREQ_SCALING_CFG,TRUE);
    
    frame = xfce_create_framebox(_("CPU governor settings on battery power"), &align);
    gtk_container_set_border_width(GTK_CONTAINER(frame),BORDER);
    gtk_widget_show(frame);
    
    vbox = gtk_vbox_new(FALSE,BORDER);
    gtk_widget_show(vbox);
    gtk_container_add(GTK_CONTAINER(align),vbox);
    
    performance_on_batt = gtk_radio_button_new_with_label(NULL,_("Performance"));
    gtk_box_pack_start (GTK_BOX (vbox), performance_on_batt, FALSE, FALSE, 0);
    
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(performance_on_batt));
    
    ondemand_on_batt = gtk_radio_button_new_with_label(list,_("Ondemand"));
    gtk_box_pack_start (GTK_BOX (vbox), ondemand_on_batt, FALSE, FALSE, 0);
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(ondemand_on_batt));
    
    userspace_on_batt = gtk_radio_button_new_with_label(list,_("Userspace"));
    gtk_box_pack_start (GTK_BOX (vbox), userspace_on_batt, FALSE, FALSE, 0);
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(userspace_on_batt));
    
    powersave_on_batt = gtk_radio_button_new_with_label(list,_("Powersave"));
    gtk_box_pack_start (GTK_BOX (vbox), powersave_on_batt, FALSE, FALSE, 0); 
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(powersave_on_batt));
    
    conservative_on_batt = gtk_radio_button_new_with_label(list,_("Conservative"));
    gtk_box_pack_start (GTK_BOX (vbox), conservative_on_batt, FALSE, FALSE, 0); 
    
    /* FIXME: Ugly, and also support for other OS to be added */
    if ( govs[0]  == 1 ) 
    {
        gtk_widget_set_sensitive(powersave_on_batt,enable);
        g_signal_connect(powersave_on_batt,"pressed",G_CALLBACK(set_powersave_on_batt_cb),channel);
        gtk_widget_show(powersave_on_batt);
        if ( current_governor == POWERSAVE )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(powersave_on_batt),TRUE);
        }
    }
    if ( govs[1]  == 1 ) 
    {
        gtk_widget_set_sensitive(ondemand_on_batt,enable);
        g_signal_connect(ondemand_on_batt,"pressed",G_CALLBACK(set_ondemand_on_batt_cb),channel);
        gtk_widget_show(ondemand_on_batt);
        if ( current_governor == ONDEMAND )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ondemand_on_batt),TRUE);
        }
    }
    if ( govs[2]  == 1 ) 
    {
        gtk_widget_set_sensitive(performance_on_batt,enable);
        g_signal_connect(performance_on_batt,"pressed",G_CALLBACK(set_performance_on_batt_cb),channel);
        gtk_widget_show(performance_on_batt);
        if ( current_governor == PERFORMANCE )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(performance_on_batt),TRUE);
        }
    }
    if ( govs[3]  == 1 ) 
    {
        gtk_widget_set_sensitive(conservative_on_batt,enable);
        g_signal_connect(conservative_on_batt,"pressed",G_CALLBACK(set_conservative_on_batt_cb),channel);
        gtk_widget_show(conservative_on_batt);
        if ( current_governor == CONSERVATIVE )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(conservative_on_batt),TRUE);
        }
    }
    if ( govs[4]  == 1 ) 
    {
        gtk_widget_set_sensitive(userspace_on_batt,enable);
        g_signal_connect(userspace_on_batt,"pressed",G_CALLBACK(set_userspace_on_batt_cb),channel);
        gtk_widget_show(userspace_on_batt);
        if ( current_governor == USERSPACE )
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(userspace_on_batt),TRUE);
        }
    }
    return frame;
    
}

static GtkWidget *
xfpm_settings_cpu_freq(XfconfChannel *channel,guint8 *govs,gboolean laptop)
{
    GtkWidget *hbox;
    hbox = gtk_hbox_new(FALSE,2);
    gtk_widget_show(hbox);
    
    GtkWidget *frame;
    frame = xfpm_settings_cpu_on_ac_adapter(channel,
                                            govs,
                                            laptop ? _("Cpu freq settings on AC power") : _("Cpu freq settings"));
    gtk_box_pack_start(GTK_BOX(hbox),frame,FALSE,FALSE,0);
    
    if ( laptop )
    {
        frame = xfpm_settings_cpu_on_battery_power(channel,govs);
        gtk_box_pack_start(GTK_BOX(hbox),frame,FALSE,FALSE,0);
    }
    
    return hbox;
}

#ifdef HAVE_DPMS
static GtkWidget *
xfpm_settings_dpms_on_battery(XfconfChannel *channel)
{
    GtkWidget *frame,*align;  
    frame = xfce_create_framebox(_("Monitor DPMS settings on battery power"), &align);
    gtk_widget_show(frame);
    
    gtk_container_set_border_width(GTK_CONTAINER(frame),BORDER);
    
    on_batt_dpms = xfpm_dpms_spins_new();
    gtk_widget_show(on_batt_dpms);
    gtk_container_add(GTK_CONTAINER(align),on_batt_dpms);
    GPtrArray *arr;
    GValue *value;
    guint value1 = 3 ,value2 = 4 ,value3 = 5;
    arr = xfconf_channel_get_arrayv(channel,ON_BATT_DPMS_TIMEOUTS_CFG);
    if ( arr ) 
    {
        value = g_ptr_array_index(arr,0);
        value1 = g_value_get_uint(value);
        
        value = g_ptr_array_index(arr,1);
        value2 = g_value_get_uint(value);
        
        value = g_ptr_array_index(arr,2);
        value3 = g_value_get_uint(value);
        xfconf_array_free(arr);
    } 
    xfpm_dpms_spins_set_default_values(XFPM_DPMS_SPINS(on_batt_dpms),value1,value2,value3);
    gboolean dpms_enabled = xfconf_channel_get_bool(channel,DPMS_ENABLE_CFG,TRUE);
    xfpm_dpms_spins_set_active(XFPM_DPMS_SPINS(on_batt_dpms),dpms_enabled);                
    g_signal_connect(on_batt_dpms,"dpms-value-changed",
                     G_CALLBACK(set_dpms_on_battery_cb),channel);
    
    return frame;
    
}

static GtkWidget *
xfpm_settings_dpms_on_ac_adapter(XfconfChannel *channel,const gchar *label)
{
    GtkWidget *frame;
    GtkWidget *align;

    frame = xfce_create_framebox(label, &align);
    gtk_container_set_border_width(GTK_CONTAINER(frame),BORDER);
    gtk_widget_show(frame);
    
    on_ac_dpms = xfpm_dpms_spins_new();
    gtk_widget_show(on_ac_dpms);
    gtk_container_add(GTK_CONTAINER(align),on_ac_dpms);
    GPtrArray *arr;
    GValue *value;
    guint value1 = 30 ,value2 = 45 ,value3 = 60;
    arr = xfconf_channel_get_arrayv(channel,ON_AC_DPMS_TIMEOUTS_CFG);
    if ( arr ) 
    {
        value = g_ptr_array_index(arr,0);
        value1 = g_value_get_uint(value);
        
        value = g_ptr_array_index(arr,1);
        value2 = g_value_get_uint(value);
        
        value = g_ptr_array_index(arr,2);
        value3 = g_value_get_uint(value);
        xfconf_array_free(arr);
    } 
    xfpm_dpms_spins_set_default_values(XFPM_DPMS_SPINS(on_ac_dpms),value1,value2,value3);
    gboolean dpms_enabled = xfconf_channel_get_bool(channel,DPMS_ENABLE_CFG,TRUE);
    xfpm_dpms_spins_set_active(XFPM_DPMS_SPINS(on_ac_dpms),dpms_enabled);             
    g_signal_connect(on_ac_dpms,"dpms-value-changed",
                     G_CALLBACK(set_dpms_on_ac_cb),channel);
    
    return frame;   
}

static GtkWidget *
xfpm_settings_dpms(XfconfChannel *channel,gboolean laptop,gboolean dpms_capable)
{
    GtkWidget *hbox;
    hbox = gtk_hbox_new(FALSE,2);
    gtk_widget_show(hbox);
    
    GtkWidget *frame;
    frame = xfpm_settings_dpms_on_ac_adapter(channel,
                                            laptop ? _("Monitor settings on AC power") : _("Monitor settings"));
    gtk_box_pack_start(GTK_BOX(hbox),frame,FALSE,FALSE,0);
    
    if ( laptop )
    {
        frame = xfpm_settings_dpms_on_battery(channel);
        gtk_box_pack_start(GTK_BOX(hbox),frame,FALSE,FALSE,0);
    }
    
    if (! dpms_capable ) 
    {
        GtkWidget *label;
        label = gtk_label_new(_("Your monitor doesn't support DPMS settings"));
        gtk_widget_show(label);
        GtkWidget *vbox;
        vbox = gtk_vbox_new(FALSE,2);
        gtk_widget_show(vbox);
        gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,0);
        gtk_widget_set_sensitive(dpms_op,FALSE);
        xfpm_dpms_spins_set_active(XFPM_DPMS_SPINS(on_ac_dpms),FALSE); 
    
        if ( GTK_IS_WIDGET(on_batt_dpms) )
        {
            xfpm_dpms_spins_set_active(XFPM_DPMS_SPINS(on_batt_dpms),FALSE); 
        }
        return vbox;
    }
    return hbox;
}
#endif

static GtkWidget *
xfpm_settings_keys(XfconfChannel *channel,gboolean can_hibernate,
                   gboolean can_suspend,gboolean laptop)
{
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *frame,*align;
    
    frame = xfce_create_framebox(_("Keyboard shortcuts"), &align);
    gtk_widget_show(frame);
    table = gtk_table_new(3,2,TRUE);
    gtk_widget_show(table);
    gtk_container_add(GTK_CONTAINER(align),table);
    
    guint default_config;
    /// Power Button
    label = gtk_label_new(_("When power button is pressed do"));
    gtk_widget_show(label);
    gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,0,1);
    
    GtkWidget *power_button;
    default_config = xfconf_channel_get_uint(channel,POWER_SWITCH_CFG,0);
    power_button = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(power_button),_("Nothing"));
    if ( can_hibernate )
        gtk_combo_box_append_text(GTK_COMBO_BOX(power_button),_("Hibernate"));
    if ( can_suspend ) 
        gtk_combo_box_append_text(GTK_COMBO_BOX(power_button),_("Suspend"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(power_button),default_config);
    gtk_widget_show(power_button);
    gtk_table_attach(GTK_TABLE(table),power_button,1,2,0,1,GTK_SHRINK,GTK_SHRINK,0,0);
    g_signal_connect(power_button,"changed",G_CALLBACK(set_power_button_action_cb),channel);
    
    /// Sleep Button
    label = gtk_label_new(_("When sleep button is pressed do"));
    gtk_widget_show(label);
    gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,1,2);
    
    GtkWidget *sleep_button;
    default_config = xfconf_channel_get_uint(channel,SLEEP_SWITCH_CFG,0);
    sleep_button = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(sleep_button),_("Nothing"));
    if ( can_hibernate )
        gtk_combo_box_append_text(GTK_COMBO_BOX(sleep_button),_("Hibernate"));
    if ( can_suspend ) 
        gtk_combo_box_append_text(GTK_COMBO_BOX(sleep_button),_("Suspend"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(sleep_button),default_config);
    gtk_widget_show(sleep_button);
    gtk_table_attach(GTK_TABLE(table),sleep_button,1,2,1,2,GTK_SHRINK,GTK_SHRINK,0,0);
    g_signal_connect(sleep_button,"changed",G_CALLBACK(set_sleep_button_action_cb),channel);
    
    /// Lid Button
    if ( laptop )
    {
        label = gtk_label_new(_("When lid button is pressed do"));
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,2,3);
        GtkWidget *lid_button;
        default_config = xfconf_channel_get_uint(channel,LID_SWITCH_CFG,0);
        lid_button = gtk_combo_box_new_text();
        gtk_combo_box_append_text(GTK_COMBO_BOX(lid_button),_("Nothing"));
        if ( can_hibernate )
            gtk_combo_box_append_text(GTK_COMBO_BOX(lid_button),_("Hibernate"));
        if ( can_suspend ) 
            gtk_combo_box_append_text(GTK_COMBO_BOX(lid_button),_("Suspend"));
        gtk_combo_box_set_active(GTK_COMBO_BOX(lid_button),default_config);
        gtk_widget_show(lid_button);
        gtk_table_attach(GTK_TABLE(table),lid_button,1,2,2,3,GTK_SHRINK,GTK_SHRINK,0,0);
        g_signal_connect(lid_button,"changed",G_CALLBACK(set_lid_button_action_cb),channel);
    }
    
    return frame;
    
}

static GtkWidget *
xfpm_settings_general(XfconfChannel *channel,gboolean battery_settings)
{
    GtkWidget *table;
    GtkWidget *show_icon;
    GtkWidget *label;
    GtkWidget *frame,*align;
    
    frame = xfce_create_framebox(_("General Options"), &align);
    gtk_widget_show(frame);
    table = gtk_table_new(4,2,FALSE);
    gtk_widget_show(table);
    gtk_container_add(GTK_CONTAINER(align),table);
    
    /* Systray icon */
    label = gtk_label_new(_("System tray icon"));
    gtk_widget_show(label);
    gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,0,1);
    
    guint icon_cfg = xfconf_channel_get_uint(channel,SHOW_TRAY_ICON_CFG,0);
    show_icon = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(show_icon),_("Always Display an icon"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(show_icon),_("When battery is present"));
    if ( battery_settings )
    {
        gtk_combo_box_append_text(GTK_COMBO_BOX(show_icon),_("When battery is charging or discharging"));
                            
    } else if ( icon_cfg == 2 ) /* set to default value then */
    {
         if(!xfconf_channel_set_uint(channel,SHOW_TRAY_ICON_CFG,0))
         {
            g_critical("Cannot set value %s\n",SHOW_TRAY_ICON_CFG);
        
         }
         icon_cfg = 0;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(show_icon),icon_cfg);
    g_signal_connect(show_icon,"changed",G_CALLBACK(set_show_tray_icon_cb),channel);
    gtk_widget_show(show_icon);
    gtk_table_attach(GTK_TABLE(table),show_icon,1,2,0,1,GTK_SHRINK,GTK_SHRINK,0,0);
    GtkWidget *cpu_gov;
    gboolean cpu_gov_enabled;
    label = gtk_label_new(_("Enable CPU freq scaling control"));
    gtk_widget_show(label);
    gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,1,2);
    cpu_gov = gtk_check_button_new();
    gtk_widget_show(cpu_gov);
    cpu_gov_enabled = xfconf_channel_get_bool(channel,CPU_FREQ_SCALING_CFG,TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cpu_gov),
                                 cpu_gov_enabled);                    
    g_signal_connect(cpu_gov,"toggled",G_CALLBACK(set_cpu_freq_scaling_cb),channel);      
    gtk_table_attach(GTK_TABLE(table),cpu_gov,1,2,1,2,GTK_SHRINK,GTK_SHRINK,0,0); 
    
#ifdef HAVE_DPMS
    gboolean dpms_enabled;
    label = gtk_label_new(_("Enable DPMS control"));
    gtk_widget_show(label);
    gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,2,3);
    dpms_op = gtk_check_button_new();
    gtk_widget_show(dpms_op);
    dpms_enabled = xfconf_channel_get_bool(channel,DPMS_ENABLE_CFG,TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dpms_op),
                                 dpms_enabled);                    
    g_signal_connect(dpms_op,"toggled",G_CALLBACK(set_dpms_cb),channel);      
    gtk_table_attach(GTK_TABLE(table),dpms_op,1,2,2,3,GTK_SHRINK,GTK_SHRINK,0,0); 
#endif    
    return frame;
}

static void
_cursor_changed_cb(GtkTreeView *view,gpointer data)
{
    GtkTreeSelection *sel;
	GtkTreeModel     *model;
	GtkTreeIter       selected_row;
	gint int_data = 0;
	
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	
	gtk_tree_selection_get_selected(sel,&model,&selected_row);
		
    gtk_tree_model_get(model,
                       &selected_row,
                       1, 
                       &int_data,
                       -1);

    if ( int_data >= 0 && int_data <= 3 )
    {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(nt),int_data);
    }
    else
    {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(nt),0);
    }
        
}

static GtkWidget *
xfpm_settings_tree_view(gboolean is_laptop)
{
    GdkPixbuf *pix;
    GtkWidget *view;
    GtkListStore *list_store;
    GtkTreeIter iter;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    //GtkCellRenderer *text_renderer;
        
    list_store = gtk_list_store_new(2,GDK_TYPE_PIXBUF,G_TYPE_INT);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    col = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_pixbuf_new();
    
    gtk_tree_view_column_pack_start(col,renderer,TRUE);
    gtk_tree_view_column_add_attribute(col,renderer,"pixbuf",0);

    gtk_tree_view_column_set_title(col,_(" Advanced settings"));

    gtk_tree_view_append_column(GTK_TREE_VIEW(view),col);

    pix = xfpm_load_icon("gnome-cpu-frequency-applet",38);      
    gtk_list_store_append(list_store,&iter);
    gtk_list_store_set(list_store,&iter,0,pix,1,0,-1);
    g_object_unref(pix);

    if ( is_laptop )
    {
        pix = xfpm_load_icon("gpm-primary-100",38);
        gtk_list_store_append(list_store,&iter);
        gtk_list_store_set(list_store,&iter,0,pix,1,1,-1);
        g_object_unref(pix);
    }
   
    pix = xfpm_load_icon("keyboard",38);
    gtk_list_store_append(list_store,&iter);
    gtk_list_store_set(list_store,&iter,0,pix,1,2,-1);
    g_object_unref(pix);
    
#ifdef HAVE_DPMS    
    pix = xfpm_load_icon("display",38);      
    gtk_list_store_append(list_store,&iter);
    gtk_list_store_set(list_store,&iter,0,pix,1,3,-1);
    g_object_unref(pix);
#endif  
    
    g_signal_connect(view,"cursor-changed",G_CALLBACK(_cursor_changed_cb),NULL);
    
    return view;
}

GtkWidget *
xfpm_settings_new(XfconfChannel *channel,gboolean is_laptop,
                  gboolean can_hibernate,gboolean can_suspend,
                  gboolean dpms_capable,guint8 *govs)
{
    GtkWidget *Dialog;  /* Main dialog window */
    GtkWidget *mainbox; /* Box to get (Dialog)->vbox */
    GtkWidget *box;
    GtkWidget *table;   
    GtkWidget *view;
        
    Dialog = xfce_titled_dialog_new_with_buttons(_("Power Manager Preferences"),
                                                    NULL,
                                                    GTK_DIALOG_NO_SEPARATOR,
                                                    GTK_STOCK_CLOSE,
                                                    GTK_RESPONSE_CANCEL,
                                                    GTK_STOCK_HELP,
                                                    GTK_RESPONSE_HELP,
                                                    NULL);
    
    gtk_window_set_icon_name(GTK_WINDOW(Dialog),"gpm-ac-adapter");
    gtk_dialog_set_default_response(GTK_DIALOG(Dialog),GTK_RESPONSE_CANCEL);
    
    mainbox = GTK_DIALOG(Dialog)->vbox;
    
    /// General Options Frame
    box = xfpm_settings_general(channel,is_laptop);
    gtk_box_pack_start (GTK_BOX (mainbox), box, FALSE, FALSE, 0);
    
    /// Notebook container
    nt = gtk_notebook_new();
    gtk_widget_show(nt);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(nt),FALSE);
    table = gtk_table_new(1,2,FALSE);
    gtk_widget_show(table);
    gtk_box_pack_start (GTK_BOX (mainbox), table, FALSE, FALSE, 0);
    view = xfpm_settings_tree_view(is_laptop);
    gtk_table_attach_defaults(GTK_TABLE(table),view,0,1,0,1);  
    gtk_widget_show(view);  
    gtk_table_attach_defaults(GTK_TABLE(table),nt,1,2,0,1); 
    
    /// Cpu freq settings
    box = xfpm_settings_cpu_freq(channel,govs,is_laptop);
    gtk_notebook_append_page(GTK_NOTEBOOK(nt),box,NULL);

    /// Battery settings
    box = xfpm_settings_battery(channel,can_hibernate);
    gtk_notebook_append_page(GTK_NOTEBOOK(nt),box,NULL);

    /// Keyboard buttons settings
    box = xfpm_settings_keys(channel,can_hibernate,can_suspend,is_laptop);
    gtk_notebook_append_page(GTK_NOTEBOOK(nt),box,NULL); 
    
    /// Dpms settings
#ifdef HAVE_DPMS 
    box = xfpm_settings_dpms(channel,is_laptop,dpms_capable);
    gtk_notebook_append_page(GTK_NOTEBOOK(nt),box,NULL);
#endif 
    
    return Dialog;
}
