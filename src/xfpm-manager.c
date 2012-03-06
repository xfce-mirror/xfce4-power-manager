/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libnotify/notify.h>

#include "xfpm-power.h"
#include "xfpm-dbus.h"
#include "xfpm-disks.h"
#include "xfpm-dpms.h"
#include "xfpm-manager.h"
#include "xfpm-button.h"
#include "xfpm-backlight.h"
#include "egg-idletime.h"
#include "xfpm-config.h"
#include "xfpm-debug.h"
#include "xfpm-xfconf.h"
#include "xfpm-errors.h"
#include "xfpm-common.h"
#include "xfpm-enum.h"
#include "xfpm-enum-glib.h"
#include "xfpm-enum-types.h"
#include "xfpm-dbus-monitor.h"
#include "xfpm-dbus-service.h"

static void xfpm_manager_finalize   (GObject *object);

static gboolean xfpm_manager_quit (XfpmManager *manager);

#define XFPM_MANAGER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_MANAGER, XfpmManagerPrivate))

#define SLEEP_KEY_TIMEOUT 6.0f

struct XfpmManagerPrivate
{
    DBusGConnection *session_bus;
    
    XfceSMClient    *client;
    
    XfpmPower       *power;
    XfpmButton      *button;
    XfpmXfconf      *conf;
    XfpmBacklight   *backlight;
    XfpmDBusMonitor *monitor;
    XfpmDisks       *disks;
    XfpmDBusService *dbus_service;
    EggIdletime     *idle;
#ifdef HAVE_DPMS
    XfpmDpms        *dpms;
#endif

    GTimer	    *timer;

    gboolean	     session_managed;
};

G_DEFINE_TYPE (XfpmManager, xfpm_manager, G_TYPE_OBJECT)

static void
xfpm_manager_class_init (XfpmManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_manager_finalize;

    g_type_class_add_private (klass, sizeof (XfpmManagerPrivate));
}

static void
xfpm_manager_init (XfpmManager *manager)
{
    manager->priv = XFPM_MANAGER_GET_PRIVATE (manager);

    manager->priv->timer = g_timer_new ();
    
    notify_init ("xfce4-power-manager");
}

static void
xfpm_manager_finalize (GObject *object)
{
    XfpmManager *manager;

    manager = XFPM_MANAGER(object);

    if ( manager->priv->session_bus )
	dbus_g_connection_unref (manager->priv->session_bus);
	
    g_object_unref (manager->priv->dbus_service);
    g_object_unref (manager->priv->power);
    g_object_unref (manager->priv->button);
    g_object_unref (manager->priv->conf);
    g_object_unref (manager->priv->client);
    g_object_unref (manager->priv->monitor);
    g_object_unref (manager->priv->disks);
    g_object_unref (manager->priv->idle);
    
    g_timer_destroy (manager->priv->timer);
    
#ifdef HAVE_DPMS
    g_object_unref (manager->priv->dpms);
#endif
    
    g_object_unref (manager->priv->backlight);
	
    G_OBJECT_CLASS (xfpm_manager_parent_class)->finalize (object);
}


static gboolean
xfpm_manager_quit (XfpmManager *manager)
{
    XFPM_DEBUG ("Exiting");
    
    gtk_main_quit ();
    return TRUE;
}

static void
xfpm_manager_system_bus_connection_changed_cb (XfpmDBusMonitor *monitor, gboolean connected, XfpmManager *manager)
{
    if ( connected == TRUE )
    {
        XFPM_DEBUG ("System bus connection changed to TRUE, restarting the power manager");
        xfpm_manager_quit (manager);
        g_spawn_command_line_async ("xfce4-power-manager", NULL);
    }
}


static void
xfpm_manager_shutdown (XfpmManager *manager)
{
    /* TODO */
}

static void
xfpm_manager_ask_shutdown (XfpmManager *manager)
{
    if ( manager->priv->session_managed )
	xfce_sm_client_request_shutdown (manager->priv->client, XFCE_SM_CLIENT_SHUTDOWN_HINT_ASK);
}

static void
xfpm_manager_sleep_request (XfpmManager *manager, XfpmShutdownRequest req, gboolean force)
{
    switch (req)
    {
	case XFPM_DO_NOTHING:
	    break;
	case XFPM_DO_SUSPEND:
	    xfpm_power_suspend (manager->priv->power, force);
	    break;
	case XFPM_DO_HIBERNATE:
	    xfpm_power_hibernate (manager->priv->power, force);
	    break;
	case XFPM_DO_SHUTDOWN:
	    xfpm_manager_shutdown (manager);
	    break;
	case XFPM_ASK:
	    xfpm_manager_ask_shutdown (manager);
	    break;
	default:
	    g_warn_if_reached ();
	    break;
    }
}

static void
xfpm_manager_reset_sleep_timer (XfpmManager *manager)
{
    g_timer_reset (manager->priv->timer);
}

static void
xfpm_manager_button_pressed_cb (XfpmButton *bt, XfpmButtonKey type, XfpmManager *manager)
{
    XfpmShutdownRequest req = XFPM_DO_NOTHING;
    
    XFPM_DEBUG_ENUM (type, XFPM_TYPE_BUTTON_KEY, "Received button press event");
  
    if ( type == BUTTON_MON_BRIGHTNESS_DOWN || type == BUTTON_MON_BRIGHTNESS_UP )
        return;
	
    if ( type == BUTTON_POWER_OFF )
    {
        g_object_get (G_OBJECT (manager->priv->conf),
                      POWER_SWITCH_CFG, &req,
                      NULL);
    }
    else if ( type == BUTTON_SLEEP )
    {
        g_object_get (G_OBJECT (manager->priv->conf),
                      SLEEP_SWITCH_CFG, &req,
                      NULL);
    }
    else if ( type == BUTTON_HIBERNATE )
    {
        g_object_get (G_OBJECT (manager->priv->conf),
                      HIBERNATE_SWITCH_CFG, &req,
                      NULL);
    }
    else
    {
        g_return_if_reached ();
    }

    XFPM_DEBUG_ENUM (req, XFPM_TYPE_SHUTDOWN_REQUEST, "Shutdown request : ");
        
    if ( req == XFPM_ASK )
	xfpm_manager_ask_shutdown (manager);
    else
    {
	if ( g_timer_elapsed (manager->priv->timer, NULL) > SLEEP_KEY_TIMEOUT )
	{
	    g_timer_reset (manager->priv->timer);
	    xfpm_manager_sleep_request (manager, req, FALSE);
	}
    }
}

static void
xfpm_manager_lid_changed_cb (XfpmPower *power, gboolean lid_is_closed, XfpmManager *manager)
{
    XfpmLidTriggerAction action;
    gboolean on_battery = FALSE;
    
    /*TODOg_object_get (G_OBJECT (power),
		  "on-battery", &on_battery,
		  NULL);*/
    
    g_object_get (G_OBJECT (manager->priv->conf),
		  on_battery ? LID_SWITCH_ON_BATTERY_CFG : LID_SWITCH_ON_AC_CFG, &action,
		  NULL);

    if ( lid_is_closed )
    {
	XFPM_DEBUG_ENUM (action, XFPM_TYPE_LID_TRIGGER_ACTION, "LID close event");
	
	if ( action == LID_TRIGGER_NOTHING )
	{
	    if ( !xfpm_is_multihead_connected () )
		xfpm_dpms_force_level (manager->priv->dpms, DPMSModeOff);
	}
	else if ( action == LID_TRIGGER_LOCK_SCREEN )
	{
	    if ( !xfpm_is_multihead_connected () )
		xfpm_lock_screen ();
	}
	else 
	{
	    /*
	     * Force sleep here as lid is closed and no point of asking the
	     * user for confirmation in case of an application is inhibiting
	     * the power manager. 
	     */
	    xfpm_manager_sleep_request (manager, action, TRUE);
	}
	
    }
    else
    {
	XFPM_DEBUG_ENUM (action, XFPM_TYPE_LID_TRIGGER_ACTION, "LID opened");
	xfpm_dpms_force_level (manager->priv->dpms, DPMSModeOn);
    }
}

static void
xfpm_manager_alarm_timeout_cb (EggIdletime *idle, guint id, XfpmManager *manager)
{
    if (xfpm_power_get_mode (manager->priv->power) == XFPM_POWER_MODE_PRESENTATION)
	return;
    
    XFPM_DEBUG ("Alarm inactivity timeout id %d", id);
    
    if ( id == TIMEOUT_INACTIVITY_ON_AC || id == TIMEOUT_INACTIVITY_ON_BATTERY )
    {
	XfpmShutdownRequest req = XFPM_DO_NOTHING;
	gchar *sleep_mode;
	gboolean on_battery = FALSE;
    
	g_object_get (G_OBJECT (manager->priv->conf),
		      INACTIVITY_SLEEP_MODE, &sleep_mode,
		      NULL);
	
	/* TODO g_object_get (G_OBJECT (manager->priv->power),
		      "on-battery", &on_battery,
		      NULL); */
		  
	if ( !g_strcmp0 (sleep_mode, "Suspend") )
	    req = XFPM_DO_SUSPEND;
	else
	    req = XFPM_DO_HIBERNATE;
	
	g_free (sleep_mode);

	if ( id == TIMEOUT_INACTIVITY_ON_AC && on_battery == FALSE )
	    xfpm_manager_sleep_request (manager, req, FALSE);
	else if ( id ==  TIMEOUT_INACTIVITY_ON_BATTERY && on_battery  )
	    xfpm_manager_sleep_request (manager, req, FALSE);
    }
}

static void
xfpm_manager_set_idle_alarm_on_ac (XfpmManager *manager)
{
    guint on_ac;
    
    g_object_get (G_OBJECT (manager->priv->conf),
		  ON_AC_INACTIVITY_TIMEOUT, &on_ac,
		  NULL);
    
#ifdef DEBUG
    if ( on_ac == 14 )
	TRACE ("setting inactivity sleep timeout on ac to never");
    else
	TRACE ("setting inactivity sleep timeout on ac to %d", on_ac);
#endif
    
    if ( on_ac == 14 )
    {
	egg_idletime_alarm_remove (manager->priv->idle, TIMEOUT_INACTIVITY_ON_AC );
    }
    else
    {
	egg_idletime_alarm_set (manager->priv->idle, TIMEOUT_INACTIVITY_ON_AC, on_ac * 1000 * 60);
    }
}

static void
xfpm_manager_set_idle_alarm_on_battery (XfpmManager *manager)
{
    guint on_battery;
    
    g_object_get (G_OBJECT (manager->priv->conf),
		  ON_BATTERY_INACTIVITY_TIMEOUT, &on_battery,
		  NULL);
    
#ifdef DEBUG
    if ( on_battery == 14 )
	TRACE ("setting inactivity sleep timeout on battery to never");
    else
	TRACE ("setting inactivity sleep timeout on battery to %d", on_battery);
#endif
    
    if ( on_battery == 14 )
    {
	egg_idletime_alarm_remove (manager->priv->idle, TIMEOUT_INACTIVITY_ON_BATTERY );
    }
    else
    {
	egg_idletime_alarm_set (manager->priv->idle, TIMEOUT_INACTIVITY_ON_BATTERY, on_battery * 1000 * 60);
    }
}

static void
xfpm_manager_on_battery_changed_cb (XfpmPower *power, gboolean on_battery, XfpmManager *manager)
{
    egg_idletime_alarm_reset_all (manager->priv->idle);
}

static void
xfpm_manager_set_idle_alarm (XfpmManager *manager)
{
    xfpm_manager_set_idle_alarm_on_ac (manager);
    xfpm_manager_set_idle_alarm_on_battery (manager);
    
}

XfpmManager *
xfpm_manager_new (DBusGConnection *bus, const gchar *client_id)
{
    XfpmManager *manager = NULL;
    GError *error = NULL;
    gchar *current_dir;
    
    const gchar *restart_command[] =
    {
	"xfce4-power-manager",
	"--restart",
	NULL
    };
	
    manager = g_object_new (XFPM_TYPE_MANAGER, NULL);

    manager->priv->session_bus = bus;
    
    current_dir = g_get_current_dir ();
    manager->priv->client = xfce_sm_client_get_full (XFCE_SM_CLIENT_RESTART_NORMAL,
						     XFCE_SM_CLIENT_PRIORITY_DEFAULT,
						     client_id,
						     current_dir,
						     restart_command,
						     SYSCONFDIR "/xdg/autostart/" PACKAGE_NAME ".desktop");
    
    g_free (current_dir);
    
    manager->priv->session_managed = xfce_sm_client_connect (manager->priv->client, &error);
    
    if ( error )
    {
	g_warning ("Unable to connect to session managet : %s", error->message);
	g_error_free (error);
    }
    else
    {
	g_signal_connect_swapped (manager->priv->client, "quit",
				  G_CALLBACK (xfpm_manager_quit), manager);
    }
    
    return manager;
}

void xfpm_manager_start (XfpmManager *manager)
{
    
    dbus_g_error_domain_register (XFPM_ERROR,
				  NULL,
				  XFPM_TYPE_ERROR);
    
    manager->priv->dbus_service = xfpm_dbus_service_new ();
    manager->priv->power = xfpm_power_get ();
    manager->priv->button = xfpm_button_new ();
    manager->priv->conf = xfpm_xfconf_new ();
    manager->priv->monitor = xfpm_dbus_monitor_new ();
    manager->priv->disks = xfpm_disks_new ();
    manager->priv->idle = egg_idletime_new ();
    
    g_signal_connect (manager->priv->idle, "alarm-expired",
		      G_CALLBACK (xfpm_manager_alarm_timeout_cb), manager);
    
    g_signal_connect_swapped (manager->priv->conf, "notify::" ON_AC_INACTIVITY_TIMEOUT,
			      G_CALLBACK (xfpm_manager_set_idle_alarm_on_ac), manager);
		      
    g_signal_connect_swapped (manager->priv->conf, "notify::" ON_BATTERY_INACTIVITY_TIMEOUT,
			      G_CALLBACK (xfpm_manager_set_idle_alarm_on_battery), manager);
    
    xfpm_manager_set_idle_alarm (manager);
    
    g_signal_connect (manager->priv->monitor, "system-bus-connection-changed",
		      G_CALLBACK (xfpm_manager_system_bus_connection_changed_cb), manager);
   
    manager->priv->backlight = xfpm_backlight_new ();
    
#ifdef HAVE_DPMS
    manager->priv->dpms = xfpm_dpms_new ();
#endif
    
    g_signal_connect (manager->priv->button, "button_pressed",
		      G_CALLBACK (xfpm_manager_button_pressed_cb), manager);
    
    g_signal_connect (manager->priv->power, "lid-changed",
		      G_CALLBACK (xfpm_manager_lid_changed_cb), manager);
    
    g_signal_connect (manager->priv->power, "on-battery-changed",
		      G_CALLBACK (xfpm_manager_on_battery_changed_cb), manager);
    
    g_signal_connect_swapped (manager->priv->power, "waking-up",
			      G_CALLBACK (xfpm_manager_reset_sleep_timer), manager);
    
    g_signal_connect_swapped (manager->priv->power, "sleeping",
			      G_CALLBACK (xfpm_manager_reset_sleep_timer), manager);
			      
    g_signal_connect_swapped (manager->priv->power, "ask-shutdown",
			      G_CALLBACK (xfpm_manager_ask_shutdown), manager);
    
    g_signal_connect_swapped (manager->priv->power, "shutdown",
			      G_CALLBACK (xfpm_manager_shutdown), manager);
			      
}

void xfpm_manager_stop (XfpmManager *manager)
{
    XFPM_DEBUG ("Stopping");
    g_return_if_fail (XFPM_IS_MANAGER (manager));
    xfpm_manager_quit (manager);
}
