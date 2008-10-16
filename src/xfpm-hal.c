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

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <glib/gi18n.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xfpm-hal.h"
#include "xfpm-debug.h"
#include "xfpm-dbus-messages.h"

#include "xfpm-marshal.h"

/* Init */
static void xfpm_hal_class_init(XfpmHalClass *klass);
static void xfpm_hal_init      (XfpmHal *xfpm_hal);
static void xfpm_hal_finalize  (GObject *object);

/*functions listner to HAL events*/
static void xfpm_hal_device_added                 (LibHalContext *ctx,
                                                 const gchar *udi);

static void xfpm_hal_device_removed               (LibHalContext *ctx,
                                                 const gchar *udi);

static void xfpm_hal_device_property_modified     (LibHalContext *ctx,
                                                 const gchar *udi,
                                                 const gchar *key,
                                                 dbus_bool_t is_removed,
                                                 dbus_bool_t is_added);
static void xfpm_hal_device_condition             (LibHalContext *ctx,
                                                 const gchar *udi,
                                                 const gchar *condition_name,
                                                 const gchar *condition_detail);
                                                 
static gboolean   xfpm_hal_monitor               (XfpmHal *xfpm_hal);
                                                        
#define XFPM_HAL_GET_PRIVATE(o)   \
(G_TYPE_INSTANCE_GET_PRIVATE((o),XFPM_TYPE_HAL,XfpmHalPrivate))

struct XfpmHalPrivate 
{
    
    DBusConnection *connection;
    LibHalContext *ctx;    
    gboolean connected;
    
};

enum 
{
    XFPM_DEVICE_ADDED,
    XFPM_DEVICE_REMOVED,
    XFPM_DEVICE_PROPERTY_CHANGED,
    XFPM_DEVICE_CONDITION,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0,};    

G_DEFINE_TYPE(XfpmHal,xfpm_hal,G_TYPE_OBJECT)

static void
xfpm_hal_class_init(XfpmHalClass *klass) {
    
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = xfpm_hal_finalize;
           
    g_type_class_add_private(klass,sizeof(XfpmHalPrivate));
    
    signals[XFPM_DEVICE_ADDED] = g_signal_new("xfpm-device-added",
					      					  XFPM_TYPE_HAL,
    									      G_SIGNAL_RUN_LAST,
    									      G_STRUCT_OFFSET(XfpmHalClass,device_added),
    									      NULL,NULL,
    									      g_cclosure_marshal_VOID__STRING,
    									      G_TYPE_NONE,1,G_TYPE_STRING);
    									                                                                               
    signals[XFPM_DEVICE_REMOVED] = g_signal_new("xfpm-device-removed",
                                                XFPM_TYPE_HAL,
    									        G_SIGNAL_RUN_LAST,
    									        G_STRUCT_OFFSET(XfpmHalClass,device_removed),
    									        NULL,NULL,
    									        g_cclosure_marshal_VOID__STRING,
    									        G_TYPE_NONE,1,G_TYPE_STRING);

	signals[XFPM_DEVICE_PROPERTY_CHANGED] = g_signal_new("xfpm-device-property-changed",
                                                          XFPM_TYPE_HAL,
                                                          G_SIGNAL_RUN_LAST,
                                                          G_STRUCT_OFFSET(XfpmHalClass,device_property_changed),
                                                          NULL,NULL,
                                                          _xfpm_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN,
                                                          G_TYPE_NONE,4,
                                                          G_TYPE_STRING,G_TYPE_STRING,
                                                          G_TYPE_BOOLEAN,G_TYPE_BOOLEAN);
                                                          
    signals[XFPM_DEVICE_CONDITION] = g_signal_new("xfpm-device-condition",
                                                  XFPM_TYPE_HAL,
                                                  G_SIGNAL_RUN_LAST,
                                                  G_STRUCT_OFFSET(XfpmHalClass,device_condition),
                                                  NULL,NULL,
                                                  _xfpm_marshal_VOID__STRING_STRING_STRING,
                                                  G_TYPE_NONE,3,
                                                  G_TYPE_STRING,G_TYPE_STRING,
                                                  G_TYPE_STRING);
      
}
    
static void
xfpm_hal_init(XfpmHal *xfpm_hal) {
    
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
        
    priv->ctx = NULL;
    
    // If this happens then nobody cares
    if ( !xfpm_hal_monitor(xfpm_hal) ) 
    {
        g_printerr("Error monitoring HAL events\n");
        priv->connected = FALSE;
        
    } 
    else
    {
        priv->connected = TRUE;
    }
}
    
static void
xfpm_hal_finalize(GObject *object) {
    
    XfpmHal *xfpm_hal;
    xfpm_hal = XFPM_HAL(object);
    xfpm_hal->priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    if ( xfpm_hal->priv->ctx ) 
    {
        libhal_ctx_shutdown(xfpm_hal->priv->ctx,NULL);
        libhal_ctx_free(xfpm_hal->priv->ctx);
    }  
    
    if ( xfpm_hal->priv->connection ) 
    {
        dbus_connection_unref(xfpm_hal->priv->connection);
    }  
    
    G_OBJECT_CLASS(xfpm_hal_parent_class)->finalize(object);
}
        
XfpmHal *
xfpm_hal_new(void) {
    
    XfpmHal *xfpm_hal = NULL;
    xfpm_hal = g_object_new (XFPM_TYPE_HAL,NULL);
    return xfpm_hal;    
    
}        

static void
xfpm_hal_device_added(LibHalContext *ctx,const gchar *udi) {
    
    XfpmHal *xfpm_hal = libhal_ctx_get_user_data(ctx);
    g_signal_emit(G_OBJECT(xfpm_hal),signals[XFPM_DEVICE_ADDED],0,udi);
    
}
    
static void
xfpm_hal_device_removed(LibHalContext *ctx,const gchar *udi) {
    
    XfpmHal *xfpm_hal = libhal_ctx_get_user_data(ctx);
    g_signal_emit(G_OBJECT(xfpm_hal),signals[XFPM_DEVICE_REMOVED],0,udi);
    
}

static void
xfpm_hal_device_property_modified(LibHalContext *ctx,const gchar *udi,
                                 const gchar *key,dbus_bool_t is_removed,
                                 dbus_bool_t is_added) {
    
    XfpmHal *xfpm_hal = libhal_ctx_get_user_data(ctx);
    g_signal_emit(G_OBJECT(xfpm_hal),signals[XFPM_DEVICE_PROPERTY_CHANGED],0,udi,key,is_removed,is_added);
    
}

static void xfpm_hal_device_condition            (LibHalContext *ctx,
                                                 const gchar *udi,
                                                 const gchar *condition_name,
                                                 const gchar *condition_detail) {
                                                     
    XfpmHal *xfpm_hal = libhal_ctx_get_user_data(ctx);
    g_signal_emit(G_OBJECT(xfpm_hal),signals[XFPM_DEVICE_CONDITION],0,udi,condition_name,condition_detail);                                                     
                                                     
}

static gboolean 
xfpm_hal_monitor(XfpmHal *xfpm_hal) {
    
    XfpmHalPrivate *priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    DBusError error;
    
    dbus_error_init(&error);
    
    priv->ctx = libhal_ctx_new();
    
    priv->connection = dbus_bus_get(DBUS_BUS_SYSTEM,&error);
    if ( dbus_error_is_set(&error) ) {
        g_printerr("Unable to connect to DBus %s\n",error.message);
        return FALSE;
    }    
    
    dbus_connection_setup_with_g_main(priv->connection,NULL);
    
    libhal_ctx_set_dbus_connection(priv->ctx,priv->connection);
    libhal_ctx_init(priv->ctx,&error);
    
    if ( dbus_error_is_set(&error) ) {
        g_printerr("Unable to connect to DBus %s\n",error.message);
        return FALSE;
    }    
    
    libhal_ctx_set_user_data(priv->ctx,xfpm_hal);    
    return TRUE;
}    

gboolean xfpm_hal_connect_to_signals(XfpmHal *hal,
                                    gboolean device_removed,
                                    gboolean device_added,
                                    gboolean device_property_changed,
                                    gboolean device_condition)
{
    g_return_val_if_fail(XFPM_IS_HAL(hal),FALSE);
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(hal);
    
    g_return_val_if_fail(priv->connected == TRUE,FALSE);

    DBusError error;    
    dbus_error_init(&error);
    
    if (device_added ) 
    {
        libhal_ctx_set_device_added(priv->ctx,xfpm_hal_device_added);
    }
    if ( device_removed )
    {
         libhal_ctx_set_device_removed(priv->ctx,xfpm_hal_device_removed);
    }
    if ( device_property_changed )
    {
        libhal_ctx_set_device_property_modified(priv->ctx,xfpm_hal_device_property_modified);
    }
    if ( device_condition )
    {
        libhal_ctx_set_device_condition(priv->ctx,xfpm_hal_device_condition);
    }

    libhal_device_property_watch_all(priv->ctx,&error);        

    if ( dbus_error_is_set(&error) ) {
        g_printerr("Unable to watch device using HAL %s\n",error.message);
        return FALSE;
    }
    
    return TRUE;
}

gchar **
xfpm_hal_get_device_udi_by_capability(XfpmHal *xfpm_hal,const gchar *capability,
                                      gint *num,GError **gerror) {

    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),NULL);
    XfpmHalPrivate *priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,NULL);
    
    gchar **udi_info = NULL;
    DBusError error;
        
    dbus_error_init(&error);
    udi_info = libhal_find_device_by_capability(priv->ctx, 
                                                capability,
                                                num,
                                                &error);
    if ( dbus_error_is_set(&error) ) {
        g_printerr("Cannot get device by capability: %s\n",error.message);
        dbus_set_g_error(gerror,&error);
        dbus_error_free(&error);
        return NULL;
    } else {
        return udi_info;
    }    
}    

gint
xfpm_hal_get_int_info(XfpmHal *xfpm_hal,const gchar *udi,
                      const gchar *property,GError **gerror)  {
 
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),-1);
    XfpmHalPrivate *priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,-1);
    
    DBusError error;
    gint ret;
    
    dbus_error_init(&error);
        
    ret = libhal_device_get_property_int(priv->ctx,
                                         udi,
                                         property,
                                         &error);
                                         
     
    if ( dbus_error_is_set(&error)) {
        g_printerr("Cannot get device int info : %s\n",error.message);
        dbus_set_g_error(gerror,&error);
        dbus_error_free(&error);
        return -1;
    } else {
        return ret;
    }    
    
}    

gchar *
xfpm_hal_get_string_info(XfpmHal *xfpm_hal,const gchar *udi,
                        const gchar *property,GError **gerror)  {
 
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),NULL);
    XfpmHalPrivate *priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,NULL);
    
    DBusError error;
    gchar *ret;
    
    dbus_error_init(&error);
        
    ret = libhal_device_get_property_string(priv->ctx,
                                            udi,
                                            property,
                                            &error);
                                         
     
    if ( dbus_error_is_set(&error) ) {
        g_printerr("Cannot get device string info : %s\n",error.message);
        dbus_set_g_error(gerror,&error);
        dbus_error_free(&error);
        return NULL;
    } else {
        return ret;
    }    
    
}    

gboolean
xfpm_hal_get_bool_info(XfpmHal *xfpm_hal,const gchar *udi,
                        const gchar *property,GError **gerror)  {
 
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),FALSE);
    XfpmHalPrivate *priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,FALSE);
    
    DBusError error;
    gboolean ret;
    
    dbus_error_init(&error);
        
    ret = libhal_device_get_property_bool(priv->ctx,
                                          udi,
                                          property,
                                          &error);
                                         
     
    if ( dbus_error_is_set(&error)) {
        g_printerr("Cannot get device bool info : %s\n",error.message);
        dbus_set_g_error(gerror,&error);
        dbus_error_free(&error);
        return FALSE;
    } else {
        return ret;
    }    
    
}    

gboolean
xfpm_hal_device_have_key (XfpmHal *xfpm_hal,const gchar *udi,
                         const gchar *key)
{
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),FALSE);
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,FALSE);
    
    DBusError error;
    
    dbus_error_init(&error);
    
    gboolean ret = libhal_device_property_exists(priv->ctx,
                                                udi,
                                                key,
                                                &error);
    if ( dbus_error_is_set(&error) )
    {
        g_printerr("Error getting device property %s\n",error.message);
        dbus_error_free(&error);    
        return FALSE;
    }
    
    return ret;
}

gboolean             
xfpm_hal_device_have_capability(XfpmHal *xfpm_hal,
                               const gchar *udi,
                               const gchar *capability)
{
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),FALSE);
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,FALSE);
    
    DBusError error;
    dbus_error_init(&error);
    
    gboolean ret = libhal_device_query_capability(priv->ctx,
                                                  udi,
                                                  capability,
                                                  &error);
                                                  
     if ( dbus_error_is_set(&error) )
    {
        g_printerr("Error query device capability %s\n",error.message);
        dbus_error_free(&error);    
        return FALSE;
    }
    
    return ret;
}

gboolean xfpm_hal_shutdown(XfpmHal *xfpm_hal)
{
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),FALSE);
     
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,FALSE);
    
    DBusMessage *mess;
	
	mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
					             HAL_ROOT_COMPUTER,
					             HAL_DBUS_INTERFACE_POWER,
					             "Shutdown");
	
	if (!mess) {
		return FALSE;
	}	
		
	if(!dbus_connection_send(priv->connection,
							 mess,
							 NULL)) {
		return FALSE;
	}						 
    return TRUE;
}

static gboolean
_filter_error_message(const gchar *error)
{
    if(!strcmp("No back-end for your operating system",error))
    {
        return TRUE;
    }
    else if (!strcmp("No hibernate script found",error) )
    {
        return TRUE;
    }
    else if (!strcmp("No suspend method found",error) )
    {
        return TRUE;
    }
    else if (!strcmp("No hibernate method found",error))
    {
        return TRUE;
    }
    return FALSE;
}

gboolean   
xfpm_hal_hibernate(XfpmHal *xfpm_hal,GError **gerror,guint8 *critical) 
{
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),FALSE);

    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,FALSE);
    
	DBusMessage *mess,*reply;
	DBusError error;
	gint exit_code;
    
	mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
                                 HAL_ROOT_COMPUTER,
                                 HAL_DBUS_INTERFACE_POWER,
                                 "Hibernate");
	if (!mess) 
	{
	    *critical = 1;
	    g_set_error(gerror,0,0,_("Out of memmory"));
		return FALSE;
	}	
	
	dbus_error_init(&error);

	reply = dbus_connection_send_with_reply_and_block(priv->connection,
                                                    mess,
                                                    SLEEP_TIMEOUT,
                                                    &error);
    dbus_message_unref(mess);
             	
    if ( dbus_error_is_set(&error) )
    {
        XFPM_DEBUG("error=%s\n",error.message);
        dbus_set_g_error(gerror,&error);
        if ( _filter_error_message(error.message) )
        {
            *critical = 1;
        }
        else 
        {
            *critical = 0;
        }
        dbus_error_free(&error);
        return FALSE;
    }
    
    if ( !reply ) 
	{						  		       
	    critical = 0;
	    g_set_error(gerror,0,0,_("Message Hibernate didn't get a reply"));
	    return FALSE;
    }

    *critical = 1;
    switch(dbus_message_get_type(reply)) {
    
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
        dbus_message_get_args(reply,NULL,
                              DBUS_TYPE_INT32,
                              &exit_code,
                              DBUS_TYPE_INVALID);
        XFPM_DEBUG("MESSAGE_TYPE_RETURN exit_code=%d\n",exit_code);
        dbus_message_unref(reply);
        if ( exit_code == 0 )
        {
            *critical = 0;
            return TRUE;
        }
        if ( exit_code > 1 ) 
        {
            g_set_error(gerror,0,0,_("System failed to hibernate"));
            return FALSE;
        }                 
        break;
    case DBUS_MESSAGE_TYPE_ERROR:
        dbus_message_unref(reply);
        g_set_error(gerror,0,0,_("Error occured while trying to suspend"));
        return FALSE;	
    default:
        g_set_error(gerror,0,0,_("Unknow reply from the message daemon"));
        dbus_message_unref(reply);
        return FALSE;	
    }
    return TRUE;
}

gboolean            
xfpm_hal_suspend(XfpmHal *xfpm_hal,GError **gerror,guint8 *critical) 
{
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),FALSE);
     
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,FALSE);
    
	DBusMessage *mess,*reply;
	DBusError error;
	int seconds= 2;
	gint exit_code;

	mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
                                 HAL_ROOT_COMPUTER,
                                 HAL_DBUS_INTERFACE_POWER,
                                 "Suspend");
	if (!mess) 
	{
	    g_set_error(gerror,0,0,_("Out of memmory"));
	    *critical = 1;
		return FALSE;
	}	
	
	dbus_message_append_args(mess,DBUS_TYPE_INT32,&seconds,DBUS_TYPE_INVALID);

    dbus_error_init(&error);
	reply = dbus_connection_send_with_reply_and_block(priv->connection,
                                                    mess,
                                                    SLEEP_TIMEOUT,
                                                    &error);
    dbus_message_unref(mess);
         	
    if ( dbus_error_is_set(&error) )
    {
        XFPM_DEBUG("error=%s\n",error.message);
        dbus_set_g_error(gerror,&error);
        if ( _filter_error_message(error.message) )
        {
            *critical = 1;
        }
        else 
        {
            *critical = 0;
        }
        dbus_error_free(&error);
        return FALSE;
    }
    
    if ( !reply ) 
	{						  		       
	    g_set_error(gerror,0,0,_("Message suspend didn't get a reply"));
	    *critical = 0;
	    return FALSE;
    }

    *critical = 1;
    switch(dbus_message_get_type(reply)) {
    
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
        dbus_message_get_args(reply,NULL,
                              DBUS_TYPE_INT32,
                              &exit_code,
                              DBUS_TYPE_INVALID);
        XFPM_DEBUG("MESSAGE_TYPE_RETURN exit_code=%d\n",exit_code);
        dbus_message_unref(reply);
        if ( exit_code == 0 ) 
        {
            *critical = 0;
            return TRUE;
        }
        if ( exit_code > 1 ) 
        {
            g_set_error(gerror,0,0,_("System failed to suspend"));
            return FALSE;
        }                 
        break;
    case DBUS_MESSAGE_TYPE_ERROR:
        dbus_message_unref(reply);
        g_set_error(gerror,0,0,_("Error occured while trying to suspend"));
        return FALSE;	
    default:
        g_set_error(gerror,0,0,_("Unknow reply from the message daemon"));
        dbus_message_unref(reply);
        return FALSE;	
    }
    return TRUE;
}	

void
xfpm_hal_set_brightness (XfpmHal *xfpm_hal,
                         const gchar *interface,
                         gint32 level,
                         GError **gerror)
                                                            
{
    g_return_if_fail(XFPM_IS_HAL(xfpm_hal));
    
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_if_fail(priv->connected == TRUE);
    
    DBusMessage *mess,*reply;
	DBusError error;

	mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
                                 interface,
                                 HAL_DBUS_INTERFACE_LCD,
                                 "SetBrightness");
	if (!mess) 
	{
	    g_set_error(gerror,0,0,_("Out of memmory"));
		return;
	}	
    
    dbus_message_append_args(mess,DBUS_TYPE_INT32,&level,DBUS_TYPE_INVALID);
        
    dbus_error_init(&error);
    
    reply = dbus_connection_send_with_reply_and_block(priv->connection,mess,-1,&error);
    dbus_message_unref(mess);
    
    if ( dbus_error_is_set(&error) )
    {
         dbus_set_g_error(gerror,&error);
         dbus_error_free(&error);
         return;
    }
        
    if ( !reply ) 
    {
        g_set_error(gerror,0,0,_("No reply from HAL daemon to set monitor brightness level"));
        return;
    }
    
    dbus_message_unref(reply);
    return;
}

gint32
xfpm_hal_get_brightness (XfpmHal *xfpm_hal,
                        const gchar *interface,
                        GError **gerror)
{
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),-1);
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,-1);
    
    DBusMessage *mess,*reply;
	DBusError error;
	gint32 brightness_level;

	mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
                                 interface,
                                 HAL_DBUS_INTERFACE_LCD,
                                 "GetBrightness");
	if (!mess) 
	{
	    g_set_error(gerror,0,0,_("Out of memmory"));
		return -1;
	}	
    
    dbus_error_init(&error);
    
    reply = dbus_connection_send_with_reply_and_block(priv->connection,mess,-1,&error);
    dbus_message_unref(mess);
        
    if ( dbus_error_is_set(&error) )
    {
         dbus_set_g_error(gerror,&error);
         dbus_error_free(&error);
         return -1;
    }
        
    if ( !reply ) 
    {
        g_set_error(gerror,0,0,_("No reply from HAL daemon to get monitor brightness level"));
        return -1;
    }
    
    dbus_message_get_args(reply,NULL,DBUS_TYPE_INT32,&brightness_level);
    dbus_message_unref(reply);
    
    return brightness_level;
}

gchar               
**xfpm_hal_get_available_cpu_governors(XfpmHal *xfpm_hal,GError **gerror)
{
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),NULL);
     
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,NULL);
    
    DBusMessage *mess;
    DBusMessage *reply;
    DBusError error;
    gchar **govs = NULL;
    int dummy;
    
    mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
                                 HAL_ROOT_COMPUTER,
                                 HAL_DBUS_INTERFACE_CPU,
                                 "GetCPUFreqAvailableGovernors");
    if (!mess) 
	{
	    g_set_error(gerror,0,0,_("Out of memmory"));
		return NULL;
	}	
	
    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block(priv->connection,mess,-1,&error);

    dbus_message_unref(mess);
         
    if ( dbus_error_is_set(&error) )
    {
         dbus_set_g_error(gerror,&error);
         dbus_error_free(&error);
         return NULL;
    }
        
    if ( !reply ) 
    {
        g_set_error(gerror,0,0,_("No reply from HAL daemon to get available cpu governors"));
        return NULL;
    }
    
    dbus_message_get_args(reply,NULL,
                          DBUS_TYPE_ARRAY,DBUS_TYPE_STRING,
                          &govs,&dummy,
                          DBUS_TYPE_INVALID,DBUS_TYPE_INVALID);
    dbus_message_unref(reply);                      
    return govs;
}

gchar                
*xfpm_hal_get_current_cpu_governor(XfpmHal *xfpm_hal,GError **gerror)
{
    g_return_val_if_fail(XFPM_IS_HAL(xfpm_hal),NULL);
     
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_val_if_fail(priv->connected == TRUE,NULL);
    
    DBusMessage *mess;
    DBusMessage *reply;
    DBusError error;
    gchar *gov = NULL;
    
    mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
                                 HAL_ROOT_COMPUTER,
                                 HAL_DBUS_INTERFACE_CPU,
                                 "GetCPUFreqGovernor");
    
    if (!mess) 
	{
	    g_set_error(gerror,0,0,_("Out of memmory"));
		return NULL;
	}	
	
    dbus_error_init(&error);
    
    reply = dbus_connection_send_with_reply_and_block(priv->connection,mess,-1,&error);
    dbus_message_unref(mess);
    
    if ( dbus_error_is_set(&error) )
    {
         dbus_set_g_error(gerror,&error);
         dbus_error_free(&error);
         return NULL;
    }
    
    if ( !reply ) 
    {
        g_set_error(gerror,0,0,_("No reply from HAL daemon to get current cpu governor"));
        return NULL;
    }

    dbus_message_get_args(reply,NULL,DBUS_TYPE_STRING,&gov,DBUS_TYPE_INVALID);
    XFPM_DEBUG("Got governor %s\n",gov);
    dbus_message_unref(reply);
    return gov;
}


void 
xfpm_hal_set_cpu_governor (XfpmHal *xfpm_hal,
                           const gchar *governor,
                           GError **gerror)
{
    g_return_if_fail(XFPM_IS_HAL(xfpm_hal));
     
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_if_fail(priv->connected == TRUE);
    
    XFPM_DEBUG("Setting CPU gov %s\n",governor);
    
    DBusMessage *mess;
    DBusMessage *reply;
    DBusError error;
    mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
                                 HAL_ROOT_COMPUTER,
                                 HAL_DBUS_INTERFACE_CPU,
                                 "SetCPUFreqGovernor");
    if (!mess) 
	{
	    g_set_error(gerror,0,0,_("Out of memmory"));
		return;
	}	
	                                                            
    dbus_message_append_args(mess,DBUS_TYPE_STRING,&governor,DBUS_TYPE_INVALID);
    
    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block(priv->connection,mess,-1,&error);
    dbus_message_unref(mess);

    if ( !reply ) 
    {
        g_set_error(gerror,0,0,_("No reply from HAL daemon to set cpu governor"));
        return;
    }
    dbus_message_unref(reply);
}

void                 
xfpm_hal_set_power_save (XfpmHal *xfpm_hal,
                         gboolean power_save,
                         GError **gerror)
{
    g_return_if_fail(XFPM_IS_HAL(xfpm_hal));
    
    XfpmHalPrivate *priv;
    priv = XFPM_HAL_GET_PRIVATE(xfpm_hal);
    
    g_return_if_fail(priv->connected == TRUE);
    
    DBusMessage *mess;
    DBusMessage *reply;
    DBusError error;
    mess = xfpm_dbus_new_message(HAL_DBUS_SERVICE,
                                 HAL_ROOT_COMPUTER,
                                 HAL_DBUS_INTERFACE_POWER,
                                 "SetPowerSave");
    if (!mess) 
	{
	    g_set_error(gerror,0,0,_("Out of memmory"));
		return;
	}	
	                                                            
    dbus_message_append_args(mess,DBUS_TYPE_BOOLEAN,&power_save,DBUS_TYPE_INVALID);
    
    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block(priv->connection,mess,-1,&error);
    dbus_message_unref(mess);

    if ( !reply ) 
    {
        g_set_error(gerror,0,0,_("No reply from HAL daemon to set power save profile"));
        return;
    }
    dbus_message_unref(reply);
}                                                            
