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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <xfconf/xfconf.h>

#include "xfpm-cpu.h"
#include "xfpm-hal.h"
#include "xfpm-debug.h"
#include "xfpm-common.h"
#include "xfpm-enum-types.h"

#define XFPM_CPU_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE(o,XFPM_TYPE_CPU,XfpmCpuPrivate))

static void xfpm_cpu_init(XfpmCpu *cpu);
static void xfpm_cpu_class_init(XfpmCpuClass *klass);
static void xfpm_cpu_finalize(GObject *object);


static void xfpm_cpu_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec);
static void xfpm_cpu_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec);

static void xfpm_cpu_load_config(XfpmCpu *cpu);
                                   
static void xfpm_cpu_set_governor(XfpmCpu *cpu,
                                  gboolean ac_adapter_present);

static void xfpm_cpu_notify_cb (GObject *object,
                                GParamSpec *arg1,
                                gpointer data);
                                
G_DEFINE_TYPE(XfpmCpu,xfpm_cpu,G_TYPE_OBJECT)

struct XfpmCpuPrivate
{
    XfpmHal *hal;
};

enum
{
    PROP_0,
    PROP_AC_ADAPTER,
    PROP_CPU_FREQ,
    PROP_ON_AC_CPU_GOV,
    PROP_ON_BATT_CPU_GOV
};

static void
xfpm_cpu_class_init(XfpmCpuClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    
    gobject_class->set_property = xfpm_cpu_set_property;
    gobject_class->get_property = xfpm_cpu_get_property;
    
    gobject_class->finalize = xfpm_cpu_finalize;
    
    g_object_class_install_property(gobject_class,
                                    PROP_AC_ADAPTER,
                                    g_param_spec_boolean("on-ac-adapter",
                                                         "On ac adapter",
                                                         "On Ac power",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
                                                         
    g_object_class_install_property(gobject_class,
                                    PROP_CPU_FREQ,
                                    g_param_spec_boolean("cpu-freq",
                                                         "cpu freq",
                                                         "cpu freq settings",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
    
    
    g_object_class_install_property(gobject_class,
                                    PROP_ON_AC_CPU_GOV,
                                    g_param_spec_flags("on-ac-cpu-gov",
                                                      "On ac cpu gov",
                                                      "Cpu governor on AC power",
                                                      XFPM_TYPE_CPU_GOVERNOR,
                                                      ONDEMAND,
                                                      G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class,
                                    PROP_ON_BATT_CPU_GOV,
                                    g_param_spec_flags("on-batt-cpu-gov",
                                                      "On battery cpu gov",
                                                      "Cpu governor on battery power",
                                                      XFPM_TYPE_CPU_GOVERNOR,
                                                      POWERSAVE,
                                                      G_PARAM_READWRITE));
    g_type_class_add_private(klass,sizeof(XfpmCpuPrivate));
}

static void
xfpm_cpu_init(XfpmCpu *cpu)
{
    XfpmCpuPrivate *priv;
    priv = XFPM_CPU_GET_PRIVATE(cpu);
    
    priv->hal = xfpm_hal_new();
    
    xfpm_cpu_load_config(cpu);
    
    g_signal_connect(G_OBJECT(cpu),"notify",G_CALLBACK(xfpm_cpu_notify_cb),NULL);
    
}

static void xfpm_cpu_set_property(GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
#ifdef DEBUG
    gchar *content;
    content = g_strdup_value_contents(value);
    XFPM_DEBUG("param:%s value contents:%s\n",pspec->name,content);
    g_free(content);
#endif      
    XfpmCpu *cpu;
    cpu = XFPM_CPU(object);
    
    switch (prop_id)
    {
    case PROP_AC_ADAPTER:
        cpu->ac_adapter_present = g_value_get_boolean(value);
        break;   
    case PROP_CPU_FREQ:
        cpu->cpu_freq_enabled = g_value_get_boolean(value);
        break;    
    case PROP_ON_AC_CPU_GOV:
        cpu->on_ac_cpu_gov = g_value_get_flags(value);
        break;
    case PROP_ON_BATT_CPU_GOV:
        cpu->on_batt_cpu_gov = g_value_get_flags(value);
        break;    
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
        break;
    }
}    
                               
static void xfpm_cpu_get_property(GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    XfpmCpu *cpu;
    cpu = XFPM_CPU(object);
        
    switch (prop_id)
    {
    case PROP_AC_ADAPTER:
        g_value_set_boolean(value,cpu->ac_adapter_present);
        break;   
    case PROP_CPU_FREQ:
        g_value_set_boolean(value,cpu->cpu_freq_enabled);
        break;
    case PROP_ON_AC_CPU_GOV:
        g_value_set_flags(value,cpu->on_ac_cpu_gov);
        break;
    case PROP_ON_BATT_CPU_GOV:
        g_value_set_flags(value,cpu->on_batt_cpu_gov);
        break;            
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
        break;
    }                         
    
#ifdef DEBUG
    gchar *content;
    content = g_strdup_value_contents(value);
    XFPM_DEBUG("param:%s value contents:%s\n",pspec->name,content);
    g_free(content);
#endif  
}

static void
xfpm_cpu_finalize(GObject *object)
{
    XfpmCpu *cpu = XFPM_CPU(object);
    cpu->priv = XFPM_CPU_GET_PRIVATE(cpu);
    
    if ( cpu->priv->hal )
    {
        g_object_unref(cpu->priv->hal);
    }
    
    G_OBJECT_CLASS(xfpm_cpu_parent_class)->finalize(object);
}

static void
xfpm_cpu_load_config(XfpmCpu *cpu)
{
    XFPM_DEBUG("loading configuration\n");
    XfconfChannel *channel;
    
    GError *g_error = NULL;
    if ( !xfconf_init(&g_error) )
    {
        g_critical("xfconf init failed: %s\n",g_error->message);
        XFPM_DEBUG("Using default values\n");
        g_error_free(g_error);
        cpu->on_ac_cpu_gov = ONDEMAND;
        cpu->on_batt_cpu_gov = POWERSAVE;
        cpu->cpu_freq_enabled = TRUE;
        return;
    }
    
    channel = xfconf_channel_new(XFPM_CHANNEL_CFG);
    
    cpu->on_ac_cpu_gov = xfconf_channel_get_uint(channel,ON_AC_CPU_GOV_CFG,ONDEMAND);
    cpu->on_batt_cpu_gov = xfconf_channel_get_uint(channel,ON_BATT_CPU_GOV_CFG,POWERSAVE);
    cpu->cpu_freq_enabled = xfconf_channel_get_bool(channel,CPU_FREQ_SCALING_CFG,TRUE);
    
    g_object_unref(channel);
    xfconf_shutdown();    
}


static
gchar *_get_governor_from_enum(XfpmCpuGovernor governor)
{
    if ( governor == POWERSAVE )
    {
        return "powersave";
    }
    else if ( governor == ONDEMAND )
    {
        return "ondemand";
    }
    else if ( governor == PERFORMANCE )
    {
        return "performance";
    }
    else if ( governor == USERSPACE )
    {
        return "userspace";
    }
    else if ( governor == CONSERVATIVE )
    {
        return "conservative";
    }
    return NULL;
}

static void
xfpm_cpu_set_governor(XfpmCpu *cpu,gboolean ac_adapter_present)
{
    XfpmCpuPrivate *priv;
    priv = XFPM_CPU_GET_PRIVATE(cpu);

    gchar *current_governor;
    GError *error = NULL;
    current_governor = xfpm_hal_get_current_cpu_governor(priv->hal,&error);
    
    if ( error )
    {
        XFPM_DEBUG("%s:\n",error->message);
        g_error_free(error);
        return;
    }
    if ( !current_governor ) return;
    
    gchar *config_gov = 
           _get_governor_from_enum(ac_adapter_present ? cpu->on_ac_cpu_gov : cpu->on_batt_cpu_gov);

    if ( !config_gov )
    {
        XFPM_DEBUG("Unknown cpu governor\n");
        return;
    }
        
    XFPM_DEBUG("Configuration governor %s\n",config_gov);
    
    if ( strcmp(current_governor,config_gov) ) 
    {
        XFPM_DEBUG("CPU actuel governor %s, setting=%s\n",current_governor,config_gov);
        xfpm_hal_set_cpu_governor(priv->hal,config_gov,&error);
        if ( error )
        {
            XFPM_DEBUG("%s:\n",error->message);
            g_error_free(error);
            return;
        }
    }    
    else
    {
        XFPM_DEBUG("No Need to change CPU Governor\n");
    }
}

static void 
xfpm_cpu_notify_cb (GObject *object,
                    GParamSpec *arg1,
                    gpointer data)
{                                
    XfpmCpu *cpu = XFPM_CPU(object);
    
    if ( !cpu->cpu_freq_enabled )
    {
        XFPM_DEBUG("Cpu freq control is disabled\n");
    }
    
    if ( !strcmp(arg1->name,"on-ac-adapter") ||
         !strcmp(arg1->name,"on-ac-cpu-gov") ||
         !strcmp(arg1->name,"on-batt-cpu-gov") )
    {
        xfpm_cpu_set_governor(cpu,cpu->ac_adapter_present);
    }
}

XfpmCpu *
xfpm_cpu_new(void)
{
    XfpmCpu *cpu = NULL;
    cpu = g_object_new(XFPM_TYPE_CPU,NULL);
    return cpu;
}
