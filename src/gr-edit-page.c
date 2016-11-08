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
 * GNU General Public License for more edit.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include "gr-category.h"
#include "gr-images.h"


struct _GrEditPage
{
        GtkBox parent_instance;

        char *image_path;
        gboolean fail_if_exists;
        char *old_name;
        char *author;

        GtkWidget *error_revealer;
        GtkWidget *error_label;
        GtkWidget *name_entry;
        GtkWidget *description_entry;
        GtkWidget *cuisine_combo;
        GtkWidget *category_combo;
        GtkWidget *prep_time_combo;
        GtkWidget *cook_time_combo;
        GtkWidget *ingredients_field;
        GtkWidget *instructions_field;
        GtkWidget *notes_field;
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
        g_auto(GStrv) images = NULL;
        int length;

        g_object_get (page->images, "images", &images, NULL);
        length = g_strv_length (images);
        gtk_widget_set_sensitive (page->add_image_button, length < 4);
        gtk_widget_set_sensitive (page->remove_image_button, length > 0);
        gtk_widget_set_sensitive (page->rotate_image_left_button, length > 0);
        gtk_widget_set_sensitive (page->rotate_image_right_button, length > 0);
}

static void
add_image (GrEditPage *page)
{
        gr_images_add_image (GR_IMAGES (page->images));
}

static void
remove_image (GrEditPage *page)
{
        gr_images_remove_image (GR_IMAGES (page->images));
}

static void
rotate_image_left (GrEditPage *page)
{
        gr_images_rotate_image (GR_IMAGES (page->images), 90);
}

static void
rotate_image_right (GrEditPage *page)
{
        gr_images_rotate_image (GR_IMAGES (page->images), 270);
}

static void
edit_page_finalize (GObject *object)
{
        GrEditPage *self = GR_EDIT_PAGE (object);

        g_free (self->old_name);
        g_free (self->author);
        g_free (self->image_path);

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
                gr_cuisine_get_data (names[i], &title, NULL);
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

        names = gr_category_get_names (&length);
        for (i = 0; i < length; i++) {
                gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (page->category_combo),
                                           names[i],
                                           gr_category_get_title (names[i]));
        }
}

static void
ingredients_changed (GrEditPage *page)
{
        GtkWidget *sw;
        GtkStyleContext *context;

        sw = gtk_widget_get_ancestor (page->ingredients_field, GTK_TYPE_SCROLLED_WINDOW);
        context = gtk_widget_get_style_context (sw);
        gtk_style_context_remove_class (context, "error");
}

static void
connect_ingredients_signals (GrEditPage *page)
{
        GtkTextBuffer *buffer;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->ingredients_field));
        g_signal_connect_swapped (buffer, "changed", G_CALLBACK (ingredients_changed), page);
}

static void
gr_edit_page_init (GrEditPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));

        populate_cuisine_combo (page);
        populate_category_combo (page);

        connect_ingredients_signals (page);
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
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, name_entry);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, description_entry);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, cuisine_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, category_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, prep_time_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, cook_time_combo);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, serves_spin);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, ingredients_field);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, instructions_field);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrEditPage, notes_field);
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

        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), dismiss_error);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), add_image);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), remove_image);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), rotate_image_left);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), rotate_image_right);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), images_changed);
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

void
gr_edit_page_clear (GrEditPage *page)
{
        GtkTextBuffer *buffer;
        GrRecipeStore *store;
        const char *paths[1] = { NULL };

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        gtk_entry_set_text (GTK_ENTRY (page->name_entry), "");
        gtk_entry_set_text (GTK_ENTRY (page->description_entry), "");
        set_combo_value (GTK_COMBO_BOX (page->cuisine_combo), "");
        set_combo_value (GTK_COMBO_BOX (page->category_combo), "");
        set_combo_value (GTK_COMBO_BOX (page->prep_time_combo), "");
        set_combo_value (GTK_COMBO_BOX (page->cook_time_combo), "");
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->serves_spin), 1);
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->ingredients_field));
        gtk_text_buffer_set_text (buffer, "", -1);
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->instructions_field));
        gtk_text_buffer_set_text (buffer, "", -1);
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (page->notes_field));
        gtk_text_buffer_set_text (buffer, "", -1);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->gluten_free_check), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->nut_free_check), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegan_check), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegetarian_check), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->milk_free_check), FALSE);

	g_clear_pointer (&page->image_path, g_free);
        g_object_set (page->images, "images", paths, NULL);

	g_free (page->author);
	page->author = g_strdup (gr_recipe_store_get_user_key (store));

        page->fail_if_exists = TRUE;
}

void
gr_edit_page_edit (GrEditPage *page,
                   GrRecipe   *recipe)
{
        GtkTextBuffer *buffer;
        g_autofree char *name = NULL;
        g_autofree char *description = NULL;
        g_autofree char *cuisine = NULL;
        g_autofree char *category = NULL;
        g_autofree char *prep_time = NULL;
        g_autofree char *cook_time = NULL;
        int serves;
        g_autofree char *ingredients = NULL;
        g_autofree char *instructions = NULL;
        g_autofree char *notes = NULL;
        g_autofree char *image_path = NULL;
        g_autofree char *author = NULL;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        GrDiets diets;

        g_object_get (recipe,
                      "name", &name,
                      "description", &description,
                      "cuisine", &cuisine,
                      "category", &category,
                      "prep-time", &prep_time,
                      "cook-time", &cook_time,
                      "serves", &serves,
                      "ingredients", &ingredients,
                      "instructions", &instructions,
                      "notes", &notes,
                      "diets", &diets,
                      "image-path", &image_path,
                      "author", &author,
                      NULL);

        gtk_entry_set_text (GTK_ENTRY (page->name_entry), name);
        gtk_entry_set_text (GTK_ENTRY (page->description_entry), description);
        set_combo_value (GTK_COMBO_BOX (page->cuisine_combo), cuisine);
        set_combo_value (GTK_COMBO_BOX (page->category_combo), category);
        set_combo_value (GTK_COMBO_BOX (page->prep_time_combo), prep_time);
        set_combo_value (GTK_COMBO_BOX (page->cook_time_combo), cook_time);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->serves_spin), serves);

        set_text_view_text (GTK_TEXT_VIEW (page->ingredients_field), ingredients);
        set_text_view_text (GTK_TEXT_VIEW (page->instructions_field), instructions);
        set_text_view_text (GTK_TEXT_VIEW (page->notes_field), notes);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->gluten_free_check), (diets & GR_DIET_GLUTEN_FREE) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->nut_free_check), (diets & GR_DIET_NUT_FREE) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegan_check), (diets & GR_DIET_VEGAN) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->vegetarian_check), (diets & GR_DIET_VEGETARIAN) != 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->milk_free_check), (diets & GR_DIET_MILK_FREE) != 0);

	g_free (page->image_path);
        page->image_path = g_strdup (image_path);
	//update_image (page);

        page->fail_if_exists = FALSE;
        g_free (page->old_name);
        page->old_name = g_strdup (name);

	g_free (page->author);
	page->author = g_strdup (author);
}

static gboolean validate_ingredients (GrEditPage  *page,
                                      const char  *ingredients,
                                      GError     **error);

gboolean
gr_edit_page_save (GrEditPage *page)
{
        const char *name;
        const char *description;
        g_autofree char *cuisine = NULL;
        g_autofree char *category = NULL;
        g_autofree char *prep_time = NULL;
        g_autofree char *cook_time = NULL;
        int serves;
        g_autofree char *ingredients = NULL;
        g_autofree char *instructions = NULL;
        g_autofree char *notes = NULL;
        GrRecipeStore *store;
        g_autoptr(GrRecipe) recipe = NULL;
        g_autoptr(GError) error = NULL;
        gboolean ret;
        GrDiets diets;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        name = gtk_entry_get_text (GTK_ENTRY (page->name_entry));
        description = gtk_entry_get_text (GTK_ENTRY (page->description_entry));
        cuisine = get_combo_value (GTK_COMBO_BOX (page->cuisine_combo));
        category = get_combo_value (GTK_COMBO_BOX (page->category_combo));
        prep_time = get_combo_value (GTK_COMBO_BOX (page->prep_time_combo));
        cook_time = get_combo_value (GTK_COMBO_BOX (page->cook_time_combo));
        serves = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page->serves_spin));
        ingredients = get_text_view_text (GTK_TEXT_VIEW (page->ingredients_field));
        instructions = get_text_view_text (GTK_TEXT_VIEW (page->instructions_field));
        notes = get_text_view_text (GTK_TEXT_VIEW (page->notes_field));
        diets = (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->gluten_free_check)) ? GR_DIET_GLUTEN_FREE : 0) |
                (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->nut_free_check)) ? GR_DIET_NUT_FREE : 0) |
                (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->vegan_check)) ? GR_DIET_VEGAN : 0) |
               (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->vegetarian_check)) ? GR_DIET_VEGETARIAN : 0) |
                (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->milk_free_check)) ? GR_DIET_MILK_FREE : 0);

        if (!validate_ingredients (page, ingredients, &error))
                goto error;

        recipe = g_object_new (GR_TYPE_RECIPE,
                               "name", name,
                               "description", description,
			       "author", page->author,
                               "cuisine", cuisine,
                               "category", category,
                               "prep-time", prep_time,
                               "cook-time", cook_time,
                               "serves", serves,
                               "ingredients", ingredients,
                               "instructions", instructions,
                               "notes", notes,
                               "diets", diets,
                               "image-path", page->image_path,
                               NULL);

        if (page->fail_if_exists)
                ret = gr_recipe_store_add (store, recipe, &error);
        else
                ret = gr_recipe_store_update (store, recipe, page->old_name, &error);

        if (!ret)
                goto error;

        g_free (page->old_name);
        page->old_name = NULL;

        return TRUE;

error:
        gtk_label_set_label (GTK_LABEL (page->error_label), error->message);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->error_revealer), TRUE);

        return FALSE;
}

static gboolean
validate_ingredients (GrEditPage  *page,
                      const char  *ingredients,
                      GError     **error)
{
        if (!gr_ingredients_list_validate (ingredients, error)) {
                GtkWidget *sw;
                GtkStyleContext *context;

                sw = gtk_widget_get_ancestor (page->ingredients_field, GTK_TYPE_SCROLLED_WINDOW);
                context = gtk_widget_get_style_context (sw);
                gtk_style_context_add_class (context, "error");

                return FALSE;
        }
        else {
                return TRUE;
        }
}
