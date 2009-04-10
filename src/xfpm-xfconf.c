/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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
#include <libxfce4util/libxfce4util.h>

#include "libxfpm/xfpm-string.h"
#include "libxfpm/xfpm-common.h"

#include "xfpm-xfconf.h"
#include "xfpm-config.h"
#include "xfpm-enum-glib.h"
#include "xfpm-enum.h"

/* Init */
static void xfpm_xfconf_class_init (XfpmXfconfClass *klass);
static void xfpm_xfconf_init       (XfpmXfconf *conf);
static void xfpm_xfconf_finalize   (GObject *object);

#define XFPM_XFCONF_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_XFCONF, XfpmXfconfPrivate ))

static gpointer xfpm_xfconf_object = NULL;

struct XfpmXfconfPrivate
{
    XfconfChannel 	*channel;
    
    XfpmShutdownRequest  power_button;
    XfpmShutdownRequest  hibernate_button;
    XfpmShutdownRequest  sleep_button;
    XfpmShutdownRequest  lid_button_ac;
    XfpmShutdownRequest  lid_button_battery;
    XfpmShutdownRequest  critical_action;
    
    gboolean             lock_screen;
#ifdef HAVE_DPMS
    gboolean       	 dpms_enabled;
    
    guint16        	 dpms_sleep_on_battery;
    guint16        	 dpms_off_on_battery;
    guint16        	 dpms_sleep_on_ac;
    guint16        	 dpms_off_on_ac;
    
    gboolean         	 sleep_dpms_mode; /*TRUE = standby FALSE = suspend*/
#endif
    gboolean             power_save_on_battery;
    
    guint16              brightness_on_ac_timeout;
    guint16              brightness_on_battery_timeout;
    
    XfpmShowIcon     	 show_icon;
    guint                critical_level;
    gboolean             general_notification;
};

enum
{
    DPMS_SETTINGS_CHANGED,
    POWER_SAVE_SETTINGS_CHANGED,
    BRIGHTNESS_SETTINGS_CHANGED,
    TRAY_ICON_SETTINGS_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmXfconf, xfpm_xfconf, G_TYPE_OBJECT)

static void
xfpm_xfconf_property_changed_cb (XfconfChannel *channel, gchar *property,
				 GValue *value, XfpmXfconf *conf)
{
    const gchar *str;
    gint val;
    
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
        return;

    TRACE("Property modified: %s\n", property);
    
    if ( xfpm_strequal (property, SLEEP_SWITCH_CFG) )
    {
	str = g_value_get_string (value);
 	val = xfpm_shutdown_string_to_int (str); 

	if ( G_UNLIKELY (val == 3) )
	{
	    g_warning ("Invalid value %s for property %s, using default\n", str, SLEEP_SWITCH_CFG);
	    conf->priv->sleep_button = XFPM_DO_NOTHING;
	}
	else
	    conf->priv->sleep_button = val;
    }
    else if ( xfpm_strequal (property, GENERAL_NOTIFICATION_CFG) )
    {
	conf->priv->general_notification = g_value_get_boolean (value);
    }
    else if ( xfpm_strequal (property, POWER_SWITCH_CFG ) )
    {
	str = g_value_get_string (value);
	val = xfpm_shutdown_string_to_int (str);
	if ( G_UNLIKELY (val == -1) )
	{
	    g_warning ("Invalid value %s fpr property %s, using default\n", str, POWER_SWITCH_CFG);
	    conf->priv->power_button = XFPM_DO_NOTHING;
	}
	else
	    conf->priv->power_button = xfpm_shutdown_string_to_int (str);
    }
    else if ( xfpm_strequal (property, HIBERNATE_SWITCH_CFG ) )
    {
	str = g_value_get_string (value);
	val = xfpm_shutdown_string_to_int (str);
	if ( G_UNLIKELY (val == -1) )
	{
	    g_warning ("Invalid value %s fpr property %s, using default\n", str, HIBERNATE_SWITCH_CFG);
	    conf->priv->hibernate_button = XFPM_DO_NOTHING;
	}
	else
	    conf->priv->hibernate_button = xfpm_shutdown_string_to_int (str);
    }
    else if ( xfpm_strequal (property, LID_SWITCH_ON_AC_CFG) )
    {
	str = g_value_get_string (value);
 	val = xfpm_shutdown_string_to_int (str);
	if ( G_UNLIKELY (val == -1 || val == 3) )
	{
	    g_warning ("Invalid value %s for property %s, using default\n", str, LID_SWITCH_ON_AC_CFG);
	    conf->priv->lid_button_ac = XFPM_DO_NOTHING;
	}
	else
	    conf->priv->lid_button_ac = val;
    }
    else if ( xfpm_strequal (property, LID_SWITCH_ON_BATTERY_CFG) )
    {
	str = g_value_get_string (value);
 	val = xfpm_shutdown_string_to_int (str); 
	if ( G_UNLIKELY (val == -1 || val == 3) )
	{
	    g_warning ("Invalid value %s for property %s, using default\n", str, LID_SWITCH_ON_BATTERY_CFG);
	    conf->priv->lid_button_battery = XFPM_DO_NOTHING;
	}
	else
	    conf->priv->lid_button_battery = val;
    }
    else if ( xfpm_strequal (property, LOCK_SCREEN_ON_SLEEP ) )
    {
	conf->priv->lock_screen = g_value_get_boolean (value);
    }
#ifdef HAVE_DPMS
    if ( xfpm_strequal (property, DPMS_ENABLED_CFG) )
    {
	conf->priv->dpms_enabled = g_value_get_boolean (value);
	g_signal_emit (G_OBJECT(conf), signals[DPMS_SETTINGS_CHANGED], 0);
    }
    else if ( xfpm_strequal (property, ON_AC_DPMS_SLEEP) )
    {
	conf->priv->dpms_sleep_on_ac = MIN(3600, g_value_get_uint (value) * 60);
	g_signal_emit (G_OBJECT(conf), signals[DPMS_SETTINGS_CHANGED], 0);
    }
    else if ( xfpm_strequal (property, ON_AC_DPMS_OFF) )
    {
	conf->priv->dpms_off_on_ac = MIN(3600, g_value_get_uint (value) * 60);
	g_signal_emit (G_OBJECT(conf), signals[DPMS_SETTINGS_CHANGED], 0);
    }
    else if ( xfpm_strequal (property, ON_BATT_DPMS_SLEEP) )
    {
	conf->priv->dpms_sleep_on_battery = MIN(3600, g_value_get_uint (value) * 60);
	g_signal_emit (G_OBJECT(conf), signals[DPMS_SETTINGS_CHANGED], 0);
    }
    else if ( xfpm_strequal (property, ON_BATT_DPMS_OFF) )
    {
	conf->priv->dpms_off_on_battery = MIN (3600, g_value_get_uint (value) * 60);
	g_signal_emit (G_OBJECT(conf), signals[DPMS_SETTINGS_CHANGED], 0);
    }
    else if ( xfpm_strequal (property, DPMS_SLEEP_MODE) )
    {
	str = g_value_get_string (value);
	if ( xfpm_strequal (str, "sleep" ) )
	{
	    conf->priv->sleep_dpms_mode = TRUE;
	}
	else if ( xfpm_strequal (str, "suspend") )
	{
	    conf->priv->sleep_dpms_mode = FALSE;
	}
	else
	{
	    g_critical("Invalid value %s for property %s\n", str, DPMS_SLEEP_MODE);
	    conf->priv->sleep_dpms_mode = TRUE;
	}
	g_signal_emit (G_OBJECT(conf), signals[DPMS_SETTINGS_CHANGED], 0);
    }
#endif /* HAVE_DPMS */
    else if ( xfpm_strequal(property, POWER_SAVE_ON_BATTERY) )
    {
	conf->priv->power_save_on_battery = g_value_get_boolean (value);
	g_signal_emit (G_OBJECT(conf), signals[POWER_SAVE_SETTINGS_CHANGED], 0);
    }
    else if ( xfpm_strequal (property, BRIGHTNESS_ON_AC ) )
    {
	conf->priv->brightness_on_ac_timeout = g_value_get_uint (value);
	
	if ( G_UNLIKELY (conf->priv->brightness_on_ac_timeout > 120 || conf->priv->brightness_on_ac_timeout < 9 ))
	{
	    g_warning ("Value %d for %s is out of range", conf->priv->brightness_on_ac_timeout, BRIGHTNESS_ON_AC );
	    conf->priv->brightness_on_ac_timeout = 9;
	}
	g_signal_emit (G_OBJECT(conf), signals[BRIGHTNESS_SETTINGS_CHANGED], 0);
    }
    else if ( xfpm_strequal (property, BRIGHTNESS_ON_BATTERY ) )
    {
	conf->priv->brightness_on_battery_timeout = g_value_get_uint (value);
	
	if ( G_UNLIKELY (conf->priv->brightness_on_battery_timeout > 120 || conf->priv->brightness_on_battery_timeout < 9 ))
	{
	    g_warning ("Value %d for %s is out of range", conf->priv->brightness_on_battery_timeout, BRIGHTNESS_ON_BATTERY );
	    conf->priv->brightness_on_battery_timeout = 9;
	}
	g_signal_emit (G_OBJECT(conf), signals[BRIGHTNESS_SETTINGS_CHANGED], 0);
    }
    else if ( xfpm_strequal (property, CRITICAL_BATT_ACTION_CFG) )
    {
	str = g_value_get_string (value);
	val = xfpm_shutdown_string_to_int (str);
	if ( G_UNLIKELY (val == -1 || val == 1 ))
	{
	    g_warning ("Invalid value %s for property %s, using default\n", str, CRITICAL_BATT_ACTION_CFG);
	    conf->priv->critical_action = XFPM_DO_NOTHING;
	}
	else
	    conf->priv->critical_action = val;
    }
    else if ( xfpm_strequal (property, SHOW_TRAY_ICON_CFG) )
    {
	conf->priv->show_icon = g_value_get_uint (value);
	g_signal_emit (G_OBJECT(conf), signals[TRAY_ICON_SETTINGS_CHANGED], 0 );
    }
    else if ( xfpm_strequal( property, CRITICAL_POWER_LEVEL) )
    {
	val = g_value_get_uint (value);
	if ( G_UNLIKELY (val > 20) )
	{
	    g_warning ("Value %d for property %s is out of range \n", val, CRITICAL_POWER_LEVEL);
	    conf->priv->critical_level = 10;
	}
	else 
	    conf->priv->critical_level = val;
    }
}

static void
xfpm_xfconf_load_configuration (XfpmXfconf *conf)
{
    gchar *str;
    gint val;
    
    str = xfconf_channel_get_string (conf->priv->channel, SLEEP_SWITCH_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);
    
    if ( G_UNLIKELY (val == -1 || val == 3) )
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, SLEEP_SWITCH_CFG);
	conf->priv->sleep_button = XFPM_DO_NOTHING;
	xfconf_channel_set_string (conf->priv->channel, SLEEP_SWITCH_CFG, "Nothing");
    }
    else conf->priv->sleep_button = val;
    
    g_free (str);
    
    str = xfconf_channel_get_string (conf->priv->channel, POWER_SWITCH_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);
    
    if ( G_UNLIKELY (val == -1 ) )
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, SLEEP_SWITCH_CFG);
	conf->priv->power_button = XFPM_DO_NOTHING;
	xfconf_channel_set_string (conf->priv->channel, POWER_SWITCH_CFG, "Nothing");
    }
    else conf->priv->power_button = val;
    
    g_free (str);
    
    str = xfconf_channel_get_string (conf->priv->channel, HIBERNATE_SWITCH_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);
    
    if ( G_UNLIKELY (val == -1 ) )
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, HIBERNATE_SWITCH_CFG);
	conf->priv->hibernate_button = XFPM_DO_NOTHING;
	xfconf_channel_set_string (conf->priv->channel, HIBERNATE_SWITCH_CFG, "Nothing");
    }
    else conf->priv->hibernate_button = val;
    
    g_free (str);
    
    conf->priv->general_notification = xfconf_channel_get_bool (conf->priv->channel, GENERAL_NOTIFICATION_CFG, TRUE);
    
    str = xfconf_channel_get_string (conf->priv->channel, LID_SWITCH_ON_AC_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);

    if ( G_UNLIKELY (val == -1 || val == 3) )
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, LID_SWITCH_ON_AC_CFG);
	conf->priv->lid_button_ac = XFPM_DO_NOTHING;
	xfconf_channel_set_string (conf->priv->channel, LID_SWITCH_ON_AC_CFG, "Nothing");
    }
    else conf->priv->lid_button_ac = val;
    
    g_free (str);
    
    str = xfconf_channel_get_string (conf->priv->channel, LID_SWITCH_ON_BATTERY_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);
    
    if ( G_UNLIKELY (val == -1 || val == 3) )
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, LID_SWITCH_ON_BATTERY_CFG);
	conf->priv->lid_button_battery = XFPM_DO_NOTHING;
	xfconf_channel_set_string (conf->priv->channel, LID_SWITCH_ON_BATTERY_CFG, "Nothing");
    }
    else conf->priv->lid_button_battery = val;
    
    g_free (str);
    
    conf->priv->lock_screen = xfconf_channel_get_bool (conf->priv->channel, LOCK_SCREEN_ON_SLEEP, TRUE);
    
#ifdef HAVE_DPMS
    conf->priv->dpms_enabled =
    	xfconf_channel_get_bool (conf->priv->channel, DPMS_ENABLED_CFG, TRUE);
    
    conf->priv->dpms_sleep_on_battery = 
    	MIN( xfconf_channel_get_uint( conf->priv->channel, ON_BATT_DPMS_SLEEP, 3) * 60, 3600);

    conf->priv->dpms_off_on_battery = 
    	MIN(xfconf_channel_get_uint( conf->priv->channel, ON_BATT_DPMS_OFF, 5) * 60, 3600);
	
    conf->priv->dpms_sleep_on_ac = 
    	MIN(xfconf_channel_get_uint( conf->priv->channel, ON_AC_DPMS_SLEEP, 10) * 60, 3600);
    
    conf->priv->dpms_off_on_ac = 
    	MIN(xfconf_channel_get_uint( conf->priv->channel, ON_AC_DPMS_OFF, 15) * 60, 3600);
	
    str = xfconf_channel_get_string (conf->priv->channel, DPMS_SLEEP_MODE, "sleep");
    
    if ( xfpm_strequal (str, "sleep" ) )
    {
	conf->priv->sleep_dpms_mode = 0;
    }
    else if ( xfpm_strequal (str, "suspend") )
    {
	conf->priv->sleep_dpms_mode = 1;
    }
    else
    {
	g_critical("Invalid value %s for property %s\n", str, DPMS_SLEEP_MODE);
	conf->priv->sleep_dpms_mode = 0;
    }
    g_free (str);
#endif /* HAVE_DPMS */
    conf->priv->power_save_on_battery =
    	xfconf_channel_get_bool (conf->priv->channel, POWER_SAVE_ON_BATTERY, TRUE);
	
    conf->priv->brightness_on_ac_timeout =
	xfconf_channel_get_uint (conf->priv->channel, BRIGHTNESS_ON_AC, 9);
	
    if ( G_UNLIKELY (conf->priv->brightness_on_ac_timeout > 120 || conf->priv->brightness_on_ac_timeout < 9 ))
    {
	g_warning ("Value %d for %s is out of range", conf->priv->brightness_on_ac_timeout, BRIGHTNESS_ON_AC );
	conf->priv->brightness_on_ac_timeout = 9;
    }
    
    conf->priv->brightness_on_battery_timeout =
	xfconf_channel_get_uint (conf->priv->channel, BRIGHTNESS_ON_BATTERY, 10);
	
    if ( G_UNLIKELY (conf->priv->brightness_on_battery_timeout > 120 || conf->priv->brightness_on_battery_timeout < 9) )
    {
	g_warning ("Value %d for %s is out of range", conf->priv->brightness_on_battery_timeout, BRIGHTNESS_ON_BATTERY );
	conf->priv->brightness_on_battery_timeout = 10;
    }
    
    //FIXME: check if valid
    str = xfconf_channel_get_string (conf->priv->channel, CRITICAL_BATT_ACTION_CFG, "Nothing");
    conf->priv->critical_action = xfpm_shutdown_string_to_int (str);
    g_free (str);
    
    conf->priv->show_icon =
    	xfconf_channel_get_uint (conf->priv->channel, SHOW_TRAY_ICON_CFG, SHOW_ICON_WHEN_BATTERY_PRESENT);
    if ( G_UNLIKELY (conf->priv->show_icon < 0 || conf->priv->show_icon > 3) )
    {
	g_warning ("Invalid value %d for property %s, using default\n", conf->priv->show_icon, SHOW_TRAY_ICON_CFG);
	xfconf_channel_set_uint (conf->priv->channel, CRITICAL_BATT_ACTION_CFG, SHOW_ICON_WHEN_BATTERY_PRESENT);
    }
    
    conf->priv->critical_level =
	xfconf_channel_get_uint (conf->priv->channel, CRITICAL_POWER_LEVEL, 0);
	
    if ( G_UNLIKELY (conf->priv->critical_level <0 || conf->priv->critical_level > 20) )
    {
	g_warning ("Value %d for property %s is out of range \n", conf->priv->critical_level, CRITICAL_POWER_LEVEL);
	conf->priv->critical_level = 10;
    }
}

static void
xfpm_xfconf_class_init (XfpmXfconfClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    signals[DPMS_SETTINGS_CHANGED] =
	    g_signal_new("dpms-settings-changed",
			 XFPM_TYPE_XFCONF,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmXfconfClass, dpms_settings_changed),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE, 0, G_TYPE_NONE);
    
     signals[POWER_SAVE_SETTINGS_CHANGED] =
	    g_signal_new("power-save-settings-changed",
			 XFPM_TYPE_XFCONF,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmXfconfClass, power_save_settings_changed),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE, 0, G_TYPE_NONE);
			 
     signals[BRIGHTNESS_SETTINGS_CHANGED] =
	    g_signal_new("brightness-settings-changed",
			 XFPM_TYPE_XFCONF,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmXfconfClass, brightness_settings_changed),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE, 0, G_TYPE_NONE);
    
    signals[TRAY_ICON_SETTINGS_CHANGED] =
	    g_signal_new("tray-icon-settings-changed",
			 XFPM_TYPE_XFCONF,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmXfconfClass, tray_icon_settings_changed),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE, 0, G_TYPE_NONE);
    
    object_class->finalize = xfpm_xfconf_finalize;
    g_type_class_add_private (klass, sizeof(XfpmXfconfPrivate));
}

static void
xfpm_xfconf_init (XfpmXfconf *conf)
{
    GError *error = NULL;
      
    conf->priv = XFPM_XFCONF_GET_PRIVATE (conf);
    
    if ( !xfconf_init(&error) )
    {
    	g_critical ("xfconf_init failed: %s\n", error->message);
       	g_error_free (error);
    }	
    else
    {
    
	conf->priv->channel = xfconf_channel_new ("xfce4-power-manager");

	g_signal_connect (conf->priv->channel, "property-changed",
			  G_CALLBACK (xfpm_xfconf_property_changed_cb), conf);
	xfpm_xfconf_load_configuration (conf);
    }
    conf->channel = xfconf_channel_new ("xfce4-power-manager");
}

static void
xfpm_xfconf_finalize(GObject *object)
{
    XfpmXfconf *conf;
    
    conf = XFPM_XFCONF(object);
    
    if (conf->priv->channel )
	g_object_unref (conf->priv->channel);
    
    G_OBJECT_CLASS(xfpm_xfconf_parent_class)->finalize(object);
}

XfpmXfconf *
xfpm_xfconf_new(void)
{
    if ( xfpm_xfconf_object != NULL )
    {
	g_object_ref (xfpm_xfconf_object);
    } 
    else
    {
	xfpm_xfconf_object = g_object_new (XFPM_TYPE_XFCONF, NULL);
	g_object_add_weak_pointer (xfpm_xfconf_object, &xfpm_xfconf_object);
    }
    return XFPM_XFCONF (xfpm_xfconf_object);
}

gboolean xfpm_xfconf_get_property_bool (XfpmXfconf *conf, const gchar *property)
{
    g_return_val_if_fail (XFPM_IS_XFCONF(conf), FALSE);
    
    if ( xfpm_strequal (property, LOCK_SCREEN_ON_SLEEP))
	return conf->priv->lock_screen;
    else if ( xfpm_strequal (property, POWER_SAVE_ON_BATTERY ) )
	return conf->priv->power_save_on_battery;
    else if ( xfpm_strequal (property, GENERAL_NOTIFICATION_CFG ) )
	return conf->priv->general_notification;
#ifdef HAVE_DPMS
    else if ( xfpm_strequal (property, DPMS_SLEEP_MODE ))
	return conf->priv->sleep_dpms_mode;
    else if ( xfpm_strequal (property, DPMS_ENABLED_CFG) )
	return conf->priv->dpms_enabled;
#endif /* HAVE_DPMS */
    
    g_warn_if_reached ();

    return FALSE;
}

guint8 xfpm_xfconf_get_property_enum (XfpmXfconf *conf, const gchar *property)
{
    g_return_val_if_fail (XFPM_IS_XFCONF(conf), 0);

    if ( xfpm_strequal (property, LID_SWITCH_ON_AC_CFG) )
	return conf->priv->lid_button_ac;
    else if ( xfpm_strequal (property, LID_SWITCH_ON_BATTERY_CFG) )
	return conf->priv->lid_button_battery;
    else if ( xfpm_strequal (property, SLEEP_SWITCH_CFG) )
	return conf->priv->sleep_button;
    else if ( xfpm_strequal (property, CRITICAL_BATT_ACTION_CFG) )
	return conf->priv->critical_action;
    else if ( xfpm_strequal (property, SHOW_TRAY_ICON_CFG ) )
	return conf->priv->show_icon;
    else if ( xfpm_strequal (property, POWER_SWITCH_CFG) )
	return conf->priv->power_button;
    else if ( xfpm_strequal (property, HIBERNATE_SWITCH_CFG ) )
	return conf->priv->hibernate_button;
    
    g_warn_if_reached ();

    return 0;
}

gint xfpm_xfconf_get_property_int (XfpmXfconf *conf, const gchar *property)
{
    g_return_val_if_fail (XFPM_IS_XFCONF(conf), 0);

#ifdef HAVE_DPMS
    if ( xfpm_strequal (property, ON_AC_DPMS_SLEEP))
	return conf->priv->dpms_sleep_on_ac;
    else if ( xfpm_strequal (property, ON_BATT_DPMS_SLEEP))
	return conf->priv->dpms_sleep_on_battery;
    else if ( xfpm_strequal (property, ON_AC_DPMS_OFF))
	return conf->priv->dpms_off_on_ac;
    else if ( xfpm_strequal (property, ON_BATT_DPMS_OFF))
	return conf->priv->dpms_off_on_battery;
    else if ( xfpm_strequal (property, BRIGHTNESS_ON_AC ) )
	return conf->priv->brightness_on_ac_timeout;
    else if ( xfpm_strequal (property, BRIGHTNESS_ON_BATTERY )) 
	return conf->priv->brightness_on_battery_timeout;
    else if ( xfpm_strequal (property, CRITICAL_POWER_LEVEL) )
	return conf->priv->critical_level;
#endif /* HAVE_DPMS */

    g_warn_if_reached ();

    return 0;
}
