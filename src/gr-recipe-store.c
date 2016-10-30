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
#include <glib/gi18n.h>
#include <sys/statfs.h>

#include <glib.h>
#include <glib/gstdio.h>
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-utils.h"

struct _GrRecipeStore
{
        GObject parent;

        GHashTable *recipes;
        GHashTable *authors;

        char **todays;
        char **picks;
        char **chefs;

        char *user;
};


G_DEFINE_TYPE (GrRecipeStore, gr_recipe_store, G_TYPE_OBJECT)

static void
gr_recipe_store_finalize (GObject *object)
{
        GrRecipeStore *self = GR_RECIPE_STORE (object);

        g_clear_pointer (&self->recipes, g_hash_table_unref);
        g_clear_pointer (&self->authors, g_hash_table_unref);
        g_strfreev (self->todays);
        g_strfreev (self->picks);
        g_strfreev (self->chefs);
        g_free (self->user);

        G_OBJECT_CLASS (gr_recipe_store_parent_class)->finalize (object);
}

static char *
get_db_path (const char *kind)
{
        return g_build_filename (get_data_dir (), kind, NULL);
}

static void
load_recipes (GrRecipeStore *self)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *path = NULL;
        g_auto(GStrv) groups = NULL;
        gsize length;
        int i;

        keyfile = g_key_file_new ();

        path = get_db_path ("recipes.db");

        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("failed to load recipe db: %s", error->message);
                return;
        }

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
                int serves;
                GrDiets diets;
                
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
			tmp = get_db_path (image_path);
			g_free (image_path);
			image_path = tmp;
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
                              "image-path", image_path,
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
	char *tmp;

        keyfile = g_key_file_new ();

        path = get_db_path ("recipes.db");

        g_hash_table_iter_init (&iter, self->recipes);
        while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&recipe)) {
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
                int serves;
                GrDiets diets;

                g_object_get (recipe,
                              "name", &name,
                              "author", &author,
                              "description", &description,
                              "cuisine", &cuisine,
                              "category", &category,
                              "prep-time", &prep_time,
                              "cook-time", &cook_time,
                              "ingredients", &ingredients,
                              "instructions", &instructions,
                              "serves", &serves,
                              "diets", &diets,
                              "image-path", &image_path,
                              NULL);

		tmp = get_db_path ("");
		if (g_str_has_prefix (image_path, tmp)) {
			char *tmp2;
			tmp2 = g_strdup (image_path + strlen (tmp) + 1);
			g_free (image_path);
			image_path = tmp2;
		}
		g_free (tmp);

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
                g_key_file_set_string (keyfile, key, "Image", image_path ? image_path : "");
                g_key_file_set_integer (keyfile, key, "Serves", serves);
                g_key_file_set_integer (keyfile, key, "Diets", diets);
        }

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save recipe database: %s", error->message);
        }
}

static void
load_picks (GrRecipeStore *self)
{
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;

        keyfile = g_key_file_new ();

        path = get_db_path ("picks.db");

        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("failed to load picks: %s", error->message);
                return;
        }

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

        self->chefs = g_key_file_get_string_list (keyfile, "Content", "Chefs", NULL, &error);
        if (error) {
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                        g_warning ("Failed to load chefs: %s", error->message);
                }
                g_clear_error (&error);
        }
}

static void
load_authors (GrRecipeStore *self)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *path = NULL;
        g_auto(GStrv) groups = NULL;
        gsize length;
        int i;

        keyfile = g_key_file_new ();

        path = get_db_path ("authors.db");

        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("failed to load authors db: %s", error->message);
                return;
        }

        groups = g_key_file_get_groups (keyfile, &length);
        for (i = 0; i < length; i++) {
                GrAuthor *author;
                g_autofree char *name = NULL;
                g_autofree char *fullname = NULL;
                g_autofree char *description = NULL;
                g_autofree char *image_path = NULL;
                
                g_clear_error (&error);

                name = g_key_file_get_string (keyfile, groups[i], "Name", &error);
                if (error) {
                        g_warning ("Failed to load author %s: %s", groups[i], error->message);
                        g_clear_error (&error);
                        continue;
                }
                fullname = g_key_file_get_string (keyfile, groups[i], "Fullname", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load author %s: %s", groups[i], error->message);
                                continue;
                        }
                        g_clear_error (&error);
                }
                description = g_key_file_get_string (keyfile, groups[i], "Description", &error);
                if (error) {
                        if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                                g_warning ("Failed to load author %s: %s", groups[i], error->message);
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

		if (image_path && image_path[0] != '\0' && image_path[0] != '/') {
			char *tmp;
			tmp = get_db_path (image_path);
			g_free (image_path);
			image_path = tmp;
		}

                author = g_hash_table_lookup (self->authors, name);
                if (author == NULL) {
                        author = gr_author_new ();
                        g_hash_table_insert (self->authors, g_strdup (name), author);
                }

                g_object_set (author,
                              "name", name,
                              "fullname", fullname,
                              "description", description,
                              "image-path", image_path,
                              NULL); 
        }
}

static void
save_authors (GrRecipeStore *store)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        GHashTableIter iter;
        const char *key;
        GrAuthor *author;
        g_autoptr(GError) error = NULL;
	char *tmp;

        keyfile = g_key_file_new ();

        path = get_db_path ("authors.db");

        g_hash_table_iter_init (&iter, store->authors);
        while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&author)) {
                g_autofree char *name = NULL;
                g_autofree char *fullname = NULL;
                g_autofree char *description = NULL;
                g_autofree char *image_path = NULL;

                g_object_get (author,
                              "name", &name,
                              "fullname", &fullname,
                              "description", &description,
                              "image-path", &image_path,
                              NULL);

		tmp = get_db_path ("");
		if (g_str_has_prefix (image_path, tmp)) {
			char *tmp2;
			tmp2 = g_strdup (image_path + strlen (tmp) + 1);
			g_free (image_path);
			image_path = tmp2;
		}
		g_free (tmp);

                g_key_file_set_string (keyfile, key, "Name", name ? name : "");
                g_key_file_set_string (keyfile, key, "Fullname", fullname ? fullname : "");
                g_key_file_set_string (keyfile, key, "Description", description ? description : "");
                g_key_file_set_string (keyfile, key, "Image", image_path ? image_path : "");
        }

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save authors database: %s", error->message);
        }
}

static void
load_user (GrRecipeStore *self)
{
        g_autofree char *path = NULL;
        g_autofree char *contents = NULL;
        g_autoptr(GError) error = NULL;

        path = get_db_path ("user");
        if (!g_file_get_contents (path, &contents, NULL, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load user id: %s", error->message);
                return;
        }

        self->user = g_strdup (contents);

        if (self->user[strlen (self->user) - 1] == '\n')
                self->user[strlen (self->user) - 1] = '\0';
}

static void
save_user (GrRecipeStore *self)
{
        g_autofree char *path = NULL;

        path = get_db_path ("user");
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
        self->recipes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
        self->authors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

        load_recipes (self);
        load_picks (self);
        load_authors (self);
        load_user (self);
}

static guint add_signal;
static guint remove_signal;
static guint changed_signal;
static guint authors_changed_signal;

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
        authors_changed_signal = g_signal_new ("authors-changed",
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
        g_autofree char *name = NULL;

        g_object_get (recipe, "name", &name, NULL);
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
        g_autofree char *name = NULL;

        g_object_ref (recipe);

        g_object_get (recipe, "name", &name, NULL);
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
        g_autofree char *name = NULL;
        gboolean ret = FALSE;

        g_object_get (recipe, "name", &name, NULL);

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
gr_recipe_store_get_keys (GrRecipeStore *self,
                          guint         *length)
{
        return (char **)g_hash_table_get_keys_as_array (self->recipes, length);
}

gboolean
gr_recipe_store_is_todays (GrRecipeStore *self,
                           GrRecipe      *recipe)
{
        g_autofree char *name = NULL;

        if (self->todays == NULL)
                return FALSE;

        g_object_get (recipe, "name", &name, NULL);

        return g_strv_contains ((const char *const*)self->todays, name);
}

gboolean
gr_recipe_store_is_pick (GrRecipeStore *self,
                         GrRecipe      *recipe)
{
        g_autofree char *name = NULL;

        if (self->picks == NULL)
                return FALSE;

        g_object_get (recipe, "name", &name, NULL);

        return g_strv_contains ((const char *const*)self->picks, name);
}

GrAuthor *
gr_recipe_store_get_author (GrRecipeStore *self,
                            const char    *name)
{
        GrAuthor *author;

        author = g_hash_table_lookup (self->authors, name);

        if (author)
                return g_object_ref (author);

        return NULL;
}

char **
gr_recipe_store_get_author_keys (GrRecipeStore *self,
                                 guint         *length)
{
        return (char **)g_hash_table_get_keys_as_array (self->authors, length);
}

const char *
gr_recipe_store_get_user_key (GrRecipeStore *self)
{
        return self->user;
}

static gboolean
recipe_store_set_author (GrRecipeStore  *self,
                         GrAuthor       *author,
                         GError        **error)
{
        g_autofree char *name = NULL;

        g_object_get (author, "name", &name, NULL);
        if (name == NULL || name[0] == '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("You need to provide a name"));
                return FALSE;
        }

        if (g_hash_table_contains (self->authors, name)) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Sorry, this name is taken"));
                return FALSE;
        }

        g_hash_table_insert (self->authors, g_strdup (name), g_object_ref (author));

        g_signal_emit (self, authors_changed_signal, 0);
        save_authors (self);

        return TRUE;
}

gboolean
gr_recipe_store_update_user (GrRecipeStore  *self,
                             GrAuthor       *author,
                             GError        **error)
{
        g_autofree char *name = NULL;
        gboolean ret = TRUE;

        g_object_get (author, "name", &name, NULL);

        if (name != NULL && name[0] != '\0') {
                if (g_strcmp0 (name, self->user) == 0) {
                        g_hash_table_remove (self->authors, name);
                }
                ret = recipe_store_set_author (self, author, error);
        }

        if (ret) {
                g_free (self->user);
                self->user = g_strdup (name);
                save_user (self);
        }

        return ret;
}

gboolean
gr_recipe_store_author_is_featured (GrRecipeStore *self,
                                    GrAuthor      *author)
{
        g_autofree char *name = NULL;

        if (self->chefs == NULL)
                return FALSE;

        g_object_get (author, "name", &name, NULL);

        return g_strv_contains ((const char *const*)self->chefs, name);
}

