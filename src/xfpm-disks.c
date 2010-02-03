/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#include <dbus/dbus-glib.h>

#include "xfpm-disks.h"
#include "xfpm-polkit.h"
#include "xfpm-xfconf.h"
#include "xfpm-power.h"
#include "xfpm-config.h"
#include "xfpm-debug.h"

static void xfpm_disks_finalize   (GObject *object);

#define XFPM_DISKS_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_DISKS, XfpmDisksPrivate))

struct XfpmDisksPrivate
{
    XfpmXfconf      *conf;
    XfpmPower       *power;
    XfpmPolkit      *polkit;
    
    DBusGConnection *bus;
    DBusGProxy      *proxy;
    gchar           *cookie;
    gboolean         set;
    gboolean         can_spin;


    gboolean         is_udisks;
};

G_DEFINE_TYPE (XfpmDisks, xfpm_disks, G_TYPE_OBJECT)

static void
xfpm_disks_class_init (XfpmDisksClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_disks_finalize;

    g_type_class_add_private (klass, sizeof (XfpmDisksPrivate));
}

static void
xfpm_disks_disable_spin_down_timeouts (XfpmDisks *disks)
{
    GError *error = NULL;
    
    disks->priv->set = FALSE;
    
    XFPM_DEBUG ("Clearing spin down timeout");
    
    dbus_g_proxy_call (disks->priv->proxy, "DriveUnsetAllSpindownTimeouts", &error,
		       G_TYPE_STRING, disks->priv->cookie,
		       G_TYPE_INVALID,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_warning ("Failed to unset spindown timeouts : %s", error->message);
	g_error_free (error);
	disks->priv->set = TRUE;
    }
    
    g_free (disks->priv->cookie);
    disks->priv->cookie = NULL;
}

static void
xfpm_disks_enable_spin_down_timeouts (XfpmDisks *disks, gint timeout)
{
    GError *error = NULL;
    const gchar **options = { NULL };
    
    disks->priv->set = TRUE;
    
    XFPM_DEBUG ("Setting spin down timeout %d", timeout);
    
    dbus_g_proxy_call (disks->priv->proxy, "DriveSetAllSpindownTimeouts", &error,
		       G_TYPE_INT, timeout,
		       G_TYPE_STRV, options,
		       G_TYPE_INVALID,
		       G_TYPE_STRING, &disks->priv->cookie,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_warning ("Failed to set spindown timeouts : %s", error->message);
	g_error_free (error);
	disks->priv->set = FALSE;
    }
}

static void
xfpm_disks_set_spin_timeouts (XfpmDisks *disks)
{
    gboolean enabled;
    gboolean on_battery;
    gint timeout = 0;
    
    if (!disks->priv->can_spin )
	return;
    
    g_object_get (G_OBJECT (disks->priv->power),
		  "on-battery", &on_battery,
		  NULL);

    if ( !on_battery )
    {
	g_object_get (G_OBJECT (disks->priv->conf),
		      SPIN_DOWN_ON_AC, &enabled,
		      SPIN_DOWN_ON_AC_TIMEOUT, &timeout,
		      NULL);
    }
    else
    {
	g_object_get (G_OBJECT (disks->priv->conf),
		      SPIN_DOWN_ON_BATTERY, &enabled,
		      SPIN_DOWN_ON_BATTERY_TIMEOUT, &timeout,
		      NULL);
    }
    
    XFPM_DEBUG ("On Battery=%d spin_down_enabled=%d timeout=%d\n", on_battery, enabled, timeout);
    
    if ( !enabled )
    {
	if ( disks->priv->set && disks->priv->cookie )
	    xfpm_disks_disable_spin_down_timeouts (disks);
    }
    else if ( timeout != 0 && timeout > 120 && !disks->priv->set)
    {
	xfpm_disks_enable_spin_down_timeouts (disks, timeout);
    }
}

static void
xfpm_disks_get_is_auth_to_spin (XfpmDisks *disks)
{
    const gchar *action_id;

    action_id = disks->priv->is_udisks ? "org.freedesktop.udisks.drive-set-spindown" : "org.freedesktop.devicekit.disks.drive-set-spindown";

    disks->priv->can_spin = xfpm_polkit_check_auth (disks->priv->polkit, 
						    action_id);
						    
    XFPM_DEBUG ("Is auth to spin down disks : %d", disks->priv->can_spin);
}

static void
xfpm_disks_init (XfpmDisks *disks)
{
    GError *error = NULL;
    
    disks->priv = XFPM_DISKS_GET_PRIVATE (disks);
    
    disks->priv->can_spin = FALSE;
    disks->priv->bus    = NULL;
    disks->priv->proxy  = NULL;
    disks->priv->conf   = NULL;
    disks->priv->power  = NULL;
    disks->priv->cookie = NULL;
    disks->priv->polkit = NULL;

    disks->priv->is_udisks = FALSE;
    
    disks->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Unable to get system bus connection : %s", error->message);
	g_error_free (error);
	goto out;
    }

    disks->priv->proxy = dbus_g_proxy_new_for_name_owner (disks->priv->bus,
							  "org.freedesktop.UDisks",
							  "/org/freedesktop/UDisks",
							  "org.freedesktop.UDisks",
							  NULL);
    
    if ( !disks->priv->proxy )
    {
	g_message ("UDisks not found, trying devkit-disks");
	disks->priv->proxy = dbus_g_proxy_new_for_name_owner (disks->priv->bus,
							      "org.freedesktop.DeviceKit.Disks",
							      "/org/freedesktop/DeviceKit/Disks",
							      "org.freedesktop.DeviceKit.Disks",
							      NULL);
    }
    else
    {
	disks->priv->is_udisks = TRUE;
    }
    
    if ( !disks->priv->proxy )
    {
	g_warning ("Unable to create proxy for 'org.freedesktop.DeviceKit.Disks'");
	goto out;
    }

    disks->priv->conf = xfpm_xfconf_new ();
    disks->priv->power  = xfpm_power_get ();
    disks->priv->polkit = xfpm_polkit_get ();

    xfpm_disks_get_is_auth_to_spin (disks);
    
    g_signal_connect_swapped (disks->priv->polkit, "auth-changed",
			      G_CALLBACK (xfpm_disks_get_is_auth_to_spin), disks);

    g_signal_connect_swapped (disks->priv->power, "on-battery-changed",
			      G_CALLBACK (xfpm_disks_set_spin_timeouts), disks);

    g_signal_connect_swapped (disks->priv->conf, "notify::" SPIN_DOWN_ON_AC,
			      G_CALLBACK (xfpm_disks_set_spin_timeouts), disks);
			
    g_signal_connect_swapped (disks->priv->conf, "notify::" SPIN_DOWN_ON_AC_TIMEOUT,
			      G_CALLBACK (xfpm_disks_set_spin_timeouts), disks);
			      
    g_signal_connect_swapped (disks->priv->conf, "notify::" SPIN_DOWN_ON_BATTERY,
			      G_CALLBACK (xfpm_disks_set_spin_timeouts), disks);
    
    g_signal_connect_swapped (disks->priv->conf, "notify::" SPIN_DOWN_ON_BATTERY_TIMEOUT,
			      G_CALLBACK (xfpm_disks_set_spin_timeouts), disks);
			      
    xfpm_disks_set_spin_timeouts (disks);
    
out:
    ;

}

static void
xfpm_disks_finalize (GObject *object)
{
    XfpmDisks *disks;

    disks = XFPM_DISKS (object);

    if (disks->priv->can_spin && disks->priv->set )
	xfpm_disks_disable_spin_down_timeouts (disks);
    
    if ( disks->priv->bus )
	dbus_g_connection_unref (disks->priv->bus);
	
    if ( disks->priv->proxy )
	g_object_unref (disks->priv->proxy);
	
    if ( disks->priv->polkit )
	g_object_unref (disks->priv->polkit);
	
    if ( disks->priv->conf )
	g_object_unref (disks->priv->conf);
	
    if ( disks->priv->power )
	g_object_unref (disks->priv->power );

    G_OBJECT_CLASS (xfpm_disks_parent_class)->finalize (object);
}

XfpmDisks *
xfpm_disks_new (void)
{
    XfpmDisks *disks = NULL;
    disks = g_object_new (XFPM_TYPE_DISKS, NULL);
    return disks;
}

gboolean xfpm_disks_get_can_spin (XfpmDisks *disks)
{
    return disks->priv->can_spin;
}

gboolean xfpm_disks_kit_is_running (XfpmDisks *disks)
{
    return disks->priv->proxy != NULL ? TRUE : FALSE;
}
