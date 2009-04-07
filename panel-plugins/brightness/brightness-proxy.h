/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __BRIGHTNESS_PROXY_H
#define __BRIGHTNESS_PROXY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define BRIGHTNESS_TYPE_PROXY        (brightness_proxy_get_type () )
#define BRIGHTNESS_PROXY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), BRIGHTNESS_TYPE_PROXY, BrightnessProxy))
#define BRIGHTNESS_IS_PROXY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRIGHTNESS_TYPE_PROXY))

typedef struct BrightnessProxyPrivate BrightnessProxyPrivate;

typedef struct
{
    GObject         		parent;
    BrightnessProxyPrivate     *priv;
    
} BrightnessProxy;

typedef struct
{
    GObjectClass 		parent_class;
} BrightnessProxyClass;

GType        			brightness_proxy_get_type        (void) G_GNUC_CONST;
BrightnessProxy       	       *brightness_proxy_new             (void);

gboolean                        brightness_proxy_set_level       (BrightnessProxy *brightness,
								  guint level);

guint                           brightness_proxy_get_level       (BrightnessProxy *brightness);

guint                           brightness_proxy_get_max_level   (BrightnessProxy *brightness) G_GNUC_PURE;

gboolean                        brightness_proxy_has_hw          (BrightnessProxy *brightness) G_GNUC_PURE;

void                            brightness_proxy_reload          (BrightnessProxy *brightness);

G_END_DECLS

#endif /* __BRIGHTNESS_PROXY_H */
