/* gr-recipe-small-tile.h:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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

#define GR_TYPE_RECIPE_SMALL_TILE (gr_recipe_small_tile_get_type ())

G_DECLARE_FINAL_TYPE (GrRecipeSmallTile, gr_recipe_small_tile, GR, RECIPE_SMALL_TILE, GtkButton)

GtkWidget      *gr_recipe_small_tile_new        (GrRecipe          *recipe,
                                                 int                serves);
GrRecipe       *gr_recipe_small_tile_get_recipe (GrRecipeSmallTile *tile);
int             gr_recipe_small_tile_get_serves (GrRecipeSmallTile *tile);
void            gr_recipe_small_tile_set_serves (GrRecipeSmallTile *tile,
                                                 int                serves);

G_END_DECLS
