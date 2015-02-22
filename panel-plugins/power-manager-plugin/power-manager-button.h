/*
 * * Copyright (C) 2014 Eric Koegel <eric@xfce.org>
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

#ifndef __POWER_MANAGER_BUTTON_H
#define __POWER_MANAGER_BUTTON_H

#include <glib-object.h>
#include <gtk/gtk.h>
#ifdef XFCE_PLUGIN
#include <libxfce4panel/xfce-panel-plugin.h>
#endif
#ifdef LXDE_PLUGIN
#include <lxpanel/plugin.h>
#endif

G_BEGIN_DECLS

#define POWER_MANAGER_TYPE_BUTTON        (power_manager_button_get_type () )
#define POWER_MANAGER_BUTTON(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), POWER_MANAGER_TYPE_BUTTON, PowerManagerButton))
#define POWER_MANAGER_IS_BUTTON(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), POWER_MANAGER_TYPE_BUTTON))

typedef struct PowerManagerButtonPrivate PowerManagerButtonPrivate;

typedef struct
{
    GtkToggleButton            parent;
    PowerManagerButtonPrivate *priv;

} PowerManagerButton;

typedef struct
{
    GtkToggleButtonClass parent_class;

    /*< Signals >*/
    void (*tooltip_changed)  (PowerManagerButton *button);
    void (*icon_name_changed)(PowerManagerButton *button);
} PowerManagerButtonClass;

GType                    power_manager_button_get_type (void) G_GNUC_CONST;

#ifdef XFCE_PLUGIN
GtkWidget               *power_manager_button_new       (XfcePanelPlugin *plugin);
#endif
#ifdef LXDE_PLUGIN
GtkWidget               *power_manager_button_new       (void);
#endif
#ifdef XFPM_SYSTRAY
GtkWidget               *power_manager_button_new       (void);
#endif

void                     power_manager_button_show      (PowerManagerButton *button);

void                     power_manager_button_set_width (PowerManagerButton *button, gint width);

void                     power_manager_button_show_menu (PowerManagerButton *button);

const gchar             *power_manager_button_get_icon_name (PowerManagerButton *button);
const gchar             *power_manager_button_get_tooltip   (PowerManagerButton *button);

G_END_DECLS

#endif /* __POWER_MANAGER_BUTTON_H */
