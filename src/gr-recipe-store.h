/* gr-recipe-store.h:
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
#include "gr-recipe.h"
#include "gr-chef.h"

G_BEGIN_DECLS

#define GR_TYPE_RECIPE_STORE (gr_recipe_store_get_type())

G_DECLARE_FINAL_TYPE (GrRecipeStore, gr_recipe_store, GR, RECIPE_STORE, GObject)

GrRecipeStore  *gr_recipe_store_get                 (void);

GrRecipeStore  *gr_recipe_store_new                 (void);

gboolean        gr_recipe_store_add_recipe          (GrRecipeStore  *self,
                                                     GrRecipe       *recipe,
                                                     GError        **error);
gboolean        gr_recipe_store_update_recipe       (GrRecipeStore  *self,
                                                     GrRecipe       *recipe,
                                                     const char     *old_id,
                                                     GError        **error);
gboolean        gr_recipe_store_remove_recipe       (GrRecipeStore  *self,
                                                     GrRecipe       *recipe);
GrRecipe       *gr_recipe_store_get_recipe          (GrRecipeStore  *self,
                                                     const char     *id);
char          **gr_recipe_store_get_recipe_keys     (GrRecipeStore  *self,
                                                     guint          *length);
gboolean        gr_recipe_store_recipe_is_todays    (GrRecipeStore  *self,
                                                     GrRecipe       *recipe);
gboolean        gr_recipe_store_recipe_is_pick      (GrRecipeStore  *self,
                                                     GrRecipe       *recipe);

gboolean        gr_recipe_store_add_chef            (GrRecipeStore  *self,
                                                     GrChef         *chef,
                                                     GError        **error);
gboolean        gr_recipe_store_update_chef         (GrRecipeStore  *self,
                                                     GrChef         *chef,
                                                     const char     *old_id,
                                                     GError        **error);
GrChef         *gr_recipe_store_get_chef            (GrRecipeStore  *self,
                                                     const char     *id);
char          **gr_recipe_store_get_chef_keys       (GrRecipeStore  *self,
                                                     guint          *length);
gboolean        gr_recipe_store_chef_is_featured    (GrRecipeStore  *self,
                                                     GrChef         *chef);

const char     *gr_recipe_store_get_user_key        (GrRecipeStore  *self);
gboolean        gr_recipe_store_update_user         (GrRecipeStore  *self,
                                                     GrChef         *chef,
                                                     GError        **error);

char          **gr_recipe_store_get_all_ingredients (GrRecipeStore  *self,
                                                     guint          *length);

void            gr_recipe_store_add_favorite        (GrRecipeStore  *self,
                                                     GrRecipe       *recipe);
void            gr_recipe_store_remove_favorite     (GrRecipeStore  *self,
                                                     GrRecipe       *recipe);
gboolean        gr_recipe_store_is_favorite         (GrRecipeStore  *self,
                                                     GrRecipe       *recipe);
GDateTime      *gr_recipe_store_last_favorite_change (GrRecipeStore *self);

void            gr_recipe_store_add_export          (GrRecipeStore  *self,
                                                     GrRecipe       *recipe);
void            gr_recipe_store_remove_export       (GrRecipeStore  *self,
                                                     GrRecipe       *recipe);
void            gr_recipe_store_clear_export_list   (GrRecipeStore  *self);
const char    **gr_recipe_store_get_export_list     (GrRecipeStore  *self);

void            gr_recipe_store_add_to_shopping      (GrRecipeStore  *self,
                                                      GrRecipe       *recipe,
                                                      double          yield);
void            gr_recipe_store_remove_from_shopping (GrRecipeStore  *self,
                                                      GrRecipe       *recipe);
void            gr_recipe_store_clear_shopping_list  (GrRecipeStore *self);
gboolean        gr_recipe_store_is_in_shopping       (GrRecipeStore  *self,
                                                      GrRecipe       *recipe);
GList          *gr_recipe_store_get_shopping_list    (GrRecipeStore  *self);
double          gr_recipe_store_get_shopping_yield   (GrRecipeStore  *self,
                                                      GrRecipe       *recipe);
gboolean        gr_recipe_store_not_shopping_ingredient    (GrRecipeStore *self,
                                                            const char    *ingredient);
void            gr_recipe_store_remove_shopping_ingredient (GrRecipeStore *self,
                                                            const char    *ingredient);
void            gr_recipe_store_readd_shopping_ingredient  (GrRecipeStore *self,
                                                            const char    *ingredient);
const char    **gr_recipe_store_get_removed_shopping_ingredients (GrRecipeStore *self);
GDateTime      *gr_recipe_store_last_shopping_change (GrRecipeStore *self);

gboolean        gr_recipe_store_has_diet            (GrRecipeStore  *self,
                                                     GrDiets         diet);
gboolean        gr_recipe_store_has_chef            (GrRecipeStore  *self,
                                                     GrChef         *chef);
gboolean        gr_recipe_store_has_cuisine         (GrRecipeStore  *self,
                                                     const char     *cuisine);
char          **gr_recipe_store_get_contributors    (GrRecipeStore *self,
                                                     guint         *length);
char          **gr_recipe_store_get_all_cuisines    (GrRecipeStore *store,
                                                     guint         *length);

#define GR_TYPE_RECIPE_SEARCH (gr_recipe_search_get_type())

G_DECLARE_FINAL_TYPE (GrRecipeSearch, gr_recipe_search, GR, RECIPE_SEARCH, GObject)

GrRecipeSearch *gr_recipe_search_new       (void);

void            gr_recipe_search_set_query (GrRecipeSearch  *search,
                                            const char      *query);
void            gr_recipe_search_set_terms (GrRecipeSearch  *search,
                                            const char     **query);
const char    **gr_recipe_search_get_terms (GrRecipeSearch  *search);
void            gr_recipe_search_stop      (GrRecipeSearch  *search);

G_END_DECLS
