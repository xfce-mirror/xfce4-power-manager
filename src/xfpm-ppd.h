/*
 * * Copyright (C) 2023 Elliot <BlindRepublic@mailo.com>
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

#ifndef __XFPM_PPD_H
#define __XFPM_PPD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_PPD        ( xfpm_ppd_get_type () )
#define XFPM_PPD(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFPM_TYPE_PPD, XfpmPPD))
#define XFPM_IS_PPD(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFPM_TYPE_PPD))

typedef struct XfpmPPDPrivate XfpmPPDPrivate;

typedef struct
{
  GObject                parent;
  XfpmPPDPrivate        *priv;

} XfpmPPD;

typedef struct
{
  GObjectClass                parent_class;

} XfpmPPDClass;

GType                  xfpm_ppd_get_type           (void) G_GNUC_CONST;
XfpmPPD               *xfpm_ppd_new                (void);

G_END_DECLS

#endif /* __XFPM_PROFILES_H */
