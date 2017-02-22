/* gr-edit-page.c:
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

#include "gr-edit-page.h"
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-app.h"
#include "gr-utils.h"
#include "gr-ingredients-list.h"
#include "gr-cuisine.h"
#include "gr-meal.h"
#include "gr-season.h"
#include "gr-images.h"
#include "gr-image-viewer.h"
#include "gr-ingredient-row.h"
#include "gr-ingredient.h"
#include "gr-number.h"
#include "gr-unit.h"
#include "gr-chef-dialog.h"
#include "gr-cooking-view.h"
#include "gr-window.h"
#include "gr-account.h"
#include "gr-recipe-tile.h"
#include "gr-recipe-formatter.h"


struct _GrEditPage
{
        GtkBox parent_instance;

        GrRecipe *recipe;

        GtkWidget *error_revealer;
        GtkWidget *error_label;
        GtkWidget *name_label;
        GtkWidget *name_entry;
        GtkWidget *cuisine_combo;
        GtkWidget *category_combo;
        GtkWidget *season_combo;
        GtkWidget *spiciness_combo;
        GtkWidget *prep_time_combo;
        GtkWidget *cook_time_combo;
        GtkWidget *description_field;
        GtkWidget *instructions_field;
        GtkWidget *serves_spin;
        GtkWidget *gluten_free_check;
        GtkWidget *nut_free_check;
        GtkWidget *vegan_check;
        GtkWidget *vegetarian_check;
        GtkWidget *milk_free_check;
        GtkWidget *images;
        GtkWidget *add_image_button;
        GtkWidget *remove_image_button;
        GtkWidget *default_image_button;
        GtkWidget *default_image_image;
        GtkWidget *rotate_image_right_button;
        GtkWidget *rotate_image_left_button;
        GtkWidget *ingredient_popover;
        GtkWidget *new_ingredient_name;
        GtkWidget *new_ingredient_amount;
        GtkWidget *new_ingredient_unit;
        GtkWidget *new_ingredient_add_button;
        GtkWidget *author_label;
        GtkWidget *ingredients_box;
        GtkWidget *cooking_view;

        GtkWidget *ing_list;
        GtkWidget *ing_search_button;
        GtkWidget *ing_search_button_label;
        GtkWidget *ing_search_revealer;

        GtkWidget *unit_list;
        GtkWidget *amount_search_button;
        GtkWidget *amount_search_button_label;
        GtkWidget *amount_search_revealer;
        GtkWidget *recipe_popover;
        GtkWidget *recipe_list;
        GtkWidget *recipe_filter_entry;
        GtkWidget *add_step_button;
        GtkWidget *link_image_button;
        GtkWidget *timer_button;
        GtkWidget *prev_step_button;
        GtkWidget *next_step_button;
        GtkWidget *image_popover;
        GtkWidget *image_flowbox;
        GtkWidget *timer_popover;
        GtkWidget *temperature_popover;
        GtkWidget *temperature_spin;
        GtkWidget *celsius_button;
        GtkWidget *timer_spin;
        GtkWidget *preview_stack;

        GrRecipeSearch *search;

        GtkSizeGroup *group;

        guint account_response_signal_id;

        GList *segments;
        GtkWidget *active_row;

        char *ing_term;
        char *unit_term;
        char *recipe_term;

        guint index_handler_id;

        char *author;
};

G_DEFINE_TYPE (GrEditPage, gr_edit_page, GTK_TYPE_BOX)

static char *get_text_view_text (GtkTextView *textview);
static void  set_text_view_text (GtkTextView *textview,
                                 const char  *text);

static void
dismiss_error (GrEditPage *page)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->error_revealer), FALSE);
}

static void add_image_cb (GrEditPage *page);

static void
populate_image_flowbox (GrEditPage *page)
{
        int i;
        g_autoptr(GArray) images = NULL;
        GtkWidget *button;

        g_object_get (page->images, "images", &images, NULL);

        container_remove_all (GTK_CONTAINER (page->image_flowbox));

        for (i = 0; i < images->len; i++) {
                GrImage *ri = &g_array_index (images, GrImage, i);
                g_autoptr(GdkPixbuf) pb = load_pixbuf_fill_size (ri->path, 0, 60, 40);
                GtkWidget *image;
                GtkWidget *child;

                image = gtk_image_new_from_pixbuf (pb);
                gtk_widget_show (image);
                gtk_container_add (GTK_CONTAINER (page->image_flowbox), image);
                child = gtk_widget_get_parent (image);
                g_object_set_data (G_OBJECT (child), "image-idx", GINT_TO_POINTER (i));
        }

        button = gtk_button_new ();
        gtk_container_add (GTK_CONTAINER (button), gtk_image_new_from_icon_name ("list-add-symbolic", 1));
        gtk_widget_show_all (button);
        gtk_container_add (GTK_CONTAINER (page->image_flowbox), button);
        g_signal_connect_swapped (button, "clicked", G_CALLBACK (add_image_cb), page);
}

static void
update_image_button_sensitivity (GrEditPage *page)
{
        g_autoptr(GArray) images = NULL;
        int length;

        g_object_get (page->images, "images", &images, NULL);
        length = images->len;
        gtk_widget_set_sensitive (page->add_image_button, TRUE);
        gtk_widget_set_sensitive (page->remove_image_button, length > 0);
        gtk_widget_set_sensitive (page->rotate_image_left_button, length > 0);
        gtk_widget_set_sensitive (page->rotate_image_right_button, length > 0);
        gtk_widget_set_sensitive (page->default_image_button, length > 0);
}

static void
update_default_image_button (GrEditPage *page)
{
        int index;

        g_object_get (page->images, "index", &index, NULL);

        if (page->recipe && gr_recipe_get_default_image (page->recipe) == index) {
                gtk_widget_set_state_flags (page->default_image_button, GTK_STATE_FLAG_CHECKED, FALSE);
                gtk_image_set_from_icon_name (GTK_IMAGE (page->default_image_image), "starred-symbolic", 1);
        }
        else {
                gtk_widget_unset_state_flags (page->default_image_button, GTK_STATE_FLAG_CHECKED);
                gtk_image_set_from_icon_name (GTK_IMAGE (page->default_image_image), "non-starred-symbolic", 1);
        }
}

static void
set_default_image_cb (GrEditPage *page)
{
        int index;

        g_object_get (page->images, "index", &index, NULL);

        if (page->recipe)
                g_object_set (page->recipe, "default-image", index, NULL);
}

static void
images_changed (GrEditPage *page)
{
        update_image_button_sensitivity (page);
        populate_image_flowbox (page);
        update_default_image_button (page);
}

static void
index_changed (GrEditPage *page)
{
        update_default_image_button (page);
}

static void
add_image_cb (GrEditPage *page)
{
        gr_image_viewer_add_image (GR_IMAGE_VIEWER (page->images));
}

static char *
rewrite_instructions_for_removed_image (const char *instructions,
                                        int         index)
{
        g_autoptr(GPtrArray) steps = NULL;
        GString *s;
        int i;

        steps = gr_recipe_parse_instructions (instructions, FALSE);

        for (i = 0; i < steps->len; i++) {
                GrRecipeStep *step = g_ptr_array_index (steps, i);

                if (step->image == index) {
                        step->image = -1;
                }
                else if (step->image > index) {
                        step->image -= 1;
                }
        }

        s = g_string_new ("");

        for (i = 0; i < steps->len; i++) {
                GrRecipeStep *step = g_ptr_array_index (steps, i);

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

                        g_string_append_printf (s, "[timer:%02d:%02d:%02d]", hours, minutes, seconds);
                }
                else if (step->image != -1) {
                        g_string_append_printf (s, "[image:%d]", step->image);
                }

                g_string_append (s, step->text);
        }

        return g_string_free (s, FALSE);
}

static void
remove_image_cb (GrEditPage *page)
{
        int index;
        g_autofree char *instructions = NULL;
        g_autofree char *rewritten = NULL;

        index = gr_image_viewer_get_index (GR_IMAGE_VIEWER (page->images));
        gr_image_viewer_remove_image (GR_IMAGE_VIEWER (page->images));

        instructions = get_text_view_text (GTK_TEXT_VIEW (page->instructions_field));
        rewritten = rewrite_instructions_for_removed_image (instructions, index);
        set_text_view_text (GTK_TEXT_VIEW (page->instructions_field), rewritten);
}

static void
rotate_image_left_cb (GrEditPage *page)
{
        gr_image_viewer_rotate_image (GR_IMAGE_VIEWER (page->images), 90);
}

static void
rotate_image_right_cb (GrEditPage *page)
{
        gr_image_viewer_rotate_image (GR_IMAGE_VIEWER (page->images), 270);
}

static void
edit_page_finalize (GObject *object)
{
        GrEditPage *self = GR_EDIT_PAGE (object);

        if (self->index_handler_id)
                g_signal_handler_disconnect (self->recipe, self->index_handler_id);
        g_clear_object (&self->recipe);
        g_clear_object (&self->group);
        g_list_free (self->segments);

        g_free (self->ing_term);
        g_free (self->unit_term);
        g_free (self->recipe_term);

        g_clear_object (&self->search);

        g_free (self->author);

        G_OBJECT_CLASS (gr_edit_page_parent_class)->finalize (object);
}

static void
populate_cuisine_combo (GrEditPage *page)
{
        const char **names;
        const char *title;
        int length;
        int i;

        names = gr_cuisine_get_names (&length);
        for (i = 0; i < length; i++) {
                gr_cuisine_get_data (names[i], &title, NULL, NULL);
                gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->cuisine_combo),
                                           names[i],
                                           title);
        }
}

static void
populate_category_combo (GrEditPage *page)
{
        const char **names;
        int length;
        int i;

        names = gr_meal_get_names (&length);
        for (i = 0; i < length; i++) {
                gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->category_combo),
                                           names[i],
                                           gr_meal_get_title (names[i]));
        }
}

static void
populate_season_combo (GrEditPage *page)
{
        const char **names;
        int length;
        int i;

        names = gr_season_get_names (&length);
        for (i = 0; i < length; i++) {
                gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->season_combo),
                                           names[i],
                                           gr_season_get_title (names[i]));
        }
}

static void
ingredient_changed (GrEditPage *page)
{
        const char *text;

        text = gtk_entry_get_text (GTK_ENTRY (page->new_ingredient_name));
        gtk_widget_set_sensitive (page->new_ingredient_add_button, strlen (text) > 0);
}

static void hide_ingredients_search_list (GrEditPage *self,
                                          gboolean    animate);

static void hide_units_search_list (GrEditPage *self,
                                    gboolean    animate);

static void
add_ingredient (GtkButton *button, GrEditPage *page)
{
        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_name), "");
        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_amount), "1");
        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_unit), "");
        gtk_label_set_label (GTK_LABEL (page->ing_search_button_label), _("Ingredient"));
        gtk_label_set_label (GTK_LABEL (page->amount_search_button_label), _("Amount"));

        gtk_popover_set_relative_to (GTK_POPOVER (page->ingredient_popover), GTK_WIDGET (button));
        gtk_button_set_label (GTK_BUTTON (page->new_ingredient_add_button), _("Add"));
        ingredient_changed (page);

        hide_ingredients_search_list (page, FALSE);
        hide_units_search_list (page, FALSE);

        gtk_popover_popup (GTK_POPOVER (page->ingredient_popover));
}

static void
remove_ingredient (GtkButton *button, GrEditPage *page)
{
        GtkWidget *row;

        row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
        if (!row)
                return;

        if (row == page->active_row)
                page->active_row = NULL;

        gtk_widget_destroy (row);
}

static void
edit_ingredients_row (GtkListBoxRow *row,
                      GrEditPage    *page)
{
        const char *amount;
        const char *unit;
        const char *ingredient;
        char *tmp;

        amount = (const char *)g_object_get_data (G_OBJECT (row), "amount");
        unit = (const char *)g_object_get_data (G_OBJECT (row), "unit");
        ingredient = (const char *)g_object_get_data (G_OBJECT (row), "ingredient");

        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_name), ingredient);
        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_amount), amount);
        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_unit), unit);
        gtk_label_set_label (GTK_LABEL (page->ing_search_button_label), ingredient);
        tmp = g_strconcat (amount, " ", unit, NULL);
        gtk_label_set_label (GTK_LABEL (page->amount_search_button_label), tmp);
        g_free (tmp);

        gtk_popover_set_relative_to (GTK_POPOVER (page->ingredient_popover), GTK_WIDGET (row));
        gtk_button_set_label (GTK_BUTTON (page->new_ingredient_add_button), _("Apply"));
        ingredient_changed (page);

        hide_ingredients_search_list (page, FALSE);
        hide_units_search_list (page, FALSE);

        gtk_popover_popup (GTK_POPOVER (page->ingredient_popover));
}

static void
edit_ingredient (GtkButton *button, GrEditPage *page)
{
        GtkWidget *row;

        row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
        if (!row)
                return;

        edit_ingredients_row (GTK_LIST_BOX_ROW (row), page);
}

static void selected_rows_changed (GtkListBox *list,
                                   GrEditPage *page);

static void
set_active_row (GrEditPage *page,
                GtkWidget *row)
{
        GtkWidget *stack;
        GtkWidget *list;
        GtkWidget *active_list;

        if (page->active_row) {
                stack = g_object_get_data (G_OBJECT (page->active_row), "buttons-stack");
                gtk_stack_set_visible_child_name (GTK_STACK (stack), "empty");

                list = row ? gtk_widget_get_parent (row) : NULL;
                active_list = gtk_widget_get_parent (page->active_row);
                if (list != active_list) {
                        g_signal_handlers_block_by_func (active_list, selected_rows_changed, page);
                        gtk_list_box_unselect_row (GTK_LIST_BOX (active_list), GTK_LIST_BOX_ROW (page->active_row));
                        g_signal_handlers_unblock_by_func (active_list, selected_rows_changed, page);
                }
        }

        if (page->active_row == row) {
                page->active_row = NULL;
                return;
        }

        page->active_row = row;

        if (page->active_row) {
                stack = g_object_get_data (G_OBJECT (page->active_row), "buttons-stack");
                gtk_stack_set_visible_child_name (GTK_STACK (stack), "buttons");
        }
}

static void
selected_rows_changed (GtkListBox *list,
                       GrEditPage *page)
{
        GtkListBoxRow *row;

        row = gtk_list_box_get_selected_row (list);
        set_active_row (page, GTK_WIDGET (row));
}

static void
update_ingredient_row (GtkWidget  *row,
                       const char *amount,
                       const char *unit,
                       const char *ingredient)
{
        GtkWidget *label;
        char *tmp;

        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "unit-label"));
        tmp = g_strconcat (amount, " ", unit, NULL);
        gtk_label_set_label (GTK_LABEL (label), tmp);
        g_free (tmp);

        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "ingredient-label"));
        gtk_label_set_label (GTK_LABEL (label), ingredient);

        g_object_set_data_full (G_OBJECT (row), "amount", g_strdup (amount), g_free);
        g_object_set_data_full (G_OBJECT (row), "unit", g_strdup (unit), g_free);
        g_object_set_data_full (G_OBJECT (row), "ingredient", g_strdup (ingredient), g_free);
}

static GtkWidget *
add_ingredient_row (GrEditPage   *page,
                    GtkWidget    *list,
                    GtkSizeGroup *group)
{
        GtkWidget *box;
        GtkWidget *unit_label;
        GtkWidget *ingredient_label;
        GtkWidget *row;
        GtkWidget *stack;
        GtkWidget *buttons;
        GtkWidget *button;
        GtkWidget *image;

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_show (box);

        unit_label = gtk_label_new ("");
        g_object_set (unit_label,
                      "visible", TRUE,
                      "xalign", 0.0,
                      "margin", 10,
                      NULL);
        gtk_style_context_add_class (gtk_widget_get_style_context (unit_label), "dim-label");
        gtk_container_add (GTK_CONTAINER (box), unit_label);
        gtk_size_group_add_widget (group, unit_label);

        ingredient_label = gtk_label_new ("");
        g_object_set (ingredient_label,
                      "visible", TRUE,
                      "xalign", 0.0,
                      "margin", 10,
                      NULL);
        gtk_container_add (GTK_CONTAINER (box), ingredient_label);

        stack = gtk_stack_new ();
        gtk_widget_set_halign (stack, GTK_ALIGN_END);
        gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_NONE);
        gtk_widget_show (stack);
        image = gtk_image_new ();
        gtk_widget_show (image);
        gtk_widget_set_opacity (image, 0);
        gtk_stack_add_named (GTK_STACK (stack), image, "empty");
        buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        g_object_set (buttons, "margin", 4, NULL);
        button = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        g_signal_connect (button, "clicked", G_CALLBACK (edit_ingredient), page);
        image = gtk_image_new_from_icon_name ("document-edit-symbolic", 1);
        gtk_container_add (GTK_CONTAINER (button), image);
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "circular");
        gtk_container_add (GTK_CONTAINER (buttons), button);
        button = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        g_signal_connect (button, "clicked", G_CALLBACK (remove_ingredient), page);
        image = gtk_image_new_from_icon_name ("user-trash-symbolic", 1);
        gtk_container_add (GTK_CONTAINER (button), image);
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "circular");
        gtk_container_add (GTK_CONTAINER (buttons), button);
        gtk_widget_show_all (buttons);
        gtk_stack_add_named (GTK_STACK (stack), buttons, "buttons");
        gtk_box_pack_end (GTK_BOX (box), stack, TRUE, TRUE, 0);

        gtk_container_add (GTK_CONTAINER (list), box);
        row = gtk_widget_get_parent (box);
        g_object_set_data (G_OBJECT (row), "unit-label", unit_label);
        g_object_set_data (G_OBJECT (row), "ingredient-label", ingredient_label);
        g_object_set_data (G_OBJECT (row), "buttons-stack", stack);

        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);

        return row;
}

static void
format_unit_for_display (const char *amount,
                         const char *unit,
                         char **amount_out,
                         char **unit_out)
{
        if (amount[0] != '\0') {
                GrNumber number;
                char *a = (char *)amount;
                if (gr_number_parse (&number, &a, NULL))
                       *amount_out = gr_number_format (&number);
                else
                       *amount_out = g_strdup (amount);
        }
        else
                *amount_out = g_strdup (amount);

        if (unit[0] != '\0') {
                const char *name;
                char *u = (char *)unit;
                name = gr_unit_parse (&u, NULL);
                if (name)
                        *unit_out = g_strdup (gr_unit_get_abbreviation (name));
                else
                        *unit_out = g_strdup (unit);
        }
        else
                *unit_out = g_strdup (unit);
}

static void
add_ingredient2 (GtkButton *button, GrEditPage *page)
{
        const char *ingredient;
        g_autofree char *amount = NULL;
        g_autofree  char *unit = NULL;
        GtkWidget *list;
        GtkWidget *b;
        GtkWidget *row;

        gtk_popover_popdown (GTK_POPOVER (page->ingredient_popover));

        b = gtk_popover_get_relative_to (GTK_POPOVER (page->ingredient_popover));
        if (GTK_IS_LIST_BOX_ROW (b)) {
             row = b;
        }
        else {
                list = GTK_WIDGET (g_object_get_data (G_OBJECT (b), "list"));
                row = add_ingredient_row (page, list, page->group);
        }

        ingredient = gtk_entry_get_text (GTK_ENTRY (page->new_ingredient_name));
        format_unit_for_display (gtk_entry_get_text (GTK_ENTRY (page->new_ingredient_amount)),
                                 gtk_entry_get_text (GTK_ENTRY (page->new_ingredient_unit)),
                                 &amount, &unit);
        update_ingredient_row (row, amount, unit, ingredient);
}

static char *
collect_ingredients (GrEditPage *page)
{
        GString *s;
        GtkWidget *segment;
        GtkWidget *list;
        GtkWidget *entry;
        GList *children, *l, *k;

        s = g_string_new ("");
        for (k = page->segments; k; k = k->next) {
                segment = k->data;
                list = GTK_WIDGET (g_object_get_data (G_OBJECT (segment), "list"));
                entry = GTK_WIDGET (g_object_get_data (G_OBJECT (segment), "entry"));
                children = gtk_container_get_children (GTK_CONTAINER (list));
                for (l = children; l; l = l->next) {
                        GtkWidget *row = l->data;
                        const char *amount;
                        const char *unit;
                        const char *ingredient;
                        const char *id;

                        amount = (const char *)g_object_get_data (G_OBJECT (row), "amount");
                        unit = (const char *)g_object_get_data (G_OBJECT (row), "unit");
                        ingredient = (const char *)g_object_get_data (G_OBJECT (row), "ingredient");
                        id = gr_ingredient_get_id (ingredient);
                        if (s->len > 0)
                                g_string_append (s, "\n");
                        g_string_append (s, amount);
                        g_string_append (s, "\t");
                        g_string_append (s, unit);
                        g_string_append (s, "\t");
                        g_string_append (s, id ? id : ingredient);
                        g_string_append (s, "\t");
                        g_string_append (s, gtk_entry_get_text (GTK_ENTRY (entry)));
                }
                g_list_free (children);
        }

        return g_string_free (s, FALSE);
}

static void
show_ingredients_search_list (GrEditPage *self)
{
        gtk_widget_hide (self->ing_search_button);
        gtk_widget_show (self->ing_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->ing_search_revealer), TRUE);
}

static void
hide_ingredients_search_list (GrEditPage *self,
                              gboolean    animate)
{
        gtk_widget_show (self->ing_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->ing_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->ing_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->ing_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
ing_search_button_clicked (GtkButton  *button,
                           GrEditPage *self)
{
        hide_units_search_list (self, TRUE);
        show_ingredients_search_list (self);
}

static void
ing_row_activated (GtkListBox    *list,
                   GtkListBoxRow *row,
                   GrEditPage *self)
{
        const char *ingredient;

        ingredient = gr_ingredient_row_get_ingredient (GR_INGREDIENT_ROW (row));
        gtk_entry_set_text (GTK_ENTRY (self->new_ingredient_name), ingredient);
        gtk_label_set_label (GTK_LABEL (self->ing_search_button_label), ingredient);
        hide_ingredients_search_list (self, TRUE);
}

static gboolean
ing_filter_func (GtkListBoxRow *row,
                 gpointer       data)
{
        GrEditPage *self = data;
        const char *cf;

        if (!GR_IS_INGREDIENT_ROW (row))
                return TRUE;

        if (!self->ing_term)
                return TRUE;

        cf = gr_ingredient_row_get_filter_term (GR_INGREDIENT_ROW (row));

        return g_str_has_prefix (cf, self->ing_term);
}

static void
ing_filter_changed (GrEditPage *self)
{
        const char *term;

        term = gtk_entry_get_text (GTK_ENTRY (self->new_ingredient_name));
        g_free (self->ing_term);
        self->ing_term = g_utf8_casefold (term, -1);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->ing_list));
}

static void
ing_filter_stop (GrEditPage *self)
{
        gtk_entry_set_text (GTK_ENTRY (self->new_ingredient_name), "");
}

static void
ing_filter_activated (GrEditPage *self)
{
        const char *ingredient;

        ingredient = gtk_entry_get_text (GTK_ENTRY (self->new_ingredient_name));
        gtk_label_set_label (GTK_LABEL (self->ing_search_button_label), ingredient);
        hide_ingredients_search_list (self, TRUE);
}

static void
populate_ingredients_list (GrEditPage *self)
{
        int i;
        const char **ingredients;
        GtkWidget *row;

        ingredients = gr_ingredient_get_names (NULL);
        for (i = 0; ingredients[i]; i++) {
                row = GTK_WIDGET (gr_ingredient_row_new (ingredients[i]));
                gtk_widget_show (row);
                gtk_container_add (GTK_CONTAINER (self->ing_list), row);
        }

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->ing_list),
                                      all_headers, self, NULL);

        gtk_list_box_set_filter_func (GTK_LIST_BOX (self->ing_list),
                                      ing_filter_func, self, NULL);

        g_signal_connect (self->ing_list, "row-activated",
                          G_CALLBACK (ing_row_activated), self);
}

static void
show_units_search_list (GrEditPage *self)
{
        gtk_widget_hide (self->amount_search_button);
        gtk_widget_show (self->amount_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->amount_search_revealer), TRUE);
}

static void
hide_units_search_list (GrEditPage *self,
                        gboolean    animate)
{
        gtk_widget_show (self->amount_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->amount_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->amount_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->amount_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
amount_search_button_clicked (GtkButton  *button,
                              GrEditPage *self)
{
        hide_ingredients_search_list (self, TRUE);
        show_units_search_list (self);
}

static void
unit_row_activated (GtkListBox    *list,
                    GtkListBoxRow *row,
                    GrEditPage *self)
{
        g_autofree char *unit = NULL;
        g_autofree char *amount = NULL;
        char *tmp;

        format_unit_for_display (gtk_entry_get_text (GTK_ENTRY (self->new_ingredient_amount)),
                                 (const char *) g_object_get_data (G_OBJECT (row), "unit"),
                                 &amount, &unit);

        gtk_entry_set_text (GTK_ENTRY (self->new_ingredient_unit), unit);
        tmp = g_strconcat (amount, " ", unit, NULL);
        gtk_label_set_label (GTK_LABEL (self->amount_search_button_label), tmp);
        g_free (tmp);

        hide_units_search_list (self, TRUE);
}

static gboolean
unit_filter_func (GtkListBoxRow *row,
                  gpointer       data)
{
        GrEditPage *self = data;
        const char *unit;
        g_autofree char *cf1 = NULL;
        g_autofree char *cf2 = NULL;

        if (!self->unit_term)
                return TRUE;

        unit = (const char *)g_object_get_data (G_OBJECT (row), "unit");
        cf1 = g_utf8_casefold (gr_unit_get_abbreviation (unit), -1);
        cf2 = g_utf8_casefold (gr_unit_get_display_name (unit), -1);

        return g_str_has_prefix (cf1, self->unit_term) ||
               g_str_has_prefix (cf2, self->unit_term);
}

static void
unit_filter_changed (GrEditPage *self)
{
        const char *term;

        term = gtk_entry_get_text (GTK_ENTRY (self->new_ingredient_unit));
        g_free (self->unit_term);
        self->unit_term = g_utf8_casefold (term, -1);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->unit_list));
}

static void
unit_filter_stop (GrEditPage *self)
{
        gtk_entry_set_text (GTK_ENTRY (self->new_ingredient_unit), "");
}

static void
unit_filter_activated (GrEditPage *self)
{
        const char *amount;
        const char *unit;
        g_autofree char *amount_out = NULL;
        g_autofree char *unit_out = NULL;
        g_autofree char *parsed = NULL;

        amount = gtk_entry_get_text (GTK_ENTRY (self->new_ingredient_amount));
        unit = gtk_entry_get_text (GTK_ENTRY (self->new_ingredient_unit));
        format_unit_for_display (amount, unit, &amount_out, &unit_out);
        parsed = g_strconcat (amount_out, " ", unit_out, NULL);
        gtk_label_set_label (GTK_LABEL (self->amount_search_button_label), parsed);
        hide_units_search_list (self, TRUE);
}

static void
populate_units_list (GrEditPage *self)
{
        const char **units;
        GtkWidget *row;
        GtkWidget *label;
        int i;
        const char *name, *abbrev;

        units = gr_unit_get_names ();
        for (i = 0; units[i]; i++) {
                row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
                gtk_widget_show (row);
                g_object_set (row, "margin", 10, NULL);
                name = gr_unit_get_display_name (units[i]);
                abbrev = gr_unit_get_abbreviation (units[i]);
                if (strcmp (name, abbrev) == 0)
                        label = gtk_label_new (name);
                else {
                        char *tmp;
                        tmp = g_strdup_printf ("%s (%s)", name, abbrev);
                        label = gtk_label_new (tmp);
                        g_free (tmp);
                }
                gtk_widget_show (label);
                gtk_label_set_xalign (GTK_LABEL (label), 0.0);
                gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);

                gtk_container_add (GTK_CONTAINER (self->unit_list), row);
                row = gtk_widget_get_parent (row);
                g_object_set_data (G_OBJECT (row), "unit", (gpointer)units[i]);
        }

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->unit_list),
                                      all_headers, self, NULL);

        gtk_list_box_set_filter_func (GTK_LIST_BOX (self->unit_list),
                                      unit_filter_func, self, NULL);

        g_signal_connect (self->unit_list, "row-activated",
                          G_CALLBACK (unit_row_activated), self);
}

static void
remove_tag_if_found (GtkTextBuffer *buffer)
{
        GtkTextIter start, end;
        g_autofree char *text = NULL;

        gtk_text_buffer_get_iter_at_mark (buffer, &start, gtk_text_buffer_get_insert (buffer));
        gtk_text_iter_set_line_offset (&start, 0);
        end = start;
        gtk_text_iter_forward_to_line_end (&end);
        text = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
        if (g_str_has_prefix (text, "[image:") ||
            g_str_has_prefix (text, "[timer:")) {
                char *p = strstr (text, "]");
                gtk_text_iter_set_line_index (&end, p - text + 1);
                gtk_text_buffer_delete (buffer, &start, &end);
        }
}

static void
add_tag_to_step (GrEditPage *self,
                 const char *tag)
{
        GtkTextBuffer *buffer;
        GtkTextIter start;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->instructions_field));
        remove_tag_if_found (buffer);

        gtk_text_buffer_get_iter_at_mark (buffer, &start, gtk_text_buffer_get_insert (buffer));
        gtk_text_iter_set_line_offset (&start, 0);

        gtk_text_buffer_insert (buffer, &start, tag, -1);
}

static void
add_tag_at_insert (GrEditPage *self,
                   const char *tag)
{
        GtkTextBuffer *buffer;
        GtkTextIter start;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->instructions_field));
        gtk_text_buffer_get_iter_at_mark (buffer, &start, gtk_text_buffer_get_insert (buffer));
        gtk_text_buffer_insert (buffer, &start, tag, -1);
}

static void
image_activated (GtkFlowBox *flowbox,
                 GtkFlowBoxChild *child,
                 GrEditPage *self)
{
        int idx;
        g_autofree char *text = NULL;

        idx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "image-idx"));
        text = g_strdup_printf ("[image:%d]", idx);
        add_tag_to_step (self, text);

        gtk_popover_popdown (GTK_POPOVER (self->image_popover));
        gtk_widget_grab_focus (self->instructions_field);
}

static void
add_image_link (GtkButton *button, GrEditPage *page)
{
        gtk_popover_popup (GTK_POPOVER (page->image_popover));
}

static int
time_spin_input (GtkSpinButton *spin_button,
                 double        *new_val)
{
        const char *text;
        gboolean found = FALSE;

        text = gtk_entry_get_text (GTK_ENTRY (spin_button));
        if (!strchr (text, ':')) {
                g_auto(GStrv) str = NULL;
                int num;
                char *endn;

                str = g_strsplit (text, " ", 2);
                num = strtol (str[0], &endn, 10);
                if (!*endn) {
                        if (str[1] == NULL) {
                                *new_val = num; /* minutes */
                                found = TRUE;
                        }
                        else if (strcmp (str[1], _("hour")) == 0 ||
                                 strcmp (str[1], _("hours")) == 0 ||
                                 strcmp (str[1], C_("hour abbreviation", "h")) == 0) {
                                *new_val = num * 3600; /* hours */
                                found = TRUE;
                        }
                        else if (strcmp (str[1], _("minute")) == 0 ||
                                 strcmp (str[1], _("minutes")) == 0 ||
                                 strcmp (str[1], C_("minute abbreviation", "min")) == 0 ||
                                 strcmp (str[1], C_("minute abbreviation", "m")) == 0) {
                                *new_val = num * 60;
                                found = TRUE;
                        }
                        else if (strcmp (str[1], _("second")) == 0 ||
                                 strcmp (str[1], _("seconds")) == 0 ||
                                 strcmp (str[1], C_("second abbreviation", "sec")) == 0 ||
                                 strcmp (str[1], C_("second abbreviation", "s")) == 0) {
                                *new_val = num;
                                found = TRUE;
                        }
                }
        }
        else {
                g_auto(GStrv) str = NULL;
                int hours;
                int minutes;
                int seconds;
                char *endh;
                char *endm;
                char *ends;

                str = g_strsplit (text, ":", 3);

                if (g_strv_length (str) == 3) {
                        hours = strtol (str[0], &endh,10);
                        minutes = strtol (str[1], &endm, 10);
                        seconds = strtol (str[2], &ends, 10);
                        if (!*endh && !*endm && !*ends &&
                            0 <= hours && hours < 24 &&
                            0 <=  minutes && minutes < 60 &&
                            0 <= seconds && seconds < 60) {
                                *new_val = (hours * 60 + minutes) * 60 + seconds;
                                found = TRUE;
                        }
                }
                else if (g_strv_length (str) == 2) {
                        hours = strtol (str[0], &endh, 10);
                        minutes = strtol (str[1], &endm, 10);
                        if (!*endh && !*endm &&
                            0 <= hours && hours < 24 &&
                            0 <=  minutes && minutes < 60) {
                                *new_val = (hours * 60 + minutes) * 60;
                                found = TRUE;
                        }
                }
        }

        if (!found) {
                *new_val = 0.0;
                return GTK_INPUT_ERROR;
        }

        return TRUE;
}

static int
time_spin_output (GtkSpinButton *spin_button)
{
        GtkAdjustment *adjustment;
        double hours;
        double minutes;
        double seconds;
        g_autofree char *buf = NULL;

        adjustment = gtk_spin_button_get_adjustment (spin_button);
        hours = gtk_adjustment_get_value (adjustment) / 3600.0;
        minutes = (hours - floor (hours)) * 60.0;
        seconds = (minutes - floor (minutes)) * 60.0;
        buf = g_strdup_printf ("%02.0f:%02.0f:%02.0f", floor (hours), floor (minutes), floor (seconds + 0.5));
        if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
                gtk_entry_set_text (GTK_ENTRY (spin_button), buf);

        return TRUE;
}

static void
time_spin_activate (GtkEntry *entry, GrEditPage *self)
{
        GtkAdjustment *adjustment;
        double hours;
        double minutes;
        double seconds;
        g_autofree char *text = NULL;

        gtk_spin_button_update (GTK_SPIN_BUTTON (entry));

        adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (entry));
        hours = gtk_adjustment_get_value (adjustment) / 3600.0;
        minutes = (hours - floor (hours)) * 60.0;
        seconds = (minutes - floor (minutes)) * 60.0;

        text = g_strdup_printf ("[timer:%02.0f:%02.0f:%02.0f]", floor (hours), floor (minutes), floor (seconds + 0.5));
        add_tag_to_step (self, text);

        gtk_popover_popdown (GTK_POPOVER (self->timer_popover));
        gtk_widget_grab_focus (self->instructions_field);
}

static void
add_timer (GtkButton *button, GrEditPage *page)
{
        gtk_popover_popup (GTK_POPOVER (page->timer_popover));
}

static void
temperature_spin_activate (GtkEntry *entry, GrEditPage *self)
{
        int value;
        g_autofree char *text = NULL;
        const char *unit;

        gtk_spin_button_update (GTK_SPIN_BUTTON (entry));

        value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (entry));
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->celsius_button)))
                unit = "C";
        else
                unit = "F";

        text = g_strdup_printf ("[temperature:%d%s]", value, unit);
        add_tag_at_insert (self, text);

        gtk_popover_popdown (GTK_POPOVER (self->temperature_popover));
        gtk_widget_grab_focus (self->instructions_field);
}
static void
add_temperature (GtkButton *button, GrEditPage *page)
{
        gtk_popover_popup (GTK_POPOVER (page->temperature_popover));
}

static void
add_step (GtkButton *button, GrEditPage *self)
{
        GtkTextBuffer *buffer;
        GtkTextIter end;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->instructions_field));
        gtk_text_buffer_get_end_iter (buffer, &end);
        gtk_text_buffer_place_cursor (buffer, &end);
        gtk_text_buffer_insert_at_cursor (buffer, "\n\n", 2);
}

static void update_author_label (GrEditPage *page,
                                 GrChef     *chef);

static void
chef_done (GrChefDialog *dialog, GrChef *chef, GrEditPage *page)
{
        if (chef) {
                g_free (page->author);
                page->author = g_strdup (gr_chef_get_id (chef));
                update_author_label (page, chef);
        }
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
edit_chef (GrEditPage *page)
{
        GrChefDialog *dialog;
        GtkWidget *win;
        const char *author;
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;

        win = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        author = page->author;
        chef = gr_recipe_store_get_chef (store, author ? author : "");

        dialog = gr_chef_dialog_new (chef, TRUE);
        g_signal_connect (dialog, "done", G_CALLBACK (chef_done), page);

        gtk_window_set_title (GTK_WINDOW (dialog), _("Recipe Author"));
        gr_window_present_dialog (GR_WINDOW (win), GTK_WINDOW (dialog));

        return TRUE;
}

static void
gr_edit_page_init (GrEditPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));

        page->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

        populate_cuisine_combo (page);
        populate_category_combo (page);
        populate_season_combo (page);
        populate_ingredients_list (page);
        populate_units_list (page);

#ifdef ENABLE_GSPELL
        {
                GspellTextView *gspell_view;

                gspell_view = gspell_text_view_get_from_gtk_text_view (GTK_TEXT_VIEW (page->description_field));
                gspell_text_view_basic_setup (gspell_view);

                gspell_view = gspell_text_view_get_from_gtk_text_view (GTK_TEXT_VIEW (page->instructions_field));
                gspell_text_view_basic_setup (gspell_view);
        }
#endif
}

static gboolean
popover_keypress_handler (GtkWidget  *widget,
                          GdkEvent   *event,
                          GrEditPage *page)
{
        GtkWidget *focus;

        focus = gtk_window_get_focus (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))));

        if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->ing_search_revealer))) {
                if (focus == NULL || !gtk_widget_is_ancestor (focus, page->ing_search_revealer)) {
                        gtk_widget_grab_focus (page->new_ingredient_name);
                        return gtk_widget_event (page->new_ingredient_name, event);
                }
        }
        else if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->amount_search_revealer))) {
                if (focus == NULL || !gtk_widget_is_ancestor (focus, page->amount_search_revealer)) {
                        gtk_widget_grab_focus (page->new_ingredient_unit);
                        return gtk_widget_event (page->new_ingredient_unit, event);
                }
        }
        else if (gtk_widget_is_sensitive (page->new_ingredient_add_button)) {
                if (event->type == GDK_KEY_PRESS) {
                        GdkEventKey *e = (GdkEventKey *) event;
                        if (e->keyval == GDK_KEY_Return ||
                            e->keyval == GDK_KEY_ISO_Enter ||
                            e->keyval == GDK_KEY_KP_Enter) {
                                gtk_widget_activate (page->new_ingredient_add_button);
                                return GDK_EVENT_STOP;
                        }
                }
        }

        return GDK_EVENT_PROPAGATE;
}

static void
update_steppers (GrEditPage *page)
{
        int step;
        int n_steps;

        step = gr_cooking_view_get_step (GR_COOKING_VIEW (page->cooking_view));
        n_steps = gr_cooking_view_get_n_steps (GR_COOKING_VIEW (page->cooking_view));

        gtk_widget_set_sensitive (page->prev_step_button, step - 1 >= 0);
        gtk_widget_set_sensitive (page->next_step_button, step + 1 <= n_steps - 1);
}

static void
preview_visible_changed (GrEditPage *page)
{
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (page->preview_stack));

        if (strcmp (visible, "edit") == 0) {
                gtk_widget_set_sensitive (page->add_step_button, TRUE);
                gtk_widget_set_sensitive (page->link_image_button, TRUE);
                gtk_widget_set_sensitive (page->timer_button, TRUE);
                gtk_widget_set_visible (page->prev_step_button, FALSE);
                gtk_widget_set_visible (page->next_step_button, FALSE);
        }
        else {
                g_autoptr(GArray) images = NULL;
                g_autofree char *instructions = NULL;
                int index = 0;

                gtk_widget_set_sensitive (page->add_step_button, FALSE);
                gtk_widget_set_sensitive (page->link_image_button, FALSE);
                gtk_widget_set_sensitive (page->timer_button, FALSE);
                gtk_widget_set_visible (page->prev_step_button, TRUE);
                gtk_widget_set_visible (page->next_step_button, TRUE);

                g_object_get (page->images, "images", &images, NULL);
                instructions = get_text_view_text (GTK_TEXT_VIEW (page->instructions_field));

                gr_cooking_view_set_images (GR_COOKING_VIEW (page->cooking_view), images, index);
                gr_cooking_view_set_instructions (GR_COOKING_VIEW (page->cooking_view), instructions);

                update_steppers (page);
        }
}

static void
prev_step (GrEditPage *page)
{
        gr_cooking_view_prev_step (GR_COOKING_VIEW (page->cooking_view));
        update_steppers (page);
}

static void
next_step (GrEditPage *page)
{
        gr_cooking_view_next_step (GR_COOKING_VIEW (page->cooking_view));
        update_steppers (page);
}

static void
gr_edit_page_class_init (GrEditPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = edit_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-edit-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrEditPage, error_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, error_label);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, name_label);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, name_entry);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, cuisine_combo);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, category_combo);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, season_combo);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, prep_time_combo);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, cook_time_combo);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, serves_spin);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, spiciness_combo);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, description_field);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, instructions_field);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, gluten_free_check);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, nut_free_check);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, vegan_check);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, vegetarian_check);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, milk_free_check);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, images);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, add_image_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, remove_image_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, default_image_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, default_image_image);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, rotate_image_left_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, rotate_image_right_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, author_label);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, ingredients_box);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, ingredient_popover);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, new_ingredient_name);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, new_ingredient_amount);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, new_ingredient_unit);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, new_ingredient_add_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, ing_list);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, ing_search_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, ing_search_button_label);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, ing_search_revealer);

        gtk_widget_class_bind_template_child (widget_class, GrEditPage, unit_list);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, amount_search_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, amount_search_button_label);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, amount_search_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, add_step_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, link_image_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, timer_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, prev_step_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, next_step_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, image_popover);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, image_flowbox);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, timer_popover);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, temperature_popover);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, temperature_spin);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, celsius_button);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, timer_spin);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, preview_stack);
        gtk_widget_class_bind_template_child (widget_class, GrEditPage, cooking_view);

        gtk_widget_class_bind_template_callback (widget_class, dismiss_error);
        gtk_widget_class_bind_template_callback (widget_class, add_image_cb);
        gtk_widget_class_bind_template_callback (widget_class, remove_image_cb);
        gtk_widget_class_bind_template_callback (widget_class, rotate_image_left_cb);
        gtk_widget_class_bind_template_callback (widget_class, rotate_image_right_cb);
        gtk_widget_class_bind_template_callback (widget_class, images_changed);
        gtk_widget_class_bind_template_callback (widget_class, index_changed);
        gtk_widget_class_bind_template_callback (widget_class, add_ingredient2);
        gtk_widget_class_bind_template_callback (widget_class, remove_ingredient);
        gtk_widget_class_bind_template_callback (widget_class, ingredient_changed);

        gtk_widget_class_bind_template_callback (widget_class, ing_filter_changed);
        gtk_widget_class_bind_template_callback (widget_class, ing_filter_stop);
        gtk_widget_class_bind_template_callback (widget_class, ing_filter_activated);
        gtk_widget_class_bind_template_callback (widget_class, ing_search_button_clicked);

        gtk_widget_class_bind_template_callback (widget_class, unit_filter_changed);
        gtk_widget_class_bind_template_callback (widget_class, unit_filter_stop);
        gtk_widget_class_bind_template_callback (widget_class, unit_filter_activated);
        gtk_widget_class_bind_template_callback (widget_class, amount_search_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, popover_keypress_handler);
        gtk_widget_class_bind_template_callback (widget_class, add_image_link);
        gtk_widget_class_bind_template_callback (widget_class, add_timer);
        gtk_widget_class_bind_template_callback (widget_class, add_step);
        gtk_widget_class_bind_template_callback (widget_class, add_temperature);
        gtk_widget_class_bind_template_callback (widget_class, image_activated);

        gtk_widget_class_bind_template_callback (widget_class, set_default_image_cb);
        gtk_widget_class_bind_template_callback (widget_class, edit_chef);

        gtk_widget_class_bind_template_callback (widget_class, time_spin_input);
        gtk_widget_class_bind_template_callback (widget_class, time_spin_output);
        gtk_widget_class_bind_template_callback (widget_class, time_spin_activate);
        gtk_widget_class_bind_template_callback (widget_class, temperature_spin_activate);
        gtk_widget_class_bind_template_callback (widget_class, preview_visible_changed);
        gtk_widget_class_bind_template_callback (widget_class, prev_step);
        gtk_widget_class_bind_template_callback (widget_class, next_step);
}

GtkWidget *
gr_edit_page_new (void)
{
        GrEditPage *page;

        page = g_object_new (GR_TYPE_EDIT_PAGE, NULL);

        return GTK_WIDGET (page);
}

static char *
get_combo_value (GtkComboBox *combo)
{
        const char *id;
        char *value;

        id = gtk_combo_box_get_active_id (combo);
        if (id)
                value = g_strdup (id);
        else
                value = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));

        return value;
}

static void
set_combo_value (GtkComboBox *combo,
                 const char  *value)
{
        if (!gtk_combo_box_set_active_id (combo, value)) {
                GtkWidget *entry;

                entry = gtk_bin_get_child (GTK_BIN (combo));
                gtk_entry_set_text (GTK_ENTRY (entry), value);
        }
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

static void
set_text_view_text (GtkTextView *textview,
                    const char  *text)
{
        GtkTextBuffer *buffer;

        buffer = gtk_text_view_get_buffer (textview);
        gtk_text_buffer_set_text (buffer, text ? text : "", -1);
}

static void
set_spiciness (GrEditPage *page,
               int         spiciness)
{
        if (spiciness < 25)
                gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->spiciness_combo), "mild");
        else if (spiciness < 50)
                gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->spiciness_combo), "spicy");
        else if (spiciness < 75)
                gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->spiciness_combo), "hot");
        else
                gtk_combo_box_set_active_id (GTK_COMBO_BOX (page->spiciness_combo), "extreme");
}

static int
get_spiciness (GrEditPage *page)
{
        const char *s;

        s = gtk_combo_box_get_active_id (GTK_COMBO_BOX (page->spiciness_combo));

        if (g_strcmp0 (s, "mild") == 0)
                return 15;
        else if (g_strcmp0 (s, "spicy") == 0)
                return 40;
        else if (g_strcmp0 (s, "hot") == 0)
                return 65;
        else if (g_strcmp0 (s, "extreme") == 0)
                return 90;
        else
                return 0;
}

static void add_list    (GtkButton *button, GrEditPage *page);
static void remove_list (GtkButton *button, GrEditPage *page);
static void update_segments (GrEditPage *page);

static GtkWidget *
add_ingredients_segment (GrEditPage *page,
                         const char *segment_label)
{
        GtkWidget *segment;
        GtkWidget *label;
        GtkWidget *entry;
        GtkWidget *list;
        GtkWidget *box;
        GtkWidget *button;
        GtkWidget *stack;
        GtkWidget *image;

        segment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_margin_top (segment, 20);
        gtk_widget_show (segment);
        gtk_container_add (GTK_CONTAINER (page->ingredients_box), segment);

        stack = gtk_stack_new ();
        g_object_set_data (G_OBJECT (segment), "stack", stack);
        gtk_widget_show (stack);
        gtk_container_add (GTK_CONTAINER (segment), stack);

        label = g_object_new (GTK_TYPE_LABEL,
                              "label", segment_label[0] ? segment_label : _("Ingredients"),
                              "xalign", 0.0,
                              "visible", TRUE,
                              NULL);
        gtk_style_context_add_class (gtk_widget_get_style_context (label), "heading");
        gtk_stack_add_named (GTK_STACK (stack), label, "label");

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_valign (box, GTK_ALIGN_START);
        gtk_widget_show (box);

        entry = gtk_entry_new ();
        g_object_set_data (G_OBJECT (segment), "entry", entry);
        gtk_widget_set_halign (box, GTK_ALIGN_FILL);
        gtk_widget_show (entry);
        gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Name of the List"));
        gtk_entry_set_text (GTK_ENTRY (entry), segment_label[0] ? segment_label : "");

#if defined(ENABLE_GSPELL) && defined(GSPELL_TYPE_ENTRY)
        {
                GspellEntry *gspell_entry;

                gspell_entry = gspell_entry_get_from_gtk_entry (GTK_ENTRY (entry));
                gspell_entry_basic_setup (gspell_entry);
        }
#endif

        gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);

        button = gtk_button_new_with_label (_("Remove"));
        g_object_set_data (G_OBJECT (button), "segment", segment);
        g_object_set_data (G_OBJECT (segment), "remove-list", button);
        g_signal_connect (button, "clicked", G_CALLBACK (remove_list), page);
        gtk_widget_show (button);
        gtk_container_add (GTK_CONTAINER (box), button);

        gtk_stack_add_named (GTK_STACK (stack), box, "entry");

        list = gtk_list_box_new ();
        g_object_set_data (G_OBJECT (segment), "list", list);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_SINGLE);
        g_signal_connect (list, "selected-rows-changed", G_CALLBACK (selected_rows_changed), page);

        gtk_widget_show (list);
        gtk_style_context_add_class (gtk_widget_get_style_context (list), "frame");
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), all_headers, NULL, NULL);

        label = g_object_new (GTK_TYPE_LABEL,
                              "label", _("No ingredients added yet"),
                              "xalign", 0.5,
                              "halign", GTK_ALIGN_FILL,
                              "margin-start", 20,
                              "margin-end", 20,
                              "margin-top", 10,
                              "margin-bottom", 10,
                              "visible", TRUE,
                              NULL);
        gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
        gtk_list_box_set_placeholder (GTK_LIST_BOX (list), label);

        gtk_container_add (GTK_CONTAINER (segment), list);

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_halign (box, GTK_ALIGN_START);
        gtk_widget_show (box);

        button = gtk_button_new ();
        gtk_widget_set_tooltip_text (button, _("Add Ingredient"));
        image = gtk_image_new_from_icon_name ("list-add-symbolic", 1);
        gtk_widget_show (image);
        gtk_widget_set_margin_top (button, 10);
        gtk_container_add (GTK_CONTAINER (button), image);
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "dim-label");
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
        gtk_style_context_remove_class (gtk_widget_get_style_context (button), "text-button");
        g_signal_connect (button, "clicked", G_CALLBACK (add_ingredient), page);
        gtk_widget_show (button);
        gtk_container_add (GTK_CONTAINER (box), button);
        g_object_set_data (G_OBJECT (button), "list", list);

        gtk_container_add (GTK_CONTAINER (segment), box);

        page->segments = g_list_append (page->segments, segment);

        return list;
}

static void
add_list (GtkButton *button, GrEditPage *page)
{
        add_ingredients_segment (page, "");

        update_segments (page);
        set_active_row (page, NULL);
}

static void
remove_list (GtkButton *button, GrEditPage *page)
{
        GtkWidget *segment;

        segment = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "segment"));

        page->segments = g_list_remove (page->segments, segment);
        gtk_widget_destroy (segment);

        update_segments (page);
        set_active_row (page, NULL);
}

static void
update_segments (GrEditPage *page)
{
        GList *l;
        GtkWidget *segment;
        GtkWidget *stack;
        GtkWidget *button;

        for (l = page->segments; l; l = l->next) {
                segment = l->data;
                stack = GTK_WIDGET (g_object_get_data (G_OBJECT (segment), "stack"));
                button = GTK_WIDGET (g_object_get_data (G_OBJECT (segment), "remove-list"));
                if (page->segments->next == NULL)
                        gtk_stack_set_visible_child_name (GTK_STACK (stack), "label");
                else {
                        gtk_stack_set_visible_child_name (GTK_STACK (stack), "entry");
                        gtk_widget_set_visible (button, l != page->segments);
                }
        }
}

static void
populate_ingredients (GrEditPage *page,
                      const char *text)
{
        g_autoptr(GrIngredientsList) ingredients = NULL;
        g_autofree char **segs = NULL;
        g_auto(GStrv) ings = NULL;
        int i, j;
        GtkWidget *list;
        GtkWidget *button;

        container_remove_all (GTK_CONTAINER (page->ingredients_box));
        g_list_free (page->segments);
        page->segments = NULL;
        page->active_row = NULL;

        ingredients = gr_ingredients_list_new (text);
        segs = gr_ingredients_list_get_segments (ingredients);
        for (j = 0; segs[j]; j++) {
                list = add_ingredients_segment (page, segs[j]);
                ings = gr_ingredients_list_get_ingredients (ingredients, segs[j]);
                for (i = 0; ings[i]; i++) {
                        g_autofree char *s = NULL;
                        g_auto(GStrv) strv = NULL;
                        const char *amount;
                        const char *unit;
                        GtkWidget *row;

                        s = gr_ingredients_list_scale_unit (ingredients, segs[j], ings[i], 1, 1);
                        strv = g_strsplit (s, " ", 2);
                        amount = strv[0];
                        unit = strv[1] ? strv[1] : "";
                        row = add_ingredient_row (page, list, page->group);
                        update_ingredient_row (row, amount, unit, ings[i]);
                }
        }

        button = gtk_button_new_with_label (_("Add List"));
        gtk_widget_show (button);
        gtk_widget_set_halign (button, GTK_ALIGN_FILL);
        gtk_widget_set_margin_top (button, 20);
        gtk_box_pack_end (GTK_BOX (page->ingredients_box), button, FALSE, TRUE, 0);
        g_signal_connect (button, "clicked", G_CALLBACK (add_list), page);

        update_segments (page);
}

void
gr_edit_page_clear (GrEditPage *page)
{
        GArray *images;
        GrRecipeStore *store;

        gr_image_viewer_revert_changes (GR_IMAGE_VIEWER (page->images));

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        gtk_label_set_label (GTK_LABEL (page->name_label), _("Name Your Recipe"));
        gtk_entry_set_text (GTK_ENTRY (page->name_entry), "");
        set_combo_value (GTK_COMBO_BOX (page->cuisine_combo), "");
        set_combo_value (GTK_COMBO_BOX (page->category_combo), "");
        set_combo_value (GTK_COMBO_BOX (page->season_combo), "");
        set_combo_value (GTK_COMBO_BOX (page->prep_time_combo), "");
        set_combo_value (GTK_COMBO_BOX (page->cook_time_combo), "");
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->serves_spin), 1);
        set_spiciness (page, 0);
        populate_ingredients (page, "");
        set_text_view_text (GTK_TEXT_VIEW (page->description_field), "");
        set_text_view_text (GTK_TEXT_VIEW (page->instructions_field), "");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->gluten_free_check), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->nut_free_check), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegan_check), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegetarian_check), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->milk_free_check), FALSE);
        gtk_widget_hide (page->author_label);

        images = gr_image_array_new ();
        g_object_set (page->images, "images", images, NULL);
        g_array_unref (images);

        if (page->index_handler_id) {
                g_signal_handler_disconnect (page->recipe, page->index_handler_id);
                page->index_handler_id = 0;
        }

        g_clear_object (&page->recipe);

        g_free (page->author);
        page->author = g_strdup (gr_recipe_store_get_user_key (store));
}

static void
set_instructions (GtkTextView *text_view,
                  const char  *text)
{
        GtkTextBuffer *buffer;
        GtkTextIter iter;
        GtkTextTag *tag;
        const char *p;
        const char *p1, *p2, *q1, *q2, *r1, *r2;
        GdkRGBA color;

        gdk_rgba_parse (&color, "blue");
        buffer = gtk_text_view_get_buffer (text_view);
        gtk_text_buffer_set_text (buffer, "", -1);

        p = text;
        while (*p) {
                g_autofree char *recipe_id = NULL;
                g_autofree char *url = NULL;
                int image_idx;

                q1 = NULL;
                r1 = NULL;
                p1 = strstr (p, "<a href=\"");
                if (!p1)
                        break;
                p2 = p1 + strlen ("<a href=\"");
                if (strncmp (p2, "recipe:", strlen ("recipe:")) == 0) {
                        p2 = p2 + strlen ("recipe:");
                        q1 = strstr (p2, "\">");
                        recipe_id = g_strndup (p2, q1 - p2);
                        url = g_strconcat ("recipe:", recipe_id, NULL);
                }
                else if (strncmp (p2, "image:", strlen ("image:")) == 0) {
                        p2 = p2 + strlen ("image:");
                        q1 = strstr (p2, "\">");
                        image_idx = (int)g_ascii_strtoll (p2, NULL, 10);
                        url = g_strdup_printf ("image:%d", image_idx);
                }
                else {
                        p = p2;
                        continue;
                }

                if (!q1)
                        break;

                q2 = q1 + strlen ("\">");
                r1 = strstr (q2, "</a>");

                if (!r1)
                        break;

                r2 = r1 + strlen ("</a>");

                gtk_text_buffer_get_end_iter (buffer, &iter);
                gtk_text_buffer_insert (buffer, &iter, p, p1 - p);

                tag = gtk_text_buffer_create_tag (buffer, NULL,
                                                  "foreground-rgba", &color,
                                                  "underline", PANGO_UNDERLINE_SINGLE,
                                                  NULL);
                g_object_set_data_full (G_OBJECT (tag), "href", g_strdup (url), g_free);

                gtk_text_buffer_get_end_iter (buffer, &iter);
                gtk_text_buffer_insert_with_tags (buffer, &iter, q2, r1 - q2, tag, NULL);

                p = r2;
        }

        gtk_text_buffer_get_end_iter (buffer, &iter);
        gtk_text_buffer_insert (buffer, &iter, p, -1);
}

static void
update_author_label (GrEditPage *page,
                     GrChef     *chef)
{
        if (chef) {
                g_autofree char *tmp = NULL;
                g_autofree char *link = NULL;

                link = g_strdup_printf ("<a href=\"edit\">%s</a>", gr_chef_get_name (chef));
                tmp = g_strdup_printf (_("Recipe by %s"), link);
                gtk_label_set_label (GTK_LABEL (page->author_label), tmp);
                gtk_widget_show (page->author_label);
        }
        else {
                gtk_widget_hide (page->author_label);
        }
}

void
gr_edit_page_edit (GrEditPage *page,
                   GrRecipe   *recipe)
{
        const char *name;
        const char *cuisine;
        const char *category;
        const char *season;
        const char *prep_time;
        const char *cook_time;
        const char *author;
        int serves;
        int spiciness;
        const char *description;
        const char *instructions;
        const char *ingredients;
        GrDiets diets;
        int index;
        g_autoptr(GArray) images = NULL;
        g_autoptr(GrChef) chef = NULL;
        GrRecipeStore *store;

        gr_image_viewer_revert_changes (GR_IMAGE_VIEWER (page->images));

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        name = gr_recipe_get_name (recipe);
        serves = gr_recipe_get_serves (recipe);
        spiciness = gr_recipe_get_spiciness (recipe);
        cuisine = gr_recipe_get_cuisine (recipe);
        category = gr_recipe_get_category (recipe);
        season = gr_recipe_get_season (recipe);
        prep_time = gr_recipe_get_prep_time (recipe);
        cook_time = gr_recipe_get_cook_time (recipe);
        diets = gr_recipe_get_diets (recipe);
        description = gr_recipe_get_description (recipe);
        instructions = gr_recipe_get_instructions (recipe);
        ingredients = gr_recipe_get_ingredients (recipe);
        author = gr_recipe_get_author (recipe);
        index = gr_recipe_get_default_image (recipe);

        g_object_get (recipe, "images", &images, NULL);

        page->author = g_strdup (author);

        chef = gr_recipe_store_get_chef (store, author ? author : "");

        gtk_label_set_label (GTK_LABEL (page->name_label), _("Name"));

        gtk_entry_set_text (GTK_ENTRY (page->name_entry), name);
        set_combo_value (GTK_COMBO_BOX (page->cuisine_combo), cuisine);
        set_combo_value (GTK_COMBO_BOX (page->category_combo), category);
        set_combo_value (GTK_COMBO_BOX (page->season_combo), season);
        set_combo_value (GTK_COMBO_BOX (page->prep_time_combo), prep_time);
        set_combo_value (GTK_COMBO_BOX (page->cook_time_combo), cook_time);
        set_spiciness (page, spiciness);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->serves_spin), serves);
        set_text_view_text (GTK_TEXT_VIEW (page->description_field), description);
        set_instructions (GTK_TEXT_VIEW (page->instructions_field), instructions);
        gtk_stack_set_visible_child_name (GTK_STACK (page->preview_stack), "edit");

        populate_ingredients (page, ingredients);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->gluten_free_check), (diets & GR_DIET_GLUTEN_FREE) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->nut_free_check), (diets & GR_DIET_NUT_FREE) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegan_check), (diets & GR_DIET_VEGAN) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegetarian_check), (diets & GR_DIET_VEGETARIAN) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->milk_free_check), (diets & GR_DIET_MILK_FREE) != 0);

        gr_image_viewer_set_images (GR_IMAGE_VIEWER (page->images), images, index);

        update_author_label (page, chef);

        if (page->index_handler_id) {
                g_signal_handler_disconnect (page->recipe, page->index_handler_id);
                page->index_handler_id = 0;
        }

        g_set_object (&page->recipe, recipe);

        if (recipe) {
                page->index_handler_id = g_signal_connect_swapped (recipe, "notify::default-image",
                                                                   G_CALLBACK (update_default_image_button), page);
        }

        update_default_image_button (page);
}

gboolean
gr_edit_page_save (GrEditPage *page)
{
        const char *name;
        g_autofree char *cuisine = NULL;
        g_autofree char *category = NULL;
        g_autofree char *season = NULL;
        g_autofree char *prep_time = NULL;
        g_autofree char *cook_time = NULL;
        int serves;
        int spiciness;
        g_autofree char *description = NULL;
        g_autofree char *ingredients = NULL;
        g_autofree char *instructions = NULL;
        GrRecipeStore *store;
        g_autoptr(GError) error = NULL;
        gboolean ret = TRUE;
        GrDiets diets;
        g_autoptr(GArray) images = NULL;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        name = gtk_entry_get_text (GTK_ENTRY (page->name_entry));
        cuisine = get_combo_value (GTK_COMBO_BOX (page->cuisine_combo));
        category = get_combo_value (GTK_COMBO_BOX (page->category_combo));
        season = get_combo_value (GTK_COMBO_BOX (page->season_combo));
        prep_time = get_combo_value (GTK_COMBO_BOX (page->prep_time_combo));
        cook_time = get_combo_value (GTK_COMBO_BOX (page->cook_time_combo));
        spiciness = get_spiciness (page);
        serves = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page->serves_spin));
        ingredients = collect_ingredients (page);
        description = get_text_view_text (GTK_TEXT_VIEW (page->description_field));
        instructions = get_text_view_text (GTK_TEXT_VIEW (page->instructions_field));
        diets = (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->gluten_free_check)) ? GR_DIET_GLUTEN_FREE : 0) |
                (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->nut_free_check)) ? GR_DIET_NUT_FREE : 0) |
                (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->vegan_check)) ? GR_DIET_VEGAN : 0) |
               (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->vegetarian_check)) ? GR_DIET_VEGETARIAN : 0) |
                (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->milk_free_check)) ? GR_DIET_MILK_FREE : 0);

        g_object_get (page->images, "images", &images, NULL);

        if (page->recipe) {
                g_autofree char *old_id = NULL;
                g_autofree char *id = NULL;

                id = generate_id ("R_", name, "_by_", page->author, NULL);
                old_id = g_strdup (gr_recipe_get_id (page->recipe));
                g_object_set (page->recipe,
                              "id", id,
                              "author", page->author,
                              "name", name,
                              "cuisine", cuisine,
                              "category", category,
                              "season", season,
                              "prep-time", prep_time,
                              "cook-time", cook_time,
                              "serves", serves,
                              "spiciness", spiciness,
                              "description", description,
                              "ingredients", ingredients,
                              "instructions", instructions,
                              "diets", diets,
                              "images", images,
                              NULL);
                ret = gr_recipe_store_update_recipe (store, page->recipe, old_id, &error);
        }
        else if (name == NULL || name[0] == '\0') {
                g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("You need to provide a name for the recipe"));
                ret = FALSE;
        }
        else {
                g_autoptr(GrRecipe) recipe = NULL;
                g_autofree char *id = NULL;
                GtkWidget *window;

                window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                gr_ensure_user_chef (GTK_WINDOW (window), NULL, NULL);
                id = generate_id ("R_", name, "_by_", page->author, NULL);
                recipe = g_object_new (GR_TYPE_RECIPE,
                                       "id", id,
                                       "author", page->author,
                                       "name", name,
                                       "cuisine", cuisine,
                                       "category", category,
                                       "season", season,
                                       "prep-time", prep_time,
                                       "cook-time", cook_time,
                                       "serves", serves,
                                       "spiciness", spiciness,
                                       "description", description,
                                       "ingredients", ingredients,
                                       "instructions", instructions,
                                       "diets", diets,
                                       "images", images,
                                       NULL);
                ret = gr_recipe_store_add_recipe (store, recipe, &error);
                g_set_object (&page->recipe, recipe);
        }

        if (ret) {
                gr_image_viewer_persist_changes (GR_IMAGE_VIEWER (page->images));
                gr_recipe_tile_recreate_css ();

                return TRUE;
        }

        gr_image_viewer_revert_changes (GR_IMAGE_VIEWER (page->images));

        gtk_label_set_label (GTK_LABEL (page->error_label), error->message);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->error_revealer), TRUE);

        if (page->index_handler_id) {
                g_signal_handler_disconnect (page->recipe, page->index_handler_id);
                page->index_handler_id = 0;
        }

        return FALSE;
}

GrRecipe *
gr_edit_page_get_recipe (GrEditPage *page)
{
        return page->recipe;
}
