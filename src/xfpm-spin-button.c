/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * * Copyright (C) 2008 Ali <ali.slackware@gmail.com>
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

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <math.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "xfpm-spin-button.h"

#define MAX_DIGITS 20

/* init */
static void xfpm_spin_button_init      (XfpmSpinButton *spin_button);
static void xfpm_spin_button_class_init(XfpmSpinButtonClass *klass);
static void xfpm_spin_button_finalize  (GObject *object);

static void xfpm_spin_button_editable_init(GtkEditableClass *iface);
static void xfpm_spin_button_delete_text (GtkEditable *editable,
                                          gint         start_pos,
                                          gint         end_pos);
static void xfpm_spin_button_insert_text (GtkEditable *editable,
										  const gchar *new_text,
										  gint         new_text_length,
										  gint        *position);
			     
G_DEFINE_TYPE_WITH_CODE(XfpmSpinButton,xfpm_spin_button,GTK_TYPE_SPIN_BUTTON,
						G_IMPLEMENT_INTERFACE(GTK_TYPE_EDITABLE,
											  xfpm_spin_button_editable_init))

static void
xfpm_spin_button_size_request(GtkWidget *widget,GtkRequisition *req)
{
    GtkSpinButton *gtk_spin_button;
    XfpmSpinButton *xfpm_spin_button;
    
    gtk_spin_button = GTK_SPIN_BUTTON(widget);
    xfpm_spin_button = XFPM_SPIN_BUTTON(widget);
    
    GTK_WIDGET_CLASS(xfpm_spin_button_parent_class)->size_request(widget,req);
    
    PangoFontMetrics *metrics;
    PangoContext *context;
      
    gtk_widget_ensure_style (GTK_WIDGET(xfpm_spin_button));
    context = gtk_widget_get_pango_context (GTK_WIDGET(xfpm_spin_button));
    metrics = pango_context_get_metrics (context,
                           widget->style->font_desc,
                           pango_context_get_language (context));	

    gint char_width = pango_font_metrics_get_approximate_char_width (metrics);
    
    gint char_pixels = (char_width + PANGO_SCALE - 1) / PANGO_SCALE;
    
    req->width += char_pixels * xfpm_spin_button->suffix_length;
    pango_font_metrics_unref(metrics);
}

static void
xfpm_spin_button_class_init(XfpmSpinButtonClass *klass) 
{
    GObjectClass *object_class     = (GObjectClass *)klass;
    GtkWidgetClass *widget_class   = (GtkWidgetClass *) klass;
    
    widget_class->size_request = xfpm_spin_button_size_request;
    
    object_class->finalize   = xfpm_spin_button_finalize;
    
}

static void
xfpm_spin_button_init(XfpmSpinButton *spin_button)
{
    spin_button->suffix_length = 0;
    spin_button->suffix = NULL;
}

static void
xfpm_spin_button_finalize(GObject *object)
{
    XfpmSpinButton *spin;
    spin = XFPM_SPIN_BUTTON(object);
    
    if ( spin->suffix )
    {
        g_free(spin->suffix);
    }
    
    G_OBJECT_CLASS(xfpm_spin_button_parent_class)->finalize(object);
}

static void
xfpm_spin_button_editable_init(GtkEditableClass *iface)
{
	iface->insert_text = xfpm_spin_button_insert_text;
	iface->delete_text = xfpm_spin_button_delete_text;
}

static void
_spin_button_update(XfpmSpinButton *spin)
{
    GtkWidget *widget;
    
    widget = GTK_WIDGET(spin);
    
    if ( GTK_WIDGET_DRAWABLE(widget) )
    {
        gtk_widget_queue_resize_no_redraw(widget);
    }
}


static void
xfpm_spin_button_delete_all_text(GtkEditable *editable)
{
    GtkEditableClass *spin_iface = g_type_interface_peek (xfpm_spin_button_parent_class,
	                                                      GTK_TYPE_EDITABLE);
    GtkEditableClass *entry_iface = g_type_interface_peek_parent(spin_iface);
    
    gint start,end;
    
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(editable));
	start = 0;
	end = strlen(text);
	
    entry_iface->delete_text(editable,start,end);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(editable),gtk_spin_button_get_value(GTK_SPIN_BUTTON(editable)));
}

static void
xfpm_spin_button_delete_text (GtkEditable *editable,
                              gint         start_pos,
                              gint         end_pos)
{
    
    GtkEditableClass *spin_iface = g_type_interface_peek (xfpm_spin_button_parent_class,
	                                                      GTK_TYPE_EDITABLE);
    GtkEditableClass *entry_iface = g_type_interface_peek_parent(spin_iface);
    
    XfpmSpinButton *spin = XFPM_SPIN_BUTTON(editable);
    
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(editable));
    gint text_length = strlen(text);

    gint length;
    
    if ( spin->suffix )
    {
        length = text_length - spin->suffix_length ;
        if ( start_pos >= ABS(length) ) return;
        entry_iface->delete_text(editable,start_pos,ABS(length));
        _spin_button_update(spin);
    }
    else
    {
        entry_iface->delete_text(editable,start_pos,end_pos);
        _spin_button_update(spin);
    }
}                                   

static gboolean
_is_digits(const gchar *text,gint length)
{
    gint i;

    for ( i = 0 ; i < length ; i++)
    {
        if ( text[i] < 0x30 || text[i] > 0x39 )
        {
            return FALSE;
        }
    }
    return TRUE;
}

static void xfpm_spin_button_insert_text (GtkEditable *editable,
										  const gchar *new_text,
										  gint         new_text_length,
										  gint        *position)
{
    GtkEditableClass *spin_iface = g_type_interface_peek (xfpm_spin_button_parent_class,
	                                                      GTK_TYPE_EDITABLE);
    GtkEditableClass *entry_iface = g_type_interface_peek_parent(spin_iface);
    
    XfpmSpinButton *spin = XFPM_SPIN_BUTTON(editable);
    
    if ( !_is_digits(new_text,new_text_length) )
    {
        if ( spin->suffix && !strcmp(new_text,spin->suffix) )
        {
            entry_iface->insert_text(editable,spin->suffix,spin->suffix_length,position);
            _spin_button_update(spin);
        }
    }
    else 
    {
        if ( spin->suffix )
        {
            const gchar *text = gtk_entry_get_text(GTK_ENTRY(editable));
            gint text_length = strlen(text);
            gint length = text_length - spin->suffix_length ;
            if ( *position > ABS(length) ) return;
            entry_iface->insert_text(editable,new_text,new_text_length,position);
            _spin_button_update(spin);
        }
        else
        {
            entry_iface->insert_text(editable,new_text,new_text_length,position);
            _spin_button_update(spin);
        }
    }
}										  

/* Constructor for the xfpm-spin button is the same as 
 * gtk_spin_button_new_with_range but slightly modified
 * 
 * GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkSpinButton widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
 *
 */
  
GtkWidget *
xfpm_spin_button_new_with_range(gdouble min,gdouble max,gdouble step)
{
    GtkObject *adj;
    XfpmSpinButton *spin;
    
	spin = g_object_new(XFPM_TYPE_SPIN_BUTTON,NULL);

    gint digits;

    g_return_val_if_fail (min <= max, NULL);
    g_return_val_if_fail (step != 0.0, NULL);

    adj = gtk_adjustment_new (min, min, max, step, 10 * step, 0);

    if (fabs (step) >= 1.0 || step == 0.0)
        digits = 0;
    else {
        digits = abs ((gint) floor (log10 (fabs (step))));
        if (digits > MAX_DIGITS)
            digits = MAX_DIGITS;
    }

    gtk_spin_button_configure (GTK_SPIN_BUTTON(spin), GTK_ADJUSTMENT (adj), step, digits);

    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON(spin), TRUE);
    
	return GTK_WIDGET(spin);
}

void           
xfpm_spin_button_set_suffix (XfpmSpinButton *spin,
							 const gchar *suffix)
{
	g_return_if_fail(XFPM_IS_SPIN_BUTTON(spin));
	g_return_if_fail(suffix != NULL);

    xfpm_spin_button_delete_all_text(GTK_EDITABLE(spin));	
    
    if ( spin->suffix ) g_free(spin->suffix);
    
    spin->suffix = g_strdup(suffix);
	spin->suffix_length = strlen(spin->suffix);
	
	gint spin_value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	gint digits_num = 1 ;
	if ( spin_value != 0 )
	digits_num = abs ((gint) floor (log10 (fabs (spin_value)))) + 1;
	
	xfpm_spin_button_insert_text(GTK_EDITABLE(spin),spin->suffix,spin->suffix_length,&(digits_num));
}
