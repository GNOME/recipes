/* gr-details-page.c:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
#include <math.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef ENABLE_GSPELL
#include <gspell/gspell.h>
#endif

#include "gr-details-page.h"
#include "gr-recipe-store.h"
#include "gr-app.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-images.h"
#include "gr-image-viewer.h"
#include "gr-ingredients-list.h"
#include "gr-timer.h"
#include "gr-recipe-printer.h"
#include "gr-recipe-exporter.h"


struct _GrDetailsPage
{
        GtkBox parent_instance;

        GrRecipe *recipe;
        GrChef *chef;
        GrIngredientsList *ingredients;

        GrRecipePrinter *printer;
        GrRecipeExporter *exporter;

        GtkWidget *recipe_image;
        GtkWidget *prep_time_label;
        GtkWidget *cook_time_label;
        GtkWidget *serves_spin;
        GtkWidget *warning_box;
        GtkWidget *spicy_warning;
        GtkWidget *garlic_warning;
        GtkWidget *gluten_warning;
        GtkWidget *dairy_warning;
        GtkWidget *ingredients_box;
        GtkWidget *instructions_label;
        GtkWidget *favorite_button;
        GtkWidget *chef_label;
        GtkWidget *edit_button;
        GtkWidget *delete_button;
        GtkWidget *notes_field;
        GtkWidget *notes_box;
        GtkWidget *notes_label;
        GtkWidget *description_label;
        GtkWidget *export_button;
        GtkWidget *error_label;
        GtkWidget *error_revealer;

        guint save_timeout;

        char *uri;
};

G_DEFINE_TYPE (GrDetailsPage, gr_details_page, GTK_TYPE_BOX)

static void connect_store_signals (GrDetailsPage *page);

static void
delete_recipe (GrDetailsPage *page)
{
        GrRecipeStore *store;
        g_autoptr(GrRecipe) recipe = NULL;
        GtkWidget *window;

        recipe = g_object_ref (page->recipe);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        gr_recipe_store_remove_recipe (store, page->recipe);
        g_set_object (&page->recipe, NULL);
        g_set_object (&page->chef, NULL);

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_go_back (GR_WINDOW (window));

        gr_window_offer_undelete (GR_WINDOW (window), recipe);
}

static void
edit_recipe (GrDetailsPage *page)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_edit_recipe (GR_WINDOW (window), page->recipe);
}

static gboolean
more_recipes (GrDetailsPage *page)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_show_chef (GR_WINDOW (window), page->chef);

        return TRUE;
}

static void
print_recipe (GrDetailsPage *page)
{
        if (!page->printer) {
                GtkWidget *window;

                window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                page->printer = gr_recipe_printer_new (GTK_WINDOW (window));
        }

        gr_recipe_printer_print (page->printer, page->recipe);
}

static void
export_recipe (GrDetailsPage *page)
{
        if (!page->exporter) {
                GtkWidget *window;

                window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                page->exporter = gr_recipe_exporter_new (GTK_WINDOW (window));
        }

        gr_recipe_exporter_export (page->exporter, page->recipe);
}

static void populate_ingredients (GrDetailsPage *page,
                                  int            num,
                                  int            denom);

static void
serves_value_changed (GrDetailsPage *page)
{
        int serves;
        int new_value;

        new_value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page->serves_spin));
        serves = gr_recipe_get_serves (page->recipe);
        populate_ingredients (page, new_value, serves);
}

static void
cook_it_later (GrDetailsPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->favorite_button)))
                gr_recipe_store_add_favorite (store, page->recipe);
        else
                gr_recipe_store_remove_favorite (store, page->recipe);
}

static void
shop_it (GrDetailsPage *page)
{
        GrRecipeStore *store;
        GtkWidget *window;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        gr_recipe_store_add_to_shopping (store, page->recipe);

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_offer_shopping (GR_WINDOW (window));
}

static gboolean save_notes (gpointer data);

static void
details_page_finalize (GObject *object)
{
        GrDetailsPage *self = GR_DETAILS_PAGE (object);

        if (self->save_timeout) {
                g_source_remove (self->save_timeout);
                self->save_timeout = 0;
                save_notes (object);
        }

        g_clear_object (&self->recipe);
        g_clear_object (&self->chef);
        g_clear_object (&self->ingredients);
        g_clear_object (&self->printer);
        g_clear_object (&self->exporter);
        g_clear_pointer (&self->uri, g_free);

        G_OBJECT_CLASS (gr_details_page_parent_class)->finalize (object);
}

static gboolean
save_notes (gpointer data)
{
        GrDetailsPage *page = data;
        GtkTextBuffer *buffer;
        GtkTextIter start, end;
        g_autofree char *text = NULL;
        GrRecipeStore *store;
        g_autofree char *id = NULL;
        g_autofree char *notes = NULL;
        g_autoptr(GError) error = NULL;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->notes_field));
        gtk_text_buffer_get_bounds (buffer, &start, &end);
        text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

        g_object_get (page->recipe,
                      "id", &id,
                      "notes", &notes,
                      NULL);
        if (g_strcmp0 (notes, text) == 0)
                goto out;

        g_object_set (page->recipe, "notes", text, NULL);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        if (!gr_recipe_store_update_recipe (store, page->recipe, id, &error)) {
                g_message ("Error: %s", error->message);
        }

out:
        page->save_timeout = 0;

        return G_SOURCE_REMOVE;
}

static void
schedule_save (GtkTextBuffer *buffer, GrDetailsPage *page)
{
        if (page->save_timeout) {
                g_source_remove (page->save_timeout);
                page->save_timeout = 0;
        }

        page->save_timeout = g_timeout_add (500, save_notes, page);
}

static gboolean
activate_uri_at_idle (gpointer data)
{
        GrDetailsPage *page = data;
        g_autofree char *uri = NULL;

        uri = page->uri;
        page->uri = NULL;

        if (g_str_has_prefix (uri, "image:")) {
                int idx;

                idx = (int)g_ascii_strtoll (uri + strlen ("image:"), NULL, 10);
                gr_image_viewer_show_image (GR_IMAGE_VIEWER (page->recipe_image), idx);
        }
        else if (g_str_has_prefix (uri, "recipe:")) {
                GrRecipeStore *store;
                const char *id;
                g_autoptr(GrRecipe) recipe = NULL;

                store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

                id = uri + strlen ("recipe:");
                recipe = gr_recipe_store_get_recipe (store, id);
                if (recipe) {
                        GtkWidget *window;

                        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                        gr_window_show_recipe (GR_WINDOW (window), recipe);
                }
                else {
                        gtk_label_set_label (GTK_LABEL (page->error_label),
                                             _("Could not find this recipe."));
                        gtk_revealer_set_reveal_child (GTK_REVEALER (page->error_revealer), TRUE);
                }
        }

        return G_SOURCE_REMOVE;
}

static gboolean
activate_link (GtkLabel      *label,
               const char    *uri,
               GrDetailsPage *page)
{
        g_free (page->uri);
        page->uri = g_strdup (uri);

        // FIXME: We can avoid the idle with GTK+ 3.22.6 or newer
        g_idle_add (activate_uri_at_idle, page);

        return TRUE;
}

static void
activate_image (GrDetailsPage *page)
{
        GtkWidget *window;
        g_autoptr(GArray) images = NULL;
        int idx;

        g_object_get (page->recipe_image, "images", &images, "index", &idx, NULL);

        window = gtk_widget_get_ancestor (GTK_WIDGET (page->recipe_image), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_show_image (GR_WINDOW (window), images, idx);
}

static void
dismiss_error (GrDetailsPage *page)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->error_revealer), FALSE);
}

static void
gr_details_page_init (GrDetailsPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
        connect_store_signals (page);

        g_signal_connect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->notes_field)), "changed", G_CALLBACK (schedule_save), page);

#ifdef ENABLE_GSPELL
        {
                GspellTextView *gspell_view;
                gspell_view = gspell_text_view_get_from_gtk_text_view (GTK_TEXT_VIEW (page->notes_field));
                gspell_text_view_basic_setup (gspell_view);
        }
#endif

#ifdef ENABLE_AUTOAR
        gtk_widget_show (page->export_button);
#endif
}

static void
gr_details_page_class_init (GrDetailsPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = details_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-details-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, recipe_image);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, prep_time_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, cook_time_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, serves_spin);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, warning_box);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, spicy_warning);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, garlic_warning);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, dairy_warning);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, gluten_warning);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, ingredients_box);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, instructions_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, favorite_button);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, chef_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, edit_button);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, delete_button);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, notes_field);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, notes_box);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, notes_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, description_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, export_button);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, error_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, error_revealer);

        gtk_widget_class_bind_template_callback (widget_class, edit_recipe);
        gtk_widget_class_bind_template_callback (widget_class, delete_recipe);
        gtk_widget_class_bind_template_callback (widget_class, more_recipes);
        gtk_widget_class_bind_template_callback (widget_class, print_recipe);
        gtk_widget_class_bind_template_callback (widget_class, export_recipe);
        gtk_widget_class_bind_template_callback (widget_class, serves_value_changed);
        gtk_widget_class_bind_template_callback (widget_class, cook_it_later);
        gtk_widget_class_bind_template_callback (widget_class, shop_it);
        gtk_widget_class_bind_template_callback (widget_class, activate_link);
        gtk_widget_class_bind_template_callback (widget_class, dismiss_error);
        gtk_widget_class_bind_template_callback (widget_class, activate_image);
}

GtkWidget *
gr_details_page_new (void)
{
        GrDetailsPage *page;

        page = g_object_new (GR_TYPE_DETAILS_PAGE, NULL);

        return GTK_WIDGET (page);
}

static void
populate_ingredients (GrDetailsPage *page,
                      int            num,
                      int            denom)
{
        g_autoptr(GtkSizeGroup) group = NULL;
        g_autofree char **segments = NULL;
        int i, j;
        GtkWidget *list;
        GtkWidget *label;

        container_remove_all (GTK_CONTAINER (page->ingredients_box));

        group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        segments = gr_ingredients_list_get_segments (page->ingredients);
        for (j = 0; segments[j]; j++) {
                g_auto(GStrv) ings = NULL;

                if (segments[j] && segments[j][0]) {
                        label = gtk_label_new (segments[j]);
                        gtk_widget_show (label);
                        gtk_label_set_xalign (GTK_LABEL (label), 0);
                        gtk_style_context_add_class (gtk_widget_get_style_context (label), "heading");
                        gtk_container_add (GTK_CONTAINER (page->ingredients_box), label);
                }

                list = gtk_list_box_new ();
                gtk_widget_show (list);
                gtk_style_context_add_class (gtk_widget_get_style_context (list), "frame");
                gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
                gtk_list_box_set_header_func (GTK_LIST_BOX (list), all_headers, NULL, NULL);
                gtk_container_add (GTK_CONTAINER (page->ingredients_box), list);

                ings = gr_ingredients_list_get_ingredients (page->ingredients, segments[j]);
                for (i = 0; ings[i]; i++) {
                        GtkWidget *row;
                        GtkWidget *box;
                        g_autofree char *s = NULL;

                        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
                        gtk_widget_show (box);

                        s = gr_ingredients_list_scale_unit (page->ingredients, segments[j], ings[i], num, denom);
                        label = gtk_label_new (s);
                        g_object_set (label,
                                      "visible", TRUE,
                                      "xalign", 0.0,
                                      "margin", 10,
                                      NULL);
                        gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
                        gtk_container_add (GTK_CONTAINER (box), label);
                        gtk_size_group_add_widget (group, label);

                        label = gtk_label_new (ings[i]);
                        g_object_set (label,
                                      "visible", TRUE,
                                      "xalign", 0.0,
                                      "margin", 10,
                                      NULL);
                        gtk_container_add (GTK_CONTAINER (box), label);

                        gtk_container_add (GTK_CONTAINER (list), box);
                        row = gtk_widget_get_parent (box);
                        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
                }
        }

        gtk_widget_hide (page->warning_box);
        gtk_widget_hide (page->garlic_warning);
        gtk_widget_hide (page->dairy_warning);
        gtk_widget_hide (page->gluten_warning);
        gtk_widget_hide (page->spicy_warning);

        if (gr_recipe_contains_garlic (page->recipe)) {
                gtk_widget_show (page->warning_box);
                gtk_widget_show (page->garlic_warning);
        }
        if (gr_recipe_contains_dairy (page->recipe)) {
                gtk_widget_show (page->warning_box);
                gtk_widget_show (page->dairy_warning);
        }
        if (gr_recipe_contains_gluten (page->recipe)) {
                gtk_widget_show (page->warning_box);
                gtk_widget_show (page->gluten_warning);
        }

        if (gr_recipe_get_spiciness (page->recipe) > 50) {
                gtk_widget_show (page->warning_box);
                gtk_widget_show (page->spicy_warning);
                if (gr_recipe_get_spiciness (page->recipe) > 75) {
                        gtk_widget_set_tooltip_text (page->spicy_warning, _("Very spicy"));
                        gtk_style_context_add_class (gtk_widget_get_style_context (page->spicy_warning),
                                                     "very-spicy");
                }
                else {
                        gtk_widget_set_tooltip_text (page->spicy_warning, _("Spicy"));
                        gtk_style_context_remove_class (gtk_widget_get_style_context (page->spicy_warning),
                                                        "very-spicy");
                }
        }
}

static char *
process_instructions (const char *instructions)
{
        GString *s;
        const char *p, *p2, *t, *q;

        s = g_string_new ("");

        t = instructions;

        while (*t) {
                char symb[30];
                const char *sym = "?";
                int idx;
                g_autofree char *title = NULL;

                p = strstr (t, "[image:");
                q = strstr (t, "[timer:");
                if (q && (!p || q < p)) {
                        p = q;
                        sym = "⏰";
                }
                else if (p) {
                        idx = atoi (p + strlen ("[image:"));
                        memset (symb, 0, 30);
                        g_unichar_to_utf8 (0x2460 + idx, symb);
                        sym = symb;
                }

                if (p == NULL) {
                        g_string_append (s, t);
                        break;
                }

                g_string_append_len (s, t, p - t);

                p2 = strstr (p, "]");

                if (p == q) {
                        const char *q2;
                        g_autofree char *timer = NULL;

                        q2 = q + strlen ("[timer:");
                        timer = g_strndup (q2, p2 - q2);
                        title = g_strdup_printf (_("Timer: %s"), timer);
                }
                else {
                        title = g_strdup_printf (_("Image %d"), idx + 1);
                }

                g_string_append (s, "<a href=\"");
                g_string_append_len (s, p + 1, p2 - p);
                g_string_append (s, "\" title=\"");
                g_string_append (s, title);
                g_string_append (s, "\">");
                g_string_append (s, sym);
                g_string_append (s, "</a>");
                t = p2 + 1;
        }

        return g_string_free (s, FALSE);
}

void
gr_details_page_set_recipe (GrDetailsPage *page,
                            GrRecipe      *recipe)
{
        const char *id = NULL;
        const char *old_id = NULL;
        const char *author;
        const char *prep_time;
        const char *cook_time;
        int serves;
        int want_serves;
        const char *ingredients;
        const char *instructions;
        const char *notes;
        const char *description;
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GrIngredientsList) ing = NULL;
        g_autoptr(GArray) images = NULL;
        gboolean favorite;
        gboolean same_recipe;
        int index;
        g_autofree char *processed = NULL;

        if (page->recipe)
                old_id = gr_recipe_get_id (page->recipe);
        id = gr_recipe_get_id (recipe);
        same_recipe = g_strcmp0 (old_id, id) == 0;

        g_set_object (&page->recipe, recipe);

        author = gr_recipe_get_author (recipe);
        serves = gr_recipe_get_serves (recipe);
        prep_time = gr_recipe_get_prep_time (recipe);
        cook_time = gr_recipe_get_cook_time (recipe);
        ingredients = gr_recipe_get_ingredients (recipe);
        notes = gr_recipe_get_notes (recipe);
        instructions = gr_recipe_get_translated_instructions (recipe);
        description = gr_recipe_get_translated_description (recipe);
        index = gr_recipe_get_default_image (recipe);

        g_object_get (recipe, "images", &images, NULL);
        gr_image_viewer_set_images (GR_IMAGE_VIEWER (page->recipe_image), images);
        gr_image_viewer_show_image (GR_IMAGE_VIEWER (page->recipe_image), index);

        ing = gr_ingredients_list_new (ingredients);
        g_set_object (&page->ingredients, ing);

        if (same_recipe)
                want_serves = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page->serves_spin));
        else
                want_serves = serves;

        populate_ingredients (page, want_serves, serves);

        if (prep_time[0] == '\0')
                gtk_label_set_label (GTK_LABEL (page->prep_time_label), "");
        else
                gtk_label_set_label (GTK_LABEL (page->prep_time_label), _(prep_time));

        if (cook_time[0] == '\0')
                gtk_label_set_label (GTK_LABEL (page->cook_time_label), "");
        else
                gtk_label_set_label (GTK_LABEL (page->cook_time_label), _(cook_time));
        processed = process_instructions (instructions);
        gtk_label_set_label (GTK_LABEL (page->instructions_label), processed);
        gtk_label_set_track_visited_links (GTK_LABEL (page->instructions_label), FALSE);

        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->serves_spin), want_serves);
        gtk_widget_set_sensitive (page->serves_spin, ing != NULL);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        favorite = gr_recipe_store_is_favorite (store, recipe);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->favorite_button), favorite);

        chef = gr_recipe_store_get_chef (store, author);
        g_set_object (&page->chef, chef);

        if (chef) {
                g_autofree char *tmp = NULL;
                g_autofree char *link = NULL;

                link = g_strdup_printf ("<a href=\"chef\">%s</a>", gr_chef_get_name (chef));
                tmp = g_strdup_printf (_("Recipe by %s"), link);
                gtk_widget_show (page->chef_label);
                gtk_label_set_markup (GTK_LABEL (page->chef_label), tmp);
        }
        else {
                gtk_widget_hide (page->chef_label);
        }

        if (notes && notes[0]) {
                gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->notes_field)), notes, -1);
                gtk_label_set_label (GTK_LABEL (page->notes_label), notes);
                gtk_widget_show (page->notes_box);
        }
        else {
                gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->notes_field)), "", -1);
                gtk_widget_hide (page->notes_box);
        }

        if (description && description[0]) {
                gtk_label_set_label (GTK_LABEL (page->description_label), description);
                gtk_widget_show (page->description_label);
        }
        else {
                gtk_widget_hide (page->description_label);
        }

        if (gr_recipe_is_readonly (recipe)) {
                gtk_widget_hide (page->edit_button);
                gtk_widget_hide (page->delete_button);
        }
        else {
                gtk_widget_show (page->edit_button);
                gtk_widget_show (page->delete_button);
        }
}

GrRecipe *
gr_details_page_get_recipe (GrDetailsPage *page)
{
        return page->recipe;
}

static void
details_page_reload (GrDetailsPage *page,
                     GrRecipe      *recipe)
{
        const char *name;
        const char *new_name;

        if (recipe == NULL || page->recipe == NULL)
                return;

        name = gr_recipe_get_id (page->recipe);
        new_name = gr_recipe_get_id (recipe);
        if (g_strcmp0 (name, new_name) == 0)
                gr_details_page_set_recipe (page, recipe);
}

static void
connect_store_signals (GrDetailsPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (details_page_reload), page);
}
