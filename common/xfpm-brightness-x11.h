/*
 * Copyright (C) 2023 Gaël Bonithon <gael@xfce.org>
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

#ifndef __XFPM_BRIGHTNESS_X11_H__
#define __XFPM_BRIGHTNESS_X11_H__

#include "xfpm-brightness.h"

G_BEGIN_DECLS

#define XFPM_TYPE_BRIGHTNESS_X11 (xfpm_brightness_x11_get_type ())
G_DECLARE_FINAL_TYPE (XfpmBrightnessX11, xfpm_brightness_x11, XFPM, BRIGHTNESS_X11, XfpmBrightness)

G_END_DECLS

#endif /* __XFPM_BRIGHTNESS_X11_H__ */
