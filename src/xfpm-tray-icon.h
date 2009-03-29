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

#ifndef __XFPM_TRAY_ICON_H
#define __XFPM_TRAY_ICON_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFPM_TYPE_TRAY_ICON        (xfpm_tray_icon_get_type () )
#define XFPM_TRAY_ICON(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), XFPM_TYPE_TRAY_ICON, XfpmTrayIcon))
#define XFPM_IS_TRAY_ICON(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), XFPM_TYPE_TRAY_ICON))

typedef struct XfpmTrayIconPrivate XfpmTrayIconPrivate;

typedef struct
{
    GObject		    parent;
    XfpmTrayIconPrivate    *priv;
	
} XfpmTrayIcon;

typedef struct
{
    GObjectClass            parent_class;
    
    void                    (*show_info)	   (XfpmTrayIcon *icon);
	
} XfpmTrayIconClass;

GType        	    xfpm_tray_icon_get_type        	(void) G_GNUC_CONST;
XfpmTrayIcon       *xfpm_tray_icon_new             	(void);

void                xfpm_tray_icon_set_show_info_menu 	(XfpmTrayIcon *icon,
						         gboolean value);

void                xfpm_tray_icon_set_icon        	(XfpmTrayIcon *icon,
							 const gchar *icon_name);
						    
void                xfpm_tray_icon_set_tooltip     	(XfpmTrayIcon *icon,
							 const gchar *tooltip);
						    
void                xfpm_tray_icon_set_visible    	(XfpmTrayIcon *icon,
							 gboolean visible);
						    
gboolean            xfpm_tray_icon_get_visible     	(XfpmTrayIcon *icon);

GtkStatusIcon      *xfpm_tray_icon_get_tray_icon   	(XfpmTrayIcon *icon) G_GNUC_PURE;

const gchar        *xfpm_tray_icon_get_icon_name   	(XfpmTrayIcon *icon) G_GNUC_PURE;  

G_END_DECLS

#endif /* __XFPM_TRAY_ICON_H */
