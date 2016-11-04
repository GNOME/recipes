/* gr-ingredient-search-tile.h:
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

G_BEGIN_DECLS

#define GR_TYPE_INGREDIENT_SEARCH_TILE (gr_ingredient_search_tile_get_type ())

G_DECLARE_FINAL_TYPE (GrIngredientSearchTile, gr_ingredient_search_tile, GR, INGREDIENT_SEARCH_TILE, GtkBox)

GtkWidget       *gr_ingredient_search_tile_new            (const char             *ingredient);
const char      *gr_ingredient_search_tile_get_ingredient (GrIngredientSearchTile *tile);
gboolean         gr_ingredient_search_tile_get_excluded   (GrIngredientSearchTile *tile);

G_END_DECLS
