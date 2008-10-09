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

#include "xfpm-dpms.h"

#ifdef HAVE_DPMS

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

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <glib/gi18n.h>

#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/dpmsstr.h>

#include <xfconf/xfconf.h>

#include "xfpm-common.h"
#include "xfpm-debug.h"

#define DPMS_TIMEOUT 120

#define XFPM_DPMS_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE((o),XFPM_TYPE_DPMS,XfpmDpmsPrivate))

static void xfpm_dpms_init        (XfpmDpms *dpms);
static void xfpm_dpms_class_init  (XfpmDpmsClass *klass);
static void xfpm_dpms_finalize    (GObject *object);

static void xfpm_dpms_set_property(GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec);
static void xfpm_dpms_get_property(GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec);
static void xfpm_dpms_load_config (XfpmDpms *dpms);                                   
static gboolean xfpm_dpms_set_dpms_mode(XfpmDpms *dpms);

static void xfpm_dpms_set_timeouts(XfpmDpms *dpms);

static void xfpm_dpms_notify_cb        (GObject *object,
                                        GParamSpec *arg1,
                                        gpointer data);  
struct XfpmDpmsPrivate
{
    gboolean dpms_capable;
};

G_DEFINE_TYPE(XfpmDpms,xfpm_dpms,G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_AC_ADAPTER,
    PROP_DPMS
};

static void xfpm_dpms_class_init(XfpmDpmsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->set_property = xfpm_dpms_set_property;
    object_class->get_property = xfpm_dpms_get_property;
    
    object_class->finalize = xfpm_dpms_finalize;
    
    g_object_class_install_property(object_class,
                                    PROP_AC_ADAPTER,
                                    g_param_spec_boolean("on-ac-adapter",
                                                         "On ac adapter",
                                                         "On Ac power",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
                                                         
    g_object_class_install_property(object_class,
                                    PROP_DPMS,
                                    g_param_spec_boolean("dpms",
                                                         "dpms",
                                                         "dpms settings",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
                                                         
    g_type_class_add_private(klass,sizeof(XfpmDpmsPrivate));
    
}

static void xfpm_dpms_init(XfpmDpms *dpms)
{
    XfpmDpmsPrivate *priv;
    priv = XFPM_DPMS_GET_PRIVATE(dpms);
    
    priv->dpms_capable = DPMSCapable(GDK_DISPLAY());
    
    xfpm_dpms_load_config(dpms);
    
    g_signal_connect(G_OBJECT(dpms),"notify",G_CALLBACK(xfpm_dpms_notify_cb),NULL);
    g_timeout_add_seconds(DPMS_TIMEOUT,(GSourceFunc)xfpm_dpms_set_dpms_mode,dpms);
    
}

static void xfpm_dpms_set_property(GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
#ifdef DEBUG
    gchar *content;
    content = g_strdup_value_contents(value);
    XFPM_DEBUG("param:%s value contents:%s\n",pspec->name,content);
    g_free(content);
#endif      
    XfpmDpms *dpms;
    dpms = XFPM_DPMS(object);
    
    switch (prop_id)
    {
    case PROP_AC_ADAPTER:
        dpms->ac_adapter_present = g_value_get_boolean(value);
        break;   
    case PROP_DPMS:
        dpms->dpms_enabled = g_value_get_boolean(value);
        break;    
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
        break;
    }
}    
                               
static void xfpm_dpms_get_property(GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    XfpmDpms *dpms;
    dpms = XFPM_DPMS(object);
        
    switch (prop_id)
    {
    case PROP_AC_ADAPTER:
        g_value_set_boolean(value,dpms->ac_adapter_present);
        break;   
    case PROP_DPMS:
        g_value_set_boolean(value,dpms->dpms_enabled);
        break;    
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
        break;
    }                         
    
#ifdef DEBUG
    gchar *content;
    content = g_strdup_value_contents(value);
    XFPM_DEBUG("param:%s value contents:%s\n",pspec->name,content);
    g_free(content);
#endif  
}

static void xfpm_dpms_finalize(GObject *object)
{
    XfpmDpms *dpms;
    dpms = XFPM_DPMS(object);
    dpms->priv = XFPM_DPMS_GET_PRIVATE(dpms);
    
    G_OBJECT_CLASS(xfpm_dpms_parent_class)->finalize(object);
    
}

static void xfpm_dpms_load_config (XfpmDpms *dpms)
{
    XFPM_DEBUG("loading configuration\n");
    XfconfChannel *channel;
    
    GError *g_error = NULL;
    if ( !xfconf_init(&g_error) )
    {
        g_critical("xfconf init failed: %s\n",g_error->message);
        XFPM_DEBUG("Using default values\n");
        g_error_free(g_error);
        dpms->on_ac_standby_timeout = 1800;
        dpms->on_ac_suspend_timeout = 2700;
        dpms->on_ac_off_timeout = 3600;
        dpms->on_batt_standby_timeout = 180;
        dpms->on_batt_suspend_timeout = 240;
        dpms->on_batt_off_timeout = 300;
        dpms->dpms_enabled = TRUE;
        return;
    }
    
    channel = xfconf_channel_new(XFPM_CHANNEL_CFG);
    
    GPtrArray *arr;
    GValue *value;
    arr = xfconf_channel_get_arrayv(channel,ON_AC_DPMS_TIMEOUTS_CFG);
    if ( arr ) 
    {
        value = g_ptr_array_index(arr,0);
        dpms->on_ac_standby_timeout = g_value_get_uint(value)*60;
        
        value = g_ptr_array_index(arr,1);
        dpms->on_ac_suspend_timeout = g_value_get_uint(value)*60;
        
        value = g_ptr_array_index(arr,2);
        dpms->on_ac_off_timeout = g_value_get_uint(value)*60;
        xfconf_array_free(arr);
    } 
    else
    {
        dpms->on_ac_standby_timeout = 1800;
        dpms->on_ac_suspend_timeout = 2700;
        dpms->on_ac_off_timeout = 3600;
    }
    
    arr = xfconf_channel_get_arrayv(channel,ON_BATT_DPMS_TIMEOUTS_CFG);
    if ( arr ) 
    {
        value = g_ptr_array_index(arr,0);
        dpms->on_batt_standby_timeout = g_value_get_uint(value)*60;
        
        value = g_ptr_array_index(arr,1);
        dpms->on_batt_suspend_timeout = g_value_get_uint(value)*60;
        
        value = g_ptr_array_index(arr,2);
        dpms->on_batt_off_timeout = g_value_get_uint(value)*60;
        xfconf_array_free(arr);
    } 
    else
    {
        dpms->on_batt_standby_timeout = 180;
        dpms->on_batt_suspend_timeout = 240;
        dpms->on_batt_off_timeout = 300;
    }
    dpms->dpms_enabled  = xfconf_channel_get_bool(channel,DPMS_ENABLE_CFG,TRUE);
    
    g_object_unref(channel);
    xfconf_shutdown();
}

static gboolean
xfpm_dpms_set_dpms_mode(XfpmDpms *dpms)
{
    XfpmDpmsPrivate *priv;
    priv = XFPM_DPMS_GET_PRIVATE(dpms);
    
    if ( !priv->dpms_capable )
    {
        return FALSE; /* We stop asking for the dpms state*/
    }
    
    BOOL on_off;
    CARD16 state = 0;    
    
    DPMSInfo(GDK_DISPLAY(),&state,&on_off);

    if ( !on_off && dpms->dpms_enabled )
    {
        XFPM_DEBUG("DPMS is disabled, enabling it: user settings\n");
        DPMSEnable(GDK_DISPLAY());
        g_object_notify(G_OBJECT(dpms),"dpms");
    } 
    else if ( on_off && !dpms->dpms_enabled )
    {
        XFPM_DEBUG("DPMS is enabled, disabling it: user settings\n");
        DPMSDisable(GDK_DISPLAY());
    }
    
    return TRUE;
}

static void
xfpm_dpms_set_timeouts(XfpmDpms *dpms)
{
    CARD16 x_standby = 0 ,x_suspend = 0,x_off = 0;
    DPMSGetTimeouts(GDK_DISPLAY(),&x_standby,&x_suspend,&x_off);
    
    if ( dpms->ac_adapter_present )
    {
        if ( x_standby != dpms->on_ac_standby_timeout ||
             x_suspend != dpms->on_ac_suspend_timeout ||
             x_off != dpms->on_ac_off_timeout )
        {
            XFPM_DEBUG("Setting timeout ac-adapter is present,standby=%d suspend=%d off=%d\n",
                      dpms->on_ac_standby_timeout,dpms->on_ac_suspend_timeout,dpms->on_ac_off_timeout);
            DPMSSetTimeouts(GDK_DISPLAY(),dpms->on_ac_standby_timeout,dpms->on_ac_suspend_timeout,
                                   dpms->on_ac_off_timeout);
        }
    }
    else if ( x_standby != dpms->on_batt_standby_timeout ||
             x_suspend != dpms->on_batt_suspend_timeout  ||
             x_off != dpms->on_batt_off_timeout )
    {
        {
            XFPM_DEBUG("Setting timeout ac-adapter not present,standby=%d suspend=%d off=%d\n",
                      dpms->on_batt_standby_timeout,dpms->on_batt_suspend_timeout,dpms->on_batt_off_timeout);
            DPMSSetTimeouts(GDK_DISPLAY(),dpms->on_batt_standby_timeout,dpms->on_batt_suspend_timeout,
                                   dpms->on_batt_off_timeout);
        }
    }
}

static void
xfpm_dpms_notify_cb(GObject *object,GParamSpec *arg1,gpointer data)
{
    XfpmDpms *dpms;
    XfpmDpmsPrivate *priv;
    
    dpms = XFPM_DPMS(object);
    priv = XFPM_DPMS_GET_PRIVATE(dpms);
    XFPM_DEBUG("dpms callback\n");
    
    if ( !priv->dpms_capable ) 
    {
        XFPM_DEBUG("dpms incapable\n");
        return;
    }
    
    if ( !strcmp(arg1->name,"dpms") )
    {
        xfpm_dpms_set_dpms_mode(dpms);
    }
    
    if ( dpms->dpms_enabled )
    {
            xfpm_dpms_set_timeouts(dpms);
    }
}

XfpmDpms *
xfpm_dpms_new(void)
{
    XfpmDpms *dpms = NULL;
    dpms = g_object_new(XFPM_TYPE_DPMS,NULL);
    return dpms;
}

gboolean   
xfpm_dpms_capable (XfpmDpms *dpms)
{
    XfpmDpmsPrivate *priv;
    priv = XFPM_DPMS_GET_PRIVATE(dpms);
    return priv->dpms_capable;
}

#endif
