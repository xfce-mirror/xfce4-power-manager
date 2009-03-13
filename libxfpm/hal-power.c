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

#include "hal-manager.h"
#include "hal-power.h"
#include "xfpm-string.h"

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
    
    HalDevice  *adapter ;
    gboolean    adapter_found;
};

enum
{
    BATTERY_ADDED,
    BATTERY_REMOVED,
    ADAPTER_ADDED,
    ADAPTER_REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(HalPower, hal_power, G_TYPE_OBJECT)

/*
 * Sanity check of the keys present of a battery device
 */
static gboolean
hal_power_check_battery (HalPower *power, const gchar *udi)
{
    if ( hal_manager_device_has_key(power->priv->manager, udi, "battery.present") &&
	 hal_manager_device_has_key(power->priv->manager, udi, "battery.rechargeable.is_charging") &&
         hal_manager_device_has_key(power->priv->manager, udi, "battery.rechargeable.is_discharging") &&
         hal_manager_device_has_key(power->priv->manager, udi, "battery.charge_level.current") &&
	 hal_manager_device_has_key(power->priv->manager, udi, "battery.charge_level.last_full") )
		 return TRUE;
    
    return FALSE;
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
    
    g_object_unref(battery );
    
    if (!g_hash_table_remove(power->priv->hash, udi))
    	g_warning ("Unable to removed object from hash\n");
}

static void
hal_power_remove_adapter (HalPower *power )
{
    g_signal_emit (G_OBJECT(power), signals[ADAPTER_REMOVED], 0);
    g_object_unref (power->priv->adapter);
    power->priv->adapter_found = FALSE;
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
    if ( hal_manager_device_has_capability (manager, udi, "battery") )
    {
	HalBattery *battery  = hal_power_add_battery (power, udi);
        g_signal_emit (G_OBJECT(power), signals[BATTERY_ADDED], 0, battery);
    }
    else if ( hal_manager_device_has_capability (manager, udi, "ac_adapter") )
    {
	if ( !power->priv->adapter_found )
	{
	    power->priv->adapter = hal_device_new (udi);
	    g_signal_emit (G_OBJECT(power), signals[ADAPTER_ADDED], 0, power->priv->adapter);
	}
    }
}

static void
hal_power_device_removed_cb (HalManager *manager, const gchar *udi, HalPower *power)
{
    HalBattery *battery  = hal_power_get_battery_from_hash (power, udi);
    
    if (battery )
    {
	hal_power_remove_battery (power, battery , udi);
    }
    else if ( power->priv->adapter_found )
    {
	gchar *adapter_udi = NULL;
	g_object_get (G_OBJECT(power->priv->adapter), "udi", &adapter_udi, NULL);
	if (adapter_udi)
	{
	    if ( xfpm_strequal (udi, adapter_udi ) )
		hal_power_remove_adapter (power);
		
	    g_free (adapter_udi);
	}
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
		     
    signals[ADAPTER_ADDED] =
    	g_signal_new("adapter-added",
		     HAL_TYPE_POWER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalPowerClass, adapter_added),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__POINTER,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
		     
    signals[ADAPTER_REMOVED] =
    	g_signal_new("adapter-removed",
		     HAL_TYPE_POWER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalPowerClass, adapter_removed),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0, G_TYPE_NONE);
		     
    object_class->finalize = hal_power_finalize;

    g_type_class_add_private(klass,sizeof(HalPowerPrivate));
}

static void
hal_power_get_batteries_internal (HalPower *power)
{
    gchar **batteries = NULL;
    gint i;
    
    batteries = hal_manager_find_device_by_capability (power->priv->manager, "battery");
    
    if ( !batteries ) 
    	goto out;
    
    for ( i = 0; batteries[i]; i++ )
    {
	if (!hal_power_check_battery(power, batteries[i]))
		continue;
		
    	hal_power_add_battery (power, batteries[i]);
    }
    
    hal_manager_free_string_array (batteries);
    
out:
    	;
    
}


static void
hal_power_get_adapter_internal (HalPower *power)
{
    gchar **adapter = NULL;
    
    adapter = hal_manager_find_device_by_capability (power->priv->manager, "ac_adapter");
    
    if ( !adapter ) 
    	goto out;

    power->priv->adapter = hal_device_new (adapter[0]);
    power->priv->adapter_found = TRUE;
    
    hal_manager_free_string_array (adapter);
out:
    	;
}


static void
hal_power_init(HalPower *power)
{
    power->priv = HAL_POWER_GET_PRIVATE(power);
    
    power->priv->manager = hal_manager_new ();
    power->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    power->priv->adapter_found = FALSE;
    power->priv->adapter = NULL;
    
    g_signal_connect (power->priv->manager, "device-added", 
		      G_CALLBACK(hal_power_device_added_cb), power);
    
    g_signal_connect (power->priv->manager, "device-removed",
		      G_CALLBACK(hal_power_device_removed_cb), power);
   
    
    hal_power_get_adapter_internal (power);
    hal_power_get_batteries_internal (power);
}

static void
hal_power_finalize(GObject *object)
{
    HalPower *power;
    
    power = HAL_POWER(object);
    
    if (power->priv->hash)
    	g_hash_table_unref(power->priv->hash);
    
    if ( power->priv->manager )
    	g_object_unref(power->priv->manager);
    
    G_OBJECT_CLASS(hal_power_parent_class)->finalize(object);
}

HalPower *
hal_power_new(void)
{
    HalPower *power = NULL;
    power = g_object_new(HAL_TYPE_POWER,NULL);
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
       g_object_ref (battery );
       g_ptr_array_add (array, battery);
    }
   
    g_list_free (list);
out:
   return array;
}

gboolean hal_power_adapter_found (HalPower *power)
{
    g_return_val_if_fail (HAL_IS_POWER (power), FALSE);
    
    return power->priv->adapter_found;
}

const HalDevice *hal_power_get_adapter (HalPower *power)
{
    g_return_val_if_fail (HAL_IS_POWER (power), NULL);
    g_return_val_if_fail (power->priv->adapter_found == TRUE, NULL);
    
    return (const HalDevice *)power->priv->adapter;
    
}
