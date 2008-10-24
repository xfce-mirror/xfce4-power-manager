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

#include <gtk/gtk.h>

#include <glib.h>

#include <string.h>

#include "xfpm-battery-icon.h"
#include "xfpm-notify.h"
#include "xfpm-debug.h"
#include "xfpm-common.h"
#include "xfpm-enum-types.h"

#ifndef _
#define _(x) x
#endif

/* init */
static void xfpm_battery_icon_init(XfpmBatteryIcon *battery_icon);
static void xfpm_battery_icon_class_init(XfpmBatteryIconClass *klass);
static void xfpm_battery_icon_finalize(GObject *object);

static void xfpm_battery_icon_set_property(GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void xfpm_battery_icon_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec);

static gboolean   xfpm_battery_icon_size_change_cb(XfpmBatteryIcon *battery_icon,
                                                   gint size,
                                                   gpointer data);
                                                
static const gchar *xfpm_battery_icon_get_index(XfpmBatteryType type,
                                                guint percent);

static gchar *    xfpm_battery_icon_get_icon_prefix (XfpmBatteryType type);

static void       xfpm_battery_icon_update (XfpmBatteryIcon *battery_icon,
                                            XfpmBatteryState state,
                                            guint level,
                                            gboolean adapter_present);

G_DEFINE_TYPE(XfpmBatteryIcon,xfpm_battery_icon,GTK_TYPE_STATUS_ICON)

enum
{
    PROP_0,
    PROP_BATTERY_TYPE,
    PROP_BATTERY_STATE,
    PROP_LAST_FULL,
    PROP_BATT_CRITICAL,
#ifdef HAVE_LIBNOTIFY
    PROP_NOTIFY
#endif        
};

static void
xfpm_battery_icon_class_init(XfpmBatteryIconClass *klass) 
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->get_property = xfpm_battery_icon_get_property;
    object_class->set_property = xfpm_battery_icon_set_property;
    object_class->finalize = xfpm_battery_icon_finalize;
    
    g_object_class_install_property(object_class,
                                    PROP_BATTERY_TYPE,
                                    g_param_spec_enum("battery-type",
                                                      "Battery type",
                                                      "Battery typed",
                                                      XFPM_TYPE_BATTERY_TYPE,
                                                      PRIMARY, 
                                                      G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
    g_object_class_install_property(object_class,
                                    PROP_BATTERY_STATE,
                                    g_param_spec_enum("battery-state",
                                                      "Battery state",
                                                      "Battery state",
                                                      XFPM_TYPE_BATTERY_STATE,
                                                      0, /* Defaul not present */
                                                      G_PARAM_READWRITE));

    g_object_class_install_property(object_class,
                                    PROP_LAST_FULL,
                                    g_param_spec_uint("last-full",
                                                      "Last full",
                                                      "Last battery full charge",
                                                      0,
                                                      G_MAXUINT32,
                                                      4000,
                                                      G_PARAM_READWRITE|G_PARAM_CONSTRUCT ));
    
    g_object_class_install_property(object_class,
                                    PROP_BATT_CRITICAL,
                                    g_param_spec_uint("batt-critical-level",
                                                      "Battery critical level",
                                                      "Low battery level",
                                                      1,
                                                      20,
                                                      10,
                                                      G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
    
#ifdef HAVE_LIBNOTIFY
    g_object_class_install_property(object_class,
                                    PROP_NOTIFY,
                                    g_param_spec_boolean("systray-notify",
                                                         "systray-notify",
                                                         "systray notification",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
#endif
                                                                                                           
}

static void
xfpm_battery_icon_init(XfpmBatteryIcon *battery_icon)
{

    g_signal_connect(battery_icon,"size-changed",
                     G_CALLBACK(xfpm_battery_icon_size_change_cb),NULL);
    
    /* This is only use when the power manager loads, just for the tray icon to take it's 
     * place in the panel */
#ifdef HAVE_LIBNOTIFY    
    battery_icon->discard_notification = TRUE;
#endif
    battery_icon->icon    = -1;
    battery_icon->icon_loaded = FALSE;
}

static void xfpm_battery_icon_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    XfpmBatteryIcon *icon;
    icon = XFPM_BATTERY_ICON(object);
#ifdef DEBUG
    gchar *content;
    content = g_strdup_value_contents(value);
    XFPM_DEBUG("param:%s value contents:%s\n",pspec->name,content);
    g_free(content);
#endif            
    switch (prop_id)
    {
        case PROP_BATTERY_TYPE:
            icon->type = g_value_get_enum(value);
            break;    
        case PROP_BATTERY_STATE:
            icon->state = g_value_get_enum(value);
            break;
        case PROP_LAST_FULL:
            icon->last_full = g_value_get_uint(value);
            break;
        case PROP_BATT_CRITICAL:
            icon->critical_level = g_value_get_uint(value);
            break;      
#ifdef HAVE_LIBNOTIFY              
        case PROP_NOTIFY:
            icon->notify = g_value_get_boolean(value);
            break;
#endif            
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
            break;
    }                                    
}

static void xfpm_battery_icon_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    XfpmBatteryIcon *icon;
    icon = XFPM_BATTERY_ICON(object);
    
    switch (prop_id)
    {
        case PROP_BATTERY_TYPE:
            g_value_set_enum(value,icon->type);
            break;
        case PROP_BATTERY_STATE:
            g_value_set_enum(value,icon->state);
            break;
        case PROP_LAST_FULL:
            g_value_set_uint(value,icon->last_full);
            break;
        case PROP_BATT_CRITICAL:
            g_value_set_uint(value,icon->critical_level);
            break; 
#ifdef HAVE_LIBNOTIFY              
        case PROP_NOTIFY:
            g_value_set_boolean(value,icon->notify);
            break;
#endif            
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
            break;
    }                 
#ifdef DEBUG
    gchar *content;
    content = g_strdup_value_contents(value);
    XFPM_DEBUG("param:%s value contents:%s\n",pspec->name,content);
    g_free(content);
#endif     
    
}
                                        
static void
xfpm_battery_icon_finalize(GObject *object)
{
    XfpmBatteryIcon *icon;
    icon = XFPM_BATTERY_ICON(object);
    
    G_OBJECT_CLASS(xfpm_battery_icon_parent_class)->finalize(object);
}

static gboolean
xfpm_battery_icon_size_change_cb(XfpmBatteryIcon *battery_icon,gint size,gpointer data)
{
    XFPM_DEBUG("size change event %d\n",size);
    if ( size > 128 )
    {
        /* Reduce the size until we get the correct size of the system tray */
        size = 48;
    }
    
    gchar *icon_name;
    g_object_get(G_OBJECT(battery_icon),"icon-name",&icon_name,NULL);
    
    if ( !icon_name )
    {
        return FALSE;
    }
    
    GdkPixbuf *icon;
    icon = xfpm_load_icon(icon_name,size);
    
    if ( icon )
    {
        XFPM_DEBUG("Setting icon for resize request\n");
        gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(battery_icon),icon);
        g_object_unref(G_OBJECT(icon));
        return TRUE;
    } else
    {
        XFPM_DEBUG("Unable to load icon %s\n",icon_name);
    }
    return FALSE;
}

static const gchar *
xfpm_battery_icon_get_index(XfpmBatteryType type,guint percent)
{
	if (percent < 10) {
		return "000";
	} else if (percent < 30) {
	    return (type == MOUSE || type == KEYBOARD ? "030" : "020");
	} else if (percent < 50) {
	    return (type == MOUSE || type == KEYBOARD ? "030" : "040");
	} else if (percent < 70) {
		return "060";
	} else if (percent < 90) {
	    return (type == MOUSE || type == KEYBOARD ? "060" : "080");
	}
	return "100";
}

static gchar *
xfpm_battery_icon_get_icon_prefix(XfpmBatteryType type)
{
    if ( type == PRIMARY )
    {
        return g_strdup("gpm-primary");
    }
    else if ( type == UPS )
    {
        return g_strdup("gpm-ups");
    }
    else if ( type == KEYBOARD)
    {
        return g_strdup("gpm-keyboard");
    }
    else if ( type == MOUSE )
    {
        return g_strdup("gpm-mouse");
    }
    /* we are going to show a primary icon */
    return g_strdup("gpm-primary");    
}

static void
xfpm_battery_icon_update(XfpmBatteryIcon *battery_icon,XfpmBatteryState state,
                        guint level,gboolean adapter_present)
{
    XFPM_DEBUG(" start\n");
#ifdef HAVE_LIBNOTIFY    
    const gchar *message = NULL;
#endif    
    gchar *icon_name = NULL;
    gchar *icon_prefix = NULL;
    
    icon_prefix = xfpm_battery_icon_get_icon_prefix(battery_icon->type);
        
    switch(state)
    {
        case NOT_PRESENT:
            icon_name = g_strdup_printf("%s-missing",icon_prefix);
            break;
        case LOW:
            icon_name = g_strdup_printf("%s-%s",icon_prefix,
                            xfpm_battery_icon_get_index(battery_icon->type,level));
#ifdef HAVE_LIBNOTIFY 
            message = _("Your battery charge level is low");
#endif
            break;
        case CRITICAL:
            icon_name = g_strdup_printf("%s-%s",icon_prefix,
                            xfpm_battery_icon_get_index(battery_icon->type,level));
#ifdef HAVE_LIBNOTIFY 
            message = _("Your battery charge level is critical");
#endif
            break;    
        case DISCHARGING:
            icon_name = g_strdup_printf("%s-%s",icon_prefix,
                            xfpm_battery_icon_get_index(battery_icon->type,level));
#ifdef HAVE_LIBNOTIFY 
            if ( !adapter_present )
            {
                message = battery_icon->type == PRIMARY ? _("You are running on Battery"):_("Battery is discharging");
            }
            else
            {
                message = _("Your battery is discharging");
            }
#endif      
            break;
        case CHARGING:
            icon_name = g_strdup_printf("%s-%s-charging",icon_prefix,
                            xfpm_battery_icon_get_index(battery_icon->type,level));
#ifdef HAVE_LIBNOTIFY 
            message = _("Battery is charging");
#endif
        case NOT_FULL:
             icon_name = g_strdup_printf("%s-%s-charging",icon_prefix,
                            xfpm_battery_icon_get_index(battery_icon->type,level));
             break;                   
        case FULL:    
             icon_name = g_strdup_printf("%s-charged",icon_prefix);
#ifdef HAVE_LIBNOTIFY 
            message = _("Your battery is fully charged");
#endif
            break;
    }
    XFPM_DEBUG("icon %s\n",icon_name);
    
    if ( battery_icon->state != state && state != NOT_FULL ) {
#ifdef HAVE_LIBNOTIFY
    gboolean visible;
    g_object_get(G_OBJECT(battery_icon),"visible",&visible,NULL);
        if ( battery_icon->notify && visible ) 
        {
            if ( battery_icon->discard_notification )
            {
                battery_icon->discard_notification = FALSE;
            }
            else
            {
                NotifyNotification *n =
                xfpm_notify_new(_("Xfce power manager"),message,5000,NOTIFY_URGENCY_LOW,
                               GTK_STATUS_ICON(battery_icon),icon_name);
                xfpm_notify_show_notification(n,0);               
            }
        }
#endif     
    g_object_set(G_OBJECT(battery_icon),"battery-state",state,NULL);
    }

    if ( icon_prefix && icon_name )
    {
        if ( battery_icon->icon == g_quark_from_string(icon_name ) && 
             battery_icon->icon_loaded )
        {
            g_free(icon_prefix);
            g_free(icon_name);
            return;
        }
        gint size;    
        GdkPixbuf *icon;
        g_object_get(G_OBJECT(battery_icon),"size",&size,NULL);
        icon = xfpm_load_icon(icon_name,size);
        battery_icon->icon = g_quark_from_string(icon_name);
        g_free(icon_prefix);
        g_free(icon_name);
        
        if ( icon )
        {
            battery_icon->icon_loaded = TRUE;
            gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(battery_icon),icon);
            g_object_unref(G_OBJECT(icon));
        }
        else
        {
            battery_icon->icon_loaded = FALSE;
        } 
     }   
}

GtkStatusIcon *
xfpm_battery_icon_new(guint32 last_full,XfpmBatteryType battery_type,
                      guint critical_level,gboolean visible)
{
    XfpmBatteryIcon *battery_icon = NULL;
    battery_icon = g_object_new(XFPM_TYPE_BATTERY_ICON,
                            "battery-type",
                            battery_type,
                            "last-full",
                            last_full,
                            "batt-critical-level",
                            critical_level,
                            "visible",
                            visible,
                            NULL);
                            
    return GTK_STATUS_ICON(battery_icon);
}

void
xfpm_battery_icon_set_state(XfpmBatteryIcon *battery_icon,guint32 charge,guint per,
                            guint32 remaining_time,gboolean present,
                            gboolean is_charging,gboolean is_discharging,
                            gboolean ac_adapter_present)
{
    g_return_if_fail(XFPM_IS_BATTERY_ICON(battery_icon));
    battery_icon->battery_present = ac_adapter_present;
    guint critical_level = battery_icon->type == PRIMARY || UPS ? battery_icon->critical_level : 5;
#ifdef DEBUG
    gchar *content_is_charging;
    gchar *content_is_discharging;
    gchar *content_present;
    
    GValue value = { 0, };
    g_value_init(&value,G_TYPE_BOOLEAN);
    g_value_set_boolean(&value,is_charging);
    content_is_charging = g_strdup_value_contents(&value);
    
    g_value_set_boolean(&value,is_discharging);
    content_is_discharging = g_strdup_value_contents(&value);
    
    g_value_set_boolean(&value,present);
    content_present = g_strdup_value_contents(&value);
        
    XFPM_DEBUG("Setting battery state charge=%d is_charging:%s is_discharging:%s present:%s \n",charge,
    content_is_charging,content_is_discharging,content_present);
    g_free(content_is_charging);
    g_free(content_is_discharging);
    g_free(content_present);
#endif    
    guint remaining_per;
    if ( per > 0 )
    {
        remaining_per =  per;
    }
    else
    {
        remaining_per = 1;
    }
    
    gchar tip[256];
    
    if ( present == FALSE )
    {
        xfpm_battery_icon_update(battery_icon,NOT_PRESENT,remaining_per,ac_adapter_present);
        sprintf(tip,"%s",_("Not present"));
        gtk_status_icon_set_tooltip(GTK_STATUS_ICON(battery_icon),tip);
        return;
    }
    
    sprintf(tip,"%i",remaining_per);
    strcat(tip,_("%"));
	strcat(tip," ");
    
    // battery is full
    if ( is_charging == FALSE && is_discharging == FALSE && battery_icon->last_full == charge )
    {
        strcat(tip,_("Battery fully charged"));
        xfpm_battery_icon_update(battery_icon,FULL,remaining_per,ac_adapter_present);
    }
    
    // battery is not dis/charging but is not full also
    if ( is_charging == FALSE && is_discharging == FALSE && battery_icon->last_full != charge )
    {
        strcat(tip,_("Battery charge level"));
        xfpm_battery_icon_update(battery_icon,NOT_FULL,remaining_per,ac_adapter_present);
    }
    
    //battery is charging
    if ( is_charging == TRUE && is_discharging == FALSE )
    {
        strcat(tip,_("Battery is charging "));
        xfpm_battery_icon_update(battery_icon,CHARGING,remaining_per,ac_adapter_present);
    }
    
    // battery is discharging
    if ( is_charging == FALSE && is_discharging == TRUE )
    {
        if ( remaining_per > ( critical_level + 10 ) )
        {
            if ( !ac_adapter_present )
            {
                strcat(tip,
                battery_icon->type == PRIMARY ? _("Running on battery"): _("Battery is discharging"));
            }
            else
            {
                strcat(tip,_("Battery is discharging"));
            }
            xfpm_battery_icon_update(battery_icon,DISCHARGING,remaining_per,ac_adapter_present);
        } 
        else if ( remaining_per <= ( critical_level+10 ) && remaining_per > critical_level )
        {
            strcat(tip,_("Battery charge level is low"));
            xfpm_battery_icon_update(battery_icon,LOW,20,ac_adapter_present);
        }
        else if ( remaining_per <= critical_level )
        {
            strcat(tip,_("Battery charge level is critical"));
            xfpm_battery_icon_update(battery_icon,CRITICAL,0,ac_adapter_present);
        }
    }
    
    if ( remaining_time != 0 )
    {
        gchar *time_str;
        gchar *est_time = NULL;
        
        gint minutes,hours,minutes_left;
       	hours = remaining_time / 3600;
		minutes = remaining_time / 60;
		minutes_left = minutes % 60;
		
		if ( battery_icon->state == DISCHARGING || 
		     battery_icon->state == LOW         || 
		     battery_icon->state == CRITICAL )
        {
            est_time = g_strdup(_("\nEstimated time left"));
           
        }
        else if ( battery_icon->state == CHARGING )
        {
            est_time = g_strdup(_("\nEstimated time to charge"));
           
        }
        time_str = g_strdup_printf("%s: %d %s %d %s",est_time,
                                        hours,hours > 1 ? _("hours") : _("hour") ,
                                        minutes_left, minutes_left > 1 ? _("minutes") : _("minute"));
                                        
		strcat(tip,time_str);
		g_free(time_str);
		g_free(est_time);
    }
    
    gtk_status_icon_set_tooltip(GTK_STATUS_ICON(battery_icon),tip);
    
}
