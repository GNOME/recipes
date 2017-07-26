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
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-image.h"
#include "gr-image-viewer.h"
#include "gr-ingredients-list.h"
#include "gr-ingredients-viewer.h"
#include "gr-timer.h"
#include "gr-recipe-printer.h"
#include "gr-recipe-exporter.h"
#include "gr-recipe-formatter.h"
#include "gr-cuisine.h"
#include "gr-meal.h"
#include "gr-season.h"


struct _GrDetailsPage
{
        GtkBox parent_instance;

        GrRecipe *recipe;
        GrChef *chef;
        GrIngredientsList *ingredients;
        char *ing_text;

        GrRecipePrinter *printer;
        GrRecipeExporter *exporter;

        GtkWidget *main_content;
        GtkWidget *recipe_image;
        GtkWidget *prep_time_desc;
        GtkWidget *prep_time_label;
        GtkWidget *cook_time_desc;
        GtkWidget *cook_time_label;
        GtkWidget *cuisine_desc;
        GtkWidget *cuisine_label;
        GtkWidget *meal_desc;
        GtkWidget *meal_label;
        GtkWidget *season_desc;
        GtkWidget *season_label;
        GtkWidget *yield_spin;
        GtkWidget *yield_label;
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

        store = gr_recipe_store_get ();
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

void
gr_details_page_contribute_recipe (GrDetailsPage *page)
{
        if (!page->exporter) {
                GtkWidget *window;

                window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                page->exporter = gr_recipe_exporter_new (GTK_WINDOW (window));
        }

        gr_recipe_exporter_contribute (page->exporter, page->recipe);
}

static void populate_ingredients (GrDetailsPage *page, double scale);

static void
update_yield_label (GrDetailsPage *page,
                    double         yield)
{
        const char *yield_unit;

        yield_unit = gr_recipe_get_yield_unit (page->recipe);
        if (yield_unit && yield_unit[0])
                gtk_label_set_label (GTK_LABEL (page->yield_label), yield_unit);
        else
                gtk_label_set_label (GTK_LABEL (page->yield_label),
                                     yield == 1.0 ? _("serving") : _("servings"));
}

static void
yield_spin_value_changed (GrDetailsPage *page)
{
        double yield;
        double new_value;

        new_value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (page->yield_spin));
        yield = gr_recipe_get_yield (page->recipe);

        update_yield_label (page, new_value);
        populate_ingredients (page, new_value / yield);
}

static int
yield_spin_input (GtkSpinButton *spin,
                   double        *new_val)
{
        char *text;

        text = (char *)gtk_entry_get_text (GTK_ENTRY (spin));

        if (!gr_number_parse (new_val, &text, NULL)) {
                *new_val = 0.0;
                return GTK_INPUT_ERROR;
        }

        return TRUE;
}

static int
yield_spin_output (GtkSpinButton *spin)
{
        GtkAdjustment *adj;
        g_autofree char *text = NULL;

        adj = gtk_spin_button_get_adjustment (spin);
        text = gr_number_format (gtk_adjustment_get_value (adj));
        gtk_entry_set_text (GTK_ENTRY (spin), text);

        return TRUE;
}

static void
cook_it_later (GrDetailsPage *page)
{
        GrRecipeStore *store;

        store = gr_recipe_store_get ();
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
        double yield;

        store = gr_recipe_store_get ();
        yield = gtk_spin_button_get_value (GTK_SPIN_BUTTON (page->yield_spin));
        gr_recipe_store_add_to_shopping (store, page->recipe, yield);

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_offer_shopping (GR_WINDOW (window));
}

static gboolean save_notes (gpointer data);
static void details_page_reload (GrDetailsPage *page, GrRecipe *recipe);

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
        g_clear_pointer (&self->ing_text, g_free);

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

        store = gr_recipe_store_get ();

        g_signal_handlers_block_by_func (store, details_page_reload, page);
        if (!gr_recipe_store_update_recipe (store, page->recipe, id, &error)) {
                g_warning ("Error: %s", error->message);
        }
        g_signal_handlers_unblock_by_func (store, details_page_reload, page);

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
activate_link (GtkLabel      *label,
               const char    *uri,
               GrDetailsPage *page)
{
        if (g_str_has_prefix (uri, "image:")) {
                int idx;

                idx = (int)g_ascii_strtoll (uri + strlen ("image:"), NULL, 10);
                gr_image_viewer_show_image (GR_IMAGE_VIEWER (page->recipe_image), idx);
        }
        else if (g_str_has_prefix (uri, "recipe:")) {
                GrRecipeStore *store;
                const char *id;
                g_autoptr(GrRecipe) recipe = NULL;

                store = gr_recipe_store_get ();

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

        return TRUE;
}

static void
activate_image (GrDetailsPage *page)
{
        GtkWidget *window;
        GPtrArray *images = NULL;
        int idx;

        images = gr_image_viewer_get_images (GR_IMAGE_VIEWER (page->recipe_image));
        idx = gr_image_viewer_get_index (GR_IMAGE_VIEWER (page->recipe_image));

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

        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, main_content);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, recipe_image);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, prep_time_desc);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, prep_time_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, cook_time_desc);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, cook_time_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, cuisine_desc);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, cuisine_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, meal_desc);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, meal_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, season_desc);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, season_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, yield_spin);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, yield_label);
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
        gtk_widget_class_bind_template_callback (widget_class, yield_spin_value_changed);
        gtk_widget_class_bind_template_callback (widget_class, yield_spin_input);
        gtk_widget_class_bind_template_callback (widget_class, yield_spin_output);
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
                      double         scale)
{
        g_autoptr(GtkSizeGroup) group = NULL;
        g_autofree char **segments = NULL;
        int j;
        GtkWidget *list;

        container_remove_all (GTK_CONTAINER (page->ingredients_box));

        group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        segments = gr_ingredients_list_get_segments (page->ingredients);
        for (j = 0; segments[j]; j++) {
                list = g_object_new (GR_TYPE_INGREDIENTS_VIEWER,
                                     "title", segments[j],
                                     "editable-title", FALSE,
                                     "editable", FALSE,
                                     "scale", scale,
                                     "ingredients", page->ing_text,
                                     NULL);
                gtk_container_add (GTK_CONTAINER (page->ingredients_box), list);
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
        g_autoptr(GPtrArray) steps = NULL;
        GString *s;
        int i;

        steps = gr_recipe_parse_instructions (instructions, TRUE);

        s = g_string_new ("");

        for (i = 0; i < steps->len; i++) {
                GrRecipeStep *step = (GrRecipeStep *)g_ptr_array_index (steps, i);
                g_autofree char *escaped = NULL;

                if (i > 0)
                        g_string_append (s, "\n\n");

                if (step->timer != 0) {
                        int seconds;
                        int minutes;
                        int hours;
                        g_autofree char *str = NULL;

                        seconds = (int)(step->timer / G_TIME_SPAN_SECOND);
                        minutes = seconds / 60;
                        seconds = seconds - 60 * minutes;
                        hours = minutes / 60;
                        minutes = minutes - 60 * hours;

                        str = g_strdup_printf ("%02d∶%02d∶%02d", hours, minutes, seconds);
                        g_string_append (s, "<a href=\"timer\" title=\"");
                        g_string_append_printf (s, _("Timer: %s"), str);
                        if (step->title)
                                g_string_append_printf (s, "\n%s", step->title);
                        g_string_append (s, "\">◇</a>");
                }
                else if (step->image != -1) {
                        g_string_append_printf (s, "<a href=\"image:%d\" title=\"", step->image);
                        g_string_append_printf (s, _("Image %d"), step->image + 1);
                        g_string_append (s, "\">◆</a>");
                }

                escaped = g_markup_escape_text (step->text, -1);
                g_string_append (s, escaped);
        }

        return g_string_free (s, FALSE);
}

static void
scroll_up (GrDetailsPage *page)
{
        GtkAdjustment *adj;

        adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (page->main_content));
        gtk_adjustment_set_value (adj, gtk_adjustment_get_lower (adj));
}

void
gr_details_page_set_recipe (GrDetailsPage *page,
                            GrRecipe      *recipe)
{
        const char *author;
        const char *prep_time;
        const char *cook_time;
        const char *cuisine;
        const char *meal;
        const char *season;
        double yield;
        const char *ingredients;
        const char *instructions;
        const char *notes;
        const char *description;
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GrIngredientsList) ing = NULL;
        GPtrArray *images;
        gboolean favorite;
        int index;
        g_autofree char *processed = NULL;
        g_autofree char *amount = NULL;
        g_autofree char *unit = NULL;

        g_set_object (&page->recipe, recipe);

        author = gr_recipe_get_author (recipe);
        yield = gr_recipe_get_yield (recipe);
        prep_time = gr_recipe_get_prep_time (recipe);
        cook_time = gr_recipe_get_cook_time (recipe);
        cuisine = gr_recipe_get_cuisine (recipe);
        meal = gr_recipe_get_category (recipe);
        season = gr_recipe_get_season (recipe);
        ingredients = gr_recipe_get_ingredients (recipe);
        notes = gr_recipe_get_translated_notes (recipe);
        instructions = gr_recipe_get_translated_instructions (recipe);
        description = gr_recipe_get_translated_description (recipe);
        index = gr_recipe_get_default_image (recipe);

        images = gr_recipe_get_images (recipe);
        gr_image_viewer_set_images (GR_IMAGE_VIEWER (page->recipe_image), images, index);

        ing = gr_ingredients_list_new (ingredients);
        g_set_object (&page->ingredients, ing);
        g_free (page->ing_text);
        page->ing_text = g_strdup (ingredients);

        populate_ingredients (page, 1.0);

        if (prep_time[0] == '\0') {
                gtk_widget_hide (page->prep_time_label);
                gtk_widget_hide (page->prep_time_desc);
        }
        else {
                gtk_widget_show (page->prep_time_label);
                gtk_widget_show (page->prep_time_desc);
                gtk_label_set_label (GTK_LABEL (page->prep_time_label), _(prep_time));
        }

        if (cook_time[0] == '\0') {
                gtk_widget_hide (page->cook_time_label);
                gtk_widget_hide (page->cook_time_desc);
        }
        else {
                gtk_widget_show (page->cook_time_label);
                gtk_widget_show (page->cook_time_desc);
                gtk_label_set_label (GTK_LABEL (page->cook_time_label), _(cook_time));
        }
        if (cuisine[0] == '\0') {
                gtk_widget_hide (page->cuisine_label);
                gtk_widget_hide (page->cuisine_desc);
        }
        else {
                const char *title;

                gr_cuisine_get_data (cuisine, &title, NULL, NULL);
                gtk_widget_show (page->cuisine_label);
                gtk_widget_show (page->cuisine_desc);
                gtk_label_set_label (GTK_LABEL (page->cuisine_label), title);
        }
        if (meal[0] == '\0') {
                gtk_widget_hide (page->meal_label);
                gtk_widget_hide (page->meal_desc);
        }
        else {
                gtk_widget_show (page->meal_label);
                gtk_widget_show (page->meal_desc);
                gtk_label_set_label (GTK_LABEL (page->meal_label), gr_meal_get_title (meal));
        }
        if (season[0] == '\0') {
                gtk_widget_hide (page->season_label);
                gtk_widget_hide (page->season_desc);
        }
        else {
                gtk_widget_show (page->season_label);
                gtk_widget_show (page->season_desc);
                gtk_label_set_label (GTK_LABEL (page->season_label), gr_season_get_title (season));
        }

        processed = process_instructions (instructions);
        gtk_label_set_label (GTK_LABEL (page->instructions_label), processed);
        gtk_label_set_track_visited_links (GTK_LABEL (page->instructions_label), FALSE);

        update_yield_label (page, yield);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->yield_spin), yield);
        gtk_widget_set_sensitive (page->yield_spin, ing != NULL);

        store = gr_recipe_store_get ();

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
        else if (gr_recipe_is_contributed (recipe)) {
                gtk_widget_show (page->edit_button);
                gtk_widget_hide (page->delete_button);
        }
        else {
                gtk_widget_show (page->edit_button);
                gtk_widget_show (page->delete_button);
        }

        scroll_up (page);
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

        if (!gtk_widget_is_drawable (GTK_WIDGET (page)))
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

        store = gr_recipe_store_get ();

        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (details_page_reload), page);
}
