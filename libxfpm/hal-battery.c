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
#include <glib/gi18n.h>

#include "hal-battery.h"
#include "hal-manager.h"
#include "hal-enum.h"

/* Init */
static void hal_battery_class_init (HalBatteryClass *klass);
static void hal_battery_init       (HalBattery *device);
static void hal_battery_finalize   (GObject *object);

static void hal_battery_get_property(GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec);

#define HAL_BATTERY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_BATTERY, HalBatteryPrivate))

struct HalBatteryPrivate
{
    /* Properties read-only */
    HalDeviceType type;
    
    gboolean  is_present;
    gboolean  is_charging;
    gboolean  is_discharging;
    
    gchar    *unit;
    gchar    *technology;
    gchar    *vendor;
    gchar    *model;
    guint     percentage;

    guint32   current_charge;
    guint32   last_full;
    
    guint32   reporting_design;
    guint32   reporting_last_full;
    
    guint      time;
    
};

enum
{
    PROP_0,
    PROP_TYPE,
    PROP_IS_PRESENT,
    PROP_IS_CHARGING,
    PROP_IS_DISCHARGING,
    PROP_CURRENT_CHARGE,
    PROP_PERCENTAGE,
    PROP_REPORTING_DESIGN,
    PROP_LAST_FULL,
    PROP_REPORTING_LAST_FULL,
    PROP_TIME,
    PROP_TECHNOLOGY,
    PROP_VENDOR,
    PROP_MODEL,
    PROP_UNIT
};

enum
{
    BATTERY_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(HalBattery, hal_battery, HAL_TYPE_DEVICE)

static void
hal_battery_class_init(HalBatteryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hal_battery_get_property;
    object_class->finalize = hal_battery_finalize;

    signals[BATTERY_CHANGED] =
    	g_signal_new("battery-changed",
		     HAL_TYPE_BATTERY,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalBatteryClass, battery_changed),
		     NULL, NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0, G_TYPE_NONE);
    
    g_object_class_install_property(object_class,
				    PROP_IS_PRESENT,
				    g_param_spec_boolean("is-present",
				    			 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

    g_object_class_install_property(object_class,
				    PROP_IS_CHARGING,
				    g_param_spec_boolean("is-charging",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));
							 
    g_object_class_install_property(object_class,
				    PROP_IS_DISCHARGING,
				    g_param_spec_boolean("is-discharging",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));
    g_object_class_install_property(object_class,
				    PROP_CURRENT_CHARGE,
				    g_param_spec_uint("current-charge",
						      NULL, NULL,
						      0,
						      G_MAXUINT32,
						      0,
						      G_PARAM_READABLE));
						      
    g_object_class_install_property(object_class,
				    PROP_LAST_FULL,
				    g_param_spec_uint("last-full",
						      NULL, NULL,
						      0,
						      G_MAXUINT32,
						      0,
						      G_PARAM_READABLE));
						      
     g_object_class_install_property(object_class,
				    PROP_REPORTING_DESIGN,
				    g_param_spec_uint("reporting-design",
						      NULL, NULL,
						      0,
						      G_MAXUINT32,
						      0,
						      G_PARAM_READABLE));
    g_object_class_install_property(object_class,
				    PROP_REPORTING_LAST_FULL,
				    g_param_spec_uint("reporting-last-full",
						      NULL, NULL,
						      0,
						      G_MAXUINT32,
						      0,
						      G_PARAM_READABLE));
    g_object_class_install_property(object_class,
				    PROP_TIME,
				    g_param_spec_uint("time",
						      NULL, NULL,
						      0,
						      G_MAXINT,
						      0,
						      G_PARAM_READABLE));
    g_object_class_install_property(object_class,
				    PROP_TYPE,
				    g_param_spec_uint("type",
						      NULL, NULL,
						      0,
						      HAL_DEVICE_TYPE_UNKNOWN,
						      HAL_DEVICE_TYPE_UNKNOWN,
						      G_PARAM_READABLE));
    g_object_class_install_property(object_class,
				    PROP_PERCENTAGE,
				    g_param_spec_uint("percentage",
						      NULL, NULL,
						      0,
						      G_MAXINT,
						      0,
						      G_PARAM_READABLE));
						      
    g_object_class_install_property(object_class,
				    PROP_TECHNOLOGY,
				    g_param_spec_string("technology",
							NULL, NULL,
						        NULL,
						        G_PARAM_READABLE));
     g_object_class_install_property(object_class,
				    PROP_VENDOR,
				    g_param_spec_string("vendor",
							NULL, NULL,
						        NULL,
						        G_PARAM_READABLE));
    g_object_class_install_property(object_class,
				    PROP_MODEL,
				    g_param_spec_string("model",
							NULL, NULL,
						        NULL,
						        G_PARAM_READABLE));
    g_object_class_install_property(object_class,
				    PROP_UNIT,
				    g_param_spec_string("unit",
							 NULL, NULL,
							 NULL,
							 G_PARAM_READABLE));
							 
    g_type_class_add_private(klass,sizeof(HalBatteryPrivate));
}

static void
hal_battery_init (HalBattery *battery)
{

    battery->priv = HAL_BATTERY_GET_PRIVATE(battery);
    
    battery->priv->is_present      = FALSE;
    battery->priv->is_charging     = FALSE;
    battery->priv->is_discharging  = FALSE;

    battery->priv->unit            = NULL;
    battery->priv->vendor          = NULL;
    battery->priv->technology      = NULL;
    battery->priv->model           = NULL;
    battery->priv->type            = HAL_DEVICE_TYPE_UNKNOWN;
    
    battery->priv->percentage      = 0;
    battery->priv->current_charge  = 0;
    battery->priv->last_full       = 0;
    battery->priv->time            = 0;
    battery->priv->reporting_design = 0;
    battery->priv->reporting_last_full = 0;
}

static void hal_battery_get_property(GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec)
{
    HalBattery *battery;
    battery = HAL_BATTERY(object);

    switch (prop_id)
    {
	case PROP_TYPE:
		g_value_set_uint (value, battery->priv->type);
		break;
	case PROP_IS_PRESENT:
		g_value_set_boolean (value, battery->priv->is_present);
		break;
	case PROP_IS_CHARGING:
		g_value_set_boolean (value, battery->priv->is_charging);
		break;
	case PROP_IS_DISCHARGING:
		g_value_set_boolean (value, battery->priv->is_discharging);
		break;
	case PROP_UNIT:
		g_value_set_string (value, battery->priv->unit);
		break;
	case PROP_TECHNOLOGY:
		g_value_set_string (value, battery->priv->technology);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, battery->priv->vendor);
		break;
	case PROP_MODEL:
		g_value_set_string (value, battery->priv->model);
		break;
	case PROP_PERCENTAGE:
		g_value_set_uint (value, battery->priv->percentage);
		break;
	case PROP_CURRENT_CHARGE:
		g_value_set_uint (value, battery->priv->current_charge);
		break;
	case PROP_LAST_FULL:
		g_value_set_uint (value, battery->priv->last_full);
		break;
	case PROP_REPORTING_DESIGN:
		g_value_set_uint (value, battery->priv->reporting_design);
		break;
	case PROP_REPORTING_LAST_FULL:
		g_value_set_uint (value, battery->priv->reporting_last_full);
		break;	
	case PROP_TIME:
		g_value_set_uint (value, battery->priv->time);
		break;
	default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
            break;
    }
}

static void
hal_battery_finalize(GObject *object)
{
    HalBattery *battery;

    battery = HAL_BATTERY(object);
    
    if ( battery->priv->technology )
    	g_free (battery->priv->technology);
	
    if ( battery->priv->vendor )
    	g_free (battery->priv->vendor);
	
    if ( battery->priv->model )
    	g_free (battery->priv->model);
	
    if ( battery->priv->unit )
    	g_free (battery->priv->unit);

    G_OBJECT_CLASS(hal_battery_parent_class)->finalize(object);
}

static HalDeviceType
hal_battery_type_enum_from_string(const gchar *string)
{
    if ( !g_strcmp0 (string, "primary") )
    {
	return HAL_DEVICE_TYPE_PRIMARY;
    }
    else if ( !g_strcmp0 (string, "ups") )
    {
	return HAL_DEVICE_TYPE_UPS;
    }
    else if ( !g_strcmp0 (string, "mouse") )
    {
	return HAL_DEVICE_TYPE_MOUSE;
    }
    else if ( !g_strcmp0 (string, "keyboard") )
    {
	return HAL_DEVICE_TYPE_KEYBOARD;
    }
    else if ( !g_strcmp0 (string, "camera") )
    {
	return HAL_DEVICE_TYPE_CAMERA;
    }
    else if ( !g_strcmp0 (string, "keyboard_mouse") )
    {
	return HAL_DEVICE_TYPE_KEYBOARD_MOUSE;
    }
    
    return HAL_DEVICE_TYPE_UNKNOWN;
}

static HalDeviceType
hal_battery_get_device_type (HalBattery *battery)
{
    const gchar *udi;
    gchar *type = NULL;
    
    HalDeviceType type_enum = HAL_DEVICE_TYPE_UNKNOWN;
    
    udi = hal_device_get_udi (HAL_DEVICE(battery));
    
    g_return_val_if_fail (udi != NULL, HAL_DEVICE_TYPE_UNKNOWN);
    
    type = hal_device_get_property_string (HAL_DEVICE(battery), "battery.type");
    
    if ( type )
    {
	type_enum  = hal_battery_type_enum_from_string(type);
	g_free(type);
    }
 
    //g_free (udi);
    return type_enum;
}

static guint 
_get_battery_percentage (guint32 last_full, guint32 current)
{
    guint val = 100;
    
    if ( last_full <= current ) return val;
    
    float f = (float)current/last_full *100;
	
	val = (guint)f;
    return val;   
}

static void
hal_battery_refresh_all (HalBattery *battery)
{
    
    battery->priv->is_present = 
    	hal_device_get_property_bool(HAL_DEVICE(battery), "battery.present");
	
    battery->priv->is_charging = 
    	hal_device_get_property_bool(HAL_DEVICE(battery), "battery.rechargeable.is_charging");
    
    battery->priv->is_discharging = 
    	hal_device_get_property_bool(HAL_DEVICE(battery), "battery.rechargeable.is_discharging");
    
    battery->priv->current_charge = 
    	hal_device_get_property_int(HAL_DEVICE(battery), "battery.charge_level.current");
	
    battery->priv->last_full = 
    	hal_device_get_property_int(HAL_DEVICE(battery), "battery.charge_level.last_full");
    
    if ( hal_device_has_key (HAL_DEVICE(battery), "battery.remaining_time") )
    	battery->priv->time = 
    		hal_device_get_property_int(HAL_DEVICE(battery), "battery.remaining_time");
    else
    	battery->priv->time = 0;
    
    //FIXME: calculate the percentage if it is not found on HAL
    if ( hal_device_has_key(HAL_DEVICE(battery), "battery.charge_level.percentage") )
     	battery->priv->percentage = 
    		hal_device_get_property_int(HAL_DEVICE(battery), "battery.charge_level.percentage");
    else battery->priv->percentage = _get_battery_percentage(battery->priv->last_full, battery->priv->current_charge);
    
    if ( hal_device_has_key(HAL_DEVICE(battery), "battery.reporting.last_full") )
     	battery->priv->reporting_last_full = 
    		hal_device_get_property_int(HAL_DEVICE(battery), "battery.reporting.last_full");
		
}

static const gchar *
_translate_technology (const gchar *tech)
{
    if ( !g_strcmp0 (tech, "lithium-ion") )
    {
	return _("Lithium ion");
    }
    else if ( !g_strcmp0 (tech, "lead-acid") )
    {
	return _("Lead acid");
    }
    else if ( !g_strcmp0 (tech, "lithium-polymer") )
    {
	return _("Lithium polymer");
    }
    else if ( !g_strcmp0 (tech, "nickel-metal-hydride") )
    {
	return _("Nickel metal hydride");
    }
    
    return _("Unknown");
}

static const gchar *
_translate_unit (const gchar *unit)
{
    if ( !g_strcmp0 (unit, "mWh") )
    {
	return _("mWh");
    }
    else if ( !g_strcmp0 (unit, "mAh") )
    {
	return _("mAh");
    }
    
    return _("Unknown unit");
}

static void
hal_battery_get_battery_info (HalBattery *battery)
{
    if ( hal_device_has_key (HAL_DEVICE(battery), "battery.technology") )
    {
	gchar *tech = hal_device_get_property_string (HAL_DEVICE(battery), "battery.technology");
	if ( tech )
	{
	    battery->priv->technology = g_strdup (_translate_technology (tech));
	    g_free (tech);
	}
    }
    
    if ( hal_device_has_key (HAL_DEVICE(battery), "battery.vendor") )
    {
	gchar *vendor = hal_device_get_property_string (HAL_DEVICE(battery), "battery.vendor");
	if ( vendor )
	{
	    battery->priv->vendor = g_strdup ( vendor);
	    g_free (vendor);
	}
    }
    
    if ( hal_device_has_key (HAL_DEVICE(battery), "battery.model") )
    {
	gchar *model = hal_device_get_property_string (HAL_DEVICE(battery), "battery.model");
	if ( model )
	{
	    battery->priv->model = g_strdup (model);
	    g_free (model);
	}
    }
    
    battery->priv->reporting_design = hal_device_get_property_int (HAL_DEVICE(battery), 
							                   "battery.reporting.design");
							      
    if ( hal_device_has_key (HAL_DEVICE(battery), "battery.reporting.unit") )
    {
	gchar *unit = hal_device_get_property_string (HAL_DEVICE(battery), "battery.reporting.unit");
	
	if ( unit )
	{
	    battery->priv->unit = g_strdup(_translate_unit(unit));
	    g_free(unit);	    
	}
    }
}

static void
hal_battery_battery_changed_cb (HalBattery *battery, const gchar *key)
{
    if ( !g_strcmp0 (key, "battery.present") ||
    	 !g_strcmp0 (key, "battery.rechargeable.is_charging") ||
	 !g_strcmp0 (key, "battery.rechargeable.is_discharging") ||
	 !g_strcmp0 (key, "battery.charge_level.current")    ||
	 !g_strcmp0 (key, "battery.remaining_time") ||
	 !g_strcmp0 (key, "battery.charge_level.percentage") )
    {
	hal_battery_refresh_all (battery);
    	g_signal_emit (G_OBJECT (battery), signals[BATTERY_CHANGED], 0);
    }
    
}


static void
hal_battery_property_modified_cb(HalBattery *battery, 
			         const gchar *udi,
				 const gchar *key, 
			         gboolean is_removed,
			         gboolean is_added,
				 gpointer data)
{
    hal_battery_battery_changed_cb (battery, key);
}

HalBattery *
hal_battery_new (const gchar *udi)
{
    HalBattery *battery = NULL;
    
    battery = g_object_new (HAL_TYPE_BATTERY, NULL);
    hal_device_set_udi (HAL_DEVICE(battery), udi);
    
    battery->priv->type = hal_battery_get_device_type (battery);
   
    hal_battery_refresh_all (battery);
    hal_battery_get_battery_info (battery);
	
    hal_device_watch (HAL_DEVICE(battery));
    
    g_signal_connect (G_OBJECT(battery), "device-changed",
		      G_CALLBACK(hal_battery_property_modified_cb), battery);
    return battery;
}
