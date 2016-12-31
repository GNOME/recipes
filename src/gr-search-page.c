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
#include "gr-app.h"
#include "gr-utils.h"


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
gr_search_page_update_search (GrSearchPage *page,
                              const char   *term)
{
        g_autofree char *cf_term = NULL;

        gtk_stack_set_visible_child_name (GTK_STACK (page->search_stack), "list");

        if (term == NULL || strlen (term) < 1) {
                container_remove_all (GTK_CONTAINER (page->flow_box));
                return;
        }

        cf_term = g_utf8_casefold (term, -1);
        gr_recipe_search_set_query (page->search, cf_term);
}

static void
search_page_reload (GrSearchPage *page)
{
        gr_search_page_update_search (page, gr_recipe_search_get_query (page->search));
}

static void
connect_store_signals (GrSearchPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        /* FIXME: inefficient */
        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (search_page_reload), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (search_page_reload), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (search_page_reload), page);
}
