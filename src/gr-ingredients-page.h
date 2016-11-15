/* gr-ingredients-page.h:
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
 * GNU General Public License for more edit.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include "gr-recipe.h"
#include "gr-chef.h"

G_BEGIN_DECLS

#define GR_TYPE_INGREDIENTS_PAGE (gr_ingredients_page_get_type ())

G_DECLARE_FINAL_TYPE (GrIngredientsPage, gr_ingredients_page, GR, INGREDIENTS_PAGE, GtkBox)

GtkWidget	*gr_ingredients_page_new (void);

char            *gr_ingredients_page_get_search_terms (GrIngredientsPage *page);
void             gr_ingredients_page_scroll           (GrIngredientsPage *page,
                                                       const char        *str);

G_END_DECLS
