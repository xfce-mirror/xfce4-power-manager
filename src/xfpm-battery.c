/*
 * * Copyright (C) 2008 Ali <aliov@xfce.org>
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
#include <gtk/gtk.h>

#include <glib.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/xfpm-string.h"

#include "xfpm-battery.h"
#include "xfpm-tray-icon.h"
#include "xfpm-string.h"
#include "xfpm-marshal.h"
#include "xfpm-enum-types.h"
#include "xfpm-battery-info.h"
#include "xfpm-xfconf.h"
#include "xfpm-config.h"
#include "xfpm-notify.h"
#include "xfpm-adapter.h"

/* Init */
static void xfpm_battery_class_init (XfpmBatteryClass *klass);
static void xfpm_battery_init       (XfpmBattery *battery);
static void xfpm_battery_finalize   (GObject *object);

#define XFPM_BATTERY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_BATTERY, XfpmBatteryPrivate))

struct XfpmBatteryPrivate
{
    XfpmTrayIcon    *icon;
    XfpmAdapter     *adapter;
    HalBattery      *device;
    XfpmXfconf      *conf;

    HalDeviceType    type;
    gchar 	    *icon_prefix;
    
    gboolean         adapter_present;
    
    XfpmShowIcon     show_icon;
    XfpmBatteryState state;
    guint            critical_level;
};

enum
{
    BATTERY_STATE_CHANGED,
    POPUP_BATTERY_MENU,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmBattery, xfpm_battery, G_TYPE_OBJECT)

static const gchar *
xfpm_battery_get_icon_index (HalDeviceType type, guint percent)
{
    if (percent < 10) {
	return "000";
    } else if (percent < 30) {
	return (type == HAL_DEVICE_TYPE_PRIMARY || type == HAL_DEVICE_TYPE_UPS ? "020" : "030");
    } else if (percent < 50) {
	return (type == HAL_DEVICE_TYPE_PRIMARY || type == HAL_DEVICE_TYPE_UPS ? "040" : "030");
    } else if (percent < 70) {
	return "060";
    } else if (percent < 90) {
	return (type == HAL_DEVICE_TYPE_PRIMARY || type == HAL_DEVICE_TYPE_UPS ? "080" : "060");
    }
    return "100";
}

static void
xfpm_battery_refresh_visible_icon (XfpmBattery *battery)
{
    gboolean visible = FALSE;
    
    if ( battery->priv->show_icon == SHOW_ICON_ALWAYS )
    	visible = TRUE;
    else if ( battery->priv->show_icon == SHOW_ICON_WHEN_BATTERY_PRESENT )
    {
	if ( battery->priv->state == BATTERY_NOT_PRESENT )
	    visible = FALSE;
	else visible = TRUE;
    }
    else if ( battery->priv->show_icon == SHOW_ICON_WHEN_BATTERY_CHARGING_DISCHARGING )
    {
	if ( battery->priv->state == BATTERY_FULLY_CHARGED )
	    visible = FALSE;
	else visible = TRUE;
    }

    xfpm_tray_icon_set_visible (battery->priv->icon, TRUE);
}
    
static void
xfpm_battery_refresh_icon (XfpmBattery *battery, 
			   gboolean is_present,
			   gboolean is_charging, 
			   gboolean is_discharging,
			   guint percentage
			   )
{
    if ( !is_present )
    {
	xfpm_tray_icon_set_icon (battery->priv->icon, 
				 battery->priv->type == HAL_DEVICE_TYPE_UPS ? "gpm-ups-missing" : "gpm-primary-missing");
	return;
    }
    /* Battery full */
    if ( !is_charging && !is_discharging )
    {
	if ( battery->priv->type == HAL_DEVICE_TYPE_PRIMARY)
	    xfpm_tray_icon_set_icon (battery->priv->icon,
				     "gpm-primary-charged");
	else
	{
	    gchar *icon = g_strdup_printf("%s%s", 
	    			          battery->priv->icon_prefix, 
	    			          xfpm_battery_get_icon_index(battery->priv->type, percentage));
	    xfpm_tray_icon_set_icon (battery->priv->icon, icon);
	    g_free(icon);
	}
	return;
    }
    
    if ( is_charging )
    {
	gchar *icon = g_strdup_printf("%s%s-%s",
				      battery->priv->icon_prefix, 
	    			      xfpm_battery_get_icon_index(battery->priv->type, percentage),
				      "charging");
	xfpm_tray_icon_set_icon (battery->priv->icon, icon);
	g_free(icon);
	return;
    }
    
    if ( is_discharging )
    {
	gchar *icon = g_strdup_printf("%s%s",
				      battery->priv->icon_prefix, 
	    			      xfpm_battery_get_icon_index(battery->priv->type, percentage));
	xfpm_tray_icon_set_icon (battery->priv->icon, icon);
	g_free(icon);
	return;
    }
}

static void
xfpm_battery_refresh_state (XfpmBattery *battery, XfpmBatteryState state)
{
    if ( battery->priv->state != state)
    {
	battery->priv->state = state;
	g_signal_emit (G_OBJECT(battery), signals[BATTERY_STATE_CHANGED], 0, state);
	TRACE("Emitting signal battery state changed");
    }
}

const gchar *
_get_battery_name (HalDeviceType type)
{
    if ( type ==  HAL_DEVICE_TYPE_UPS)
	return _("Your UPS");
    else if ( type == HAL_DEVICE_TYPE_MOUSE )
	return _("Your Mouse battery");
    else if ( type == HAL_DEVICE_TYPE_KEYBOARD )
	return _("Your Keyboard battery");
    else if ( type ==  HAL_DEVICE_TYPE_CAMERA )
	return _("Your Camera battery");
    else if ( type == HAL_DEVICE_TYPE_PDA)
	return _("Your PDA battery");
	
    return _("Your Battery");
}

const gchar *
xfpm_battery_get_battery_state (XfpmBatteryState *state, 
				gboolean is_charging, 
				gboolean is_discharging,
				guint32 last_full, 
				guint32 current_charge, 
				guint percentage,
				guint8 critical_level)
{
    if ( !is_charging && !is_discharging && last_full == current_charge )
    {
	*state = BATTERY_FULLY_CHARGED;
	return _("is fully charged");
    }
    else if ( !is_charging && !is_discharging && last_full != current_charge )
    {
	*state = BATTERY_NOT_FULLY_CHARGED;
	return _("charge level");
    }
    else if ( is_charging && !is_discharging )
    {
	*state = BATTERY_IS_CHARGING;
	return  _("is charging");
    }
    else if ( !is_charging && is_discharging )
    {
	if ( percentage >= 30 )
	{
	    *state = BATTERY_IS_DISCHARGING;
	    return  _("is discharging");
	}
	else if ( percentage >= critical_level && percentage < 30)
	{
	    *state = BATTERY_CHARGE_LOW;
	    return  _("charge is low");
	}
	else if ( percentage < critical_level )
	{
	    *state = BATTERY_CHARGE_CRITICAL;
	    return _("is almost empty");
    	}
    }
    return "";
}

static void
xfpm_battery_refresh_tooltip_misc (XfpmBattery *battery, gboolean is_present, 
				   gboolean is_charging, gboolean is_discharging,
				   guint32 last_full, guint32 current_charge,
				   guint percentage, guint time)
{
    gchar *tip;
    if ( !is_present )
    {
	tip = g_strdup_printf ("%s %s", _get_battery_name(battery->priv->type), _("is not present"));
	xfpm_tray_icon_set_tooltip (battery->priv->icon, tip);
	g_free(tip);
	return;
    }
    
    XfpmBatteryState state = battery->priv->state;
    const gchar *str = xfpm_battery_get_battery_state (&state, is_charging, is_discharging,
 						       last_full, current_charge, percentage, 
						       battery->priv->critical_level);
    tip = g_strdup_printf("%i%% %s %s", percentage, _get_battery_name(battery->priv->type), str);
    //FIXME: Time for misc batteries
    xfpm_tray_icon_set_tooltip (battery->priv->icon, tip);
    g_free (tip);
    xfpm_battery_refresh_state (battery, state);
}

static void
xfpm_battery_refresh_tooltip_primary (XfpmBattery *battery, gboolean is_present, 
				      gboolean is_charging, gboolean is_discharging,
				      guint32 last_full, guint32 current_charge,
				      guint percentage, guint time)
{
    gchar *tip;
    const gchar *str;
    XfpmBatteryState state = battery->priv->state;
    
    if ( !is_present )
    {
	xfpm_tray_icon_set_tooltip(battery->priv->icon, _("Battery not present"));
	return;
    }

    str = xfpm_battery_get_battery_state (&state, is_charging, is_discharging,
					  last_full, current_charge, percentage, battery->priv->critical_level);
    
    if ( time != 0  && time <= 28800 /* 8 hours */ && 
	 state != BATTERY_FULLY_CHARGED && state != BATTERY_NOT_FULLY_CHARGED )
    {
	gchar *time_str;
        const gchar *est_time;
	        
        gint minutes, hours, minutes_left;
       	hours = time / 3600;
		minutes = time / 60;
		minutes_left = minutes % 60;
		
	if ( state == BATTERY_IS_DISCHARGING || 
	     state == BATTERY_CHARGE_LOW         || 
	     state == BATTERY_CHARGE_CRITICAL )
        {
            est_time = _("Estimated time left");
        }
        else if ( state == BATTERY_IS_CHARGING )
        {
            est_time = _("Estimated time to be fully charged");
        }
        time_str = g_strdup_printf("%s: %d %s %d %s",est_time,
                                   hours,hours > 1 ? _("hours") : _("hour") ,
                                   minutes_left, minutes_left > 1 ? _("minutes") : _("minute"));
				   
	tip = g_strdup_printf ("%i%% %s %s\n%s\n%s", 
			       percentage, 
			       _("Battery"),
			       str,
			       battery->priv->adapter_present ? 
			       _("System is running on AC power") :
			       _("System is running on battery power"),
			       time_str);
	g_free (time_str);
    }
    else
    {
	 tip = g_strdup_printf ("%i%% %s %s\n%s", 
			   percentage, 
			   _("Battery"),
			   str, 
			   battery->priv->adapter_present ? 
			   _("System is running on AC power") :
			   _("System is running on battery power"));
    }

    xfpm_tray_icon_set_tooltip(battery->priv->icon, tip);
    g_free(tip);
    xfpm_battery_refresh_state (battery, state);
}

static void
xfpm_battery_refresh (XfpmBattery *battery)
{
    gboolean is_present, is_charging, is_discharging = FALSE;
    guint percentage = 0;
    guint32 last_full, current_charge = 0;
    guint time = 0;
    
    g_object_get (G_OBJECT(battery->priv->device), 
    		  "is-present", &is_present,
		  "is-charging", &is_charging,
		  "is-discharging", &is_discharging, 
		  "percentage", &percentage,
		  "last-full", &last_full,
		  "current-charge", &current_charge,
		  "time", &time,
		  NULL);
    
    xfpm_battery_refresh_icon (battery, is_present, is_charging, is_discharging, percentage);
    battery->priv->type == HAL_DEVICE_TYPE_PRIMARY ?
			   xfpm_battery_refresh_tooltip_primary (battery, is_present, 
								 is_charging, is_discharging, 
								 last_full, current_charge,
								 percentage, time)
  					           :
			    xfpm_battery_refresh_tooltip_misc   (battery, is_present, 
								 is_charging, is_discharging, 
								 last_full, current_charge,
								 percentage, time);
}

static void
xfpm_battery_device_changed_cb (HalBattery *device, XfpmBattery *battery)
{
    TRACE("start");
    xfpm_battery_refresh (battery);
    xfpm_battery_refresh_visible_icon (battery);
}

static gchar *
_get_icon_prefix_from_enum_type (HalDeviceType type)
{
    if ( type == HAL_DEVICE_TYPE_PRIMARY )
    {
	return g_strdup("gpm-primary-");
    }
    else if ( type == HAL_DEVICE_TYPE_UPS ) 
    {
	return g_strdup("gpm-ups-");
    }
    else if ( type == HAL_DEVICE_TYPE_MOUSE ) 
    {
	return g_strdup("gpm-mouse-");
    }
    else if ( type == HAL_DEVICE_TYPE_KEYBOARD ) 
    {
	return g_strdup("gpm-keyboard-");
    }
    else if ( type == HAL_DEVICE_TYPE_CAMERA ) 
    {
	return g_strdup("gpm-camera-");
    }
    else if ( type == HAL_DEVICE_TYPE_PDA ) 
    {
	return g_strdup("gpm-pda-");
    }
    else if ( type == HAL_DEVICE_TYPE_KEYBOARD_MOUSE ) 
    {
	return g_strdup("gpm-keyboard-mouse-");
    }
    
    return g_strdup("gpm-primary-");
}

static void
xfpm_battery_popup_menu_cb (GtkStatusIcon *icon, guint button, guint activate_time, XfpmBattery *battery)
{
    g_signal_emit (G_OBJECT(battery), signals[POPUP_BATTERY_MENU], 0,
    		   icon, button, activate_time, battery->priv->type);
}

static void
xfpm_battery_adapter_changed_cb (XfpmAdapter *adapter, gboolean present, XfpmBattery *battery)
{
    battery->priv->adapter_present = present;
}

static void
xfpm_battery_property_changed_cb (XfconfChannel *channel, gchar *property,
				  GValue *value, XfpmBattery *battery)
{
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
    	return;
	
    if ( xfpm_strequal (property, SHOW_TRAY_ICON_CFG) )
    {
	guint val = g_value_get_uint (value);
	battery->priv->show_icon = val;
	xfpm_battery_refresh_visible_icon (battery);
    }
    else if ( xfpm_strequal( property, CRITICAL_POWER_LEVEL) )
    {
	guint val = g_value_get_uint (value);
	if ( val > 20 )
	{
	    g_warning ("Value %d for property %s is out of range \n", val, CRITICAL_POWER_LEVEL);
	    battery->priv->critical_level = 10;
	}
	else 
	    battery->priv->critical_level = val;
    }
}

static void
xfpm_battery_load_configuration (XfpmBattery *battery)
{
    battery->priv->show_icon =
    	xfconf_channel_get_uint (battery->priv->conf->channel, SHOW_TRAY_ICON_CFG, 0);
	
    if ( battery->priv->show_icon < 0 || battery->priv->show_icon > 3 )
    {
	g_warning ("Invalid value %d for property %s, using default\n", battery->priv->show_icon, SHOW_TRAY_ICON_CFG);
	xfconf_channel_set_uint (battery->priv->conf->channel, CRITICAL_BATT_ACTION_CFG, 0);
    }
    
    battery->priv->critical_level =
	xfconf_channel_get_uint (battery->priv->conf->channel, CRITICAL_POWER_LEVEL, 0);
	
    if ( battery->priv->critical_level <0 || battery->priv->critical_level > 20 )
    {
	g_warning ("Value %d for property %s is out of range \n", battery->priv->critical_level, CRITICAL_POWER_LEVEL);
	battery->priv->critical_level = 10;
    }
}

static void
xfpm_battery_class_init(XfpmBatteryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[BATTERY_STATE_CHANGED] = 
    	g_signal_new("battery-state-changed",
		      XFPM_TYPE_BATTERY,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET(XfpmBatteryClass, battery_state_changed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__ENUM,
		      G_TYPE_NONE, 1, XFPM_TYPE_BATTERY_STATE);

    signals[POPUP_BATTERY_MENU] = 
   	g_signal_new("popup-battery-menu",
		      XFPM_TYPE_BATTERY,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET(XfpmBatteryClass, popup_battery_menu),
		      NULL, NULL,
		      _xfpm_marshal_VOID__POINTER_UINT_UINT_UINT,
		      G_TYPE_NONE, 4, 
		      GTK_TYPE_STATUS_ICON, 
		      G_TYPE_UINT,
		      G_TYPE_UINT,
		      G_TYPE_UINT);
		      
    object_class->finalize = xfpm_battery_finalize;
    
    g_type_class_add_private(klass,sizeof(XfpmBatteryPrivate));
}

static void
xfpm_battery_init(XfpmBattery *battery)
{
    battery->priv = XFPM_BATTERY_GET_PRIVATE(battery);
    
    battery->priv->icon      = xfpm_tray_icon_new ();
    battery->priv->show_icon = SHOW_ICON_ALWAYS;
    battery->priv->adapter   = xfpm_adapter_new ();
    battery->priv->conf      = xfpm_xfconf_new ();
    
    battery->priv->adapter_present = xfpm_adapter_get_present (battery->priv->adapter);
    
    g_signal_connect (battery->priv->adapter ,"adapter-changed",
		      G_CALLBACK(xfpm_battery_adapter_changed_cb), battery);
		      
    g_signal_connect (battery->priv->conf->channel ,"property-changed",
		      G_CALLBACK(xfpm_battery_property_changed_cb), battery);
		      
    xfpm_battery_load_configuration (battery);
}

static void
xfpm_battery_finalize(GObject *object)
{
    XfpmBattery *battery;
    battery = XFPM_BATTERY(object);
    
    g_object_unref (battery->priv->icon);
    
    g_object_unref (battery->priv->device);
	
    if ( battery->priv->icon_prefix )
    	g_free(battery->priv->icon_prefix);
	
    g_object_unref (battery->priv->adapter);
    
    g_object_unref (battery->priv->conf);

    G_OBJECT_CLASS(xfpm_battery_parent_class)->finalize(object);
}

XfpmBattery *
xfpm_battery_new(const HalBattery *device)
{
    XfpmBattery *battery = NULL;
    
    battery = g_object_new(XFPM_TYPE_BATTERY, NULL);
    
    battery->priv->device = (HalBattery *)g_object_ref(G_OBJECT(device));
    
    g_object_get(G_OBJECT(battery->priv->device), "type", &battery->priv->type, NULL);
    
    battery->priv->icon_prefix = _get_icon_prefix_from_enum_type(battery->priv->type);
    
    xfpm_battery_refresh (battery);
    
    g_signal_connect (G_OBJECT(battery->priv->device), "battery-changed",
		      G_CALLBACK(xfpm_battery_device_changed_cb), battery);
		      
    g_signal_connect (G_OBJECT(xfpm_tray_icon_get_tray_icon(battery->priv->icon)), "popup-menu",
		      G_CALLBACK(xfpm_battery_popup_menu_cb), battery);
    
    return battery;
}

void xfpm_battery_set_show_icon (XfpmBattery *battery, XfpmShowIcon show_icon)
{
    g_return_if_fail (XFPM_IS_BATTERY(battery));
    
    battery->priv->show_icon = show_icon;
    
    xfpm_battery_refresh_visible_icon (battery);
}

const HalBattery*
xfpm_battery_get_device (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY(battery), NULL);
    
    return battery->priv->device;
}

//FIXME: default g_return value
XfpmBatteryState xfpm_battery_get_state (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY(battery), 0);
    
    return battery->priv->state;
}

GtkStatusIcon  *xfpm_battery_get_status_icon    (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY(battery), NULL);
    
    return xfpm_tray_icon_get_tray_icon (battery->priv->icon);
    
}

const gchar *xfpm_battery_get_icon_name (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY (battery), NULL);
    
    return xfpm_tray_icon_get_icon_name (battery->priv->icon);
}
    
void xfpm_battery_show_info (XfpmBattery *battery)
{
    g_return_if_fail (XFPM_IS_BATTERY(battery));
    
    gchar *icon = g_strdup_printf("%s%s",
				  battery->priv->icon_prefix, 
	    			  xfpm_battery_get_icon_index(battery->priv->type, 100));
				      
    GtkWidget *info = xfpm_battery_info_new (battery->priv->device, icon);
    
    g_free (icon);
    
    gtk_widget_show_all (info);
}

void xfpm_battery_set_critical_level (XfpmBattery *battery, guint8 critical_level)
{
    g_return_if_fail (XFPM_IS_BATTERY(battery));
    
    battery->priv->critical_level = critical_level;
}
