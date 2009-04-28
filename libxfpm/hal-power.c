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

#include <glib.h>
#include <dbus/dbus-glib.h>

#include "hal-manager.h"
#include "hal-power.h"

/* Init */
static void hal_power_class_init (HalPowerClass *klass);
static void hal_power_init       (HalPower *power);
static void hal_power_finalize   (GObject *object);

#define HAL_POWER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_POWER, HalPowerPrivate))

struct HalPowerPrivate
{
    GHashTable *hash  	;
    HalManager *manager ;
    
    gulong      sig_1;
    gulong      sig_2;
};

enum
{
    BATTERY_ADDED,
    BATTERY_REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(HalPower, hal_power, G_TYPE_OBJECT)

/*
 * Sanity check of the keys present of a battery device
 */
static gboolean
hal_power_check_battery (HalPower *power, HalDevice *device)
{
    if ( hal_device_has_key(device, "battery.present") &&
	 hal_device_has_key(device, "battery.rechargeable.is_charging") &&
         hal_device_has_key(device, "battery.rechargeable.is_discharging") &&
         hal_device_has_key(device, "battery.charge_level.current") &&
	 hal_device_has_key(device, "battery.charge_level.last_full") )
		 return TRUE;
    
    return FALSE;
}

/*
 * Check if the device added is actually a new non-moniotred device
 * Hald duplicates udi when running udevadm trigger
 */
static gboolean
hal_power_is_battery_new (HalPower *power, HalDevice *device)
{
    GList *list = NULL;
    gboolean new_device = TRUE;
    gint is_new = 0;
    HalDevice *hash_device;
    int i;
    
    list = g_hash_table_get_values (power->priv->hash);
    if ( !list )
	return new_device;
	
    if ( g_list_length(list) == 0 )
	return new_device;
	
    for ( i = 0; i < g_list_length(list); i++ )
    {
	hash_device = (HalDevice *) g_list_nth_data (list, i);
	
	if ( hal_device_get_property_int (hash_device, "battery.charge_level.current") !=
	     hal_device_get_property_int (device, "battery.charge_level.current") && 
	     hal_device_get_property_int (hash_device, "battery.charge_level.last_full") !=
	     hal_device_get_property_int (device, "battery.charge_level.last_full") )
	    is_new++;
    }

    /* Device doesn't match to any one in the hash*/
    if ( is_new == g_list_length (list) )
	new_device = TRUE;
    else
	new_device = FALSE;
	
    g_list_free (list);
    return new_device;
}

static HalBattery *
hal_power_add_battery (HalPower *power, const gchar *udi)
{
    HalBattery *battery;
    battery  = hal_battery_new (udi);
    
    g_hash_table_insert (power->priv->hash, g_strdup(udi), battery );
    
    return battery ;
}

static void
hal_power_remove_battery (HalPower *power, HalBattery *battery, const gchar *udi)
{
    g_signal_emit (power, signals[BATTERY_REMOVED], 0, battery);
    
    if (!g_hash_table_remove(power->priv->hash, udi))
    	g_warning ("Unable to removed object from hash\n");
}

static HalBattery *
hal_power_get_battery_from_hash (HalPower *power, const gchar *udi)
{
    HalBattery *battery = NULL;
    
    battery = g_hash_table_lookup(power->priv->hash, udi);
    
    return battery;
}

static void
hal_power_device_added_cb (HalManager *manager, const gchar *udi, HalPower *power)
{
    HalDevice *device = hal_device_new ();
    hal_device_set_udi (device, udi);
    
    if ( hal_device_has_capability (device, "battery") )
    {
	if ( !hal_power_check_battery (power, device) )
	    goto out;
	    
	if ( !hal_power_is_battery_new (power, device) )
	    goto out;
	    
	HalBattery *battery  = hal_power_add_battery (power, udi);
        g_signal_emit (G_OBJECT(power), signals[BATTERY_ADDED], 0, battery);
    }
out:
    g_object_unref (device);
}

static void
hal_power_device_removed_cb (HalManager *manager, const gchar *udi, HalPower *power)
{
    HalBattery *battery  = hal_power_get_battery_from_hash (power, udi);
    
    if (battery)
    {
	hal_power_remove_battery (power, battery , udi);
    }
}

static void
hal_power_class_init(HalPowerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[BATTERY_ADDED] =
    	g_signal_new("battery-added",
		     HAL_TYPE_POWER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalPowerClass, battery_added),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__POINTER,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
		     
    signals[BATTERY_REMOVED] =
    	g_signal_new("battery-removed",
		     HAL_TYPE_POWER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalPowerClass, battery_removed),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__POINTER,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
		     
    object_class->finalize = hal_power_finalize;

    g_type_class_add_private(klass,sizeof(HalPowerPrivate));
}

static void
hal_power_get_batteries_internal (HalPower *power)
{
    HalDevice *device = hal_device_new ();
    gchar **batteries = NULL;
    gint i;
    
    batteries = hal_manager_find_device_by_capability (power->priv->manager, "battery");
    
    if ( !batteries ) 
    	goto out;
    
    for ( i = 0; batteries[i]; i++ )
    {
	hal_device_set_udi (device, batteries[i]);
	
	if (!hal_power_check_battery(power, device))
	    continue;
		
	if ( !hal_power_is_battery_new (power, device) )
	    continue;
	    
    	hal_power_add_battery (power, batteries[i]);
    }
    hal_manager_free_string_array (batteries);

out:
    g_object_unref (device);
    
}

static gboolean
hal_power_is_power_save_set (void)
{
    HalDevice *device;
    gboolean   val;
    
    device = hal_device_new ();
    
    hal_device_set_udi (device, "/org/freedesktop/Hal/devices/computer");
    
    val = hal_device_get_property_bool (device, "power_management.is_powersave_set");
    
    g_object_unref (device);
    
    return val;
}

static gboolean 
hal_power_set_power_save_internal (HalPower *power, gboolean set)
{
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    GError          *error = NULL;
    gint             ret = 0;
    
    if ( hal_power_is_power_save_set () == set )
	return TRUE;
    
    bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
    
    proxy = dbus_g_proxy_new_for_name (bus, 
				       "org.freedesktop.Hal",
				       "/org/freedesktop/Hal/devices/computer",
				       "org.freedesktop.Hal.Device.SystemPowerManagement");
				       
    if ( !proxy )
    {
	g_warning ("Unable to get proxy for /org/freedesktop/Hal/devices/computer");
	dbus_g_connection_unref (bus);
	return FALSE;
    }
    
    dbus_g_proxy_call (proxy, "SetPowerSave", &error,
		       G_TYPE_BOOLEAN, set,
		       G_TYPE_INVALID,
		       G_TYPE_INT, &ret,
		       G_TYPE_INVALID);
    
    dbus_g_connection_unref (bus);
    g_object_unref (proxy);
    
    if ( error )
    {
	g_warning ("%s: ", error->message);
	g_error_free (error);
	return FALSE;
    }
    return ret == 0 ? TRUE : FALSE;
}

static void
hal_power_init(HalPower *power)
{
    power->priv = HAL_POWER_GET_PRIVATE(power);
    
    power->priv->manager = hal_manager_new ();
    power->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    
    power->priv->sig_1 = g_signal_connect (power->priv->manager, "device-added", 
					   G_CALLBACK(hal_power_device_added_cb), power);
    
    power->priv->sig_2 = g_signal_connect (power->priv->manager, "device-removed",
					    G_CALLBACK(hal_power_device_removed_cb), power);
   
    hal_power_get_batteries_internal (power);
}

static void
hal_power_finalize(GObject *object)
{
    HalPower *power;
    
    power = HAL_POWER(object);
    
    if ( g_signal_handler_is_connected (power->priv->manager, power->priv->sig_1 ) )
	g_signal_handler_disconnect (power->priv->manager, power->priv->sig_1 );
	
    if ( g_signal_handler_is_connected (power->priv->manager, power->priv->sig_2 ) )
	g_signal_handler_disconnect (power->priv->manager, power->priv->sig_2 );
    
    g_hash_table_destroy (power->priv->hash);
    
    g_object_unref (power->priv->manager);
	
    G_OBJECT_CLASS(hal_power_parent_class)->finalize(object);
}

HalPower *
hal_power_new(void)
{
    HalPower *power = NULL;
    power = g_object_new (HAL_TYPE_POWER,NULL);
    return power;
}

/*
 * Return an array of power devices, the array should be freed by the caller
 */
GPtrArray *
hal_power_get_batteries (HalPower *power)
{
    g_return_val_if_fail (HAL_IS_POWER (power), NULL);
    
    guint i;
    GPtrArray *array;
    HalBattery *battery;
   
    array = g_ptr_array_new ();
   
    GList *list = NULL;
    list = g_hash_table_get_values (power->priv->hash);
   
    if (!list)
   	goto out;
	
    for ( i=0; i < g_list_length (list); i++)
    {
       battery = g_list_nth_data (list, i);
       g_ptr_array_add (array, battery);
    }
   
    g_list_free (list);
out:
   return array;
}

gboolean hal_power_set_power_save  (HalPower *power)
{
    g_return_val_if_fail (HAL_IS_POWER (power), FALSE);

    return hal_power_set_power_save_internal (power, TRUE);
}

gboolean hal_power_unset_power_save (HalPower *power)
{
    g_return_val_if_fail (HAL_IS_POWER (power), FALSE);

    return hal_power_set_power_save_internal (power, FALSE);
}
