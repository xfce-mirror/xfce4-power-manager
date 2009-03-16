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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/hal-iface.h"
#include "libxfpm/xfpm-string.h"

#include "xfpm-cpu.h"
#include "xfpm-adapter.h"
#include "xfpm-xfconf.h"
#include "xfpm-config.h"
#include "xfpm-enum.h"

/* Init */
static void xfpm_cpu_class_init (XfpmCpuClass *klass);
static void xfpm_cpu_init       (XfpmCpu *cpu);
static void xfpm_cpu_finalize   (GObject *object);

#define XFPM_CPU_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_CPU, XfpmCpuPrivate))

struct XfpmCpuPrivate
{
    XfpmAdapter   *adapter;
    XfpmXfconf    *conf;
    HalIface      *iface;
    
    gboolean 	   on_battery;
    gboolean 	   power_save;
    
    guint8         cpu_governors;
};

G_DEFINE_TYPE(XfpmCpu, xfpm_cpu, G_TYPE_OBJECT)

static void
xfpm_cpu_set_governor (XfpmCpu *cpu, const gchar *governor)
{
    GError *error = NULL;
    
    if (! hal_iface_set_cpu_governor(cpu->priv->iface, governor, &error))
    {
    	g_critical ("Unable to set CPU governor to %s: %s\n", governor, error->message);
    }
}

static XfpmCpuGovernor 
_governor_name_to_enum (const gchar *governor)
{
    if ( xfpm_strequal(governor, "ondemand") )
    	return CPU_ONDEMAND;
    else if ( xfpm_strequal(governor, "powersave") )
    	return CPU_POWERSAVE;
    else if ( xfpm_strequal (governor, "performance") )
    	return CPU_PERFORMANCE;
    
    return CPU_UNKNOWN;
}

static void
xfpm_cpu_update_governor (XfpmCpu *cpu)
{
    GError *error = NULL;
    gboolean cpu_freq_iface;
    
    g_object_get (G_OBJECT(cpu->priv->iface), 
		  "cpu-freq-iface", &cpu_freq_iface,
		  NULL);
		  
    if ( !cpu_freq_iface)
	return;
    
    gchar *current_governor = hal_iface_get_cpu_current_governor (cpu->priv->iface, &error);
    
    if ( error )
    {
	g_warning ("Get cpu governor failed: %s.", error->message);
	g_error_free (error);
	return;
    }
    
    if ( !current_governor )
    	return;

    XfpmCpuGovernor current = _governor_name_to_enum (current_governor);

    if ( !cpu->priv->power_save || ( !cpu->priv->on_battery && cpu->priv->power_save) )
    {
	if ( cpu->priv->cpu_governors & CPU_ONDEMAND )
	    xfpm_cpu_set_governor (cpu, "ondemand");
	else if ( cpu->priv->cpu_governors & CPU_PERFORMANCE )
	    xfpm_cpu_set_governor (cpu, "performance");
	else
	    g_critical ("No conveniant cpu governor found\n");
	return;
    }

    if ( cpu->priv->on_battery && cpu->priv->power_save )
    {
    	if ( current != CPU_POWERSAVE )
	{
	    TRACE ("Settings cpu governor to powersave");
	    xfpm_cpu_set_governor (cpu, "powersave");
	}
    }
    g_free (current_governor);
}

static gboolean
xfpm_cpu_get_available_governors (XfpmCpu *cpu)
{
    GError *error = NULL;
    gchar **governors =
    	hal_iface_get_cpu_governors (cpu->priv->iface, &error);
	
    if ( error )
    {
	g_critical ("Error getting available cpu governors");
	return FALSE;
    }
	
    if ( !governors )
    {
    	g_critical ("Unable to get CPU governors\n");
	return FALSE;
    }
    
    int i =0 ;
    for ( i = 0; governors[i]; i++)
    {
    	TRACE("found CPU governor %s", governors[i]);
	
	if (xfpm_strequal(governors[i], "powersave") )
	    cpu->priv->cpu_governors |= CPU_POWERSAVE;
	else if ( xfpm_strequal(governors[i], "ondemand") )
	    cpu->priv->cpu_governors |= CPU_ONDEMAND;
	else if ( xfpm_strequal(governors[i], "performance") )
	    cpu->priv->cpu_governors |= CPU_PERFORMANCE;
    }

    hal_iface_free_string_array (governors);
    
    return TRUE;
}

static void
xfpm_cpu_load_configuration (XfpmCpu *cpu)
{
    cpu->priv->power_save =
    	xfconf_channel_get_bool (cpu->priv->conf->channel, POWER_SAVE_ON_BATTERY, TRUE);
}

static void
xfpm_cpu_property_changed_cb (XfconfChannel *channel, gchar *property, GValue *value, XfpmCpu *cpu)
{
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
        return;
	
    if ( xfpm_strequal(property, POWER_SAVE_ON_BATTERY) )
    {
	gboolean val = g_value_get_boolean (value);
	cpu->priv->power_save = val;
	xfpm_cpu_update_governor(cpu);
    }
}

static void
xfpm_cpu_check (XfpmCpu *cpu)
{
    gboolean caller_privilege, cpu_freq_iface;
    
    g_object_get (G_OBJECT(cpu->priv->iface), 
		  "caller-privilege", &caller_privilege,
		  "cpu-freq-iface", &cpu_freq_iface,
		  NULL);
		  
    if ( !caller_privilege )
    {
	g_warning ("Using CPU FREQ interface permission denied");
	goto out;
    }
    
    if (!cpu_freq_iface)
    {
	g_warning ("CPU FREQ interface cannot be used");
	goto out;
    }
    
    if ( !xfpm_cpu_get_available_governors (cpu) )
    {
	g_critical ("Failed to handle cpu governors");
	goto out;
    }
out:
	;
}

static void
xfpm_cpu_adapter_changed_cb (XfpmAdapter *adapter, gboolean is_present, XfpmCpu *cpu)
{
    cpu->priv->on_battery = !is_present;
    xfpm_cpu_update_governor (cpu);
}

static void
xfpm_cpu_class_init(XfpmCpuClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = xfpm_cpu_finalize;

    g_type_class_add_private(klass,sizeof(XfpmCpuPrivate));
}

static void
xfpm_cpu_init(XfpmCpu *cpu)
{
    cpu->priv = XFPM_CPU_GET_PRIVATE(cpu);
    
    cpu->priv->cpu_governors = 0;
    cpu->priv->iface         = hal_iface_new ();
    cpu->priv->adapter       = xfpm_adapter_new ();
    
    cpu->priv->conf = xfpm_xfconf_new ();
    xfpm_cpu_load_configuration (cpu);
    
    g_signal_connect (cpu->priv->conf->channel, "property-changed",
		      G_CALLBACK(xfpm_cpu_property_changed_cb), cpu);
		      
    g_signal_connect (cpu->priv->adapter, "adapter-changed",
		      G_CALLBACK(xfpm_cpu_adapter_changed_cb), cpu);
    
    cpu->priv->on_battery = !xfpm_adapter_get_present (cpu->priv->adapter);
		
    xfpm_cpu_check (cpu);
}

static void
xfpm_cpu_finalize(GObject *object)
{
    XfpmCpu *cpu;

    cpu = XFPM_CPU(object);
    
    if ( cpu->priv->conf )
	g_object_unref (cpu->priv->conf);
	
    if ( cpu->priv->iface )
	g_object_unref (cpu->priv->iface);
	
    if ( cpu->priv->adapter)
	g_object_unref (cpu->priv->adapter);

    G_OBJECT_CLASS(xfpm_cpu_parent_class)->finalize(object);
}

XfpmCpu *
xfpm_cpu_new (void)
{
    XfpmCpu *cpu = NULL;
    cpu = g_object_new (XFPM_TYPE_CPU, NULL);
    return cpu;
}
