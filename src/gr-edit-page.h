/* gr-edit-page.h:
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include "gr-recipe.h"

G_BEGIN_DECLS

#define GR_TYPE_EDIT_PAGE (gr_edit_page_get_type ())

G_DECLARE_FINAL_TYPE (GrEditPage, gr_edit_page, GR, EDIT_PAGE, GtkBox)

GtkWidget       *gr_edit_page_new (void);

void             gr_edit_page_clear (GrEditPage *page);
void             gr_edit_page_edit  (GrEditPage *page,
                                     GrRecipe   *recipe);
gboolean         gr_edit_page_save  (GrEditPage *page);
GrRecipe        *gr_edit_page_get_recipe (GrEditPage *page);

G_END_DECLS
