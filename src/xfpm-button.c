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
                                           
static void xfpm_button_handle_device_condition_cb(XfpmHal *hal,
                                                   const gchar *udi,
                                                   const gchar *condition_name,
                                                   const gchar *condition_detail,
                                                   XfpmButton *bt);

static void xfpm_button_load_config(XfpmButton *bt);

struct XfpmButtonPrivate
{
    XfpmHal *hal;

};

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
    
    priv->hal = xfpm_hal_new();
    
    xfpm_button_load_config(bt);
    
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

static void
xfpm_button_handle_device_condition_cb(XfpmHal *hal,
                                       const gchar *udi,
                                       const gchar *condition_name,
                                       const gchar *condition_detail,
                                       XfpmButton *bt)
{
    XFPM_DEBUG("condition name=%s detail=%s\n",condition_name,condition_detail);
    
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

    bt->lid_action   = xfconf_channel_get_bool(channel,LID_SWITCH_CFG,BUTTON_DO_NOTHING);
    bt->sleep_action   = xfconf_channel_get_bool(channel,SLEEP_SWITCH_CFG,BUTTON_DO_NOTHING);
    bt->power_action = xfconf_channel_get_bool(channel,POWER_SWITCH_CFG,BUTTON_DO_NOTHING);
    
    g_object_unref(channel);
    xfconf_shutdown();    
}

XfpmButton *
xfpm_button_new(void)
{
    XfpmButton *bt;
    bt = g_object_new(XFPM_TYPE_BUTTON,NULL);
    return bt;
}
