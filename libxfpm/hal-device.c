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
#include <glib/gi18n.h>

#include "hal-device.h"
#include "hal-ctx.h"
#include "xfpm-string.h"
#include "hal-enum.h"

/* Init */
static void hal_device_class_init (HalDeviceClass *klass);
static void hal_device_init       (HalDevice *device);
static void hal_device_finalize   (GObject *object);

static void hal_device_get_property(GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec);

#define HAL_DEVICE_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_DEVICE, HalDevicePrivate))

struct HalDevicePrivate
{
    HalCtx *ctx;
    
    /* Properties read-only */
    HalDeviceType type;
    
    gboolean  is_present;
    gboolean  is_charging;
    gboolean  is_discharging;
    
    gchar    *unit;
    gchar    *technology;
    gchar    *udi;
    gchar    *vendor;
    gchar    *model;
    guint     percentage;

    guint32   current_charge;
    guint32   last_full;
    
    guint32   reporting_design;
    guint32   reporting_last_full;
    
    gint      time;
    
};

enum
{
    PROP_0,
    PROP_TYPE,
    PROP_IS_PRESENT,
    PROP_IS_CHARGING,
    PROP_IS_DISCHARGING,
    PROP_UDI,
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
    DEVICE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(HalDevice, hal_device, G_TYPE_OBJECT)

static void
hal_device_class_init(HalDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hal_device_get_property;
    object_class->finalize = hal_device_finalize;

    signals[DEVICE_CHANGED] =
    	g_signal_new("device-changed",
		     HAL_TYPE_DEVICE,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HalDeviceClass, device_changed),
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
				    PROP_UDI,
				    g_param_spec_string("udi",
							NULL, NULL,
						        NULL,
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
							 FALSE,
							 G_PARAM_READABLE));
							 
    g_type_class_add_private(klass,sizeof(HalDevicePrivate));
}

static void
hal_device_init(HalDevice *device)
{
    device->priv = HAL_DEVICE_GET_PRIVATE(device);
    
    device->priv->ctx = hal_ctx_new();
    
    device->priv->is_present      = FALSE;
    device->priv->is_charging     = FALSE;
    device->priv->is_discharging  = FALSE;

    device->priv->unit            = NULL;
    device->priv->vendor          = NULL;
    device->priv->technology      = NULL;
    device->priv->udi             = NULL;
    device->priv->model           = NULL;
    device->priv->type            = HAL_DEVICE_TYPE_UNKNOWN;
    
    device->priv->percentage      = 0;
    device->priv->current_charge  = 0;
    device->priv->last_full       = 0;
    device->priv->time            = 0;
    device->priv->reporting_design = 0;
    device->priv->reporting_last_full = 0;
}

static void hal_device_get_property(GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec)
{
    HalDevice *device;
    device = HAL_DEVICE(object);

    switch (prop_id)
    {
	case PROP_TYPE:
		g_value_set_uint (value, device->priv->type);
		break;
	case PROP_IS_PRESENT:
		g_value_set_boolean (value, device->priv->is_present);
		break;
	case PROP_IS_CHARGING:
		g_value_set_boolean (value, device->priv->is_charging);
		break;
	case PROP_IS_DISCHARGING:
		g_value_set_boolean (value, device->priv->is_discharging);
		break;
	case PROP_UNIT:
		g_value_set_string (value, device->priv->unit);
		break;
 	case PROP_UDI:
		g_value_set_string (value, device->priv->udi);
		break;
	case PROP_TECHNOLOGY:
		g_value_set_string (value, device->priv->technology);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, device->priv->vendor);
		break;
	case PROP_MODEL:
		g_value_set_string (value, device->priv->model);
		break;
	case PROP_PERCENTAGE:
		g_value_set_uint (value, device->priv->percentage);
		break;
	case PROP_CURRENT_CHARGE:
		g_value_set_uint (value, device->priv->current_charge);
		break;
	case PROP_LAST_FULL:
		g_value_set_uint (value, device->priv->last_full);
		break;
	case PROP_REPORTING_DESIGN:
		g_value_set_uint (value, device->priv->reporting_design);
		break;
	case PROP_REPORTING_LAST_FULL:
		g_value_set_uint (value, device->priv->reporting_last_full);
		break;	
	case PROP_TIME:
		g_value_set_int (value, device->priv->time);
		break;
	default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
            break;
    }
}

static void
hal_device_finalize(GObject *object)
{
    HalDevice *device;

    device = HAL_DEVICE(object);
    
    if ( device->priv->ctx )
    	g_object_unref(device->priv->ctx);
	
    if ( device->priv->udi)
    	g_free(device->priv->udi);
	
    if ( device->priv->technology )
    	g_free (device->priv->technology);
	
    if ( device->priv->vendor )
    	g_free (device->priv->vendor);
	
    if ( device->priv->model )
    	g_free (device->priv->model);
	
    if ( device->priv->unit )
    	g_free (device->priv->unit);

    G_OBJECT_CLASS(hal_device_parent_class)->finalize(object);
}

static HalDeviceType
hal_device_type_enum_from_string(const gchar *string)
{
    if ( xfpm_strequal (string, "primary") )
    {
	return HAL_DEVICE_TYPE_PRIMARY;
    }
    else if ( xfpm_strequal (string, "ups") )
    {
	return HAL_DEVICE_TYPE_UPS;
    }
    else if ( xfpm_strequal (string, "mouse") )
    {
	return HAL_DEVICE_TYPE_MOUSE;
    }
    else if ( xfpm_strequal (string, "keyboard") )
    {
	return HAL_DEVICE_TYPE_KEYBOARD;
    }
    else if ( xfpm_strequal (string, "camera") )
    {
	return HAL_DEVICE_TYPE_CAMERA;
    }
    else if ( xfpm_strequal (string, "keyboard_mouse") )
    {
	return HAL_DEVICE_TYPE_KEYBOARD_MOUSE;
    }
    
    return HAL_DEVICE_TYPE_UNKNOWN;
}

static HalDeviceType
hal_device_get_device_type(HalDevice *device)
{
    if ( hal_ctx_device_has_capability(device->priv->ctx, device->priv->udi, "battery") )
    {
	gchar *type = hal_ctx_get_property_string(device->priv->ctx, device->priv->udi, "battery.type");
	if ( type )
	{
	    HalDeviceType type_enum  = hal_device_type_enum_from_string(type);
	    g_free(type);
	    return type_enum;
	}
	else 
		goto out;
    }
    else if ( hal_ctx_device_has_capability(device->priv->ctx, device->priv->udi, "ac_adapter") )
    {
	return HAL_DEVICE_TYPE_ADAPTER;
    }
    
out:
    return HAL_DEVICE_TYPE_UNKNOWN;
}

static void
hal_device_refresh_all_adapter (HalDevice *device)
{
    device->priv->is_present =
    	hal_ctx_get_property_bool(device->priv->ctx, device->priv->udi, "ac_adapter.present");
    
}

static void
hal_device_refresh_all_battery (HalDevice *device)
{
    device->priv->is_present = 
    	hal_ctx_get_property_bool(device->priv->ctx, device->priv->udi, "battery.present");
	
    device->priv->is_charging = 
    	hal_ctx_get_property_bool(device->priv->ctx, device->priv->udi, "battery.rechargeable.is_charging");
    
    device->priv->is_discharging = 
    	hal_ctx_get_property_bool(device->priv->ctx, device->priv->udi, "battery.rechargeable.is_discharging");
    
    device->priv->current_charge = 
    	hal_ctx_get_property_int(device->priv->ctx, device->priv->udi, "battery.charge_level.current");
	
    device->priv->last_full = 
    	hal_ctx_get_property_int(device->priv->ctx, device->priv->udi, "battery.charge_level.last_full");
    
    if ( hal_ctx_device_has_key(device->priv->ctx, device->priv->udi, "battery.remaining_time") )
    	device->priv->time = 
    		hal_ctx_get_property_int(device->priv->ctx, device->priv->udi, "battery.remaining_time");
    else
    	device->priv->time = 0;
    
    //FIXME: calculate the percentage if it is not found on HAL
    if ( hal_ctx_device_has_key(device->priv->ctx, device->priv->udi, "battery.charge_level.percentage") )
     	device->priv->percentage = 
    		hal_ctx_get_property_int(device->priv->ctx, device->priv->udi, "battery.charge_level.percentage");
    else device->priv->percentage = 0;
    
    if ( hal_ctx_device_has_key(device->priv->ctx, device->priv->udi, "battery.reporting.last_full") )
     	device->priv->reporting_last_full = 
    		hal_ctx_get_property_int(device->priv->ctx, device->priv->udi, "battery.reporting.last_full");
		
}

static const gchar *
_translate_technology (const gchar *tech)
{
    if ( xfpm_strequal (tech, "lithium-ion") )
    {
	return _("Lithium ion");
    }
    else if ( xfpm_strequal (tech, "lead-acid") )
    {
	return _("Lead acid");
    }
    else if ( xfpm_strequal (tech, "lithium-polymer") )
    {
	return _("Lithium polymer");
    }
    else if ( xfpm_strequal (tech, "nickel-metal-hydride") )
    {
	return _("Nickel metal hydride");
    }
    
    return _("Unknown");
}

static const gchar *
_translate_unit (const gchar *unit)
{
    if ( xfpm_strequal (unit, "mWh") )
    {
	return _("mWh");
    }
    else if ( xfpm_strequal (unit, "mAh") )
    {
	return _("mAh");
    }
    
    return _("Unknown unit");
}

static void
hal_device_get_battery_info (HalDevice *device)
{
    if ( hal_ctx_device_has_key (device->priv->ctx, device->priv->udi, "battery.technology") )
    {
	gchar *tech = hal_ctx_get_property_string (device->priv->ctx, device->priv->udi, "battery.technology");
	if ( tech )
	{
	    device->priv->technology = g_strdup (_translate_technology (tech));
	    g_free (tech);
	}
    }
    
    if ( hal_ctx_device_has_key (device->priv->ctx, device->priv->udi, "battery.vendor") )
    {
	gchar *vendor = hal_ctx_get_property_string (device->priv->ctx, device->priv->udi, "battery.vendor");
	if ( vendor )
	{
	    device->priv->vendor = g_strdup ( vendor);
	    g_free (vendor);
	}
    }
    
    if ( hal_ctx_device_has_key (device->priv->ctx, device->priv->udi, "battery.model") )
    {
	gchar *model = hal_ctx_get_property_string (device->priv->ctx, device->priv->udi, "battery.model");
	if ( model )
	{
	    device->priv->model = g_strdup (model);
	    g_free (model);
	}
    }
    
    device->priv->reporting_design = hal_ctx_get_property_int (device->priv->ctx, 
							      device->priv->udi, 
							      "battery.reporting.design");
							      
    if ( hal_ctx_device_has_key (device->priv->ctx, device->priv->udi, "battery.reporting.unit") )
    {
	gchar *unit = hal_ctx_get_property_string (device->priv->ctx, device->priv->udi, "battery.reporting.unit");
	
	if ( unit )
	{
	    device->priv->unit = g_strdup(_translate_unit(unit));
	    g_free(unit);	    
	}
    }
}

static void
hal_device_adapter_changed_cb (HalDevice *device, const gchar *key)
{
    if ( xfpm_strequal (key, "ac_adapter.present") )
    {
    	hal_device_refresh_all_adapter(device);
    	g_signal_emit(G_OBJECT(device), signals[DEVICE_CHANGED], 0);
    }
    
}

static void
hal_device_battery_changed_cb (HalDevice *device, const gchar *key)
{
    if ( xfpm_strequal (key, "battery.present") ||
    	 xfpm_strequal (key, "battery.rechargeable.is_charging") ||
	 xfpm_strequal (key, "battery.rechargeable.is_discharging") ||
	 xfpm_strequal (key, "battery.charge_level.current")    ||
	 xfpm_strequal (key, "battery.remaining_time") ||
	 xfpm_strequal (key, "battery.charge_level.percentage") )
    {
	hal_device_refresh_all_battery(device);
    	g_signal_emit(G_OBJECT(device), signals[DEVICE_CHANGED], 0);
    }
    
}


static void
hal_device_property_modified(LibHalContext *ctx, 
			     const gchar *udi,
			     const gchar *key, 
			     dbus_bool_t is_removed,
			     dbus_bool_t is_added)
{
    HalDevice *device = libhal_ctx_get_user_data(ctx);
    
    if ( device->priv->type == HAL_DEVICE_TYPE_ADAPTER )
    	hal_device_adapter_changed_cb (device, key);
    else
    	hal_device_battery_changed_cb (device, key);
}

HalDevice *
hal_device_new(const gchar *udi)
{
    HalDevice *device = NULL;
    device = g_object_new(HAL_TYPE_DEVICE,NULL);
    
    device->priv->udi = g_strdup(udi);
    
    if ( !hal_ctx_connect(device->priv->ctx)) goto out;
    
    device->priv->type = hal_device_get_device_type(device);
   
    if ( device->priv->type == HAL_DEVICE_TYPE_ADAPTER )
    	hal_device_refresh_all_adapter(device);
    else
    {
    	hal_device_refresh_all_battery (device);
	hal_device_get_battery_info    (device);
    }
	
    hal_ctx_set_user_data(device->priv->ctx, device);
    hal_ctx_set_device_property_callback(device->priv->ctx, 
    					 hal_device_property_modified);
    hal_ctx_watch_device(device->priv->ctx, device->priv->udi);
    
out:
    return device;
}
