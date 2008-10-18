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

#include "xfpm-lcd-brightness.h"
#include "xfpm-hal.h"
#include "xfpm-debug.h"
#include "xfpm-common.h"

#define XFPM_LCD_BRIGHTNESS_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE(o,XFPM_TYPE_LCD_BRIGHTNESS,XfpmLcdBrightnessPrivate))

static void xfpm_lcd_brightness_init(XfpmLcdBrightness *lcd);
static void xfpm_lcd_brightness_class_init(XfpmLcdBrightnessClass *klass);
static void xfpm_lcd_brightness_finalize(GObject *object);

static void xfpm_lcd_brightness_set_property(GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void xfpm_lcd_brightness_get_property(GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);

static void xfpm_lcd_brightness_set_level(XfpmLcdBrightness *lcd);

static void xfpm_lcd_brightness_load_config(XfpmLcdBrightness *lcd);

static void xfpm_lcd_brightness_get_device(XfpmLcdBrightness *lcd);

static void xfpm_lcd_brightness_increase(XfpmLcdBrightness *lcd);
static void xfpm_lcd_brightness_decrease(XfpmLcdBrightness *lcd);

static void xfpm_lcd_brightness_handle_device_condition_cb(XfpmHal *hal,
                                                           const gchar *udi,
                                                           const gchar *condition_name,
                                                           const gchar *condition_detail,
                                                           XfpmLcdBrightness *lcd);
static void xfpm_lcd_brightness_notify_cb(GObject *object,
                                          GParamSpec *arg1,
                                          gpointer data);

struct XfpmLcdBrightnessPrivate
{
    
    XfpmHal *hal;
    gboolean device_exists;
    gboolean brightness_in_hardware;
    gchar *udi;
    gint max_brightness;
    gint step;

};

G_DEFINE_TYPE(XfpmLcdBrightness,xfpm_lcd_brightness,G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_AC_ADAPTER,
    PROP_BRIGHTNESS
};

static void xfpm_lcd_brightness_class_init(XfpmLcdBrightnessClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = xfpm_lcd_brightness_finalize;
    gobject_class->set_property = xfpm_lcd_brightness_set_property;
    gobject_class->get_property = xfpm_lcd_brightness_get_property;
    
    g_object_class_install_property(gobject_class,
                                    PROP_AC_ADAPTER,
                                    g_param_spec_boolean("on-ac-adapter",
                                                         "On ac adapter",
                                                         "On Ac power",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
                                                         
    g_object_class_install_property(gobject_class,
                                    PROP_BRIGHTNESS,
                                    g_param_spec_boolean("brightness-enabled",
                                                         "brightness enabled",
                                                         "brightness settings",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
    
    g_type_class_add_private(klass,sizeof(XfpmLcdBrightnessPrivate));

}

static void xfpm_lcd_brightness_init(XfpmLcdBrightness *lcd)
{
    XfpmLcdBrightnessPrivate *priv;
    priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);
    
    priv->hal = xfpm_hal_new();
    priv->device_exists = FALSE;
    priv->brightness_in_hardware = TRUE; 
    priv->udi = NULL;
    priv->max_brightness = -1;
    priv->step = 0;
    
    xfpm_lcd_brightness_load_config(lcd);
    xfpm_lcd_brightness_get_device(lcd);
    
    if ( priv->device_exists )
    {
        g_signal_connect(G_OBJECT(lcd),"notify",
                         G_CALLBACK(xfpm_lcd_brightness_notify_cb),NULL);
        if (!priv->brightness_in_hardware )
        {
            if (xfpm_hal_connect_to_signals(priv->hal,FALSE,FALSE,FALSE,TRUE) )
            {
                g_signal_connect(priv->hal,"xfpm-device-condition",
                            G_CALLBACK(xfpm_lcd_brightness_handle_device_condition_cb),lcd);
            }
        }                 
    }
    
}

static void xfpm_lcd_brightness_finalize(GObject *object)
{
    XfpmLcdBrightness *lcd;
    lcd = XFPM_LCD_BRIGHTNESS(object);
    lcd->priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);
    
    if ( lcd->priv->hal ) 
    {
        g_object_unref(lcd->priv->hal);
    }
    if ( lcd->priv->udi )
    {
        g_free(lcd->priv->udi);
    }
    
    G_OBJECT_CLASS(xfpm_lcd_brightness_parent_class)->finalize(object);
}

static void 
xfpm_lcd_brightness_set_property(GObject *object,
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
    XfpmLcdBrightness *lcd;
    lcd = XFPM_LCD_BRIGHTNESS(object);
    
    switch (prop_id)
    {
    case PROP_AC_ADAPTER:
        lcd->ac_adapter_present = g_value_get_boolean(value);
        break;   
    case PROP_BRIGHTNESS:
        lcd->brightness_control_enabled = g_value_get_boolean(value);
        break;    
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
        break;
    }

}

static void 
xfpm_lcd_brightness_get_property(GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    XfpmLcdBrightness *lcd;
    lcd = XFPM_LCD_BRIGHTNESS(object);
        
    switch (prop_id)
    {
    case PROP_AC_ADAPTER:
        g_value_set_boolean(value,lcd->ac_adapter_present);
        break;   
    case PROP_BRIGHTNESS:
        g_value_set_boolean(value,lcd->brightness_control_enabled);
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

static void
xfpm_lcd_brightness_set_level(XfpmLcdBrightness *lcd)
{
    XfpmLcdBrightnessPrivate *priv;
    priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);
    
    GError *error = NULL;
    gint32 level =
        xfpm_hal_get_brightness(priv->hal,priv->udi,&error);
        
    if ( error )
    {
        XFPM_DEBUG("Get brightness failed: %s\n",error->message);
        g_error_free(error);
        return;
    }
    
    if ( lcd->ac_adapter_present )
    {
        XFPM_DEBUG("Setting level on ac adapter\n");
        if ( level != priv->max_brightness -1 )
        {
            xfpm_hal_set_brightness(priv->hal,priv->udi,priv->max_brightness -1,&error);
            if ( error )
            {
                XFPM_DEBUG("Error setting brigthness level: %s\n",error->message);
                g_error_free(error);
                return;
            }
        }
    }
    else 
    {
        XFPM_DEBUG("Setting level battery power\n");
        xfpm_hal_set_brightness(priv->hal,priv->udi, 1,&error);
        if ( error )
        {
            XFPM_DEBUG("Error setting brigthness level: %s\n",error->message);
            g_error_free(error);
            return;
        }
    }
}

static void xfpm_lcd_brightness_load_config(XfpmLcdBrightness *lcd)
{
    XFPM_DEBUG("Loading configuration\n");
    GError *g_error = NULL;
    if ( !xfconf_init(&g_error) )
    {
        g_critical("xfconf init failed: %s\n",g_error->message);
        XFPM_DEBUG("Using default values\n");
        g_error_free(g_error);
        lcd->brightness_control_enabled = TRUE;
        return;
    }
    
    XfconfChannel *channel;
    channel = xfconf_channel_new(XFPM_CHANNEL_CFG);

    lcd->brightness_control_enabled = xfconf_channel_get_bool(channel,LCD_BRIGHTNESS_CFG,TRUE);
    
    g_object_unref(channel);
    xfconf_shutdown();    
}

static void
_get_steps(XfpmLcdBrightness *lcd)
{
    XfpmLcdBrightnessPrivate *priv;
    priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);
    
    if ( priv->max_brightness <= 9 )
    {
        priv->step = 1;
        return;
    }
    
    priv->step = priv->max_brightness/10; /* 77 for example will give 7 step, so that's okay */
    XFPM_DEBUG("Found approximate lcd brightness steps = %d\n",priv->step);
}

static void
xfpm_lcd_brightness_get_device(XfpmLcdBrightness *lcd)
{
    XfpmLcdBrightnessPrivate *priv;
    priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);

    gchar **udi = NULL;
    gint num;
    GError *error = NULL;
    udi = xfpm_hal_get_device_udi_by_capability(priv->hal,"laptop_panel",&num,&error);
    
    if ( error ) 
    {
        XFPM_DEBUG("%s: \n",error->message);
        g_error_free(error);
        return;
    }
    
    if ( !udi || num == 0 )
    {
        XFPM_DEBUG("No device with laptop_panel capability\n");
        return;
    }
    
    /* Only one device should be in HAL */
    g_return_if_fail(num == 1);        
    
    priv->udi = g_strdup(udi[0]);
    XFPM_DEBUG("Found laptop panel device: %s\n",udi[0]);
    libhal_free_string_array(udi);
    
    if ( xfpm_hal_device_have_key(priv->hal,priv->udi,"laptop_panel.num_levels") )
    {
        priv->max_brightness = xfpm_hal_get_int_info(priv->hal,priv->udi,"laptop_panel.num_levels",&error);
        if ( error )
        {
            XFPM_DEBUG("error getting max brigthness level: %s\n",error->message);
            g_error_free(error);
            return;
        }
        XFPM_DEBUG("Max screen luminosity = %d\n",priv->max_brightness);
        priv->device_exists = TRUE;
    }
    
    if ( xfpm_hal_device_have_key(priv->hal,priv->udi,"laptop_panel.brightness_in_hardware") )
    {
        priv->brightness_in_hardware = xfpm_hal_get_bool_info(priv->hal,priv->udi,
                                            "laptop_panel.brightness_in_hardware",&error);
        if ( error )
        {
            XFPM_DEBUG("error getting max brigthness level: %s\n",error->message);
            g_error_free(error);
            priv->brightness_in_hardware = TRUE; /* we always assume that control is in hardware */
            return;
        }          
        if ( !priv->brightness_in_hardware )
        {
            _get_steps(lcd);
        }
    }
    
}

static void
xfpm_lcd_brightness_increase(XfpmLcdBrightness *lcd)
{
    XfpmLcdBrightnessPrivate *priv;
    priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);
    GError *error = NULL;
    gint32 level =
        xfpm_hal_get_brightness(priv->hal,priv->udi,&error);
        
    if ( error )
    {
        XFPM_DEBUG("Get brightness failed: %s\n",error->message);
        g_error_free(error);
        return;
    }
    
    if ( level != priv->max_brightness -1 )
    {
        gint32 set = priv->step + level;
        if ( set > priv->max_brightness -1 )
        {
            set = priv->max_brightness -1;
        }
        XFPM_DEBUG("Setting brightness=%d\n",set);
        xfpm_hal_set_brightness(priv->hal,priv->udi,set,&error);
        if ( error )
        {
            XFPM_DEBUG("Error setting brigthness level: %s\n",error->message);
            g_error_free(error);
            return;
        }
    }
}

static void
xfpm_lcd_brightness_decrease(XfpmLcdBrightness *lcd)
{
    XfpmLcdBrightnessPrivate *priv;
    priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);
    GError *error = NULL;
    gint32 level =
        xfpm_hal_get_brightness(priv->hal,priv->udi,&error);
        
    if ( error )
    {
        XFPM_DEBUG("Get brightness failed: %s\n",error->message);
        g_error_free(error);
        return;
    }
    
    if ( level != 1 )
    {
        gint32 set =  level - priv->step;
        
        if ( set < 0 )
        {
            set = 1;
        }
        XFPM_DEBUG("Setting brightness=%d\n",set);
        xfpm_hal_set_brightness(priv->hal,priv->udi,set,&error);
        if ( error )
        {
            XFPM_DEBUG("Error setting brigthness level: %s\n",error->message);
            g_error_free(error);
            return;
        }
    }
}

static void 
xfpm_lcd_brightness_handle_device_condition_cb(XfpmHal *hal,
                                               const gchar *udi,
                                               const gchar *condition_name,
                                               const gchar *condition_detail,
                                               XfpmLcdBrightness *lcd)
{
    if ( !lcd->brightness_control_enabled ) return;
        
    if ( xfpm_hal_device_have_capability(hal,udi,"button") )
    {
        if ( !strcmp(condition_name,"ButtonPressed") )
        {
            if ( !strcmp(condition_detail,"brightness-down") )
            {
                xfpm_lcd_brightness_decrease(lcd);
            }
            else if ( !strcmp(condition_name,"brightness-up") )
            {
                xfpm_lcd_brightness_increase(lcd);
            }
        }
    }
}                                                           
                                                           

static void
xfpm_lcd_brightness_notify_cb(GObject *object,GParamSpec *arg1,gpointer data)
{
    XfpmLcdBrightness *lcd;
    
    lcd = XFPM_LCD_BRIGHTNESS(object);
    
    XFPM_DEBUG("brightness callback\n");
    
    if ( !lcd->brightness_control_enabled )
    {
        XFPM_DEBUG("Lcd brightness is disabled\n");
        return;
    }
    
    xfpm_lcd_brightness_set_level(lcd);    
}

XfpmLcdBrightness *
xfpm_lcd_brightness_new(void)
{
    XfpmLcdBrightness *lcd;
    lcd = g_object_new(XFPM_TYPE_LCD_BRIGHTNESS,NULL);
    return lcd;
}

gboolean
xfpm_lcd_brightness_device_exists(XfpmLcdBrightness *lcd)
{
    g_return_val_if_fail(XFPM_IS_LCD_BRIGHTNESS(lcd),FALSE);
    XfpmLcdBrightnessPrivate *priv;
    priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);
    return priv->device_exists;
}
