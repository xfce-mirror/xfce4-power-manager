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

#ifndef __HAL_CTX_H
#define __HAL_CTX_H

#include <glib-object.h>

#include <hal/libhal.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define HAL_TYPE_CTX        (hal_ctx_get_type () )
#define HAL_CTX(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), HAL_TYPE_CTX, HalCtx))
#define HAL_IS_CTX(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), HAL_TYPE_CTX))

#define HAL_CTX_CHECK_DBUS_ERROR(_dbus_error_)		\
    if ( dbus_error_is_set(&_dbus_error_) )		\
    {							\
	g_critical("%s", _dbus_error_.message);		\
	dbus_error_free(&_dbus_error_);			\
    }

typedef struct HalCtxPrivate HalCtxPrivate;

typedef struct
{
    GObject		parent;
    HalCtxPrivate      *priv;
    
} HalCtx;

typedef struct
{
    GObjectClass 	parent_class;
    
} HalCtxClass;

GType         hal_ctx_get_type        			(void) G_GNUC_CONST;
HalCtx       *hal_ctx_new             			(void);
gboolean      hal_ctx_connect         			(HalCtx *ctx);

void          hal_ctx_set_user_data			(HalCtx *ctx,
							 void *data);
void          hal_ctx_set_device_added_callback		(HalCtx *ctx,
							 LibHalDeviceAdded callback);
void          hal_ctx_set_device_removed_callback	(HalCtx *ctx,
							 LibHalDeviceRemoved callback);
							 
void          hal_ctx_set_device_property_callback	(HalCtx *ctx,
							 LibHalDevicePropertyModified callback);
							 
gboolean      hal_ctx_watch_all 			(HalCtx *ctx);
gboolean      hal_ctx_watch_device 			(HalCtx *ctx,
							 const gchar *udi);

gchar       **hal_ctx_get_all_devices                   (HalCtx *ctx);
gchar       **hal_ctx_get_device_by_capability		(HalCtx *ctx,
							 const gchar *capability,
							 gint *number);
gboolean      hal_ctx_device_has_capability		(HalCtx *ctx,
							 const gchar *udi,
							 const gchar *capability);
gint32        hal_ctx_get_property_int			(HalCtx *ctx,
							 const gchar *udi,
							 const gchar *key);
gboolean      hal_ctx_get_property_bool			(HalCtx *ctx,
							 const gchar *udi,
							 const gchar *key);
gchar        *hal_ctx_get_property_string               (HalCtx *ctx,
							 const gchar *udi,
							 const gchar *key);
gboolean      hal_ctx_device_has_key			(HalCtx *ctx,
							 const gchar *udi,
							 const gchar *key);
G_END_DECLS

#endif /* __HAL_CTX_H */
