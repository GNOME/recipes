/* gr-recipe.h:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
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
#include "gr-diet.h"
#include "gr-number.h"

G_BEGIN_DECLS

#define GR_TYPE_RECIPE (gr_recipe_get_type())

G_DECLARE_FINAL_TYPE (GrRecipe, gr_recipe, GR, RECIPE, GObject)

GrRecipe       *gr_recipe_new              (void);

const char     *gr_recipe_get_id           (GrRecipe   *recipe);
const char     *gr_recipe_get_name         (GrRecipe   *recipe);
const char     *gr_recipe_get_author       (GrRecipe   *recipe);
const char     *gr_recipe_get_description  (GrRecipe   *recipe);
const char     *gr_recipe_get_cuisine      (GrRecipe   *recipe);
const char     *gr_recipe_get_season       (GrRecipe   *recipe);
const char     *gr_recipe_get_category     (GrRecipe   *recipe);
const char     *gr_recipe_get_prep_time    (GrRecipe   *recipe);
const char     *gr_recipe_get_cook_time    (GrRecipe   *recipe);
GrDiets         gr_recipe_get_diets        (GrRecipe   *recipe);
const char     *gr_recipe_get_ingredients  (GrRecipe   *recipe);
const char     *gr_recipe_get_instructions (GrRecipe   *recipe);
const char     *gr_recipe_get_notes        (GrRecipe   *recipe);
gboolean        gr_recipe_contains_garlic  (GrRecipe   *recipe);
gboolean        gr_recipe_contains_dairy   (GrRecipe   *recipe);
gboolean        gr_recipe_contains_gluten  (GrRecipe   *recipe);
int             gr_recipe_get_spiciness    (GrRecipe   *recipe);
int             gr_recipe_get_default_image (GrRecipe   *recipe);
GDateTime      *gr_recipe_get_ctime        (GrRecipe   *recipe);
GDateTime      *gr_recipe_get_mtime        (GrRecipe   *recipe);
gboolean        gr_recipe_is_readonly      (GrRecipe   *recipe);
gboolean        gr_recipe_is_contributed   (GrRecipe   *recipe);
GPtrArray      *gr_recipe_get_images       (GrRecipe   *recipe);
const char     *gr_recipe_get_yield_unit   (GrRecipe   *recipe);
double          gr_recipe_get_yield        (GrRecipe   *recipe);

const char     *gr_recipe_get_translated_name         (GrRecipe   *recipe);
const char     *gr_recipe_get_translated_description  (GrRecipe   *recipe);
const char     *gr_recipe_get_translated_instructions (GrRecipe   *recipe);
const char     *gr_recipe_get_translated_notes        (GrRecipe   *recipe);

gboolean        gr_recipe_matches          (GrRecipe    *recipe,
                                            const char **terms);

G_END_DECLS
