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

#define XFPM_LCD_BRIGHTNESS_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE(o,XFPM_TYPE_LCD_BRIGHTNESS,XfpmLcdBrightnessPrivate))

static void xfpm_lcd_brightness_init(XfpmLcdBrightness *lcd);
static void xfpm_lcd_brightness_class_init(XfpmLcdBrightnessClass *klass);
static void xfpm_lcd_brightness_finalize(GObject *object);

static void xfpm_lcd_brightness_load_config(XfpmLcdBrightness *lcd);

struct XfpmLcdBrightnessPrivate
{
    XfpmHal *hal;

};

G_DEFINE_TYPE(XfpmLcdBrightness,xfpm_lcd_brightness,G_TYPE_OBJECT)

static void xfpm_lcd_brightness_class_init(XfpmLcdBrightnessClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = xfpm_lcd_brightness_finalize;
    
    g_type_class_add_private(klass,sizeof(XfpmLcdBrightnessPrivate));

}

static void xfpm_lcd_brightness_init(XfpmLcdBrightness *lcd)
{
    XfpmLcdBrightnessPrivate *priv;
    priv = XFPM_LCD_BRIGHTNESS_GET_PRIVATE(lcd);
    
    priv->hal = xfpm_hal_new();
    xfpm_lcd_brightness_load_config(lcd);
    
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
    
    G_OBJECT_CLASS(xfpm_lcd_brightness_parent_class)->finalize(object);
}

static void xfpm_lcd_brightness_load_config(XfpmLcdBrightness *lcd)
{
    
}

XfpmLcdBrightness *
xfpm_lcd_brightness_new(void)
{
    XfpmLcdBrightness *lcd;
    lcd = g_object_new(XFPM_TYPE_LCD_BRIGHTNESS,NULL);
    return lcd;
}
