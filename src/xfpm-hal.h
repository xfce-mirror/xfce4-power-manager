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

#ifndef __XFPM_HAL_H
#define __XFPM_HAL_H

#include <glib.h>

#include <hal/libhal.h>

#define HAL_DBUS_SERVICE             "org.freedesktop.Hal"
#define HAL_ROOT_COMPUTER	         "/org/freedesktop/Hal/devices/computer"
#define	HAL_DBUS_INTERFACE_POWER	 "org.freedesktop.Hal.Device.SystemPowerManagement"
#define HAL_DBUS_INTERFACE_CPU       "org.freedesktop.Hal.Device.CPUFreq"
#define HAL_DBUS_INTERFACE_LCD       "org.freedesktop.Hal.Device.LaptopPanel"

G_BEGIN_DECLS

#define XFPM_TYPE_HAL                  (xfpm_hal_get_type ())
#define XFPM_HAL(o)                    (G_TYPE_CHECK_INSTANCE_CAST((o),XFPM_TYPE_HAL,XfpmHal))
#define XFPM_HAL_CLASS(k)              (G_TYPE_CHECK_CLASS_CAST((k),XFPM_TYPE_HAL,XfpmHalClass))
#define XFPM_IS_HAL(o)                 (G_TYPE_CHECK_INSTANCE_TYPE((o),XFPM_TYPE_HAL))
#define XFPM_IS_HAL_CLASS(k)           (G_TYPE_CHECK_CLASS_TYPE((k),XFPM_TYPE_HAL))
#define XFPM_HAL_GET_CLASS(k)          (G_TYPE_INSTANCE_GET_CLASS((k),XFPM_TYPE_HAL,XfpmHalClass))

typedef struct XfpmHalPrivate XfpmHalPrivate;

typedef struct {
    
    GObject        parent;
    XfpmHalPrivate *priv;
    
} XfpmHal;     

typedef struct {
    
    GObjectClass parent_class;
    
    /*signals*/
    void         (*device_added)              (XfpmHal *xfpm_hal,
                                              const gchar *udi);
    
    void         (*device_removed)            (XfpmHal *xfpm_hal,
                                              const gchar *udi);
    
    void         (*device_property_changed)   (XfpmHal *xfpm_hal,
                                              const gchar *udi,
                                              const gchar *key,
                                              gboolean is_removed,
                                              gboolean is_added);
                                              
    void         (*device_condition)          (XfpmHal *xfpm_hal,
                                              const gchar *udi,
                                              const gchar *condition_name,
                                              const gchar *condition_detail);
                                                                                        
} XfpmHalClass;    

GType                xfpm_hal_get_type                     (void);
XfpmHal             *xfpm_hal_new                          (void);
gboolean             xfpm_hal_is_connected                 (XfpmHal *hal);
gboolean             xfpm_hal_power_management_can_be_used (XfpmHal *hal);
gboolean 			 xfpm_hal_cpu_freq_interface_can_be_used(XfpmHal *hal);
gboolean             xfpm_hal_connect_to_signals           (XfpmHal *hal,
                                                            gboolean device_removed,
                                                            gboolean device_added,
                                                            gboolean device_property_changed,
                                                            gboolean device_condition);
                                                            
gchar              **xfpm_hal_get_device_udi_by_capability (XfpmHal *xfpm_hal,
                                                             const gchar *capability,
                                                             gint *num,
                                                             GError **gerror);
gint32               xfpm_hal_get_int_info                 (XfpmHal *xfpm_hal,
                                                             const gchar *udi,
                                                             const gchar *property,
                                                             GError **gerror);    
gchar                *xfpm_hal_get_string_info              (XfpmHal *xfpm_hal,
                                                            const gchar *udi,
                                                            const gchar *property,
                                                            GError **gerror); 
gboolean             xfpm_hal_get_bool_info                (XfpmHal *xfpm_hal,
                                                            const gchar *udi,
                                                            const gchar *property,
                                                            GError **gerror); 
gboolean             xfpm_hal_device_have_key              (XfpmHal *xfpm_hal,
                                                            const gchar *udi,
                                                            const gchar *key);
gboolean             xfpm_hal_device_have_capability       (XfpmHal *xfpm_hal,
                                                            const gchar *udi,
                                                            const gchar *capability);
                                                            
gboolean             xfpm_hal_shutdown                     (XfpmHal *xfpm_hal);
gboolean             xfpm_hal_hibernate                    (XfpmHal *xfpm_hal,
                                                            GError **gerror,
                                                            guint8 *critical);
gboolean             xfpm_hal_suspend                      (XfpmHal *xfpm_hal,
                                                            GError **gerror,
                                                            guint8 *critical);
void                 xfpm_hal_set_brightness               (XfpmHal *xfpm_hal,
                                                            const gchar *interface,
                                                            gint level32,
                                                            GError **gerror);
gint32               xfpm_hal_get_brightness               (XfpmHal *xfpm_hal,
                                                            const gchar *interface,
                                                            GError **gerror);                                                            

gchar              **xfpm_hal_get_available_cpu_governors  (XfpmHal *xfpm_hal,
                                                            GError **gerror);
gchar               *xfpm_hal_get_current_cpu_governor     (XfpmHal *xfpm_hal,
                                                            GError **gerror);
void                 xfpm_hal_set_cpu_governor             (XfpmHal *xfpm_hal,
                                                            const gchar *governor,
                                                            GError **gerror);
void                 xfpm_hal_set_power_save               (XfpmHal *xfpm_hal,
                                                            gboolean power_save,
                                                            GError **gerror);
G_END_DECLS

#endif /* __XFPM_HAL_H */
