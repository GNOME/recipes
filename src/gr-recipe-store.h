/* gr-recipe-store.h
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
#include "gr-recipe.h"
#include "gr-chef.h"

G_BEGIN_DECLS

#define GR_TYPE_RECIPE_STORE (gr_recipe_store_get_type())

G_DECLARE_FINAL_TYPE (GrRecipeStore, gr_recipe_store, GR, RECIPE_STORE, GObject)

GrRecipeStore *gr_recipe_store_new (void);

gboolean gr_recipe_store_add (GrRecipeStore  *self,
                              GrRecipe       *recipe,
                              GError        **error);
gboolean gr_recipe_store_update (GrRecipeStore  *self,
                                 GrRecipe       *recipe,
                                 const char     *old_name,
                                 GError        **error);
gboolean gr_recipe_store_remove (GrRecipeStore *self,
				 GrRecipe      *recipe);

GrRecipe *gr_recipe_store_get (GrRecipeStore *self,
                               const char    *name);

char **gr_recipe_store_get_recipe_keys (GrRecipeStore *self,
                                        guint         *length);

gboolean gr_recipe_store_is_todays (GrRecipeStore *self,
                                    GrRecipe      *recipe);

gboolean gr_recipe_store_is_pick (GrRecipeStore *self,
                                  GrRecipe      *recipe);

GrChef *gr_recipe_store_get_chef (GrRecipeStore *self,
                                  const char    *name);

char **gr_recipe_store_get_chef_keys (GrRecipeStore *self,
                                      guint         *length);

gboolean gr_recipe_store_chef_is_featured (GrRecipeStore *self,
                                           GrChef        *chef);

const char *gr_recipe_store_get_user_key (GrRecipeStore *self);

gboolean gr_recipe_store_update_user (GrRecipeStore  *self,
                                      GrChef         *chef,
                                      GError        **error);

G_END_DECLS
