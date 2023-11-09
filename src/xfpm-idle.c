/*
 * Copyright (C) 2007-2009 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xfpm-idle.h"
#include "xfpm-debug.h"

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include "xfpm-idle-x11.h"
#endif

#define get_instance_private(instance) ((XfpmIdlePrivate *) \
  xfpm_idle_get_instance_private (XFPM_IDLE (instance)))

typedef struct _XfpmIdlePrivate
{
  XfpmAlarmId added_alarm_ids;
} XfpmIdlePrivate;

enum
{
  SIGNAL_ALARM_EXPIRED,
  SIGNAL_RESET,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (XfpmIdle, xfpm_idle, G_TYPE_OBJECT)



static void
xfpm_idle_class_init (XfpmIdleClass *klass)
{
  signals[SIGNAL_ALARM_EXPIRED] =
    g_signal_new ("alarm-expired",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0, NULL, NULL, g_cclosure_marshal_VOID__UINT,
            G_TYPE_NONE, 1, G_TYPE_UINT);
  signals[SIGNAL_RESET] =
    g_signal_new ("reset",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
}

static void
xfpm_idle_init (XfpmIdle *idle)
{
}



XfpmIdle *
xfpm_idle_new (void)
{
  static gpointer singleton = NULL;

  if (singleton != NULL)
  {
    g_object_ref (singleton);
  }
  else
  {
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
      singleton = g_object_new (XFPM_TYPE_IDLE_X11, NULL);
#endif
    if (singleton != NULL)
      g_object_add_weak_pointer (singleton, &singleton);
    else
      g_critical ("Idle status monitoring is not supported on this windowing environment");
  }

  return singleton;
}



void
xfpm_idle_alarm_reset_all (XfpmIdle *idle)
{
  g_return_if_fail (XFPM_IS_IDLE (idle));
  XFPM_IDLE_GET_CLASS (idle)->alarm_reset_all (idle);
  g_signal_emit (idle, signals[SIGNAL_RESET], 0);
}

void
xfpm_idle_alarm_add (XfpmIdle *idle,
                     XfpmAlarmId id,
                     guint timeout)
{
  XfpmIdlePrivate *priv = get_instance_private (idle);

  g_return_if_fail (XFPM_IS_IDLE (idle));
  g_return_if_fail (id != XFPM_ALARM_ID_USER_INPUT);
  g_return_if_fail (timeout != 0);

  if (!(priv->added_alarm_ids & id))
  {
    XFPM_DEBUG ("Adding alarm id %d", id);
    priv->added_alarm_ids |= id;
  }
  else
  {
    XFPM_DEBUG ("Updating alarm id %d", id);
  }

  XFPM_IDLE_GET_CLASS (idle)->alarm_add (idle, id, timeout);
}

void
xfpm_idle_alarm_remove (XfpmIdle *idle,
                        XfpmAlarmId id)
{
  XfpmIdlePrivate *priv = get_instance_private (idle);

  g_return_if_fail (XFPM_IS_IDLE (idle));

  if (priv->added_alarm_ids & id)
  {
    XFPM_DEBUG ("Removing alarm id %d", id);
    priv->added_alarm_ids &= ~id;
    XFPM_IDLE_GET_CLASS (idle)->alarm_remove (idle, id);
  }
  else
  {
    XFPM_DEBUG ("Alarm id %d already removed", id);
  }
}
