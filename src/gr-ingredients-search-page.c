/* gr-ingredients-search-page.c:
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

#include "gr-recipe-store.h"
#include "gr-recipe-tile.h"
#include "gr-recipe.h"
#include "gr-app.h"
#include "gr-utils.h"
#include "gr-ingredients-search-page.h"
#include "gr-ingredient-search-tile.h"
#include "gr-ingredient.h"


typedef struct {
        GrIngredientsSearchPage *page;
        GrRecipeStore *store;
        char *cf_term;
        char **keys;
        int length;
        int pos;
	gboolean filled;
} SearchParams;

static void
search_params_free (gpointer data)
{
        SearchParams *params = data;

        g_free (params->cf_term);
        g_free (params->keys);
        g_free (params);
}

struct _GrIngredientsSearchPage
{
        GtkBox parent_instance;

        GtkWidget *ingredients_popover;
        GtkWidget *ingredients_list;
        GtkWidget *flow_box;
        GtkWidget *terms_box;
        GtkWidget *search_entry;
	GtkWidget *search_stack;

        char *cf_term;
        guint search;
};

G_DEFINE_TYPE (GrIngredientsSearchPage, gr_ingredients_search_page, GTK_TYPE_BOX)

static void update_search (GrIngredientsSearchPage *page);

static void
remove_tile (GrIngredientSearchTile *tile, gpointer data)
{
        GrIngredientsSearchPage *page = data;

        gtk_container_remove (GTK_CONTAINER (page->terms_box), GTK_WIDGET (tile));
        update_search (page);
}

static void
tile_changed (GrIngredientSearchTile *tile, gpointer data)
{
        GrIngredientsSearchPage *page = data;

        update_search (page);
}

static void
add_ingredient (GrIngredientsSearchPage *page,
                const char              *ingredient)
{
        GtkWidget *tile;

        tile = gr_ingredient_search_tile_new (ingredient);
        g_signal_connect (tile, "remove-tile", G_CALLBACK (remove_tile), page);
        g_signal_connect (tile, "tile-changed", G_CALLBACK (tile_changed), page);

        gtk_widget_show (tile);
        gtk_container_add (GTK_CONTAINER (page->terms_box), tile);

        update_search (page);
}

static void
search_changed (GrIngredientsSearchPage *page)
{
        const char *term;

        term = gtk_entry_get_text (GTK_ENTRY (page->search_entry));
        if (strlen (term) < 2) {
                gtk_widget_hide (page->ingredients_popover);
                return;
        }

        g_free (page->cf_term);
        page->cf_term = g_utf8_casefold (gtk_entry_get_text (GTK_ENTRY (page->search_entry)), -1);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (page->ingredients_list));
        gtk_widget_show (page->ingredients_popover);
}

static void
search_activate (GrIngredientsSearchPage *page)
{
        const char *term;

        term = gtk_entry_get_text (GTK_ENTRY (page->search_entry));
        if (strlen (term) < 2) {
                gtk_widget_error_bell (page->search_entry);
                return;
        }

        add_ingredient (page, term);

        gtk_entry_set_text (GTK_ENTRY (page->search_entry), "");
        gtk_widget_hide (page->ingredients_popover);
        gtk_widget_grab_focus (page->search_entry);
}

static void
ingredients_search_page_finalize (GObject *object)
{
        GrIngredientsSearchPage *self = GR_INGREDIENTS_SEARCH_PAGE (object);

        g_clear_pointer (&self->cf_term, g_free);
        if (self->search)
                g_source_remove (self->search);

        G_OBJECT_CLASS (gr_ingredients_search_page_parent_class)->finalize (object);
}

static gboolean
update_search_idle (gpointer data)
{
        SearchParams *params = data;

        if (params->cf_term == NULL) {
                GList *terms, *l;
                GString *s;

                container_remove_all (GTK_CONTAINER (params->page->flow_box));
                terms = gtk_container_get_children (GTK_CONTAINER (params->page->terms_box));
                if (terms == NULL) {
		        gtk_stack_set_visible_child_name (GTK_STACK (params->page->search_stack), "list");
                        return G_SOURCE_REMOVE;
                }

                s = g_string_new ("");
                for (l = terms; l; l = l->next) {
                        GrIngredientSearchTile *tile = l->data;

                        if (gr_ingredient_search_tile_get_excluded (tile))
                                g_string_append (s, "i-:");
                        else
                                g_string_append (s, "i+:");
                        g_string_append (s, gr_ingredient_search_tile_get_ingredient (tile));
                        g_string_append (s, " ");
                }
                g_list_free (terms);

                params->cf_term = g_utf8_casefold (s->str, -1);

                g_string_free (s, FALSE);

                return G_SOURCE_CONTINUE;
        }

        if (params->keys == NULL) {
                params->keys = gr_recipe_store_get_recipe_keys (params->store, &params->length);
                params->pos = 0;

                return G_SOURCE_CONTINUE;
        }

        if (params->pos < params->length) {
                int i;

                for (i = 0; params->pos < params->length && i < 5; params->pos++, i++) {
                        g_autoptr(GrRecipe) recipe = NULL;
                        GtkWidget *tile;

                        recipe = gr_recipe_store_get (params->store, params->keys[params->pos]);
                        if (gr_recipe_matches (recipe, params->cf_term)) {
                                tile = gr_recipe_tile_new (recipe);
                                gtk_widget_show (tile);
                                gtk_container_add (GTK_CONTAINER (params->page->flow_box), tile);
			        params->filled = TRUE;
                        }
                }
        }

	if (params->pos < params->length)
		return G_SOURCE_CONTINUE;

	gtk_stack_set_visible_child_name (GTK_STACK (params->page->search_stack), params->filled ? "list" : "empty");

	return G_SOURCE_REMOVE;
}

static void
update_search (GrIngredientsSearchPage *page)
{
        SearchParams *params;

        if (page->search) {
                g_source_remove (page->search);
                page->search = 0;
        }

        params = g_new0 (SearchParams, 1);
        params->page = page;
        params->store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, update_search_idle, params, search_params_free);
}

static void
row_activated (GtkListBox              *list,
               GtkListBoxRow           *row,
               GrIngredientsSearchPage *page)
{
        GtkWidget *label;
        const char *text;

        label = gtk_bin_get_child (GTK_BIN (row));
        text = gtk_label_get_label (GTK_LABEL (label));

        add_ingredient (page, text);

        gtk_entry_set_text (GTK_ENTRY (page->search_entry), "");
        gtk_widget_hide (page->ingredients_popover);
        gtk_widget_grab_focus (page->search_entry);
}

static gboolean
filter_ingredients_list (GtkListBoxRow *row,
                         gpointer       data)
{
        GrIngredientsSearchPage *page = data;
        GtkWidget *label;
        char *cf;

        if (!page->cf_term)
                return TRUE;

        label = gtk_bin_get_child (GTK_BIN (row));
        cf = (char *)g_object_get_data (G_OBJECT (label), "casefolded");

        return g_str_has_prefix (cf, page->cf_term);
}

static void
populate_popover (GrIngredientsSearchPage *page)
{
        int i;
        const char **ingredients;

        ingredients = gr_ingredient_get_names (NULL);
        for (i = 0; ingredients[i]; i++) {
                GtkWidget *label;
                gchar *cf;

                label = gtk_label_new (ingredients[i]);

                cf = g_utf8_casefold (ingredients[i], -1);
                g_object_set_data_full (G_OBJECT (label), "casefolded", cf, g_free);

                gtk_label_set_xalign (GTK_LABEL (label), 0);
                g_object_set (label, "margin", 6, NULL);
                gtk_widget_show (label);
                gtk_container_add (GTK_CONTAINER (page->ingredients_list), label);
        }

        gtk_list_box_set_filter_func (GTK_LIST_BOX (page->ingredients_list),
                                      filter_ingredients_list, page, NULL);

        g_signal_connect (page->ingredients_list, "row-activated", G_CALLBACK (row_activated), page);
}

static void
gr_ingredients_search_page_init (GrIngredientsSearchPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));

        populate_popover (page);
}

static void
gr_ingredients_search_page_class_init (GrIngredientsSearchPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = ingredients_search_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredients-search-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrIngredientsSearchPage, flow_box);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsSearchPage, terms_box);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsSearchPage, ingredients_popover);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsSearchPage, search_entry);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsSearchPage, ingredients_list);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsSearchPage, search_stack);


        gtk_widget_class_bind_template_callback (widget_class, search_changed);
        gtk_widget_class_bind_template_callback (widget_class, search_activate);
}

GtkWidget *
gr_ingredients_search_page_new (void)
{
        GrIngredientsSearchPage *page;

        page = g_object_new (GR_TYPE_INGREDIENTS_SEARCH_PAGE, NULL);

        return GTK_WIDGET (page);
}

void
gr_ingredients_search_page_set_ingredient (GrIngredientsSearchPage *page,
                                           const char              *ingredient)
{
        container_remove_all (GTK_CONTAINER (page->terms_box));
	gtk_stack_set_visible_child_name (GTK_STACK (page->search_stack), "list");
	if (ingredient)
        	add_ingredient (page, ingredient);
}
