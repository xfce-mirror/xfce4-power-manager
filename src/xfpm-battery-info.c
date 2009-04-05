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

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfpm-battery-info.h"

static GtkWidget *
xfpm_battery_info (HalBattery *device)
{
    PangoFontDescription *pfd;
    
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *align;
    
    gint i = 0;

    gchar   *unit = NULL;
    guint32  last_full = 0;
    guint32  design_capacity = 0;
    gchar   *tech = NULL;
    gchar   *vendor = NULL;
    gchar   *model = NULL;

    g_object_get (G_OBJECT(device), 
    		  "unit", &unit,
		  "reporting-last-full", &last_full, 
		  "technology", &tech, 
		  "vendor", &vendor,
		  "model", &model,
		  "reporting-design", &design_capacity,
		  NULL);
		  
    pfd = pango_font_description_from_string("bold");
    
    table = gtk_table_new (4, 2, FALSE);
    
    if (!unit)
    	unit = g_strdup (_("Unknown unit"));
    
    //Technology
    if ( tech )
    {
    	align = gtk_alignment_new (0.0, 0.5, 0, 0);
    
    	label = gtk_label_new (_("Technology:"));
    	gtk_container_add (GTK_CONTAINER(align), label);
    	gtk_widget_modify_font (label, pfd);
    
    	gtk_table_attach(GTK_TABLE(table), align,
			 0, 1, i, i+1, 
		     	 GTK_FILL, GTK_FILL,
		     	 2, 8);

	label = gtk_label_new (tech);
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
	g_free (tech);
	gtk_table_attach(GTK_TABLE(table), align,
			 1, 2, i, i+1, 
			 GTK_FILL, GTK_FILL,
		     	 2, 8);
    	i++;
    }

    /// Capacity design
    if ( design_capacity != 0 )
    {
	label = gtk_label_new (_("Design:"));
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
	
	gtk_widget_modify_font (label, pfd);
	
	gtk_table_attach(GTK_TABLE(table), align,
			 0, 1, i, i+1, 
			 GTK_FILL, GTK_FILL,
			 2, 8);
			 
	gchar *str = g_strdup_printf ("%d %s", design_capacity, unit);
	
	label = gtk_label_new (str);
	g_free (str);
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
	
	gtk_table_attach(GTK_TABLE(table), align,
			 1, 2, i, i+1, 
			 GTK_FILL, GTK_FILL,
			 2, 8);
	i++;
    }
    
    
    if ( last_full != 0 )
    {
    	label = gtk_label_new (_("Last full:"));
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
    	gtk_widget_modify_font (label, pfd);
    
    	gtk_table_attach(GTK_TABLE(table), align,
			 0, 1, i, i+1, 
			 GTK_FILL, GTK_FILL,
		     	 2, 8);
			 
	gchar *str = g_strdup_printf ("%d %s", last_full, unit);
	label = gtk_label_new (str);
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
	g_free (str);
	gtk_table_attach(GTK_TABLE(table), align,
			 1, 2, i, i+1, 
			 GTK_FILL, GTK_FILL,
		     	 2, 8);
    	i++;
    }
    
    if ( vendor )
    {
    	label = gtk_label_new (_("Vendor:"));
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
	gtk_widget_modify_font (label, pfd);
	
	gtk_table_attach(GTK_TABLE(table), align,
			 0, 1, i, i+1, 
			 GTK_FILL, GTK_FILL,
			 2, 8);
	
	label = gtk_label_new (vendor);
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
	g_free (vendor);
	gtk_table_attach(GTK_TABLE(table), align,
			 1, 2, i, i+1, 
			 GTK_FILL, GTK_FILL,
		     	 2, 8);
	i++;
    }
    
     
    if ( model )
    {
    	label = gtk_label_new (_("Model:"));
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
	gtk_widget_modify_font (label, pfd);
	
	gtk_table_attach(GTK_TABLE(table), align,
			 0, 1, i, i+1, 
			 GTK_FILL, GTK_FILL,
			 2, 8);
	
	label = gtk_label_new (model);
	align = gtk_alignment_new (0.0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER(align), label);
	g_free (model);
	gtk_table_attach(GTK_TABLE(table), align,
			 1, 2, i, i+1, 
			 GTK_FILL, GTK_FILL,
		     	 2, 8);
	i++;
    }
    
    if ( unit )
    	g_free(unit);
	
    return table;
}

GtkWidget *xfpm_battery_info_new (HalBattery *device, const gchar *icon_name)
{
    GtkWidget *info;
    GtkWidget *mainbox;
    GtkWidget *allbox;
    
    info = xfce_titled_dialog_new_with_buttons(_("Battery information"),
					       NULL,
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_STOCK_CLOSE,
                                               GTK_RESPONSE_CANCEL,
					       NULL);
					       
    gtk_window_set_icon_name (GTK_WINDOW(info), icon_name);
    gtk_dialog_set_default_response (GTK_DIALOG(info), GTK_RESPONSE_CLOSE);
    
    mainbox = GTK_DIALOG (info)->vbox;
    
    allbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(mainbox), allbox, TRUE, TRUE, 0);
    
    gtk_box_pack_start (GTK_BOX (allbox), xfpm_battery_info(device), FALSE, FALSE, 8);
    
    g_signal_connect (info, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    
    return info;
}
