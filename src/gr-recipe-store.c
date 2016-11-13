/* gr-recipe-store.c
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

#include "config.h"

#include <sys/statfs.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-utils.h"
#include "gr-ingredients-list.h"
#include "gr-images.h"


struct _GrRecipeStore
{
        GObject parent;

        GHashTable *recipes;
        GHashTable *chefs;

        char **todays;
        char **picks;
        char **favorites;
        char **featured_chefs;
        char *user;
};


G_DEFINE_TYPE (GrRecipeStore, gr_recipe_store, G_TYPE_OBJECT)

static void
gr_recipe_store_finalize (GObject *object)
{
        GrRecipeStore *self = GR_RECIPE_STORE (object);

        g_clear_pointer (&self->recipes, g_hash_table_unref);
        g_clear_pointer (&self->chefs, g_hash_table_unref);
        g_strfreev (self->todays);
        g_strfreev (self->picks);
        g_strfreev (self->favorites);
        g_strfreev (self->featured_chefs);
        g_free (self->user);

        G_OBJECT_CLASS (gr_recipe_store_parent_class)->finalize (object);
}

static void
load_recipes (GrRecipeStore *self, const char *dir)
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
                return;
        }
        g_message ("Load recipe db: %s", path);

        groups = g_key_file_get_groups (keyfile, &length);
        for (i = 0; i < length; i++) {
                GrRecipe *recipe;
                g_autofree char *name = NULL;
                g_autofree char *author = NULL;
                g_autofree char *description = NULL;
                g_autofree char *cuisine = NULL;
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
                GrDiets diets;
                g_autoptr(GArray) images = NULL;
                GrRotatedImage ri;

                g_clear_error (&error);

                name = g_key_file_get_string (keyfile, groups[i], "Name", &error);
                if (error) {
                        g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                        g_clear_error (&error);
                        continue;
                }
                author = g_key_file_get_string (keyfile, groups[i], "Author", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                                continue;
                        }
                        author = g_strdup ("Unknown");
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
                        g_warning ("Failed to load recipe %s: %s", groups[i], error->message);
                        g_clear_error (&error);
                        continue;
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
                serves = g_key_file_get_integer (keyfile, groups[i], "Serves", &error);
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
                        char *tmp;
                        tmp = g_build_filename (dir, image_path, NULL);
                        g_free (image_path);
                        image_path = tmp;
                }
                if (paths) {
                        for (j = 0; paths[j]; j++) {
                                if (paths[j][0] != '/') {
                                        char *tmp;
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

                recipe = g_hash_table_lookup (self->recipes, name);
                if (recipe == NULL) {
                        recipe = gr_recipe_new ();
                        g_hash_table_insert (self->recipes, g_strdup (name), recipe);
                }

                g_object_set (recipe,
                              "name", name,
                              "author", author,
                              "description", description,
                              "cuisine", cuisine,
                              "category", category,
                              "prep-time", prep_time,
                              "cook-time", cook_time,
                              "ingredients", ingredients,
                              "instructions", instructions,
                              "notes", notes,
                              "serves", serves,
                              "diets", diets,
                              "images", images,
                              NULL);
        }
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
                const char *category;
                const char *prep_time;
                const char *cook_time;
                const char *ingredients;
                const char *instructions;
                const char *notes;
                g_autoptr(GArray) images = NULL;
                int serves;
                GrDiets diets;
                g_auto(GStrv) paths = NULL;
                g_autofree int *angles = NULL;
                int i;

                name = gr_recipe_get_name (recipe);
                author = gr_recipe_get_author (recipe);
                description = gr_recipe_get_description (recipe);
                serves = gr_recipe_get_serves (recipe);
                cuisine = gr_recipe_get_cuisine (recipe);
                category = gr_recipe_get_category (recipe);
                prep_time = gr_recipe_get_prep_time (recipe);
                cook_time = gr_recipe_get_cook_time (recipe);
                diets = gr_recipe_get_diets (recipe);
                ingredients = gr_recipe_get_ingredients (recipe);
                instructions = gr_recipe_get_instructions (recipe);
                notes = gr_recipe_get_notes (recipe);

                g_object_get (recipe, "images", &images, NULL);

                tmp = get_user_data_dir ();
                paths = g_new0 (char *, images->len + 1);
                angles = g_new0 (int, images->len);
                for (i = 0; i < images->len; i++) {
                        GrRotatedImage *ri = &g_array_index (images, GrRotatedImage, i);
                        if (g_str_has_prefix (ri->path, tmp))
                                paths[i] = g_strdup (ri->path + strlen (tmp) + 1);
                        else
                                paths[i] = g_strdup (ri->path);
                        angles[i] = ri->angle;
                }

                g_key_file_set_string (keyfile, key, "Name", name ? name : "");
                g_key_file_set_string (keyfile, key, "Author", author ? author : "");
                g_key_file_set_string (keyfile, key, "Description", description ? description : "");
                g_key_file_set_string (keyfile, key, "Cuisine", cuisine ? cuisine : "");
                g_key_file_set_string (keyfile, key, "Category", category ? category : "");
                g_key_file_set_string (keyfile, key, "PrepTime", prep_time ? prep_time : "");
                g_key_file_set_string (keyfile, key, "CookTime", cook_time ? cook_time : "");
                g_key_file_set_string (keyfile, key, "Ingredients", ingredients ? ingredients : "");
                g_key_file_set_string (keyfile, key, "Instructions", instructions ? instructions : "");
                g_key_file_set_string (keyfile, key, "Notes", notes ? notes : "");
                g_key_file_set_integer (keyfile, key, "Serves", serves);
                g_key_file_set_integer (keyfile, key, "Diets", diets);
                g_key_file_set_string_list (keyfile, key, "Images", (const char * const *)paths, images->len);
                g_key_file_set_integer_list (keyfile, key, "Angles", angles, images->len);
        }

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save recipe database: %s", error->message);
        }
}

static void
load_picks (GrRecipeStore *self, const char *dir)
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
                return;
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
}

static void
load_favorites (GrRecipeStore *self, const char *dir)
{
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;

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

        self->favorites = g_key_file_get_string_list (keyfile, "Content", "Favorites", NULL, &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load favorites: %s", error->message);
                }
                g_clear_error (&error);
        }
}

static void
save_favorites (GrRecipeStore *self)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        const char *key;
        g_autoptr(GError) error = NULL;
        const char *tmp;

        keyfile = g_key_file_new ();

        path = g_build_filename (get_user_data_dir (), "favorites.db", NULL);

        g_message ("Save favorites db: %s", path);

        g_key_file_set_string_list (keyfile, "Content", "Favorites", (const char * const *)self->favorites, g_strv_length (self->favorites));

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save recipe database: %s", error->message);
        }
}

static void
load_chefs (GrRecipeStore *self, const char *dir)
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
                return;
        }

        g_message ("Load chefs db: %s", path);

        groups = g_key_file_get_groups (keyfile, &length);
        for (i = 0; i < length; i++) {
                GrChef *chef;
                g_autofree char *name = NULL;
                g_autofree char *fullname = NULL;
                g_autofree char *description = NULL;
                g_autofree char *image_path = NULL;

                g_clear_error (&error);

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

                chef = g_hash_table_lookup (self->chefs, name);
                if (chef == NULL) {
                        chef = gr_chef_new ();
                        g_hash_table_insert (self->chefs, g_strdup (name), chef);
                }

                g_object_set (chef,
                              "name", name,
                              "fullname", fullname,
                              "description", description,
                              "image-path", image_path,
                              NULL);
        }
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

                name = gr_chef_get_name (chef);
                fullname = gr_chef_get_fullname (chef);
                description = gr_chef_get_description (chef);
                image_path = gr_chef_get_image (chef);

                tmp = get_user_data_dir ();
                if (g_str_has_prefix (image_path, tmp)) {
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
load_user (GrRecipeStore *self, const char *dir)
{
        g_autofree char *path = NULL;
        g_autofree char *contents = NULL;
        g_autoptr(GError) error = NULL;

        path = g_build_filename (dir, "user", NULL);
        if (!g_file_get_contents (path, &contents, NULL, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load user id: %s", error->message);
                else
                        g_message ("No user id at: %s", path);
                return;
        }

        g_message ("Load user id: %s", path);

        self->user = g_strdup (contents);

        if (self->user[strlen (self->user) - 1] == '\n')
                self->user[strlen (self->user) - 1] = '\0';
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
gr_recipe_store_init (GrRecipeStore *self)
{
        const char *dir;

        self->recipes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
        self->chefs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

        /* First load preinstalled data */
        dir = get_pkg_data_dir ();
        load_recipes (self, dir);
        load_chefs (self, dir);
        load_picks (self, dir);

        /* Now load saved data */
        dir = get_user_data_dir ();
        load_recipes (self, dir);
        load_favorites (self, dir);
        load_chefs (self, dir);
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

static gboolean
recipe_store_set (GrRecipeStore  *self,
                  GrRecipe       *recipe,
                  gboolean        fail_if_exists,
                  GError        **error)
{
        const char *name;

        name = gr_recipe_get_name (recipe);
        if (name == NULL || name[0] == '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("You need to provide a name for the recipe"));
                return FALSE;
        }

        if (fail_if_exists &&
            g_hash_table_contains (self->recipes, name)) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("A recipe with this name already exists"));
                return FALSE;
        }

        if (g_hash_table_insert (self->recipes, g_strdup (name), g_object_ref (recipe))) {
                g_signal_emit (self, add_signal, 0, recipe);
        }
        else {
                g_signal_emit (self, changed_signal, 0, recipe);
        }

        save_recipes (self);

        return TRUE;
}

gboolean
gr_recipe_store_add (GrRecipeStore  *self,
                     GrRecipe       *recipe,
                     GError        **error)
{
        return recipe_store_set (self, recipe, TRUE, error);
}

gboolean
gr_recipe_store_update (GrRecipeStore  *self,
                        GrRecipe       *recipe,
                        const char     *old_name,
                        GError        **error)
{
        gboolean ret;
        const char *name;

        g_object_ref (recipe);

        name = gr_recipe_get_name (recipe);

        if (g_strcmp0 (name, old_name) != 0) {
                g_hash_table_remove (self->recipes, old_name);
                ret = recipe_store_set (self, recipe, TRUE, error);
        }
        else {
                ret = recipe_store_set (self, recipe, FALSE, error);
        }

        g_object_unref (recipe);

        return ret;
}

gboolean
gr_recipe_store_remove (GrRecipeStore *self,
                        GrRecipe      *recipe)
{
        const char *name;
        gboolean ret = FALSE;

        name = gr_recipe_get_name (recipe);

        g_object_ref (recipe);

        if (g_hash_table_remove (self->recipes, name)) {
                g_signal_emit (self, remove_signal, 0, recipe);
                save_recipes (self);
                ret = TRUE;
        }

        g_object_unref (recipe);

        return ret;
}

GrRecipe *
gr_recipe_store_get (GrRecipeStore *self,
                     const char    *name)
{
        GrRecipe *recipe;

        recipe = g_hash_table_lookup (self->recipes, name);

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
gr_recipe_store_is_todays (GrRecipeStore *self,
                           GrRecipe      *recipe)
{
        const char *name;

        if (self->todays == NULL)
                return FALSE;

        name = gr_recipe_get_name (recipe);

        return g_strv_contains ((const char *const*)self->todays, name);
}

gboolean
gr_recipe_store_is_pick (GrRecipeStore *self,
                         GrRecipe      *recipe)
{
        const char *name;

        if (self->picks == NULL)
                return FALSE;

        name = gr_recipe_get_name (recipe);

        return g_strv_contains ((const char *const*)self->picks, name);
}

GrChef *
gr_recipe_store_get_chef (GrRecipeStore *self,
                          const char    *name)
{
        GrChef *chef;

        chef = g_hash_table_lookup (self->chefs, name);

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

static gboolean
recipe_store_set_chef (GrRecipeStore  *self,
                       GrChef         *chef,
                       GError        **error)
{
        const char *name;

        name = gr_chef_get_name (chef);

        if (name == NULL || name[0] == '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("You need to provide a name"));
                return FALSE;
        }

        if (g_hash_table_contains (self->chefs, name)) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Sorry, this name is taken"));
                return FALSE;
        }

        g_hash_table_insert (self->chefs, g_strdup (name), g_object_ref (chef));

        g_signal_emit (self, chefs_changed_signal, 0);
        save_chefs (self);

        return TRUE;
}

gboolean
gr_recipe_store_update_user (GrRecipeStore  *self,
                             GrChef         *chef,
                             GError        **error)
{
        const char *name;
        gboolean ret = TRUE;

        name = gr_chef_get_name (chef);

        if (name != NULL && name[0] != '\0') {
                if (g_strcmp0 (name, self->user) == 0) {
                        g_hash_table_remove (self->chefs, name);
                }
                ret = recipe_store_set_chef (self, chef, error);
        }

        if (ret) {
                g_free (self->user);
                self->user = g_strdup (name);
                save_user (self);
        }

        return ret;
}

gboolean
gr_recipe_store_chef_is_featured (GrRecipeStore *self,
                                  GrChef        *chef)
{
        const char *name;

        if (self->featured_chefs == NULL)
                return FALSE;

        name = gr_chef_get_name (chef);

        return g_strv_contains ((const char *const*)self->featured_chefs, name);
}

char **
gr_recipe_store_get_all_ingredients (GrRecipeStore *self, guint *length)
{
        GHashTableIter iter;
        GrRecipe *recipe;
        GHashTable *ingreds;
        char **result;
        int i;

        ingreds = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

        g_hash_table_iter_init (&iter, self->recipes);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&recipe)) {
                const char *ingredients;
                GrIngredientsList *list = NULL;
                g_autofree char **ret = NULL;

                ingredients = gr_recipe_get_ingredients (recipe);

                if (!ingredients || ingredients[0] == '\0')
                        continue;

                list = gr_ingredients_list_new (ingredients);
                ret = gr_ingredients_list_get_ingredients (list);
                for (i = 0; ret[i]; i++)
                        g_hash_table_add (ingreds, ret[i]);

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
        const char *name;

        name = gr_recipe_get_name (recipe);

        if (g_strv_contains ((const char * const*)self->favorites, name))
                return;

        length = g_strv_length (self->favorites);
        strv = g_new (char *, length + 2);
        strv[0] = g_strdup (name);
        for (i = 0; i < length; i++)
                strv[i + 1] = self->favorites[i];
        strv[length + 1] = NULL;

        g_free (self->favorites);
        self->favorites = strv;

        save_favorites (self);

        g_signal_emit (self, changed_signal, 0, recipe);
}

void
gr_recipe_store_remove_favorite (GrRecipeStore *self,
                                 GrRecipe      *recipe)
{
        int i, j;
        const char *name;

        name = gr_recipe_get_name (recipe);

        for (i = 0; self->favorites[i]; i++) {
                if (strcmp (self->favorites[i], name) == 0) {
                        g_free (self->favorites[i]);
                        for (j = i; self->favorites[j]; j++) {
                                self->favorites[j] = self->favorites[j + 1];
                        }
                        break;
                }
        }

        save_favorites (self);

        g_signal_emit (self, changed_signal, 0, recipe);
}

gboolean
gr_recipe_store_is_favorite (GrRecipeStore *self,
                             GrRecipe      *recipe)
{
        const char *name;

        if (self->favorites == NULL)
                return FALSE;

        name = gr_recipe_get_name (recipe);

        return g_strv_contains ((const char *const*)self->favorites, name);
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
