/* gr-details-page.h:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#pragma once

#include <gtk/gtk.h>

#include "gr-recipe.h"

G_BEGIN_DECLS

#define GR_TYPE_DETAILS_PAGE (gr_details_page_get_type ())

G_DECLARE_FINAL_TYPE (GrDetailsPage, gr_details_page, GR, DETAILS_PAGE, GtkBox)

GtkWidget	*gr_details_page_new (void);

void             gr_details_page_set_recipe (GrDetailsPage *page,
                                             GrRecipe      *recipe);

gboolean         gr_details_page_is_cooking  (GrDetailsPage *page);
void             gr_details_page_set_cooking (GrDetailsPage *page,
					      gboolean       cooking);

void             gr_details_page_show_timers (GrDetailsPage *page);

G_END_DECLS
