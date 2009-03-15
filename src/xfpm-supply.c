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
#include "libxfpm/hal-iface.h"
#include "libxfpm/xfpm-string.h"
#include "libxfpm/xfpm-common.h"

#include "xfpm-supply.h"
#include "xfpm-battery.h"
#include "xfpm-adapter.h"
#include "xfpm-notify.h"
#include "xfpm-enum.h"
#include "xfpm-enum-types.h"
#include "xfpm-xfconf.h"
#include "xfpm-config.h"

/* Init */
static void xfpm_supply_class_init (XfpmSupplyClass *klass);
static void xfpm_supply_init       (XfpmSupply *xfpm_supply);
static void xfpm_supply_finalize   (GObject *object);

#define XFPM_SUPPLY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_SUPPLY, XfpmSupplyPrivate))

struct XfpmSupplyPrivate
{
    XfpmNotify    *notify;
    XfpmAdapter   *adapter;
    XfpmXfconf    *conf;
    
    HalPower      *power;
    GHashTable    *hash;
    
    XfpmShutdownRequest critical_action;
    XfpmShowIcon   show_icon;
    
    gboolean 	   adapter_found;
    gboolean       adapter_present;

    guint8         critical_level;
    guint8         power_management;
};

enum
{
    SHUTDOWN_REQUEST,
    BLOCK_SHUTDOWN,
    ON_BATTERY,
    ON_LOW_BATTERY,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmSupply, xfpm_supply, G_TYPE_OBJECT)

static void
xfpm_supply_class_init(XfpmSupplyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    signals[SHUTDOWN_REQUEST] = 
    	g_signal_new("shutdown-request",
                      XFPM_TYPE_SUPPLY,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmSupplyClass, shutdown_request),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__ENUM,
                      G_TYPE_NONE, 1, XFPM_TYPE_SHUTDOWN_REQUEST);

    signals[BLOCK_SHUTDOWN] = 
    	g_signal_new("block-shutdown",
                      XFPM_TYPE_SUPPLY,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmSupplyClass, block_shutdown),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
    
    signals[ON_BATTERY] = 
    	g_signal_new("on-battery",
                      XFPM_TYPE_SUPPLY,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmSupplyClass, on_battery),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
		      
    signals[ON_LOW_BATTERY] = 
    	g_signal_new("on-low-battery",
                      XFPM_TYPE_SUPPLY,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmSupplyClass, on_low_battery),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);
		      
    object_class->finalize = xfpm_supply_finalize;

    g_type_class_add_private(klass,sizeof(XfpmSupplyPrivate));
}

static void
xfpm_supply_init (XfpmSupply *supply)
{
    supply->priv = XFPM_SUPPLY_GET_PRIVATE (supply);
  
    supply->priv->power   = hal_power_new ();
    supply->priv->hash    = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    supply->priv->notify  = xfpm_notify_new ();
    supply->priv->conf    = xfpm_xfconf_new ();
    
    supply->priv->adapter = NULL;
    supply->priv->adapter_found = FALSE;
}

static void
xfpm_supply_finalize (GObject *object)
{
    XfpmSupply *supply;
    supply = XFPM_SUPPLY(object);
    
    if ( supply->priv->power )
    	g_object_unref(supply->priv->power);
	
    if ( supply->priv->hash )
    	g_hash_table_destroy (supply->priv->hash);
	
    if ( supply->priv->notify )
    	g_object_unref (supply->priv->notify);
	
    if ( supply->priv->conf )
    	g_object_unref (supply->priv->conf);
    
    G_OBJECT_CLASS(xfpm_supply_parent_class)->finalize(object);
}

static void
xfpm_supply_hibernate_cb (GtkWidget *w, XfpmSupply *supply)
{
    gboolean ret = 
    xfce_confirm (_("Are you sure you want to hibernate the system?"),
                  GTK_STOCK_YES,
                  _("Hibernate"));
    
    if ( ret ) 
    {
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, XFPM_DO_HIBERNATE);
    }
}

static void
xfpm_supply_suspend_cb (GtkWidget *w, XfpmSupply *supply)
{
    gboolean ret = 
    xfce_confirm (_("Are you sure you want to suspend the system?"),
                  GTK_STOCK_YES,
                  _("Suspend"));
    
    if ( ret ) 
    {
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, XFPM_DO_SUSPEND);
    }
}

//FIXME: more types
static const gchar *
_get_icon_name_from_battery_type (HalDeviceType type)
{
    switch (type)
    {
	case HAL_DEVICE_TYPE_PRIMARY:
	    return "gpm-primary-charged";
	    break;
	case HAL_DEVICE_TYPE_UPS:
	    return "gpm-ups-charged";
	    break;
	case HAL_DEVICE_TYPE_MOUSE:
	    return "gpm-mouse-100";
	    break;
	case HAL_DEVICE_TYPE_KEYBOARD:
	    return "gpm-keyboard-100";
	    break;
	case HAL_DEVICE_TYPE_PDA:
	    return "gpm-phone-100";
	    break;
	default:
	    return "gpm-primary-charged";
	    break;
    }
}

gboolean xfpm_supply_on_low_power ( XfpmSupply *supply)
{
    GList *list = NULL;
    int i;
    gboolean low_power = FALSE;
    list = g_hash_table_get_values (supply->priv->hash);
    
    if ( !list)
	return FALSE;
	
    for ( i=0; i< g_list_length(list); i++)
    {
	XfpmBattery *battery = NULL;
	HalDeviceType type;
	guint percentage;
	battery = g_list_nth_data(list, i);
	
	if ( !battery )
	    continue;
	    
	const HalBattery *device = xfpm_battery_get_device (battery);
	g_object_get (G_OBJECT(device), "type", &type, "percentage", &percentage, NULL);
	if ( type != HAL_DEVICE_TYPE_PRIMARY )
	    continue;
	    
	if ( percentage < 10 ) //FIXME: make this configurable
	    low_power = TRUE;
	else 
	    low_power = FALSE;
    }
    
    return low_power;
}

static const gchar *
xfpm_supply_get_message_from_battery_state (XfpmBatteryState state, gboolean adapter_present)
{
    switch (state)
    {
	case BATTERY_FULLY_CHARGED:
	    return _("Your battery is fully charged");
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
xfpm_supply_process_critical_action (XfpmSupply *supply)
{
    //FIXME: shouldn't happen
    g_return_if_fail (supply->priv->critical_action != XFPM_DO_SUSPEND );
    
    g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, supply->priv->critical_action);
}

static void
_notify_action_callback (NotifyNotification *n, gchar *action, XfpmSupply *supply)
{
    if ( xfpm_strequal(action, "shutdow") )
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, XFPM_DO_SHUTDOWN);
    else if ( xfpm_strequal(action, "hibernate") )
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, XFPM_DO_SHUTDOWN);
}

static void
xfpm_supply_show_critical_action (XfpmSupply *supply, XfpmBattery *battery)
{
    const gchar *message;
    message = _("Your battery is almost empty. "\
              "Save your work to avoid losing data");
	      
    NotifyNotification *n = 
	xfpm_notify_new_notification (supply->priv->notify, 
				      _("Xfce power manager"), 
				      message, 
				      xfpm_battery_get_icon_name (battery),
				      10000,
				      XFPM_NOTIFY_CRITICAL,
				      xfpm_battery_get_status_icon (battery));
				   
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
    
    xfpm_notify_present_notification (supply->priv->notify, n, FALSE);
    
}

static void
xfpm_supply_handle_primary_critical (XfpmSupply *supply, XfpmBattery *battery)
{
    if ( xfpm_supply_on_low_power (supply) )
    {
	TRACE ("System is running on low power");
	if ( supply->priv->critical_action == XFPM_DO_NOTHING )
	{
	    xfpm_supply_show_critical_action (supply, battery);
	}
	else
	{
	    xfpm_supply_process_critical_action (supply);
	}
    }
    else 
    {
	const gchar *message = _("Your battery is almost empty");
	xfpm_notify_show_notification (supply->priv->notify, 
				   _("Xfce power manager"), 
				   message, 
				   xfpm_battery_get_icon_name (battery),
				   10000,
				   FALSE,
				   XFPM_NOTIFY_NORMAL,
				   xfpm_battery_get_status_icon (battery));
    }
}

static void
xfpm_supply_primary_battery_changed (XfpmSupply *supply, XfpmBattery *battery, XfpmBatteryState state)
{
    if ( state == BATTERY_CHARGE_CRITICAL )
    {
	xfpm_supply_handle_primary_critical (supply, battery);
	return;
    }
    
    const gchar *message 
    	= xfpm_supply_get_message_from_battery_state (state, supply->priv->adapter_found ? 
							     supply->priv->adapter_present : TRUE); // FIXME, TRUE makes sense here ?

    if ( !message )
    	return;
    
    xfpm_notify_show_notification (supply->priv->notify, 
				   _("Xfce power manager"), 
				   message, 
				   xfpm_battery_get_icon_name (battery),
				   10000,
				   FALSE,
				   XFPM_NOTIFY_NORMAL,
				   xfpm_battery_get_status_icon (battery));
}

static void
xfpm_supply_misc_battery_changed (XfpmSupply *supply, XfpmBattery *battery, XfpmBatteryState state)
{
    const gchar *message 
    	= xfpm_supply_get_message_from_battery_state (state, TRUE);
	
    if ( !message )
    	return;
    
    xfpm_notify_show_notification (supply->priv->notify, 
				   _("Xfce power manager"), 
				   message, 
				   xfpm_battery_get_icon_name (battery),
				   10000,
				   TRUE, 
				   XFPM_NOTIFY_NORMAL,
				   xfpm_battery_get_status_icon (battery));
}

static void
xfpm_supply_show_battery_notification (XfpmSupply *supply, XfpmBatteryState state, XfpmBattery *battery)
{
    HalDeviceType type;
    const HalBattery *device = xfpm_battery_get_device (battery);
    
    if ( device )
	g_object_get (G_OBJECT(device), "type", &type, NULL);
    else
    {
	g_critical ("Unable to get device type");
	return;
    }
    
    if ( type == HAL_DEVICE_TYPE_PRIMARY )
    {
	xfpm_supply_primary_battery_changed (supply, battery, state);
    }
    else 
    {
	xfpm_supply_misc_battery_changed (supply, battery, state);
    }
}

static void
xfpm_supply_battery_state_changed_cb (XfpmBattery *battery, XfpmBatteryState state, XfpmSupply *supply)
{
    xfpm_supply_show_battery_notification (supply, state, battery);
}

static void
xfpm_supply_show_battery_info (GtkWidget *w, XfpmBattery *battery)
{
    xfpm_battery_show_info (battery);
}

static void
xfpm_supply_popup_battery_menu_cb (XfpmBattery *battery, GtkStatusIcon *icon, 
				   guint button, guint activate_time, 
				   guint battery_type, XfpmSupply *supply)
{
    GtkWidget *menu, *mi, *img;
    
    menu = gtk_menu_new();
    
    // Hibernate menu option
    mi = gtk_image_menu_item_new_with_label(_("Hibernate"));
    img = gtk_image_new_from_icon_name("gpm-hibernate",GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),img);
    gtk_widget_set_sensitive(mi,FALSE);
    
    if ( supply->priv->power_management & SYSTEM_CAN_HIBERNATE )
    {
	gtk_widget_set_sensitive (mi, TRUE);
	g_signal_connect (G_OBJECT(mi), "activate",
			  G_CALLBACK(xfpm_supply_hibernate_cb), supply);
    }
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
    
    // Suspend menu option
    mi = gtk_image_menu_item_new_with_label(_("Suspend"));
    img = gtk_image_new_from_icon_name("gpm-suspend",GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),img);
    
    gtk_widget_set_sensitive(mi,FALSE);
    if ( supply->priv->power_management & SYSTEM_CAN_SUSPEND )
    {
	gtk_widget_set_sensitive (mi,TRUE);
	g_signal_connect(mi,"activate",
			 G_CALLBACK(xfpm_supply_suspend_cb),
			 supply);
    }
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
    
    // Separator
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
    
    // Battery informations
    mi = gtk_image_menu_item_new_with_label (_("Information"));
    img = gtk_image_new_from_icon_name (_get_icon_name_from_battery_type(battery_type), GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(mi), img);
    
    gtk_widget_set_sensitive(mi,FALSE);
    gtk_widget_set_sensitive (mi,TRUE);
    
    g_signal_connect(mi,"activate",
		     G_CALLBACK(xfpm_supply_show_battery_info),
		     battery);
		     
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
    
    // Separator
    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
    
    mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_HELP,NULL);
    gtk_widget_set_sensitive(mi,TRUE);
    gtk_widget_show(mi);
    g_signal_connect(mi,"activate",G_CALLBACK(xfpm_help),NULL);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
    
    mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT,NULL);
    gtk_widget_set_sensitive(mi,TRUE);
    gtk_widget_show(mi);
    g_signal_connect(mi,"activate",G_CALLBACK(xfpm_about),NULL);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
    
    mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES,NULL);
    gtk_widget_set_sensitive(mi,TRUE);
    gtk_widget_show(mi);
    g_signal_connect(mi,"activate",G_CALLBACK(xfpm_preferences),NULL);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);

    // Popup the menu
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		   gtk_status_icon_position_menu, 
		   icon, button, activate_time);
}

//FIXME: Change the name of this function

static void
xfpm_supply_set_adapter_presence (XfpmSupply *supply)
{
    if ( g_hash_table_size (supply->priv->hash) == 0 ) 
	return;
	
    int i;
    GList *list = g_hash_table_get_values (supply->priv->hash );
    
    if (!list)
	return;
    
    for ( i = 0;i <g_list_length(list); i++)
    {
	XfpmBattery *battery = NULL;
	battery = (XfpmBattery *) g_list_nth_data (list, i);
	if ( battery )
	    xfpm_battery_set_adapter_presence (battery, supply->priv->adapter_present);
    }
    
    g_list_free (list);
}

static void
xfpm_supply_adapter_changed_cb (XfpmAdapter *adapter, gboolean present, XfpmSupply *supply )
{
    if ( !supply->priv->adapter_found )
    	g_warning ("Callback from the adapter object but no adapter found in the system");
	
    supply->priv->adapter_present = present;
    xfpm_supply_set_adapter_presence (supply);
    
    g_signal_emit (G_OBJECT(supply), signals[ON_BATTERY], 0, !supply->priv->adapter_present);
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
    const gchar *udi;
    
    udi = hal_device_get_udi (HAL_DEVICE(device));

    TRACE("New battery found %s", udi);

    XfpmBattery *battery = xfpm_battery_new (device);
    
    xfpm_battery_set_show_icon (battery, supply->priv->show_icon);
    
    xfpm_battery_set_adapter_presence (battery, supply->priv->adapter_present);
    xfpm_battery_set_critical_level (battery, supply->priv->critical_level);
    g_hash_table_insert (supply->priv->hash, g_strdup(udi), battery);
    
    g_signal_connect (G_OBJECT(battery), "battery-state-changed",
		      G_CALLBACK(xfpm_supply_battery_state_changed_cb), supply);

    g_signal_connect (G_OBJECT(battery), "popup-battery-menu",
		      G_CALLBACK(xfpm_supply_popup_battery_menu_cb), supply);

}

static void
xfpm_supply_remove_battery (XfpmSupply *supply,  const HalBattery *device)
{
    const gchar *udi = hal_device_get_udi (HAL_DEVICE(device));
    
    XfpmBattery *battery = xfpm_supply_get_battery(supply, udi);
	
    if ( battery )
    {
	TRACE("Removing battery %s", udi);
	g_object_unref(battery);
	if (!g_hash_table_remove(supply->priv->hash, udi))
		g_critical ("Unable to removed battery object from hash");
    }
    
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

static void
xfpm_supply_set_battery_show_tray_icon (XfpmSupply *supply)
{
    GList *list = NULL;
    
    list = g_hash_table_get_values (supply->priv->hash);
    
    if ( !list )
    	return;
	
    int i;
    for ( i = 0; i<g_list_length(list); i++)
    {
	XfpmBattery *battery = NULL;
	battery = (XfpmBattery *) g_list_nth_data(list, i);
	
	if ( battery )
	{
	    xfpm_battery_set_show_icon (battery, supply->priv->show_icon);
	}
    }
    g_list_free (list);
}

static void
xfpm_supply_set_critical_power_level (XfpmSupply *supply)
{
    GList *list = NULL;
    
    list = g_hash_table_get_values (supply->priv->hash);
    
    if ( !list )
    	return;
	
    int i;
    for ( i = 0; i<g_list_length(list); i++)
    {
	XfpmBattery *battery = NULL;
	battery = (XfpmBattery *) g_list_nth_data(list, i);
	
	if ( battery )
	{
	    xfpm_battery_set_critical_level (battery, supply->priv->critical_level);
	}
    }
    g_list_free (list);
}

static void
xfpm_supply_add_adapter (XfpmSupply *supply, const HalDevice *device)
{
    supply->priv->adapter_found = TRUE;
    
    supply->priv->adapter = xfpm_adapter_new (device);
    g_signal_connect (supply->priv->adapter, "adapter-changed", 
		      G_CALLBACK(xfpm_supply_adapter_changed_cb), supply);
    
    supply->priv->adapter_present = xfpm_adapter_get_presence (supply->priv->adapter);
}

static void
xfpm_supply_get_adapter (XfpmSupply *supply)
{
    const HalDevice *device;
    
    if ( hal_power_adapter_found (supply->priv->power) )
    {
	device = hal_power_get_adapter (supply->priv->power);
	if ( device )
	{
#ifdef DEBUG
	    const gchar *udi;
	    udi = hal_device_get_udi (HAL_DEVICE(device));
	    TRACE ("Adapter found in the system with udi=%s\n", udi);
#endif
	    xfpm_supply_add_adapter (supply, device);
	}
    }
    else
    {
	supply->priv->adapter_found = FALSE;
    }
}

static void
xfpm_supply_remove_adapter (XfpmSupply *supply)
{
    supply->priv->adapter_found = FALSE;
    g_object_unref (supply->priv->adapter);
}

static void
xfpm_supply_adapter_added_cb (HalPower *power, const HalDevice *device, XfpmSupply *supply)
{
    if ( supply->priv->adapter_found )
	return;
	
    xfpm_supply_add_adapter (supply, device);
}

static void
xfpm_supply_adapter_removed_cb (HalPower *power, XfpmSupply *supply)
{
    if ( supply->priv->adapter_found )
	xfpm_supply_remove_adapter (supply);
}

static void
xfpm_supply_monitor_start (XfpmSupply *supply)
{
    //FIXME: Check the system formfactor
    xfpm_supply_get_adapter (supply );
    
    GPtrArray *array = hal_power_get_batteries (supply->priv->power);
    
    int i = 0;
    for ( i = 0; i<array->len; i++ )
    {
	HalBattery *device;
	device = (HalBattery *)g_ptr_array_index(array, i);
	xfpm_supply_add_battery (supply, device);
    }
    
    g_ptr_array_free(array, TRUE);
}

static void
xfpm_supply_property_changed_cb (XfconfChannel *channel, gchar *property, GValue *value, XfpmSupply *supply)
{
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
    	return;
	
    if ( xfpm_strequal (property, CRITICAL_BATT_ACTION_CFG) )
    {
	const gchar *str = g_value_get_string (value);
	gint val = xfpm_shutdown_string_to_int (str);
	if ( val == -1 || val == 3 || val == 1)
	{
	    g_warning ("Invalid value %s for property %s, using default\n", str, CRITICAL_BATT_ACTION_CFG);
	    supply->priv->critical_action = XFPM_DO_NOTHING;
	}
	else
	    supply->priv->critical_action = val;
    }
    else if ( xfpm_strequal (property, SHOW_TRAY_ICON_CFG) )
    {
	guint val = g_value_get_uint (value);
	supply->priv->show_icon = val;
	xfpm_supply_set_battery_show_tray_icon (supply);
    }
    else if ( xfpm_strequal( property, CRITICAL_POWER_LEVEL) )
    {
	guint val = g_value_get_uint (value);
	if ( val > 20 )
	{
	    g_warning ("Value %d for property %s is out of range \n", val, CRITICAL_POWER_LEVEL);
	    supply->priv->critical_level = 10;
	}
	else 
	    supply->priv->critical_level = val;
	    
	xfpm_supply_set_critical_power_level (supply);
    }
}

static void
xfpm_supply_load_configuration (XfpmSupply *supply)
{
    //FIXME: Check if the action specified we can actually do it
    gchar *str;
    gint val;
    
    str = xfconf_channel_get_string (supply->priv->conf->channel, CRITICAL_BATT_ACTION_CFG, "Nothing");
    val = xfpm_shutdown_string_to_int (str);
    
    if ( val == -1 || val > 3 || val == 1)
    {
	g_warning ("Invalid value %s for property %s, using default\n", str, CRITICAL_BATT_ACTION_CFG);
	supply->priv->critical_action = XFPM_DO_NOTHING;
	xfconf_channel_set_string ( supply->priv->conf->channel, CRITICAL_BATT_ACTION_CFG, "Nothing");
    }
    else supply->priv->critical_action = val;
    
    g_free (str);
    
    supply->priv->show_icon =
    	xfconf_channel_get_uint (supply->priv->conf->channel, SHOW_TRAY_ICON_CFG, 0);
	
    if ( supply->priv->show_icon < 0 || supply->priv->show_icon > 3 )
    {
	g_warning ("Invalid value %d for property %s, using default\n", supply->priv->show_icon, SHOW_TRAY_ICON_CFG);
	xfconf_channel_set_uint (supply->priv->conf->channel, CRITICAL_BATT_ACTION_CFG, 0);
    }
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
    
    xfpm_supply_load_configuration (supply);
      
    g_signal_connect (supply->priv->conf->channel, "property-changed", 
		      G_CALLBACK(xfpm_supply_property_changed_cb), supply);

    xfpm_supply_monitor_start(supply);
    
    g_signal_connect(supply->priv->power, "battery-added",
		     G_CALLBACK(xfpm_supply_battery_added_cb), supply);
		     
    g_signal_connect(supply->priv->power, "battery-removed",
		     G_CALLBACK(xfpm_supply_battery_removed_cb), supply);
		     
    g_signal_connect(supply->priv->power, "adapter-added",
		     G_CALLBACK(xfpm_supply_adapter_added_cb), supply);
		     
    g_signal_connect(supply->priv->power, "adapter-removed",
		     G_CALLBACK(xfpm_supply_adapter_removed_cb), supply);
    return supply;
}
