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

#define WINDOW_HIDE_TIMEOUT	1.f

struct XfpmBrightnessWidgetPrivate
{
    GtkWidget *window;
    GTimer    *timer;
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
	gtk_widget_hide_all (widget->priv->window);
	widget->priv->timeout_added = FALSE;
	return FALSE;
    }
    return TRUE;
}

static gboolean
xfpm_brightness_widget_expose_event_cb (GtkWidget *w, GdkEventExpose *ev, XfpmBrightnessWidget *widget)
{
    cairo_t *cr;
    gdouble x, y;
    gdouble arc;
    
    g_return_val_if_fail ( widget->priv->max_level != 0, FALSE );
    
    cr = gdk_cairo_create (widget->priv->window->window);
    
    cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.0f);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    
    //cairo_set_source_rgb (cr, 0.9f, 0.9f, 0.9f);
    cairo_paint (cr);
    
    x = widget->priv->window->allocation.width  / 2;
    y = widget->priv->window->allocation.height / 2;
    
    cairo_set_line_width (cr, 0);
    
    cairo_move_to (cr, x, y);
    
    arc = ( 2 * M_PI * (gdouble)widget->priv->level ) / (gdouble)widget->priv->max_level;
    
    cairo_arc (cr, x, y, x, 0, arc);
    
    cairo_set_source_rgb (cr, 0.9f, 0.8f, 0.0f);
    
    cairo_fill_preserve (cr);
    cairo_stroke (cr);

    cairo_destroy (cr);
    
    return FALSE;
}

static void
xfpm_brightness_widget_set_colormap ( GtkWidget *widget)
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
    GdkPixbuf *pix;
    GtkWidget *image;
    
    widget->priv = XFPM_BRIGHTNESS_WIDGET_GET_PRIVATE (widget);

    widget->priv->level  = 0;
    widget->priv->max_level = 0;
    widget->priv->window = gtk_window_new(GTK_WINDOW_POPUP);

    gtk_window_set_decorated (GTK_WINDOW (widget->priv->window), FALSE);
    gtk_window_set_resizable (GTK_WINDOW (widget->priv->window), TRUE);
    gtk_window_set_position (GTK_WINDOW (widget->priv->window), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_widget_set_app_paintable (widget->priv->window, TRUE);
    gtk_widget_set_size_request (widget->priv->window, 148, 148);
    gtk_window_set_type_hint (GTK_WINDOW (widget->priv->window), GDK_WINDOW_TYPE_HINT_UTILITY);
    
    pix = xfpm_load_icon ("gpm-brightness-lcd", 128 );
    if ( pix )
    {
	image = gtk_image_new_from_pixbuf (pix);
	g_object_unref (pix);
	gtk_container_add (GTK_CONTAINER (widget->priv->window), image);
    }
    
    g_signal_connect (widget->priv->window, "expose-event",
		      G_CALLBACK (xfpm_brightness_widget_expose_event_cb), widget);
    
    xfpm_brightness_widget_set_colormap (widget->priv->window);
    
    widget->priv->timer = g_timer_new ();
}

static void
xfpm_brightness_widget_finalize (GObject *object)
{
    XfpmBrightnessWidget *widget;

    widget = XFPM_BRIGHTNESS_WIDGET (object);
    
    gtk_widget_destroy (widget->priv->window);

    g_timer_destroy (widget->priv->timer);

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
    
    gtk_widget_queue_draw (widget->priv->window);
    gtk_widget_show_all (widget->priv->window);
    
    if ( widget->priv->timeout_added == FALSE )
    {
	g_timeout_add (100, (GSourceFunc) xfpm_brightness_widget_timeout, widget);
	widget->priv->timeout_added = TRUE;
    }
	
    g_timer_reset (widget->priv->timer);
}
