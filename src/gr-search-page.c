/* gr-search-page.c:
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

#include "gr-search-page.h"
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-recipe-tile.h"
#include "gr-utils.h"
#include "gr-list-page.h"
#include "gr-settings.h"


struct _GrSearchPage
{
        GtkBox parent_instance;

        GtkWidget *search_stack;
        GtkWidget *flow_box;
        int count;

        GrRecipeSearch *search;
};

G_DEFINE_TYPE (GrSearchPage, gr_search_page, GTK_TYPE_BOX)

static void connect_store_signals (GrSearchPage *page);

static void
search_page_finalize (GObject *object)
{
        GrSearchPage *self = GR_SEARCH_PAGE (object);

        g_clear_object (&self->search);

        G_OBJECT_CLASS (gr_search_page_parent_class)->finalize (object);
}

static void
search_started (GrRecipeSearch *search,
                GrSearchPage   *page)
{
        container_remove_all (GTK_CONTAINER (page->flow_box));
        page->count = 0;
}

static void
search_hits_added (GrRecipeSearch *search,
                   GList          *hits,
                   GrSearchPage   *page)
{
        GList *l;

        for (l = hits; l; l = l->next) {
                GrRecipe *recipe = l->data;
                GtkWidget *tile;

                tile = gr_recipe_tile_new (recipe);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (page->flow_box), tile);

                page->count++;
        }
}

static void
search_hits_removed (GrRecipeSearch *search,
                     GList          *hits,
                     GrSearchPage   *page)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (page->flow_box));
        for (l = children; l; l = l->next) {
                GtkWidget *item = l->data;
                GtkWidget *tile;
                GrRecipe *recipe;

                tile = gtk_bin_get_child (GTK_BIN (item));
                recipe = gr_recipe_tile_get_recipe (GR_RECIPE_TILE (tile));
                if (g_list_find (hits, recipe)) {
                        gtk_container_remove (GTK_CONTAINER (page->flow_box), item);
                        page->count--;
                }
        }
}

static void
search_finished (GrRecipeSearch *search,
                 GrSearchPage   *page)
{
        gtk_stack_set_visible_child_name (GTK_STACK (page->search_stack),
                                          page->count > 0 ? "list" : "empty");
}

static int
name_sort_func (GtkFlowBoxChild *child1,
                GtkFlowBoxChild *child2,
                gpointer         data)
{
        GtkWidget *tile1 = gtk_bin_get_child (GTK_BIN (child1));
        GtkWidget *tile2 = gtk_bin_get_child (GTK_BIN (child2));
        GrRecipe *recipe1 = gr_recipe_tile_get_recipe (GR_RECIPE_TILE (tile1));
        GrRecipe *recipe2 = gr_recipe_tile_get_recipe (GR_RECIPE_TILE (tile2));
        const char *name1 = gr_recipe_get_name (recipe1);
        const char *name2 = gr_recipe_get_name (recipe2);

        return strcmp (name1, name2);
}

static int
date_sort_func (GtkFlowBoxChild *child1,
                GtkFlowBoxChild *child2,
                gpointer         data)
{
        GtkWidget *tile1 = gtk_bin_get_child (GTK_BIN (child1));
        GtkWidget *tile2 = gtk_bin_get_child (GTK_BIN (child2));
        GrRecipe *recipe1 = gr_recipe_tile_get_recipe (GR_RECIPE_TILE (tile1));
        GrRecipe *recipe2 = gr_recipe_tile_get_recipe (GR_RECIPE_TILE (tile2));
        GDateTime *mtime1 = gr_recipe_get_mtime (recipe1);
        GDateTime *mtime2 = gr_recipe_get_mtime (recipe2);

        return g_date_time_compare (mtime2, mtime1);
}

static void
gr_search_page_set_sort (GrSearchPage *page,
                         GrSortKey   sort)
{
        switch (sort) {
        case SORT_BY_NAME:
                gtk_flow_box_set_sort_func (GTK_FLOW_BOX (page->flow_box), name_sort_func, page, NULL);
                break;
        case SORT_BY_RECENCY:
                gtk_flow_box_set_sort_func (GTK_FLOW_BOX (page->flow_box), date_sort_func, page, NULL);
                break;
        default:
                g_assert_not_reached ();
        }
}

static void
sort_key_changed (GrSearchPage *page)
{
        GrSortKey sort;

        if (!gtk_widget_get_visible (GTK_WIDGET (page)))
                return;

        sort = g_settings_get_enum (gr_settings_get (), "sort-key");
        gr_search_page_set_sort (page, sort);
}

static void
gr_search_page_init (GrSearchPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
        connect_store_signals (page);

        page->search = gr_recipe_search_new ();
        g_signal_connect (page->search, "started", G_CALLBACK (search_started), page);
        g_signal_connect (page->search, "hits-added", G_CALLBACK (search_hits_added), page);
        g_signal_connect (page->search, "hits-removed", G_CALLBACK (search_hits_removed), page);
        g_signal_connect (page->search, "finished", G_CALLBACK (search_finished), page);

        g_signal_connect_swapped (gr_settings_get (), "changed::sort-key", G_CALLBACK (sort_key_changed), page);
        g_signal_connect (page, "notify::visible", G_CALLBACK (sort_key_changed), NULL);
}

static void
gr_search_page_class_init (GrSearchPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = search_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-search-page.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrSearchPage, flow_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrSearchPage, search_stack);
}

GtkWidget *
gr_search_page_new (void)
{
        GrSearchPage *page;

        page = g_object_new (GR_TYPE_SEARCH_PAGE, NULL);

        return GTK_WIDGET (page);
}

typedef struct
{
        const char *term;
        gboolean filled;
} CheckData;

#if 0
static void
check_match (GtkWidget *child,
             gpointer   data)
{
        GtkWidget *tile;
        GrRecipe *recipe;
        CheckData *check_data = data;

        tile = gtk_bin_get_child (GTK_BIN (child));
        recipe = gr_recipe_tile_get_recipe (GR_RECIPE_TILE (tile));

        if (!gr_recipe_matches (recipe, check_data->term))
                gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (child)), child);
        else
                check_data->filled = TRUE;
}
#endif

void
gr_search_page_update_search (GrSearchPage  *page,
                              const char   **terms)
{
        gtk_stack_set_visible_child_name (GTK_STACK (page->search_stack), "list");

        if (terms == NULL || terms[0] == NULL) {
                container_remove_all (GTK_CONTAINER (page->flow_box));
        }

        gr_recipe_search_set_terms (page->search, terms);
}

static void
search_page_reload (GrSearchPage *page)
{
        if (gtk_widget_is_drawable (GTK_WIDGET (page)))
                gr_search_page_update_search (page, gr_recipe_search_get_terms (page->search));
}

static void
connect_store_signals (GrSearchPage *page)
{
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (search_page_reload), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (search_page_reload), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (search_page_reload), page);
}
