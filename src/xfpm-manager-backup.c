/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <ali.slackware@gmail.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <hal/libhal.h>

#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>

#include "xfpm-enums.h"

#include "xfpm-manager.h"
#include "xfpm-driver.h"
#include "xfpm-settings.h"
#include "xfpm-debug.h"
#include "xfpm-common.h"
#include "xfpm-enums.h"

#ifdef HAVE_LIBNOTIFY
#include "xfpm-notify.h"
#endif

#ifdef HAVE_DPMS
#include "xfpm-dpms-spins.h"
#endif

/* Init */
static void xfpm_manager_class_init(XfpmManagerClass *klass);
static void xfpm_manager_init      (XfpmManager *manager);
static void xfpm_manager_finalize  (GObject *object);

static void xfpm_manager_new_driver(XfpmManager *manager);
static void xfpm_manager_property_changed_cb(XfconfChannel *channel,
                                             gchar *property,
                                             GValue *value,
                                             XfpmManager *manager);
static gboolean xfpm_manager_show_options_dialog(XfpmManager *manager);


static DBusHandlerResult _signal_filter
    (DBusConnection *connection,DBusMessage *message,void *user_data);

#define XFPM_MANAGER_GET_PRIVATE(o)   \
(G_TYPE_INSTANCE_GET_PRIVATE((o),XFCE_TYPE_POWER_MANAGER,XfpmManagerPrivate))

struct XfpmManagerPrivate 
{
    
    DBusConnection *conn;
    XfpmDriver  *drv;
    XfconfChannel *ConfChannel;
    
    gboolean dialog_opened;
        
};

G_DEFINE_TYPE(XfpmManager,xfpm_manager,G_TYPE_OBJECT)

static void
xfpm_manager_class_init(XfpmManagerClass *klass) 
{
    
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = xfpm_manager_finalize;
           
    g_type_class_add_private(klass,sizeof(XfpmManagerPrivate));
    
}
    
static void
xfpm_manager_init(XfpmManager *manager) 
{
    XfpmManagerPrivate *priv;
    priv = XFPM_MANAGER_GET_PRIVATE(manager);
    priv->conn = NULL;
    priv->drv  = NULL;
    priv->ConfChannel = NULL;
     
#ifdef HAVE_LIBNOTIFY
    notify_init("Basics");
#endif    
}
    
static void
xfpm_manager_finalize(GObject *object) 
{
    XfpmManager *manager;
    manager = XFPM_MANAGER(object);
    manager->priv = XFPM_MANAGER_GET_PRIVATE(manager);
    
    if ( manager->priv->conn ) 
    {
        dbus_connection_unref(manager->priv->conn);
    }
    
    if ( manager->priv->drv )
    {
        g_object_unref(manager->priv->drv);
    }
    
    if ( manager->priv->ConfChannel )
    {
        g_object_unref(manager->priv->ConfChannel);
    }
    
    G_OBJECT_CLASS(xfpm_manager_parent_class)->finalize(object);
}
        
XfpmManager *
xfpm_manager_new(void) 
{
    XfpmManager *manager = NULL;
    manager = g_object_new (XFCE_TYPE_POWER_MANAGER,NULL);
    return manager;    
}        

static void 
xfpm_manager_new_driver(XfpmManager *manager)
{
    /* the order here for loading and setting the configuration of xfpm-driver
     * is important
     */
    
    XFPM_DEBUG("Loading configuration from config file\n");
    XfpmManagerPrivate *priv;
    priv = XFPM_MANAGER_GET_PRIVATE(manager);
    guint critical_level;
    guint critical_action;
    XfpmShowIcon show_tray_icon;
    
    critical_level = xfconf_channel_get_uint(priv->ConfChannel,CRITICAL_BATT_CFG,12);
    critical_action = xfconf_channel_get_uint(priv->ConfChannel,CRITICAL_BATT_ACTION_CFG,0);
    show_tray_icon = xfconf_channel_get_uint(priv->ConfChannel,SHOW_TRAY_ICON_CFG,0);
                
    priv->drv = xfpm_driver_new(critical_level,critical_action,show_tray_icon);
    
#ifdef HAVE_LIBNOTIFY
    gboolean notify_enabled;
    notify_enabled = xfconf_channel_get_bool(priv->ConfChannel,BATT_STATE_NOTIFICATION_CFG,TRUE);
    g_object_set(G_OBJECT(priv->drv),"enable-notification",notify_enabled,NULL);
    priv->drv->show_sleep_error = xfconf_channel_get_bool(priv->ConfChannel,SHOW_SLEEP_ERRORS_CFG,TRUE);
#endif

#ifdef HAVE_DPMS
    GPtrArray *arr;
    GValue *value;
    arr = xfconf_channel_get_arrayv(priv->ConfChannel,ON_AC_DPMS_TIMEOUTS_CFG);
    if ( arr ) 
    {
        value = g_ptr_array_index(arr,0);
        priv->drv->dpms->on_ac_standby_timeout = g_value_get_uint(value)*60;
        
        value = g_ptr_array_index(arr,1);
        priv->drv->dpms->on_ac_suspend_timeout = g_value_get_uint(value)*60;
        
        value = g_ptr_array_index(arr,2);
        priv->drv->dpms->on_ac_off_timeout = g_value_get_uint(value)*60;
        xfconf_array_free(arr);
    } 
    else
    {
        priv->drv->dpms->on_ac_standby_timeout = 1800;
        priv->drv->dpms->on_ac_suspend_timeout = 2700;
        priv->drv->dpms->on_ac_off_timeout = 3600;
    }
    
    arr = xfconf_channel_get_arrayv(priv->ConfChannel,ON_BATT_DPMS_TIMEOUTS_CFG);
    if ( arr ) 
    {
        value = g_ptr_array_index(arr,0);
        priv->drv->dpms->on_batt_standby_timeout = g_value_get_uint(value)*60;
        
        value = g_ptr_array_index(arr,1);
        priv->drv->dpms->on_batt_suspend_timeout = g_value_get_uint(value)*60;
        
        value = g_ptr_array_index(arr,2);
        priv->drv->dpms->on_batt_off_timeout = g_value_get_uint(value)*60;
        xfconf_array_free(arr);
    } 
    else
    {
        priv->drv->dpms->on_batt_standby_timeout = 180;
        priv->drv->dpms->on_batt_suspend_timeout = 240;
        priv->drv->dpms->on_batt_off_timeout = 300;
    }
    priv->drv->dpms->dpms_enabled  = xfconf_channel_get_bool(priv->ConfChannel,DPMS_ENABLE_CFG,TRUE);
    /* The driver monitor will notify dpms after getting AC adapter status*/
    //g_object_notify(G_OBJECT(priv->drv->dpms),"dpms");
#endif  
    xfpm_driver_monitor(priv->drv);
    
    /* After start monitoring we set CPU governor property here*/
    
    XfpmCpuGovernor ac_governor,batt_governor;
    gboolean freq_scaling_enabled;
    ac_governor = xfconf_channel_get_uint(priv->ConfChannel,ON_AC_CPU_GOV_CFG,ONDEMAND);
    batt_governor = xfconf_channel_get_uint(priv->ConfChannel,ON_BATT_CPU_GOV_CFG,POWERSAVE);
    freq_scaling_enabled = xfconf_channel_get_bool(priv->ConfChannel,CPU_FREQ_SCALING_CFG,TRUE);
    
    g_object_set(G_OBJECT(priv->drv),"cpu-freq-scaling-enabled",freq_scaling_enabled,
                                     "on-ac-cpu-gov",ac_governor,
                                     "on-batt-cpu-gov",batt_governor,
                                     NULL);
    
}

static void
xfpm_manager_property_changed_cb(XfconfChannel *channel,gchar *property,
                                GValue *value,XfpmManager *manager)
{
    XFPM_DEBUG("%s \n",property);
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
    {
        XFPM_DEBUG(" invalid value type\n");
        return;
    }
    XfpmManagerPrivate *priv;
    priv = XFPM_MANAGER_GET_PRIVATE(manager);
    
    if ( !strcmp(property,CRITICAL_BATT_CFG) ) 
    {
        guint val = g_value_get_uint(value);
        g_object_set(G_OBJECT(priv->drv),"critical-charge",val,NULL);
        return;
    }
    
    if ( !strcmp(property,CRITICAL_BATT_ACTION_CFG) ) 
    {
        guint val = g_value_get_uint(value);
        g_object_set(G_OBJECT(priv->drv),"critical-action",val,NULL);
        return;
    }
    
    if ( !strcmp(property,SHOW_TRAY_ICON_CFG) ) 
    {
        gboolean val = g_value_get_uint(value);
        g_object_set(G_OBJECT(priv->drv),"show-tray-icon",val,NULL);
        return;
    }
    
    if ( !strcmp(property,CPU_FREQ_SCALING_CFG) ) 
    {
        gboolean val = g_value_get_boolean(value);
        g_object_set(G_OBJECT(priv->drv),"cpu-freq-scaling-enabled",val,NULL);
        return;
    }
    
    if ( !strcmp(property,ON_AC_CPU_GOV_CFG) ) 
    {
        guint val = g_value_get_uint(value);
        g_object_set(G_OBJECT(priv->drv),"on-ac-cpu-gov",val,NULL);
        return;
    }
    if ( !strcmp(property,ON_BATT_CPU_GOV_CFG) ) 
    {
        guint val = g_value_get_uint(value);
        g_object_set(G_OBJECT(priv->drv),"on-batt-cpu-gov",val,NULL);
        return;
    }
    
#ifdef HAVE_LIBNOTIFY    
    if ( !strcmp(property,BATT_STATE_NOTIFICATION_CFG) ) 
    {
        gboolean val = g_value_get_boolean(value);
        g_object_set(G_OBJECT(priv->drv),"enable-notification",val,NULL);
        return;
    }
#endif    

#ifdef HAVE_DPMS
    if ( !strcmp(property,DPMS_ENABLE_CFG) ) 
    {
        gboolean val = g_value_get_boolean(value);
        priv->drv->dpms->dpms_enabled = val;
        g_object_notify(G_OBJECT(priv->drv->dpms),"dpms");
        return;
    }

    if ( !strcmp(property,ON_BATT_DPMS_TIMEOUTS_CFG) ) 
    {
        GPtrArray *arr;
        gpointer data;
        GValue *val;    
        guint value1 = 180,value2 = 240,value3 = 300;
        data = g_value_get_boxed(value);
        arr = (GPtrArray *) data;
        
        val = g_ptr_array_index(arr,0);
        value1 = g_value_get_uint(val);
        
        val = g_ptr_array_index(arr,1);
        value2 = g_value_get_uint(val);
        
        val = g_ptr_array_index(arr,2);
        value3 = g_value_get_uint(val);
        priv->drv->dpms->on_batt_standby_timeout = value1 * 60;
        priv->drv->dpms->on_batt_suspend_timeout = value2 * 60;
        priv->drv->dpms->on_batt_off_timeout     = value3 * 60; 
        g_object_notify(G_OBJECT(priv->drv->dpms),"dpms");
        return;
    }
    
    if ( !strcmp(property,ON_AC_DPMS_TIMEOUTS_CFG) ) 
    {
        GPtrArray *arr;
        gpointer data;
        GValue *val;
        guint value1 = 1800,value2 = 2700,value3 = 3600;
        data = g_value_get_boxed(value);
        arr = (GPtrArray *) data;
        
        val = g_ptr_array_index(arr,0);
        value1 = g_value_get_uint(val);
        
        val = g_ptr_array_index(arr,1);
        value2 = g_value_get_uint(val);
        
        val = g_ptr_array_index(arr,2);
        value3 = g_value_get_uint(val);
        
        priv->drv->dpms->on_ac_standby_timeout = value1 * 60;
        priv->drv->dpms->on_ac_suspend_timeout = value2 * 60;
        priv->drv->dpms->on_ac_off_timeout     = value3 * 60;
        
        g_object_notify(G_OBJECT(priv->drv->dpms),"dpms");
        return;
    }
#endif    
}

static void
dialog_cb(GtkDialog *dialog, gint response, XfpmManager *manager)
{
    XfpmManagerPrivate *priv;
    priv = XFPM_MANAGER_GET_PRIVATE(manager);
    priv->dialog_opened = FALSE;
    
    switch(response) 
    {
            case GTK_RESPONSE_HELP:
                /*FIXME : Show help */
                gtk_widget_destroy(GTK_WIDGET(dialog));
                break;
            default:
                gtk_widget_destroy(GTK_WIDGET(dialog));
                break;
    }
}                                 
                                    
static gboolean
xfpm_manager_show_options_dialog(XfpmManager *manager)
{
    XfpmManagerPrivate *priv;
    priv = XFPM_MANAGER_GET_PRIVATE(manager);
    
    if ( priv->dialog_opened == TRUE )
    {
        return FALSE;
    }
    
    /* FIXME: we shouldn't show the battery configuration unless is a primary one*/
    gboolean battery = xfpm_driver_has_battery(priv->drv);
    
    gchar **govs;
    govs = xfpm_hal_get_available_cpu_governors(XFPM_HAL(priv->drv));
    int i = 0;
    guint8 gov[4] = { -1, };
    
    if ( govs ) 
    {
        for ( i = 0 ; govs[i] ; i++ )
        {
            if ( !strcmp(govs[i],"powersave") )    gov[0] = 1;
            if ( !strcmp(govs[i],"ondemand") )     gov[1] = 1;
            if ( !strcmp(govs[i],"performance") )  gov[2] = 1;
            if ( !strcmp(govs[i],"conservative") ) gov[3] = 1;
            if ( !strcmp(govs[i],"userspace") )    gov[4] = 1;
        }   
        libhal_free_string_array(govs);
    }
    
    GtkWidget *dialog;
    
    dialog = xfpm_settings_new(priv->ConfChannel,battery,priv->drv->can_hibernate,gov);
    
    xfce_gtk_window_center_on_monitor_with_pointer(GTK_WINDOW(dialog));
    gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
    
    gdk_x11_window_set_user_time(dialog->window,gdk_x11_get_server_time (dialog->window));
    
    g_signal_connect(dialog,"response",G_CALLBACK(dialog_cb),manager);        
    gtk_widget_show(dialog);
    priv->dialog_opened = TRUE;
    
    return FALSE;
}

static void
_send_reply(DBusConnection *conn,DBusMessage *mess)
{
    DBusMessage *reply;
    reply = dbus_message_new_method_return(mess);
    dbus_connection_send (conn, reply, NULL);
    dbus_connection_flush (conn);
    dbus_message_unref(reply);
    
}

static DBusHandlerResult _signal_filter
    (DBusConnection *connection,DBusMessage *message,void *user_data) 
{
    XfpmManager *manager = XFPM_MANAGER(user_data);
    
    if ( dbus_message_is_signal(message,"xfpm.power.manager","Customize" ) )
    {
        XFPM_DEBUG("message customize received\n");
        /* don't block the signal filter so show the configuration dialog in a
         * timeout function otherwise xfce4-power-manager -q will have no effect*/
        g_timeout_add(100,(GSourceFunc)xfpm_manager_show_options_dialog,manager);
        return DBUS_HANDLER_RESULT_HANDLED;
    }    
    
    if ( dbus_message_is_signal(message,"xfpm.power.manager","Quit" ) )
    {
        XFPM_DEBUG("message quit received\n");
        _send_reply(connection,message);      
        gtk_main_quit();
        g_object_unref(manager);
        return DBUS_HANDLER_RESULT_HANDLED;
    }    
    
    if ( dbus_message_is_signal(message,"xfpm.power.manager","Running" ) )
    {
        XFPM_DEBUG("message running received\n");
        _send_reply(connection,message);
       
        return DBUS_HANDLER_RESULT_HANDLED;
    }    
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

gboolean
xfpm_manager_start (XfpmManager *manager) 
{
    XFPM_DEBUG("starting xfpm manager\n");
    XfpmManagerPrivate *priv;
    priv = XFPM_MANAGER_GET_PRIVATE(manager);

    DBusError error;

    dbus_error_init(&error);
    priv->conn = dbus_bus_get(DBUS_BUS_SESSION,&error);
    if (!priv->conn) 
    {
       g_critical("Failed to connect to the DBus daemon: %s\n",error.message);
       dbus_error_free(&error);
       return FALSE;
    }
        
    dbus_connection_setup_with_g_main(priv->conn,NULL);
    dbus_bus_add_match(priv->conn, "type='signal',interface='xfpm.power.manager'",NULL);
    dbus_connection_add_filter(priv->conn,_signal_filter,manager,NULL);
    
    GError *g_error = NULL;
    if ( !xfconf_init(&g_error) )
    {
        g_critical("Unable to xfconf init failed: %s\n",g_error->message);
        g_error_free(g_error);
    }

    priv->ConfChannel = xfconf_channel_new(XFPM_CHANNEL_CFG);
    
    g_signal_connect(priv->ConfChannel,"property-changed",
                     G_CALLBACK(xfpm_manager_property_changed_cb),manager);
    
    xfpm_manager_new_driver(manager);
   
    gtk_main();
    dbus_connection_remove_filter(priv->conn,_signal_filter,NULL);
    xfconf_shutdown();
    
    return TRUE;
}
