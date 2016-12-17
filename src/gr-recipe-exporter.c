/* gr-recipe-exporter.c:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
    <file preprocess="xml-stripblanks">gr-big-cuisine-tile.ui</file>
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
#include <gnome-autoar/gnome-autoar.h>
#include "glnx-shutil.h"

#include "gr-recipe-exporter.h"
#include "gr-images.h"
#include "gr-chef.h"
#include "gr-recipe.h"
#include "gr-recipe-store.h"
#include "gr-app.h"
#include "gr-utils.h"


struct _GrRecipeExporter
{
        GObject parent_instance;

        GrRecipe *recipe;
        GtkWindow *window;

        AutoarCompressor *compressor;
        GFile *dest;
        GFile *output;
        GList *sources;
        char *dir;
};

G_DEFINE_TYPE (GrRecipeExporter, gr_recipe_exporter, G_TYPE_OBJECT)

static void
gr_recipe_exporter_finalize (GObject *object)
{
        GrRecipeExporter *exporter = GR_RECIPE_EXPORTER (object);

        g_clear_object (&exporter->recipe);
        g_clear_object (&exporter->dest);
        g_clear_object (&exporter->output);
        g_list_free_full (exporter->sources, g_object_unref);
        g_free (exporter->dir);

        G_OBJECT_CLASS (gr_recipe_exporter_parent_class)->finalize (object);
}

static void
gr_recipe_exporter_class_init (GrRecipeExporterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_recipe_exporter_finalize;
}

static void
gr_recipe_exporter_init (GrRecipeExporter *self)
{
}

GrRecipeExporter *
gr_recipe_exporter_new (GtkWindow *parent)
{
        GrRecipeExporter *exporter;

        exporter = g_object_new (GR_TYPE_RECIPE_EXPORTER, NULL);

        exporter->window = parent;

        return exporter;
}

static void
cleanup_export (GrRecipeExporter *exporter)
{
        g_autoptr(GError) error = NULL;

        if (!glnx_shutil_rm_rf_at (-1, exporter->dir, NULL, &error))
                g_warning ("Failed to clean up temp directory %s: %s", exporter->dir, error->message);

        g_clear_pointer (&exporter->dir, g_free);
        g_clear_object (&exporter->recipe);
        g_clear_object (&exporter->compressor);
        g_clear_object (&exporter->output);
        g_clear_object (&exporter->dest);
        g_list_free_full (exporter->sources, g_object_unref);
        exporter->sources = NULL;
}

static void
completed_cb (AutoarCompressor *compressor,
              GrRecipeExporter *exporter)
{
        GtkWidget *dialog;
        g_autofree char *path =  NULL;

        path = g_file_get_path (exporter->dest);
        g_message (_("The recipe %s has been exported as “%s”"),
                   gr_recipe_get_name (exporter->recipe), path);

        dialog = gtk_message_dialog_new (exporter->window,
                                         GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_OK,
                                         _("The recipe %s has been exported as “%s”"),
                                         gr_recipe_get_name (exporter->recipe), path);
        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_widget_show (dialog);

        cleanup_export (exporter);
}

static void
error_cb (AutoarCompressor *compressor,
          GError           *error,
          GrRecipeExporter *exporter)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (exporter->window,
                                         GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("Error while exporting recipe %s:\n%s"),
                                         gr_recipe_get_name (exporter->recipe),
                                         error->message);
        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_widget_show (dialog);

        cleanup_export (exporter);
}

static void
decide_dest_cb (AutoarCompressor *compressor,
                GFile            *file,
                GrRecipeExporter *exporter)
{
        g_set_object (&exporter->dest, file);
}

static gboolean
prepare_export (GrRecipeExporter  *exporter,
                GError           **error)
{
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        const char *key;
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
        const char *fullname;
        const char *image_path;
        int serves;
        GrDiets diets;
        GDateTime *ctime;
        GDateTime *mtime;
        g_autoptr(GrChef) chef = NULL;
        GrRecipeStore *store;
        g_autoptr(GArray) images = NULL;
        g_auto(GStrv) paths = NULL;
        g_autofree int *angles = NULL;
        g_autofree gboolean *dark = NULL;
        int i, j;

        g_assert (exporter->dir == NULL);
        g_assert (exporter->sources == NULL);

        exporter->dir = g_mkdtemp (g_build_filename (g_get_tmp_dir (), "recipeXXXXXX", NULL));
        path = g_build_filename (exporter->dir, "recipes.db", NULL);
        keyfile = g_key_file_new ();

        key = name = gr_recipe_get_name (exporter->recipe);
        author = gr_recipe_get_author (exporter->recipe);
        description = gr_recipe_get_description (exporter->recipe);
        serves = gr_recipe_get_serves (exporter->recipe);
        cuisine = gr_recipe_get_cuisine (exporter->recipe);
        season = gr_recipe_get_season (exporter->recipe);
        category = gr_recipe_get_category (exporter->recipe);
        prep_time = gr_recipe_get_prep_time (exporter->recipe);
        cook_time = gr_recipe_get_cook_time (exporter->recipe);
        diets = gr_recipe_get_diets (exporter->recipe);
        ingredients = gr_recipe_get_ingredients (exporter->recipe);
        instructions = gr_recipe_get_instructions (exporter->recipe);
        notes = gr_recipe_get_notes (exporter->recipe);
        ctime = gr_recipe_get_ctime (exporter->recipe);
        mtime = gr_recipe_get_mtime (exporter->recipe);

        g_object_get (exporter->recipe, "images", &images, NULL);
        paths = g_new0 (char *, images->len + 1);
        angles = g_new0 (int, images->len + 1);
        dark = g_new0 (gboolean, images->len + 1);
        for (i = 0, j = 0; i < images->len; i++) {
                GrRotatedImage *ri = &g_array_index (images, GrRotatedImage, i);
                g_autoptr(GFile) source = NULL;
                g_autoptr(GFile) dest = NULL;
                g_autofree char *basename = NULL;
                g_autofree char *destname = NULL;

                source = g_file_new_for_path (ri->path);
                basename = g_file_get_basename (source);
                destname = g_build_filename (exporter->dir, basename, NULL);

                dest = g_file_new_for_path (destname);

                if (!g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, error)) {
                        return FALSE;
                }

                exporter->sources = g_list_append (exporter->sources, g_object_ref (dest));

                paths[j] = g_strdup (basename);
                angles[j] = ri->angle;
                dark[j] = ri->dark_text;

                j++;
        }

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
        g_key_file_set_string (keyfile, key, "Notes", notes ? notes : "");
        g_key_file_set_integer (keyfile, key, "Serves", serves);
        g_key_file_set_integer (keyfile, key, "Diets", diets);

        g_key_file_set_string_list (keyfile, key, "Images", (const char * const *)paths, g_strv_length (paths));
        g_key_file_set_integer_list (keyfile, key, "Angles", angles, g_strv_length (paths));
        g_key_file_set_integer_list (keyfile, key, "DarkText", dark, g_strv_length (paths));


        if (ctime) {
                g_autofree char *created = date_time_to_string (ctime);
                g_key_file_set_string (keyfile, key, "Created", created);
        }
        if (mtime) {
                g_autofree char *modified = date_time_to_string (mtime);
                g_key_file_set_string (keyfile, key, "Modified", modified);
        }

        if (!g_key_file_save_to_file (keyfile, path, error)) {
                return FALSE;
        }

        exporter->sources = g_list_append (exporter->sources, g_file_new_for_path (path));

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        chef = gr_recipe_store_get_chef (store, author);
        if (!chef)
                return TRUE;

        g_clear_pointer (&path, g_free);
        g_clear_pointer (&keyfile, g_key_file_unref);

        path = g_build_filename (exporter->dir, "chefs.db", NULL);
        keyfile = g_key_file_new ();

        key = gr_chef_get_id (chef);
        name = gr_chef_get_name (chef);
        fullname = gr_chef_get_fullname (chef);
        description = gr_chef_get_description (chef);
        image_path = gr_chef_get_image (chef);
        if (image_path) {
                g_autoptr(GFile) source = NULL;
                g_autoptr(GFile) dest = NULL;
                g_autofree char *basename = NULL;
                g_autofree char *destname = NULL;

                source = g_file_new_for_path (image_path);
                basename = g_file_get_basename (source);
                destname = g_build_filename (exporter->dir, basename, NULL);

                dest = g_file_new_for_path (destname);

                if (!g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, error)) {
                        return FALSE;
                }
                else {
                        g_key_file_set_string (keyfile, key, "Image", basename);
                        exporter->sources = g_list_append (exporter->sources, g_object_ref (dest));
                }
        }

        g_key_file_set_string (keyfile, key, "Name", name ? name : "");
        g_key_file_set_string (keyfile, key, "Fullname", fullname ? fullname : "");
        g_key_file_set_string (keyfile, key, "Description", description ? description : "");

        if (!g_key_file_save_to_file (keyfile, path, error)) {
                return FALSE;
        }

        exporter->sources = g_list_append (exporter->sources, g_file_new_for_path (path));

        return TRUE;
}

void
gr_recipe_exporter_export_to (GrRecipeExporter *exporter,
                              GrRecipe         *recipe,
                              GFile            *file)
{
        g_autoptr(GFile) output = NULL;
        g_autoptr(GError) error = NULL;

        g_set_object (&exporter->recipe, recipe);
        g_set_object (&exporter->output, file);

        if (!prepare_export (exporter, &error)) {
                error_cb (NULL, error, exporter);
                return;
        }

        exporter->compressor = autoar_compressor_new (exporter->sources, exporter->output, AUTOAR_FORMAT_TAR, AUTOAR_FILTER_GZIP, FALSE);

        g_signal_connect (exporter->compressor, "decide-dest", G_CALLBACK (decide_dest_cb), exporter);
        g_signal_connect (exporter->compressor, "completed", G_CALLBACK (completed_cb), exporter);
        g_signal_connect (exporter->compressor, "error", G_CALLBACK (error_cb), exporter);

        autoar_compressor_start_async (exporter->compressor, NULL);
}
