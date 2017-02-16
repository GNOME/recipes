/* gr-recipe-importer.c:
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
#include <glib/gi18n.h>

#ifdef ENABLE_AUTOAR
#include <gnome-autoar/gnome-autoar.h>
#include "glnx-shutil.h"
#endif

#include "gr-recipe-importer.h"
#include "gr-images.h"
#include "gr-chef.h"
#include "gr-recipe.h"
#include "gr-recipe-store.h"
#include "gr-app.h"
#include "gr-utils.h"


struct _GrRecipeImporter
{
        GObject parent_instance;

        GtkWindow *window;

#ifdef ENABLE_AUTOAR
        AutoarExtractor *extractor;
#endif
        GFile *output;
        char *dir;

        GKeyFile *chefs_keyfile;
        char **chef_ids;
        int current_chef;

        GHashTable *chef_id_map;
        char *chef_id;
        char *chef_name;
        char *chef_fullname;
        char *chef_description;
        char *chef_image_path;

        GtkWidget *new_chef_name;
        GtkWidget *new_chef_fullname;
        GtkWidget *new_chef_description;

        GKeyFile *recipes_keyfile;
        char **recipe_ids;
        int current_recipe;

        char *recipe_id;
        char *recipe_name;
        char *recipe_author;
        char *recipe_description;
        char *recipe_cuisine;
        char *recipe_season;
        char *recipe_category;
        char *recipe_prep_time;
        char *recipe_cook_time;
        char *recipe_ingredients;
        char *recipe_instructions;
        char *recipe_notes;
        char **recipe_paths;
        int *recipe_angles;
        gboolean *recipe_dark;
        int recipe_serves;
        GrDiets recipe_diets;
        GDateTime *recipe_ctime;
        GDateTime *recipe_mtime;

        GList *recipes;
};

G_DEFINE_TYPE (GrRecipeImporter, gr_recipe_importer, G_TYPE_OBJECT)

static void
gr_recipe_importer_finalize (GObject *object)
{
        GrRecipeImporter *importer = GR_RECIPE_IMPORTER (object);

#ifdef ENABLE_AUTOAR
        g_clear_object (&importer->extractor);
#endif
        g_clear_object (&importer->output);
        g_free (importer->dir);

        g_clear_pointer (&importer->chefs_keyfile, g_key_file_unref);
        g_clear_pointer (&importer->chef_ids, g_strfreev);

        g_clear_pointer (&importer->chef_id_map, g_hash_table_unref);
        g_free (importer->chef_id);
        g_free (importer->chef_name);
        g_free (importer->chef_fullname);
        g_free (importer->chef_description);
        g_free (importer->chef_image_path);

        g_clear_pointer (&importer->recipes_keyfile, g_key_file_unref);
        g_clear_pointer (&importer->recipe_ids, g_strfreev);

        g_free (importer->recipe_id);
        g_free (importer->recipe_name);
        g_free (importer->recipe_author);
        g_free (importer->recipe_description);
        g_free (importer->recipe_cuisine);
        g_free (importer->recipe_season);
        g_free (importer->recipe_category);
        g_free (importer->recipe_prep_time);
        g_free (importer->recipe_cook_time);
        g_free (importer->recipe_ingredients);
        g_free (importer->recipe_instructions);
        g_free (importer->recipe_notes);
        g_strfreev (importer->recipe_paths);
        g_free (importer->recipe_angles);
        g_free (importer->recipe_dark);
        g_clear_pointer (&importer->recipe_ctime, g_date_time_unref);
        g_clear_pointer (&importer->recipe_mtime, g_date_time_unref);
        g_list_free_full (importer->recipes, g_object_unref);

        G_OBJECT_CLASS (gr_recipe_importer_parent_class)->finalize (object);
}

static guint done_signal;

static void
gr_recipe_importer_class_init (GrRecipeImporterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_recipe_importer_finalize;

        done_signal = g_signal_new ("done",
                                    G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL,
                                    NULL,
                                    G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
gr_recipe_importer_init (GrRecipeImporter *self)
{
        self->current_chef = -1;
        self->current_recipe = -1;
        self->chef_id_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

GrRecipeImporter *
gr_recipe_importer_new (GtkWindow *parent)
{
        GrRecipeImporter *importer;

        importer = g_object_new (GR_TYPE_RECIPE_IMPORTER, NULL);

        importer->window = parent;

        return importer;
}

static void
cleanup_import (GrRecipeImporter *importer)
{
#ifdef ENABLE_AUTOAR
        g_autoptr(GError) error = NULL;

        if (!glnx_shutil_rm_rf_at (-1, importer->dir, NULL, &error))
                g_warning ("Failed to clean up temp directory %s: %s", importer->dir, error->message);
        g_clear_object (&importer->extractor);
#endif

        g_clear_pointer (&importer->dir, g_free);
        g_clear_object (&importer->output);

        g_clear_pointer (&importer->chefs_keyfile, g_key_file_unref);
        g_clear_pointer (&importer->chef_ids, g_strfreev);
        importer->current_chef = -1;

        g_hash_table_remove_all (importer->chef_id_map);

        g_clear_pointer (&importer->chef_id, g_free);
        g_clear_pointer (&importer->chef_name, g_free);
        g_clear_pointer (&importer->chef_fullname, g_free);
        g_clear_pointer (&importer->chef_description, g_free);
        g_clear_pointer (&importer->chef_image_path, g_free);

        g_clear_pointer (&importer->recipes_keyfile, g_key_file_unref);
        g_clear_pointer (&importer->recipe_ids, g_strfreev);
        importer->current_recipe = -1;

        g_clear_pointer (&importer->recipe_id, g_free);
        g_clear_pointer (&importer->recipe_name, g_free);
        g_clear_pointer (&importer->recipe_author, g_free);
        g_clear_pointer (&importer->recipe_description, g_free);
        g_clear_pointer (&importer->recipe_cuisine, g_free);
        g_clear_pointer (&importer->recipe_season, g_free);
        g_clear_pointer (&importer->recipe_category, g_free);
        g_clear_pointer (&importer->recipe_prep_time, g_free);
        g_clear_pointer (&importer->recipe_cook_time, g_free);
        g_clear_pointer (&importer->recipe_ingredients, g_free);
        g_clear_pointer (&importer->recipe_instructions, g_free);
        g_clear_pointer (&importer->recipe_notes, g_free);
        g_clear_pointer (&importer->recipe_paths, g_strfreev);
        g_clear_pointer (&importer->recipe_angles, g_free);
        g_clear_pointer (&importer->recipe_dark, g_free);
        g_clear_pointer (&importer->recipe_ctime, g_date_time_unref);
        g_clear_pointer (&importer->recipe_mtime, g_date_time_unref);

        g_list_free_full (importer->recipes, g_object_unref);
        importer->recipes = NULL;
}

static void
error_cb (gpointer          extractor,
          GError           *error,
          GrRecipeImporter *importer)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (importer->window,
                                         GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("Error while importing recipe:\n%s"),
                                         error->message);
        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_widget_show (dialog);

        cleanup_import (importer);
}

#ifdef ENABLE_AUTOAR
static gboolean
copy_image (GrRecipeImporter  *importer,
            const char        *path,
            char             **new_path,
            GError           **error)
{
        g_autofree char *srcpath = NULL;
        g_autofree char *destpath = NULL;
        g_autoptr(GFile) source = NULL;
        g_autoptr(GFile) dest = NULL;
        g_autofree char *orig_dest = NULL;
        int i;

        srcpath = g_build_filename (importer->dir, path, NULL);
        source = g_file_new_for_path (srcpath);
        orig_dest = g_build_filename (g_get_user_data_dir (), "recipes", path, NULL);

        destpath = g_strdup (orig_dest);
        for (i = 1; i < 10; i++) {
                if (!g_file_test (destpath, G_FILE_TEST_EXISTS))
                        break;
                g_free (destpath);
                destpath = g_strdup_printf ("%s%d", orig_dest, i);
        }
        dest = g_file_new_for_path (destpath);

        if (!g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, error)) {
                return FALSE;
        }

        *new_path = g_strdup (destpath);

        return TRUE;
}

static gboolean
import_recipe (GrRecipeImporter *importer)
{
        GrRecipeStore *store;
        g_autoptr(GArray) images = NULL;
        g_autoptr(GrRecipe) recipe = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *id = NULL;
        const char *author;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        images = gr_rotated_image_array_new ();
        if (importer->recipe_paths) {
                int i;
                for (i = 0; importer->recipe_paths[i]; i++) {
                        GrRotatedImage ri;
                        char *new_path;

                        if (!copy_image (importer, importer->recipe_paths[i], &new_path, &error)) {
                                error_cb (importer->extractor, error, importer);
                                return FALSE;
                        }

                        ri.path = new_path;
                        ri.angle = importer->recipe_angles[i];
                        g_array_append_val (images, ri);
                }
        }

        author = (const char *)g_hash_table_lookup (importer->chef_id_map, importer->recipe_author);
        id = generate_id ("R_", importer->recipe_name, "_by_", author, NULL);

        recipe = gr_recipe_new ();
        g_object_set (recipe,
                      "id", id,
                      "name", importer->recipe_name,
                      "author", author,
                      "description", importer->recipe_description,
                      "cuisine", importer->recipe_cuisine,
                      "season", importer->recipe_season,
                      "category", importer->recipe_category,
                      "prep-time", importer->recipe_prep_time,
                      "cook-time", importer->recipe_cook_time,
                      "ingredients", importer->recipe_ingredients,
                      "instructions", importer->recipe_instructions,
                      "notes", importer->recipe_notes,
                      "serves", importer->recipe_serves,
                      "diets", importer->recipe_diets,
                      "images", images,
                      "ctime", importer->recipe_ctime,
                      "mtime", importer->recipe_mtime,
                      NULL);

        if (!gr_recipe_store_add_recipe (store, recipe, &error)) {
                error_cb (importer->extractor, error, importer);
                return FALSE;
        }

        importer->recipes = g_list_append (importer->recipes, g_object_ref (recipe));

        return TRUE;
}

static gboolean import_next_recipe (GrRecipeImporter *importer);

static void
recipe_dialog_response (GtkWidget        *dialog,
                        int               response_id,
                        GrRecipeImporter *importer)
{
        if (response_id == GTK_RESPONSE_CANCEL) {
                g_message ("Not importing recipe %s", importer->recipe_name);
                gtk_widget_destroy (dialog);
        }
        else {
                g_free (importer->recipe_name);
                importer->recipe_name = g_strdup (g_object_get_data (G_OBJECT (dialog), "name"));
                g_message ("Renaming recipe to %s while importing", importer->recipe_name);
                gtk_widget_destroy (dialog);
                if (!import_recipe (importer))
                        return;
        }

        import_next_recipe (importer);
}

static void
recipe_name_changed (GtkEntry         *entry,
                     GrRecipeImporter *importer)
{
        GrRecipeStore *store;
        const char *name;
        const char *author;
        g_autofree char *id = NULL;
        GtkWidget *dialog;
        g_autoptr(GrRecipe) recipe = NULL;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        name = gtk_entry_get_text (entry);
        author = (const char *)g_hash_table_lookup (importer->chef_id_map, importer->recipe_author);
        id = generate_id ("R_", name, "_by_", author, NULL);

        recipe = gr_recipe_store_get_recipe (store, id);

        dialog = gtk_widget_get_ancestor (GTK_WIDGET (entry), GTK_TYPE_DIALOG);
        g_object_set_data_full (G_OBJECT (dialog), "name", g_strdup (name), g_free);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_APPLY,
                                           name[0] != 0 && recipe == NULL);
}

static void
show_recipe_conflict_dialog (GrRecipeImporter *importer)
{
        g_autoptr(GtkBuilder) builder = NULL;
        GtkWidget *dialog;
        GtkWidget *name_entry;

        builder = gtk_builder_new_from_resource ("/org/gnome/Recipes/recipe-conflict-dialog.ui");
        dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (importer->window));

        name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
        gtk_entry_set_text (GTK_ENTRY (name_entry), importer->recipe_name);

        g_signal_connect (name_entry, "changed", G_CALLBACK (recipe_name_changed), importer);
        g_signal_connect (dialog, "response", G_CALLBACK (recipe_dialog_response), importer);
        gtk_widget_show (dialog);
}

#define handle_or_clear_error(error) \
        if (error) { \
                if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) { \
                        error_cb (importer->extractor, error, importer); \
                        return FALSE; \
                } \
                g_clear_error (&error); \
        }

#define key_file_get_string(keyfile, group, key) ({ \
                g_autoptr(GError) my_error = NULL; \
                char *my_tmp = g_key_file_get_string (keyfile, group, key, &my_error); \
                handle_or_clear_error (my_error) \
                my_tmp; })

static gboolean
import_next_recipe (GrRecipeImporter *importer)
{
        char *tmp;
        GrRecipeStore *store;
        g_autoptr(GrRecipe) recipe = NULL;
        gsize length2, length3;
        g_autoptr(GError) error = NULL;
        const char *id;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

next:
        importer->current_recipe++;

        id = importer->recipe_ids[importer->current_recipe];

        if (id == NULL) {
                g_signal_emit (importer, done_signal, 0, importer->recipes);
                cleanup_import (importer);
                return TRUE;
        }

        importer->recipe_id = g_strdup (id);
        importer->recipe_name = g_key_file_get_string (importer->recipes_keyfile, id, "Name", &error);
        if (error) {
                error_cb (importer->extractor, error, importer);
                return FALSE;
        }
        importer->recipe_author = key_file_get_string (importer->recipes_keyfile, id, "Author");
        importer->recipe_description = key_file_get_string (importer->recipes_keyfile, id, "Description");
        importer->recipe_cuisine = key_file_get_string (importer->recipes_keyfile, id, "Cuisine");
        importer->recipe_season = key_file_get_string (importer->recipes_keyfile, id, "Season");
        importer->recipe_category = key_file_get_string (importer->recipes_keyfile, id, "Category");
        importer->recipe_prep_time = key_file_get_string (importer->recipes_keyfile, id, "PrepTime");
        importer->recipe_cook_time = key_file_get_string (importer->recipes_keyfile, id, "CookTime");
        importer->recipe_ingredients = key_file_get_string (importer->recipes_keyfile, id, "Ingredients");
        importer->recipe_instructions = key_file_get_string (importer->recipes_keyfile, id, "Instructions");
        importer->recipe_notes = key_file_get_string (importer->recipes_keyfile, id, "Notes");
        importer->recipe_serves = g_key_file_get_integer (importer->recipes_keyfile, id, "Serves", &error);
        handle_or_clear_error (error);
        importer->recipe_diets = g_key_file_get_integer (importer->recipes_keyfile, id, "Diets", &error);
        handle_or_clear_error (error);
        tmp = key_file_get_string (importer->recipes_keyfile, id, "Created");
        if (tmp) {
               importer->recipe_ctime = date_time_from_string (tmp);
               if (!importer->recipe_ctime) {
                        g_free (tmp);
                        g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                     _("Failed to load recipe: Couldn’t parse Created key"));
                        error_cb (importer->extractor, error, importer);
                        return FALSE;
                }
        }
        tmp = key_file_get_string (importer->recipes_keyfile, id, "Modified");
        if (tmp) {
               importer->recipe_mtime = date_time_from_string (tmp);
               if (!importer->recipe_mtime) {
                        g_free (tmp);
                        g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                     _("Failed to load recipe: Couldn’t parse Modified key"));
                        error_cb (importer->extractor, error, importer);
                        return FALSE;
                }
        }
        importer->recipe_paths = g_key_file_get_string_list (importer->recipes_keyfile, id, "Images", &length2, &error);
        handle_or_clear_error (error);
        importer->recipe_angles = g_key_file_get_integer_list (importer->recipes_keyfile, id, "Angles", &length3, &error);
        handle_or_clear_error (error);
        if (length2 != length3) {
                g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Failed to load recipe: Images and Angles length mismatch"));
                error_cb (importer->extractor, error, importer);
                return FALSE;
        }
        importer->recipe_dark = g_key_file_get_boolean_list (importer->recipes_keyfile, id, "DarkText", &length3, &error);
        handle_or_clear_error (error);
        if (length2 != length3) {
                g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Failed to load recipe: Images and DarkText length mismatch"));
                error_cb (importer->extractor, error, importer);
                return FALSE;
        }

        recipe = gr_recipe_store_get_recipe (store, importer->recipe_id);
        if (!recipe) {
                g_message ("Recipe %s not yet known; importing", importer->recipe_id);
                if (!import_recipe (importer))
                        return FALSE;
                goto next;
        }

        show_recipe_conflict_dialog (importer);

        return TRUE;
}

static gboolean
import_recipes (GrRecipeImporter *importer)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        g_auto(GStrv) groups = NULL;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GError) error = NULL;

        g_assert (importer->recipes_keyfile == NULL);
        g_assert (importer->recipe_ids == NULL);
        g_assert (importer->current_recipe == -1);

        keyfile = g_key_file_new ();
        path = g_build_filename (importer->dir, "recipes.db", NULL);
        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                error_cb (importer->extractor, error, importer);
                return FALSE;
        }

        importer->recipes_keyfile = g_key_file_ref (keyfile);
        importer->recipe_ids = g_key_file_get_groups (keyfile, NULL);

        return import_next_recipe (importer);
}

static gboolean
import_chef (GrRecipeImporter *importer)
{
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GError) error = NULL;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        if (importer->chef_image_path) {
                char *new_path;

                if (!copy_image (importer, importer->chef_image_path, &new_path, &error)) {
                        error_cb (importer->extractor, error, importer);
                        return FALSE;
                }

                g_free (importer->chef_image_path);
                importer->chef_image_path = new_path;
        }

        chef = g_object_new (GR_TYPE_CHEF,
                             "id", importer->chef_id,
                             "name", importer->chef_name,
                             "fullname", importer->chef_fullname,
                             "description", importer->chef_description,
                             "image-path", importer->chef_image_path,
                             NULL);

        g_clear_pointer (&importer->chef_id, g_free);
        g_clear_pointer (&importer->chef_name, g_free);
        g_clear_pointer (&importer->chef_fullname, g_free);
        g_clear_pointer (&importer->chef_description, g_free);
        g_clear_pointer (&importer->chef_image_path, g_free);

        if (!gr_recipe_store_add_chef (store, chef, &error)) {
                error_cb (importer->extractor, error, importer);
                return FALSE;
        }

        return TRUE;
}

static char *
find_unused_chef_id (const char *base)
{
        GrRecipeStore *store;
        int i;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        for (i = 0; i < 100; i++) {
                g_autofree char *new_id = NULL;
                g_autoptr(GrChef) chef = NULL;

                new_id = g_strdup_printf ("%s%d", base, i);
                chef = gr_recipe_store_get_chef (store, new_id);
                if (!chef)
                        return g_strdup (new_id);
        }

        return NULL;
}

static char *
get_text_view_text (GtkTextView *textview)
{
        GtkTextBuffer *buffer;
        GtkTextIter start, end;

        buffer = gtk_text_view_get_buffer (textview);
        gtk_text_buffer_get_bounds (buffer, &start, &end);

        return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static gboolean import_next_chef (GrRecipeImporter *importer);

static void
chef_dialog_response (GtkWidget        *dialog,
                      int               response_id,
                      GrRecipeImporter *importer)
{
        if (response_id == GTK_RESPONSE_CANCEL) {
                g_message ("Chef %s known after all; not importing", importer->chef_id);
                gtk_widget_destroy (dialog);
        }
        else {
                char *id = find_unused_chef_id (importer->chef_id);

                g_hash_table_insert (importer->chef_id_map, g_strdup (importer->chef_id), g_strdup (id));

                g_free (importer->chef_id);
                g_free (importer->chef_name);
                g_free (importer->chef_fullname);
                g_free (importer->chef_description);

                importer->chef_id = id;
                importer->chef_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (importer->new_chef_name)));
                importer->chef_fullname = g_strdup (gtk_entry_get_text (GTK_ENTRY (importer->new_chef_fullname)));
                importer->chef_description = get_text_view_text (GTK_TEXT_VIEW (importer->new_chef_description));

                g_message ("Renaming chef to %s while importing", importer->chef_id);
                gtk_widget_destroy (dialog);
                if (!import_chef (importer))
                        return;
        }

        import_next_chef (importer);
}

static void
show_chef_conflict_dialog (GrRecipeImporter *importer,
                           GrChef           *chef)
{
        g_autoptr(GtkBuilder) builder = NULL;
        GtkWidget *dialog;
        GtkWidget *old_chef_name;
        GtkWidget *old_chef_fullname;
        GtkWidget *old_chef_description;
        GtkWidget *old_chef_picture;
        GtkWidget *new_chef_name;
        GtkWidget *new_chef_fullname;
        GtkWidget *new_chef_description;
        GtkWidget *new_chef_picture;
        GtkTextBuffer *buffer;

        builder = gtk_builder_new_from_resource ("/org/gnome/Recipes/chef-conflict-dialog.ui");
        dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (importer->window));

        old_chef_name = GTK_WIDGET (gtk_builder_get_object (builder, "old_chef_name"));
        old_chef_fullname = GTK_WIDGET (gtk_builder_get_object (builder, "old_chef_fullname"));
        old_chef_description = GTK_WIDGET (gtk_builder_get_object (builder, "old_chef_description"));
        old_chef_picture = GTK_WIDGET (gtk_builder_get_object (builder, "old_chef_picture"));
        gtk_entry_set_text (GTK_ENTRY (old_chef_name), gr_chef_get_name (chef));
        gtk_entry_set_text (GTK_ENTRY (old_chef_fullname), gr_chef_get_fullname (chef));
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (old_chef_description));
        gtk_text_buffer_set_text (buffer, gr_chef_get_description (chef), -1);
        if (gr_chef_get_image (chef) != NULL) {
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                pixbuf = load_pixbuf_fit_size (gr_chef_get_image (chef), 0, 64, 64, TRUE);
                gtk_image_set_from_pixbuf (GTK_IMAGE (old_chef_picture), pixbuf);
        }

        new_chef_name = GTK_WIDGET (gtk_builder_get_object (builder, "new_chef_name"));
        new_chef_fullname = GTK_WIDGET (gtk_builder_get_object (builder, "new_chef_fullname"));
        new_chef_description = GTK_WIDGET (gtk_builder_get_object (builder, "new_chef_description"));
        new_chef_picture = GTK_WIDGET (gtk_builder_get_object (builder, "new_chef_picture"));
        gtk_entry_set_text (GTK_ENTRY (new_chef_name), importer->chef_name);
        gtk_entry_set_text (GTK_ENTRY (new_chef_fullname), importer->chef_fullname);
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (new_chef_description));
        gtk_text_buffer_set_text (buffer, importer->chef_description, -1);
        if (importer->chef_image_path != NULL) {
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                g_autofree char *path = NULL;
                path = g_build_filename (importer->dir, importer->chef_image_path, NULL);
                pixbuf = load_pixbuf_fit_size (path, 0, 64, 64, TRUE);
                gtk_image_set_from_pixbuf (GTK_IMAGE (new_chef_picture), pixbuf);
        }

        importer->new_chef_name = new_chef_name;
        importer->new_chef_fullname = new_chef_fullname;
        importer->new_chef_description = new_chef_description;

        g_signal_connect (dialog, "response", G_CALLBACK (chef_dialog_response), importer);
        gtk_widget_show (dialog);
}

static gboolean
import_next_chef (GrRecipeImporter *importer)
{
        const char *id;
        g_autoptr(GrChef) chef = NULL;
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

next:
        importer->current_chef++;

        id = importer->chef_ids[importer->current_chef];

        if (id == NULL)
                return import_recipes (importer);

        importer->chef_id = g_strdup (id);
        importer->chef_name = key_file_get_string (importer->chefs_keyfile, id, "Name");
        importer->chef_fullname = key_file_get_string (importer->chefs_keyfile, id, "Fullname");
        importer->chef_description = key_file_get_string (importer->chefs_keyfile, id, "Description");
        importer->chef_image_path = key_file_get_string (importer->chefs_keyfile, id, "Image");

        g_hash_table_insert (importer->chef_id_map, g_strdup (importer->chef_id), g_strdup (id));
        chef = gr_recipe_store_get_chef (store, id);
        if (!chef) {
                g_message ("Chef %s not yet known; importing", id);
                import_chef (importer);
                goto next;
        }

        if (g_strcmp0 (importer->chef_fullname, gr_chef_get_fullname (chef)) == 0 &&
            g_strcmp0 (importer->chef_name, gr_chef_get_name (chef)) == 0 &&
            g_strcmp0 (importer->chef_description, gr_chef_get_description (chef)) == 0) {
                g_message ("Chef %s already known, not importing", id);
                goto next;
        }

        show_chef_conflict_dialog (importer, chef);

        return TRUE;
}

static gboolean
import_chefs (GrRecipeImporter *importer)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        g_auto(GStrv) groups = NULL;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GError) error = NULL;

        g_assert (importer->chefs_keyfile == NULL);
        g_assert (importer->chef_ids == NULL);
        g_assert (importer->current_chef == -1);

        keyfile = g_key_file_new ();
        path = g_build_filename (importer->dir, "chefs.db", NULL);
        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                error_cb (importer->extractor, error, importer);
                return FALSE;
        }

        importer->chefs_keyfile = g_key_file_ref (keyfile);
        importer->chef_ids = g_key_file_get_groups (keyfile, NULL);
        return import_next_chef (importer);
}

static void
finish_import (GrRecipeImporter *importer)
{
        import_chefs (importer);
}

static void
completed_cb (AutoarExtractor  *extractor,
              GrRecipeImporter *importer)
{
        finish_import (importer);
}
#endif

static gboolean
in_element (GMarkupParseContext *context,
            ...)
{
        va_list args;
        gboolean result;
        char *next;
        const GSList *stack;

        va_start (args, context);

        result = TRUE;

        stack = g_markup_parse_context_get_element_stack (context);

        while ((next = va_arg (args, char*)) != NULL) {
                const char *elt;

                if (stack == NULL) {
                        result = FALSE;
                        break;
                }

                elt = stack->data;
                stack = stack->next;

                if (strcmp (next, elt) != 0) {
                        result = FALSE;
                        break;
                }
        }

        va_end (args);

        return result;
}

static const char *
find_attribute (const char *name,
                const char **names,
                const char **values)
{
        int i;

        for (i = 0; names[i]; i++) {
                if (strcmp (name, names[i]) == 0)
                        return values[i];
        }

        return NULL;
}

typedef struct {
        gboolean collecting_text;
        GString *text;

        char *author;
        char *name;
        GList *categories;
        int yield;
        char *prep_time;
        char *instructions;

        char *ing_group;
        char *ing_name;
        double ing_amount;
        char *ing_unit;
        GString *ingredients;

        GList *objects;
} ParserData;

static void
kreml_start_element (GMarkupParseContext *context,
                     const gchar         *element_name,
                     const gchar        **attribute_names,
                     const gchar        **attribute_values,
                     gpointer             user_data,
                     GError             **error)
{
        ParserData *pd = user_data;

        if (in_element (context, "title", "krecipes-description", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
        else if (in_element (context, "author", "krecipes-description", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
        else if (in_element (context, "cat", "category", "krecipes-description", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
        else if (in_element (context, "amount", "yield", "krecipes-description", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
        else if (in_element (context, "preparation-time", "krecipes-description", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
        else if (in_element (context, "krecipes-instructions", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
        else if (in_element (context, "krecipes-ingredients", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->ingredients, 0);
        }
        else if (in_element (context, "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                pd->ing_group = g_strdup (find_attribute ("name", attribute_names, attribute_values));
        }
        else if (in_element (context, "name", "ingredient", "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
        else if (in_element (context, "amount", "ingredient", "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
        else if (in_element (context, "unit", "ingredient", "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                g_string_set_size (pd->text, 0);
                pd->collecting_text = TRUE;
        }
}

static void
kreml_end_element (GMarkupParseContext *context,
                   const gchar         *element_name,
                   gpointer             user_data,
                   GError             **error)
{
        ParserData *pd = user_data;

        if (in_element (context, "title", "krecipes-description", NULL)) {
                pd->collecting_text = FALSE;
                pd->name = g_strdup (pd->text->str);
        }
        else if (in_element (context, "author", "krecipes-description", "krecipes-recipe", NULL)) {
                pd->collecting_text = FALSE;
                pd->author = g_strdup (pd->text->str);
        }
        else if (in_element (context, "cat", "category", "krecipes-description", "krecipes-recipe", NULL)) {
                char *cat;

                pd->collecting_text = FALSE;
                cat = g_strdup (pd->text->str);
                pd->categories = g_list_prepend (pd->categories, cat);
        }
        else if (in_element (context, "amount", "yield", "krecipes-description", "krecipes-recipe", NULL)) {
                g_autofree char *cat = NULL;

                pd->collecting_text = FALSE;
                cat = g_strdup (pd->text->str);
                pd->yield = atoi (cat);
        }
        else if (in_element (context, "preparation-time", "krecipes-description", "krecipes-recipe", NULL)) {
                pd->collecting_text = FALSE;
                pd->prep_time = g_strdup (pd->text->str);
        }
        else if (in_element (context, "krecipes-instructions", "krecipes-recipe", NULL)) {
                pd->collecting_text = FALSE;
                pd->instructions = g_strdup (pd->text->str);
        }
        else if (in_element (context, "name", "ingredient", "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                pd->collecting_text = FALSE;
                pd->ing_name = g_strdup (pd->text->str);
        }
        else if (in_element (context, "amount", "ingredient", "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                g_autofree char *amount = NULL;

                pd->collecting_text = FALSE;
                amount = g_strdup (pd->text->str);
                pd->ing_amount = atof (amount);
        }
        else if (in_element (context, "unit", "ingredient", "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                pd->collecting_text = FALSE;
                pd->ing_unit = g_strdup (pd->text->str);
        }
        else if (in_element (context, "ingredient", "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                if (pd->ingredients->len > 0)
                        g_string_append (pd->ingredients, "\n");
                g_string_append_printf (pd->ingredients, "%g\t%s\t\%s\t%s",
                                        pd->ing_amount, pd->ing_unit, pd->ing_name, pd->ing_group ? pd->ing_group : "");
                g_clear_pointer (&pd->ing_unit, g_free);
                g_clear_pointer (&pd->ing_name, g_free);
                pd->ing_amount = 0;
        }
        else if (in_element (context, "ingredient-group", "krecipes-ingredients", "krecipes-recipe", NULL)) {
                g_clear_pointer (&pd->ing_group, g_free);
        }

        if (strcmp (element_name, "krecipes-recipe") == 0) {
                GrRecipe *recipe;
                GrChef *chef;
                g_auto(GStrv) strv = NULL;
                g_autofree char *chef_id = NULL;
                g_autofree char *id = NULL;

                strv = g_strsplit (pd->author, " ", -1);
                if (strv[1])
                        chef_id = generate_id (strv[0], "_", strv[1], NULL);
                else
                        chef_id = generate_id (strv[0], NULL);

                /* FIXME: find chef by name */
                chef = gr_chef_new ();
                g_object_set (chef,
                              "id", chef_id,
                              "fullname", pd->author,
                              NULL);

                id = generate_id ("R_", pd->name, "_by_", chef_id, NULL);

                recipe = gr_recipe_new ();
                g_object_set (recipe,
                              "id", id,
                              "author", chef_id,
                              "name", pd->name,
                              "serves", pd->yield,
                              "prep-time", pd->prep_time,
                              "instructions", pd->instructions,
                              "ingredients", pd->ingredients->str,
                              NULL);

                if (pd->categories) {
                        /* FIXME find best match */
                        g_object_set (recipe,
                                      "category", pd->categories->data,
                                      NULL);
                }

                pd->objects = g_list_prepend (pd->objects, chef);
                pd->objects = g_list_prepend (pd->objects, recipe);

                g_clear_pointer (&pd->author, g_free);
                g_clear_pointer (&pd->name, g_free);
                g_list_free_full (pd->categories, g_free);
                pd->categories = NULL;
                pd->yield = 0;
                g_clear_pointer (&pd->prep_time, g_free);
                g_clear_pointer (&pd->instructions, g_free);
                g_string_set_size (pd->ingredients, 0);
        }
}

static void kreml_text (GMarkupParseContext *context,
                        const gchar         *text,
                        gsize                text_len,
                        gpointer             user_data,
                        GError             **error)
{
        ParserData *pd = user_data;

        if (pd->collecting_text)
                g_string_append_len (pd->text, text, text_len);
}

static const GMarkupParser parser = {
        kreml_start_element,
        kreml_end_element,
        kreml_text,
        NULL,
        NULL
};

static void
import_from_krecipes (GrRecipeImporter *importer,
                      GFile            *file)
{
        g_autoptr(GMarkupParseContext) context = NULL;
        g_autofree char *buffer;
        gsize length;
        g_autoptr(GError) error = NULL;
        g_autofree char *path = NULL;
        ParserData data;
        GList *l;
        GrRecipeStore *store;

        path = g_file_get_path (file);

        data.collecting_text = FALSE;
        data.text = g_string_new ("");
        data.name = NULL;
        data.author= NULL;
        data.categories = NULL;
        data.objects = NULL;
        data.yield = 0;
        data.prep_time = NULL;
        data.instructions = NULL;
        data.ing_group = NULL;
        data.ing_name = NULL;
        data.ing_unit = NULL;
        data.ing_amount = 0;
        data.ingredients = g_string_new ("");

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        if (!g_file_load_contents (file, NULL, &buffer, &length, NULL, &error)) {
                g_message ("Failed to load %s: %s", path, error->message);
                return;
        }

        context = g_markup_parse_context_new (&parser, 0, &data, NULL);
        if (!g_markup_parse_context_parse (context, buffer, length, &error)) {
                g_message ("Failed to parse %s: %s", path, error->message);
        }

        for (l = data.objects; l; l = l->next) {
                GObject *obj = l->data;

                if (GR_IS_CHEF (obj)) {
                        GrChef *chef = GR_CHEF (obj);

                        g_print ("chef: id=%s, fullname=%s\n",
                                 gr_chef_get_id (chef),
                                 gr_chef_get_fullname (chef));

                        if (!gr_recipe_store_add_chef (store, chef, &error)) {
                                g_message ("Failed to add chef: %s", error->message);
                        }
                }
                else if (GR_IS_RECIPE (obj)) {
                        GrRecipe *recipe = GR_RECIPE (obj);

                        g_print ("recipe:\n\tid=%s\n\tname=%s\n\tserves=%d\n\tcategory: %s\n\tprep-time: %s\n\tinstructions: %s\n",
                                 gr_recipe_get_id (recipe),
                                 gr_recipe_get_name (recipe),
                                 gr_recipe_get_serves (recipe),
                                 gr_recipe_get_category (recipe),
                                 gr_recipe_get_prep_time (recipe),
                                 gr_recipe_get_instructions (recipe));

                        if (!gr_recipe_store_add_recipe (store, recipe, &error)) {
                                g_message ("Failed to add recipe: %s", error->message);
                        }
                }
                else {
                        g_print ("unknown object: %s\n", g_type_name_from_instance ((GTypeInstance*)obj));
                }
        }
}

void
gr_recipe_importer_import_from (GrRecipeImporter *importer,
                                GFile            *file)
{
#ifndef ENABLE_AUTOAR
        g_autoptr(GError) error = NULL;

        g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("This build does not support importing"));
        error_cb (NULL, error, importer);
#else
        g_autofree char *basename = NULL;

        basename = g_file_get_basename (file);
        if (g_str_has_suffix (basename , ".kreml")) {
                import_from_krecipes (importer, file);
                return;
        }

        importer->dir = g_mkdtemp (g_build_filename (g_get_tmp_dir (), "recipeXXXXXX", NULL));
        importer->output = g_file_new_for_path (importer->dir);

        importer->extractor = autoar_extractor_new (file, importer->output);
        autoar_extractor_set_output_is_dest (importer->extractor, TRUE);

        g_signal_connect (importer->extractor, "completed", G_CALLBACK (completed_cb), importer);
        g_signal_connect (importer->extractor, "error", G_CALLBACK (error_cb), importer);

        autoar_extractor_start_async (importer->extractor, NULL);
#endif
}
