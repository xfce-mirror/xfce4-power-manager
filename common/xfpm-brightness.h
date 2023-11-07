/*
 * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#ifndef __XFPM_BRIGHTNESS_H__
#define __XFPM_BRIGHTNESS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_BRIGHTNESS (xfpm_brightness_get_type ())
G_DECLARE_DERIVABLE_TYPE (XfpmBrightness, xfpm_brightness, XFPM, BRIGHTNESS, GObject)

struct _XfpmBrightnessClass
{
  GObjectClass parent_class;

  gboolean        (*setup)              (XfpmBrightness       *brightness,
                                         gint32               *min_level,
                                         gint32               *max_level);
  gboolean        (*get_level)          (XfpmBrightness       *brightness,
                                         gint32               *level);
  gboolean        (*set_level)          (XfpmBrightness       *brightness,
                                         gint32                level);
  gboolean        (*get_switch)         (XfpmBrightness       *brightness,
                                         gint                 *_switch);
  gboolean        (*set_switch)         (XfpmBrightness       *brightness,
                                         gint                  _switch);
};

XfpmBrightness      *xfpm_brightness_new                 (void);
gint32               xfpm_brightness_get_min_level       (XfpmBrightness       *brightness);
void                 xfpm_brightness_set_min_level       (XfpmBrightness       *brightness,
                                                          gint32                level);
gint32               xfpm_brightness_get_max_level       (XfpmBrightness       *brightness);
void                 xfpm_brightness_set_step_count      (XfpmBrightness       *brightness,
                                                          guint32               count,
                                                          gboolean              exponential);
gboolean             xfpm_brightness_decrease            (XfpmBrightness       *brightness);
gboolean             xfpm_brightness_increase            (XfpmBrightness       *brightness);

gboolean             xfpm_brightness_get_level           (XfpmBrightness       *brightness,
                                                          gint32               *level);
gboolean             xfpm_brightness_set_level           (XfpmBrightness       *brightness,
                                                          gint32                level);
gboolean             xfpm_brightness_get_switch          (XfpmBrightness       *brightness,
                                                          gint                 *_switch);
gboolean             xfpm_brightness_set_switch          (XfpmBrightness       *brightness,
                                                          gint                  _switch);

G_END_DECLS

#endif /* __XFPM_BRIGHTNESS_H__ */
