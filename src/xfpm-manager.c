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

#include "xfpm-dkp.h"
#include "xfpm-dbus.h"
#include "xfpm-dpms.h"
#include "xfpm-manager.h"
#include "xfpm-button.h"
#include "xfpm-brightness.h"
#include "xfpm-config.h"
#include "xfpm-debug.h"
#include "xfpm-xfconf.h"
#include "xfpm-errors.h"
#include "xfpm-common.h"
#include "xfpm-enum.h"
#include "xfpm-enum-glib.h"
#include "xfpm-enum-types.h"

static void xfpm_manager_finalize   (GObject *object);

static void xfpm_manager_dbus_class_init (XfpmManagerClass *klass);
static void xfpm_manager_dbus_init	 (XfpmManager *manager);

static gboolean xfpm_manager_quit (XfpmManager *manager);

#define XFPM_MANAGER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_MANAGER, XfpmManagerPrivate))

#define SLEEP_KEY_TIMEOUT 6.0f

struct XfpmManagerPrivate
{
    DBusGConnection *session_bus;
    
    XfceSMClient    *client;
    
    XfpmDkp         *dkp;
    XfpmButton      *button;
    XfpmXfconf      *conf;
    XfpmBrightness  *brightness;
#ifdef HAVE_DPMS
    XfpmDpms        *dpms;
#endif
    GTimer	    *timer;
    
    gboolean	     inhibited;
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
    manager->priv = XFPM_MANAGER_GET_PRIVATE(manager);
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
	
    g_object_unref (manager->priv->dkp);
    g_object_unref (manager->priv->button);
    g_object_unref (manager->priv->conf);
    g_object_unref (manager->priv->client);
    g_timer_destroy (manager->priv->timer);
    
#ifdef HAVE_DPMS
    g_object_unref (manager->priv->dpms);
#endif
    
    if ( manager->priv->brightness )
	g_object_unref (manager->priv->brightness);
	
    G_OBJECT_CLASS (xfpm_manager_parent_class)->finalize (object);
}

static void
xfpm_manager_release_names (XfpmManager *manager)
{
    xfpm_dbus_release_name (dbus_g_connection_get_connection(manager->priv->session_bus),
			   "org.xfce.PowerManager");

    xfpm_dbus_release_name (dbus_g_connection_get_connection(manager->priv->session_bus),
			    "org.freedesktop.PowerManagement");
}

static gboolean
xfpm_manager_quit (XfpmManager *manager)
{
    TRACE ("Exiting");
    
    xfpm_manager_release_names (manager);
    gtk_main_quit ();
    return TRUE;
}

static gboolean
xfpm_manager_reserve_names (XfpmManager *manager)
{
    if ( !xfpm_dbus_register_name (dbus_g_connection_get_connection (manager->priv->session_bus),
				   "org.xfce.PowerManager") ||
	 !xfpm_dbus_register_name (dbus_g_connection_get_connection (manager->priv->session_bus),
				  "org.freedesktop.PowerManagement") )
    {
	g_warning ("Unable to reserve bus name: Maybe any already running instance?\n");
	
	g_object_unref (G_OBJECT (manager));
	gtk_main_quit ();
	
	return FALSE;
    }
    return TRUE;
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
	    xfpm_dkp_suspend (manager->priv->dkp, force);
	    break;
	case XFPM_DO_HIBERNATE:
	    xfpm_dkp_hibernate (manager->priv->dkp, force);
	    break;
	case XFPM_DO_SHUTDOWN:
	    /*FIXME ConsoleKit*/
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
    
    XFPM_DEBUG_ENUM ("Received button press event", type, XFPM_TYPE_BUTTON_KEY);
  
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

    XFPM_DEBUG_ENUM ("Shutdown request : ", req, XFPM_TYPE_SHUTDOWN_REQUEST);
        
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
xfpm_manager_lid_changed_cb (XfpmDkp *dkp, gboolean lid_is_closed, XfpmManager *manager)
{
    XfpmLidTriggerAction action;
    gboolean on_battery;
    
    g_object_get (G_OBJECT (dkp),
		  "on-battery", &on_battery,
		  NULL);
    
    g_object_get (G_OBJECT (manager->priv->conf),
		  on_battery ? LID_SWITCH_ON_BATTERY_CFG : LID_SWITCH_ON_AC_CFG, &action,
		  NULL);

    if ( lid_is_closed )
    {
	XFPM_DEBUG_ENUM ("LID close event", action, XFPM_TYPE_LID_TRIGGER_ACTION);
	
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
	XFPM_DEBUG_ENUM ("LID opened", action, XFPM_TYPE_LID_TRIGGER_ACTION);
	xfpm_dpms_force_level (manager->priv->dpms, DPMSModeOn);
    }
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
						     NULL);
    
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
    
    xfpm_manager_dbus_class_init (XFPM_MANAGER_GET_CLASS (manager));
    xfpm_manager_dbus_init (manager);
    return manager;
}

void xfpm_manager_start (XfpmManager *manager)
{
    if ( !xfpm_manager_reserve_names (manager) )
	goto out;
	
    dbus_g_error_domain_register (XFPM_ERROR,
				  NULL,
				  XFPM_TYPE_ERROR);
    
    manager->priv->dkp = xfpm_dkp_get ();
    manager->priv->button = xfpm_button_new ();
    manager->priv->conf = xfpm_xfconf_new ();
   
    manager->priv->brightness = xfpm_brightness_new ();
    
#ifdef HAVE_DPMS
    manager->priv->dpms = xfpm_dpms_new ();
#endif
    
    if ( !xfpm_brightness_setup (manager->priv->brightness) )
    {
	g_object_unref (manager->priv->brightness);
	manager->priv->brightness = NULL;
    }
     
    g_signal_connect (manager->priv->button, "button_pressed",
		      G_CALLBACK (xfpm_manager_button_pressed_cb), manager);
    
    g_signal_connect (manager->priv->dkp, "lid-changed",
		      G_CALLBACK (xfpm_manager_lid_changed_cb), manager);
    
    g_signal_connect_swapped (manager->priv->dkp, "waking-up",
			      G_CALLBACK (xfpm_manager_reset_sleep_timer), manager);
    
    g_signal_connect_swapped (manager->priv->dkp, "sleeping",
			      G_CALLBACK (xfpm_manager_reset_sleep_timer), manager);
			      
    g_signal_connect_swapped (manager->priv->dkp, "ask-shutdown",
			      G_CALLBACK (xfpm_manager_ask_shutdown), manager);
out:
	;
}

void xfpm_manager_stop (XfpmManager *manager)
{
    TRACE ("Stopping");
    g_return_if_fail (XFPM_IS_MANAGER (manager));
    xfpm_manager_quit (manager);
}

/*
 * 
 * DBus server implementation
 * 
 */
static gboolean xfpm_manager_dbus_quit       (XfpmManager *manager,
					      GError **error);
					      
static gboolean xfpm_manager_dbus_restart     (XfpmManager *manager,
					       GError **error);
					      
static gboolean xfpm_manager_dbus_get_config (XfpmManager *manager,
					      GHashTable **OUT_config,
					      GError **error);
					      					      
static gboolean xfpm_manager_dbus_get_info   (XfpmManager *manager,
					      gchar **OUT_name,
					      gchar **OUT_version,
					      gchar **OUT_vendor,
					      GError **error);

#include "xfce-power-manager-dbus-server.h"

static void
xfpm_manager_dbus_class_init (XfpmManagerClass *klass)
{
     dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
				     &dbus_glib_xfpm_manager_object_info);
}

static void
xfpm_manager_dbus_init (XfpmManager *manager)
{
    dbus_g_connection_register_g_object (manager->priv->session_bus,
					"/org/xfce/PowerManager",
					G_OBJECT (manager));
}

static gboolean
xfpm_manager_dbus_quit (XfpmManager *manager, GError **error)
{
    TRACE("Quit message received\n");
    
    xfpm_manager_quit (manager);
    
    return TRUE;
}

static gboolean xfpm_manager_dbus_restart     (XfpmManager *manager,
					       GError **error)
{
    TRACE("Restart message received");
    
    xfpm_manager_quit (manager);
    
    g_spawn_command_line_async ("xfce4-power-manager", NULL);
    
    return TRUE;
}

static gboolean xfpm_manager_dbus_get_config (XfpmManager *manager,
					      GHashTable **OUT_config,
					      GError **error)
{
    
    guint8 mapped_buttons;
    gboolean auth_hibernate = FALSE;
    gboolean auth_suspend = FALSE;
    gboolean can_suspend = FALSE;
    gboolean can_hibernate = FALSE;
    gboolean has_sleep_button = FALSE;
    gboolean has_hibernate_button = FALSE;
    gboolean has_power_button = FALSE;
    gboolean has_battery = TRUE;
    gboolean has_lcd_brightness = TRUE;
    gboolean has_lid = FALSE;
    
    *OUT_config = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    

    g_object_get (G_OBJECT (manager->priv->dkp),
                  "auth-suspend", &auth_suspend,
		  "auth-hibernate", &auth_hibernate,
                  "can-suspend", &can_suspend,
                  "can-hibernate", &can_hibernate, 
		  "has-lid", &has_lid,
		  NULL);
    
    has_battery = xfpm_dkp_has_battery (manager->priv->dkp);
    
    mapped_buttons = xfpm_button_get_mapped (manager->priv->button);
    
    if ( mapped_buttons & SLEEP_KEY )
        has_sleep_button = TRUE;
    if ( mapped_buttons & HIBERNATE_KEY )
        has_hibernate_button = TRUE;
    if ( mapped_buttons & POWER_KEY )
        has_power_button = TRUE;

    g_hash_table_insert (*OUT_config, g_strdup ("sleep-button"), g_strdup (xfpm_bool_to_string (has_sleep_button)));
    g_hash_table_insert (*OUT_config, g_strdup ("power-button"), g_strdup (xfpm_bool_to_string (has_power_button)));
    g_hash_table_insert (*OUT_config, g_strdup ("hibernate-button"), g_strdup (xfpm_bool_to_string (has_hibernate_button)));
    g_hash_table_insert (*OUT_config, g_strdup ("auth-suspend"), g_strdup (xfpm_bool_to_string (auth_suspend)));
    g_hash_table_insert (*OUT_config, g_strdup ("auth-hibernate"), g_strdup (xfpm_bool_to_string (auth_hibernate)));
    g_hash_table_insert (*OUT_config, g_strdup ("can-suspend"), g_strdup (xfpm_bool_to_string (can_suspend)));
    g_hash_table_insert (*OUT_config, g_strdup ("can-hibernate"), g_strdup (xfpm_bool_to_string (can_hibernate)));
    
    g_hash_table_insert (*OUT_config, g_strdup ("has-battery"), g_strdup (xfpm_bool_to_string (has_battery)));
    g_hash_table_insert (*OUT_config, g_strdup ("has-lid"), g_strdup (xfpm_bool_to_string (has_lid)));
    
    /*
    g_hash_table_insert (*OUT_config, g_strdup ("has-brightness"), g_strdup (xfpm_bool_to_string (has_lcd_brightness)));
    */
    return TRUE;
}
					      
static gboolean 
xfpm_manager_dbus_get_info (XfpmManager *manager,
			    gchar **OUT_name,
			    gchar **OUT_version,
			    gchar **OUT_vendor,
			    GError **error)
{
    
    *OUT_name    = g_strdup(PACKAGE);
    *OUT_version = g_strdup(VERSION);
    *OUT_vendor  = g_strdup("Xfce-goodies");
    
    return TRUE;
}
