/* -*- c-basic-offset: 4 -*- vi:set ts=4 sts=4 sw=4:
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
 * * Copyright (C) 2015 Xfce Development Team <xfce4-dev@xfce.org>
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
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfpm-settings-app.h"

int main (int argc, char **argv)
{

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    return g_application_run (G_APPLICATION (xfpm_settings_app_new ()), argc, argv);
}
