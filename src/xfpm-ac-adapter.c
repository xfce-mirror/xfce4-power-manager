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

#include <gtk/gtkstatusicon.h>

#include <libxfcegui4/libxfcegui4.h>

#include <glib/gi18n.h>

#include "xfpm-hal.h"
#include "xfpm-driver.h"
#include "xfpm-ac-adapter.h"
#include "xfpm-common.h"
#include "xfpm-notify.h"
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
                                        
static void xfpm_ac_adapter_device_added_cb(XfpmHal *hal,
                                            const gchar *udi,
                                            XfpmAcAdapter *adapter);

static void xfpm_ac_adapter_device_removed_cb(XfpmHal *hal,
                                              const gchar *udi,
                                              XfpmAcAdapter *adapter);
                                              
static void xfpm_ac_adapter_property_changed_cb(XfpmHal *hal,
                                                const gchar *udi,
                                                const gchar *key,
                                                gboolean is_removed,
                                                gboolean is_added,
                                                XfpmAcAdapter *adapter);
                                                
static void xfpm_ac_adapter_popup_menu(GtkStatusIcon *tray_icon,
                                       guint button,
                                       guint activate_time,
                                       XfpmAcAdapter *adapter);

struct XfpmAcAdapterPrivate
{
    XfpmHal *hal;
    SystemFormFactor factor;
    
    GQuark adapter_udi;
    
    gboolean adapter_found;
    gboolean present;
    gboolean can_suspend;
    gboolean can_hibernate;
    
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
    priv->adapter_udi = 0 ;
    
    GError *error = NULL;
    
    priv->can_hibernate = xfpm_hal_get_bool_info(priv->hal,
                                                 HAL_ROOT_COMPUTER,
                                                 "power_management.can_hibernate",
                                                 &error);
    if ( error )
    {
        XFPM_DEBUG("%s: \n",error->message);
        g_error_free(error);
        error = NULL;
    }                                          

    priv->can_suspend = xfpm_hal_get_bool_info(priv->hal,
                                               HAL_ROOT_COMPUTER,
                                               "power_management.can_suspend",
                                               &error);
    if ( error )
    {
        XFPM_DEBUG("%s: \n",error->message);
        g_error_free(error);
    }            
    
    g_signal_connect(adapter,"size-changed",
                    G_CALLBACK(xfpm_ac_adapter_size_changed_cb),NULL);
    g_signal_connect(adapter,"popup-menu",
                    G_CALLBACK(xfpm_ac_adapter_popup_menu),adapter);
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
_ac_adapter_not_found(XfpmAcAdapter *adapter)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    
    /* then the ac kernel module is not loaded */
    if ( priv->factor == SYSTEM_LAPTOP )
    {
        priv->present = TRUE; /* assuming present */
        priv->adapter_found = FALSE;
        gtk_status_icon_set_tooltip(GTK_STATUS_ICON(adapter),
                                   _("Unkown adapter status, the power manager will not work properly"));
#ifdef HAVE_LIBNOTIFY
        xfpm_notify_simple(_("Xfce power manager"),
                           _("Unkown adapter status, the power manager will not work properly,"\
                            "make sure ac adapter driver is loaded into the kernel"),
                           10000,
                           NOTIFY_URGENCY_CRITICAL,
                           NULL,
                           "gpm-ac-adapter",
                           2);
        
#endif                                       
    }     
    else  
    {
        priv->present = TRUE; /* just for eveything to function correctly */
        priv->adapter_found = FALSE;
    }
    g_signal_emit(G_OBJECT(adapter),signals[XFPM_AC_ADAPTER_CHANGED],0,priv->present);
    
}

static void
xfpm_ac_adapter_get_adapter(XfpmAcAdapter *adapter)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    
    priv->adapter_udi = 0 ;
    
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
    
    if ( num == 0 )
    {
        priv->present = TRUE;
        XFPM_DEBUG("No ac adapter device found\n");
        _ac_adapter_not_found(adapter);
        return;
    }
    
    if ( !udi ) 
    {
        _ac_adapter_not_found(adapter);
        return;
    }
    
    int i;
    for ( i = 0 ; udi[i]; i++)
    {
        if ( xfpm_hal_device_have_key(priv->hal,udi[i],"ac_adapter.present"))
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
            priv->adapter_udi = g_quark_from_string(udi[i]);
            priv->adapter_found = TRUE;
            break;
        }
    }
    
    gtk_status_icon_set_tooltip(GTK_STATUS_ICON(adapter),
                priv->present ? _("Adapter is online") : _("Adapter is offline"));   
    g_signal_emit(G_OBJECT(adapter),signals[XFPM_AC_ADAPTER_CHANGED],0,priv->present);
    libhal_free_string_array(udi);
}

static void
_get_adapter_status(XfpmAcAdapter *adapter,const gchar *udi)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    GError *error = NULL;
    gboolean ac_adapter = 
    xfpm_hal_get_bool_info(priv->hal,udi,"ac_adapter.present",&error);
    if ( error )                                        
    {
        XFPM_DEBUG("%s\n",error->message);
        g_error_free(error);
        return;
    }       
    XFPM_DEBUG("Ac adapter changed %d\n",ac_adapter);
    priv->present = ac_adapter;
    gtk_status_icon_set_tooltip(GTK_STATUS_ICON(adapter),
                priv->present ? _("Adapter is online") : _("Adapter is offline"));   
    g_signal_emit(G_OBJECT(adapter),signals[XFPM_AC_ADAPTER_CHANGED],0,priv->present);
}

static void
xfpm_ac_adapter_device_added_cb(XfpmHal *hal,const gchar *udi,XfpmAcAdapter *adapter)
{
    if ( xfpm_hal_device_have_key(hal,udi,"ac_adapter.present"))
    {
        XfpmAcAdapterPrivate *priv;
        priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
        priv->adapter_found = TRUE;
        priv->adapter_udi = g_quark_from_string(udi);
    
        _get_adapter_status(adapter,udi);
    }
}

static void
xfpm_ac_adapter_device_removed_cb(XfpmHal *hal,const gchar *udi,XfpmAcAdapter *adapter)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    
    if ( priv->adapter_udi == g_quark_from_string(udi) ) 
    {
        XFPM_DEBUG("Adapter removed\n");
        xfpm_ac_adapter_get_adapter(adapter);
    }
}

static void
xfpm_ac_adapter_property_changed_cb(XfpmHal *hal,const gchar *udi,
                                    const gchar *key,gboolean is_removed,
                                    gboolean is_added,XfpmAcAdapter *adapter)
{   
    if ( xfpm_hal_device_have_key(hal,udi,"ac_adapter.present"))
    {
        XfpmAcAdapterPrivate *priv;
        priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
        _get_adapter_status(adapter,udi);
        
    }
}

static void
xfpm_ac_adapter_report_sleep_errors(XfpmAcAdapter *adapter,const gchar *error,
                                    const gchar *icon_name)
{
#ifdef HAVE_LIBNOTIFY
    xfpm_notify_simple("Xfce power manager",
                       error,
                       14000,
                       NOTIFY_URGENCY_CRITICAL,
                       GTK_STATUS_ICON(adapter),
                       icon_name,
                       0);
#endif
}

static gboolean
xfpm_ac_adapter_do_hibernate(XfpmAcAdapter *adapter)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    
    GError *error = NULL;
    guint8 critical;
    gboolean ret =
    xfpm_hal_hibernate(priv->hal,&error,&critical);
     
    if ( !ret && critical == 1) {
        xfpm_ac_adapter_report_sleep_errors(adapter,error->message,"gpm-hibernate");
        g_error_free(error);
    }
    
    return FALSE;
    
}

static void
xfpm_ac_adapter_hibernate_callback(GtkWidget *widget,XfpmAcAdapter *adapter)
{
    gboolean ret = 
    xfce_confirm(_("Are you sure you want to hibernate the system"),
                GTK_STOCK_YES,
                _("Hibernate"));
    
    if ( ret ) 
    {
        xfpm_lock_screen();
        g_timeout_add_seconds(4,(GSourceFunc)xfpm_ac_adapter_do_hibernate,adapter);
	}
}

static gboolean
xfpm_ac_adapter_do_suspend(XfpmAcAdapter *adapter)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    
    GError *error = NULL;
    guint8 critical = 0;
    gboolean ret =
    xfpm_hal_suspend(priv->hal,&error,&critical);
    
    if ( !ret && critical == 1 ) 
    {
        xfpm_ac_adapter_report_sleep_errors(adapter,error->message,"gpm-suspend");
        g_error_free(error);
    }
    
    return FALSE;
}

static void
xfpm_ac_adapter_suspend_callback(GtkWidget *widget,XfpmAcAdapter *adapter)
{
    gboolean ret = 
    xfce_confirm(_("Are you sure you want to suspend the system"),
                GTK_STOCK_YES,
                _("Suspend"));
    
    if ( ret ) 
    {
        xfpm_lock_screen();
        g_timeout_add_seconds(3,(GSourceFunc)xfpm_ac_adapter_do_suspend,adapter);
    }
}

static void 
xfpm_ac_adapter_popup_menu(GtkStatusIcon *tray_icon,
                           guint button,
                           guint activate_time,
                           XfpmAcAdapter *adapter)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(XFPM_AC_ADAPTER(tray_icon));
    
    GtkWidget *menu,*mi,*img;
	
	menu = gtk_menu_new();

	// Hibernate menu option
	mi = gtk_image_menu_item_new_with_label(_("Hibernate"));
	img = gtk_image_new_from_icon_name("gpm-hibernate",GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),img);
	gtk_widget_set_sensitive(mi,FALSE);
	
	
	if ( priv->can_hibernate )
	{
		gtk_widget_set_sensitive(mi,TRUE);
		g_signal_connect(mi,"activate",
					 	 G_CALLBACK(xfpm_ac_adapter_hibernate_callback),
					 	 adapter);
	}
	gtk_widget_show(mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
	
	// Suspend menu option
	mi = gtk_image_menu_item_new_with_label(_("Suspend"));
	img = gtk_image_new_from_icon_name("gpm-suspend",GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),img);
	
	gtk_widget_set_sensitive(mi,FALSE);
	if ( priv->can_suspend )
    {	
		gtk_widget_set_sensitive(mi,TRUE);
		g_signal_connect(mi,"activate",
					     G_CALLBACK(xfpm_ac_adapter_suspend_callback),
					     adapter);
	}
	gtk_widget_show(mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
	// Separotor
	mi = gtk_separator_menu_item_new();
	gtk_widget_show(mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
	mi = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES,NULL);
	gtk_widget_set_sensitive(mi,TRUE);
	gtk_widget_show(mi);
	g_signal_connect(mi,"activate",G_CALLBACK(xfpm_preferences),NULL);
	
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
	
	// Popup the menu
	gtk_menu_popup(GTK_MENU(menu),NULL,NULL,
		       NULL,NULL,button,activate_time);
}                                             

GtkStatusIcon *
xfpm_ac_adapter_new(gboolean visible)
{
    XfpmAcAdapter *adapter = NULL;
    adapter = g_object_new(XFPM_TYPE_AC_ADAPTER,"visible",visible,NULL);
    return GTK_STATUS_ICON(adapter);
}

void
xfpm_ac_adapter_monitor(XfpmAcAdapter *adapter,SystemFormFactor factor)
{
    XfpmAcAdapterPrivate *priv;
    priv = XFPM_AC_ADAPTER_GET_PRIVATE(adapter);
    priv->factor = factor;
    xfpm_ac_adapter_get_adapter(adapter);
    
    if ( factor == SYSTEM_LAPTOP )
    {
        xfpm_hal_connect_to_signals(priv->hal,TRUE,TRUE,TRUE,FALSE);
        g_signal_connect(priv->hal,"xfpm-device-added",
                    G_CALLBACK(xfpm_ac_adapter_device_added_cb),adapter);
        g_signal_connect(priv->hal,"xfpm-device-removed",
                    G_CALLBACK(xfpm_ac_adapter_device_removed_cb),adapter); 
        g_signal_connect(priv->hal,"xfpm-device-property-changed",
                     G_CALLBACK(xfpm_ac_adapter_property_changed_cb),adapter);            
    }
}   
