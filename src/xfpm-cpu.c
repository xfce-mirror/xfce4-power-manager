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
    guint8         cpu_governors;
};

G_DEFINE_TYPE(XfpmCpu, xfpm_cpu, G_TYPE_OBJECT)

static void
xfpm_cpu_set_governor (XfpmCpu *cpu, const gchar *governor)
{
    GError *error = NULL;
    TRACE("Settings cpu governor to %s", governor);
    
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

static XfpmCpuGovernor
xfpm_cpu_get_current_governor (XfpmCpu *cpu)
{
    gchar *current_governor = NULL;
    XfpmCpuGovernor governor_enum;
    
    current_governor = hal_iface_get_cpu_current_governor (cpu->priv->iface, NULL);
    
    if ( !current_governor )
    {
	g_warning ("Unable to get current governor");
	return CPU_UNKNOWN;
    }
    
    governor_enum = _governor_name_to_enum (current_governor);
    
    g_free (current_governor);
    return governor_enum;
}

/*
 * Get the available CPU governors on the system
 */
static void
xfpm_cpu_get_available_governors (XfpmCpu *cpu)
{
    int i;
    gchar **governors = NULL;
    governors = hal_iface_get_cpu_governors (cpu->priv->iface, NULL);
	
    if ( !governors || !governors[0])
    {
    	g_critical ("Unable to get CPU governors\n");
	return;
    }
    
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
}

static gboolean
xfpm_cpu_check_iface (XfpmCpu *cpu)
{
    gboolean caller_privilege, cpu_freq_iface;
    g_object_get (G_OBJECT(cpu->priv->iface), 
		  "caller-privilege", &caller_privilege,
		  "cpu-freq-iface", &cpu_freq_iface,
		  NULL);
		  
    if ( !caller_privilege )
    {
	g_warning ("Using CPU FREQ interface permission denied");
	return FALSE;
    }
    
    if (!cpu_freq_iface)
    {
	g_warning ("CPU FREQ interface cannot be used");
	return FALSE;
    }
    
    xfpm_cpu_get_available_governors (cpu);
    
    if ( !cpu->priv->cpu_governors & CPU_POWERSAVE || 
	 !cpu->priv->cpu_governors & CPU_ONDEMAND  ||
	 !cpu->priv->cpu_governors & CPU_PERFORMANCE )
    {
	g_warning ("No convenient cpu governors found on the system, cpu frequency control will be disabled");
	return FALSE;
    }
    return TRUE;
}

static void
xfpm_cpu_set_power_save (XfpmCpu *cpu)
{
    if ( xfpm_cpu_get_current_governor (cpu) != CPU_POWERSAVE )
	xfpm_cpu_set_governor (cpu, "powersave");
}

static void
xfpm_cpu_set_performance_ondemand (XfpmCpu *cpu)
{
    if ( xfpm_cpu_get_current_governor (cpu) != CPU_ONDEMAND )
	xfpm_cpu_set_governor (cpu, "ondemand");
}

static void
xfpm_cpu_refresh (XfpmCpu *cpu)
{
    gboolean power_save = xfpm_xfconf_get_property_bool (cpu->priv->conf, POWER_SAVE_ON_BATTERY);
    
    if (!power_save)
	return;
	
    if ( cpu->priv->on_battery )
	xfpm_cpu_set_power_save (cpu);
    else 
	xfpm_cpu_set_performance_ondemand (cpu);
}

static void
xfpm_cpu_adapter_changed_cb (XfpmAdapter *adapter, gboolean is_present, XfpmCpu *cpu)
{
    cpu->priv->on_battery = !is_present;
    xfpm_cpu_refresh (cpu);
}

static void
xfpm_cpu_power_save_settings_changed_cb (XfpmXfconf *conf, XfpmCpu *cpu)
{
    xfpm_cpu_refresh (cpu);
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
    
    if ( xfpm_cpu_check_iface (cpu))
    {
	cpu->priv->adapter       = xfpm_adapter_new ();
	cpu->priv->conf 	 = xfpm_xfconf_new ();
	
	g_signal_connect (cpu->priv->adapter, "adapter-changed",
			  G_CALLBACK(xfpm_cpu_adapter_changed_cb), cpu);
			  
	g_signal_connect (cpu->priv->conf, "power-save-settings-changed",
			  G_CALLBACK(xfpm_cpu_power_save_settings_changed_cb), cpu);
	
	cpu->priv->on_battery = !xfpm_adapter_get_present (cpu->priv->adapter);
    }
    
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
