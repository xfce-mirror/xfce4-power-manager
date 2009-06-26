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

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/xfpm-common.h"

#include "xfpm-brightness-widget.h"

static void xfpm_brightness_widget_finalize   (GObject *object);

#define XFPM_BRIGHTNESS_WIDGET_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_BRIGHTNESS_WIDGET, XfpmBrightnessWidgetPrivate))

#define WINDOW_HIDE_TIMEOUT	1.0f
#define BRIGHTNESS_POPUP_SIZE	180

struct XfpmBrightnessWidgetPrivate
{
    GtkWidget *window;
    GTimer    *timer;
    GdkPixbuf *pix;
    
    guint      level;
    guint      max_level;
    gboolean   timeout_added;
};

G_DEFINE_TYPE (XfpmBrightnessWidget, xfpm_brightness_widget, G_TYPE_OBJECT)

static gboolean
xfpm_brightness_widget_timeout (XfpmBrightnessWidget *widget)
{
    if ( g_timer_elapsed (widget->priv->timer, NULL) > WINDOW_HIDE_TIMEOUT )
    {
	gtk_widget_hide (widget->priv->window);
	widget->priv->timeout_added = FALSE;
	return FALSE;
    }
    return TRUE;
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
	    cairo_set_source_rgb (cr, 0.3, 0.3, 0.4);
	}
	else
	{
	    cairo_set_source_rgb (cr, 0.6, 1.0, 0.4);
	}
	cairo_rectangle (cr, (gdouble)i*width, 140, width - padding , 10);
	cairo_fill (cr);
	
	if ( i >= widget->priv->level )
	{
	    cairo_set_source_rgb (cr, 0.3, 0.3, 0.3);
	}
	else
	{
	    cairo_set_source_rgb (cr, 0.6, 1.0, 0.);
	}
	cairo_rectangle (cr, (gdouble)i*width, 150, width - padding , 10);
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

    gtk_widget_set_size_request (GTK_WIDGET (widget->priv->window), BRIGHTNESS_POPUP_SIZE, BRIGHTNESS_POPUP_SIZE);
    
    widget->priv->pix = xfpm_load_icon ("xfpm-brightness-lcd", 128);
    
    xfpm_brightness_widget_set_colormap (GTK_WIDGET (widget->priv->window));

    widget->priv->timer = g_timer_new ();
    
    g_signal_connect (widget->priv->window, "expose_event",
		      G_CALLBACK (xfpm_brightness_widget_expose_event), widget);
}

static void
xfpm_brightness_widget_finalize (GObject *object)
{
    XfpmBrightnessWidget *widget;

    widget = XFPM_BRIGHTNESS_WIDGET (object);
    
    g_timer_destroy (widget->priv->timer);
    if ( widget->priv->pix )
	gdk_pixbuf_unref (widget->priv->pix);

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

    gtk_window_present (GTK_WINDOW (widget->priv->window));
    gtk_widget_queue_draw (widget->priv->window);
    
    if ( widget->priv->timeout_added == FALSE )
    {
	g_timeout_add (100, (GSourceFunc) xfpm_brightness_widget_timeout, widget);
	widget->priv->timeout_added = TRUE;
    }
	
    g_timer_reset (widget->priv->timer);
}
