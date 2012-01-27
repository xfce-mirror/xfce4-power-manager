/*
 * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
 * Copyright (C) 2011      Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "xfpm-dbus-service.h"
#include "xfpm-dbus.h"
#include "xfpm-power.h"



static void     xfpm_dbus_service_finalize        (GObject *object);
static gboolean xfpm_dbus_service_suspend         (XfpmDBusService *dbus_service, 
                                                   GError **error);
static gboolean xfpm_dbus_service_hibernate       (XfpmDBusService *dbus_service, 
                                                   GError **error);
static gboolean xfpm_dbus_service_can_suspend     (XfpmDBusService *dbus_service, 
                                                   gboolean *OUT_can_suspend,
                                                   GError **error);
static gboolean xfpm_dbus_service_can_hibernate   (XfpmDBusService *dbus_service, 
                                                   gboolean *OUT_can_hibernate,
                                                   GError **error);
static gboolean xfpm_dbus_service_get_on_battery  (XfpmDBusService *dbus_service, 
                                                   gboolean *OUT_on_battery, 
                                                   GError **error);
static gboolean xfpm_dbus_service_get_low_battery (XfpmDBusService *dbus_service, 
                                                   gboolean *OUT_low_battery, 
                                                   GError **error);
static gboolean xfpm_dbus_service_terminate       (XfpmDBusService *dbus_service, 
                                                   gboolean restart, 
                                                   GError **error);



/* include generate dbus infos */
#include "xfpm-dbus-service-infos.h"



struct _XfpmDBusServiceClass
{
  GObjectClass __parent__;
};

struct _XfpmDBusService
{
  GObject __parent__;

  DBusGConnection *connection;
  
  XfpmPower  *power;
};



G_DEFINE_TYPE (XfpmDBusService, xfpm_dbus_service, G_TYPE_OBJECT)



static void
xfpm_dbus_service_class_init (XfpmDBusServiceClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfpm_dbus_service_finalize;

  /* install the D-BUS info for our class */
  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass), 
                                   &dbus_glib_xfpm_dbus_service_object_info);
}



static void
xfpm_dbus_service_init (XfpmDBusService *dbus_service)
{
    GError *error = NULL;

    /* try to connect to the session bus */
    dbus_service->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (G_LIKELY (dbus_service->connection != NULL))
    {
        /* register the /org/xfce/PowerManager object */
        dbus_g_connection_register_g_object (dbus_service->connection,
                                             "/org/xfce/PowerManager",
                                             G_OBJECT (dbus_service));
       
        xfpm_dbus_register_name (dbus_g_connection_get_connection (dbus_service->connection), 
                                                                   "org.xfce.PowerManager");
       
        xfpm_dbus_register_name (dbus_g_connection_get_connection (dbus_service->connection), 
                                                                   "org.xfce.PowerManager.Priv");
    }
    else
    {
        /* notify the user that D-BUS service won't be available */
        g_printerr ("%s: Failed to connect to the D-BUS session bus: %s\n",
                    PACKAGE_NAME, error->message);
        g_error_free (error);
    }

    dbus_service->power = xfpm_power_get ();
}



static void
xfpm_dbus_service_finalize (GObject *object)
{
    XfpmDBusService *dbus_service = XFPM_DBUS_SERVICE (object);
    
    /* release the D-BUS connection object */
    if (G_LIKELY (dbus_service->connection != NULL))
        dbus_g_connection_unref (dbus_service->connection);
    
    g_object_unref (dbus_service->power);
    
    (*G_OBJECT_CLASS (xfpm_dbus_service_parent_class)->finalize) (object);
}



static gboolean 
xfpm_dbus_service_suspend (XfpmDBusService  *dbus_service, 
                           GError          **error)
{
    xfpm_power_suspend (dbus_service->power, FALSE);
    return TRUE;
}



static gboolean 
xfpm_dbus_service_hibernate (XfpmDBusService  *dbus_service, 
                             GError          **error)
{
    xfpm_power_hibernate (dbus_service->power, FALSE);
    return TRUE;
}



static gboolean 
xfpm_dbus_service_can_suspend (XfpmDBusService  *dbus_service, 
                               gboolean         *OUT_can_suspend,
                               GError          **error)
{
    return xfpm_power_can_suspend (dbus_service->power, OUT_can_suspend, error);
}



static gboolean
xfpm_dbus_service_can_hibernate (XfpmDBusService  *dbus_service, 
                                 gboolean         *OUT_can_hibernate,
                                 GError          **error)
{
    return xfpm_power_can_hibernate (dbus_service->power, OUT_can_hibernate, error);
}



static gboolean 
xfpm_dbus_service_get_on_battery (XfpmDBusService  *dbus_service, 
                                  gboolean         *OUT_on_battery, 
                                  GError          **error)
{
    return xfpm_power_get_on_battery (dbus_service->power, OUT_on_battery, error);
}



static gboolean 
xfpm_dbus_service_get_low_battery (XfpmDBusService  *dbus_service, 
                                   gboolean         *OUT_low_battery, 
                                   GError          **error)
{
    return xfpm_power_get_low_battery (dbus_service->power, OUT_low_battery, error);
}



static gboolean 
xfpm_dbus_service_terminate (XfpmDBusService *dbus_service, 
                             gboolean         restart, 
                             GError         **error)
{
    gtk_main_quit ();
    //TODO restart
    return TRUE;
}



XfpmDBusService *
xfpm_dbus_service_new (void)
{
    return g_object_new (XFPM_TYPE_DBUS_SERVICE, NULL);
}
