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

#include "libxfpm/dbus-hal.h"
#include "libxfpm/xfpm-string.h"

#include "xfpm-cpu.h"
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
    XfconfChannel *channel;
    DbusHal       *hbus;
    
    gboolean 	   on_battery;
    gboolean 	   power_save;
    
    gboolean       interface_ok;
    guint8         cpu_governors;
};

G_DEFINE_TYPE(XfpmCpu, xfpm_cpu, G_TYPE_OBJECT)

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
}

static void
xfpm_cpu_finalize(GObject *object)
{
    XfpmCpu *cpu;

    cpu = XFPM_CPU(object);

    G_OBJECT_CLASS(xfpm_cpu_parent_class)->finalize(object);
}

static void
xfpm_cpu_set_governor (XfpmCpu *cpu, const gchar *governor)
{
    
    if (!dbus_hal_set_cpu_governor (cpu->priv->hbus, governor, NULL))
    	g_critical ("Unable to set CPU governor to %s\n", governor);
    
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
    if ( !cpu->priv->interface_ok )
    	return;
	
    gchar *current_governor = dbus_hal_get_cpu_current_governor (cpu->priv->hbus, NULL);
    
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
	return;
    }

}

static void
xfpm_cpu_load_configuration (XfpmCpu *cpu)
{
    cpu->priv->power_save =
    	xfconf_channel_get_bool (cpu->priv->channel, POWER_SAVE_ON_BATTERY, TRUE);
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

static gboolean
xfpm_cpu_get_available_governors (XfpmCpu *cpu)
{
    gchar **governors =
    	dbus_hal_get_cpu_available_governors (cpu->priv->hbus, NULL);
	
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

    //libhal_free_string_array (governors);
    
    return TRUE;
    
}

XfpmCpu *
xfpm_cpu_new (XfconfChannel *channel, DbusHal *hbus)
{
    XfpmCpu *cpu = NULL;
    cpu = g_object_new (XFPM_TYPE_CPU, NULL);
    
    cpu->priv->hbus = hbus;
    
    if ( !xfpm_cpu_get_available_governors (cpu))
    	goto out;
    
    cpu->priv->interface_ok = TRUE;
    cpu->priv->channel = channel;
    xfpm_cpu_load_configuration (cpu);
    
    g_signal_connect (cpu->priv->channel, "property-changed",
		      G_CALLBACK(xfpm_cpu_property_changed_cb), cpu);
		      
out:
    return cpu;
}

void xfpm_cpu_set_on_battery (XfpmCpu *cpu, gboolean on_battery)
{
    g_return_if_fail (XFPM_IS_CPU(cpu));
    
    cpu->priv->on_battery = on_battery;
    
    xfpm_cpu_update_governor (cpu);
}
