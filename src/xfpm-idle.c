/*
 * * Copyright (C) 2008-2009 Ali <aliov@xfce.org>
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

/*
 * Modified version of libidletime from gpm version 2.24.2
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include "xfpm-idle.h"

/* Init */
static void xfpm_idle_class_init (XfpmIdleClass *klass);
static void xfpm_idle_init       (XfpmIdle *idle);
static void xfpm_idle_finalize   (GObject *object);

#define XFPM_IDLE_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_IDLE, XfpmIdlePrivate))

struct XfpmIdlePrivate
{
    int 		sync_event;
    XSyncCounter	idle_counter;
    GPtrArray          *array;
};

typedef struct
{
    guint	id;
    XSyncValue  timeout;
    XSyncAlarm  xalarm;
    
} IdleAlarm;

enum
{
    RESET,
    ALARM_TIMEOUT,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmIdle, xfpm_idle, G_TYPE_OBJECT)

static void
xfpm_idle_class_init(XfpmIdleClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

     signals[RESET] =
	    g_signal_new("reset",
			 XFPM_TYPE_IDLE,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmIdleClass, reset),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE, 0, G_TYPE_NONE);

    signals[ALARM_TIMEOUT] =
	    g_signal_new("alarm-timeout",
			 XFPM_TYPE_IDLE,
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(XfpmIdleClass, alarm_timeout),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__INT,
			 G_TYPE_NONE, 1, G_TYPE_INT);
			 
    object_class->finalize = xfpm_idle_finalize;

    g_type_class_add_private(klass,sizeof(XfpmIdlePrivate));
}

static IdleAlarm *
xfpm_idle_find_alarm (XfpmIdle *idle, guint id)
{
    int i;
    IdleAlarm *alarm;
    for (i = 0; i<idle->priv->array->len; i++) 
    {
	alarm = g_ptr_array_index (idle->priv->array, i);
	if (alarm->id == id)
	{
	    return alarm;
	}
    }
    return NULL;
}

static void
xfpm_idle_xsync_alarm_set (XfpmIdle *idle, IdleAlarm *alarm, gboolean positive)
{
    XSyncAlarmAttributes attr;
    XSyncValue delta;
    unsigned int flags;
    XSyncTestType test;

    if (positive) 
	test = XSyncPositiveComparison;
    else 
	test = XSyncNegativeComparison;

    XSyncIntToValue (&delta, 0);

    attr.trigger.counter = idle->priv->idle_counter;
    attr.trigger.value_type = XSyncAbsolute;
    attr.trigger.test_type = test;
    attr.trigger.wait_value = alarm->timeout;
    attr.delta = delta;

    flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType | XSyncCAValue | XSyncCADelta;

    if ( alarm->xalarm ) 
	XSyncChangeAlarm ( GDK_DISPLAY (), alarm->xalarm, flags, &attr);
    else 
	alarm->xalarm = XSyncCreateAlarm (GDK_DISPLAY (), flags, &attr);
}

static void
xfpm_idle_xsync_value_add_one (XSyncValue *from, XSyncValue *to)
{
    int overflow;
    XSyncValue add;
    XSyncIntToValue (&add, -1);
    XSyncValueAdd (to, *from, add, &overflow);
}

static void
xfpm_idle_x_set_reset (XfpmIdle *idle, XSyncAlarmNotifyEvent *alarm_event)
{
    IdleAlarm *alarm;
    
    alarm = xfpm_idle_find_alarm (idle, 0);
    xfpm_idle_xsync_value_add_one (&alarm_event->counter_value, &alarm->timeout);
    xfpm_idle_xsync_alarm_set (idle, alarm, FALSE);
}

static IdleAlarm *
xfpm_idle_alarm_find_event (XfpmIdle *idle, XSyncAlarmNotifyEvent *alarm_event)
{
    guint i;
    IdleAlarm *alarm;
    
    for (i=0; i<idle->priv->array->len; i++) 
    {
	alarm = g_ptr_array_index (idle->priv->array, i);
	if (alarm_event->alarm == alarm->xalarm) 
	{
	    return alarm;
	}
    }
    return NULL;
}

void
xfpm_idle_alarm_reset_all (XfpmIdle *idle)
{
    guint i;
    IdleAlarm *alarm;

    for ( i=1; i<idle->priv->array->len; i++) 
    {
	alarm = g_ptr_array_index (idle->priv->array, i);
	xfpm_idle_xsync_alarm_set (idle, alarm, TRUE);
    }
	
    g_signal_emit (G_OBJECT(idle), signals[RESET], 0 );
}

static GdkFilterReturn
xfpm_idle_x_event_filter (GdkXEvent *gdkxevent, GdkEvent *event, gpointer data)
{
    IdleAlarm *alarm;
    XfpmIdle *idle = (XfpmIdle *) data;
    XEvent *xevent = ( XEvent *) gdkxevent;
    XSyncAlarmNotifyEvent *alarm_event;
    
    if ( xevent->type != idle->priv->sync_event + XSyncAlarmNotify )
        return GDK_FILTER_CONTINUE;
    
    alarm_event = (XSyncAlarmNotifyEvent *) xevent;
    
    alarm = xfpm_idle_alarm_find_event (idle, alarm_event);
    
    if ( alarm )
    {
	if (alarm->id != 0 )
	{
	    g_signal_emit (G_OBJECT(idle), signals[ALARM_TIMEOUT], 0, alarm->id );
	    xfpm_idle_x_set_reset (idle, alarm_event);
	    return GDK_FILTER_CONTINUE;
	}
	xfpm_idle_alarm_reset_all (idle);
    }
    
    return GDK_FILTER_CONTINUE;
}

static IdleAlarm *
xfpm_idle_new_alarm_internal (XfpmIdle *idle, guint id)
{
    IdleAlarm *alarm;
    alarm = g_new0 (IdleAlarm, 1);
    alarm->id = id;
    g_ptr_array_add (idle->priv->array, alarm);
    
    return alarm;
}

static void
xfpm_idle_init (XfpmIdle *idle)
{
    IdleAlarm *alarm;
    int sync_error = 0;
    int ncounters;
    XSyncSystemCounter *counters;
    int i;
    
    idle->priv = XFPM_IDLE_GET_PRIVATE(idle);
    
    idle->priv->array = g_ptr_array_new ();
    idle->priv->sync_event = 0;
    
    if (!XSyncQueryExtension (GDK_DISPLAY (), &idle->priv->sync_event, &sync_error) )
    {
	g_warning ("No Sync extension.");
	return;
    }
    
    counters = XSyncListSystemCounters (GDK_DISPLAY (), &ncounters);
    
    for ( i = 0; i < ncounters && !idle->priv->idle_counter; i++)
    {
	if (!strcmp(counters[i].name, "IDLETIME"))
	    idle->priv->idle_counter = counters[i].counter;
    }
    
    if ( !idle->priv->idle_counter )
    {
	g_warning ("No idle counter.");
	return;
    }
    
    gdk_window_add_filter (NULL, xfpm_idle_x_event_filter, idle);
    
    alarm = xfpm_idle_new_alarm_internal (idle, 0);
}

static void
xfpm_idle_free_alarm (XfpmIdle *idle, IdleAlarm *alarm)
{
    gdk_error_trap_push ();
    XSyncDestroyAlarm (GDK_DISPLAY(), alarm->xalarm);
    gdk_flush ();
    gdk_error_trap_pop ();
    g_free(alarm);
    g_ptr_array_remove (idle->priv->array, alarm);
}

static void
xfpm_idle_finalize(GObject *object)
{
    int i;
    XfpmIdle *idle;
    IdleAlarm *alarm;
    
    idle = XFPM_IDLE(object);

    for ( i = 0; i<idle->priv->array->len; i++) 
    {
	alarm = g_ptr_array_index (idle->priv->array, i);
	xfpm_idle_free_alarm (idle, alarm);
    }
    g_ptr_array_free (idle->priv->array, TRUE);
    
    gdk_window_remove_filter (NULL, xfpm_idle_x_event_filter, idle);

    G_OBJECT_CLASS(xfpm_idle_parent_class)->finalize(object);
}

XfpmIdle *
xfpm_idle_new(void)
{
    XfpmIdle *idle = NULL;
    idle = g_object_new (XFPM_TYPE_IDLE,NULL);
    return idle;
}

gboolean 
xfpm_idle_set_alarm (XfpmIdle *idle, guint id, guint timeout)
{
    IdleAlarm *alarm;
    
    g_return_val_if_fail (XFPM_IS_IDLE (idle), FALSE);
    
    if ( id == 0 )
	return FALSE;
	
    if ( timeout == 0 )
	return FALSE;
    
    alarm = xfpm_idle_find_alarm (idle, id);
    
    if ( !alarm )
    {
	alarm = xfpm_idle_new_alarm_internal (idle, id);
    }
    
    XSyncIntToValue (&alarm->timeout, timeout);
    xfpm_idle_xsync_alarm_set (idle, alarm, TRUE);
    return TRUE;
}

