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
#include <glib/gstdio.h>
#ifdef ENABLE_AUTOAR
#include <gnome-autoar/gnome-autoar.h>
#endif

#include "gr-recipe-exporter.h"
#include "gr-recipe-formatter.h"
#include "gr-image.h"
#include "gr-chef.h"
#include "gr-app.h"
#include "gr-recipe.h"
#include "gr-recipe-store.h"
#include "gr-utils.h"
#include "gr-mail.h"
#include "gr-recipe-printer.h"


struct _GrRecipeExporter
{
        GObject parent_instance;

        GList *recipes;
        GtkWidget *button_now;
        GtkWindow *window;

#ifdef ENABLE_AUTOAR
        AutoarCompressor *compressor;
#endif
        GFile *output; /* the location where compressor writes the archive */
        GList *sources; /* the list of all files */
        GList *pdf_sources;
        char *dir;

        gboolean just_export;
        gboolean contribute;

        GtkWidget *dialog_heading;
        GtkWidget *friend_button;
        GtkWidget *contribute_button;
};

G_DEFINE_TYPE (GrRecipeExporter, gr_recipe_exporter, G_TYPE_OBJECT)

static void
gr_recipe_exporter_finalize (GObject *object)
{
        GrRecipeExporter *exporter = GR_RECIPE_EXPORTER (object);

        g_list_free_full (exporter->recipes, g_object_unref);
        g_clear_object (&exporter->output);
        g_list_free_full (exporter->sources, g_object_unref);
        g_list_free_full (exporter->pdf_sources, g_object_unref);
        g_free (exporter->dir);

        G_OBJECT_CLASS (gr_recipe_exporter_parent_class)->finalize (object);
}

static guint done_signal;

static void
gr_recipe_exporter_class_init (GrRecipeExporterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_recipe_exporter_finalize;

        done_signal = g_signal_new ("done",
                                    G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL,
                                    NULL,
                                    G_TYPE_NONE, 1, G_TYPE_FILE);
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
#ifdef ENABLE_AUTOAR
        g_clear_object (&exporter->compressor);
#endif

        g_clear_pointer (&exporter->dir, g_free);
        g_list_free_full (exporter->recipes, g_object_unref);
        exporter->recipes = NULL;
        g_clear_object (&exporter->output);
        g_list_free_full (exporter->sources, g_object_unref);
        g_list_free_full (exporter->pdf_sources, g_object_unref);
        exporter->sources = NULL;
        exporter->pdf_sources = NULL;

        gr_recipe_store_clear_export_list (gr_recipe_store_get ());
}

#ifdef ENABLE_AUTOAR

static void
file_chooser_response (GtkNativeDialog  *self,
                       int               response_id,
                       GrRecipeExporter *exporter)
{
        if (response_id == GTK_RESPONSE_ACCEPT) {
                g_autoptr(GFile) file = NULL;

                file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self));
                g_file_copy_async (exporter->output, file, 0, 0, NULL, NULL, NULL, NULL, NULL);
        }

        gtk_native_dialog_destroy (self);

        cleanup_export (exporter);
}

static void
mail_done (GObject      *source,
           GAsyncResult *result,
           gpointer      data)
{
        GrRecipeExporter *exporter = data;
        g_autoptr(GError) error = NULL;

        if (!gr_send_mail_finish (result, &error)) {
                GObject *file_chooser;

                g_info ("Sending mail failed: %s", error->message);

                file_chooser = (GObject *)gtk_file_chooser_native_new (_("Save the exported recipe"),
                                                                       GTK_WINDOW (exporter->window),
                                                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                                                       _("Save"),
                                                                       _("Cancel"));
                gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (file_chooser), TRUE);

                g_signal_connect (file_chooser, "response", G_CALLBACK (file_chooser_response), exporter);

                gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser));
                return;
        }

        cleanup_export (exporter);
}

static void
completed_cb (AutoarCompressor *compressor,
              GrRecipeExporter *exporter)
{
        g_autofree char *path = NULL;
        const char *address;
        const char *subject;
        g_autofree char *body = NULL;
        g_auto(GStrv) attachments;
        int i = 1;
        GList *tmp_pdf_list;
        int pdf_sources_length = g_list_length (exporter->pdf_sources);
        attachments = g_new (char*, pdf_sources_length + 2);

        if (exporter->just_export) {
                g_signal_emit (exporter, done_signal, 0, exporter->output);
                cleanup_export (exporter);
                return;
        }

        if (exporter->contribute) {
                address = "recipes-list@gnome.org";
                subject = _("Recipe contribution");
                body = g_strdup (_("Please accept my attached recipe contribution."));
        }
        else {
                GString *s;
                GList *l;

                address = "";
                s = g_string_new ("");

                if (exporter->recipes->next == NULL) {
                        subject = _("Try this recipe");
                        g_string_append (s, _("Hi,\n\nyou should try this recipe."));
                }
                else {
                        subject = _("Try these recipes");
                        g_string_append (s, _("Hi,\n\nyou should try these recipes."));
                }
                g_string_append (s, "\n\n");
                g_string_append (s, _("(The attached file can be imported into GNOME Recipes.)"));

                for (l = exporter->recipes; l; l = l->next) {
                        g_autofree char *formatted = NULL;

                        formatted = gr_recipe_format (GR_RECIPE (l->data));
                        g_string_append (s, "\n\n");
                        g_string_append (s, formatted);
                }

                body = g_string_free (s, FALSE);
        }

        path = g_file_get_path (exporter->output);

        attachments[0] = g_strdup (path);

        for (tmp_pdf_list = exporter->pdf_sources; i-1 < pdf_sources_length; i++, tmp_pdf_list = tmp_pdf_list->next)
                attachments[i] = g_file_get_path (tmp_pdf_list->data);

        attachments[pdf_sources_length + 1] = NULL;


        gr_send_mail (GTK_WINDOW (exporter->window),
                      address, subject, body, (const char **)attachments,
                      mail_done, exporter);
}
#endif

static gboolean
#ifndef ENABLE_AUTOAR
G_GNUC_UNUSED
#endif
export_one_recipe (GrRecipeExporter  *exporter,
                   GrRecipe          *recipe,
                   GKeyFile          *keyfile,
                   GError           **error)
{
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
        double yield;
        const char *yield_unit;
        g_autofree char *yield_str = NULL;
        int default_image;
        int spiciness;
        GrDiets diets;
        GDateTime *ctime;
        GDateTime *mtime;
        GPtrArray *images;
        g_auto(GStrv) paths = NULL;
        int i;
        g_autofree char *imagedir = NULL;

        GrRecipePrinter *printer;
        GFile *recipe_pdf;
        printer = gr_recipe_printer_new (exporter->window);
        key = gr_recipe_get_id (recipe);
        name = gr_recipe_get_name (recipe);
        author = gr_recipe_get_author (recipe);
        description = gr_recipe_get_description (recipe);
        yield = gr_recipe_get_yield (recipe);
        yield_unit = gr_recipe_get_yield_unit (recipe);
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
        spiciness = gr_recipe_get_spiciness (recipe);

        images = gr_recipe_get_images (recipe);

        imagedir = g_build_filename (exporter->dir, "images", NULL);
        g_mkdir_with_parents (imagedir, 0755);

        paths = g_new0 (char *, images->len + 1);
        for (i = 0; i < images->len; i++) {
                GrImage *ri = g_ptr_array_index (images, i);
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                g_autoptr(GFile) source = NULL;
                g_autoptr(GFile) dest = NULL;
                g_autofree char *path = NULL;
                g_autofree char *basename = NULL;
                g_autofree char *destname = NULL;

                pixbuf = gr_image_load_sync (ri, 400, 400, TRUE);
                path = gr_image_get_cache_path (ri);
                source = g_file_new_for_path (path);
                basename = g_file_get_basename (source);
                destname = g_build_filename (imagedir, basename, NULL);

                dest = g_file_new_for_path (destname);

                if (!g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, error)) {
                        return FALSE;
                }

                paths[i] = g_build_filename  ("images", basename, NULL);
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
        g_key_file_set_integer (keyfile, key, "Serves", (int)yield);
        yield_str = g_strdup_printf ("%g %s", yield, yield_unit);
        g_key_file_set_string (keyfile, key, "Yield", yield_str);
        g_key_file_set_integer (keyfile, key, "Spiciness", spiciness);
        g_key_file_set_integer (keyfile, key, "Diets", diets);
        g_key_file_set_integer (keyfile, key, "DefaultImage", default_image);

        g_key_file_set_string_list (keyfile, key, "Images", (const char * const *)paths, g_strv_length (paths));

        recipe_pdf = gr_recipe_printer_get_pdf (printer, recipe);
        exporter->pdf_sources = g_list_append (exporter->pdf_sources, recipe_pdf);

        if (ctime) {
                g_autofree char *created = date_time_to_string (ctime);
                g_key_file_set_string (keyfile, key, "Created", created);
        }
        if (mtime) {
                g_autofree char *modified = date_time_to_string (mtime);
                g_key_file_set_string (keyfile, key, "Modified", modified);
        }

        return TRUE;
}

static gboolean
#ifndef ENABLE_AUTOAR
G_GNUC_UNUSED
#endif
export_one_chef (GrRecipeExporter  *exporter,
                 GrChef            *chef,
                 GKeyFile          *keyfile,
                 GError           **error)
{
        const char *key;
        const char *name;
        const char *fullname;
        const char *description;
        const char *image_path;
        g_autoptr(GMainLoop) loop = NULL;

        key = gr_chef_get_id (chef);
        name = gr_chef_get_name (chef);
        fullname = gr_chef_get_fullname (chef);
        description = gr_chef_get_description (chef);
        image_path = gr_chef_get_image (chef);
        if (image_path && image_path[0]) {
                g_autoptr(GFile) source = NULL;
                g_autoptr(GFile) dest = NULL;
                g_autoptr(GrImage) ri = NULL;
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                g_autofree char *basename = NULL;
                g_autofree char *destname = NULL;
                g_autofree char *path = NULL;
                g_autofree char *cache_path = NULL;

                ri = gr_image_new (gr_app_get_soup_session (GR_APP (g_application_get_default ())), gr_chef_get_id (chef), image_path);
                pixbuf = gr_image_load_sync (ri, 400, 400, FALSE);

                cache_path = gr_image_get_cache_path (ri);
                source = g_file_new_for_path (cache_path);
                basename = g_file_get_basename (source);
                path = g_build_filename ("images", basename, NULL);
                destname = g_build_filename (exporter->dir, path, NULL);

                dest = g_file_new_for_path (destname);

                if (!g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, error))
                        return FALSE;

                g_key_file_set_string (keyfile, key, "Image", path);
        }

        g_key_file_set_string (keyfile, key, "Name", name ? name : "");
        g_key_file_set_string (keyfile, key, "Fullname", fullname ? fullname : "");
        g_key_file_set_string (keyfile, key, "Description", description ? description : "");

        return TRUE;
}

static gboolean
prepare_export (GrRecipeExporter  *exporter,
                GError           **error)
{
#ifndef ENABLE_AUTOAR
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("This build does not support exporting"));

        return FALSE;
#else
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        GrRecipeStore *store;
        GList *l;
        g_autoptr(GHashTable) chefs = NULL;
        g_autofree char *imagedir = NULL;

        store = gr_recipe_store_get ();

        g_assert (exporter->dir == NULL);
        g_assert (exporter->sources == NULL);
        g_assert (exporter->pdf_sources == NULL);

        exporter->dir = g_mkdtemp (g_build_filename (g_get_tmp_dir (), "recipeXXXXXX", NULL));

        path = g_build_filename (exporter->dir, "recipes.db", NULL);
        keyfile = g_key_file_new ();

        imagedir = g_build_filename (exporter->dir, "images", NULL);
        exporter->sources = g_list_append (exporter->sources, g_file_new_for_path (imagedir));

        for (l = exporter->recipes; l; l = l->next) {
                GrRecipe *recipe = l->data;

                if (!export_one_recipe (exporter, recipe, keyfile, error))
                        return FALSE;
        }

        if (!g_key_file_save_to_file (keyfile, path, error))
                return FALSE;

        exporter->sources = g_list_append (exporter->sources, g_file_new_for_path (path));

        g_clear_pointer (&path, g_free);
        g_clear_pointer (&keyfile, g_key_file_unref);

        path = g_build_filename (exporter->dir, "chefs.db", NULL);
        keyfile = g_key_file_new ();

        chefs = g_hash_table_new (g_str_hash, g_str_equal);
        for (l = exporter->recipes; l; l = l->next) {
                GrRecipe *recipe = l->data;
                const char *author;
                g_autoptr(GrChef) chef = NULL;

                author = gr_recipe_get_author (recipe);
                if (g_hash_table_contains (chefs, author))
                        continue;

                chef = gr_recipe_store_get_chef (store, author);
                if (!chef)
                        continue;

                if (!export_one_chef (exporter, chef, keyfile, error))
                        return FALSE;

                g_hash_table_add (chefs, (gpointer)author);
        }

        if (!g_key_file_save_to_file (keyfile, path, error))
                return FALSE;

        exporter->sources = g_list_append (exporter->sources, g_file_new_for_path (path));

        return TRUE;
#endif
}

static void
error_cb (gpointer          compressor,
          GError           *error,
          GrRecipeExporter *exporter)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (exporter->window,
                                         GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("Error while exporting:\n%s"),
                                         error->message);
        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_widget_show (dialog);

        cleanup_export (exporter);
}

static void
start_export (GrRecipeExporter *exporter)
{
        g_autoptr(GError) error = NULL;

        if (!prepare_export (exporter, &error)) {
                error_cb (NULL, error, exporter);
                return;
        }

#ifdef ENABLE_AUTOAR
        exporter->compressor = autoar_compressor_new (exporter->sources, exporter->output, AUTOAR_FORMAT_TAR, AUTOAR_FILTER_GZIP, FALSE);

        autoar_compressor_set_output_is_dest (exporter->compressor, TRUE);
        g_signal_connect (exporter->compressor, "completed", G_CALLBACK (completed_cb), exporter);
        g_signal_connect (exporter->compressor, "error", G_CALLBACK (error_cb), exporter);

        autoar_compressor_start_async (exporter->compressor, NULL);
#endif
}

static void
do_export (GrRecipeExporter *exporter)
{
        int i;
        g_autofree char *path = NULL;
        int extra;
        g_autofree char *name = NULL;

        extra = g_list_length (exporter->recipes);
        if (extra > 1)
                name = g_strdup_printf ("%s (%d recipes)", gr_recipe_get_name (GR_RECIPE (exporter->recipes->data)), extra);
        else
                name = g_strdup (gr_recipe_get_name (GR_RECIPE (exporter->recipes->data)));
        g_strdelimit (name, "./", ' ');

        for (i = 0; i < 1000; i++) {
                g_autofree char *tmp;

                if (i == 0)
                        tmp = g_strdup_printf ("%s/%s.gnome-recipes-export", get_user_data_dir (), name);
                else
                        tmp = g_strdup_printf ("%s/%s(%d).gnome-recipes-export", get_user_data_dir (), name, i);

                if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
                        path = g_strdup (tmp);
                        break;
                }
         }

         if (!path) {
                g_autofree char *dir = NULL;

                dir = g_dir_make_tmp ("recipesXXXXXX", NULL);
                path = g_build_filename (dir, "recipes.gnome-recipes-export", NULL);
         }

         exporter->output = g_file_new_for_path (path);

         start_export (exporter);
}

static void
export_dialog_response (GtkWidget        *dialog,
                        int               response_id,
                        GrRecipeExporter *exporter)
{
        if (response_id == GTK_RESPONSE_CANCEL) {
                g_info ("Not exporting now");
        }
        else if (response_id == GTK_RESPONSE_OK) {
                g_info ("Exporting %d recipes now", g_list_length (exporter->recipes));

                exporter->contribute = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (exporter->contribute_button));

                do_export (exporter);
        }

        gtk_widget_destroy (dialog);
        exporter->dialog_heading = NULL;
}

static void
update_heading (GrRecipeExporter *exporter)
{
        g_autofree char *tmp = NULL;
        int n;

        n = g_list_length (exporter->recipes);
        tmp = g_strdup_printf (ngettext ("%d recipe selected for sharing",
                                         "%d recipes selected for sharing", n), n);
        gtk_label_set_label (GTK_LABEL (exporter->dialog_heading), tmp);
}

static void
update_export_button (GrRecipeExporter *exporter)
{
  gboolean empty = exporter->recipes == NULL;

  if (empty)
    gtk_widget_set_sensitive (exporter->button_now, FALSE);
  else
    gtk_widget_set_sensitive (exporter->button_now, TRUE);
}

static void
update_contribute_button (GrRecipeExporter *exporter)
{
        GList *l;
        gboolean readonly;

        readonly = FALSE;
        for (l = exporter->recipes; l; l = l->next) {
                if (gr_recipe_is_readonly (GR_RECIPE (l->data))) {
                        readonly = TRUE;
                        break;
                }
        }

        if (readonly) {
                gtk_widget_set_sensitive (exporter->contribute_button, FALSE);
                gtk_widget_set_tooltip_text (exporter->contribute_button, _("Some of the selected recipes are included\n"
                                                                            "with GNOME Recipes. You should only contribute\n"
                                                                            "your own recipes."));
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (exporter->friend_button), TRUE);
        }
        else {
                gtk_widget_set_sensitive (exporter->contribute_button, TRUE);
                gtk_widget_set_tooltip_text (exporter->contribute_button, "");
        }
}

static void
row_activated (GtkListBox *list,
               GtkListBoxRow *row,
               GrRecipeExporter *exporter)
{
        GrRecipeStore *store;
        GrRecipe *recipe;
        GtkWidget *image;

        store = gr_recipe_store_get ();

        recipe = GR_RECIPE (g_object_get_data (G_OBJECT (row), "recipe"));
        image = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "check"));

        if (gtk_widget_get_opacity (image) > 0.5) {
                gr_recipe_store_remove_export (store, recipe);
                exporter->recipes = g_list_remove (exporter->recipes, recipe);
                g_object_unref (recipe);
                gtk_widget_set_opacity (image, 0.0);
        }
        else {
                gr_recipe_store_add_export (store, recipe);
                exporter->recipes = g_list_append (exporter->recipes, g_object_ref (recipe));
                gtk_widget_set_opacity (image, 1.0);
        }

        update_heading (exporter);
        update_export_button (exporter);
        update_contribute_button (exporter);
}

static void
add_recipe_row (GrRecipeExporter *exporter,
                GtkWidget *list,
                GrRecipe  *recipe)
{
        GtkWidget *row;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *image;

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
        gtk_widget_show (box);
        g_object_set (box, "margin", 10, NULL);
        label = gtk_label_new (gr_recipe_get_name (recipe));
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

        image = gtk_image_new_from_icon_name ("object-select-symbolic", 1);
        gtk_widget_show (image);
        gtk_style_context_add_class (gtk_widget_get_style_context (image), "dim-label");
        gtk_box_pack_start (GTK_BOX (box), image, FALSE, TRUE, 0);

        row = gtk_list_box_row_new ();
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (row), box);
        g_object_set_data_full (G_OBJECT (row), "recipe", g_object_ref (recipe), g_object_unref);
        g_object_set_data (G_OBJECT (row), "check", image);

        gtk_container_add (GTK_CONTAINER (list), row);

}

static int
sort_recipe_row (GtkListBoxRow *row1,
                 GtkListBoxRow *row2,
                 gpointer       data)
{
        GrRecipe *r1, *r2;

        r1 = GR_RECIPE (g_object_get_data (G_OBJECT (row1), "recipe"));
        r2 = GR_RECIPE (g_object_get_data (G_OBJECT (row2), "recipe"));

        return g_strcmp0 (gr_recipe_get_name (r1), gr_recipe_get_name (r2));
}

static void
populate_recipe_list (GrRecipeExporter *exporter,
                      GtkWidget        *list)
{
        GList *l;

        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), sort_recipe_row, NULL, NULL);

        g_signal_connect (list, "row-activated", G_CALLBACK (row_activated), exporter);
        for (l = exporter->recipes; l; l = l->next) {
                GrRecipe *recipe = l->data;
                add_recipe_row (exporter, list, recipe);
        }
}

static void
show_export_dialog (GrRecipeExporter *exporter)
{
        g_autoptr(GtkBuilder) builder = NULL;
        GtkWidget *dialog;
        GtkWidget *list;

        builder = gtk_builder_new_from_resource ("/org/gnome/Recipes/recipe-export-dialog.ui");
        dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
        exporter->button_now = GTK_WIDGET (gtk_builder_get_object (builder, "button_now"));

        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (exporter->window));

        list = GTK_WIDGET (gtk_builder_get_object (builder, "recipe_list"));
        populate_recipe_list (exporter, list);

        exporter->dialog_heading = GTK_WIDGET (gtk_builder_get_object (builder, "heading"));
        exporter->friend_button = GTK_WIDGET (gtk_builder_get_object (builder, "friend_button"));
        exporter->contribute_button = GTK_WIDGET (gtk_builder_get_object (builder, "contribute_button"));

        update_heading (exporter);
        update_export_button (exporter);
        update_contribute_button (exporter);

        g_signal_connect (dialog, "response", G_CALLBACK (export_dialog_response), exporter);
        gtk_widget_show (dialog);
}

void
gr_recipe_exporter_export (GrRecipeExporter *exporter,
                           GrRecipe         *recipe)
{
        GrRecipeStore *store;
        const char **keys;
        int i;

        store = gr_recipe_store_get ();
        gr_recipe_store_add_export (store, recipe);
        keys = gr_recipe_store_get_export_list (store);

        g_list_free_full (exporter->recipes, g_object_unref);
        g_list_free_full (exporter->pdf_sources, g_object_unref);
        exporter->recipes = NULL;

        for (i = 0; keys[i]; i++) {
                g_autoptr(GrRecipe) r = gr_recipe_store_get_recipe (store, keys[i]);
                if (r)
                        exporter->recipes = g_list_prepend (exporter->recipes, g_object_ref (r));
        }

        show_export_dialog (exporter);
}

void
gr_recipe_exporter_contribute (GrRecipeExporter *exporter,
                               GrRecipe         *recipe)
{
        g_list_free_full (exporter->recipes, g_object_unref);

        exporter->recipes = g_list_append (NULL, g_object_ref (recipe));

        exporter->contribute = TRUE;

        do_export (exporter);
}

static void
collect_all_recipes (GrRecipeExporter *exporter)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;

        store = gr_recipe_store_get ();
        keys = gr_recipe_store_get_recipe_keys (store, &length);
        for (i = 0; keys[i]; i++) {
                g_autoptr(GrRecipe) recipe = gr_recipe_store_get_recipe (store, keys[i]);
                if (!gr_recipe_is_readonly (recipe))
                        exporter->recipes = g_list_append (exporter->recipes, recipe);
        }
}

void
gr_recipe_exporter_export_all (GrRecipeExporter *exporter,
                               GFile            *file)
{
        collect_all_recipes (exporter);

        if (exporter->recipes == NULL)
                return;

        g_info ("Exporting %d recipes", g_list_length (exporter->recipes));

        exporter->output = file;

        exporter->just_export = TRUE;

        start_export (exporter);
}
