/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <aliov@xfce.org>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <hal/libhal.h>

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4util/libxfce4util.h>

#include <xfconf/xfconf.h>

#include "xfpm-common.h"

#include "xfpm-driver.h"
#include "xfpm-debug.h"
#include "xfpm-battery.h"
#include "xfpm-cpu.h"
#include "xfpm-ac-adapter.h"
#include "xfpm-button.h"
#include "xfpm-lcd-brightness.h"
#include "xfpm-popups.h"
#include "xfpm-enum-types.h"
#include "xfpm-settings.h"
#include "xfpm-dbus-messages.h"

#ifdef HAVE_LIBNOTIFY
#include "xfpm-notify.h"
#endif

#ifdef HAVE_DPMS
#include "xfpm-dpms.h"
#include "xfpm-dpms-spins.h"
#endif

#ifndef _
#define _(x) x
#endif

/*init*/
static void xfpm_driver_init        (XfpmDriver *drv);
static void xfpm_driver_class_init  (XfpmDriverClass *klass);
static void xfpm_driver_finalize    (GObject *object);

static void xfpm_driver_ac_adapter_state_changed_cb(XfpmAcAdapter *adapter,
                                                    gboolean present,
                                                    gboolean state_ok,
                                                    XfpmDriver *drv);
                                                    
static void xfpm_driver_set_power_save(XfpmDriver *drv);

static void xfpm_driver_property_changed_cb(XfconfChannel *channel,
                                            gchar *property,
                                            GValue *value,
                                            XfpmDriver *drv);
                                            
static gboolean xfpm_driver_show_options_dialog(XfpmDriver *drv);

/* Function that receives events suspend/hibernate,shutdown for all
 * Xfce power manager components and syncronize them
 */
#ifdef HAVE_LIBNOTIFY
static void xfpm_driver_report_sleep_errors(XfpmDriver *driver,
                                            const gchar *icon_name,
                                            const gchar *error);
#endif
static gboolean xfpm_driver_do_suspend(gpointer data);
static gboolean xfpm_driver_do_hibernate(gpointer data);
static gboolean xfpm_driver_do_shutdown(gpointer data);

static void xfpm_driver_suspend(XfpmDriver *drv,gboolean critical);
static void xfpm_driver_hibernate(XfpmDriver *drv,gboolean critical);
static void xfpm_driver_shutdown(XfpmDriver *drv,gboolean critical);

static void xfpm_driver_handle_action_request(GObject *object,
                                              guint action,
                                              gboolean critical,
                                              XfpmDriver *drv);
static void xfpm_driver_load_config(XfpmDriver *drv);

static void xfpm_driver_check_nm(XfpmDriver *drv);
static void xfpm_driver_check_power_management(XfpmDriver *drv);
static void xfpm_driver_check_power_save(XfpmDriver *drv);
static void xfpm_driver_check(XfpmDriver *drv);

static void xfpm_driver_load_all(XfpmDriver *drv);

/* DBus message Filter and replies */
static void xfpm_driver_send_reply(DBusConnection *conn,DBusMessage *mess);
static DBusHandlerResult xfpm_driver_signal_filter
    (DBusConnection *connection,DBusMessage *message,void *user_data);

static guint32 socket_id = 0;

#define XFPM_DRIVER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o),XFPM_TYPE_DRIVER,XfpmDriverPrivate))

struct XfpmDriverPrivate 
{
    DBusConnection *conn;
    GMainLoop *loop;
    
    SystemFormFactor formfactor;
    
    guint8 power_management;

	gboolean dialog_opened;
#ifdef HAVE_LIBNOTIFY    
    gboolean show_power_management_error;
#endif
    gboolean enable_power_save;
    
    gboolean accept_sleep_request;
    
    gboolean cpufreq_control;
    gboolean buttons_control;
    gboolean lcd_brightness_control;
	gboolean nm_responding;
    
    XfpmHal     *hal;
    XfpmCpu     *cpu;
    XfpmBattery *batt;
    XfpmButton  *bt;
    XfpmLcdBrightness *lcd;
    
#ifdef HAVE_DPMS    
    XfpmDpms *dpms;
#endif    
    GtkStatusIcon *adapter;
    gboolean ac_adapter_present;
};

G_DEFINE_TYPE(XfpmDriver,xfpm_driver,G_TYPE_OBJECT)

static void
xfpm_driver_class_init(XfpmDriverClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = xfpm_driver_finalize;
    
    g_type_class_add_private(klass,sizeof(XfpmDriverPrivate));
}

static void
xfpm_driver_init(XfpmDriver *drv)
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);

	priv->conn = NULL;
    priv->power_management = 0;
        
    priv->dialog_opened = FALSE;
    priv->cpufreq_control = FALSE;
    priv->buttons_control = FALSE;
    priv->enable_power_save = FALSE;
    priv->lcd_brightness_control = FALSE;
    priv->accept_sleep_request = TRUE;
	priv->nm_responding = FALSE;
 
    priv->loop    = NULL;
    priv->cpu     = NULL;
    priv->adapter = NULL;
    priv->batt    = NULL;
    priv->bt      = NULL;
    priv->lcd     = NULL;
    priv->hal     = NULL;
#ifdef HAVE_DPMS
    priv->dpms    = NULL;
#endif     

#ifdef HAVE_LIBNOTIFY
    notify_init("xfce4-power-manager");
#endif    
}

static void
xfpm_driver_finalize(GObject *object)
{
    XfpmDriver *drv;
    drv = XFPM_DRIVER(object);
    drv->priv = XFPM_DRIVER_GET_PRIVATE(drv);

#ifdef HAVE_DPMS
    if ( drv->priv->dpms )
    {
        g_object_unref(drv->priv->dpms);
    }
#endif
    if ( drv->priv->adapter )
    {
        g_object_unref(drv->priv->adapter);
    }
    if ( drv->priv->batt )
    {
        g_object_unref(drv->priv->batt);
    }
    if ( drv->priv->cpufreq_control )
    {
        g_object_unref(drv->priv->cpu);
    }
    if ( drv->priv->buttons_control )
    {
        g_object_unref(drv->priv->bt);
    }
    if ( drv->priv->lcd_brightness_control )
    {
        g_object_unref(drv->priv->lcd);
    }
    if ( drv->priv->conn )
    {
        dbus_connection_unref(drv->priv->conn);
    }
    if ( drv->priv->loop )
    {
        g_main_loop_unref(drv->priv->loop);
    }
    G_OBJECT_CLASS(xfpm_driver_parent_class)->finalize(object);
}

static void
xfpm_driver_set_power_save(XfpmDriver *drv)
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    GError *error = NULL;
    xfpm_hal_set_power_save(priv->hal,priv->ac_adapter_present,&error);
    
    if ( error )
    {
        XFPM_DEBUG("Set power save failed: %s\n",error->message);
        g_error_free(error);
    }
}

static void xfpm_driver_ac_adapter_state_changed_cb(XfpmAcAdapter *adapter,
                                                    gboolean present,
                                                    gboolean state_ok,
                                                    XfpmDriver *drv)
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);

    const gchar *error;
        error =  _("Unable to read AC adapter status, the power manager will not work properly. "\
                  "Possible reasons: The AC adapter driver is not loaded, "\
                  "broken connection with the hardware abstract layer or "\
				  "the message bus daemon is not running");
    if ( !state_ok && priv->formfactor == SYSTEM_LAPTOP )  
    {
#ifdef HAVE_LIBNOTIFY         
        gboolean visible;
        g_object_get(G_OBJECT(priv->adapter),"visible",&visible,NULL);
                             
        if ( visible ) 
        {
            NotifyNotification *n =
            xfpm_notify_new(_("Xfce power manager"),
                            error,
                            12000,
                            NOTIFY_URGENCY_CRITICAL,
                            GTK_STATUS_ICON(adapter),
                            "gpm-ac-adapter");
            xfpm_notify_show_notification(n,0);                
        }  
        else
        {
            xfpm_battery_show_error(priv->batt,"gpm-ac-adapter",error);
        }
#else
    xfpm_popup_message(_("Xfce power manager"),error,GTK_MESSAGE_ERROR);
#endif    
    }
   
    XFPM_DEBUG("start \n");
    priv->ac_adapter_present = present;
    
#ifdef HAVE_DPMS    
    XFPM_DEBUG("Setting DPMS ac-adapter property\n");
    g_object_set(G_OBJECT(priv->dpms),"on-ac-adapter",priv->ac_adapter_present,NULL);
#endif
    if ( priv->cpufreq_control )    
    {
        g_object_set(G_OBJECT(priv->cpu),"on-ac-adapter",priv->ac_adapter_present,NULL);
    }
    g_object_set(G_OBJECT(priv->batt),"on-ac-adapter",priv->ac_adapter_present,NULL);
    if ( priv->lcd_brightness_control )
    {
        g_object_set(G_OBJECT(priv->lcd),"on-ac-adapter",priv->ac_adapter_present,NULL);
    }
    
    if ( priv->power_management & SYSTEM_CAN_POWER_SAVE ) 
    {
        xfpm_driver_set_power_save(drv);
    }
}                                                    

static void
_dialog_response_cb(GtkDialog *dialog, gint response, XfpmDriver *drv)
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    priv->dialog_opened = FALSE;
    
    switch(response) 
    {
            case GTK_RESPONSE_HELP:
				xfpm_help();
                break;
            default:
                gtk_widget_destroy(GTK_WIDGET(dialog));
                break;
    }
    
}              

static void
_close_dialog_cb(GtkDialog *dialog,XfpmDriver *drv)
{
    gpointer data = 
    g_object_get_data(G_OBJECT(drv),"conf-channel");
    if ( data )
    {
        XfconfChannel *channel = (XfconfChannel *)data;
        g_object_unref(channel);
    }
    
}

static void
_close_plug_cb(GtkWidget *plug,GdkEvent *ev,XfpmDriver *drv)
{
	XfpmDriverPrivate *priv;
	priv = XFPM_DRIVER_GET_PRIVATE(drv);
	priv->dialog_opened = FALSE;
	_close_dialog_cb(NULL,drv);
	gtk_widget_destroy(plug);
	
}

static void
xfpm_driver_property_changed_cb(XfconfChannel *channel,gchar *property,
                                GValue *value,XfpmDriver *drv)
{
    XFPM_DEBUG("%s \n",property);
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
    {
        XFPM_DEBUG(" invalid value type\n");
        return;
    }
    
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    
    if ( !strcmp(property,CRITICAL_BATT_CFG) ) 
    {
        guint val = g_value_get_uint(value);
        g_object_set(G_OBJECT(priv->batt),"critical-charge",val,NULL);
        return;
    }
    
    if ( !strcmp(property,CRITICAL_BATT_ACTION_CFG) ) 
    {
        guint val = g_value_get_uint(value);
        g_object_set(G_OBJECT(priv->batt),"critical-action",val,NULL);
        return;
    }
    
    if ( !strcmp(property,SHOW_TRAY_ICON_CFG) ) 
    {
        gboolean val = g_value_get_uint(value);
        g_object_set(G_OBJECT(priv->batt),"show-tray-icon",val,NULL);
        return;
    }
    
    if ( priv->cpufreq_control )
    {
        if ( !strcmp(property,CPU_FREQ_SCALING_CFG) ) 
        {
            gboolean val = g_value_get_boolean(value);
            g_object_set(G_OBJECT(priv->cpu),"cpu-freq",val,NULL);
            return;
        }
        
        if ( !strcmp(property,ON_AC_CPU_GOV_CFG) ) 
        {
            guint val = g_value_get_uint(value);
            g_object_set(G_OBJECT(priv->cpu),"on-ac-cpu-gov",val,NULL);
            return;
        }
        if ( !strcmp(property,ON_BATT_CPU_GOV_CFG) ) 
        {
            guint val = g_value_get_uint(value);
            g_object_set(G_OBJECT(priv->cpu),"on-batt-cpu-gov",val,NULL);
            return;
        }
    }
#ifdef HAVE_LIBNOTIFY    
    if ( !strcmp(property,BATT_STATE_NOTIFICATION_CFG) ) 
    {
        gboolean val = g_value_get_boolean(value);
        g_object_set(G_OBJECT(priv->batt),"enable-notification",val,NULL);
        return;
    }
#endif    

    if ( priv->lcd_brightness_control )
    {
        if ( !strcmp(property,LCD_BRIGHTNESS_CFG) )
        {
            gboolean val = g_value_get_boolean(value);
            g_object_set(G_OBJECT(priv->lcd),"brightness-enabled",val,NULL);
            return;
        }
    }
#ifdef HAVE_DPMS
    if ( !strcmp(property,DPMS_ENABLE_CFG) ) 
    {
        gboolean val = g_value_get_boolean(value);
        priv->dpms->dpms_enabled = val;
        g_object_notify(G_OBJECT(priv->dpms),"dpms");
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
        priv->dpms->on_batt_standby_timeout = value1 * 60;
        priv->dpms->on_batt_suspend_timeout = value2 * 60;
        priv->dpms->on_batt_off_timeout     = value3 * 60; 
        g_object_notify(G_OBJECT(priv->dpms),"dpms");
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
        
        priv->dpms->on_ac_standby_timeout = value1 * 60;
        priv->dpms->on_ac_suspend_timeout = value2 * 60;
        priv->dpms->on_ac_off_timeout     = value3 * 60;
        
        g_object_notify(G_OBJECT(priv->dpms),"dpms");
        return;
    }
#endif

    if ( priv->buttons_control )
    {
        if ( !strcmp(property,LID_SWITCH_CFG) ) 
        {
            guint val = g_value_get_uint(value);
            g_object_set(G_OBJECT(priv->bt),"lid-switch-action",val,NULL);
            return;
        }
        
        if ( !strcmp(property,SLEEP_SWITCH_CFG) ) 
        {
            guint val = g_value_get_uint(value);
            g_object_set(G_OBJECT(priv->bt),"sleep-switch-action",val,NULL);
            return;
        }
        
        if ( !strcmp(property,POWER_SWITCH_CFG) ) 
        {
            guint val = g_value_get_uint(value);
            g_object_set(G_OBJECT(priv->bt),"power-switch-action",val,NULL);
            return;
        }
    }
    
    if ( !strcmp(property,POWER_SAVE_CFG) )
    {
        gboolean val = g_value_get_boolean(value);
        priv->enable_power_save = val;
        xfpm_driver_set_power_save(drv);
    }
    
} 

static gboolean
xfpm_driver_show_options_dialog(XfpmDriver *drv)
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    if ( priv->dialog_opened )
    {
        return FALSE;
    }

    XfconfChannel *channel;
    GtkWidget *dialog;

    gboolean with_dpms;
    
    channel = xfconf_channel_new(XFPM_CHANNEL_CFG);
    g_object_set_data(G_OBJECT(drv),"conf-channel",channel);
    g_signal_connect(channel,"property-changed",
                     G_CALLBACK(xfpm_driver_property_changed_cb),drv);

    guint8 governors = 0;
    if ( priv->cpufreq_control )
    {    
       governors = xfpm_cpu_get_available_governors(priv->cpu);
    }

#ifdef HAVE_DPMS
    with_dpms = xfpm_dpms_capable(priv->dpms);
#else
    with_dpms = FALSE;
#endif

    guint8 switch_buttons = 0;
    if ( priv->buttons_control )
    {
        switch_buttons = xfpm_button_get_available_buttons(priv->bt);
    }
    gboolean ups_found = xfpm_battery_ups_found(priv->batt);
    
    dialog = xfpm_settings_new(channel,
                               priv->formfactor == SYSTEM_LAPTOP ? TRUE : FALSE,
                               priv->power_management,
                               with_dpms,
                               governors,
                               switch_buttons,
                               priv->lcd_brightness_control,
                               ups_found,
                               socket_id);
    if ( socket_id == 0 )
    {
    	xfce_gtk_window_center_on_monitor_with_pointer(GTK_WINDOW(dialog));
    	gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
    
    	gdk_x11_window_set_user_time(dialog->window,gdk_x11_get_server_time (dialog->window));
    
    	g_signal_connect(dialog,"response",G_CALLBACK(_dialog_response_cb),drv);        
    	g_signal_connect(dialog,"close",G_CALLBACK(_close_dialog_cb),drv);
    	gtk_widget_show(dialog);
    }	
    else
    {
    	g_signal_connect(dialog,"delete-event",G_CALLBACK(_close_plug_cb),drv);
		gdk_notify_startup_complete();
	}
    
    priv->dialog_opened = TRUE;
    socket_id = 0 ;
    
    return FALSE;
}

#ifdef HAVE_LIBNOTIFY
static void
xfpm_driver_report_sleep_errors(XfpmDriver *driver,const gchar *icon_name,const gchar *error)
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(driver);
    
    gboolean adapter_visible;
    g_object_get(G_OBJECT(priv->adapter),"visible",&adapter_visible,NULL);
    
    if ( adapter_visible )
    {
        NotifyNotification *n =
        xfpm_notify_new("Xfce power manager",
                         error,
                         14000,
                         NOTIFY_URGENCY_CRITICAL,
                         GTK_STATUS_ICON(priv->adapter),
                         icon_name);
        xfpm_notify_show_notification(n,0);
    }
    else  /* the battery object will take care */
    {
        xfpm_battery_show_error(priv->batt,icon_name,error);
    }
    
}
#endif

static gboolean
xfpm_driver_do_suspend(gpointer data)
{
    XfpmDriver *drv = XFPM_DRIVER(data);
    XfpmDriverPrivate *priv;
    GError *error = NULL;
    guint8 critical;
    
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    xfpm_hal_suspend(priv->hal,&error,&critical);
    if ( error )
    {
        XFPM_DEBUG("error suspend: %s\n",error->message);
#ifdef HAVE_LIBNOTIFY        
        if ( critical == 1)
        xfpm_driver_report_sleep_errors(drv,"gpm-suspend",error->message);
#endif        
        g_error_free(error);
    }
    
    priv->accept_sleep_request = TRUE;
	if ( priv->nm_responding )
		xfpm_dbus_send_nm_message("wake");
    
    return FALSE;
    
}

static gboolean
xfpm_driver_do_hibernate(gpointer data)
{
    XfpmDriver *drv = XFPM_DRIVER(data);
    XfpmDriverPrivate *priv;
    GError *error = NULL;
    guint8 critical;
    
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    xfpm_hal_hibernate(priv->hal,&error,&critical);
    
    if ( error )
    {
        XFPM_DEBUG("error hibernate: %s\n",error->message);
#ifdef HAVE_LIBNOTIFY        
        if ( critical == 1)
        xfpm_driver_report_sleep_errors(drv,"gpm-hibernate",error->message);
#endif        
        g_error_free(error);
    }
        
    priv->accept_sleep_request = TRUE;
	if ( priv->nm_responding )
		xfpm_dbus_send_nm_message("wake");
		
    return FALSE;
    
}

static gboolean
xfpm_driver_do_shutdown(gpointer data)
{
    XfpmDriver *drv = XFPM_DRIVER(data);
    XfpmDriverPrivate *priv;
    
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    if (!xfpm_hal_shutdown(priv->hal))
    {
#ifdef HAVE_LIBNOTIFY        
         xfpm_driver_report_sleep_errors(drv,"gpm-ac-adapter",_("System failed to shutdown"));
#endif         
    }
    
    priv->accept_sleep_request = TRUE;
    return FALSE;
}

static void
xfpm_driver_hibernate(XfpmDriver *drv,gboolean critical)
{
	XfpmDriverPrivate *priv;
	priv = XFPM_DRIVER_GET_PRIVATE(drv);
	
    xfpm_lock_screen();
	if ( priv->nm_responding )
		xfpm_dbus_send_nm_message("sleep");
		
    g_timeout_add_seconds(2,(GSourceFunc)xfpm_driver_do_hibernate,drv);
}

static void
xfpm_driver_suspend(XfpmDriver *drv,gboolean critical)
{
	XfpmDriverPrivate *priv;
	priv = XFPM_DRIVER_GET_PRIVATE(drv);
	
    xfpm_lock_screen();
	
	if ( priv->nm_responding )
		xfpm_dbus_send_nm_message("sleep");

    g_timeout_add_seconds(2,(GSourceFunc)xfpm_driver_do_suspend,drv);
}

static void
xfpm_driver_shutdown(XfpmDriver *drv,gboolean critical)
{
    g_timeout_add(100,(GSourceFunc)xfpm_driver_do_shutdown,drv); 
}

/* Currently the critical argument is ignored */
static void
xfpm_driver_handle_action_request(GObject *object,XfpmActionRequest action,
                                  gboolean critical,XfpmDriver *drv)
{
    g_return_if_fail(XFPM_IS_DRIVER(drv));
    
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
#ifdef DEBUG
    gchar *content;
    GValue value = { 0, };
    g_value_init(&value,XFPM_TYPE_ACTION_REQUEST);
    g_value_set_enum(&value,action);
    content = g_strdup_value_contents(&value);
        
    if ( XFPM_IS_BATTERY(object) ) XFPM_DEBUG("Action %s request from the Battery object\n",content);
    if ( XFPM_IS_AC_ADAPTER(object) ) XFPM_DEBUG("Action %s request from the adapter object\n",content);
    if ( XFPM_IS_BUTTON(object) ) XFPM_DEBUG("Action %s request from the Button object\n",content);
    
    g_free(content);
    
#endif    
    if ( priv->power_management == 0 )
    {
        XFPM_DEBUG("We cannot use power management interface\n");
        return;
    }
    if ( !priv->accept_sleep_request )
    {
        XFPM_DEBUG("Ignoring sleep request\n");
        return;
    }

    /* Block any other event here */   
    priv->accept_sleep_request = FALSE;    
    
    switch ( action )
    {
        case XFPM_DO_SUSPEND:
            if (priv->power_management & SYSTEM_CAN_SUSPEND )
                xfpm_driver_suspend(drv,critical);
            break;
        case XFPM_DO_HIBERNATE:
            if (priv->power_management & SYSTEM_CAN_HIBERNATE )
                xfpm_driver_hibernate(drv,critical);
            break;
        case XFPM_DO_SHUTDOWN:
            if ( priv->power_management & SYSTEM_CAN_SHUTDOWN ) 
                xfpm_driver_shutdown(drv,critical);
            break;    
        default:
            break;
    } 
}

static void
_show_adapter_icon(XfpmBattery *batt,gboolean show,XfpmDriver *drv)
{
    XFPM_DEBUG("start\n");
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    g_object_set(G_OBJECT(priv->adapter),"visible",show,NULL);
    
}

static void
_get_system_form_factor(XfpmDriverPrivate *priv)
{
    priv->formfactor = SYSTEM_UNKNOWN;
    
    gchar *factor = xfpm_hal_get_string_info(priv->hal,
                                            HAL_ROOT_COMPUTER,
                                            "system.formfactor",
                                            NULL);
    if (!factor ) return;
    
    if ( !strcmp(factor,"laptop") )
    {
        priv->formfactor = SYSTEM_LAPTOP;
    }
    else if ( !strcmp(factor,"desktop") )
    {
        priv->formfactor = SYSTEM_DESKTOP;
    }
    else if ( !strcmp(factor,"server") )
    {
        priv->formfactor = SYSTEM_SERVER;
    }
    else if ( !strcmp(factor,"unknown") )
    {
        priv->formfactor = SYSTEM_UNKNOWN;
    }
    libhal_free_string(factor);
}

static void
xfpm_driver_load_config(XfpmDriver *drv)    
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    XFPM_DEBUG("Loading configuration\n");
    
    XfconfChannel *channel;
    
    channel = xfconf_channel_new(XFPM_CHANNEL_CFG);
#ifdef HAVE_LIBNOTIFY
    priv->show_power_management_error
     = xfconf_channel_get_bool(channel,SHOW_POWER_MANAGEMENT_ERROR,TRUE);
#endif
    priv->enable_power_save = xfconf_channel_get_bool(channel,POWER_SAVE_CFG,FALSE);
    
    g_object_unref(channel);
}

#ifdef HAVE_LIBNOTIFY
static void
_disable_error(NotifyNotification *n,gchar *action,XfpmDriver *drv)
{
    if (strcmp(action,"confirmed")) return;
    
    XfconfChannel *channel;
    
    channel = xfconf_channel_new(XFPM_CHANNEL_CFG);
    xfconf_channel_set_bool(channel,SHOW_POWER_MANAGEMENT_ERROR,FALSE);
    
    g_object_unref(channel);
    g_object_unref(n);
}
#endif

#ifdef HAVE_LIBNOTIFY
static gboolean
_show_power_management_error_message(XfpmDriver *drv)
{
     const gchar *error =
                 _("Unable to use power management service, functionalities "\
				  "like hibernate and suspend will not work. "\
                  "Possible reasons: you don't have enough permission, "\
                  "broken connection with the hardware abstract layer "\
				  "or the message bus daemon is not running");
     NotifyNotification *n = xfpm_notify_new(_("Xfce power manager"),
                                             error,
                                             14000,
                                             NOTIFY_URGENCY_CRITICAL,
                                             NULL,
                                             "gpm-ac-adapter");
     xfpm_notify_add_action(n,
                            "confirmed",
                            _("Don't show this message again"),
                            (NotifyActionCallback)_disable_error,
                            drv);   
     xfpm_notify_show_notification(n,10);                        
     return FALSE;                       
}
#endif

static void xfpm_driver_check_nm(XfpmDriver *drv)
{
	XFPM_DEBUG("Checking Network Manager interface\n");
	
	XfpmDriverPrivate *priv;
	priv = XFPM_DRIVER_GET_PRIVATE(drv);
	
	DBusError error;
    DBusConnection *connection;

    dbus_error_init(&error);
    
    connection = dbus_bus_get(DBUS_BUS_SYSTEM,&error);
    
    if ( dbus_error_is_set(&error) )
    {
        XFPM_DEBUG("Error getting D-Bus connection: %s\n",error.message);
        dbus_error_free(&error);
        priv->nm_responding = FALSE;
        return;
    }    
    
    if ( !connection )
    {
        XFPM_DEBUG("Error, D-Bus connection NULL\n");
        priv->nm_responding = FALSE;
        return;
    }
    
	priv->nm_responding = xfpm_dbus_name_has_owner(connection,NM_SERVICE);
	dbus_connection_unref(connection);
	
}

static void
xfpm_driver_check_power_management(XfpmDriver *drv)
{
    XFPM_DEBUG("Checking power management\n");
    
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    GError *g_error = NULL;
    
    gboolean can_use_power_management = xfpm_hal_power_management_can_be_used(priv->hal);
    if ( !can_use_power_management )
    {
#ifdef HAVE_LIBNOTIFY
    g_timeout_add_seconds(8,(GSourceFunc)_show_power_management_error_message,drv);
#endif                                       
        XFPM_DEBUG("Unable to use HAL power management services\n");
        return;
    }
    priv->power_management |= SYSTEM_CAN_SHUTDOWN;
    
    gboolean can_suspend = xfpm_hal_get_bool_info(priv->hal,
                                               HAL_ROOT_COMPUTER,
                                               "power_management.can_suspend",
                                               &g_error);
    if ( g_error )
    {
        XFPM_DEBUG("%s: \n",g_error->message);
        g_error_free(g_error);
        g_error = NULL;
    }            
    
    if ( can_suspend )  priv->power_management |= SYSTEM_CAN_SUSPEND;
    
    gboolean can_hibernate = xfpm_hal_get_bool_info(priv->hal,
                                                    HAL_ROOT_COMPUTER,
                                                    "power_management.can_hibernate",
                                                    &g_error);
    if ( g_error )
    {
        XFPM_DEBUG("%s: \n",g_error->message);
        g_error_free(g_error);
    }       
    
    if ( can_hibernate )  priv->power_management |= SYSTEM_CAN_HIBERNATE;
}

static void
xfpm_driver_check_power_save(XfpmDriver *drv)
{
    XFPM_DEBUG("Checking power save availability\n");
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    GError *error = NULL;
    gboolean ret = xfpm_hal_get_bool_info(priv->hal,
                                          HAL_ROOT_COMPUTER,
                                          "power_management.is_powersave_set",
                                          &error);
    if ( error )
    {
        XFPM_DEBUG("error getting powersave_set %s\n",error->message);
        g_error_free(error);
        return;
    }
    
    xfpm_hal_set_power_save(priv->hal,ret,&error);
    
    if ( error )
    {
        XFPM_DEBUG("System doesn't support power save: %s\n",error->message);
        g_error_free(error);
        return;
    }
    
    if ( ret ) priv->power_management |= SYSTEM_CAN_POWER_SAVE;
    
}

static void
xfpm_driver_check(XfpmDriver *drv)
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    xfpm_driver_check_power_management(drv);
    
    if ( priv->power_management != 0 )
    {
        xfpm_driver_check_power_save(drv);
		/* We only check nm if we can use power management*/
		xfpm_driver_check_nm(drv);
    }
}

static void
xfpm_driver_load_all(XfpmDriver *drv)
{
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    GError *g_error = NULL;
    
#ifdef HAVE_DPMS    
    priv->dpms = xfpm_dpms_new();
#endif  

    // Load Cpu only if device exists
    gchar **cpu_udi = NULL;
    gint cpu_udi_num;
    cpu_udi = xfpm_hal_get_device_udi_by_capability(priv->hal,
                                                    "cpufreq_control",
                                                    &cpu_udi_num,
                                                    &g_error);
    if ( g_error )
    {
        XFPM_DEBUG("%s: \n",g_error->message);
        g_error_free(g_error);
        priv->cpufreq_control = FALSE;
    }
    
    if ( !cpu_udi || cpu_udi_num == 0 )
    {
        XFPM_DEBUG("Cpu control not found\n");
    }
    else 
    {
        priv->cpu = xfpm_cpu_new();
        priv->cpufreq_control = TRUE;
        libhal_free_string_array(cpu_udi);
    }
    
    priv->bt  = xfpm_button_new();
    // if no device found free the allocated memory
    guint8 buttons = xfpm_button_get_available_buttons(priv->bt);
    if ( buttons == 0 )
    {
        g_object_unref(priv->bt);
        priv->buttons_control = FALSE;
    }
    else
    {
        priv->buttons_control = TRUE;
        xfpm_button_set_sleep_info(priv->bt,priv->power_management);
        g_signal_connect(priv->bt,"xfpm-action-request",
                        G_CALLBACK(xfpm_driver_handle_action_request),drv);
    }
    
    priv->lcd = xfpm_lcd_brightness_new();
    priv->lcd_brightness_control = xfpm_lcd_brightness_device_exists(priv->lcd);
    
    if ( !priv->lcd_brightness_control )
    {
        g_object_unref(priv->lcd);
        
    }
    
    /*We call the battery object to start monitoring after loading the Ac adapter*/
    priv->batt = xfpm_battery_new(); 
    xfpm_battery_set_power_info(priv->batt,priv->power_management);
    
    g_signal_connect(priv->batt,"xfpm-action-request",
                        G_CALLBACK(xfpm_driver_handle_action_request),drv);
    g_signal_connect(priv->batt,"xfpm-show-adapter-icon",G_CALLBACK(_show_adapter_icon),drv);
                        
    priv->adapter = xfpm_ac_adapter_new(FALSE);
    xfpm_ac_adapter_set_sleep_info(XFPM_AC_ADAPTER(priv->adapter),priv->power_management);
    
    g_signal_connect(priv->adapter,"xfpm-ac-adapter-changed",
                    G_CALLBACK(xfpm_driver_ac_adapter_state_changed_cb),drv);
    g_signal_connect(priv->adapter,"xfpm-action-request",
                        G_CALLBACK(xfpm_driver_handle_action_request),drv);
                        
    /* This will give a signal concerning the AC adapter presence,
     * so we get the callback and then we set up the ac adapter
     * status for dpms and cpu lcd,...
     */                    
    xfpm_ac_adapter_monitor(XFPM_AC_ADAPTER(priv->adapter),priv->formfactor);

    xfpm_battery_monitor(priv->batt);
}

static void
xfpm_driver_send_reply(DBusConnection *conn,DBusMessage *mess)
{
    DBusMessage *reply;
    reply = dbus_message_new_method_return(mess);
    dbus_connection_send (conn, reply, NULL);
    dbus_connection_flush (conn);
    dbus_message_unref(reply);
}

static DBusHandlerResult xfpm_driver_signal_filter
    (DBusConnection *connection,DBusMessage *message,void *user_data) 
{
    XfpmDriver *drv = XFPM_DRIVER(user_data);
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    if ( dbus_message_is_signal(message,"xfpm.power.manager","Customize" ) )
    {
        /* don't block the signal filter so show the configuration dialog in a
         * timeout function otherwise xfce4-power-manager -q will have no effect*/
        DBusError error;
        dbus_error_init(&error);
        dbus_message_get_args(message,&error,DBUS_TYPE_UINT32,&socket_id,DBUS_TYPE_INVALID);
        if ( dbus_error_is_set(&error) )
        {
        	XFPM_DEBUG("Failed to get socket id: %s\n",error.message);
        	dbus_error_free(&error);
		}
		XFPM_DEBUG("message customize received with socket_id=%d\n",socket_id);
        g_timeout_add(100,(GSourceFunc)xfpm_driver_show_options_dialog,drv);
        return DBUS_HANDLER_RESULT_HANDLED;
    }    
    
    if ( dbus_message_is_signal(message,"xfpm.power.manager","Quit" ) )
    {
        XFPM_DEBUG("message quit received\n");
        xfpm_driver_send_reply(connection,message);      
        g_main_loop_quit(priv->loop);
        g_object_unref(drv);
        return DBUS_HANDLER_RESULT_HANDLED;
    }    
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

XfpmDriver *
xfpm_driver_new(void)
{
    XfpmDriver *driver = NULL;
    driver =  g_object_new(XFPM_TYPE_DRIVER,NULL);
    return driver;
}

gboolean
xfpm_driver_monitor (XfpmDriver *drv)
{
    g_return_val_if_fail (XFPM_IS_DRIVER(drv),FALSE);
    XFPM_DEBUG("starting xfpm manager\n");
    
    XfpmDriverPrivate *priv;
    priv = XFPM_DRIVER_GET_PRIVATE(drv);
    
    priv->loop = g_main_loop_new(NULL,FALSE);
    
    DBusError error;

    dbus_error_init(&error);
   
    priv->conn = dbus_bus_get(DBUS_BUS_SESSION,&error);
   
    if (!priv->conn ) 
    {
       g_critical("Failed to connect to the DBus daemon: %s\n",error.message);
       dbus_error_free(&error);
       return FALSE;
    }
        
    dbus_connection_setup_with_g_main(priv->conn,NULL);
    dbus_bus_add_match(priv->conn, "type='signal',interface='xfpm.power.manager'",NULL);
    dbus_connection_add_filter(priv->conn,xfpm_driver_signal_filter,drv,NULL);
    
    priv->hal = xfpm_hal_new();
    if ( !xfpm_hal_is_connected(priv->hal) )  
    {
        return FALSE;
    }
    
    GError *g_error = NULL;
    if ( !xfconf_init(&g_error) )
    {
        g_critical("xfconf init failed: %s using default settings\n",g_error->message);
        xfpm_popup_message(_("Xfce4 Power Manager"),_("Failed to load power manager configuration, "\
                            "using defaults"),GTK_MESSAGE_WARNING);
        g_error_free(g_error);
        
    }
    
    xfpm_dbus_register_name(priv->conn);
    
    xfpm_driver_load_config(drv);    
    _get_system_form_factor(priv);
    
    xfpm_driver_check(drv);

    xfpm_driver_load_all(drv);
    
    g_main_loop_run(priv->loop);
   
    xfpm_dbus_release_name(priv->conn);
      
    dbus_connection_remove_filter(priv->conn,xfpm_driver_signal_filter,NULL);
    
       
    xfconf_shutdown();
    
    return TRUE;
}
