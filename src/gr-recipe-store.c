/* gr-recipe-store.c:
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

#include "config.h"

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-utils.h"
#include "gr-ingredients-list.h"
#include "gr-images.h"
#include "gr-app.h"


struct _GrRecipeStore
{
        GObject parent;

        GHashTable *recipes;
        GHashTable *chefs;
        GHashTable *cooked;

        char **todays;
        char **picks;
        char **favorites;
        char **shopping;
        int   *shopping_serves;
        char **featured_chefs;
        char *user;

        GDateTime *favorite_change;
        GDateTime *shopping_change;

        char *recipes_data;
        gsize recipes_data_length;
};


G_DEFINE_TYPE (GrRecipeStore, gr_recipe_store, G_TYPE_OBJECT)

static void
gr_recipe_store_finalize (GObject *object)
{
        GrRecipeStore *self = GR_RECIPE_STORE (object);

        g_clear_pointer (&self->recipes, g_hash_table_unref);
        g_clear_pointer (&self->chefs, g_hash_table_unref);
        g_clear_pointer (&self->cooked, g_hash_table_unref);
        g_clear_pointer (&self->favorite_change, g_date_time_unref);
        g_clear_pointer (&self->shopping_change, g_date_time_unref);
        g_strfreev (self->todays);
        g_strfreev (self->picks);
        g_strfreev (self->favorites);
        g_strfreev (self->shopping);
        g_strfreev (self->featured_chefs);
        g_free (self->user);
        g_free (self->recipes_data);

        G_OBJECT_CLASS (gr_recipe_store_parent_class)->finalize (object);
}

static gboolean
load_recipes (GrRecipeStore *self,
              const char    *dir,
              gboolean       readonly)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *path = NULL;
        g_auto(GStrv) groups = NULL;
        gsize length, length2, length3;
        int i, j;

        keyfile = g_key_file_new ();

        path = g_build_filename (dir, "recipes.db", NULL);

        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load recipe db: %s", error->message);
                else
                        g_message ("No recipe db at: %s", path);
                return FALSE;
        }
        g_message ("Load recipe db: %s", path);

        groups = g_key_file_get_groups (keyfile, &length);
        for (i = 0; i < length; i++) {
                GrRecipe *recipe;
                const char *id;
                g_autofree char *name = NULL;
                g_autofree char *author = NULL;
                g_autofree char *description = NULL;
                g_autofree char *cuisine = NULL;
                g_autofree char *season = NULL;
                g_autofree char *category = NULL;
                g_autofree char *prep_time = NULL;
                g_autofree char *cook_time = NULL;
                g_autofree char *ingredients = NULL;
                g_autofree char *instructions = NULL;
                g_autofree char *notes = NULL;
                g_autofree char *image_path = NULL;
                g_auto(GStrv) paths = NULL;
                g_autofree int *angles = NULL;
                int serves;
                int spiciness;
                int default_image = 0;
                GrDiets diets;
                g_autoptr(GArray) images = NULL;
                GrRotatedImage ri;
                g_autoptr(GDateTime) ctime = NULL;
                g_autoptr(GDateTime) mtime = NULL;
                char *tmp;

                g_clear_error (&error);

                id = groups[i];
                name = g_key_file_get_string (keyfile, groups[i], "Name", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        name = g_strdup ("unknown");
                        g_clear_error (&error);
                }
                author = g_key_file_get_string (keyfile, groups[i], "Author", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        author = g_strdup ("anonymous");
                        g_clear_error (&error);
                }
                description = g_key_file_get_string (keyfile, groups[i], "Description", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                cuisine = g_key_file_get_string (keyfile, groups[i], "Cuisine", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                season = g_key_file_get_string (keyfile, groups[i], "Season", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                category = g_key_file_get_string (keyfile, groups[i], "Category", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                prep_time = g_key_file_get_string (keyfile, groups[i], "PrepTime", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                cook_time = g_key_file_get_string (keyfile, groups[i], "CookTime", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                ingredients = g_key_file_get_string (keyfile, groups[i], "Ingredients", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                instructions = g_key_file_get_string (keyfile, groups[i], "Instructions", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                notes = g_key_file_get_string (keyfile, groups[i], "Notes", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                image_path = g_key_file_get_string (keyfile, groups[i], "Image", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                paths = g_key_file_get_string_list (keyfile, groups[i], "Images", &length2, &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                angles = g_key_file_get_integer_list (keyfile, groups[i], "Angles", &length3, &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                if (length2 != length3) {
                        g_warning ("Failed to load recipe %s: Images and Angles length mismatch", groups[i]);
                        continue;
                }
                default_image = g_key_file_get_integer (keyfile, groups[i], "DefaultImage", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                serves = g_key_file_get_integer (keyfile, groups[i], "Serves", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                spiciness = g_key_file_get_integer (keyfile, groups[i], "Spiciness", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                diets = g_key_file_get_integer (keyfile, groups[i], "Diets", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }

                if (image_path && image_path[0] != '\0' && image_path[0] != '/') {
                        tmp = g_build_filename (dir, image_path, NULL);
                        g_free (image_path);
                        image_path = tmp;
                }
                if (paths) {
                        for (j = 0; paths[j]; j++) {
                                if (paths[j][0] != '/') {
                                        tmp = g_build_filename (dir, paths[j], NULL);
                                        g_free (paths[j]);
                                        paths[j] = tmp;
                                }
                        }
                }

                images = gr_rotated_image_array_new ();
                if (paths) {
                        for (j = 0; paths[j]; j++) {
                                ri.path = g_strdup (paths[j]);
                                ri.angle = angles[j];
                                g_array_append_val (images, ri);
                        }
                }
                else if (image_path) {
                        ri.path = g_strdup (image_path);
                        ri.angle = 0;
                        g_array_append_val (images, ri);
                }

                tmp = g_key_file_get_string (keyfile, groups[i], "Created", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                if (tmp) {
                        ctime = date_time_from_string (tmp);
                        g_free (tmp);
                        if (!ctime) {
                                g_warning ("Failed to load recipe %s: Couldn't parse Created key", groups[i]);
                                continue;
                        }
                }
                else {
                        ctime = g_date_time_new_now_utc ();
                }

                tmp = g_key_file_get_string (keyfile, groups[i], "Modified", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                if (tmp) {
                        mtime = date_time_from_string (tmp);
                        g_free (tmp);
                        if (!mtime) {
                                g_warning ("Failed to load recipe %s: Couldn't parse Modified key", groups[i]);
                                continue;
                        }
                }
                else {
                        mtime = g_date_time_new_now_utc ();
                }

                recipe = g_hash_table_lookup (self->recipes, id);
                if (recipe) {
                        if (gr_recipe_is_readonly (recipe))
                                g_object_set (recipe,
                                              "notes", notes,
                                              NULL);
                        else
                                g_object_set (recipe,
                                              "id", id,
                                              "name", name,
                                              "author", author,
                                              "description", description,
                                              "cuisine", cuisine,
                                              "season", season,
                                              "category", category,
                                              "prep-time", prep_time,
                                              "cook-time", cook_time,
                                              "ingredients", ingredients,
                                              "instructions", instructions,
                                              "serves", serves,
                                              "spiciness", spiciness,
                                              "diets", diets,
                                              "images", images,
                                              "default-image", default_image,
                                              "readonly", readonly,
                                              NULL);
                }
                else {
                        recipe = g_object_new (GR_TYPE_RECIPE,
                                               "id", id,
                                               "name", name,
                                               "author", author,
                                               "description", description,
                                               "cuisine", cuisine,
                                               "season", season,
                                               "category", category,
                                               "prep-time", prep_time,
                                               "cook-time", cook_time,
                                               "ingredients", ingredients,
                                               "instructions", instructions,
                                               "notes", notes,
                                               "serves", serves,
                                               "spiciness", spiciness,
                                               "diets", diets,
                                               "images", images,
                                               "default-image", default_image,
                                               "ctime", ctime,
                                               "mtime", mtime,
                                               "readonly", readonly,
                                               NULL);
                        g_hash_table_insert (self->recipes, g_strdup (id), recipe);
                }
        }

        return TRUE;
}

static void
write_data (GObject       *obj,
            GAsyncResult  *res,
            gpointer       data)
{
        GrRecipeStore *store = data;
        g_autoptr(GError) error = NULL;
        g_autoptr(GFileOutputStream) ostream = NULL;
        g_autofree char *path = NULL;
        gsize written;

        ostream = g_file_replace_finish (G_FILE (obj), res, &error);
        if (!ostream)
                goto error;

        if (!g_output_stream_write_all (G_OUTPUT_STREAM (ostream),
                                        store->recipes_data,
                                        store->recipes_data_length,
                                        &written,
                                        NULL,
                                        &error))
                goto error;

        g_clear_pointer (&store->recipes_data, g_free);
        store->recipes_data_length = 0;

        return;
error:
        path = g_file_get_path (G_FILE (obj));
        g_error ("Failed to save %s: %s", path, error->message);
}

static void
replace_keyfile_async (GrRecipeStore *store,
                       GKeyFile      *keyfile,
                       const char    *path)
{
        g_autoptr(GError) error = NULL;
        g_autoptr(GFile) file = NULL;

        if (store->recipes_data) {
                g_print ("recipes store already scheduled\n");
                return;
        }

        store->recipes_data = g_key_file_to_data (keyfile, &store->recipes_data_length, &error);
        if (store->recipes_data == NULL)
                g_error ("Failed to save recipe database: %s", error->message);

        file = g_file_new_for_path (path);
        g_file_replace_async (file, NULL, FALSE, G_FILE_CREATE_NONE, 0, NULL, write_data, store);

}

static void
save_recipes (GrRecipeStore *self)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        GHashTableIter iter;
        const char *key;
        GrRecipe *recipe;
        g_autoptr(GError) error = NULL;
        const char *tmp;

        keyfile = g_key_file_new ();

        path = g_build_filename (get_user_data_dir (), "recipes.db", NULL);

        g_message ("Save recipe db: %s", path);

        g_hash_table_iter_init (&iter, self->recipes);
        while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&recipe)) {
                const char *name;
                const char *author;
                const char *description;
                const char *cuisine;
                const char *season;
                const char *category;
                const char *prep_time;
                const char *cook_time;
                const char *ingredients;
                const char *instructions;
                const char *notes;
                g_autoptr(GArray) images = NULL;
                int serves;
                int spiciness;
                GrDiets diets;
                g_auto(GStrv) paths = NULL;
                g_autofree int *angles = NULL;
                GDateTime *ctime;
                GDateTime *mtime;
                int default_image = 0;
                int i;
                gboolean readonly;

                name = gr_recipe_get_name (recipe);
                author = gr_recipe_get_author (recipe);
                description = gr_recipe_get_description (recipe);
                serves = gr_recipe_get_serves (recipe);
                spiciness = gr_recipe_get_spiciness (recipe);
                cuisine = gr_recipe_get_cuisine (recipe);
                season = gr_recipe_get_season (recipe);
                category = gr_recipe_get_category (recipe);
                prep_time = gr_recipe_get_prep_time (recipe);
                cook_time = gr_recipe_get_cook_time (recipe);
                diets = gr_recipe_get_diets (recipe);
                ingredients = gr_recipe_get_ingredients (recipe);
                instructions = gr_recipe_get_instructions (recipe);
                notes = gr_recipe_get_notes (recipe);
                ctime = gr_recipe_get_ctime (recipe);
                mtime = gr_recipe_get_mtime (recipe);
                default_image = gr_recipe_get_default_image (recipe);
                readonly = gr_recipe_is_readonly (recipe);

                g_object_get (recipe, "images", &images, NULL);

                tmp = get_user_data_dir ();
                paths = g_new0 (char *, images->len + 1);
                angles = g_new0 (int, images->len + 1);
                for (i = 0; i < images->len; i++) {
                        GrRotatedImage *ri = &g_array_index (images, GrRotatedImage, i);
                        if (g_str_has_prefix (ri->path, tmp))
                                paths[i] = g_strdup (ri->path + strlen (tmp) + 1);
                        else
                                paths[i] = g_strdup (ri->path);
                        angles[i] = ri->angle;
                }

                // For readonly recipes, we just store notes
                if (notes && notes[0])
                        g_key_file_set_string (keyfile, key, "Notes", notes);

                if (readonly)
                        continue;

                g_key_file_set_string (keyfile, key, "Name", name ? name : "");
                g_key_file_set_string (keyfile, key, "Author", author ? author : "");
                g_key_file_set_string (keyfile, key, "Description", description ? description : "");
                g_key_file_set_string (keyfile, key, "Cuisine", cuisine ? cuisine : "");
                g_key_file_set_string (keyfile, key, "Season", season ? season : "");
                g_key_file_set_string (keyfile, key, "Category", category ? category : "");
                g_key_file_set_string (keyfile, key, "PrepTime", prep_time ? prep_time : "");
                g_key_file_set_string (keyfile, key, "CookTime", cook_time ? cook_time : "");
                g_key_file_set_string (keyfile, key, "Ingredients", ingredients ? ingredients : "");
                g_key_file_set_string (keyfile, key, "Instructions", instructions ? instructions : "");
                g_key_file_set_integer (keyfile, key, "Serves", serves);
                g_key_file_set_integer (keyfile, key, "Spiciness", spiciness);
                g_key_file_set_integer (keyfile, key, "Diets", diets);
                g_key_file_set_integer (keyfile, key, "DefaultImage", default_image);
                g_key_file_set_string_list (keyfile, key, "Images", (const char * const *)paths, images->len);
                g_key_file_set_integer_list (keyfile, key, "Angles", angles, images->len);
                if (ctime) {
                        g_autofree char *created = date_time_to_string (ctime);
                        g_key_file_set_string (keyfile, key, "Created", created);
                }
                if (mtime) {
                        g_autofree char *modified = date_time_to_string (mtime);
                        g_key_file_set_string (keyfile, key, "Modified", modified);
                }
        }

        replace_keyfile_async (self, keyfile, path);
}

static gboolean
load_picks (GrRecipeStore *self,
            const char    *dir)
{
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;

        keyfile = g_key_file_new ();

        path = g_build_filename (dir, "picks.db", NULL);

        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load picks db: %s", error->message);
                else
                        g_message ("No picks db at: %s", path);
                return FALSE;
        }

        g_message ("Load picks db: %s", path);

        self->picks = g_key_file_get_string_list (keyfile, "Content", "Picks", NULL, &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load picks: %s", error->message);
                }
                g_clear_error (&error);
        }

        self->todays = g_key_file_get_string_list (keyfile, "Content", "Todays", NULL, &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load todays: %s", error->message);
                }
                g_clear_error (&error);
        }

        self->featured_chefs = g_key_file_get_string_list (keyfile, "Content", "Chefs", NULL, &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load chefs: %s", error->message);
                }
                g_clear_error (&error);
        }

        return TRUE;
}

static void
load_favorites (GrRecipeStore *self,
                const char    *dir)
{
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *tmp = NULL;
        const char *key;

        keyfile = g_key_file_new ();

        path = g_build_filename (dir, "favorites.db", NULL);

        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load favorites db: %s", error->message);
                else
                        g_message ("No favorites db at: %s", path);
                return;
        }

        g_message ("Load favorites db: %s", path);

        if (g_key_file_has_key (keyfile, "Content", "Recipes", NULL))
                key = "Recipes";
        else
                key = "Favorites";

        self->favorites = g_key_file_get_string_list (keyfile, "Content", key, NULL, &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load favorites: %s", error->message);
                }
                g_clear_error (&error);
        }

        tmp = g_key_file_get_string (keyfile, "Content", "LastChange", &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load favorites: %s", error->message);
                }
                g_clear_error (&error);
        }

        if (tmp)
                self->favorite_change = date_time_from_string (tmp);
}

static void
save_favorites (GrRecipeStore *self)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        g_autoptr(GError) error = NULL;

        keyfile = g_key_file_new ();

        path = g_build_filename (get_user_data_dir (), "favorites.db", NULL);

        g_message ("Save favorites db: %s", path);

        g_key_file_set_string_list (keyfile, "Content", "Recipes", (const char * const *)self->favorites, g_strv_length (self->favorites));

        if (self->favorite_change) {
                g_autofree char *tmp = NULL;

                tmp = date_time_to_string (self->favorite_change);
                g_key_file_set_string (keyfile, "Content", "LastChange", tmp);
        }

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save recipe database: %s", error->message);
        }
}

static void
load_shopping (GrRecipeStore *self,
               const char    *dir)
{
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *tmp = NULL;
        gsize len1, len2;

        keyfile = g_key_file_new ();

        path = g_build_filename (dir, "shopping.db", NULL);

        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load shopping db: %s", error->message);
                else
                        g_message ("No shopping db at: %s", path);
                return;
        }

        g_message ("Load shopping db: %s", path);

        self->shopping = g_key_file_get_string_list (keyfile, "Content", "Recipes", &len1, &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load shopping: %s", error->message);
                }
                g_clear_error (&error);
        }
        self->shopping_serves = g_key_file_get_integer_list (keyfile, "Content", "Serves", &len2, &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load shopping: %s", error->message);
                }
                g_clear_error (&error);
        }

        if (len1 != len2) {
                g_warning ("Failed to load shopping: Recipes and Serves lists have different lengths");
        }

        tmp = g_key_file_get_string (keyfile, "Content", "LastChange", &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load shopping: %s", error->message);
                }
                g_clear_error (&error);
        }

        if (tmp)
                self->shopping_change = date_time_from_string (tmp);
}

static void
save_shopping (GrRecipeStore *self)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        g_autoptr(GError) error = NULL;

        keyfile = g_key_file_new ();

        path = g_build_filename (get_user_data_dir (), "shopping.db", NULL);

        g_message ("Save shopping db: %s", path);

        g_key_file_set_string_list (keyfile, "Content", "Recipes", (const char * const *)self->shopping, g_strv_length (self->shopping));

        g_key_file_set_integer_list (keyfile, "Content", "Serves", self->shopping_serves, g_strv_length (self->shopping));

        if (self->shopping_change) {
                g_autofree char *tmp = NULL;

                tmp = date_time_to_string (self->shopping_change);
                g_key_file_set_string (keyfile, "Content", "LastChange", tmp);
        }

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save shopping database: %s", error->message);
        }
}

static void
load_cooked (GrRecipeStore *store,
             const char    *dir)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *path = NULL;
        g_auto(GStrv) groups = NULL;
        gsize length;
        int i;

        keyfile = g_key_file_new ();

        path = g_build_filename (dir, "cooked.db", NULL);
        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load cooked db: %s", error->message);
                else
                        g_message ("No cooked db at: %s", path);
                return;
        }

        g_message ("Load cooked db: %s", path);

        groups = g_key_file_get_groups (keyfile, &length);
        for (i = 0; i < length; i++) {
                g_autofree char *name = NULL;
                int count;

                g_clear_error (&error);

                name = g_key_file_get_string (keyfile, groups[i], "Name", &error);
                if (error) {
                        g_warning ("Failed to load cooked entry %s: %s", groups[i], error->message);
                        g_clear_error (&error);
                        continue;
                }
                count = g_key_file_get_integer (keyfile, groups[i], "Count", &error);
                if (error) {
                        g_warning ("Failed to load cooked entry %s: %s", groups[i], error->message);
                        g_clear_error (&error);
                        continue;
                }

                g_hash_table_insert (store->cooked, g_strdup (name), GINT_TO_POINTER (count));
        }
}

static void
save_cooked (GrRecipeStore *store)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        GHashTableIter iter;
        const char *name;
        gpointer value;
        g_autoptr(GError) error = NULL;

        keyfile = g_key_file_new ();

        path = g_build_filename (get_user_data_dir (), "cooked.db", NULL);

        g_message ("Save cooked db: %s", path);

        g_hash_table_iter_init (&iter, store->cooked);
        while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&value)) {
                int count;
                count = GPOINTER_TO_INT (value);

                g_key_file_set_string (keyfile, name, "Name", name);
                g_key_file_set_integer (keyfile, name, "Count", count);
        }

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save cooked database: %s", error->message);
        }
}

static gboolean
load_chefs (GrRecipeStore *self,
            const char    *dir,
            gboolean       readonly)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *path = NULL;
        g_auto(GStrv) groups = NULL;
        gsize length;
        int i;

        keyfile = g_key_file_new ();

        path = g_build_filename (dir, "chefs.db", NULL);
        /* Just since my example dataset has authors.db */
        if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
                g_free (path);
                path = g_build_filename (dir, "authors.db", NULL);
        }

        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load chefs db: %s", error->message);
                else
                        g_message ("No chefs db at: %s", path);
                return FALSE;
        }

        g_message ("Load chefs db: %s", path);

        groups = g_key_file_get_groups (keyfile, &length);
        for (i = 0; i < length; i++) {
                GrChef *chef;
                const char *id;
                g_autofree char *name = NULL;
                g_autofree char *fullname = NULL;
                g_autofree char *description = NULL;
                g_autofree char *image_path = NULL;

                g_clear_error (&error);

                id = groups[i];
                name = g_key_file_get_string (keyfile, groups[i], "Name", &error);
                if (error) {
                        g_warning ("Failed to load chef %s: %s", groups[i], error->message);
                        g_clear_error (&error);
                        continue;
                }
                fullname = g_key_file_get_string (keyfile, groups[i], "Fullname", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load chef %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                description = g_key_file_get_string (keyfile, groups[i], "Description", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load chef %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                image_path = g_key_file_get_string (keyfile, groups[i], "Image", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load chef %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }

                if (image_path && image_path[0] != '\0' && image_path[0] != '/') {
                        char *tmp;
                        tmp = g_build_filename (dir, image_path, NULL);
                        g_free (image_path);
                        image_path = tmp;
                }

                chef = g_hash_table_lookup (self->chefs, id);
                if (chef == NULL) {
                        chef = gr_chef_new ();
                        g_hash_table_insert (self->chefs, g_strdup (id), chef);
                }

                g_object_set (chef,
                              "id", id,
                              "name", name,
                              "fullname", fullname,
                              "description", description,
                              "image-path", image_path,
                              "readonly", readonly,
                              NULL);
        }

        return TRUE;
}

static void
save_chefs (GrRecipeStore *store)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        GHashTableIter iter;
        const char *key;
        GrChef *chef;
        g_autoptr(GError) error = NULL;
        const char *tmp;

        keyfile = g_key_file_new ();

        path = g_build_filename (get_user_data_dir (), "chefs.db", NULL);

        g_message ("Save chefs db: %s", path);

        g_hash_table_iter_init (&iter, store->chefs);
        while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&chef)) {
                const char *name;
                const char *fullname;
                const char *description;
                const char *image_path;

                if (gr_chef_is_readonly (chef))
                        continue;

                name = gr_chef_get_name (chef);
                fullname = gr_chef_get_fullname (chef);
                description = gr_chef_get_description (chef);
                image_path = gr_chef_get_image (chef);

                tmp = get_user_data_dir ();
                if (image_path && g_str_has_prefix (image_path, tmp)) {
                        g_autofree char *tmp2 = NULL;

                        tmp2 = g_strdup (image_path + strlen (tmp) + 1);
                        g_key_file_set_string (keyfile, key, "Image", tmp2);
                }
                else
                        g_key_file_set_string (keyfile, key, "Image", image_path ? image_path : "");

                g_key_file_set_string (keyfile, key, "Name", name ? name : "");
                g_key_file_set_string (keyfile, key, "Fullname", fullname ? fullname : "");
                g_key_file_set_string (keyfile, key, "Description", description ? description : "");
        }

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save chefs database: %s", error->message);
        }
}

static void
save_user (GrRecipeStore *self)
{
        g_autofree char *path = NULL;

        path = g_build_filename (get_user_data_dir (), "user", NULL);

        g_message ("Save user id: %s", path);

        if (self->user == NULL || self->user[0] == '\0') {
                g_unlink (path);
        }
        else {
                g_autoptr(GError) error = NULL;

                if (!g_file_set_contents (path, self->user, -1, &error)) {
                        g_error ("Failed to save user id: %s", error->message);
                }
        }
}

static void
load_user (GrRecipeStore *self,
           const char    *dir)
{
        g_autofree char *path = NULL;
        g_autofree char *contents = NULL;
        g_autoptr(GError) error = NULL;

        path = g_build_filename (dir, "user", NULL);
        if (!g_file_get_contents (path, &contents, NULL, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load user id: %s", error->message);
                else {
                        self->user = g_strdup (g_get_user_name ());
                        save_user (self);
                }
                return;
        }

        g_message ("Load user id: %s", path);

        self->user = g_strdup (contents);

        if (self->user[strlen (self->user) - 1] == '\n')
                self->user[strlen (self->user) - 1] = '\0';
}

static void
gr_recipe_store_init (GrRecipeStore *self)
{
        const char *dir;
        g_autofree char *current_dir = NULL;
        g_autofree char *uninstalled_dir = NULL;

        self->recipes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
        self->chefs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
        self->cooked = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

        /* First load preinstalled data */
        dir = get_pkg_data_dir ();
        current_dir = g_get_current_dir ();
        uninstalled_dir = g_build_filename (current_dir, "data", NULL);

        if (!load_recipes (self, dir, TRUE))
                load_recipes (self, uninstalled_dir, TRUE);
        if (!load_chefs (self, dir, TRUE))
                load_chefs (self, uninstalled_dir, TRUE);
        if (!load_picks (self, dir))
                load_picks (self, uninstalled_dir);

        /* Now load saved data */
        dir = get_user_data_dir ();
        load_recipes (self, dir, FALSE);
        load_favorites (self, dir);
        load_shopping (self, dir);
        load_cooked (self, dir);
        load_chefs (self, dir, FALSE);
        load_user (self, dir);

        g_message ("%d recipes loaded", g_hash_table_size (self->recipes));
        g_message ("%d chefs loaded", g_hash_table_size (self->chefs));
}

static guint add_signal;
static guint remove_signal;
static guint changed_signal;
static guint chefs_changed_signal;

static void
gr_recipe_store_class_init (GrRecipeStoreClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_recipe_store_finalize;

        add_signal = g_signal_new ("recipe-added",
                                   G_TYPE_FROM_CLASS (object_class),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL,
                                   NULL,
                                   G_TYPE_NONE, 1, GR_TYPE_RECIPE);
        remove_signal = g_signal_new ("recipe-removed",
                                      G_TYPE_FROM_CLASS (object_class),
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 1, GR_TYPE_RECIPE);
        changed_signal = g_signal_new ("recipe-changed",
                                       G_TYPE_FROM_CLASS (object_class),
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL,
                                       NULL,
                                       G_TYPE_NONE, 1, GR_TYPE_RECIPE);
        chefs_changed_signal = g_signal_new ("chefs-changed",
                                             G_TYPE_FROM_CLASS (object_class),
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, NULL,
                                             NULL,
                                             G_TYPE_NONE, 0);
}

GrRecipeStore *
gr_recipe_store_new (void)
{
        return g_object_new (GR_TYPE_RECIPE_STORE, NULL);
}

gboolean
gr_recipe_store_add_recipe (GrRecipeStore  *self,
                            GrRecipe       *recipe,
                            GError        **error)
{
        const char *id;

        g_object_ref (recipe);

        id = gr_recipe_get_id (recipe);

        if (id == NULL || g_str_has_prefix (id, "_by_")) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("You need to provide a name for the recipe"));
                return FALSE;
        }
        if (g_hash_table_contains (self->recipes, id)) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("A recipe with this name and author already exists.\nPlease choose a different name"));
                return FALSE;
        }

        g_hash_table_insert (self->recipes, g_strdup (id), g_object_ref (recipe));
        g_signal_emit (self, add_signal, 0, recipe);

        save_recipes (self);

        g_object_unref (recipe);

        return TRUE;
}

gboolean
gr_recipe_store_update_recipe (GrRecipeStore  *self,
                               GrRecipe       *recipe,
                               const char     *old_id,
                               GError        **error)
{
        const char *id;
        GrRecipe *old;

        g_object_ref (recipe);

        id = gr_recipe_get_id (recipe);

        if (id == NULL || id[0] == '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("You need to provide an ID for the recipe"));
                return FALSE;
        }
        if (strcmp (id, old_id) != 0 &&
            g_hash_table_contains (self->recipes, id)) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("A recipe with this ID already exists"));
                return FALSE;
        }

        old = g_hash_table_lookup (self->recipes, old_id);
        g_assert (recipe == old);

        g_hash_table_remove (self->recipes, old_id);
        g_hash_table_insert (self->recipes, g_strdup (id), g_object_ref (recipe));

        g_signal_emit (self, changed_signal, 0, recipe);

        save_recipes (self);

        g_object_unref (recipe);

        return TRUE;
}

gboolean
gr_recipe_store_remove_recipe (GrRecipeStore *self,
                               GrRecipe      *recipe)
{
        const char *id;
        gboolean ret = FALSE;

        id = gr_recipe_get_id (recipe);

        g_object_ref (recipe);

        if (g_hash_table_remove (self->recipes, id)) {
                g_signal_emit (self, remove_signal, 0, recipe);
                save_recipes (self);
                ret = TRUE;
        }

        g_object_unref (recipe);

        return ret;
}

GrRecipe *
gr_recipe_store_get_recipe (GrRecipeStore *self,
                            const char    *id)
{
        GrRecipe *recipe;

        recipe = g_hash_table_lookup (self->recipes, id);

        if (recipe)
                return g_object_ref (recipe);

        return NULL;
}

char **
gr_recipe_store_get_recipe_keys (GrRecipeStore *self,
                                 guint         *length)
{
        return (char **)g_hash_table_get_keys_as_array (self->recipes, length);
}

gboolean
gr_recipe_store_recipe_is_todays (GrRecipeStore *self,
                                  GrRecipe      *recipe)
{
        const char *id;

        if (self->todays == NULL)
                return FALSE;

        id = gr_recipe_get_id (recipe);

        return g_strv_contains ((const char *const*)self->todays, id);
}

gboolean
gr_recipe_store_recipe_is_pick (GrRecipeStore *self,
                                GrRecipe      *recipe)
{
        const char *id;

        if (self->picks == NULL)
                return FALSE;

        id = gr_recipe_get_id (recipe);

        return g_strv_contains ((const char *const*)self->picks, id);
}

GrChef *
gr_recipe_store_get_chef (GrRecipeStore *self,
                          const char    *id)
{
        GrChef *chef;

        chef = g_hash_table_lookup (self->chefs, id);

        if (chef)
                return g_object_ref (chef);

        return NULL;
}

char **
gr_recipe_store_get_chef_keys (GrRecipeStore *self,
                               guint         *length)
{
        return (char **)g_hash_table_get_keys_as_array (self->chefs, length);
}

const char *
gr_recipe_store_get_user_key (GrRecipeStore *self)
{
        return self->user;
}

gboolean
gr_recipe_store_add_chef (GrRecipeStore  *self,
                          GrChef         *chef,
                          GError        **error)
{
        const char *id;

        id = gr_chef_get_id (chef);

        if (id == NULL || id[0] == '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("You need to provide an ID"));
                return FALSE;
        }

        if (g_hash_table_contains (self->chefs, id)) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Sorry, this ID is taken"));
                return FALSE;
        }

        g_hash_table_insert (self->chefs, g_strdup (id), g_object_ref (chef));

        g_signal_emit (self, chefs_changed_signal, 0);
        save_chefs (self);

        return TRUE;
}

gboolean
gr_recipe_store_update_chef (GrRecipeStore  *self,
                             GrChef         *chef,
                             const char     *old_id,
                             GError        **error)
{
        const char *id;
        GrChef *old;

        g_object_ref (chef);

        id = gr_chef_get_id (chef);

        if (id == NULL || id[0] == '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("You need to provide an ID for the chef"));
                return FALSE;
        }
        if (strcmp (id, old_id) != 0 &&
            g_hash_table_contains (self->chefs, id)) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("A chef with this ID already exists"));
                return FALSE;
        }

        old = g_hash_table_lookup (self->chefs, old_id);
        g_assert (chef == old);

        g_hash_table_remove (self->chefs, old_id);
        g_hash_table_insert (self->chefs, g_strdup (id), g_object_ref (chef));

        g_signal_emit (self, chefs_changed_signal, 0);
        save_chefs (self);

        g_object_unref (chef);

        return TRUE;
}

gboolean
gr_recipe_store_update_user (GrRecipeStore  *self,
                             GrChef         *chef,
                             GError        **error)
{
        const char *id;
        gboolean ret = TRUE;

        id = gr_chef_get_id (chef);

        if (id != NULL && id[0] != '\0') {
                g_object_ref (chef);
                if (g_strcmp0 (id, self->user) == 0) {
                        g_hash_table_remove (self->chefs, id);
                }
                ret = gr_recipe_store_add_chef (self, chef, error);
                g_object_unref (chef);
        }

        if (ret) {
                g_free (self->user);
                self->user = g_strdup (id);
                save_user (self);
        }

        return ret;
}

gboolean
gr_recipe_store_chef_is_featured (GrRecipeStore *self,
                                  GrChef        *chef)
{
        const char *id;

        if (self->featured_chefs == NULL)
                return FALSE;

        id = gr_chef_get_id (chef);

        return g_strv_contains ((const char *const*)self->featured_chefs, id);
}

char **
gr_recipe_store_get_all_ingredients (GrRecipeStore *self,
                                     guint         *length)
{
        GHashTableIter iter;
        GrRecipe *recipe;
        GHashTable *ingreds;
        char **result;
        int i, j;

        ingreds = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

        g_hash_table_iter_init (&iter, self->recipes);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&recipe)) {
                const char *ingredients;
                GrIngredientsList *list = NULL;
                g_autofree char **segments = NULL;

                ingredients = gr_recipe_get_ingredients (recipe);

                if (!ingredients || ingredients[0] == '\0')
                        continue;

                list = gr_ingredients_list_new (ingredients);
                segments = gr_ingredients_list_get_segments (list);
                for (j = 0; segments[j]; j++) {
                        g_autofree char **ret = NULL;
                        ret = gr_ingredients_list_get_ingredients (list, segments[j]);
                        for (i = 0; ret[i]; i++)
                                g_hash_table_add (ingreds, ret[i]);
                }

                g_object_unref (list);
        }

        result = (char **)g_hash_table_get_keys_as_array (ingreds, length);
        g_hash_table_steal_all (ingreds);
        g_hash_table_unref (ingreds);

        return result;
}

void
gr_recipe_store_add_favorite (GrRecipeStore *self,
                              GrRecipe      *recipe)
{
        char **strv;
        int length;
        int i;
        const char *id;

        id = gr_recipe_get_id (recipe);

        if (g_strv_contains ((const char * const*)self->favorites, id))
                return;

        length = g_strv_length (self->favorites);
        strv = g_new (char *, length + 2);
        strv[0] = g_strdup (id);
        for (i = 0; i < length; i++)
                strv[i + 1] = self->favorites[i];
        strv[length + 1] = NULL;

        g_free (self->favorites);
        self->favorites = strv;

        if (self->favorite_change)
                g_date_time_unref (self->favorite_change);
        self->favorite_change = g_date_time_new_now_utc ();

        save_favorites (self);

        g_signal_emit (self, changed_signal, 0, recipe);
}

void
gr_recipe_store_remove_favorite (GrRecipeStore *self,
                                 GrRecipe      *recipe)
{
        int i, j;
        const char *id;

        id = gr_recipe_get_id (recipe);

        for (i = 0; self->favorites[i]; i++) {
                if (strcmp (self->favorites[i], id) == 0) {
                        g_free (self->favorites[i]);
                        for (j = i; self->favorites[j]; j++) {
                                self->favorites[j] = self->favorites[j + 1];
                        }
                        break;
                }
        }

        if (self->favorite_change)
                g_date_time_unref (self->favorite_change);
        self->favorite_change = g_date_time_new_now_utc ();

        save_favorites (self);

        g_signal_emit (self, changed_signal, 0, recipe);
}

gboolean
gr_recipe_store_is_favorite (GrRecipeStore *self,
                             GrRecipe      *recipe)
{
        const char *id;

        if (self->favorites == NULL)
                return FALSE;

        id = gr_recipe_get_id (recipe);

        return g_strv_contains ((const char *const*)self->favorites, id);
}

GDateTime *
gr_recipe_store_last_favorite_change (GrRecipeStore *self)
{
        return self->favorite_change;
}

void
gr_recipe_store_add_to_shopping (GrRecipeStore *self,
                                 GrRecipe      *recipe,
                                 int            serves)
{
        char **strv;
        int *intv;
        int length;
        int i;
        const char *id;

        id = gr_recipe_get_id (recipe);

        for (i = 0; self->shopping[i]; i++) {
                if (strcmp (self->shopping[i], id) == 0) {
                        self->shopping_serves[i] = serves;
                        goto save;
                }
        }

        length = g_strv_length (self->shopping);
        strv = g_new (char *, length + 2);
        strv[0] = g_strdup (id);
        intv = g_new (int, length + 1);
        intv[0] = serves;
        for (i = 0; i < length; i++) {
                strv[i + 1] = self->shopping[i];
                intv[i + 1] = self->shopping_serves[i];
        }
        strv[length + 1] = NULL;

        g_free (self->shopping);
        g_free (self->shopping_serves);
        self->shopping = strv;
        self->shopping_serves = intv;

save:
        if (self->shopping_change)
                g_date_time_unref (self->shopping_change);
        self->shopping_change = g_date_time_new_now_utc ();

        save_shopping (self);

        g_signal_emit (self, changed_signal, 0, recipe);
}

void
gr_recipe_store_remove_from_shopping (GrRecipeStore *self,
                                      GrRecipe      *recipe)
{
        int i, j;
        const char *id;

        id = gr_recipe_get_id (recipe);

        for (i = 0; self->shopping[i]; i++) {
                if (strcmp (self->shopping[i], id) == 0) {
                        g_free (self->shopping[i]);
                        for (j = i; self->shopping[j]; j++) {
                                self->shopping[j] = self->shopping[j + 1];
                                self->shopping_serves[j] = self->shopping_serves[j + 1];
                        }
                        break;
                }
        }

        if (self->shopping_change)
                g_date_time_unref (self->shopping_change);
        self->shopping_change = g_date_time_new_now_utc ();

        save_shopping (self);

        g_signal_emit (self, changed_signal, 0, recipe);
}

gboolean
gr_recipe_store_is_in_shopping (GrRecipeStore *self,
                                GrRecipe      *recipe)
{
        const char *id;

        if (self->shopping == NULL)
                return FALSE;

        id = gr_recipe_get_id (recipe);

        return g_strv_contains ((const char *const*)self->shopping, id);
}

int
gr_recipe_store_get_shopping_serves (GrRecipeStore *self,
                                     GrRecipe      *recipe)
{
        const char *id;
        int i;

        id = gr_recipe_get_id (recipe);

        for (i = 0; self->shopping[i]; i++) {
                if (strcmp (self->shopping[i], id) == 0) {
                        return self->shopping_serves[i];
                }
        }

        return 0;
}

GDateTime *
gr_recipe_store_last_shopping_change (GrRecipeStore *self)
{
        return self->shopping_change;
}

gboolean
gr_recipe_store_has_diet (GrRecipeStore *self,
                          GrDiets        diet)
{
        GHashTableIter iter;
        GrRecipe *recipe;

        g_hash_table_iter_init (&iter, self->recipes);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&recipe)) {
                if ((gr_recipe_get_diets (recipe) & diet) == diet)
                        return TRUE;
        }

        return FALSE;
}

gboolean
gr_recipe_store_has_chef (GrRecipeStore *self,
                          GrChef        *chef)
{
        GHashTableIter iter;
        GrRecipe *recipe;

        g_hash_table_iter_init (&iter, self->recipes);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&recipe)) {
                if (strcmp (gr_chef_get_id (chef), gr_recipe_get_author (recipe)) == 0)
                        return TRUE;
        }

        return FALSE;
}

gboolean
gr_recipe_store_has_cuisine (GrRecipeStore *self,
                             const char    *cuisine)
{
        GHashTableIter iter;
        GrRecipe *recipe;

        g_hash_table_iter_init (&iter, self->recipes);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&recipe)) {
                if (strcmp (cuisine, gr_recipe_get_cuisine (recipe)) == 0)
                        return TRUE;
        }

        return FALSE;
}

void
gr_recipe_store_add_cooked (GrRecipeStore *store,
                            GrRecipe      *recipe)
{
        int count;
        const char *id;

        id = gr_recipe_get_id (recipe);

        count = GPOINTER_TO_INT (g_hash_table_lookup (store->cooked, id));
        count++;

        g_hash_table_insert (store->cooked, g_strdup (id), GINT_TO_POINTER (count));

        save_cooked (store);
}

int
gr_recipe_store_get_cooked (GrRecipeStore *store,
                            GrRecipe      *recipe)
{
        const char *id;

        id = gr_recipe_get_id (recipe);

        return GPOINTER_TO_INT (g_hash_table_lookup (store->cooked, id));
}

/*** search implementation ***/

struct _GrRecipeSearch
{
        GObject parent_instance;

        GrRecipeStore *store;

        char **query;

        gulong idle;
        GHashTableIter iter;

        GList *results;
        GList *pending;
        int n_pending;

        gboolean running;
};

enum {
        STARTED,
        HITS_ADDED,
        HITS_REMOVED,
        FINISHED,
        LAST_SEARCH_SIGNAL
};

static guint search_signals[LAST_SEARCH_SIGNAL];

G_DEFINE_TYPE (GrRecipeSearch, gr_recipe_search, G_TYPE_OBJECT)

GrRecipeSearch *
gr_recipe_search_new (void)
{
        GrRecipeSearch *search;

        search = GR_RECIPE_SEARCH (g_object_new (GR_TYPE_RECIPE_SEARCH, NULL));

        search->store = g_object_ref (gr_app_get_recipe_store (GR_APP (g_application_get_default ())));

        return search;
}

static void
add_pending (GrRecipeSearch *search,
             GrRecipe       *recipe)
{
        search->pending = g_list_prepend (search->pending, recipe);
        search->n_pending++;
}

static void
clear_pending (GrRecipeSearch *search)
{
        search->results = g_list_concat (search->results, search->pending);
        search->pending = NULL;
        search->n_pending = 0;
}

static void
send_pending (GrRecipeSearch *search)
{
        if (search->n_pending > 0) {
                g_signal_emit (search, search_signals[HITS_ADDED], 0, search->pending);
                clear_pending (search);
        }
}

static void
clear_results (GrRecipeSearch *search)
{
        g_list_free (search->results);
        search->results = NULL;
}

static gboolean
recipe_matches (GrRecipeSearch *search,
                GrRecipe       *recipe)
{
        if (strcmp (search->query[0], "is:favorite") == 0)
                return gr_recipe_store_is_favorite (search->store, recipe);
        else if (strcmp (search->query[0], "is:shopping") == 0)
                return gr_recipe_store_is_in_shopping (search->store, recipe);
        else
                return gr_recipe_matches (recipe, (const char **)search->query);
}

static gboolean
search_idle (gpointer data)
{
        GrRecipeSearch *search = data;
        const char *key;
        GrRecipe *recipe;
        gint64 start_time;

        start_time = g_get_monotonic_time ();

        while (g_hash_table_iter_next (&search->iter, (gpointer *)&key, (gpointer *)&recipe)) {
                if (recipe_matches (search, recipe))
                        add_pending (search, recipe);

                if (search->n_pending > 2) {
                        send_pending (search);
                }

                if (g_get_monotonic_time () >= start_time + 4000) {
                        send_pending (search);
                        search->idle = g_timeout_add (16, search_idle, search);
                        return G_SOURCE_REMOVE;
                }
        }

        send_pending (search);

        search->idle = 0;
        g_signal_emit (search, search_signals[FINISHED], 0);

        return G_SOURCE_REMOVE;
}

static void
start_search (GrRecipeSearch *search)
{
        if (search->query == NULL) {
                g_warning ("No query set");
                return;
        }

        if (search->idle == 0) {
                g_hash_table_iter_init (&search->iter, search->store->recipes);
                clear_pending (search);
                clear_results (search);
                g_signal_emit (search, search_signals[STARTED], 0);
                search_idle (search);
        }
}

static void
stop_search (GrRecipeSearch *search)
{
        send_pending (search);
        clear_results (search);
        if (search->idle != 0) {
                g_source_remove (search->idle);
                search->idle = 0;
        }
}

static void
refilter_existing_results (GrRecipeSearch *search)
{
        GList *l;
        GList *rejected, *accepted;

        /* FIXME: do this incrementally */

        rejected = NULL;
        accepted = NULL;
        for (l = search->results; l; l = l->next) {
                GrRecipe *recipe = l->data;

                if (recipe_matches (search, recipe))
                        accepted = g_list_append (accepted, recipe);
                else
                        rejected = g_list_append (rejected, recipe);
        }

        g_list_free (search->results);
        search->results = accepted;
        if (rejected) {
                g_signal_emit (search, search_signals[HITS_REMOVED], 0, rejected);
                g_list_free (rejected);
        }

        if (search->idle == 0) {
                g_signal_emit (search, search_signals[FINISHED], 0);
        }
}

static gboolean
query_is_narrowing (GrRecipeSearch  *search,
                    const char     **query)
{
        int i, j;

        /* Being narrower means having more conditions */
        if (search->query == NULL)
                return FALSE;

        for (i = 0; search->query[i]; i++) {
                const char *term = search->query[i];

                if (g_str_has_prefix (term, "i+:") ||
                    g_str_has_prefix (term, "i-:") ||
                    g_str_has_prefix (term, "by:") ||
                    g_str_has_prefix (term, "se:") ||
                    g_str_has_prefix (term, "me:") ||
                    g_str_has_prefix (term, "di:")) {
                        if (!g_strv_contains (query, term))
                                return FALSE;
                }
                else {
                        gboolean res = FALSE;
                        for (j = 0; query[j]; j++) {
                                if (g_str_has_prefix (query[j], term)) {
                                        res = TRUE;
                                        break;
                                }
                        }
                        if (!res)
                                return FALSE;
                }
        }

        return TRUE;
}

void
gr_recipe_search_stop (GrRecipeSearch *search)
{
        stop_search (search);
        g_clear_pointer (&search->query, g_strfreev);
}

void
gr_recipe_search_set_query (GrRecipeSearch *search,
                            const char     *query)
{
        g_auto(GStrv) strv = NULL;

        if (query)
                strv = g_strsplit (query, " ", -1);

        gr_recipe_search_set_terms (search, (const char **)strv);
}

void
gr_recipe_search_set_terms (GrRecipeSearch  *search,
                            const char     **terms)
{
        gboolean narrowing;

        if (terms == NULL || terms[0] == NULL) {
                stop_search (search);
                g_clear_pointer (&search->query, g_strfreev);
                return;
        }

        narrowing = query_is_narrowing (search, terms);

        g_strfreev (search->query);
        search->query = g_strdupv ((char **)terms);

        if (narrowing) {
                refilter_existing_results (search);
        }
        else {
                stop_search (search);
                start_search (search);
        }
}

const char **
gr_recipe_search_get_terms (GrRecipeSearch *search)
{
        return (const char **)search->query;
}

static void
gr_recipe_search_finalize (GObject *object)
{
        GrRecipeSearch *search = (GrRecipeSearch *)object;

        stop_search (search);
        g_strfreev (search->query);
        g_object_unref (search->store);

        G_OBJECT_CLASS (gr_recipe_search_parent_class)->finalize (object);
}

static void
gr_recipe_search_class_init (GrRecipeSearchClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_recipe_search_finalize;

        /**
         * GrRecipeSearch::started:
         *
         * Gets emitted whenever a new set of results is started.
         * Users are expected to clear their list of results.
         */
        search_signals[STARTED] =
                g_signal_new ("started",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              NULL,
                              G_TYPE_NONE, 0);

        /**
         * GrRecipeSearch::hits-added:
         *
         * Gets emitted whenever more results are added to the total result set.
         * Users are expeted to update their list of results.
         */
        search_signals[HITS_ADDED] =
                g_signal_new ("hits-added",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              NULL,
                              G_TYPE_NONE, 1,
                              G_TYPE_POINTER);

        /**
         * GrRecipeSearch::hits-removed:
         *
         * Gets emitted when existing results are removed from the total result set.
         * This can happen when a narrower query is set.
         * Users are expeted to update their list of results.
         */
        search_signals[HITS_REMOVED] =
                g_signal_new ("hits-removed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              NULL,
                              G_TYPE_NONE, 1,
                              G_TYPE_POINTER);

        /**
         * GrRecipeSearch::finished:
         *
         * Indicates that no more ::hits-added or ::hits-removed signals will be
         * emitted until the query is changed.
         */
        search_signals[FINISHED] =
                g_signal_new ("finished",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              NULL,
                              G_TYPE_NONE, 0);
}

static void
gr_recipe_search_init (GrRecipeSearch *self)
{
}
