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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XFPM_POPUPS_H
#define __XFPM_POPUPS_H

void 	xfpm_popup_message	(const gchar *title,
				 const gchar *message,
				 GtkMessageType message_type);


void 	xfpm_info 		(const gchar *title, 
				 const gchar *message);
				 
void    xfpm_error              (const gchar *title,
				 const gchar *message);

#endif /* __XFPM_POPUPS_H */
