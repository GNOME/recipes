/* gr-ingredients-page.c:
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

#include "gr-ingredients-page.h"
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-recipe-tile.h"
#include "gr-app.h"
#include "gr-utils.h"
#include "gr-ingredient-tile.h"
#include "gr-window.h"


typedef struct {
	char *name;
	GtkWidget *item;
	GtkWidget *label;
	GtkWidget *box;
	gboolean filled;
} Category;

static void
category_free (gpointer data)
{
	Category *category = data;

	g_free (category->name);
	g_free (category);
}

struct _GrIngredientsPage
{
        GtkBox parent_instance;

        GtkWidget *main_box;
	GtkWidget *letter_box;
	GtkWidget *scrolled_window;

	GHashTable *categories;
};

G_DEFINE_TYPE (GrIngredientsPage, gr_ingredients_page, GTK_TYPE_BOX)

static void ingredients_page_reload (GrIngredientsPage *page);
static void connect_store_signals (GrIngredientsPage *page);

static void
row_activated (GrIngredientsPage *page, GtkListBoxRow *row)
{
        GtkWidget *item;
	int i;
	GtkAdjustment *adj;
	GtkAllocation alloc;
	Category *category = NULL;

        item = gtk_bin_get_child (GTK_BIN (row));
        category = (Category *)g_object_get_data (G_OBJECT (item), "category");
	if (!category)
		return;

	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (page->scrolled_window));
	gtk_widget_get_allocation (category->label, &alloc);
	gtk_adjustment_set_value (adj, alloc.y);
}

static void
ingredient_activated (GrIngredientsPage *page, GtkFlowBoxChild *item)
{
        GtkWidget *window;
        GtkWidget *tile;
        const char *ingredient;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GR_TYPE_WINDOW);
        tile = gtk_bin_get_child (GTK_BIN (item));
        ingredient = gr_ingredient_tile_get_ingredient (GR_INGREDIENT_TILE (tile));
        gr_window_show_search_by_ingredients (GR_WINDOW (window), ingredient);
}

static void
ingredients_page_finalize (GObject *object)
{
        GrIngredientsPage *page = GR_INGREDIENTS_PAGE (object);

	g_clear_pointer (&page->categories, g_hash_table_unref);

        G_OBJECT_CLASS (gr_ingredients_page_parent_class)->finalize (object);
}

static void
populate_initially (GrIngredientsPage *self)
{
	const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	self->categories = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, category_free);

        for (i = 0; i < strlen (alphabet); i++) {
		g_autofree char *letter = NULL;
                GtkWidget *item, *label, *box;
		Category *category;

		category = g_new (Category, 1);

		letter = g_strdup_printf ("%c", alphabet[i]);

                item = gtk_label_new (letter);
                g_object_set_data (G_OBJECT (item), "category", category);
                g_object_set (item, "xalign", 0.5, NULL);
                gtk_widget_show (item);
                gtk_style_context_add_class (gtk_widget_get_style_context (item), "letterbar");
                gtk_list_box_insert (GTK_LIST_BOX (self->letter_box), item, -1);

                label = gtk_label_new (letter);
                g_object_set (label, "xalign", 0, NULL);
                gtk_style_context_add_class (gtk_widget_get_style_context (label), "heading");
                gtk_widget_show (label);
                gtk_container_add (GTK_CONTAINER (self->main_box), label);

                box = gtk_flow_box_new ();
		gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (box), GTK_SELECTION_NONE);
                gtk_widget_show (box);
                gtk_container_add (GTK_CONTAINER (self->main_box), box);
                g_signal_connect_swapped (box, "child-activated", G_CALLBACK (ingredient_activated), self);

                category->name = g_strdup (letter);
                category->item = item;
                category->label = label;
                category->box = box;
		category->filled = FALSE;

		g_hash_table_insert (self->categories, g_strdup (letter), category);
        }
}

static void
gr_ingredients_page_init (GrIngredientsPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
	populate_initially (page);
	ingredients_page_reload (page);
	connect_store_signals (page);
}

static void
gr_ingredients_page_class_init (GrIngredientsPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = ingredients_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredients-page.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrIngredientsPage, main_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrIngredientsPage, letter_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrIngredientsPage, scrolled_window);

	gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), row_activated);
	gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), ingredient_activated);
}

GtkWidget *
gr_ingredients_page_new (void)
{
        GrIngredientsPage *page;

        page = g_object_new (GR_TYPE_INGREDIENTS_PAGE, NULL);

        return GTK_WIDGET (page);
}

static void
ingredients_page_reload (GrIngredientsPage *page)
{
	int i;
	Category *category;
	GHashTableIter iter;
        char **ingredients;
        GrRecipeStore *store;
        guint length;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        ingredients = gr_recipe_store_get_all_ingredients (store, &length);
	for (i = 0; i < length; i++) {
		GtkWidget *tile;
                gunichar ch;
                char buf[6] = { 0, };

                ch = g_utf8_get_char (ingredients[i]);
                ch = g_unichar_toupper (ch);
                g_unichar_to_utf8 (ch, buf);

		category = g_hash_table_lookup (page->categories, buf);

		if (!category) {
			g_print ("no such category: %s", buf);
			continue;
		}
		tile = gr_ingredient_tile_new (ingredients[i]);
		gtk_widget_show (tile);

		gtk_container_add (GTK_CONTAINER (category->box), tile);
		category->filled = TRUE;
	}
        g_strfreev (ingredients);

	g_hash_table_iter_init (&iter, page->categories);
	while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&category)) {
		gtk_widget_set_visible (category->label, category->filled);
		gtk_widget_set_visible (category->box, category->filled);
		gtk_label_set_label (GTK_LABEL (category->item), category->filled ? category->name : "⋯");
		gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (gtk_widget_get_parent (category->item)), category->filled);
		}
}

static void
collect_selected (GtkWidget *widget,
                  gpointer   data)
{
	GString *s = data;
	GtkWidget *tile;

	if (!gtk_flow_box_child_is_selected (GTK_FLOW_BOX_CHILD (widget)))
		return;

	tile = gtk_bin_get_child (GTK_BIN (widget));

	if (s->len > 0)
		g_string_append (s, " ");

	g_string_append (s, "i:");
	g_string_append (s, gr_ingredient_tile_get_ingredient (GR_INGREDIENT_TILE (tile)));
}

char *
gr_ingredients_page_get_search_terms (GrIngredientsPage *page)
{
	GString *s;
	GHashTableIter iter;
	Category *category;

	s = g_string_new ("");

	g_hash_table_iter_init (&iter, page->categories);
	while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&category)) {
		gtk_container_foreach (GTK_CONTAINER (category->box), collect_selected, s);
	}

	return g_string_free (s, FALSE);
}

static void
connect_store_signals (GrIngredientsPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        /* FIXME: inefficient */
        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (ingredients_page_reload), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (ingredients_page_reload), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (ingredients_page_reload), page);
}
