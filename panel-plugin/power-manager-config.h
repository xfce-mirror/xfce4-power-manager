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

#ifndef __POWER_MANAGER_CONFIG_H__
#define __POWER_MANAGER_CONFIG_H__

#include "power-manager-plugin.h"

#include <glib-object.h>
#include <libxfce4panel/libxfce4panel.h>

G_BEGIN_DECLS

#define POWER_MANAGER_TYPE_CONFIG (power_manager_config_get_type ())
G_DECLARE_FINAL_TYPE (PowerManagerConfig, power_manager_config, POWER_MANAGER, CONFIG, GObject)

PowerManagerConfig *
power_manager_config_new (PowerManagerPlugin *plugin);

gint
power_manager_config_get_show_panel_label (PowerManagerConfig *config);

gboolean
power_manager_config_get_presentation_mode (PowerManagerConfig *config);

gboolean
power_manager_config_get_show_presentation_indicator (PowerManagerConfig *config);

G_END_DECLS

#endif /* !__POWER_MANAGER_CONFIG_H__ */
