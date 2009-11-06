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
#include <libxfce4ui/libxfce4ui.h>

#include "xfpm-battery-info.h"
#include "xfpm-dkp.h"

static const gchar *
xfpm_battery_info_get_device_tech (GHashTable *props)
{
    GValue *value;
    gint tech;
    
    value = g_hash_table_lookup (props, "Technology:");
    
    if ( !value )
	return NULL;
	
    tech = g_value_get_uint (value);
    
    switch (tech )
    {
	case 0:
	    return _("Unknown");
	case 1:
	    return _("Lithium ion");
	case 2:
	    return _("Lithium polymer");
	case 3:
	    return _("Lithium iron phosphate");
	case 4:
	    return _("Lead acid");
	case 5:
	    return _("Nickel cadmium");
	case 6:
	    return _("Nickel metal hybride");
	default:
	    g_warn_if_reached ();
    }
    
    return NULL;
}    

static const gchar *
xfpm_battery_info_get_device_prop_string (GHashTable *props, const gchar *prop)
{
    GValue *value;
    
    value = g_hash_table_lookup (props, prop);
    
    if ( !value )
	return NULL;
	
    return g_value_get_string (value);
}

static const gchar *
xfpm_battery_info_get_device_prop_bool (GHashTable *props, const gchar *prop)
{
    GValue *value;
    gboolean ret;
    
    value = g_hash_table_lookup (props, prop);
    
    if ( !value )
	return NULL;
	
    ret = g_value_get_boolean (value);
	
    return ret ? _("Yes") : _("Non");
}

static const gchar *
xfpm_battery_info_get_device_type (GHashTable *props)
{
    GValue *value;
    guint type;
    
    value = g_hash_table_lookup (props, "Type");
    
    if ( !value )
	return NULL;
	
    type = g_value_get_uint (value);
    switch (type)
    {
	case XFPM_DKP_DEVICE_TYPE_BATTERY:
	    return _("Battery");
	case XFPM_DKP_DEVICE_TYPE_UPS:
	    return _("UPS");
	case XFPM_DKP_DEVICE_TYPE_LINE_POWER:
	    return _("Line power");
	case XFPM_DKP_DEVICE_TYPE_MOUSE:
	    return _("Mouse");
	case XFPM_DKP_DEVICE_TYPE_KBD:
	    return _("Keyboard");
    }
    
    return NULL;
}

static gchar *
xfpm_battery_info_get_energy_property (GHashTable *props, const gchar *prop, const gchar *unit)
{
    GValue *value;
    gchar *val = NULL;
    gdouble energy;
    
    value = g_hash_table_lookup (props, prop);
    
    if ( !value )
	return NULL;
	
    energy = g_value_get_double (value);
    
    val = g_strdup_printf ("%.1f %s", energy, unit);
    
    return val;
}

static void
xfpm_battery_info_add (GtkWidget *table,
		       PangoFontDescription *pfd, 
		       const gchar *name, 
		       const gchar *value, 
		       gint i)
{
    GtkWidget *label, *align;
    align = gtk_alignment_new (0.0, 0.5, 0, 0);

    label = gtk_label_new (name);
    gtk_container_add (GTK_CONTAINER(align), label);
    gtk_widget_modify_font (label, pfd);

    gtk_table_attach (GTK_TABLE(table), align,
		      0, 1, i, i+1, 
		      GTK_FILL, GTK_FILL,
		      2, 2);

    label = gtk_label_new (value);
    align = gtk_alignment_new (0.0, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER(align), label);
    gtk_table_attach (GTK_TABLE(table), align,
		      1, 2, i, i+1, 
		      GTK_FILL, GTK_FILL,
		      2, 2);
}

static GtkWidget *
xfpm_battery_info (GHashTable *props)
{
    PangoFontDescription *pfd;
    
    GtkWidget *table;
    
    gint i = 0;
    const gchar *cstr;
    gchar *str = NULL;
    pfd = pango_font_description_from_string("bold");
    
    table = gtk_table_new (4, 2, FALSE);
    
    cstr = xfpm_battery_info_get_device_type (props);
    
    if ( cstr )
    {
	xfpm_battery_info_add (table, pfd, _("Type:"), cstr, i);
	i++;
    }
    
    
    cstr = xfpm_battery_info_get_device_prop_string (props, "Model");
    
    if ( cstr )
    {
	xfpm_battery_info_add (table, pfd, _("Model:"), cstr, i);
	i++;
    }
    
    cstr = xfpm_battery_info_get_device_tech (props);
    if ( cstr )
    {
	xfpm_battery_info_add (table, pfd, _("Technology:"), cstr, i);
	i++;
    }

    /* TRANSLATORS: Unit here is What hour*/
    str = xfpm_battery_info_get_energy_property (props, "EnergyFullDesign", _("Wh"));
    
    if ( str )
    {
	xfpm_battery_info_add (table, pfd, _("Energy full design:"), str, i);
	i++;
	g_free (str);
    }
    
    /* TRANSLATORS: Unit here is What hour*/
    str = xfpm_battery_info_get_energy_property (props, "EnergyFull", _("Wh"));
    
    if ( str )
    {
	xfpm_battery_info_add (table, pfd, _("Energy full:"), str, i);
	i++;
	g_free (str);
    }
    
    /* TRANSLATORS: Unit here is What hour*/
    str = xfpm_battery_info_get_energy_property (props, "EnergyEmpty", _("Wh"));
    
    if ( str )
    {
	xfpm_battery_info_add (table, pfd, _("Energy empty:"), str, i);
	i++;
	g_free (str);
    }
    
    /* TRANSLATORS: Unit here is volt*/
    str = xfpm_battery_info_get_energy_property (props, "Voltage", _("V"));
    if ( str )
    {
	xfpm_battery_info_add (table, pfd, _("Voltage:"), str, i);
	i++;
	g_free (str);
    }
    
    cstr = xfpm_battery_info_get_device_prop_bool (props, "IsRechargeable");
    if ( cstr )
    {
	xfpm_battery_info_add (table, pfd, _("Rechargeable:"), cstr, i);
	i++;
    }
    
    cstr = xfpm_battery_info_get_device_prop_string (props, "Vendor");
    if ( cstr )
    {
	xfpm_battery_info_add (table, pfd, _("Vendor:"), cstr, i);
	i++;
    }
    
    cstr = xfpm_battery_info_get_device_prop_string (props, "Serial");
    if ( cstr )
    {
	xfpm_battery_info_add (table, pfd, _("Serial number:"), cstr, i);
	i++;
    }
    
    return table;
}

void xfpm_battery_info_show (GHashTable *props, const gchar *icon_name)
{
    GtkWidget *dialog;
    GtkWidget *mainbox;
    GtkWidget *allbox;
    GtkWidget *info;
    
    dialog = xfce_titled_dialog_new_with_buttons (_("Battery information"),
	 				          NULL,
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CLOSE,
                                                  GTK_RESPONSE_CANCEL,
					          NULL);
					       
    gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
    
    mainbox = GTK_DIALOG (dialog)->vbox;
    
    allbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(mainbox), allbox, TRUE, TRUE, 0);
    
    info = xfpm_battery_info (props);
    
    gtk_box_pack_start (GTK_BOX (allbox), info, FALSE, FALSE, 8);
    
    g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
    
    gtk_widget_show_all (dialog);
}
