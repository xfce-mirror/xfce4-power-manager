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

#ifndef __POWER_MANAGER_DIALOG_H__
#define __POWER_MANAGER_DIALOG_H__

#include "power-manager-config.h"
#include "power-manager-plugin.h"

G_BEGIN_DECLS
typedef struct _PowerManagerDialogClass PowerManagerDialogClass;
typedef struct _PowerManagerDialog PowerManagerDialog;

#define POWER_MANAGER_TYPE_DIALOG (power_manager_dialog_get_type ())
#define POWER_MANAGER_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), POWER_MANAGER_TYPE_DIALOG, PowerManagerDialog))
#define POWER_MANAGER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), POWER_MANAGER_TYPE_DIALOG, PowerManagerDialogClass))
#define POWER_MANAGER_IS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POWER_MANAGER_TYPE_DIALOG))
#define POWER_MANAGER_IS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), POWER_MANAGER_TYPE_DIALOG))
#define POWER_MANAGER_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), POWER_MANAGER_TYPE_DIALOG, PowerManagerDialogClass))

GType
power_manager_dialog_get_type (void) G_GNUC_CONST;

PowerManagerDialog *
power_manager_dialog_new (PowerManagerPlugin *plugin,
                          PowerManagerConfig *config);

void
power_manager_dialog_show (PowerManagerDialog *dialog,
                           GdkScreen *screen);

G_END_DECLS

#endif /* !__POWER_MANAGER_DIALOG_H__ */
