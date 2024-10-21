/*
 * * Copyright (C) 2024 Andrzej Radecki <andrzejr@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __POWER_MANAGER_PLUGIN_H__
#define __POWER_MANAGER_PLUGIN_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

G_BEGIN_DECLS
typedef struct _PowerManagerPluginClass PowerManagerPluginClass;
typedef struct _PowerManagerPlugin PowerManagerPlugin;

#define POWER_MANAGER_TYPE_PLUGIN (power_manager_plugin_get_type ())
#define POWER_MANAGER_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), POWER_MANAGER_TYPE_PLUGIN, PowerManagerPlugin))
#define POWER_MANAGER_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), POWER_MANAGER_TYPE_PLUGIN, PowerManagerPluginClass))
#define POWER_MANAGER_IS_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POWER_MANAGER_TYPE_PLUGIN))
#define POWER_MANAGER_IS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), POWER_MANAGER_TYPE_PLUGIN))
#define POWER_MANAGER_PLUGIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), POWER_MANAGER_TYPE_PLUGIN, PowerManagerPluginClass))

GType
power_manager_plugin_get_type (void) G_GNUC_CONST;

void
power_manager_plugin_register_type (XfcePanelTypeModule *type_module);

G_END_DECLS

#endif /* !__POWER_MANAGER_PLUGIN_H__ */
