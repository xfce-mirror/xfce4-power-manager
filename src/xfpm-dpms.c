/*
 *
 * * Copyright (C) 2008 Ali <aliov@xfce.org>
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

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/dpmsstr.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfpm/xfpm-string.h"

#include "xfpm-dpms.h"
#include "xfpm-config.h"

#ifdef HAVE_DPMS

#define CHECK_DPMS_TIMEOUT 120

/* Init */
static void xfpm_dpms_class_init (XfpmDpmsClass *klass);
static void xfpm_dpms_init       (XfpmDpms *xfpm_dpms);
static void xfpm_dpms_finalize   (GObject *object);

#define XFPM_DPMS_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_DPMS, XfpmDpmsPrivate))

struct XfpmDpmsPrivate
{
    XfconfChannel *channel;
    
    gboolean       dpms_capable;
    gboolean       dpms_enabled;
    
    guint16        sleep_on_battery;
    guint16        off_on_battery;
    guint16        sleep_on_ac;
    guint16        off_on_ac;
    
    gboolean       on_battery;
    guint8         sleep_dpms_mode; /*0=sleep 1=suspend*/
};

G_DEFINE_TYPE(XfpmDpms, xfpm_dpms, G_TYPE_OBJECT)

static void
xfpm_dpms_class_init(XfpmDpmsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = xfpm_dpms_finalize;


    g_type_class_add_private(klass,sizeof(XfpmDpmsPrivate));
}

static void
xfpm_dpms_init(XfpmDpms *dpms)
{
    dpms->priv = XFPM_DPMS_GET_PRIVATE(dpms);
    
    dpms->priv->dpms_capable = DPMSCapable (GDK_DISPLAY());
    
}

static void
xfpm_dpms_finalize(GObject *object)
{
    XfpmDpms *dpms;

    dpms = XFPM_DPMS (object);

    G_OBJECT_CLASS(xfpm_dpms_parent_class)->finalize(object);
}

static void
xfpm_dpms_set_timeouts (XfpmDpms *dpms, guint16 standby, guint16 suspend, guint off)
{
    CARD16 x_standby = 0 ,x_suspend = 0,x_off = 0;
    
    DPMSGetTimeouts (GDK_DISPLAY(), &x_standby, &x_suspend, &x_off);
    
    if ( standby != x_standby || suspend != x_suspend || off != x_off )
    {
	TRACE ("Settings dpms: standby=%d suspend=%d off=%d\n", standby, suspend, off);
	DPMSSetTimeouts (GDK_DISPLAY(), standby,
					suspend,
					off );
    }
    
}

static void
xfpm_dpms_timeouts_on_battery (XfpmDpms *dpms)
{
    if ( dpms->priv->sleep_dpms_mode == 0 )
    {
	xfpm_dpms_set_timeouts      (dpms, 
				     dpms->priv->sleep_on_battery ,
				     0,
				     dpms->priv->off_on_battery );
    }
    else 
    {
	xfpm_dpms_set_timeouts 	    (dpms, 
				     0,
				     dpms->priv->sleep_on_battery ,
				     dpms->priv->off_on_battery );
    }
}

static void
xfpm_dpms_timeouts_on_adapter (XfpmDpms *dpms)
{
    if ( dpms->priv->sleep_dpms_mode == 0 )
    {
	xfpm_dpms_set_timeouts	   (dpms, 
				    dpms->priv->sleep_on_ac ,
				    0,
				    dpms->priv->off_on_ac );
    }
    else
    {
	xfpm_dpms_set_timeouts     (dpms, 
				    0,
				    dpms->priv->sleep_on_ac ,
				    dpms->priv->off_on_ac );
    }
}

static gboolean
xfpm_dpms_enable_disable (XfpmDpms *dpms)
{
    if ( !dpms->priv->dpms_capable )
    	return FALSE;
	
    BOOL on_off;
    CARD16 state = 0;
    
    DPMSInfo (GDK_DISPLAY(), &state, &on_off);
    
    if ( !on_off && dpms->priv->dpms_enabled )
    {
        TRACE("DPMS is disabled, enabling it: user settings");
        DPMSEnable(GDK_DISPLAY());
    } 
    else if ( on_off && !dpms->priv->dpms_enabled )
    {
        TRACE("DPMS is enabled, disabling it: user settings");
        DPMSDisable(GDK_DISPLAY());
    }
    
    return TRUE;
}

static void
xfpm_dpms_check (XfpmDpms *dpms)
{
    xfpm_dpms_enable_disable (dpms);
    
    if ( !dpms->priv->on_battery )
    	xfpm_dpms_timeouts_on_adapter (dpms);
    else 
    	xfpm_dpms_timeouts_on_battery (dpms);
}

static void
xfpm_dpms_value_changed_cb (XfconfChannel *channel, gchar *property,
			    GValue *value, XfpmDpms *dpms)
{
    if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
    	return;
	
    gboolean set = FALSE;
    
    if ( xfpm_strequal (property, DPMS_ENABLED_CFG) )
    {
	gboolean val = g_value_get_boolean (value);
	dpms->priv->dpms_enabled = val;
	set = TRUE;
    }
    else if ( xfpm_strequal (property, ON_AC_DPMS_SLEEP) )
    {
	guint val = g_value_get_uint (value);
	dpms->priv->sleep_on_ac = MIN(3600, val * 60);
	set = TRUE;
    }
    else if ( xfpm_strequal (property, ON_AC_DPMS_OFF) )
    {
	guint val = g_value_get_uint (value);
	dpms->priv->off_on_ac = MIN(3600, val * 60);
	set = TRUE;
    }
    else if ( xfpm_strequal (property, ON_BATT_DPMS_SLEEP) )
    {
	guint val = g_value_get_uint (value);
	dpms->priv->sleep_on_battery = MIN(3600, val * 60);
	set = TRUE;
    }
    else if ( xfpm_strequal (property, ON_BATT_DPMS_OFF) )
    {
	guint val = g_value_get_uint (value);
	dpms->priv->off_on_battery = MIN (3600, val * 60);
	set = TRUE;
    }
    else if ( xfpm_strequal (property, DPMS_SLEEP_MODE) )
    {
	const gchar *str = g_value_get_string (value);
	if ( xfpm_strequal (str, "sleep" ) )
	{
	    dpms->priv->sleep_dpms_mode = 0;
	}
	else if ( xfpm_strequal (str, "suspend") )
	{
	    dpms->priv->sleep_dpms_mode = 1;
	}
	else
	{
	    g_critical("Invalid value %s for property %s\n", str, DPMS_SLEEP_MODE);
	    dpms->priv->sleep_dpms_mode = 0;
	}
	set = TRUE;
    }
    
    if ( set )
    	xfpm_dpms_check (dpms);
}

static void
xfpm_dpms_load_configuration (XfpmDpms *dpms)
{
    dpms->priv->dpms_enabled =
    	xfconf_channel_get_bool (dpms->priv->channel, DPMS_ENABLED_CFG, TRUE);
    
    dpms->priv->sleep_on_battery = 
    	MIN( xfconf_channel_get_uint( dpms->priv->channel, ON_BATT_DPMS_SLEEP, 3) * 60, 3600);

    dpms->priv->off_on_battery = 
    	MIN(xfconf_channel_get_uint( dpms->priv->channel, ON_BATT_DPMS_OFF, 5) * 60, 3600);
	
    dpms->priv->sleep_on_ac = 
    	MIN(xfconf_channel_get_uint( dpms->priv->channel, ON_AC_DPMS_SLEEP, 10) * 60, 3600);
    
    dpms->priv->off_on_ac = 
    	MIN(xfconf_channel_get_uint( dpms->priv->channel, ON_AC_DPMS_OFF, 15) * 60, 3600);
	
    gchar *str = xfconf_channel_get_string (dpms->priv->channel, DPMS_SLEEP_MODE, "sleep");
    
    if ( xfpm_strequal (str, "sleep" ) )
    {
	dpms->priv->sleep_dpms_mode = 0;
    }
    else if ( xfpm_strequal (str, "suspend") )
    {
	dpms->priv->sleep_dpms_mode = 1;
    }
    else
    {
	g_critical("Invalid value %s for property %s\n", str, DPMS_SLEEP_MODE);
	dpms->priv->sleep_dpms_mode = 0;
    }
    g_free (str);
}

XfpmDpms *
xfpm_dpms_new (XfconfChannel *channel)
{
    XfpmDpms *dpms = NULL;
    dpms = g_object_new (XFPM_TYPE_DPMS, NULL);
    
    
    if ( !dpms->priv->dpms_capable )
    {
    	g_warning ("Display dpms incapable\n");
    	goto out;
    }

    dpms->priv->channel = channel;
    
    xfpm_dpms_load_configuration (dpms);
    g_signal_connect (dpms->priv->channel, "property-changed",
		      G_CALLBACK(xfpm_dpms_value_changed_cb), dpms);
		      
    g_timeout_add_seconds ( CHECK_DPMS_TIMEOUT,
    			    (GSourceFunc) xfpm_dpms_enable_disable, dpms);
out:
    return dpms;
}

void xfpm_dpms_set_on_battery  (XfpmDpms *dpms, gboolean on_battery)
{
    g_return_if_fail (XFPM_IS_DPMS(dpms));
    
    dpms->priv->on_battery = on_battery;
    
    xfpm_dpms_check (dpms);
}

gboolean xfpm_dpms_capable (XfpmDpms *dpms)
{
    g_return_val_if_fail (XFPM_IS_DPMS(dpms), FALSE);
    
    return dpms->priv->dpms_capable;
}

#endif /* HAVE_DPMS */
