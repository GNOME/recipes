/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Rafał Lużyński <digitalfreak@lingonborough.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GS_HIDING_BOX_H_
#define GS_HIDING_BOX_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GS_TYPE_HIDING_BOX (gs_hiding_box_get_type ())

G_DECLARE_FINAL_TYPE (GsHidingBox, gs_hiding_box, GS, HIDING_BOX, GtkContainer)

GtkWidget	*gs_hiding_box_new		(void);
void		 gs_hiding_box_set_spacing	(GsHidingBox	*box,
						 gint		 spacing);
gint		 gs_hiding_box_get_spacing	(GsHidingBox	*box);

G_END_DECLS

#endif /* GS_HIDING_BOX_H_ */

/* vim: set noexpandtab: */
