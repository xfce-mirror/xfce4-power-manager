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

#include <dbus/dbus-glib-lowlevel.h>

#include "hal-ctx.h"

/* Init */
static void hal_ctx_class_init (HalCtxClass *klass);
static void hal_ctx_init       (HalCtx *ctx);
static void hal_ctx_finalize   (GObject *object);

#define HAL_CTX_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE((o), HAL_TYPE_CTX, HalCtxPrivate))

struct HalCtxPrivate
{
    DBusGConnection *bus;
    LibHalContext   *context;
    
    gboolean         connected;
};

G_DEFINE_TYPE(HalCtx, hal_ctx, G_TYPE_OBJECT)

static void
hal_ctx_class_init(HalCtxClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = hal_ctx_finalize;

    g_type_class_add_private(klass,sizeof(HalCtxPrivate));
}

static void
hal_ctx_init(HalCtx *ctx)
{
    ctx->priv = HAL_CTX_GET_PRIVATE(ctx);
    
    ctx->priv->bus       = NULL;
    ctx->priv->context   = NULL;
    ctx->priv->connected = FALSE;
}

static void
hal_ctx_finalize(GObject *object)
{
    HalCtx *ctx;

    ctx = HAL_CTX(object);
    
    if ( ctx->priv->context )
    	libhal_ctx_free(ctx->priv->context);
	
    if ( ctx->priv->bus)
    	dbus_g_connection_unref(ctx->priv->bus);

    G_OBJECT_CLASS(hal_ctx_parent_class)->finalize(object);
}

HalCtx *
hal_ctx_new(void)
{
    HalCtx *ctx = NULL;
    ctx = g_object_new(HAL_TYPE_CTX,NULL);
    return ctx;
}

/*
 * Set HAL context assosiated with a DBus connection
 */
gboolean hal_ctx_connect (HalCtx *ctx)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), FALSE);

    GError *gerror = NULL;
    DBusError error;
    
    ctx->priv->bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &gerror);

    if ( gerror )
    {
	g_critical("Error connecting to the system bus: %s\n", gerror->message);
	g_error_free(gerror);
	return FALSE;
    }
    
    ctx->priv->context = libhal_ctx_new();
    libhal_ctx_set_dbus_connection(ctx->priv->context, 
				   dbus_g_connection_get_connection(ctx->priv->bus));
    
    dbus_error_init(&error);
    libhal_ctx_init(ctx->priv->context, &error);
    
    if ( dbus_error_is_set(&error) )
    {
	g_critical("Hal initialization failed: %s", error.message);
	dbus_error_free(&error);
	return FALSE;
    }
    
    ctx->priv->connected = TRUE;
    
    return TRUE;
}

/*
 * Set a user data to be collected on signals 
 * with libhal_ctx_get_user_data
 */
void hal_ctx_set_user_data (HalCtx *ctx, void *data)
{
    g_return_if_fail(HAL_IS_CTX(ctx));
    g_return_if_fail(ctx->priv->connected == TRUE);
    
    libhal_ctx_set_user_data(ctx->priv->context, data);
}

/*
 * Add a device-added callback
 */
void hal_ctx_set_device_added_callback (HalCtx *ctx, LibHalDeviceAdded callback)
{
    g_return_if_fail(HAL_IS_CTX(ctx));
    g_return_if_fail(ctx->priv->connected == TRUE);
    
    libhal_ctx_set_device_added(ctx->priv->context, callback);
}

/*
 * Add a device-removed callback
 */
void hal_ctx_set_device_removed_callback (HalCtx *ctx, LibHalDeviceRemoved callback)
{
    g_return_if_fail(HAL_IS_CTX(ctx));
    g_return_if_fail(ctx->priv->connected == TRUE);
    
    libhal_ctx_set_device_removed(ctx->priv->context, callback);
}

/*
 * Add a property-changed callback
 */
void hal_ctx_set_device_property_callback (HalCtx *ctx, LibHalDevicePropertyModified callback)
{
    g_return_if_fail(HAL_IS_CTX(ctx));
    g_return_if_fail(ctx->priv->connected == TRUE);
    
    libhal_ctx_set_device_property_modified(ctx->priv->context, callback);
}

/*
 * Watch all the devices monitored by HAL 
 * 
 */
gboolean hal_ctx_watch_all (HalCtx *ctx)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), FALSE);
    g_return_val_if_fail(ctx->priv->connected == TRUE, FALSE);
    
    DBusError error;
    dbus_error_init(&error);
    
    libhal_device_property_watch_all(ctx->priv->context, &error);
    
    HAL_CTX_CHECK_DBUS_ERROR(error)
    
    return TRUE;
}


/*
 * Add a watch for a unique device 
 */
gboolean hal_ctx_watch_device (HalCtx *ctx, const gchar *udi)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), FALSE);
    g_return_val_if_fail(ctx->priv->connected == TRUE, FALSE);
    
    DBusError error;
    dbus_error_init(&error);
    
    libhal_device_add_property_watch(ctx->priv->context,
				     udi,
				     &error);
				     
    HAL_CTX_CHECK_DBUS_ERROR(error)
    
    return TRUE;
}

/*
 * Get All the devices found by HAL
 */
gchar **hal_ctx_get_all_devices (HalCtx *ctx)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), NULL);
    g_return_val_if_fail(ctx->priv->connected == TRUE, NULL);
    
    gchar **udis = NULL;
    DBusError error;
    gint number;
    
    dbus_error_init(&error);
    
    udis = libhal_get_all_devices (ctx->priv->context,
				   &number,
				   &error);
				   
    HAL_CTX_CHECK_DBUS_ERROR(error)
    
    return udis;
}

/*
 * Get devices udi's from HAL by capability
 */
gchar **hal_ctx_get_device_by_capability (HalCtx *ctx, const gchar *capability, gint *number)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), NULL);
    g_return_val_if_fail(ctx->priv->connected == TRUE, NULL);
    
    gchar **udis = NULL;
    DBusError error;
    
    dbus_error_init(&error);
    
    udis = libhal_find_device_by_capability (ctx->priv->context,
					     capability,
					     number,
					     &error);
    
    HAL_CTX_CHECK_DBUS_ERROR(error)
    
    return udis;
}

gboolean hal_ctx_device_has_capability (HalCtx *ctx, const gchar *udi, const gchar *capability)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), FALSE);
    g_return_val_if_fail(ctx->priv->connected == TRUE, FALSE);
    
    DBusError error;
    gboolean ret;
    
    dbus_error_init(&error);
    
    ret = libhal_device_property_exists(ctx->priv->context,
					udi,
					"info.capabilities",
					&error);
    if ( dbus_error_is_set (&error) )
    {
	dbus_error_free (&error);
	return FALSE;
    }
    
    if ( !ret )
    	return FALSE;
    
    ret = libhal_device_query_capability(ctx->priv->context,
					 udi,
					 capability,
					 &error);
    HAL_CTX_CHECK_DBUS_ERROR (error)
    
    return ret;
    
}

gint32 hal_ctx_get_property_int (HalCtx *ctx, const gchar *udi, const gchar *key)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), 0);
    g_return_val_if_fail(ctx->priv->connected == TRUE, 0);
    
    DBusError error;
    
    dbus_error_init(&error);
    
    gint32 ret = libhal_device_get_property_int(ctx->priv->context,
						udi,
						key,
						&error);
    HAL_CTX_CHECK_DBUS_ERROR(error)
    
    return ret;
}

gboolean hal_ctx_get_property_bool (HalCtx *ctx, const gchar *udi, const gchar *key)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), FALSE);
    g_return_val_if_fail(ctx->priv->connected == TRUE, FALSE);
    
    DBusError error;
    
    dbus_error_init(&error);
    
    gboolean ret = libhal_device_get_property_bool(ctx->priv->context,
		   				   udi,
						   key,
						   &error);
    HAL_CTX_CHECK_DBUS_ERROR(error)
    
    return ret;
}

gchar *hal_ctx_get_property_string (HalCtx *ctx, const gchar *udi, const gchar *key)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), NULL);
    g_return_val_if_fail(ctx->priv->connected == TRUE, NULL);
    
    DBusError error;
    
    gchar *ret     = NULL;
    gchar *ret_str = NULL;
    
    dbus_error_init(&error);
    
    ret = libhal_device_get_property_string(ctx->priv->context,
				  	    udi,
					    key,
					    &error);
    
    HAL_CTX_CHECK_DBUS_ERROR(error)
    
    if ( ret )
    {
	ret_str = g_strdup(ret);
	libhal_free_string(ret);
    }
    
    return ret_str;
}

gboolean hal_ctx_device_has_key (HalCtx *ctx, const gchar *udi, const gchar *key)
{
    g_return_val_if_fail(HAL_IS_CTX(ctx), FALSE);
    g_return_val_if_fail(ctx->priv->connected == TRUE, FALSE);
    
      
    DBusError error;
    dbus_error_init(&error);
    gboolean ret;
    
    ret = libhal_device_property_exists(ctx->priv->context,
					udi,
					key,
					&error);
    HAL_CTX_CHECK_DBUS_ERROR(error)
					
    return ret;
}
