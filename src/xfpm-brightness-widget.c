/* * 
 *  Copyright (C) 2009 Ali <aliov@xfce.org>
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
#include <glib.h>
#include <cairo.h>

#include <libnotify/notify.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/xfpm-common.h"

#include "xfpm-brightness-widget.h"
#include "xfpm-dbus-monitor.h"

static void xfpm_brightness_widget_finalize   (GObject *object);

#define XFPM_BRIGHTNESS_WIDGET_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_BRIGHTNESS_WIDGET, XfpmBrightnessWidgetPrivate))

#define BRIGHTNESS_POPUP_SIZE	180

struct XfpmBrightnessWidgetPrivate
{
    XfpmDBusMonitor     *monitor;
    GtkWidget 		*window;
    GdkPixbuf 		*pix;
    
    guint      		 level;
    guint      		 max_level;
    gulong     		 timeout_id;
    
    gboolean		 check_server_caps;
    gboolean		 notify_osd;
    NotifyNotification	*n;
    
    gulong		 sig_1;
};

G_DEFINE_TYPE (XfpmBrightnessWidget, xfpm_brightness_widget, G_TYPE_OBJECT)

static void
xfpm_brightness_widget_service_connection_changed_cb (XfpmDBusMonitor *monitor, gchar *service_name, 
						      gboolean connected, gboolean on_session,
						      XfpmBrightnessWidget *widget)
{
    if ( !g_strcmp0 (service_name, "org.freedesktop.Notifications")  && on_session )
    {
	if ( connected )
	    widget->priv->check_server_caps = TRUE;
	else
	    widget->priv->notify_osd = FALSE;
    }
}

static gboolean
xfpm_brightness_widget_server_is_notify_osd (void)
{
    gboolean supports_sync = FALSE;
    GList *caps = NULL;

    caps = notify_get_server_caps ();
    if (caps != NULL) 
    {
	if (g_list_find_custom (caps, "x-canonical-private-synchronous", (GCompareFunc) g_strcmp0) != NULL)
	    supports_sync = TRUE;

	    g_list_foreach(caps, (GFunc)g_free, NULL);
	    g_list_free(caps);
    }

    return supports_sync;
}

static void
xfpm_brightness_widget_display_notification (XfpmBrightnessWidget *widget)
{
    guint i;
    gfloat value = 0;
    
    static const char *display_icon_name[] = 
    {
	"notification-display-brightness-off",
	"notification-display-brightness-low",
	"notification-display-brightness-medium",
	"notification-display-brightness-high",
	"notification-display-brightness-full",
	NULL
    };
    
    value = (gfloat) 100 * widget->priv->level / widget->priv->max_level;
    
    i = (gint)value / 25;
    
    notify_notification_set_hint_int32 (widget->priv->n,
					"value",
					 value);
    
    notify_notification_set_hint_string (widget->priv->n,
					 "x-canonical-private-synchronous",
					 "brightness");
					 
    notify_notification_update (widget->priv->n,
			        " ",
				"",
				display_icon_name[i]);
				
    notify_notification_show (widget->priv->n, NULL);
}

static gboolean
xfpm_brightness_widget_timeout (XfpmBrightnessWidget *widget)
{
    gtk_widget_hide (widget->priv->window);
    return FALSE;
}

static gboolean
xfpm_brightness_widget_expose_event (GtkWidget *w, GdkEventExpose *ev, XfpmBrightnessWidget *widget)
{
    cairo_t *cr;
    gdouble width;
    gdouble padding;
    guint i;

    g_return_val_if_fail (widget->priv->max_level != 0, FALSE );

    cr = gdk_cairo_create (widget->priv->window->window);

    cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.0f);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr); 
    
    gdk_cairo_set_source_pixbuf (cr, widget->priv->pix,  (180 - 128)/2, 0);
    cairo_paint (cr);
    
    width = (gdouble)  (180-20) / widget->priv->max_level;
    padding = width /10;
    cairo_translate (cr, 10, 0);
    
    for ( i = 0; i < widget->priv->max_level; i++) 
    {
	if ( i >= widget->priv->level )
	{
	    cairo_set_source_rgb (cr, 0., 0., 0.);
	}
	else
	{
	    cairo_set_source_rgb (cr, 1., 1.0, 0.0);
	}
	cairo_rectangle (cr, (gdouble)i*width, 130, width - padding , 20);
	cairo_fill (cr);
    }

    cairo_destroy (cr);
    return TRUE;
}

static void
xfpm_brightness_widget_set_colormap (GtkWidget *widget)
{
    GdkScreen *screen = gtk_widget_get_screen (widget);
    GdkColormap* colmap = gdk_screen_get_rgba_colormap (screen);
  
    if (!colmap)
	colmap = gdk_screen_get_rgb_colormap (screen);
  
    gtk_widget_set_colormap (widget, colmap);
}

static void
xfpm_brightness_widget_class_init (XfpmBrightnessWidgetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_brightness_widget_finalize;
    
    g_type_class_add_private (klass, sizeof (XfpmBrightnessWidgetPrivate));
}

static void
xfpm_brightness_widget_init (XfpmBrightnessWidget *widget)
{
    widget->priv = XFPM_BRIGHTNESS_WIDGET_GET_PRIVATE (widget);
    widget->priv->monitor = xfpm_dbus_monitor_new ();
    
    xfpm_dbus_monitor_add_service (widget->priv->monitor, 
				   DBUS_BUS_SESSION,
				   "org.freedesktop.Notifications");
    
    widget->priv->sig_1 = g_signal_connect (widget->priv->monitor, "service-connection-changed",
					    G_CALLBACK (xfpm_brightness_widget_service_connection_changed_cb),
					    widget);
    
    widget->priv->window = gtk_window_new (GTK_WINDOW_POPUP);

    g_object_set (G_OBJECT (widget->priv->window), 
		  "window-position", GTK_WIN_POS_CENTER_ALWAYS,
		  "decorated", FALSE,
		  "resizable", FALSE,
		  "type-hint", GDK_WINDOW_TYPE_HINT_UTILITY,
		  "app-paintable", TRUE,
		  NULL);

    
    widget->priv->level  = 0;
    widget->priv->max_level = 0;
    widget->priv->timeout_id = 0;
    widget->priv->notify_osd = FALSE;
    widget->priv->check_server_caps = TRUE;

    gtk_widget_set_size_request (GTK_WIDGET (widget->priv->window), BRIGHTNESS_POPUP_SIZE, BRIGHTNESS_POPUP_SIZE);
    
    widget->priv->pix = xfpm_load_icon ("xfpm-brightness-lcd", 128);
    
    xfpm_brightness_widget_set_colormap (GTK_WIDGET (widget->priv->window));

    g_signal_connect (widget->priv->window, "expose_event",
		      G_CALLBACK (xfpm_brightness_widget_expose_event), widget);
		      
    widget->priv->n = notify_notification_new (" ",
					       "",
					       NULL,
					       NULL);
}

static void
xfpm_brightness_widget_finalize (GObject *object)
{
    XfpmBrightnessWidget *widget;

    widget = XFPM_BRIGHTNESS_WIDGET (object);
    
    if ( widget->priv->pix )
	gdk_pixbuf_unref (widget->priv->pix);
	
    if ( g_signal_handler_is_connected (G_OBJECT (widget->priv->monitor), widget->priv->sig_1) )
	g_signal_handler_disconnect (G_OBJECT (widget->priv->monitor), widget->priv->sig_1);
	
    g_object_unref (widget->priv->n);
    g_object_unref (widget->priv->monitor);

    G_OBJECT_CLASS (xfpm_brightness_widget_parent_class)->finalize (object);
}

XfpmBrightnessWidget *
xfpm_brightness_widget_new (void)
{
    XfpmBrightnessWidget *widget = NULL;
    widget = g_object_new (XFPM_TYPE_BRIGHTNESS_WIDGET, NULL);

    return widget;
}

void xfpm_brightness_widget_set_max_level (XfpmBrightnessWidget *widget, guint level)
{
    g_return_if_fail (XFPM_IS_BRIGHTNESS_WIDGET (widget));

    widget->priv->max_level = level;
}

void xfpm_brightness_widget_set_level (XfpmBrightnessWidget *widget, guint level)
{
    g_return_if_fail (XFPM_IS_BRIGHTNESS_WIDGET (widget));
    
    widget->priv->level = level;

    if ( widget->priv->check_server_caps )
    {
	widget->priv->notify_osd = xfpm_brightness_widget_server_is_notify_osd ();
	widget->priv->check_server_caps = FALSE;
    }
	
    if ( widget->priv->notify_osd )
    {
	xfpm_brightness_widget_display_notification (widget);
    }
    else
    {
	gtk_window_present (GTK_WINDOW (widget->priv->window));
	
	if ( widget->priv->timeout_id != 0 )
	    g_source_remove (widget->priv->timeout_id);
	    
	widget->priv->timeout_id = 
	    g_timeout_add (900, (GSourceFunc) xfpm_brightness_widget_timeout, widget);
    }
}
