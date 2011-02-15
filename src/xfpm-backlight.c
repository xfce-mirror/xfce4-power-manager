/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include <math.h>

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "xfpm-backlight.h"
#include "egg-idletime.h"
#include "xfpm-notify.h"
#include "xfpm-xfconf.h"
#include "xfpm-power.h"
#include "xfpm-config.h"
#include "xfpm-button.h"
#include "xfpm-brightness.h"
#include "xfpm-debug.h"
#include "xfpm-icons.h"

#include "gsd-media-keys-window.h"

static void xfpm_backlight_finalize     (GObject *object);

static void xfpm_backlight_create_popup (XfpmBacklight *backlight);

#define ALARM_DISABLED 9
#define BRIGHTNESS_POPUP_SIZE	180

#define XFPM_BACKLIGHT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_BACKLIGHT, XfpmBacklightPrivate))

struct XfpmBacklightPrivate
{
    XfpmBrightness *brightness;
    XfpmPower      *power;
    EggIdletime    *idle;
    XfpmXfconf     *conf;
    XfpmButton     *button;
    XfpmNotify     *notify;
    
    GtkWidget	   *osd;
    NotifyNotification *n;
    
    
    gulong	    destroy_id;
    
    gboolean	    has_hw;
    gboolean	    on_battery;
    
    gint            last_level;
    gint 	    max_level;
    
    gboolean        dimmed;
    gboolean	    block;
};

G_DEFINE_TYPE (XfpmBacklight, xfpm_backlight, G_TYPE_OBJECT)

static void
xfpm_backlight_dim_brightness (XfpmBacklight *backlight)
{
    gboolean ret;
    
    if (xfpm_power_get_mode (backlight->priv->power) == XFPM_POWER_MODE_NORMAL )
    {
	ret = xfpm_brightness_get_level (backlight->priv->brightness, &backlight->priv->last_level);
	
	if ( !ret )
	{
	    g_warning ("Unable to get current brightness level");
	    return;
	}
	XFPM_DEBUG ("Current brightness level before dimming : %u", backlight->priv->last_level);
	
	backlight->priv->dimmed = xfpm_brightness_dim_down (backlight->priv->brightness);
    }
}

static gboolean
xfpm_backlight_destroy_popup (gpointer data)
{
    XfpmBacklight *backlight;
    
    backlight = XFPM_BACKLIGHT (data);
    
    if ( backlight->priv->osd )
    {
	gtk_widget_destroy (backlight->priv->osd);
	backlight->priv->osd = NULL;
    }
    
    if ( backlight->priv->n )
    {
	g_object_unref (backlight->priv->n);
	backlight->priv->n = NULL;
    }
    
    return FALSE;
}

static void
xfpm_backlight_composited_changed_cb (XfpmBacklight *backlight)
{
    xfpm_backlight_destroy_popup (backlight);
    xfpm_backlight_create_popup (backlight);
}

static void
xfpm_backlight_show_notification (XfpmBacklight *backlight, gfloat value)
{
    gint i;
    
    static const char *display_icon_name[] = 
    {
	"notification-display-brightness-off",
	"notification-display-brightness-low",
	"notification-display-brightness-medium",
	"notification-display-brightness-high",
	"notification-display-brightness-full",
	NULL
    };
    
    if ( backlight->priv->n == NULL )
    {
	backlight->priv->n = xfpm_notify_new_notification (backlight->priv->notify, 
							   " ", 
							   "", 
							   NULL, 
							   0, 
							   XFPM_NOTIFY_NORMAL,
							   NULL);
    }
    
    i = (gint)value / 25;
    
    if ( i > 4 || i < 0 )
	return;
    
    notify_notification_set_hint_int32  (backlight->priv->n,
					 "value",
					 value);
    
    notify_notification_set_hint_string (backlight->priv->n,
					 "x-canonical-private-synchronous",
					 "brightness");
    
    notify_notification_update (backlight->priv->n,
			        " ",
				"",
				display_icon_name[i]);
				
    notify_notification_show (backlight->priv->n, NULL);
}

static void
xfpm_backlight_create_popup (XfpmBacklight *backlight)
{
    if ( backlight->priv->osd != NULL )
	return;
	
    backlight->priv->osd = gsd_media_keys_window_new ();
    gsd_media_keys_window_set_action_custom (GSD_MEDIA_KEYS_WINDOW (backlight->priv->osd),
					     XFPM_DISPLAY_BRIGHTNESS_ICON,
					     TRUE);
    gtk_window_set_position (GTK_WINDOW (backlight->priv->osd), GTK_WIN_POS_CENTER);
    
    g_signal_connect_swapped (backlight->priv->osd, "composited-changed",
			      G_CALLBACK (xfpm_backlight_composited_changed_cb), backlight);
			      
}

static void
xfpm_backlight_show (XfpmBacklight *backlight, gint level)
{
    gfloat value;
    gboolean sync_notify;
    gboolean show_popup;
    
    XFPM_DEBUG ("Level %u", level);
    
    g_object_get (G_OBJECT (backlight->priv->conf),
                  SHOW_BRIGHTNESS_POPUP, &show_popup,
                  NULL);
		  
    if ( !show_popup )
	goto out;
    
    g_object_get (G_OBJECT (backlight->priv->notify),
		  "sync", &sync_notify,
		  NULL);
    
    value = (gfloat) 100 * level / backlight->priv->max_level;
    
    if ( !sync_notify ) /*Notification server doesn't support sync notifications*/
    {
	xfpm_backlight_create_popup (backlight);
	gsd_media_keys_window_set_volume_level (GSD_MEDIA_KEYS_WINDOW (backlight->priv->osd),
						round (value));
	if ( !GTK_WIDGET_VISIBLE (backlight->priv->osd))
	    gtk_window_present (GTK_WINDOW (backlight->priv->osd));
    }
    else
    {
	xfpm_backlight_show_notification (backlight, value);
    }
    
    if ( backlight->priv->destroy_id != 0 )
    {
	g_source_remove (backlight->priv->destroy_id);
	backlight->priv->destroy_id = 0;
    }
    
out:
    /* Release the memory after 60 seconds */
    backlight->priv->destroy_id = g_timeout_add_seconds (60, (GSourceFunc) xfpm_backlight_destroy_popup, backlight);
}


static void
xfpm_backlight_alarm_timeout_cb (EggIdletime *idle, guint id, XfpmBacklight *backlight)
{
    backlight->priv->block = FALSE;
    
    if ( id == TIMEOUT_BRIGHTNESS_ON_AC && !backlight->priv->on_battery)
	xfpm_backlight_dim_brightness (backlight);
    else if ( id == TIMEOUT_BRIGHTNESS_ON_BATTERY && backlight->priv->on_battery)
	xfpm_backlight_dim_brightness (backlight);
}

static void
xfpm_backlight_reset_cb (EggIdletime *idle, XfpmBacklight *backlight)
{
    if ( backlight->priv->dimmed)
    {
	if ( !backlight->priv->block)
	{
	    XFPM_DEBUG ("Alarm reset, setting level to %i", backlight->priv->last_level);
	    xfpm_brightness_set_level (backlight->priv->brightness, backlight->priv->last_level);
	}
	backlight->priv->dimmed = FALSE;
    }
}

static void
xfpm_backlight_button_pressed_cb (XfpmButton *button, XfpmButtonKey type, XfpmBacklight *backlight)
{
    gint level;
    gboolean ret = TRUE;
    
    gboolean enable_brightness, show_popup;
    
    g_object_get (G_OBJECT (backlight->priv->conf),
                  ENABLE_BRIGHTNESS_CONTROL, &enable_brightness,
		  SHOW_BRIGHTNESS_POPUP, &show_popup,
                  NULL);
    
    if ( type == BUTTON_MON_BRIGHTNESS_UP )
    {
	backlight->priv->block = TRUE;
	if ( enable_brightness )
	    ret = xfpm_brightness_up (backlight->priv->brightness, &level);
	if ( ret && show_popup)
	    xfpm_backlight_show (backlight, level);
    }
    else if ( type == BUTTON_MON_BRIGHTNESS_DOWN )
    {
	backlight->priv->block = TRUE;
	if ( enable_brightness )
	    ret = xfpm_brightness_down (backlight->priv->brightness, &level);
	if ( ret && show_popup)
	    xfpm_backlight_show (backlight, level);
    }
}

static void
xfpm_backlight_brightness_on_ac_settings_changed (XfpmBacklight *backlight)
{
    guint timeout_on_ac;
    
    g_object_get (G_OBJECT (backlight->priv->conf),
		  BRIGHTNESS_ON_AC, &timeout_on_ac,
		  NULL);
		  
    XFPM_DEBUG ("Alarm on ac timeout changed %u", timeout_on_ac);
    
    if ( timeout_on_ac == ALARM_DISABLED )
    {
	egg_idletime_alarm_remove (backlight->priv->idle, TIMEOUT_BRIGHTNESS_ON_AC );
    }
    else
    {
	egg_idletime_alarm_set (backlight->priv->idle, TIMEOUT_BRIGHTNESS_ON_AC, timeout_on_ac * 1000);
    }
}

static void
xfpm_backlight_brightness_on_battery_settings_changed (XfpmBacklight *backlight)
{
    guint timeout_on_battery ;
    
    g_object_get (G_OBJECT (backlight->priv->conf),
		  BRIGHTNESS_ON_BATTERY, &timeout_on_battery,
		  NULL);
    
    XFPM_DEBUG ("Alarm on battery timeout changed %u", timeout_on_battery);
    
    if ( timeout_on_battery == ALARM_DISABLED )
    {
	egg_idletime_alarm_remove (backlight->priv->idle, TIMEOUT_BRIGHTNESS_ON_BATTERY );
    }
    else
    {
	egg_idletime_alarm_set (backlight->priv->idle, TIMEOUT_BRIGHTNESS_ON_BATTERY, timeout_on_battery * 1000);
    } 
}


static void
xfpm_backlight_set_timeouts (XfpmBacklight *backlight)
{
    xfpm_backlight_brightness_on_ac_settings_changed (backlight);
    xfpm_backlight_brightness_on_battery_settings_changed (backlight);
}

static void
xfpm_backlight_on_battery_changed_cb (XfpmPower *power, gboolean on_battery, XfpmBacklight *backlight)
{
    backlight->priv->on_battery = on_battery;
}

static void
xfpm_backlight_class_init (XfpmBacklightClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_backlight_finalize;

    g_type_class_add_private (klass, sizeof (XfpmBacklightPrivate));
}

static void
xfpm_backlight_init (XfpmBacklight *backlight)
{
    backlight->priv = XFPM_BACKLIGHT_GET_PRIVATE (backlight);
    
    backlight->priv->brightness = xfpm_brightness_new ();
    backlight->priv->has_hw     = xfpm_brightness_setup (backlight->priv->brightness);
    
    backlight->priv->osd    = NULL;
    backlight->priv->notify = NULL;
    backlight->priv->idle   = NULL;
    backlight->priv->conf   = NULL;
    backlight->priv->button = NULL;
    backlight->priv->power    = NULL;
    backlight->priv->dimmed = FALSE;
    backlight->priv->block = FALSE;
    backlight->priv->destroy_id = 0;
    
    if ( !backlight->priv->has_hw )
    {
	g_object_unref (backlight->priv->brightness);
	backlight->priv->brightness = NULL;
    }
    else
    {
	backlight->priv->idle   = egg_idletime_new ();
	backlight->priv->conf   = xfpm_xfconf_new ();
	backlight->priv->button = xfpm_button_new ();
	backlight->priv->power    = xfpm_power_get ();
	backlight->priv->notify = xfpm_notify_new ();
	backlight->priv->max_level = xfpm_brightness_get_max_level (backlight->priv->brightness);
	g_signal_connect (backlight->priv->idle, "alarm-expired",
                          G_CALLBACK (xfpm_backlight_alarm_timeout_cb), backlight);
        
        g_signal_connect (backlight->priv->idle, "reset",
                          G_CALLBACK(xfpm_backlight_reset_cb), backlight);
			  
	g_signal_connect (backlight->priv->button, "button-pressed",
		          G_CALLBACK (xfpm_backlight_button_pressed_cb), backlight);
			  
	g_signal_connect_swapped (backlight->priv->conf, "notify::" BRIGHTNESS_ON_AC,
				  G_CALLBACK (xfpm_backlight_brightness_on_ac_settings_changed), backlight);
	
	g_signal_connect_swapped (backlight->priv->conf, "notify::" BRIGHTNESS_ON_BATTERY,
				  G_CALLBACK (xfpm_backlight_brightness_on_battery_settings_changed), backlight);
				
	g_signal_connect (backlight->priv->power, "on-battery-changed",
			  G_CALLBACK (xfpm_backlight_on_battery_changed_cb), backlight);
	g_object_get (G_OBJECT (backlight->priv->power),
		      "on-battery", &backlight->priv->on_battery,
		      NULL);
	xfpm_brightness_get_level (backlight->priv->brightness, &backlight->priv->last_level);
	xfpm_backlight_set_timeouts (backlight);
    }
}

static void
xfpm_backlight_finalize (GObject *object)
{
    XfpmBacklight *backlight;

    backlight = XFPM_BACKLIGHT (object);

    xfpm_backlight_destroy_popup (backlight);

    if ( backlight->priv->brightness )
	g_object_unref (backlight->priv->brightness);

    if ( backlight->priv->idle )
	g_object_unref (backlight->priv->idle);

    if ( backlight->priv->conf )
	g_object_unref (backlight->priv->conf);

    if ( backlight->priv->button )
	g_object_unref (backlight->priv->button);

    if ( backlight->priv->power )
	g_object_unref (backlight->priv->power);

    if ( backlight->priv->notify)
	g_object_unref (backlight->priv->notify);

    G_OBJECT_CLASS (xfpm_backlight_parent_class)->finalize (object);
}

XfpmBacklight *
xfpm_backlight_new (void)
{
    XfpmBacklight *backlight = NULL;
    backlight = g_object_new (XFPM_TYPE_BACKLIGHT, NULL);
    return backlight;
}

gboolean xfpm_backlight_has_hw (XfpmBacklight *backlight)
{
    return backlight->priv->has_hw;
}
