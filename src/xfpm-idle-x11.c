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

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include <gdk/gdkx.h>

#include "xfpm-idle-x11.h"

/* undef and use the function instead of the macro as the macro is buggy */
#ifdef XSyncValueAdd
#undef XSyncValueAdd
#endif

static void     xfpm_idle_x11_finalize           (GObject        *object);
static void     xfpm_idle_x11_alarm_reset_all    (XfpmIdle       *idle);
static void     xfpm_idle_x11_alarm_add          (XfpmIdle       *idle,
                                                  XfpmAlarmId     id,
                                                  guint           timeout);
static void     xfpm_idle_x11_alarm_remove       (XfpmIdle       *idle,
                                                  XfpmAlarmId     id);

struct _XfpmIdleX11
{
  XfpmIdle __parent__;

  gint sync_event;
  gboolean reset_set;
  XSyncCounter idle_counter;
  GPtrArray *alarms;
};

typedef struct _Alarm
{
  XfpmAlarmId id;
  XSyncValue timeout;
  XSyncAlarm xalarm;
} Alarm;

typedef enum _AlarmType
{
  ALARM_TYPE_POSITIVE,
  ALARM_TYPE_NEGATIVE,
  ALARM_TYPE_DISABLED
} AlarmType;



G_DEFINE_FINAL_TYPE (XfpmIdleX11, xfpm_idle_x11, XFPM_TYPE_IDLE)



static void
xfpm_idle_x11_class_init (XfpmIdleX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  XfpmIdleClass *idle_class = XFPM_IDLE_CLASS (klass);

  object_class->finalize = xfpm_idle_x11_finalize;

  idle_class->alarm_reset_all = xfpm_idle_x11_alarm_reset_all;
  idle_class->alarm_add = xfpm_idle_x11_alarm_add;
  idle_class->alarm_remove = xfpm_idle_x11_alarm_remove;
}

static void
xsync_alarm_set (XfpmIdleX11 *idle,
                 Alarm *alarm,
                 AlarmType alarm_type)
{
  Display *display = gdk_x11_get_default_xdisplay ();
  XSyncAlarmAttributes attr;
  XSyncValue delta;
  unsigned int flags;
  XSyncTestType test;

  /* just remove it */
  if (alarm_type == ALARM_TYPE_DISABLED)
  {
    if (alarm->xalarm)
    {
      XSyncDestroyAlarm (display, alarm->xalarm);
      alarm->xalarm = None;
    }
    return;
  }

  /* which way do we do the test? */
  if (alarm_type == ALARM_TYPE_POSITIVE)
    test = XSyncPositiveTransition;
  else
    test = XSyncNegativeTransition;

  XSyncIntToValue (&delta, 0);

  attr.trigger.counter = idle->idle_counter;
  attr.trigger.value_type = XSyncAbsolute;
  attr.trigger.test_type = test;
  attr.trigger.wait_value = alarm->timeout;
  attr.delta = delta;

  flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType | XSyncCAValue | XSyncCADelta;

  if (alarm->xalarm)
    XSyncChangeAlarm (display, alarm->xalarm, flags, &attr);
  else
    alarm->xalarm = XSyncCreateAlarm (display, flags, &attr);
}

static Alarm *
alarm_find_id (XfpmIdleX11 *idle,
               XfpmAlarmId id)
{
  for (guint i = 0; i < idle->alarms->len; i++)
  {
    Alarm *alarm = g_ptr_array_index (idle->alarms, i);
    if (alarm->id == id)
      return alarm;
  }
  return NULL;
}

static void
set_reset_alarm (XfpmIdleX11 *idle,
                 XSyncAlarmNotifyEvent *alarm_event)
{
  Alarm *alarm;
  int overflow;
  XSyncValue add;

  if (idle->reset_set)
    return;

  /* don't match on the current value because
   * XSyncNegativeComparison means less or equal */
  alarm = alarm_find_id (idle, 0);
  XSyncIntToValue (&add, -1);
  XSyncValueAdd (&alarm->timeout, alarm_event->counter_value, add, &overflow);

  /* set the reset alarm to fire the next time
   * idle->idle_counter < the current counter value */
  xsync_alarm_set (idle, alarm, ALARM_TYPE_NEGATIVE);

  /* don't try to set this again if multiple timers are going off in sequence */
  idle->reset_set = TRUE;
}

static Alarm *
alarm_find_event (XfpmIdleX11 *idle,
                  XSyncAlarmNotifyEvent *alarm_event)
{
  for (guint i = 0; i < idle->alarms->len; i++)
  {
    Alarm *alarm = g_ptr_array_index (idle->alarms, i);
    if (alarm_event->alarm == alarm->xalarm)
      return alarm;
  }
  return NULL;
}

static GdkFilterReturn
event_filter_cb (GdkXEvent *gdkxevent,
                 GdkEvent *event,
                 gpointer data)
{
  Alarm *alarm;
  XEvent *xevent = (XEvent *) gdkxevent;
  XfpmIdleX11 *idle = (XfpmIdleX11 *) data;
  XSyncAlarmNotifyEvent *alarm_event;

  /* no point continuing */
  if (xevent->type != idle->sync_event + XSyncAlarmNotify)
    return GDK_FILTER_CONTINUE;

  alarm_event = (XSyncAlarmNotifyEvent *) xevent;

  /* did we match one of our alarms? */
  alarm = alarm_find_event (idle, alarm_event);
  if (alarm == NULL)
    return GDK_FILTER_CONTINUE;

  /* are we the reset alarm? */
  if (alarm->id == 0)
  {
    xfpm_idle_x11_alarm_reset_all (XFPM_IDLE (idle));
    g_signal_emit_by_name (idle, "reset");
    goto out;
  }

  /* emit */
  g_signal_emit_by_name (idle, "alarm-expired", alarm->id);

  /* we need the first alarm to go off to set the reset alarm */
  set_reset_alarm (idle, alarm_event);
out:
  /* don't propagate */
  return GDK_FILTER_REMOVE;
}

static Alarm *
alarm_new (XfpmIdleX11 *idle,
           XfpmAlarmId id)
{
  /* create a new alarm */
  Alarm *alarm = g_new0 (Alarm, 1);

  /* set the default values */
  alarm->id = id;
  alarm->xalarm = None;

  return alarm;
}

static void
alarm_free (gpointer data)
{
  Alarm *alarm = data;
  if (alarm->xalarm)
    XSyncDestroyAlarm (gdk_x11_get_default_xdisplay (), alarm->xalarm);
  g_free (alarm);
}

static void
xfpm_idle_x11_init (XfpmIdleX11 *idle)
{
  Alarm *alarm;
  Display *display = gdk_x11_get_default_xdisplay ();
  XSyncSystemCounter *counters;
  int sync_error;
  int ncounters;

  idle->alarms = g_ptr_array_new_with_free_func (alarm_free);

  idle->reset_set = FALSE;
  idle->idle_counter = None;
  idle->sync_event = 0;

  /* get the sync event */
  if (!XSyncQueryExtension (display, &idle->sync_event, &sync_error))
  {
    g_warning ("No Sync extension.");
    return;
  }

  /* gtk_init should do XSyncInitialize for us */
  counters = XSyncListSystemCounters (display, &ncounters);
  for (guint i = 0; i < (guint) ncounters && !idle->idle_counter; i++)
  {
    if (strcmp(counters[i].name, "IDLETIME") == 0)
      idle->idle_counter = counters[i].counter;
  }
  XSyncFreeSystemCounterList (counters);

  /* arh. we don't have IDLETIME support */
  if (!idle->idle_counter)
  {
    g_warning ("No idle counter.");
    return;
  }

  /* catch the timer alarm */
  gdk_window_add_filter (NULL, event_filter_cb, idle);

  /* create a reset alarm */
  alarm = alarm_new (idle, 0);
  g_ptr_array_add (idle->alarms, alarm);
}

static void
xfpm_idle_x11_finalize (GObject *object)
{
  XfpmIdleX11 *idle = XFPM_IDLE_X11 (object);

  /* free all counters, including reset counter */
  g_ptr_array_free (idle->alarms, TRUE);

  G_OBJECT_CLASS (xfpm_idle_x11_parent_class)->finalize (object);
}

static void
xfpm_idle_x11_alarm_reset_all (XfpmIdle *_idle)
{
  XfpmIdleX11 *idle = XFPM_IDLE_X11 (_idle);
  Alarm *alarm;

  /* reset all the alarms (except the reset alarm) to their timeouts */
  for (guint i = 1; i < idle->alarms->len; i++)
  {
    alarm = g_ptr_array_index (idle->alarms, i);
    xsync_alarm_set (idle, alarm, ALARM_TYPE_POSITIVE);
  }

  /* set the reset alarm to be disabled */
  alarm = g_ptr_array_index (idle->alarms, 0);
  xsync_alarm_set (idle, alarm, ALARM_TYPE_DISABLED);

  /* we need to be reset again on the next event */
  idle->reset_set = FALSE;
}

static void
xfpm_idle_x11_alarm_add (XfpmIdle *_idle,
                         XfpmAlarmId id,
                         guint timeout)
{
  XfpmIdleX11 *idle = XFPM_IDLE_X11 (_idle);
  Alarm *alarm = alarm_find_id (idle, id);

  /* see if we already created an alarm with this ID */
  if (alarm == NULL)
  {
    /* create a new alarm */
    alarm = alarm_new (idle, id);

    /* add to array */
    g_ptr_array_add (idle->alarms, alarm);
  }

  /* set the timeout */
  XSyncIntToValue (&alarm->timeout, (gint) timeout);

  /* set, and start the timer */
  xsync_alarm_set (idle, alarm, ALARM_TYPE_POSITIVE);
}

static void
xfpm_idle_x11_alarm_remove (XfpmIdle *_idle,
                            XfpmAlarmId id)
{
  XfpmIdleX11 *idle = XFPM_IDLE_X11 (_idle);
  Alarm *alarm = alarm_find_id (idle, id);
  if (alarm == NULL)
    return;

  g_ptr_array_remove (idle->alarms, alarm);
}
