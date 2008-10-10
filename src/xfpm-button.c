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

#include <xfconf/xfconf.h>

#include "xfpm-button.h"
#include "xfpm-hal.h"
#include "xfpm-debug.h"
#include "xfpm-common.h"
#include "xfpm-notify.h"
#include "xfpm-enum-types.h"

#define XFPM_BUTTON_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE(o,XFPM_TYPE_BUTTON,XfpmButtonPrivate))

static void xfpm_button_init(XfpmButton *bt);
static void xfpm_button_class_init(XfpmButtonClass *klass);
static void xfpm_button_finalize(GObject *object);

static void xfpm_button_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void xfpm_button_get_property (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);
static void xfpm_button_do_suspend(XfpmButton *bt);
static void xfpm_button_do_hibernate(XfpmButton *bt);
static gboolean xfpm_button_hibernate(XfpmButton *bt);
static gboolean xfpm_button_suspend(XfpmButton *bt);

static void xfpm_button_handle_condition_detail(XfpmButton *bt,
                                                const gchar *udi,
                                                const gchar *condition_detail);
                                           
static void xfpm_button_handle_device_condition_cb(XfpmHal *hal,
                                                   const gchar *udi,
                                                   const gchar *condition_name,
                                                   const gchar *condition_detail,
                                                   XfpmButton *bt);

static void xfpm_button_load_config(XfpmButton *bt);
static void xfpm_button_get_switches(XfpmButton *bt);

struct XfpmButtonPrivate
{
    XfpmHal *hal;
    GHashTable *buttons;
    gulong handler_id;

};

typedef enum
{
    LID,
    SLEEP,
    POWER
    
} XfpmSwitchButton;

G_DEFINE_TYPE(XfpmButton,xfpm_button,G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_LID_ACTION,
    PROP_SLEEP_ACTION,
    PROP_POWER_ACTION
};

static void xfpm_button_class_init(XfpmButtonClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = xfpm_button_finalize;
    
    gobject_class->set_property = xfpm_button_set_property;
    gobject_class->get_property = xfpm_button_get_property;

    g_object_class_install_property(gobject_class,
                                    PROP_LID_ACTION,
                                    g_param_spec_enum("lid-switch-action",
                                                      "lid switch action",
                                                      "lid switch action",
                                                      XFPM_TYPE_BUTTON_ACTION,
                                                      BUTTON_DO_NOTHING,
                                                      G_PARAM_READWRITE));    
    
    g_object_class_install_property(gobject_class,
                                    PROP_SLEEP_ACTION,
                                    g_param_spec_enum("sleep-switch-action",
                                                      "sleep switch action",
                                                      "sleep switch action",
                                                      XFPM_TYPE_BUTTON_ACTION,
                                                      BUTTON_DO_NOTHING,
                                                      G_PARAM_READWRITE));    
    g_object_class_install_property(gobject_class,
                                    PROP_LID_ACTION,
                                    g_param_spec_enum("power-switch-action",
                                                      "power switch action",
                                                      "power switch action",
                                                      XFPM_TYPE_BUTTON_ACTION,
                                                      BUTTON_DO_NOTHING,
                                                      G_PARAM_READWRITE));                                                      
    
    g_type_class_add_private(klass,sizeof(XfpmButtonPrivate));

}

static void xfpm_button_init(XfpmButton *bt)
{
    XfpmButtonPrivate *priv;
    priv = XFPM_BUTTON_GET_PRIVATE(bt);
    
    priv->buttons = g_hash_table_new(g_str_hash,g_str_equal);
    
    priv->hal = xfpm_hal_new();
    
    xfpm_button_load_config(bt);
    xfpm_button_get_switches(bt);
    
    priv->handler_id =
    g_signal_connect(priv->hal,"xfpm-device-condition",
                    G_CALLBACK(xfpm_button_handle_device_condition_cb),bt);
}

static void xfpm_button_set_property (GObject *object,
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
    XfpmButton *bt;
    bt = XFPM_BUTTON(object);
    
    switch (prop_id)
    {
    case PROP_LID_ACTION:
        bt->lid_action = g_value_get_enum(value);
        break;   
    case PROP_SLEEP_ACTION:
        bt->sleep_action = g_value_get_enum(value);
        break;   
     case PROP_POWER_ACTION:
        bt->power_action = g_value_get_enum(value);
        break;           
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
        break;
    }
}
                                           
static void xfpm_button_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    XfpmButton *bt;
    bt = XFPM_BUTTON(object);
        
    switch (prop_id)
    {
    case  PROP_LID_ACTION:
        g_value_set_enum(value,bt->lid_action);
        break;   
     case PROP_SLEEP_ACTION:
        g_value_set_enum(value,bt->sleep_action);
        break;   
     case PROP_POWER_ACTION:
        g_value_set_enum(value,bt->power_action);
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

static void xfpm_button_finalize(GObject *object)
{
    XfpmButton *bt;
    bt = XFPM_BUTTON(object);
    
    bt->priv = XFPM_BUTTON_GET_PRIVATE(bt);
    
    if ( bt->priv->hal ) 
    {
        g_object_unref(bt->priv->hal);
    }
    
    G_OBJECT_CLASS(xfpm_button_parent_class)->finalize(object);
}

static gboolean
_unblock_handler(XfpmButtonPrivate *priv)
{
    g_signal_handler_unblock(priv->hal,priv->handler_id);
    return FALSE;
}

static gboolean
xfpm_button_suspend(XfpmButton *bt)
{
    XfpmButtonPrivate *priv;
    priv = XFPM_BUTTON_GET_PRIVATE(bt);
    
    guint8 critical;

    xfpm_hal_suspend(priv->hal,NULL,&critical);
    g_timeout_add(10,(GSourceFunc)_unblock_handler,priv);
    
    return FALSE;
}

static gboolean
xfpm_button_hibernate(XfpmButton *bt)
{
    XfpmButtonPrivate *priv;
    priv = XFPM_BUTTON_GET_PRIVATE(bt);
    guint8 critical;
    
    xfpm_hal_hibernate(priv->hal,NULL,&critical);
    g_timeout_add(10,(GSourceFunc)_unblock_handler,priv);
    
    return FALSE;
}

static void
xfpm_button_do_hibernate(XfpmButton *bt)
{
    XfpmButtonPrivate *priv;
    priv = XFPM_BUTTON_GET_PRIVATE(bt);
    
    g_signal_handler_block(priv->hal,priv->handler_id);
    g_timeout_add_seconds(2,(GSourceFunc)xfpm_button_hibernate,bt);
    
}

static void
xfpm_button_do_suspend(XfpmButton *bt)
{
    XfpmButtonPrivate *priv;
    priv = XFPM_BUTTON_GET_PRIVATE(bt);
    
    g_signal_handler_block(priv->hal,priv->handler_id);
    g_timeout_add_seconds(2,(GSourceFunc)xfpm_button_suspend,bt);
}

static void
xfpm_button_handle_condition_detail(XfpmButton *bt,const gchar *udi,const gchar *condition_detail)
{
    XfpmButtonPrivate *priv;
    priv = XFPM_BUTTON_GET_PRIVATE(bt);
    
    if ( !strcmp(condition_detail,"lid") && bt->lid_action != BUTTON_DO_NOTHING )
    {
        GError *error = NULL;
        gboolean pressed = 
        xfpm_hal_get_bool_info(priv->hal,udi,"button.state.value",&error);
        if ( error ) 
        {
            XFPM_DEBUG("Error getting lid switch state: %s\n",error->message);
            g_error_free(error);
            return;
        }
        else if ( pressed == TRUE )
        {
            if ( bt->lid_action == BUTTON_DO_SUSPEND )
            {
                xfpm_lock_screen();
                xfpm_button_do_suspend(bt);
            }
            else if ( bt->lid_action == BUTTON_DO_HIBERNATE )
            {
                xfpm_lock_screen();
                xfpm_button_do_hibernate(bt);
            }
        }
    }
    else if ( !strcmp(condition_detail,"sleep") && bt->sleep_action != BUTTON_DO_NOTHING )
    {
        if ( bt->sleep_action == BUTTON_DO_SUSPEND )
        {
            xfpm_lock_screen();
            xfpm_button_do_suspend(bt);
        }
        else if ( bt->sleep_action == BUTTON_DO_HIBERNATE )
        {
            xfpm_lock_screen();
            xfpm_button_do_hibernate(bt);
        }
    }
        
    else if ( !strcmp(condition_detail,"power") && bt->lid_action != BUTTON_DO_NOTHING )
    {
        if ( bt->power_action == BUTTON_DO_SUSPEND )
        {
            xfpm_lock_screen();
            xfpm_button_do_suspend(bt);
        }
        else if ( bt->power_action == BUTTON_DO_HIBERNATE )
        {
            xfpm_lock_screen();
            xfpm_button_do_hibernate(bt);
        }
    }
}

static void
xfpm_button_handle_device_condition_cb(XfpmHal *hal,
                                       const gchar *udi,
                                       const gchar *condition_name,
                                       const gchar *condition_detail,
                                       XfpmButton *bt)
{
    XfpmButtonPrivate *priv;
    priv = XFPM_BUTTON_GET_PRIVATE(bt);
    
    if ( xfpm_hal_device_have_capability(priv->hal,udi,"button") )
    {
        if ( strcmp(condition_name,"ButtonPressed") )
        {
            XFPM_DEBUG("Not processing event with condition_name=%s\n",condition_name);
            return;
        }
        XFPM_DEBUG("proccessing event: %s %s\n",condition_name,condition_detail);
        xfpm_button_handle_condition_detail(bt,udi,condition_detail);
    }
}                                       

static void 
xfpm_button_load_config(XfpmButton *bt)
{
    XFPM_DEBUG("Loading configuration\n");
    
    GError *g_error = NULL;
    if ( !xfconf_init(&g_error) )
    {
        g_critical("xfconf init failed: %s\n",g_error->message);
        XFPM_DEBUG("Using default values\n");
        g_error_free(g_error);
        bt->lid_action = BUTTON_DO_NOTHING;
        bt->sleep_action = BUTTON_DO_NOTHING;
        bt->power_action = BUTTON_DO_NOTHING;
        return;
    }
    XfconfChannel *channel;
    
    channel = xfconf_channel_new(XFPM_CHANNEL_CFG);

    bt->lid_action   = xfconf_channel_get_uint(channel,LID_SWITCH_CFG,BUTTON_DO_NOTHING);
    bt->sleep_action   = xfconf_channel_get_uint(channel,SLEEP_SWITCH_CFG,BUTTON_DO_NOTHING);
    bt->power_action = xfconf_channel_get_uint(channel,POWER_SWITCH_CFG,BUTTON_DO_NOTHING);
    
    g_object_unref(channel);
    xfconf_shutdown();    
}

static void
xfpm_button_get_switches(XfpmButton *bt)
{
    XfpmButtonPrivate *priv;
    priv = XFPM_BUTTON_GET_PRIVATE(bt);
    
    gchar **udi;
    gint dummy;
    GError *error = NULL;
    
    udi = xfpm_hal_get_device_udi_by_capability(priv->hal,"button",&dummy,&error);
    
    if ( !udi )
    {
        XFPM_DEBUG("No buttons found\n");
    }
    int i = 0 ;
    for ( i = 0 ; udi[i] ; i++)
    {
        
    }
    
    libhal_free_string_array(udi);
}

XfpmButton *
xfpm_button_new(void)
{
    XfpmButton *bt;
    bt = g_object_new(XFPM_TYPE_BUTTON,NULL);
    return bt;
}
