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


#include <gtk/gtkstatusicon.h>

#include "xfpm-hal.h"
#include "xfpm-ac-adapter.h"
#include "xfpm-common.h"

#include "xfpm-debug.h"

#define XFPM_AC_ADAPTER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE(o,XFPM_TYPE_AC_ADAPTER,XfpmAcAdapterPrivate))

static void xfpm_ac_adapter_init(XfpmAcAdapter *adapter);
static void xfpm_ac_adapter_class_init(XfpmAcAdapterClass *klass);
static void xfpm_ac_adapter_finalize(GObject *object);

static gboolean xfpm_ac_adapter_size_changed_cb(GtkStatusIcon *adapter,
                                                gint size,
                                                gpointer data);
static void xfpm_ac_adapter_get_adapter(XfpmAcAdapter *adapter);
static void xfpm_ac_adapter_property_changed_cb(XfpmHal *hal,const gchar *udi,
                                                const gchar *key,gboolean is_removed,
                                                gboolean is_added,XfpmAcAdapter *adapter);

struct XfpmAcAdapterPrivate
{
    XfpmHal *hal;
    gboolean present;
};

enum 
{
    XFPM_AC_ADAPTER_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0,};    

G_DEFINE_TYPE(XfpmAcAdapter,xfpm_ac_adapter,GTK_TYPE_STATUS_ICON)

static void
xfpm_ac_adapter_class_init(XfpmAcAdapterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    
    gobject_class->finalize = xfpm_ac_adapter_finalize;
    
    signals[XFPM_AC_ADAPTER_CHANGED] = g_signal_new("xfpm-ac-adapter-changed",
                                                   XFPM_TYPE_AC_ADAPTER,
                                                   G_SIGNAL_RUN_LAST,
                                                   G_STRUCT_OFFSET(XfpmAcAdapterClass,ac_adapter_changed),
                                                   NULL,NULL,
                                                   g_cclosure_marshal_VOID__BOOLEAN,
                                                   G_TYPE_NONE,1,G_TYPE_BOOLEAN);
    
    g_type_class_add_private(klass,sizeof(XfpmAcAdapterPrivate));
    
}

static void
xfpm_ac_adapter_init(XfpmAcAdapter *adapter)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    
    priv->hal = xfpm_hal_new();
    
    g_signal_connect(priv->hal,"xfpm-device-property-changed",
                     G_CALLBACK(xfpm_ac_adapter_property_changed_cb),adapter);
    
    g_signal_connect(adapter,"size-changed",
                    G_CALLBACK(xfpm_ac_adapter_size_changed_cb),NULL);
}

static void
xfpm_ac_adapter_finalize(GObject *object)
{
    XfpmAcAdapter *adapter = XFPM_AC_ADAPTER(object);
    adapter->priv = XFPM_AC_ADAPTER_GET_PRIVATE(object);
   
    if ( adapter->priv->hal )
    {
        g_object_unref(adapter->priv->hal);
    }
    
    G_OBJECT_CLASS(xfpm_ac_adapter_parent_class)->finalize(object);
}

static gboolean
xfpm_ac_adapter_size_changed_cb(GtkStatusIcon *adapter,gint size,gpointer data)
{
    GdkPixbuf *icon;
    icon = xfpm_load_icon("gpm-ac-adapter",size);
    
    if ( icon )
    {
        gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(adapter),icon);
        g_object_unref(G_OBJECT(icon));
        return TRUE;
    } 
    return FALSE;
}

static void
xfpm_ac_adapter_get_adapter(XfpmAcAdapter *adapter)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    
    gchar **udi = NULL;
    gint num;
    GError *error = NULL;
    
    udi = xfpm_hal_get_device_udi_by_capability(priv->hal,"ac_adapter",&num,&error);
    if ( error ) 
    {
        XFPM_DEBUG("%s:\n",error->message);
        g_error_free(error);
        return;
    }
    if ( !udi ) 
    {
        /* I think we should have something like g_strange()! */
        XFPM_DEBUG("No AC Adapter found, assuming running on Solar power\n");
        priv->present = TRUE;
        return;
    }
    int i;
    for ( i = 0 ; udi[i]; i++)
    {
        if ( xfpm_hal_device_has_key(priv->hal,udi[i],"ac_adapter.present"))
        {
            priv->present = xfpm_hal_get_bool_info(priv->hal,
                                                  udi[i],
                                                 "ac_adapter.present",
                                                  &error);
            if ( error ) 
            {
                XFPM_DEBUG("%s:\n",error->message);
                g_error_free(error);
                return;
            }                                                 
            XFPM_DEBUG("Getting udi %s\n",udi[i]);
            break;
        }
    }
    libhal_free_string_array(udi);
    g_signal_emit(G_OBJECT(adapter),signals[XFPM_AC_ADAPTER_CHANGED],0,priv->present);
}

static void
xfpm_ac_adapter_property_changed_cb(XfpmHal *hal,const gchar *udi,
                                    const gchar *key,gboolean is_removed,
                                    gboolean is_added,XfpmAcAdapter *adapter)
{   
    if ( xfpm_hal_device_has_key(hal,udi,"ac_adapter.present"))
    {
        XfpmAcAdapterPrivate *priv;
        priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
        
        GError *error = NULL;
        gboolean ac_adapter = 
        xfpm_hal_get_bool_info(hal,udi,"ac_adapter.present",&error);
        if ( error )                                        
        {
            XFPM_DEBUG("%s\n",error->message);
            g_error_free(error);
            return;
        }       
        if ( priv->present != ac_adapter )
        {
            XFPM_DEBUG("Ac adapter changed %d\n",ac_adapter);
            priv->present = ac_adapter;
            g_signal_emit(G_OBJECT(adapter),signals[XFPM_AC_ADAPTER_CHANGED],0,priv->present);
        }
    }
}

GtkStatusIcon *
xfpm_ac_adapter_new(gboolean visible)
{
    XfpmAcAdapter *adapter = NULL;
    adapter = g_object_new(XFPM_TYPE_AC_ADAPTER,"visible",visible,NULL);
    return GTK_STATUS_ICON(adapter);
}

void
xfpm_ac_adapter_monitor(XfpmAcAdapter *adapter)
{
    xfpm_ac_adapter_get_adapter(adapter);
}    
