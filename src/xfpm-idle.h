/*
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XFPM_IDLE_H__
#define __XFPM_IDLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_IDLE (xfpm_idle_get_type ())
G_DECLARE_DERIVABLE_TYPE (XfpmIdle, xfpm_idle, XFPM, IDLE, GObject)

typedef enum _XfpmAlarmId
{
  XFPM_ALARM_ID_USER_INPUT            = 0,
  XFPM_ALARM_ID_BRIGHTNESS_ON_AC      = 1 << 0,
  XFPM_ALARM_ID_BRIGHTNESS_ON_BATTERY = 1 << 1,
  XFPM_ALARM_ID_INACTIVITY_ON_AC      = 1 << 2,
  XFPM_ALARM_ID_INACTIVITY_ON_BATTERY = 1 << 3,
} XfpmAlarmId;

struct _XfpmIdleClass
{
  GObjectClass parent_class;

  void       (*alarm_reset_all)      (XfpmIdle       *idle);
  void       (*alarm_add)            (XfpmIdle       *idle,
                                      XfpmAlarmId     id,
                                      guint           timeout);
  void       (*alarm_remove)         (XfpmIdle       *idle,
                                      XfpmAlarmId     id);
};

XfpmIdle      *xfpm_idle_new                (void);

void           xfpm_idle_alarm_reset_all    (XfpmIdle       *idle);
void           xfpm_idle_alarm_add          (XfpmIdle       *idle,
                                             XfpmAlarmId     id,
                                             guint           timeout);
void           xfpm_idle_alarm_remove       (XfpmIdle       *idle,
                                             XfpmAlarmId     id);

G_END_DECLS

#endif  /* __XFPM_IDLE_H__ */
