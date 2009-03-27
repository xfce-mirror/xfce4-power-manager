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
#include "libxfpm/xfpm-notify.h"

#include "xfpm-supply.h"
#include "xfpm-adapter.h"
#include "xfpm-battery.h"
#include "xfpm-enum.h"
#include "xfpm-enum-types.h"
#include "xfpm-xfconf.h"
#include "xfpm-config.h"
#include "xfpm-marshal.h"

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
    
    gboolean       adapter_present;
    guint8         power_management;
};

enum
{
    SHUTDOWN_REQUEST,
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
                      _xfpm_marshal_VOID__BOOLEAN_ENUM,
                      G_TYPE_NONE, 2, 
		      G_TYPE_BOOLEAN,
		      XFPM_TYPE_SHUTDOWN_REQUEST);

    object_class->finalize = xfpm_supply_finalize;

    g_type_class_add_private(klass,sizeof(XfpmSupplyPrivate));
}

static void
xfpm_supply_init (XfpmSupply *supply)
{
    supply->priv = XFPM_SUPPLY_GET_PRIVATE (supply);
  
    supply->priv->power   = hal_power_new ();
    supply->priv->hash    = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    supply->priv->notify  = xfpm_notify_new ();
    supply->priv->conf    = xfpm_xfconf_new ();
}


static void
xfpm_supply_finalize (GObject *object)
{
    XfpmSupply *supply;
    supply = XFPM_SUPPLY(object);
    
    if ( supply->priv->power )
    	g_object_unref(supply->priv->power);
	
    g_hash_table_destroy (supply->priv->hash);
	
    if ( supply->priv->notify )
    	g_object_unref (supply->priv->notify);
	
    if ( supply->priv->conf )
    	g_object_unref (supply->priv->conf);
	
    if ( supply->priv->adapter )
	g_object_unref (supply->priv->adapter);
	
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
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, FALSE, XFPM_DO_HIBERNATE);
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
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, FALSE, XFPM_DO_SUSPEND);
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
     XfpmShutdownRequest critical_action = 
	xfpm_xfconf_get_property_enum (supply->priv->conf, CRITICAL_BATT_ACTION_CFG);

    if ( G_UNLIKELY (critical_action == XFPM_DO_SUSPEND ) )
	return;
	
    g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, TRUE, critical_action);
}

static void
_notify_action_callback (NotifyNotification *n, gchar *action, XfpmSupply *supply)
{
    if ( xfpm_strequal(action, "shutdown") )
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, TRUE, XFPM_DO_SHUTDOWN);
    else if ( xfpm_strequal(action, "hibernate") )
	g_signal_emit (G_OBJECT(supply ), signals[SHUTDOWN_REQUEST], 0, TRUE, XFPM_DO_SHUTDOWN);
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
    XfpmShutdownRequest critical_action = 
	xfpm_xfconf_get_property_enum (supply->priv->conf, CRITICAL_BATT_ACTION_CFG);
    
    if ( xfpm_supply_on_low_power (supply) )
    {
	TRACE ("System is running on low power");
	if ( critical_action == XFPM_DO_NOTHING )
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
    	= xfpm_supply_get_message_from_battery_state (state, supply->priv->adapter_present);

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
    g_signal_connect(mi,"activate",G_CALLBACK(xfpm_about), _("Xfce Power Manager"));
    
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

static gboolean
xfpm_supply_monitor_start (XfpmSupply *supply)
{
    //FIXME: Check the system formfactor
    
    GPtrArray *array = hal_power_get_batteries (supply->priv->power);
    
    int i = 0;
    for ( i = 0; i<array->len; i++ )
    {
	HalBattery *device;
	device = (HalBattery *)g_ptr_array_index(array, i);
	xfpm_supply_add_battery (supply, device);
    }
    
    g_ptr_array_free(array, TRUE);
    
    return FALSE;
}

static void
xfpm_supply_adapter_changed_cb (XfpmAdapter *adapter, gboolean present, XfpmSupply *supply)
{
    supply->priv->adapter_present = present;
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
    supply->priv->adapter_present = xfpm_adapter_get_present (supply->priv->adapter);
    
    g_signal_connect (supply->priv->adapter, "adapter-changed",
		      G_CALLBACK(xfpm_supply_adapter_changed_cb), supply);
      
    xfpm_supply_monitor_start (supply);
    
    g_signal_connect(supply->priv->power, "battery-added",
		     G_CALLBACK(xfpm_supply_battery_added_cb), supply);
		     
    g_signal_connect(supply->priv->power, "battery-removed",
		     G_CALLBACK(xfpm_supply_battery_removed_cb), supply);
    
}

gboolean xfpm_supply_on_low_battery (XfpmSupply *supply)
{
    g_return_val_if_fail (XFPM_IS_SUPPLY(supply), FALSE);
    
    return xfpm_supply_on_low_power(supply);
}
