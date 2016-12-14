/* gr-category-tile.h:
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

#define GR_TYPE_CATEGORY_TILE (gr_category_tile_get_type ())

G_DECLARE_FINAL_TYPE (GrCategoryTile, gr_category_tile, GR, CATEGORY_TILE, GtkButton)

GtkWidget       *gr_category_tile_new            (GrDiets         diet);
GtkWidget       *gr_category_tile_new_with_label (const char     *category,
                                                  const char     *label);
GrDiets          gr_category_tile_get_diet       (GrCategoryTile *tile);
const char      *gr_category_tile_get_category   (GrCategoryTile *tile);
const char      *gr_category_tile_get_label      (GrCategoryTile *tile);

G_END_DECLS
