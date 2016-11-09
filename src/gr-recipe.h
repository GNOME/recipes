/* gr-recipe.h
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GR_TYPE_RECIPE (gr_recipe_get_type())

G_DECLARE_DERIVABLE_TYPE (GrRecipe, gr_recipe, GR, RECIPE, GObject)

struct _GrRecipeClass
{
	GObjectClass parent;
};

typedef enum { /*< flags >*/
	GR_DIET_GLUTEN_FREE   =  1,
	GR_DIET_NUT_FREE      =  2,
	GR_DIET_VEGAN         =  4,
	GR_DIET_VEGETARIAN    =  8,
	GR_DIET_MILK_FREE     = 16
} GrDiets;

GrRecipe *gr_recipe_new (void);

const char *gr_recipe_get_name         (GrRecipe   *recipe);
const char *gr_recipe_get_author       (GrRecipe   *recipe);
const char *gr_recipe_get_description  (GrRecipe   *recipe);
int         gr_recipe_get_serves       (GrRecipe   *recipe);
const char *gr_recipe_get_cuisine      (GrRecipe   *recipe);
const char *gr_recipe_get_category     (GrRecipe   *recipe);
const char *gr_recipe_get_prep_time    (GrRecipe   *recipe);
const char *gr_recipe_get_cook_time    (GrRecipe   *recipe);
GrDiets     gr_recipe_get_diets        (GrRecipe   *recipe);
const char *gr_recipe_get_ingredients  (GrRecipe   *recipe);
const char *gr_recipe_get_instructions (GrRecipe   *recipe);
const char *gr_recipe_get_notes        (GrRecipe   *recipe);

gboolean    gr_recipe_matches          (GrRecipe   *recipe,
				        const char *term);

G_END_DECLS
