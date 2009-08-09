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
#include "libxfpm/xfpm-notify.h"

#include "xfpm-battery.h"
#include "xfpm-tray-icon.h"
#include "xfpm-string.h"
#include "xfpm-marshal.h"
#include "xfpm-enum-types.h"
#include "xfpm-enum.h"
#include "xfpm-battery-info.h"
#include "xfpm-xfconf.h"
#include "xfpm-config.h"
#include "xfpm-adapter.h"
#include "xfpm-debug.h"

static void xfpm_battery_finalize   (GObject *object);

#define XFPM_BATTERY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_BATTERY, XfpmBatteryPrivate))

struct XfpmBatteryPrivate
{
    XfpmTrayIcon    *icon;
    XfpmAdapter     *adapter;
    HalBattery      *device;
    XfpmXfconf      *conf;
    XfpmNotify      *notify;

    HalDeviceType    type;
    gchar 	    *icon_prefix;
    
    gboolean         adapter_present;
    XfpmBatteryState state;
    
    gulong	     sig_1;
    gulong           sig_2;
    gulong           sig_3;
};

enum
{
    BATTERY_STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmBattery, xfpm_battery, G_TYPE_OBJECT)

static const gchar * G_GNUC_CONST
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

static const gchar * G_GNUC_CONST
xfpm_battery_get_message_from_battery_state (XfpmBatteryState state, gboolean adapter_present)
{
    switch (state)
    {
	case BATTERY_FULLY_CHARGED:
	    return _("Your battery is fully charged");
	    break;
	case BATTERY_NOT_FULLY_CHARGED:
	    return _("Your battery is charging");
	    break;
	case BATTERY_IS_CHARGING:
	    return  _("Battery is charging");
	    break;
	case BATTERY_IS_DISCHARGING:
	    return  adapter_present ? _("Your battery is discharging"): _("System is running on battery power");
	    break;
	case BATTERY_CHARGE_LOW:
	    return adapter_present ? _("Your battery charge is low") : _("System is running on low power"); 
	    break;
	default:
	    return NULL;
    }
}

static void
xfpm_battery_refresh_visible_icon (XfpmBattery *battery)
{
    XfpmShowIcon show_icon;
    gboolean visible = TRUE;
    
    g_object_get (G_OBJECT (battery->priv->conf),
		  SHOW_TRAY_ICON_CFG, &show_icon,
		  NULL);
		  
    if ( show_icon == SHOW_ICON_ALWAYS )
    	visible = TRUE;
    else if ( show_icon == NEVER_SHOW_ICON )
	visible = FALSE;
    else if ( show_icon == SHOW_ICON_WHEN_BATTERY_PRESENT )
    {
	if ( battery->priv->state == BATTERY_NOT_PRESENT )
	    visible = FALSE;
	else visible = TRUE;
    }
    else if ( show_icon == SHOW_ICON_WHEN_BATTERY_CHARGING_DISCHARGING )
    {
	if ( battery->priv->state == BATTERY_FULLY_CHARGED )
	    visible = FALSE;
	else visible = TRUE;
    }

    xfpm_tray_icon_set_visible (battery->priv->icon, visible);
}
    
static void
xfpm_battery_refresh_icon (XfpmBattery *battery, 
			   XfpmBatteryState state,
			   guint percentage)
{
    gchar *icon;

    if ( state == BATTERY_NOT_PRESENT )
    {
	xfpm_tray_icon_set_icon (battery->priv->icon, 
				 battery->priv->type == HAL_DEVICE_TYPE_UPS ? "xfpm-ups-missing" : "xfpm-primary-missing");
	return;
    }
    
    /* Battery full */
    if ( state == BATTERY_FULLY_CHARGED )
    {
	if ( battery->priv->type == HAL_DEVICE_TYPE_PRIMARY)
	    xfpm_tray_icon_set_icon (battery->priv->icon, battery->priv->adapter_present ? "xfpm-primary-charged" : "xfpm-primary-100");
	else
	{
	    icon = g_strdup_printf("%s%s", 
	    		           battery->priv->icon_prefix, 
	    			   xfpm_battery_get_icon_index (battery->priv->type, percentage));
	    xfpm_tray_icon_set_icon (battery->priv->icon, icon);
	    g_free(icon);
	}
    }
    else if ( state == BATTERY_NOT_FULLY_CHARGED )
    {
	if ( battery->priv->adapter_present )
	{
	    icon = g_strdup_printf("%s%s-%s",
			      battery->priv->icon_prefix, 
			      xfpm_battery_get_icon_index (battery->priv->type, percentage),
			      "charging");
	}
	else
	{
	    icon = g_strdup_printf("%s%s",
			      battery->priv->icon_prefix, 
			      xfpm_battery_get_icon_index (battery->priv->type, percentage));
	}
	xfpm_tray_icon_set_icon (battery->priv->icon, icon);
	g_free(icon);
	
    }
    else if ( state == BATTERY_IS_CHARGING )
    {
	icon = g_strdup_printf("%s%s-%s",
			      battery->priv->icon_prefix, 
			      xfpm_battery_get_icon_index (battery->priv->type, percentage),
			      "charging");
				      
	xfpm_tray_icon_set_icon (battery->priv->icon, icon);
	g_free(icon);
    }
    else if ( state == BATTERY_IS_DISCHARGING || state == BATTERY_CHARGE_CRITICAL ||
	      state == BATTERY_CHARGE_LOW )
    {
	icon = g_strdup_printf("%s%s",
			      battery->priv->icon_prefix, 
			      xfpm_battery_get_icon_index (battery->priv->type, percentage));
				      
	xfpm_tray_icon_set_icon (battery->priv->icon, icon);
	g_free(icon);
	
    }
}

static gboolean
xfpm_battery_notify_idle (gpointer data)
{
    XfpmBattery *battery;
    const gchar *message;
    
    battery = XFPM_BATTERY (data);
    
    message = xfpm_battery_get_message_from_battery_state (battery->priv->state, battery->priv->adapter_present);
    if ( !message )
	return FALSE;
	
    xfpm_notify_show_notification (battery->priv->notify, 
				    _("Xfce power manager"), 
				    message, 
				    xfpm_tray_icon_get_icon_name (battery->priv->icon),
				    8000,
				    battery->priv->type == HAL_DEVICE_TYPE_PRIMARY ? FALSE : TRUE,
				    XFPM_NOTIFY_NORMAL,
				    xfpm_tray_icon_get_tray_icon(battery->priv->icon));
    
    return FALSE;
}

static void
xfpm_battery_notify (XfpmBattery *battery)
{
    gboolean notify;
    static gboolean starting_up = TRUE;

    if ( starting_up )
    {
	starting_up = FALSE;
	return;
    }

    g_object_get (G_OBJECT (battery->priv->conf),
		  GENERAL_NOTIFICATION_CFG, &notify,
		  NULL);
		  
    if ( notify )
    {
	g_idle_add ((GSourceFunc) xfpm_battery_notify_idle, battery);
    }
}

static const gchar * G_GNUC_CONST
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

static const gchar * G_GNUC_PURE
xfpm_battery_get_battery_state (XfpmBatteryState *state, 
				gboolean is_charging, 
				gboolean is_discharging,
				guint32 last_full, 
				guint32 current_charge, 
				guint percentage,
				guint8 critical_level)
{
    if ( G_UNLIKELY (current_charge == 0 || percentage == 0) )
    {
	*state = BATTERY_CHARGE_CRITICAL;
	return _("is empty");
    }
    
    if ( !is_charging && !is_discharging &&  current_charge >= last_full )
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
    
    g_warn_if_reached ();
    
    return "";
}

static void
xfpm_battery_refresh_common (XfpmBattery *battery, guint percentage, XfpmBatteryState state)
{
    XfpmShowIcon show_icon;
    
    g_object_get (G_OBJECT (battery->priv->conf),
		  SHOW_TRAY_ICON_CFG, &show_icon,
		  NULL);

    if ( show_icon != NEVER_SHOW_ICON )
	xfpm_battery_refresh_icon (battery, state, percentage);
    
    if ( battery->priv->state != state)
    {
	battery->priv->state = state;
	XFPM_DEBUG_ENUM ("battery state change", battery->priv->state, XFPM_TYPE_BATTERY_STATE);
	TRACE("Emitting signal battery state changed");
	g_signal_emit (G_OBJECT(battery), signals[BATTERY_STATE_CHANGED], 0, state);
	
	if ( battery->priv->state != BATTERY_NOT_FULLY_CHARGED && show_icon != NEVER_SHOW_ICON)
	    xfpm_battery_notify (battery);
	else
	    xfpm_notify_close_normal (battery->priv->notify);
    }
}

static void
xfpm_battery_refresh_misc (XfpmBattery *battery, gboolean is_present, 
			   gboolean is_charging, gboolean is_discharging,
			   guint32 last_full, guint32 current_charge,
			   guint percentage, guint time_per)
{
    gchar *tip;
    XfpmBatteryState state;
    const gchar *str;
    guint critical_level;
    
    g_object_get (G_OBJECT (battery->priv->conf),
		  CRITICAL_POWER_LEVEL, &critical_level,
		  NULL);
    
    if ( !is_present )
    {
	tip = g_strdup_printf ("%s %s", _get_battery_name(battery->priv->type), _("is not present"));
	xfpm_tray_icon_set_tooltip (battery->priv->icon, tip);
	g_free(tip);
	battery->priv->state = BATTERY_NOT_PRESENT;
	return;
    }
    
    state = battery->priv->state;
    str = xfpm_battery_get_battery_state (&state, is_charging, is_discharging,
    				          last_full, current_charge, percentage, 
					  critical_level);
    tip = g_strdup_printf("%i%% %s %s", percentage, _get_battery_name(battery->priv->type), str);
    //FIXME: Time for misc batteries
    xfpm_tray_icon_set_tooltip (battery->priv->icon, tip);
    g_free (tip);
    
    xfpm_battery_refresh_common (battery, percentage, state);
}

static void
xfpm_battery_refresh_primary (XfpmBattery *battery, gboolean is_present, 
			      gboolean is_charging, gboolean is_discharging,
			      guint32 last_full, guint32 current_charge,
			      guint percentage, guint time_per)
{
    gchar *tip;
    const gchar *str;
    guint critical_level;

    XfpmBatteryState state = battery->priv->state;
    
    g_object_get (G_OBJECT (battery->priv->conf),
		  CRITICAL_POWER_LEVEL, &critical_level,
		  NULL);
    
    TRACE ("Start");
    
    if ( !is_present )
    {
	xfpm_tray_icon_set_tooltip(battery->priv->icon, _("Battery not present"));
	battery->priv->state = BATTERY_NOT_PRESENT;
	return;
    }

    str = xfpm_battery_get_battery_state (&state, is_charging, is_discharging,
					  last_full, current_charge, percentage, critical_level);
    
    XFPM_DEBUG_ENUM ("battery state", state, XFPM_TYPE_BATTERY_STATE);
    
    if ( time_per != 0  && time_per <= 28800 /* 8 hours */ && 
	 state != BATTERY_FULLY_CHARGED && state != BATTERY_NOT_FULLY_CHARGED )
    {
	gchar *time_str;
        gchar *tip_no_time;
	const gchar *est_time;
	        
        gint minutes, hours, minutes_left;
       	hours = time_per / 3600;
		minutes = time_per / 60;
		minutes_left = minutes % 60;

	tip_no_time = g_strdup_printf ("%i%% %s %s\n%s", 
			       percentage, 
			       _("Battery"),
			       str,
			       battery->priv->adapter_present ? 
			       _("System is running on AC power") :
			       _("System is running on battery power"));

	if ( state == BATTERY_IS_DISCHARGING || 
	     state == BATTERY_CHARGE_LOW         || 
	     state == BATTERY_CHARGE_CRITICAL )
        {
            est_time = _("Estimated time left");
        }
        else //* BATTERY_IS_CHARGING
        {
            est_time = _("Estimated time to be fully charged");
        }

        time_str = g_strdup_printf("%s: %d %s %d %s",est_time,
                                   hours,hours > 1 ? _("hours") : _("hour") ,
                                   minutes_left, minutes_left > 1 ? _("minutes") : _("minute"));

	tip = (hours != 0 || minutes_left != 0 ) ? 
	     g_strdup_printf ("%s\n%s", tip_no_time, time_str) :
	     g_strdup (tip_no_time);
	     
	g_free (tip_no_time);
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

    xfpm_battery_refresh_common (battery, percentage, state);
}

static void
xfpm_battery_refresh (XfpmBattery *battery)
{
    gboolean is_present, is_charging, is_discharging = FALSE;
    guint percentage = 0;
    guint32 last_full, current_charge = 0;
    guint time_per = 0;
    
    g_object_get (G_OBJECT(battery->priv->device), 
    		  "is-present", &is_present,
		  "is-charging", &is_charging,
		  "is-discharging", &is_discharging, 
		  "percentage", &percentage,
		  "last-full", &last_full,
		  "current-charge", &current_charge,
		  "time", &time_per,
		  NULL);
		  
    TRACE ("Battery status is_present %s is_charging %s is_discharging %s", 
	   xfpm_bool_to_string (is_present), 
	   xfpm_bool_to_string (is_charging), 
	   xfpm_bool_to_string (is_discharging));
	   
    TRACE ("Battery info precentage %i last_full %i current_charge %i time_per %i",
	   percentage, last_full, current_charge, time_per);
	   
    battery->priv->type == HAL_DEVICE_TYPE_PRIMARY ?
			   xfpm_battery_refresh_primary (battery, is_present, 
							 is_charging, is_discharging, 
							 last_full, current_charge,
							 percentage, time_per)
  					           :
			    xfpm_battery_refresh_misc   (battery, is_present, 
							 is_charging, is_discharging, 
							 last_full, current_charge,
							 percentage, time_per);
							 
    xfpm_battery_refresh_visible_icon (battery);
}

static void
xfpm_battery_device_changed_cb (HalBattery *device, XfpmBattery *battery)
{
    TRACE("start");
    xfpm_battery_refresh (battery);
}

static gchar *
_get_icon_prefix_from_enum_type (HalDeviceType type)
{
    if ( type == HAL_DEVICE_TYPE_PRIMARY )
    {
	return g_strdup("xfpm-primary-");
    }
    else if ( type == HAL_DEVICE_TYPE_UPS ) 
    {
	return g_strdup("xfpm-ups-");
    }
    else if ( type == HAL_DEVICE_TYPE_MOUSE ) 
    {
	return g_strdup("xfpm-mouse-");
    }
    else if ( type == HAL_DEVICE_TYPE_KEYBOARD ) 
    {
	return g_strdup("xfpm-keyboard-");
    }
    else if ( type == HAL_DEVICE_TYPE_CAMERA ) 
    {
	return g_strdup("xfpm-camera-");
    }
    else if ( type == HAL_DEVICE_TYPE_PDA ) 
    {
	return g_strdup("xfpm-pda-");
    }
    else if ( type == HAL_DEVICE_TYPE_KEYBOARD_MOUSE ) 
    {
	return g_strdup("xfpm-keyboard-mouse-");
    }
    
    return g_strdup("xfpm-primary-");
}

static void
xfpm_battery_adapter_changed_cb (XfpmAdapter *adapter, gboolean present, XfpmBattery *battery)
{
    battery->priv->adapter_present = present;
    xfpm_battery_refresh (battery);
}

static void
xfpm_battery_tray_icon_settings_changed (GObject *obj, GParamSpec *spec, XfpmBattery *battery)
{
    xfpm_battery_refresh_visible_icon (battery);
}

static void
xfpm_battery_show_info (XfpmTrayIcon *tray, XfpmBattery *battery)
{
    gchar *icon = g_strdup_printf("%s%s",
				  battery->priv->icon_prefix, 
	    			  xfpm_battery_get_icon_index(battery->priv->type, 100));
				      
    GtkWidget *info = xfpm_battery_info_new (battery->priv->device, icon);
    
    g_free (icon);
    
    gtk_widget_show_all (info);
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

    object_class->finalize = xfpm_battery_finalize;
    
    g_type_class_add_private(klass,sizeof(XfpmBatteryPrivate));
}

static void
xfpm_battery_init(XfpmBattery *battery)
{
    battery->priv = XFPM_BATTERY_GET_PRIVATE(battery);
    
    battery->priv->icon      = xfpm_tray_icon_new ();
    battery->priv->adapter   = xfpm_adapter_new ();
    battery->priv->conf      = xfpm_xfconf_new ();
    battery->priv->notify    = xfpm_notify_new ();
    battery->priv->state = BATTERY_STATE_UNKNOWN;
    battery->priv->icon_prefix = NULL;
    
    battery->priv->adapter_present = xfpm_adapter_get_present (battery->priv->adapter);
    
    battery->priv->sig_1 = g_signal_connect (battery->priv->adapter ,"adapter-changed",
					     G_CALLBACK (xfpm_battery_adapter_changed_cb), battery);
    
    g_signal_connect (battery->priv->icon, "show-information", 
		      G_CALLBACK (xfpm_battery_show_info), battery);
}

static void
xfpm_battery_finalize(GObject *object)
{
    XfpmBattery *battery;
    battery = XFPM_BATTERY(object);
 
    if ( g_signal_handler_is_connected (battery->priv->adapter, battery->priv->sig_1 ) )
	g_signal_handler_disconnect (G_OBJECT (battery->priv->adapter), battery->priv->sig_1);
	
    if ( g_signal_handler_is_connected (battery->priv->conf, battery->priv->sig_2 ) )
	g_signal_handler_disconnect (G_OBJECT (battery->priv->conf), battery->priv->sig_2);
	
     if ( g_signal_handler_is_connected (battery->priv->device, battery->priv->sig_3 ) )
	g_signal_handler_disconnect (G_OBJECT (battery->priv->device), battery->priv->sig_3);

    g_object_unref (battery->priv->icon);
    
    g_object_unref (battery->priv->device);
	
    if ( battery->priv->icon_prefix )
    	g_free(battery->priv->icon_prefix);
	
    g_object_unref (battery->priv->adapter);
    
    g_object_unref (battery->priv->conf);
    
    g_object_unref (battery->priv->notify);

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
    
    battery->priv->sig_3 = g_signal_connect (G_OBJECT(battery->priv->device), "battery-changed",
					     G_CALLBACK(xfpm_battery_device_changed_cb), battery);
		      
    battery->priv->sig_2 = g_signal_connect (G_OBJECT(battery->priv->conf), "notify::" SHOW_TRAY_ICON_CFG,
					     G_CALLBACK(xfpm_battery_tray_icon_settings_changed), battery);
    
    return battery;
}

const HalBattery*
xfpm_battery_get_device (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY(battery), NULL);
    
    return battery->priv->device;
}

XfpmBatteryState xfpm_battery_get_state (XfpmBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_BATTERY(battery), BATTERY_NOT_PRESENT);
    
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
