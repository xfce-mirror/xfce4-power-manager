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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "xfpm-backlight.h"
#include "egg-idletime.h"
#include "xfpm-notify.h"
#include "xfpm-xfconf.h"
#include "xfpm-dkp.h"
#include "xfpm-config.h"
#include "xfpm-button.h"
#include "xfpm-brightness.h"
#include "xfpm-debug.h"
#include "xfpm-icons.h"

static void xfpm_backlight_finalize   (GObject *object);

#define ALARM_DISABLED 9
#define BRIGHTNESS_POPUP_SIZE	180

#define XFPM_BACKLIGHT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_BACKLIGHT, XfpmBacklightPrivate))

struct XfpmBacklightPrivate
{
    XfpmBrightness *brightness;
    XfpmDkp        *dkp;
    EggIdletime    *idle;
    XfpmXfconf     *conf;
    XfpmButton     *button;
    XfpmNotify     *notify;
    
    GtkWidget 	   *window;
    GtkWidget      *progress_bar;
    NotifyNotification *n;
    
    
    gulong     	    timeout_id;
    gulong	    destroy_id;
    
    gboolean	    has_hw;
    gboolean	    on_battery;
    
    gint            last_level;
    gint 	    max_level;
    
    gboolean        dimmed;
    gboolean	    block;
#ifdef WITH_HAL
    gboolean	    brightness_in_hw;
#endif
};

G_DEFINE_TYPE (XfpmBacklight, xfpm_backlight, G_TYPE_OBJECT)

static void
xfpm_backlight_dim_brightness (XfpmBacklight *backlight)
{
    gboolean ret;
    
    ret = xfpm_brightness_get_level (backlight->priv->brightness, &backlight->priv->last_level);
    
    if ( !ret )
    {
	g_warning ("Unable to get current brightness level");
	return;
    }
    XFPM_DEBUG ("Current brightness level before dimming : %u", backlight->priv->last_level);
    
    backlight->priv->dimmed = xfpm_brightness_dim_down (backlight->priv->brightness);
}

static gboolean
xfpm_backlight_destroy_popup (gpointer data)
{
    XfpmBacklight *backlight;
    
    backlight = XFPM_BACKLIGHT (data);
    
    if ( backlight->priv->window )
    {
	gtk_widget_destroy (backlight->priv->window);
	backlight->priv->window = NULL;
    }
    
    if ( backlight->priv->n )
    {
	g_object_unref (backlight->priv->n);
	backlight->priv->n = NULL;
    }
    
    return FALSE;
}

static void
xfpm_backlight_show_notification (XfpmBacklight *backlight, gint level, gint max_level)
{
    gint i;
    gfloat value = 0;
    NotifyNotification *n;
    
    static const char *display_icon_name[] = 
    {
	"notification-display-brightness-off",
	"notification-display-brightness-low",
	"notification-display-brightness-medium",
	"notification-display-brightness-high",
	"notification-display-brightness-full",
	NULL
    };
    
    if ( !backlight->priv->n )
    {
	n = xfpm_notify_new_notification (backlight->priv->notify, 
					  NULL, 
					  NULL, 
					  NULL, 
					  0, 
					  XFPM_NOTIFY_NORMAL,
					  NULL);
    }
    
    value = (gfloat) 100 * level / max_level;
    
    i = (gint)value / 25;
    
    notify_notification_set_hint_int32  (n,
					 "value",
					 value);
    
    notify_notification_set_hint_string (n,
					 "x-canonical-private-synchronous",
					 "brightness");
					 
    notify_notification_update (n,
			        " ",
				"",
				display_icon_name[i]);
				
    notify_notification_show (n, NULL);
    backlight->priv->n = n;
}

static gboolean
xfpm_backlight_hide_popup_timeout (XfpmBacklight *backlight)
{
    gtk_widget_hide (backlight->priv->window);
    return FALSE;
}

static void
xfpm_backlight_create_popup (XfpmBacklight *backlight)
{
    GtkWidget *vbox;
    GtkWidget *img;
    GtkWidget *align;
    GtkObject *adj;
    
    if ( backlight->priv->window != NULL )
	return;
	
    backlight->priv->window = gtk_window_new (GTK_WINDOW_POPUP);

    g_object_set (G_OBJECT (backlight->priv->window), 
		  "window-position", GTK_WIN_POS_CENTER_ALWAYS,
		  "decorated", FALSE,
		  "resizable", FALSE,
		  "type-hint", GDK_WINDOW_TYPE_HINT_UTILITY,
		  "app-paintable", TRUE,
		  NULL);

    gtk_window_set_default_size (GTK_WINDOW (backlight->priv->window), BRIGHTNESS_POPUP_SIZE, BRIGHTNESS_POPUP_SIZE);
    
    align = gtk_alignment_new (0., 0.5, 0, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 5, 5, 5, 5);
    
    vbox = gtk_vbox_new (FALSE, 0);
    
    gtk_container_add (GTK_CONTAINER (backlight->priv->window), align);
    gtk_container_add (GTK_CONTAINER (align), vbox);
    
    img = gtk_image_new_from_icon_name (XFPM_DISPLAY_BRIGHTNESS_ICON, GTK_ICON_SIZE_DIALOG);
    
    gtk_box_pack_start (GTK_BOX (vbox), img, TRUE, TRUE, 0);
    
    backlight->priv->progress_bar = gtk_progress_bar_new ();
    
    adj = gtk_adjustment_new (0., 0., backlight->priv->max_level, 1., 0., 0.);

    g_object_set (G_OBJECT (backlight->priv->progress_bar),
		  "adjustment", adj,
		  NULL);
    
    gtk_box_pack_start (GTK_BOX (vbox), backlight->priv->progress_bar, TRUE, TRUE, 0);
    
    gtk_widget_show_all (align);
}

static void
xfpm_backlight_show (XfpmBacklight *backlight, gint level)
{
    gboolean sync;
    gboolean show_popup;
    
    XFPM_DEBUG ("Level %u", level);
    
    
    g_object_get (G_OBJECT (backlight->priv->conf),
                  SHOW_BRIGHTNESS_POPUP, &show_popup,
                  NULL);
		  
    if ( !show_popup )
	goto out;
    
    g_object_get (G_OBJECT (backlight->priv->notify),
		  "sync", &sync,
		  NULL);
    
    if ( sync )
    {
	xfpm_backlight_show_notification (backlight, level, backlight->priv->max_level);
    }
    else
    {
	GtkAdjustment *adj;
	
	xfpm_backlight_create_popup (backlight);
	g_object_get (G_OBJECT (backlight->priv->progress_bar),
		      "adjustment", &adj,
		      NULL);
	
	gtk_adjustment_set_value (adj, level);
	
	if ( !GTK_WIDGET_VISIBLE (backlight->priv->window))
	    gtk_window_present (GTK_WINDOW (backlight->priv->window));
	
	if ( backlight->priv->timeout_id != 0 )
	    g_source_remove (backlight->priv->timeout_id);
	    
	backlight->priv->timeout_id = 
	    g_timeout_add (900, (GSourceFunc) xfpm_backlight_hide_popup_timeout, backlight);
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
#ifdef WITH_HAL
	if ( !backlight->priv->brightness_in_hw && enable_brightness)
	    ret = xfpm_brightness_up (backlight->priv->brightness, &level);
#else
	if ( enable_brightness )
	    ret = xfpm_brightness_up (backlight->priv->brightness, &level);
#endif
	if ( ret && show_popup)
	    xfpm_backlight_show (backlight, level);
    }
    else if ( type == BUTTON_MON_BRIGHTNESS_DOWN )
    {
	backlight->priv->block = TRUE;
#ifdef WITH_HAL
	if ( !backlight->priv->brightness_in_hw && enable_brightness )
	    ret = xfpm_brightness_down (backlight->priv->brightness, &level);
#else
	if ( enable_brightness )
	    ret = xfpm_brightness_down (backlight->priv->brightness, &level);
#endif
	    
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
xfpm_backlight_on_battery_changed_cb (XfpmDkp *dkp, gboolean on_battery, XfpmBacklight *backlight)
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
    
    backlight->priv->notify = NULL;
    backlight->priv->idle   = NULL;
    backlight->priv->conf   = NULL;
    backlight->priv->button = NULL;
    backlight->priv->dkp    = NULL;
    backlight->priv->window = NULL;
    backlight->priv->progress_bar = NULL;
    backlight->priv->dimmed = FALSE;
    backlight->priv->block = FALSE;
    backlight->priv->destroy_id = 0;
    backlight->priv->timeout_id = 0;
    
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
	backlight->priv->dkp    = xfpm_dkp_get ();
	backlight->priv->notify = xfpm_notify_new ();
	backlight->priv->max_level = xfpm_brightness_get_max_level (backlight->priv->brightness);
#ifdef WITH_HAL
	if ( xfpm_brightness_get_control (backlight->priv->brightness) == XFPM_BRIGHTNESS_CONTROL_HAL )
	    backlight->priv->brightness_in_hw = xfpm_brightness_in_hw (backlight->priv->brightness);
#endif
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
				
	g_signal_connect (backlight->priv->dkp, "on-battery-changed",
			  G_CALLBACK (xfpm_backlight_on_battery_changed_cb), backlight);
	g_object_get (G_OBJECT (backlight->priv->dkp),
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

    if ( backlight->priv->dkp )
	g_object_unref (backlight->priv->dkp);

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
