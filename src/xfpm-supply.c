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

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "libxfpm/hal-power.h"
#include "libxfpm/xfpm-string.h"
#include "libxfpm/xfpm-common.h"
#include "libxfpm/xfpm-notify.h"
#include "libxfpm/xfpm-icons.h"

#include "xfpm-supply.h"
#include "xfpm-adapter.h"
#include "xfpm-battery.h"
#include "xfpm-enum.h"
#include "xfpm-enum-types.h"
#include "xfpm-xfconf.h"
#include "xfpm-tray-icon.h"
#include "xfpm-config.h"
#include "xfpm-shutdown.h"
#include "xfpm-inhibit.h"
#include "xfpm-debug.h"
#include "xfpm-marshal.h"

static void xfpm_supply_finalize   (GObject *object);
static void xfpm_supply_refresh_tray_icon (XfpmSupply *supply);
static void xfpm_supply_hide_adapter_icon (XfpmSupply *supply);

#define XFPM_SUPPLY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_SUPPLY, XfpmSupplyPrivate))

struct XfpmSupplyPrivate
{
    XfpmNotify    *notify;
    XfpmAdapter   *adapter;
    XfpmXfconf    *conf;
    XfpmTrayIcon  *tray;
    XfpmInhibit   *inhibit;
    
    HalPower      *power;
    GHashTable    *hash;
    
    gboolean       low_power;
    gboolean       adapter_present;
    gboolean       inhibited;
    guint8         power_management;
};

enum
{
    PROP_O,
    PROP_ON_BATTERY,
    PROP_ON_LOW_BATTERY
};    

enum
{
    SHUTDOWN_REQUEST,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmSupply, xfpm_supply, G_TYPE_OBJECT)

static void
xfpm_supply_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
    XfpmSupply *supply;
    
    supply = XFPM_SUPPLY (object);
    
    switch ( prop_id )
    {
	case PROP_ON_BATTERY:
	    g_value_set_boolean (value, !supply->priv->adapter_present);
	    break;
	case PROP_ON_LOW_BATTERY:
	    g_value_set_boolean (value, supply->priv->low_power);
	    break;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	    break;
    }
    
}

static void
xfpm_supply_has_inhibit_changed_cb (XfpmInhibit *inhibit, gboolean inhibited, XfpmSupply *supply)
{
    supply->priv->inhibited = inhibited;
}

static void 
xfpm_supply_tray_settings_changed (GObject *obj, GParamSpec *spec, XfpmSupply *supply)
{
    xfpm_supply_refresh_tray_icon (supply);
}

static void
xfpm_supply_class_init(XfpmSupplyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = xfpm_supply_get_property;

    signals[SHUTDOWN_REQUEST] = 
    	g_signal_new("shutdown-request",
                      XFPM_TYPE_SUPPLY,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmSupplyClass, shutdown_request),
                      NULL, NULL,
                      _xfpm_marshal_VOID__BOOLEAN_ENUM,
                      G_TYPE_NONE, 2, 
		      G_TYPE_BOOLEAN,
		      XFPM_TYPE_SHUTDOWN_REQUEST);
		      
    g_object_class_install_property(object_class,
                                    PROP_ON_BATTERY,
                                    g_param_spec_boolean("on-battery",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));
							 
    g_object_class_install_property(object_class,
                                    PROP_ON_LOW_BATTERY,
                                    g_param_spec_boolean("on-low-battery",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

    object_class->finalize = xfpm_supply_finalize;

    g_type_class_add_private (klass, sizeof (XfpmSupplyPrivate));
}

static void
xfpm_supply_init (XfpmSupply *supply)
{
    supply->priv = XFPM_SUPPLY_GET_PRIVATE (supply);
  
    supply->priv->hash    = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
     
    supply->priv->power   = hal_power_new      ();
    supply->priv->notify  = xfpm_notify_new    ();
    supply->priv->conf    = xfpm_xfconf_new    ();
    supply->priv->tray    = NULL;
    supply->priv->inhibit = xfpm_inhibit_new   ();
    supply->priv->inhibited = FALSE;
    supply->priv->low_power = FALSE;
    
    g_signal_connect (supply->priv->inhibit, "has-inhibit-changed",
		      G_CALLBACK (xfpm_supply_has_inhibit_changed_cb), supply);
			  
    g_signal_connect (supply->priv->conf, "notify::" SHOW_TRAY_ICON_CFG,
		      G_CALLBACK (xfpm_supply_tray_settings_changed), supply);
}

static void
xfpm_supply_finalize (GObject *object)
{
    XfpmSupply *supply;
    supply = XFPM_SUPPLY(object);
    
    g_object_unref(supply->priv->power);
	
    g_hash_table_destroy (supply->priv->hash);
	
    g_object_unref (supply->priv->notify);
	
    g_object_unref (supply->priv->conf);
	
    g_object_unref (supply->priv->adapter);
    
    g_object_unref (supply->priv->inhibit);
    xfpm_supply_hide_adapter_icon (supply);
	
    G_OBJECT_CLASS(xfpm_supply_parent_class)->finalize(object);
}

static void
xfpm_supply_hide_adapter_icon (XfpmSupply *supply)
{
    if ( supply->priv->tray )
    {
	g_object_unref (supply->priv->tray);
	supply->priv->tray = NULL;
    }
}

static void
xfpm_supply_show_tray_icon (XfpmSupply *supply)
{
#ifdef DEBUG 
    /* This shouldn't happen at all*/
    if ( supply->priv->tray )
    {
	g_critical ("Already have tray icon for adapter!");
	xfpm_supply_hide_adapter_icon (supply);
    }
#endif    

    supply->priv->tray = xfpm_tray_icon_new ();
    
    xfpm_tray_icon_set_visible (supply->priv->tray, FALSE);
    xfpm_tray_icon_set_icon (supply->priv->tray, XFPM_AC_ADAPTER_ICON);
    xfpm_tray_icon_set_show_info_menu (supply->priv->tray, FALSE);
}

static void
xfpm_supply_refresh_tray_icon (XfpmSupply *supply)
{
    XfpmShowIcon show_icon;
    
    g_object_get (G_OBJECT (supply->priv->conf),
		  SHOW_TRAY_ICON_CFG, &show_icon,
		  NULL);
		  
    XFPM_DEBUG_ENUM ("Tray icon configuration: ", show_icon, XFPM_TYPE_SHOW_ICON);
    
    if ( show_icon == SHOW_ICON_ALWAYS )
    {
	if ( g_hash_table_size (supply->priv->hash) == 0 )
	{
	    xfpm_supply_show_tray_icon (supply);
	    xfpm_tray_icon_set_tooltip (supply->priv->tray,
					supply->priv->adapter_present ? 
					(_("Adapter present")) :
					(_("Adapter not present")) );
	}
	else
	{
	    xfpm_supply_hide_adapter_icon (supply);
	}
    }
    else
    {
	xfpm_supply_hide_adapter_icon (supply);
    }
}

static gboolean 
xfpm_supply_on_low_power (XfpmSupply *supply)
{
    const HalBattery *device;
    guint low_power_level;
    
    GList *list = NULL;
    guint i;
    gboolean low_power = FALSE;
    
    if (supply->priv->adapter_present )
	return FALSE;
    
    list = g_hash_table_get_values (supply->priv->hash);
    
    if ( !list)
	return FALSE;
	
    g_object_get (G_OBJECT (supply->priv->conf),
		  CRITICAL_POWER_LEVEL, &low_power_level,
		  NULL);
		  
    for ( i=0; i< g_list_length(list); i++)
    {
	XfpmBattery *battery = NULL;
	HalDeviceType type;
	guint percentage;
	battery = g_list_nth_data(list, i);
	
	if ( !battery )
	    continue;
	    
	device = xfpm_battery_get_device (battery);
	g_object_get (G_OBJECT(device), "type", &type, "percentage", &percentage, NULL);
	if ( type != HAL_DEVICE_TYPE_PRIMARY )
	    continue;
	    
	if ( percentage < low_power_level ) 
	    low_power = TRUE;
	else 
	    low_power = FALSE;
    }
    return low_power;
}

static void
_notify_action_callback (NotifyNotification *n, gchar *action, XfpmSupply *supply)
{
    if ( xfpm_strequal(action, "shutdown") )
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, TRUE, XFPM_DO_SHUTDOWN);
    else if ( xfpm_strequal(action, "hibernate") )
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, TRUE, XFPM_DO_HIBERNATE);
}

static void
xfpm_supply_add_actions_to_notification (XfpmSupply *supply, XfpmBattery *battery, NotifyNotification *n)
{
    if (supply->priv->power_management != 0 )
    {
        xfpm_notify_add_action_to_notification(
			       supply->priv->notify,
			       n,
                               "shutdown",
                               _("Shutdown the system"),
                               (NotifyActionCallback)_notify_action_callback,
                               supply);   
    }
    
    if ( supply->priv->power_management & SYSTEM_CAN_HIBERNATE )
    {
        xfpm_notify_add_action_to_notification(
			       supply->priv->notify,
			       n,
                               "hibernate",
                               _("Hibernate the system"),
                               (NotifyActionCallback)_notify_action_callback,
                               supply);      
    }
}

static void
xfpm_supply_show_critical_action_inhibited (XfpmSupply *supply, XfpmBattery *battery)
{
    NotifyNotification *n;
    const gchar *message;
    
    message = _("System is running on low power, "\
               "but an application is currently disabling the automatic sleep, "\
	       "this means that doing a sleep now may damage the data of this application. "\
	       "Close this application before putting the computer on sleep mode or plug "\
	       "in your AC adapter");
    
     n = 
	xfpm_notify_new_notification (supply->priv->notify, 
				      _("Xfce power manager"), 
				      message, 
				      xfpm_battery_get_icon_name (battery),
				      30000,
				      XFPM_NOTIFY_CRITICAL,
				      xfpm_battery_get_status_icon (battery));
				      
    xfpm_supply_add_actions_to_notification (supply, battery, n);
    
    xfpm_notify_critical (supply->priv->notify, n);
}

static void
xfpm_supply_show_critical_action (XfpmSupply *supply, XfpmBattery *battery)
{
    const gchar *message;
    NotifyNotification *n;
    
    message = _("System is running on low power. "\
              "Save your work to avoid losing data");
	      
    n = 
	xfpm_notify_new_notification (supply->priv->notify, 
				      _("Xfce power manager"), 
				      message, 
				      xfpm_battery_get_icon_name (battery),
				      20000,
				      XFPM_NOTIFY_CRITICAL,
				      xfpm_battery_get_status_icon (battery));
    
    xfpm_supply_add_actions_to_notification (supply, battery, n);
    xfpm_notify_critical (supply->priv->notify, n);
}

static void
xfpm_supply_process_critical_action (XfpmSupply *supply, XfpmBattery *battery, XfpmShutdownRequest critical_action)
{
    if ( critical_action == XFPM_ASK )
    {
	if ( supply->priv->inhibited )
	    xfpm_supply_show_critical_action_inhibited (supply, battery);
	else
	    xfpm_supply_show_critical_action (supply, battery);
	    
	return;
    }
	
    g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, TRUE, critical_action);
}

static void
xfpm_supply_handle_primary_critical (XfpmSupply *supply, XfpmBattery *battery)
{
    XfpmShutdownRequest critical_action;
    
    g_object_get (G_OBJECT (supply->priv->conf),
	          CRITICAL_BATT_ACTION_CFG, &critical_action,
		  NULL);

    TRACE ("System is running on low power");
    XFPM_DEBUG_ENUM ("Critical battery action", critical_action, XFPM_TYPE_SHUTDOWN_REQUEST);
    
    if ( supply->priv->inhibited )
    {
	xfpm_supply_show_critical_action_inhibited (supply, battery);
    }
    else if ( critical_action == XFPM_DO_NOTHING )
    {
	xfpm_supply_show_critical_action (supply, battery);
    }
    else
    {
	xfpm_supply_process_critical_action (supply, battery, critical_action);
    }
}

static void
xfpm_supply_battery_state_changed_cb (XfpmBattery *battery, XfpmBatteryState state, XfpmSupply *supply)
{
    gboolean low_power;
    
    low_power = xfpm_supply_on_low_power (supply);
    
    if ( state == BATTERY_CHARGE_CRITICAL && low_power )
    {
	supply->priv->low_power = TRUE;
	g_object_notify (G_OBJECT (supply), "on-low-battery");
	xfpm_supply_handle_primary_critical (supply, battery);
    }
    else if ( !low_power && supply->priv->low_power )
    {
	supply->priv->low_power = FALSE;
	g_object_notify (G_OBJECT (supply), "on-low-battery");
	xfpm_notify_close_critical (supply->priv->notify);
    }
}

static XfpmBattery *
xfpm_supply_get_battery (XfpmSupply *supply, const gchar *udi)
{
    XfpmBattery *battery;
    battery = (XfpmBattery *)g_hash_table_lookup (supply->priv->hash, udi);
    return battery;
}

static void
xfpm_supply_add_battery (XfpmSupply *supply, const HalBattery *device)
{
    XfpmBattery *battery;
    const gchar *udi;
    
    udi = hal_device_get_udi (HAL_DEVICE(device));

    TRACE("New battery found %s", udi);

    battery = xfpm_battery_new (device);
    
    g_hash_table_insert (supply->priv->hash, g_strdup(udi), battery);
    
    g_signal_connect (G_OBJECT(battery), "battery-state-changed",
		      G_CALLBACK(xfpm_supply_battery_state_changed_cb), supply);
		      
    xfpm_supply_refresh_tray_icon (supply);
}

static void
xfpm_supply_remove_battery (XfpmSupply *supply,  const HalBattery *device)
{
    const gchar *udi = hal_device_get_udi (HAL_DEVICE(device));
    
    XfpmBattery *battery = xfpm_supply_get_battery(supply, udi);
	
    if ( battery )
    {
	TRACE("Removing battery %s", udi);
	if (!g_hash_table_remove (supply->priv->hash, udi))
		g_critical ("Unable to remove battery object from hash");
    }
    xfpm_supply_refresh_tray_icon (supply);
}

static void
xfpm_supply_battery_added_cb (HalPower *power, const HalBattery *device, XfpmSupply *supply)
{
    xfpm_supply_add_battery (supply, device);
}

static void
xfpm_supply_battery_removed_cb (HalPower *power, const HalBattery *device, XfpmSupply *supply)
{
    xfpm_supply_remove_battery (supply, device);
}

static gboolean
xfpm_supply_monitor_start (XfpmSupply *supply)
{
    GPtrArray *array;
    guint i = 0;
    
    supply->priv->adapter_present = xfpm_adapter_get_present (supply->priv->adapter);
    
    array = hal_power_get_batteries (supply->priv->power);
    
    for ( i = 0; i<array->len; i++ )
    {
	HalBattery *device;
	device = (HalBattery *)g_ptr_array_index(array, i);
	xfpm_supply_add_battery (supply, device);
    }
    
    g_ptr_array_free(array, TRUE);
    
    g_signal_connect(supply->priv->power, "battery-added",
		     G_CALLBACK(xfpm_supply_battery_added_cb), supply);
		     
    g_signal_connect(supply->priv->power, "battery-removed",
		     G_CALLBACK(xfpm_supply_battery_removed_cb), supply);
    
    return FALSE;
}

static void
xfpm_supply_save_power (XfpmSupply *supply)
{
    gboolean save_power;

    g_object_get (G_OBJECT (supply->priv->conf),
		  POWER_SAVE_ON_BATTERY, &save_power,
		  NULL);
    
    if ( save_power == FALSE )
	hal_power_unset_power_save (supply->priv->power);
    else if ( supply->priv->adapter_present )
	hal_power_unset_power_save (supply->priv->power);
    else 
	hal_power_set_power_save (supply->priv->power);
}

static void
xfpm_supply_adapter_changed_cb (XfpmAdapter *adapter, gboolean present, XfpmSupply *supply)
{
    supply->priv->adapter_present = present;
    
    g_object_notify (G_OBJECT (supply), "on-battery");
    
    if ( supply->priv->adapter_present && supply->priv->low_power )
    {
	supply->priv->low_power = FALSE;
	g_object_notify (G_OBJECT (supply), "on-low-battery");
	xfpm_notify_close_critical (supply->priv->notify);
    }
    
    xfpm_supply_save_power (supply);
}

/*
 * Public functions
 */ 
XfpmSupply *
xfpm_supply_new (guint8 power_management_info)
{
    XfpmSupply *supply = NULL;
    supply = g_object_new(XFPM_TYPE_SUPPLY,NULL);
    
    supply->priv->power_management = power_management_info;
    
    return supply;
}

void xfpm_supply_monitor (XfpmSupply *supply)
{
    supply->priv->adapter = xfpm_adapter_new ();
    
    g_signal_connect (supply->priv->adapter, "adapter-changed",
		      G_CALLBACK(xfpm_supply_adapter_changed_cb), supply);
      
    xfpm_supply_monitor_start (supply);
    
    xfpm_supply_refresh_tray_icon (supply);
    
    supply->priv->low_power = xfpm_supply_on_low_power (supply);
    
    if ( supply->priv->low_power )
	g_object_notify (G_OBJECT (supply), "on-low-battery");
}

gboolean xfpm_supply_on_low_battery (XfpmSupply *supply)
{
    g_return_val_if_fail (XFPM_IS_SUPPLY(supply), FALSE);
    
    return supply->priv->low_power;
}

void xfpm_supply_reload (XfpmSupply *supply)
{
    g_return_if_fail (XFPM_IS_SUPPLY (supply));
    
    g_object_unref (supply->priv->power);
    
    g_hash_table_destroy (supply->priv->hash);
    
    supply->priv->power = hal_power_new ();
    supply->priv->hash  = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    
    xfpm_supply_monitor_start (supply);
}
