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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

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

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif


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
        GtkWidget *rotate_image_right_button;
        GtkWidget *rotate_image_left_button;
        GtkWidget *ingredient_popover;
        GtkWidget *new_ingredient_name;
        GtkWidget *new_ingredient_amount;
        GtkWidget *new_ingredient_unit;
        GtkWidget *new_ingredient_add_button;
        GtkWidget *author_label;
        GtkWidget *ingredients_box;

        GtkSizeGroup *group;

        guint account_response_signal_id;

        GList *segments;
        GtkWidget *active_row;
};

G_DEFINE_TYPE (GrEditPage, gr_edit_page, GTK_TYPE_BOX)

static void
dismiss_error (GrEditPage *page)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->error_revealer), FALSE);
}

static void
images_changed (GrEditPage *page)
{
        g_autoptr(GArray) images = NULL;
        int length;

        g_object_get (page->images, "images", &images, NULL);
        length = images->len;
        gtk_widget_set_sensitive (page->add_image_button, length < 4);
        gtk_widget_set_sensitive (page->remove_image_button, length > 0);
        gtk_widget_set_sensitive (page->rotate_image_left_button, length > 0);
        gtk_widget_set_sensitive (page->rotate_image_right_button, length > 0);
}

static void
add_image (GrEditPage *page)
{
        gr_image_viewer_add_image (GR_IMAGE_VIEWER (page->images));
}

static void
remove_image (GrEditPage *page)
{
        gr_image_viewer_remove_image (GR_IMAGE_VIEWER (page->images));
}

static void
rotate_image_left (GrEditPage *page)
{
        gr_image_viewer_rotate_image (GR_IMAGE_VIEWER (page->images), 90);
}

static void
rotate_image_right (GrEditPage *page)
{
        gr_image_viewer_rotate_image (GR_IMAGE_VIEWER (page->images), 270);
}

static void
edit_page_finalize (GObject *object)
{
        GrEditPage *self = GR_EDIT_PAGE (object);

        g_clear_object (&self->recipe);
        g_clear_object (&self->group);
        g_list_free (self->segments);

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

static void
add_ingredient (GtkButton *button, GrEditPage *page)
{
        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_name), "");
        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_amount), "");
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (page->new_ingredient_unit))), "");

        gtk_popover_set_relative_to (GTK_POPOVER (page->ingredient_popover), GTK_WIDGET (button));
        gtk_button_set_label (GTK_BUTTON (page->new_ingredient_add_button), _("Add"));
        ingredient_changed (page);
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

        amount = (const char *)g_object_get_data (G_OBJECT (row), "amount");
        unit = (const char *)g_object_get_data (G_OBJECT (row), "unit");
        ingredient = (const char *)g_object_get_data (G_OBJECT (row), "ingredient");

        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_name), ingredient);
        gtk_entry_set_text (GTK_ENTRY (page->new_ingredient_amount), amount);
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (page->new_ingredient_unit))), unit);

        gtk_popover_set_relative_to (GTK_POPOVER (page->ingredient_popover), GTK_WIDGET (row));
        gtk_button_set_label (GTK_BUTTON (page->new_ingredient_add_button), _("Apply"));
        ingredient_changed (page);
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

static void
ingredient_row_activated (GtkListBox    *list,
                          GtkListBoxRow *row,
                          GrEditPage    *page)
{
        GtkWidget *stack;

        if ((GtkWidget *)row == page->active_row)
                return;

        if (page->active_row) {
                stack = g_object_get_data (G_OBJECT (page->active_row), "buttons-stack");
                gtk_stack_set_visible_child_name (GTK_STACK (stack), "empty");
        }

        page->active_row = (GtkWidget *)row;

        if (page->active_row) {
                stack = g_object_get_data (G_OBJECT (page->active_row), "buttons-stack");
                gtk_stack_set_visible_child_name (GTK_STACK (stack), "buttons");
        }
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
        gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
        gtk_widget_show (stack);
        image = gtk_image_new ();
        gtk_widget_show (image);
        gtk_widget_set_opacity (image, 0);
        gtk_stack_add_named (GTK_STACK (stack), image, "empty");
        buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        g_object_set (buttons, "margin", 4, NULL);
        button = gtk_button_new ();
        g_signal_connect (button, "clicked", G_CALLBACK (edit_ingredient), page);
        image = gtk_image_new_from_icon_name ("document-edit-symbolic", 1);
        gtk_container_add (GTK_CONTAINER (button), image);
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "dim-label");
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "circular");
        gtk_container_add (GTK_CONTAINER (buttons), button);
        button = gtk_button_new ();
        g_signal_connect (button, "clicked", G_CALLBACK (remove_ingredient), page);
        image = gtk_image_new_from_icon_name ("user-trash-symbolic", 1);
        gtk_container_add (GTK_CONTAINER (button), image);
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "dim-label");
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

        g_signal_connect (list, "row-activated", G_CALLBACK (ingredient_row_activated), page);

        return row;
}

static void
add_ingredient2 (GtkButton *button, GrEditPage *page)
{
        const char *ingredient;
        double amount;
        const char *unit;
        g_autofree char *s = NULL;
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
        amount = gtk_spin_button_get_value (GTK_SPIN_BUTTON (page->new_ingredient_amount));
        unit = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (page->new_ingredient_unit))));
        s = g_strdup_printf ("%g", amount);
        update_ingredient_row (row, s, unit, ingredient);
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
                        if (s->len > 0)
                                g_string_append (s, "\n");
                        g_string_append (s, (const char *)g_object_get_data (G_OBJECT (row), "amount"));
                        g_string_append (s, " ");
                        g_string_append (s, (const char *)g_object_get_data (G_OBJECT (row), "unit"));
                        g_string_append (s, " ");
                        g_string_append (s, (const char *)g_object_get_data (G_OBJECT (row), "ingredient"));
                        if (page->segments->next != NULL) {
                                g_string_append (s, "\t");
                                g_string_append (s, gtk_entry_get_text (GTK_ENTRY (entry)));
                        }
                }
                g_list_free (children);
        }

        return g_string_free (s, FALSE);
}

static void
all_headers (GtkListBoxRow *row,
             GtkListBoxRow *before,
             gpointer       user_data)
{
        GtkWidget *header;

        header = gtk_list_box_row_get_header (row);
        if (header)
                return;

        header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_list_box_row_set_header (row, header);
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
}

static void
gr_edit_page_class_init (GrEditPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = edit_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-edit-page.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, error_revealer);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, error_label);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, name_label);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, name_entry);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, cuisine_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, category_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, season_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, prep_time_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, cook_time_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, serves_spin);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, spiciness_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, description_field);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, instructions_field);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, gluten_free_check);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, nut_free_check);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, vegan_check);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, vegetarian_check);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, milk_free_check);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, images);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, add_image_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, remove_image_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, rotate_image_left_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, rotate_image_right_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, author_label);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, ingredients_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, ingredient_popover);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, new_ingredient_name);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, new_ingredient_amount);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, new_ingredient_unit);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, new_ingredient_add_button);

        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), dismiss_error);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), add_image);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), remove_image);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), rotate_image_left);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), rotate_image_right);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), images_changed);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), add_ingredient2);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), remove_ingredient);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), ingredient_changed);
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

        if (strcmp (s, "mild") == 0)
                return 15;
        else if (strcmp (s, "spicy") == 0)
                return 40;
        else if (strcmp (s, "hot") == 0)
                return 65;
        else
                return 90;
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
        g_object_set (segment,
                      "margin-top", 20,
                      "margin-bottom", 20,
                      NULL);
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
        gtk_entry_set_text (GTK_ENTRY (entry), segment_label[0] ? segment_label : _("Ingredients for …"));

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

        gtk_widget_show (list);
        gtk_style_context_add_class (gtk_widget_get_style_context (list), "frame");
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
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
}

static void
remove_list (GtkButton *button, GrEditPage *page)
{
        GtkWidget *segment;

        segment = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "segment"));

        page->segments = g_list_remove (page->segments, segment);
        gtk_widget_destroy (segment);

        update_segments (page);
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
        gtk_box_pack_end (GTK_BOX (page->ingredients_box), button, FALSE, TRUE, 0);
        g_signal_connect (button, "clicked", G_CALLBACK (add_list), page);

        update_segments (page);
}

void
gr_edit_page_clear (GrEditPage *page)
{
        GArray *images;

        gtk_label_set_label (GTK_LABEL (page->name_label), _("Name your recipe"));
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

        images = gr_rotated_image_array_new ();
        g_object_set (page->images, "images", images, NULL);
        g_array_unref (images);

        g_clear_object (&page->recipe);
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
        g_autofree char *image_path = NULL;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        GrDiets diets;
        g_autoptr(GArray) images = NULL;
        g_autoptr(GrChef) chef = NULL;
        GrRecipeStore *store;

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

        g_object_get (recipe, "images", &images, NULL);

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
        set_text_view_text (GTK_TEXT_VIEW (page->instructions_field), instructions);

        populate_ingredients (page, ingredients);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->gluten_free_check), (diets & GR_DIET_GLUTEN_FREE) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->nut_free_check), (diets & GR_DIET_NUT_FREE) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegan_check), (diets & GR_DIET_VEGAN) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegetarian_check), (diets & GR_DIET_VEGETARIAN) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->milk_free_check), (diets & GR_DIET_MILK_FREE) != 0);

        g_object_set (page->images, "images", images, NULL);

        if (chef) {
                g_autofree char *tmp = NULL;
                tmp = g_strdup_printf (_("Recipe by %s"), gr_chef_get_name (chef));
                gtk_label_set_label (GTK_LABEL (page->author_label), tmp);
                gtk_widget_show (page->author_label);
        }
        else {
                gtk_widget_hide (page->author_label);
        }

        g_set_object (&page->recipe, recipe);
}

static void
account_response (GDBusConnection *connection,
                  const char *sender_name,
                  const char *object_path,
                  const char *interface_name,
                  const char *signal_name,
                  GVariant *parameters,
                  gpointer user_data)
{
        GrEditPage *page = user_data;
        guint32 response;
        GVariant *options;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GError) error = NULL;

        g_variant_get (parameters, "(u@a{sv})", &response, &options);

        if (response == 0) {
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                const char *id;
                const char *name;
                const char *uri;
                g_autofree char *path = NULL;
                GrRecipeStore *store;

                store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
                chef = gr_recipe_store_get_chef (store, gr_recipe_store_get_user_key (store));
                if (!chef)
                        chef = gr_chef_new ();

                g_variant_lookup (options, "id", "&s", &id);
                g_variant_lookup (options, "name", "&s", &name);
                g_variant_lookup (options, "image", "&s", &uri);

                g_object_set (chef, "id", id, "fullname", name, NULL);

                if (uri && uri[0]) {
                        g_autoptr(GFile) source = NULL;
                        g_autoptr(GFile) dest = NULL;
                        g_autofree char *orig_dest = NULL;
                        g_autofree char *destpath = NULL;
                        int i;

                        source = g_file_new_for_uri (uri);
                        orig_dest = g_build_filename (g_get_user_data_dir (), "recipes", id, NULL);
                        destpath = g_strdup (orig_dest);
                        for (i = 1; i < 10; i++) {
                                if (!g_file_test (destpath, G_FILE_TEST_EXISTS))
                                        break;
                                g_free (destpath);
                                destpath = g_strdup_printf ("%s%d", orig_dest, i);
                        }
                        dest = g_file_new_for_path (destpath);
                        if (!g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, &error))
                                return;
                        g_object_set (chef, "image-path", destpath, NULL);
                }

                if (!gr_recipe_store_update_user (store, chef, &error)) {
                        g_warning ("Failed to update chef for user: %s", error->message);
                }
        }

        if (page->account_response_signal_id != 0) {
                g_dbus_connection_signal_unsubscribe (connection,
                                                      page->account_response_signal_id);
                page->account_response_signal_id = 0;
        }
}

typedef void (*GtkWindowHandleExported)  (GtkWindow               *window,
                                          const char              *handle,
                                          gpointer                 user_data);


#ifdef GDK_WINDOWING_WAYLAND
typedef struct {
  GtkWindow *window;
  GtkWindowHandleExported callback;
  gpointer user_data;
} WaylandWindowHandleExportedData;

static void
wayland_window_handle_exported (GdkWindow  *window,
                                const char *wayland_handle_str,
                                gpointer    user_data)
{
  WaylandWindowHandleExportedData *data = user_data;
  char *handle_str;

  handle_str = g_strdup_printf ("wayland:%s", wayland_handle_str);
  data->callback (data->window, handle_str, data->user_data);
  g_free (handle_str);

  g_free (data);
}
#endif

static gboolean
gtk_window_export_handle (GtkWindow               *window,
                          GtkWindowHandleExported  callback,
                          gpointer                 user_data)
{

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
      GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
      char *handle_str;
      guint32 xid = (guint32) gdk_x11_window_get_xid (gdk_window);

      handle_str = g_strdup_printf ("x11:%x", xid);
      callback (window, handle_str, user_data);

      return TRUE;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
      GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
      WaylandWindowHandleExportedData *data;

      data = g_new0 (WaylandWindowHandleExportedData, 1);
      data->window = window;
      data->callback = callback;
      data->user_data = user_data;

      if (!gdk_wayland_window_export_handle (gdk_window,
                                             wayland_window_handle_exported,
                                             data,
                                             g_free))
        {
          g_free (data);
          return FALSE;
        }
      else
        {
          return TRUE;
        }
    }
#endif

  g_warning ("Couldn't export handle, unsupported windowing system");

  return FALSE;
}

static void
window_handle_exported (GtkWindow *window,
                        const char *handle_str,
                        gpointer user_data)
{
        GrEditPage *page = user_data;
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) ret = NULL;
        const char *handle;

        g_autoptr(GDBusConnection) bus = NULL;

        bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

        ret = g_dbus_connection_call_sync (bus,
                                           "org.freedesktop.portal.Desktop",
                                           "/org/freedesktop/portal/desktop",
                                           "org.freedesktop.portal.Account",
                                           "GetUserInformation",
                                           g_variant_new ("(s)", handle_str),
                                           G_VARIANT_TYPE ("(o)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           G_MAXINT,
                                           NULL,
                                           &error);

        if (!ret) {
                g_message ("Could not talk to Account portal: %s", error->message);
                return;
        }

        g_variant_get (ret, "(&o)", &handle);

        page->account_response_signal_id =
                g_dbus_connection_signal_subscribe (bus,
                                                    "org.freedesktop.portal.Desktop",
                                                    "org.freedesktop.portal.Request",
                                                    "Response",
                                                    handle,
                                                    NULL,
                                                    G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                    account_response,
                                                    page, NULL);
}

static void
ensure_user_chef (GrRecipeStore *store,
                  GrEditPage *page)
{
        GtkWidget *window;
        g_autoptr(GrChef) chef = NULL;

        chef = gr_recipe_store_get_chef (store, gr_recipe_store_get_user_key (store));
        if (chef)
                return;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_WINDOW);
        gtk_window_export_handle (GTK_WINDOW (window), window_handle_exported, page);
}

static char *
generate_id (const char *name,
             const char *author)
{
        char *s, *q;

        s = g_strconcat (name, "_by_", author, NULL);
        for (q = s; *q; q = g_utf8_find_next_char (q, NULL)) {
                if (*q == ']' || *q == '[' || g_ascii_iscntrl (*q))
                        *q = '_';
        }

        return s;
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
                const char *author;

                author = gr_recipe_get_author (page->recipe);
                id = generate_id (name, author);
                old_id = g_strdup (gr_recipe_get_id (page->recipe));
                g_object_set (page->recipe,
                              "id", id,
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
        else {
                g_autoptr(GrRecipe) recipe = NULL;
                const char *author;
                g_autofree char *id = NULL;

                author = gr_recipe_store_get_user_key (store);
                ensure_user_chef (store, page);
                id = generate_id (name, author);
                recipe = g_object_new (GR_TYPE_RECIPE,
                                       "id", id,
                                       "name", name,
                                       "author", author,
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
        }

        if (!ret)
                goto error;

        g_clear_object (&page->recipe);

        return TRUE;

error:
        gtk_label_set_label (GTK_LABEL (page->error_label), error->message);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->error_revealer), TRUE);

        g_clear_object (&page->recipe);

        return FALSE;
}
