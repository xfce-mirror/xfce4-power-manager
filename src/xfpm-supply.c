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

#include "xfpm-supply.h"
#include "xfpm-battery.h"
#include "xfpm-adapter.h"
#include "xfpm-notify.h"
#include "xfpm-network-manager.h"
#include "xfpm-enum.h"
#include "xfpm-enum-types.h"
#include "xfpm-config.h"

/* Init */
static void xfpm_supply_class_init (XfpmSupplyClass *klass);
static void xfpm_supply_init       (XfpmSupply *xfpm_supply);
static void xfpm_supply_finalize   (GObject *object);

#define XFPM_SUPPLY_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_SUPPLY, XfpmSupplyPrivate))

struct XfpmSupplyPrivate
{
    DbusHal       *hbus;
    XfpmNotify    *notify;
    XfpmAdapter   *adapter;
    XfconfChannel *channel;
    
    HalPower      *power;
    GHashTable    *hash;
    
    XfpmShutdownRequest critical_action;
    XfpmShowIcon   show_icon;
    
    gboolean 	   adapter_found;
    gboolean       adapter_present;
    
    guint8         power_management;
};

enum
{
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
    
    supply->priv->channel = NULL;
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
	g_signal_emit (G_OBJECT(supply ), signals[BLOCK_SHUTDOWN], 0, TRUE);
	
	xfpm_send_message_to_network_manager ("sleep");
	xfpm_lock_screen ();
	dbus_hal_shutdown (supply->priv->hbus, "Hibernate", NULL);
	xfpm_send_message_to_network_manager ("wake");
	
	g_signal_emit (G_OBJECT(supply ), signals[BLOCK_SHUTDOWN], 0, FALSE);
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
	g_signal_emit (G_OBJECT(supply ), signals[BLOCK_SHUTDOWN], 0, TRUE);
	
	xfpm_send_message_to_network_manager ("sleep");
	xfpm_lock_screen ();
	dbus_hal_shutdown (supply->priv->hbus, "Suspend", NULL);
	xfpm_send_message_to_network_manager ("wake");
	
	g_signal_emit (G_OBJECT(supply ), signals[BLOCK_SHUTDOWN], 0, FALSE);
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

static const gchar *
xfpm_supply_get_message_from_battery_state ( XfpmBatteryState state, HalDeviceType type, gboolean adapter_present)
{
    switch ( state )
    {
	case BATTERY_FULLY_CHARGED:
	    return _("Your battery is fully charged");
	    break;
	case BATTERY_IS_CHARGING:
	    return  _("Battery is charging");
	    break;
	case BATTERY_IS_DISCHARGING:
	    if ( type == HAL_DEVICE_TYPE_PRIMARY )
	    	return adapter_present ? _("Your battery is discharging") : _("System is running on battery");
	    else
	    	return _("Your battery is discharging");
	    break;
	case BATTERY_CHARGE_LOW:
	    return _("Your battery charge is low");
	    break;
	default:
	    return  NULL;
	    break;
    }
}

/*
static gboolean
xfpm_supply_get_running_on_battery (XfpmSupply *supply)
{
    if ( !supply->priv->adapter_found )
    	return FALSE;
    else if ( supply->priv->adapter_present )
	return FALSE;
    
    return TRUE;
}
*/
static void
xfpm_supply_battery_state_changed_cb (XfpmBattery *battery, XfpmBatteryState state, XfpmSupply *supply)
{
    HalDeviceType type;
    const HalDevice *device = xfpm_battery_get_device (battery);
    
    if ( device )
	g_object_get (G_OBJECT(device), "type", &type, NULL);
    else
    {
	g_critical ("Unable to get device object\n");
	return;
    }
	
    
    const gchar *message 
    	= xfpm_supply_get_message_from_battery_state (state,
						      type,
						      supply->priv->adapter_found   ? 
						      supply->priv->adapter_present : 
						      TRUE);
    
    if ( !message )
    	return;
    
    xfpm_notify_show_notification (xfpm_battery_get_notify_obj(battery), 
				   _("Xfce power manager"), 
				   message, 
				   xfpm_battery_get_icon_name (battery),
				   10000,
				   XFPM_NOTIFY_NORMAL, 
				   xfpm_battery_get_status_icon (battery));
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
    mi = gtk_image_menu_item_new_with_label (_("Battery information"));
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

static void
xfpm_supply_adapter_changed_cb (XfpmAdapter *adapter, gboolean present, XfpmSupply *supply )
{
    if ( !supply->priv->adapter_found )
    	g_critical ("Callback from the adapter object but no adapter found in the system\n");
	
    supply->priv->adapter_present = present;
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
xfpm_supply_add_device (XfpmSupply *supply, const HalDevice *device)
{
    HalDeviceType type;
    gchar *udi = NULL;
    
    g_object_get(G_OBJECT(device), "type", &type, "udi", &udi, NULL);
    
    if ( type == HAL_DEVICE_TYPE_ADAPTER )
    {
	TRACE("Adapter found %s", udi);
	if ( supply->priv->adapter_found )
	{
	    TRACE("System with more than one AC adapter"); /* g_strange */
	    goto out;
	}
	supply->priv->adapter = xfpm_adapter_new (device);
	g_signal_connect (supply->priv->adapter, "adapter-changed", 
			  G_CALLBACK (xfpm_supply_adapter_changed_cb), supply);
			  
	supply->priv->adapter_found = TRUE;
	gboolean adapter_present; 
	g_object_get (G_OBJECT(device), "is-present", &adapter_present, NULL);
	supply->priv->adapter_present = adapter_present;
	g_signal_emit (G_OBJECT(supply), signals[ON_BATTERY], 0, supply->priv->adapter_present);
    }
    else
    {
	TRACE("New battery found %s", udi);
	XfpmBattery *battery = xfpm_battery_new (device);
	xfpm_battery_set_show_icon (battery, supply->priv->show_icon);
	g_hash_table_insert (supply->priv->hash, g_strdup(udi), battery);
	
	g_signal_connect (G_OBJECT(battery), "battery-state-changed",
			  G_CALLBACK(xfpm_supply_battery_state_changed_cb), supply);

	g_signal_connect (G_OBJECT(battery), "popup-battery-menu",
			  G_CALLBACK(xfpm_supply_popup_battery_menu_cb), supply);
    }
out:
    if (udi)
    	g_free (udi);
}

static void
xfpm_supply_remove_device(XfpmSupply *supply,  const HalDevice *device)
{
    HalDeviceType type;
    gchar *udi = NULL;
    
    g_object_get(G_OBJECT(device), "type", &type, "udi", &udi, NULL);
    
    if ( type == HAL_DEVICE_TYPE_ADAPTER )
    {
	if ( !supply->priv->adapter_found )
	{
	    g_warning ("No adapter found in the system to removed from the data\n");
	    goto out;
	}
	TRACE("Adapter removed %s", udi);
	g_object_unref (supply->priv->adapter);
	supply->priv->adapter_found = FALSE;
    }
    else 
    {
	XfpmBattery *battery = xfpm_supply_get_battery(supply, udi);
	
	if ( battery )
	{
	    TRACE("Removing battery %s", udi);
	    g_object_unref(battery);
	    if (!g_hash_table_remove(supply->priv->hash, udi))
		    g_critical ("Unable to removed battery object from hash\n");
	}
    }
    
out:
    if (udi)
    	g_free (udi);
}

static void
xfpm_supply_monitor_start (XfpmSupply *supply)
{
    GPtrArray *array = hal_power_get_devices (supply->priv->power);
    
    int i = 0;
    for ( i = 0; i<array->len; i++ )
    {
	HalDevice *device;
	device = (HalDevice *)g_ptr_array_index(array, i);
	xfpm_supply_add_device (supply, device);
    }
    
    g_ptr_array_free(array, TRUE);
}

static void
xfpm_supply_device_added_cb (HalPower *power, const HalDevice *device, XfpmSupply *supply)
{
    xfpm_supply_add_device (supply, device);
}

static void
xfpm_supply_device_removed_cb (HalPower *power, const HalDevice *device, XfpmSupply *supply)
{
    xfpm_supply_remove_device (supply, device);
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
xfpm_supply_property_changed_cb (XfconfChannel *channel, gchar *property, GValue *value, XfpmSupply *supply)
{
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
    	return;
	
    if ( xfpm_strequal (property, CRITICAL_BATT_ACTION_CFG) )
    {
	guint val = g_value_get_uint (value);
	supply->priv->critical_action = val;
    }
    else if ( xfpm_strequal (property, SHOW_TRAY_ICON_CFG) )
    {
	guint val = g_value_get_uint (value);
	supply->priv->show_icon = val;
	xfpm_supply_set_battery_show_tray_icon (supply);
    }
    
}

static void
xfpm_supply_load_configuration (XfpmSupply *supply)
{
    //FIXME: Check if the action specified we can actually do it
    supply->priv->critical_action =
    	xfconf_channel_get_uint (supply->priv->channel, CRITICAL_BATT_ACTION_CFG, 0);
	
    supply->priv->show_icon =
    	xfconf_channel_get_uint (supply->priv->channel, SHOW_TRAY_ICON_CFG, 0);
}

/*
 * Public functions
 */ 
XfpmSupply *
xfpm_supply_new (DbusHal *bus, XfconfChannel *channel )
{
    XfpmSupply *supply = NULL;
    supply = g_object_new(XFPM_TYPE_SUPPLY,NULL);
    
    supply->priv->hbus = bus;
    supply->priv->channel    = channel;
    
    xfpm_supply_load_configuration (supply);
    
    g_object_get (G_OBJECT(supply->priv->hbus) , 
		  "power-management-info", &supply->priv->power_management, NULL);
    
    g_signal_connect (channel, "property-changed", 
		      G_CALLBACK(xfpm_supply_property_changed_cb), supply);

    xfpm_supply_monitor_start(supply);
    
    g_signal_connect(supply->priv->power, "device-added",
		     G_CALLBACK(xfpm_supply_device_added_cb), supply);
		     
    g_signal_connect(supply->priv->power, "device-removed",
		     G_CALLBACK(xfpm_supply_device_removed_cb), supply);
    return supply;
}
