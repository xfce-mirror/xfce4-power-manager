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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>

#include "hal-ctx.h"
#include "hal-power.h"

/* Init */
static void hal_power_class_init (HalPowerClass *klass);
static void hal_power_init       (HalPower *power);
static void hal_power_finalize   (GObject *object);

#define HAL_POWER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_POWER, HalPowerPrivate))

struct HalPowerPrivate
{
    GHashTable *hash  ;
    HalCtx     *ctx   ;
};

enum
{
    DEVICE_ADDED,
    DEVICE_REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(HalPower, hal_power, G_TYPE_OBJECT)

/*
 * Sanity check of the keys present of a battery device
 */
static gboolean
hal_power_chech_battery (HalPower *power, const gchar *udi)
{
    if ( hal_ctx_device_has_key(power->priv->ctx, udi, "battery.present") &&
	 hal_ctx_device_has_key(power->priv->ctx, udi, "battery.rechargeable.is_charging") &&
         hal_ctx_device_has_key(power->priv->ctx, udi, "battery.rechargeable.is_discharging") &&
         hal_ctx_device_has_key(power->priv->ctx, udi, "battery.charge_level.current") &&
	 hal_ctx_device_has_key(power->priv->ctx, udi, "battery.charge_level.last_full") )
		 return TRUE;
    
    return FALSE;
}

static gboolean
hal_power_is_power_device(HalPower *power, const gchar *udi)
{
    if ( hal_ctx_device_has_capability(power->priv->ctx, udi, "battery") ||
    	 hal_ctx_device_has_capability(power->priv->ctx, udi, "ac_adapter") )
	return TRUE;
    
    return FALSE;
}

static HalDevice *
hal_power_add_device(HalPower *power, const gchar *udi)
{
    HalDevice *device;
    device = hal_device_new (udi);
    
    g_hash_table_insert (power->priv->hash, g_strdup(udi), device);
    
    return device;
}

static void
hal_power_remove_device(HalPower *power, HalDevice *device, const gchar *udi)
{
    g_object_unref(device);
    if (!g_hash_table_remove(power->priv->hash, udi))
    	g_warning ("Unable to removed object from hash\n");
}

static HalDevice *
hal_power_get_device(HalPower *power, const gchar *udi)
{
    HalDevice *device = NULL;
    device = g_hash_table_lookup(power->priv->hash, udi);
    return device;
}

static void
hal_power_device_added_cb (LibHalContext *context, const gchar *udi)
{
    HalPower *power = libhal_ctx_get_user_data(context);
    if ( !hal_power_is_power_device(power, udi) ) return;
    
    HalDevice *device = hal_power_add_device(power, udi);
    
    g_signal_emit(G_OBJECT(power), signals[DEVICE_ADDED], 0, device);
}

static void
hal_power_device_removed_cb (LibHalContext *context, const gchar *udi)
{
    HalPower *power = libhal_ctx_get_user_data (context);
    
    HalDevice *device = hal_power_get_device (power, udi);
    
    if (device)
    {
    	g_signal_emit(power, signals[DEVICE_REMOVED], 0, device);
	hal_power_remove_device(power, device, udi);
    }
}

static void
hal_power_class_init(HalPowerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[DEVICE_ADDED] =
    	g_signal_new("device-added",
		     HAL_TYPE_POWER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalPowerClass, device_added),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__POINTER,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
		     
    signals[DEVICE_REMOVED] =
    	g_signal_new("device-removed",
		     HAL_TYPE_POWER,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalPowerClass, device_removed),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__POINTER,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
		     
    object_class->finalize = hal_power_finalize;

    g_type_class_add_private(klass,sizeof(HalPowerPrivate));
}

static void
hal_power_get_batteries(HalPower *power)
{
    gint number = 0;
    gchar **batteries = NULL;
    gint i;
    
    batteries = hal_ctx_get_device_by_capability(power->priv->ctx, "battery", &number);
    
    if ( !batteries || number == 0 ) 
    	goto out;
    
    for ( i = 0; batteries[i]; i++ )
    {
	if (!hal_power_chech_battery(power, batteries[i]))
		continue;
		
    	hal_power_add_device(power, batteries[i]);
    }
	
    libhal_free_string_array(batteries);
    
out:
    	;
    
}

static void
hal_power_get_adapter(HalPower *power)
{
    gint number = 0;
    gchar **adapter = NULL;
    gint i = 0;
    
    adapter = hal_ctx_get_device_by_capability(power->priv->ctx, "ac_adapter", &number);
    
    if ( !adapter || number == 0 ) 
    	goto out;
    
    if ( number > 1) 
    	g_warning("More than one adapter were found on the system");
    
    hal_power_add_device (power, adapter[i]);
    
    libhal_free_string_array(adapter);
    
out:
    	;
    
}

static void
hal_power_init(HalPower *power)
{
    power->priv = HAL_POWER_GET_PRIVATE(power);
    
    power->priv->ctx   = hal_ctx_new();
    power->priv->hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    if ( !hal_ctx_connect(power->priv->ctx) )
    {
    	g_critical("Unable to connect to HAL");
	goto out;
    }
    
    hal_ctx_set_user_data (power->priv->ctx, power);
    hal_ctx_set_device_added_callback (power->priv->ctx, hal_power_device_added_cb);
    hal_ctx_set_device_removed_callback (power->priv->ctx, hal_power_device_removed_cb);
   
    
    hal_power_get_adapter (power);
    hal_power_get_batteries (power);
    
out:
	;
}

static void
hal_power_finalize(GObject *object)
{
    HalPower *power;
    
    power = HAL_POWER(object);
    
    if (power->priv->hash)
    	g_hash_table_unref(power->priv->hash);
    
    if ( power->priv->ctx )
    	g_object_unref(power->priv->ctx);
    
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
hal_power_get_devices (HalPower *power)
{
    guint i;
    GPtrArray *array;
    HalDevice *device;
   
    array = g_ptr_array_new ();
   
    GList *list = NULL;
    list = g_hash_table_get_values (power->priv->hash);
   
    if (!list)
   	goto out;
	
    for ( i=0; i < g_list_length (list); i++)
    {
       device = g_list_nth_data (list, i);
       g_object_ref (device);
       g_ptr_array_add (array, device);
    }
   
    g_list_free (list);

out:

   return array;
}
