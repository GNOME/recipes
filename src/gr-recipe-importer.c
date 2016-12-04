/* gr-recipe-importer.c
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

#include <stdlib.h>
#include <glib/gi18n.h>
#include <gnome-autoar/gnome-autoar.h>
#include "glnx-shutil.h"

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

        AutoarExtractor *extractor;
        GFile *output;
        char *dir;
};

G_DEFINE_TYPE (GrRecipeImporter, gr_recipe_importer, G_TYPE_OBJECT)

static void
gr_recipe_importer_finalize (GObject *object)
{
        GrRecipeImporter *importer = GR_RECIPE_IMPORTER (object);

        g_clear_object (&importer->extractor);
        g_clear_object (&importer->output);
        g_free (importer->dir);

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
                                    G_TYPE_NONE, 1, GR_TYPE_RECIPE);
}

static void
gr_recipe_importer_init (GrRecipeImporter *self)
{
}

GrRecipeImporter *
gr_recipe_importer_new (GtkWindow *parent)
{
        GrRecipeImporter *importer;

        importer = g_object_new (GR_TYPE_RECIPE_IMPORTER, NULL);

        importer->window = parent;

        return importer;
}

static GDateTime *
date_time_from_string (const char *string)
{
        g_auto(GStrv) s = NULL;
        g_auto(GStrv) s1 = NULL;
        g_auto(GStrv) s2 = NULL;
        int year, month, day, hour, minute, second;
        char *endy, *endm, *endd, *endh, *endmi, *ends;

        s = g_strsplit (string, " ", -1);
        if (g_strv_length (s) != 2)
                return NULL;

        s1 = g_strsplit (s[0], "-", -1);
        if (g_strv_length (s1) != 3)
                return NULL;

        s2 = g_strsplit (s[1], ":", -1);
        if (g_strv_length (s1) != 3)
                return NULL;

        year = strtol (s1[0], &endy, 10);
        month = strtol (s1[1], &endm, 10);
        day = strtol (s1[2], &endd, 10);

        hour = strtol (s2[0], &endh, 10);
        minute = strtol (s2[1], &endmi, 10);
        second = strtol (s2[2], &ends, 10);

        if (!*endy && !*endm && !*endd &&
            !*endh && !*endmi && !*ends &&
            0 < month && month <= 12 &&
            0 < day && day <= 31 &&
            0 <= hour && hour < 24 &&
            0 <= minute && minute < 60 &&
            0 <= second && second < 60)
                    return g_date_time_new_utc (year, month, day,
                                                hour, minute, second);

        return NULL;
}

static void
cleanup_import (GrRecipeImporter *importer)
{
        g_autoptr(GError) error = NULL;

        if (!glnx_shutil_rm_rf_at (-1, importer->dir, NULL, &error))
                g_warning ("Failed to clean up temp directory %s: %s", importer->dir, error->message);

        g_clear_pointer (&importer->dir, g_free);
        g_clear_object (&importer->extractor);
        g_clear_object (&importer->output);
}

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

        srcpath = g_build_filename (importer->dir, path, NULL);
        source = g_file_new_for_path (srcpath);
        destpath = g_build_filename (g_get_user_data_dir (), "recipes", path, NULL);
        dest = g_file_new_for_path (destpath);

        if (!g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, error)) {
                return FALSE;
        }

        *new_path = g_strdup (destpath);

        return TRUE;
}

static gboolean
finish_import (GrRecipeImporter  *importer,
               GError           **error)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autofree char *path = NULL;
        g_auto(GStrv) groups = NULL;
        g_autofree char *name = NULL;
        g_autofree char *fullname = NULL;
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
        g_autofree gboolean *dark = NULL;
        int serves = 1;
        GrDiets diets = 0;
        g_autoptr(GArray) images = NULL;
        g_autoptr(GDateTime) ctime = NULL;
        g_autoptr(GDateTime) mtime = NULL;
        char *tmp;
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GrRecipe) recipe = NULL;
        int i;
        gsize length2, length3;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keyfile = g_key_file_new ();
        path = g_build_filename (importer->dir, "chefs.db", NULL);
        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, error))
                return FALSE;

        groups = g_key_file_get_groups (keyfile, NULL);
        if (!groups || !groups[0]) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("No recipe found"));
                return FALSE;
        }

        name = g_key_file_get_string (keyfile, groups[0], "Name", error);
        if (*error)
                return FALSE;
        fullname = g_key_file_get_string (keyfile, groups[0], "Fullname", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        description = g_key_file_get_string (keyfile, groups[0], "Description", error);
        if (*error) {
               if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
               g_clear_error (error);
        }
        image_path = g_key_file_get_string (keyfile, groups[0], "Image", error);
        if (*error) {
               if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
               g_clear_error (error);
        }
        if (image_path) {
                char *new_path;

                if (!copy_image (importer, image_path, &new_path, error))
                        return FALSE;

                g_free (image_path);
                image_path = new_path;
        }

        chef = gr_recipe_store_get_chef (store, name);
        if (chef) {
                if (g_strcmp0 (fullname, gr_chef_get_fullname (chef)) == 0 &&
                    g_strcmp0 (description, gr_chef_get_description (chef)) == 0) {
                        /* Assume its the same chef */
                        goto have_chef;
                }

                // FIXME: ask the user if the chef is the same
                for (i = 2; i < 100; i++) {
                        g_autofree char *new_name = NULL;

                        g_clear_object (&chef);
                        new_name = g_strdup_printf ("%s%d", name, i);
                        chef = gr_recipe_store_get_chef (store, new_name);
                        if (!chef) {
                                g_free (name);
                                name = g_strdup (new_name);
                                break;
                        }
                }
        }

        chef = g_object_new (GR_TYPE_CHEF,
                             "name", name,
                             "fullname", fullname,
                             "description", description,
                             "image-path", image_path,
                             NULL);
        if (!gr_recipe_store_add_chef (store, chef, error))
                return FALSE;

have_chef:

        g_clear_pointer (&path, g_free);
        g_clear_pointer (&name, g_free);
        g_clear_pointer (&description, g_free);
        g_clear_pointer (&keyfile, g_key_file_unref);
        g_strfreev (groups);

        keyfile = g_key_file_new ();
        path = g_build_filename (importer->dir, "recipes.db", NULL);
        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, error))
                return FALSE;

        groups = g_key_file_get_groups (keyfile, NULL);
        if (!groups || !groups[0]) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("No recipe found"));
                return FALSE;
        }

        name = g_key_file_get_string (keyfile, groups[0], "Name", error);
        if (*error)
                return FALSE;
        description = g_key_file_get_string (keyfile, groups[0], "Description", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        cuisine = g_key_file_get_string (keyfile, groups[0], "Cuisine", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        season = g_key_file_get_string (keyfile, groups[0], "Season", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        category = g_key_file_get_string (keyfile, groups[0], "Category", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        prep_time = g_key_file_get_string (keyfile, groups[0], "PrepTime", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        cook_time = g_key_file_get_string (keyfile, groups[0], "CookTime", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        ingredients = g_key_file_get_string (keyfile, groups[0], "Ingredients", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        instructions = g_key_file_get_string (keyfile, groups[0], "Instructions", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        notes = g_key_file_get_string (keyfile, groups[0], "Notes", error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        tmp = g_key_file_get_string (keyfile, groups[0], "Created", error);
        if (*error) {
               if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
               g_clear_error (error);
        }
        if (tmp) {
               ctime = date_time_from_string (tmp);
               if (!ctime) {
                        g_free (tmp);
                        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                     _("Failed to load recipe: Couldn't parse Created key"));
                        return FALSE;
                }
        }
        tmp = g_key_file_get_string (keyfile, groups[0], "Modified", error);
        if (*error) {
                        if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                                return FALSE;
                        g_clear_error (error);
        }
        if (tmp) {
               mtime = date_time_from_string (tmp);
               if (!mtime) {
                        g_free (tmp);
                        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                     _("Failed to load recipe: Couldn't parse Modified key"));
                        return FALSE;
                }
        }
        paths = g_key_file_get_string_list (keyfile, groups[0], "Images", &length2, error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        angles = g_key_file_get_integer_list (keyfile, groups[0], "Angles", &length3, error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        if (length2 != length3) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Failed to load recipe: Images and Angles length mismatch"));
                return FALSE;
        }
        dark = g_key_file_get_boolean_list (keyfile, groups[0], "DarkText", &length3, error);
        if (*error) {
                if (!g_error_matches (*error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
                        return FALSE;
                g_clear_error (error);
        }
        if (length2 != length3) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Failed to load recipe: Images and DarkText length mismatch"));
                return FALSE;
        }

        images = gr_rotated_image_array_new ();
        if (paths) {
                for (i = 0; paths[i]; i++) {
                        GrRotatedImage ri;
                        char *new_path;

                        if (!copy_image (importer, paths[i], &new_path, error))
                                return FALSE;

                        ri.path = new_path;
                        ri.angle = angles[i];
                        ri.dark_text = dark[i];
                        g_array_append_val (images, ri);
                }
        }

        recipe = gr_recipe_new ();

        g_object_set (recipe,
                      "name", name,
                      "author", gr_chef_get_name (chef),
                      "description", description ? description : "",
                      "cuisine", cuisine ? cuisine : "",
                      "season", season ? season : "",
                      "category", category ? category : "",
                      "prep-time", prep_time ? prep_time : "",
                      "cook-time", cook_time ? cook_time : "",
                      "ingredients", ingredients ? ingredients : "",
                      "instructions", instructions ? instructions : "",
                      "notes", notes ? notes : "",
                      "serves", serves,
                      "diets", diets,
                      "images", images,
                      "ctime", ctime,
                      "mtime", mtime,
                      NULL);

        if (!gr_recipe_store_add (store, recipe, error)) {
                // FIXME show a dialog to allow renaming
                return FALSE;
        }

        g_signal_emit (importer, done_signal, 0, recipe);

        return TRUE;
}

static void
error_cb (AutoarExtractor  *extractor,
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

static void
completed_cb (AutoarExtractor  *extractor,
              GrRecipeImporter *importer)
{
        g_autoptr(GError) error = NULL;

        if (!finish_import (importer, &error)) {
                error_cb (extractor, error, importer);
                return;
        }

        cleanup_import (importer);
}

void
gr_recipe_importer_import_from (GrRecipeImporter *importer,
                                GFile            *file)
{
        importer->dir = g_mkdtemp (g_build_filename (g_get_tmp_dir (), "recipeXXXXXX", NULL));
        importer->output = g_file_new_for_path (importer->dir);

        importer->extractor = autoar_extractor_new (file, importer->output);
        autoar_extractor_set_output_is_dest (importer->extractor, TRUE);

        g_signal_connect (importer->extractor, "completed", G_CALLBACK (completed_cb), importer);
        g_signal_connect (importer->extractor, "error", G_CALLBACK (error_cb), importer);

        autoar_extractor_start_async (importer->extractor, NULL);
}
