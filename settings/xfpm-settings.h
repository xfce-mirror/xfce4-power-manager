/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
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

#ifndef __XFPM_SETTINGS_H
#define __XFPM_SETTINGS_H

#include <gtk/gtkx.h>
#include <xfconf/xfconf.h>

GtkWidget *xfpm_settings_dialog_new (XfconfChannel *channel,
                                     gboolean auth_suspend,
                                     gboolean auth_hibernate,
                                     gboolean can_suspend,
                                     gboolean can_hibernate,
                                     gboolean can_shutdown,
                                     gboolean has_battery,
                                     gboolean has_lcd_brightness,
                                     gboolean has_lid,
                                     gboolean has_sleep_button,
                                     gboolean has_hibernate_button,
                                     gboolean has_power_button,
                                     Window id,
                                     gchar *device_id);

#endif /* __XFPM_SETTINGS_H */
