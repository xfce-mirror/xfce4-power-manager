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

#include "libxfpm/hal-manager.h"
#include "libxfpm/hal-device.h"
#include "libxfpm/xfpm-string.h"

#include "xfpm-lid-hal.h"

/* Init */
static void xfpm_lid_hal_class_init (XfpmLidHalClass *klass);
static void xfpm_lid_hal_init       (XfpmLidHal *lid);
static void xfpm_lid_hal_finalize   (GObject *object);

#define XFPM_LID_HAL_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), XFPM_TYPE_LID_HAL, XfpmLidHalPrivate))

struct XfpmLidHalPrivate
{
    HalManager *manager;
    HalDevice  *device;
    gboolean    hw_found;
};

enum
{
    LID_CLOSED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(XfpmLidHal, xfpm_lid_hal, G_TYPE_OBJECT)

static void
xfpm_lid_hal_class_init(XfpmLidHalClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);


    signals[LID_CLOSED] = 
        g_signal_new("lid-closed",
                      XFPM_TYPE_LID_HAL,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmLidHalClass, lid_closed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);
		      
    object_class->finalize = xfpm_lid_hal_finalize;


    g_type_class_add_private(klass,sizeof(XfpmLidHalPrivate));
}

static void
xfpm_lid_hal_init (XfpmLidHal *lid)
{
    lid->priv = XFPM_LID_HAL_GET_PRIVATE(lid);
    
    lid->priv->manager      = NULL;
    lid->priv->device       = NULL;
    lid->priv->hw_found     = FALSE;
}

static void
xfpm_lid_hal_finalize(GObject *object)
{
    XfpmLidHal *lid;

    lid = XFPM_LID_HAL(object);
    
    if ( lid->priv->manager )
    	g_object_unref (lid->priv->manager);

    if ( lid->priv->device )
    	g_object_unref (lid->priv->device);

    G_OBJECT_CLASS(xfpm_lid_hal_parent_class)->finalize(object);
}

static void
xfpm_lid_hal_device_changed_cb(HalDevice *device,
			       const gchar *udi,
                               const gchar *key, 
                               gboolean is_removed,
                               gboolean is_added,
			       XfpmLidHal *lid)
{
    if ( !xfpm_strequal (key, "button.state.value") )
    	return;
	
    TRACE("Property modified key=%s  key=%s\n", key, udi);
    
    gboolean pressed = hal_manager_get_device_property_bool (lid->priv->manager, udi, key);
    
    if ( pressed )
    {
	TRACE("Emitting signal lid closed");
	g_signal_emit ( G_OBJECT(lid), signals[LID_CLOSED], 0);
    }
    
}

static gboolean
xfpm_lid_hal_setup (XfpmLidHal *lid)
{
    lid->priv->manager = hal_manager_new ();
    
    gchar **udi = NULL;
    
    udi = hal_manager_find_device_by_capability (lid->priv->manager, "button");
    
    if ( !udi )
    	return FALSE;
	
    int i;
    
    for ( i = 0; udi[i]; i++ )
    {
	if ( hal_manager_device_has_key (lid->priv->manager, udi[i], "button.type" ) &&
	     hal_manager_device_has_key (lid->priv->manager, udi[i], "button.has_state" ) )
    	{
	    gchar *button_type =
	    	hal_manager_get_device_property_string (lid->priv->manager, udi[i], "button.type");
		
	    if ( !button_type )
	    	continue;
		
	    if ( xfpm_strequal (button_type, "lid") )
	    {
	    	lid->priv->hw_found = TRUE;
		lid->priv->device = hal_device_new (udi[i]);
		g_signal_connect (lid->priv->device, "device-changed", 
				  G_CALLBACK(xfpm_lid_hal_device_changed_cb), lid);
		if ( !hal_device_watch (lid->priv->device) )
		    g_critical ("Unable to watch lid button device: %s\n", udi[i]);
		
		g_free(button_type);
		TRACE ("Found lid switch on device: %s\n", udi[i]);
		break;
	    }
	}
    }
        
    hal_manager_free_string_array (udi);
    return TRUE;
}

XfpmLidHal *
xfpm_lid_hal_new(void)
{
    XfpmLidHal *lid = NULL;
    lid = g_object_new (XFPM_TYPE_LID_HAL, NULL);
    
    xfpm_lid_hal_setup (lid);

    return lid;
}

gboolean xfpm_lid_hw_found (XfpmLidHal *lid)
{
    g_return_val_if_fail (XFPM_IS_LID_HAL(lid), FALSE);
    
    return lid->priv->hw_found;
    
}
