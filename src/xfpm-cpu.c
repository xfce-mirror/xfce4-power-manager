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
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/hal-manager.h"
#include "libxfpm/xfpm-string.h"

#include "xfpm-cpu.h"
#include "xfpm-adapter.h"
#include "xfpm-xfconf.h"
#include "xfpm-config.h"
#include "xfpm-enum.h"

#ifdef SYSTEM_IS_LINUX /* the end if at the end of the file */

static void xfpm_cpu_finalize   (GObject *object);

#define XFPM_CPU_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_CPU, XfpmCpuPrivate))

struct XfpmCpuPrivate
{
    DBusGConnection *bus;
    XfpmAdapter     *adapter;
    XfpmXfconf      *conf;
    
    gboolean 	     on_battery;
    guint8           cpu_governors;
};

G_DEFINE_TYPE(XfpmCpu, xfpm_cpu, G_TYPE_OBJECT)

static gboolean
xfpm_cpu_check_interface (XfpmCpu *cpu)
{
    DBusMessage *message;
    DBusMessage *reply;
    DBusError error ;
    
    message = dbus_message_new_method_call ("org.freedesktop.Hal",
					    "/org/freedesktop/Hal",
					    "org.freedesktop.Hal.Device.CPUFreq",
					    "JustToCheck");
    
    if (!message)
    	return FALSE;
    
    dbus_error_init (&error);
    
    reply = 
    	dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection(cpu->priv->bus),
						   message, 2000, &error);
    dbus_message_unref (message);
    
    if ( reply ) dbus_message_unref (reply);
    
    if ( dbus_error_is_set(&error) )
    {
	if ( !g_strcmp0 (error.name, "org.freedesktop.DBus.Error.UnknownMethod") )
        {
            dbus_error_free(&error);
	    return TRUE;
        }
    }
    return FALSE;
}

static gchar **
xfpm_cpu_get_hal_available_governors (XfpmCpu *cpu)
{
    gchar **governors = NULL;
    GError *error = NULL;
    DBusGProxy *proxy = dbus_g_proxy_new_for_name (cpu->priv->bus,
						   "org.freedesktop.Hal",
						   "/org/freedesktop/Hal/devices/computer",
						   "org.freedesktop.Hal.Device.CPUFreq");
    if ( !proxy )
    {
	g_critical ("Failed to create proxy");
	goto out;
    }
    
    dbus_g_proxy_call (proxy, "GetCPUFreqAvailableGovernors", &error,
		       G_TYPE_INVALID,
		       G_TYPE_STRV, &governors,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_warning ("Failed to get cpu governors: %s", error->message);
	g_error_free (error);
    }
    
    g_object_unref (proxy);
out:
    return governors;
}

static gchar *
xfpm_cpu_get_hal_current_governor (XfpmCpu *cpu)
{
    gchar *governor = NULL;
    GError *error = NULL;
    
    DBusGProxy *proxy = dbus_g_proxy_new_for_name (cpu->priv->bus,
						   "org.freedesktop.Hal",
						   "/org/freedesktop/Hal/devices/computer",
						   "org.freedesktop.Hal.Device.CPUFreq");
    if ( !proxy )
    {
	g_critical ("Failed to create proxy");
	goto out;
    }
    
    dbus_g_proxy_call (proxy, "GetCPUFreqGovernor", &error,
		       G_TYPE_INVALID,
		       G_TYPE_STRING, &governor,
		       G_TYPE_INVALID);
    
    if ( error )
    {
	g_warning ("Failed to get cpu governor: %s", error->message);
	g_error_free (error);
    }
    
    g_object_unref (proxy);
out:
    return governor;
}

static gboolean
xfpm_cpu_set_hal_governor (XfpmCpu *cpu, const gchar *governor)
{
    gboolean ret = FALSE;
    GError *error = NULL;
    
    DBusGProxy *proxy = dbus_g_proxy_new_for_name (cpu->priv->bus,
						   "org.freedesktop.Hal",
						   "/org/freedesktop/Hal/devices/computer",
						   "org.freedesktop.Hal.Device.CPUFreq");
    if ( !proxy )
    {
	g_critical ("Failed to create proxy");
	goto out;
    }
    
    ret = dbus_g_proxy_call (proxy, "SetCPUFreqGovernor", &error,
	  		     G_TYPE_STRING, governor,
			     G_TYPE_INVALID,
			     G_TYPE_INVALID);
			     
    if ( error )
    {
	g_warning ("Failed to set cpu governor: %s", error->message);
	g_error_free (error);
    }
    		     
    g_object_unref (proxy);
out:
    return ret;
}

static void
xfpm_cpu_set_governor (XfpmCpu *cpu, const gchar *governor)
{
    TRACE("Settings cpu governor to %s", governor);
    
    if (! xfpm_cpu_set_hal_governor(cpu, governor))
    {
    	g_critical ("Unable to set CPU governor to %s", governor);
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
    
    current_governor = xfpm_cpu_get_hal_current_governor (cpu);
    
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
    governors = xfpm_cpu_get_hal_available_governors (cpu);
	
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
    hal_manager_free_string_array (governors);
}

static gboolean
xfpm_cpu_check_iface (XfpmCpu *cpu)
{
    gboolean cpu_freq_iface;
    
    cpu_freq_iface = xfpm_cpu_check_interface (cpu);
		  
    if (!cpu_freq_iface)
    {
	g_warning ("CPU FREQ interface cannot be used");
	return FALSE;
    }
    
    xfpm_cpu_get_available_governors (cpu);
    
    if ( (cpu->priv->cpu_governors & CPU_ONDEMAND || cpu->priv->cpu_governors & CPU_PERFORMANCE)
	 && (cpu->priv->cpu_governors & CPU_POWERSAVE) )
	return TRUE;
    
    g_warning ("No convenient cpu governors found on the system, cpu frequency control will be disabled");
    return FALSE;
}

static void
xfpm_cpu_set_power_save (XfpmCpu *cpu)
{
    if ( xfpm_cpu_get_current_governor (cpu) != CPU_POWERSAVE && cpu->priv->cpu_governors & CPU_POWERSAVE )
	xfpm_cpu_set_governor (cpu, "powersave");
}

static void
xfpm_cpu_set_performance_ondemand (XfpmCpu *cpu)
{
    XfpmCpuGovernor gov;
    
    gov = xfpm_cpu_get_current_governor (cpu);
    
    if ( gov == CPU_ONDEMAND )
	return;
	
    if ( (gov != CPU_ONDEMAND) && (cpu->priv->cpu_governors & CPU_ONDEMAND) )
	xfpm_cpu_set_governor (cpu, "ondemand");
    else if ( (gov != CPU_PERFORMANCE) && (cpu->priv->cpu_governors & CPU_PERFORMANCE) )
	xfpm_cpu_set_governor (cpu, "performance");
}

static void
xfpm_cpu_refresh (XfpmCpu *cpu)
{
    gboolean power_save;
    gboolean enable_cpu_freq;

    TRACE ("start");
    
    g_object_get (G_OBJECT (cpu->priv->conf),
		  POWER_SAVE_ON_BATTERY, &power_save,
		  CPU_FREQ_CONTROL, &enable_cpu_freq,
		  NULL);
    
    if ( enable_cpu_freq == FALSE )
	return;
    
    if (power_save == FALSE)
    {
	xfpm_cpu_set_performance_ondemand (cpu);
	return;
    }
	
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
xfpm_cpu_settings_changed (GObject *obj, GParamSpec *spec, XfpmCpu *cpu)
{
    if ( !g_strcmp0 (spec->name, CPU_FREQ_CONTROL) || !g_strcmp0 (spec->name, POWER_SAVE_ON_BATTERY))
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
    
    cpu->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
    
    if (!cpu->priv->bus)
	goto out;
    
    if ( xfpm_cpu_check_iface (cpu))
    {
	cpu->priv->adapter       = xfpm_adapter_new ();
	cpu->priv->conf 	 = xfpm_xfconf_new ();
	
	g_signal_connect (cpu->priv->adapter, "adapter-changed",
			  G_CALLBACK(xfpm_cpu_adapter_changed_cb), cpu);
	
	g_signal_connect (cpu->priv->conf, "notify",
			  G_CALLBACK (xfpm_cpu_settings_changed), cpu);
	
	cpu->priv->on_battery = !xfpm_adapter_get_present (cpu->priv->adapter);
	xfpm_cpu_refresh (cpu);
    }
    
out:
    ;
}

static void
xfpm_cpu_finalize(GObject *object)
{
    XfpmCpu *cpu;

    cpu = XFPM_CPU(object);

    if ( cpu->priv->conf )
	g_object_unref (cpu->priv->conf);
    
    if ( cpu->priv->conf )
	g_object_unref (cpu->priv->adapter);
	
    if ( cpu->priv->bus )
	dbus_g_connection_unref (cpu->priv->bus);

    G_OBJECT_CLASS(xfpm_cpu_parent_class)->finalize(object);
}

XfpmCpu *
xfpm_cpu_new (void)
{
    XfpmCpu *cpu = NULL;
    cpu = g_object_new (XFPM_TYPE_CPU, NULL);
    return cpu;
}

void xfpm_cpu_reload (XfpmCpu *cpu)
{
    g_return_if_fail (XFPM_IS_CPU (cpu));
    
    cpu->priv->cpu_governors = 0;
    xfpm_cpu_check_iface (cpu);
}

#endif /*SYSTEM_IS_LINUX*/
