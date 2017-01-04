/* gr-list-page.c:
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

#include "gr-list-page.h"
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-recipe-tile.h"
#include "gr-app.h"
#include "gr-utils.h"
#include "gr-season.h"
#include "gr-category-tile.h"

struct _GrListPage
{
        GtkBox parent_instance;

        GrChef *chef;
        GrDiets diet;
        gboolean favorites;
        char *season;

        GtkWidget *top_box;
        GtkWidget *list_stack;
        GtkWidget *flow_box;
        GtkWidget *empty_title;
        GtkWidget *empty_subtitle;

        GtkWidget *chef_grid;
        GtkWidget *chef_image;
        GtkWidget *chef_fullname;
        GtkWidget *chef_description;
        GtkWidget *heading;
        GtkWidget *diet_description;

        int count;
        GrRecipeSearch *search;
};

G_DEFINE_TYPE (GrListPage, gr_list_page, GTK_TYPE_BOX)

static void connect_store_signals (GrListPage *page);

static void
clear_data (GrListPage *self)
{
        if (self->chef) {
                gtk_style_context_remove_class (gtk_widget_get_style_context (self->chef_image),
                                                gr_chef_get_id (self->chef));
        }

        g_clear_object (&self->chef);
        self->diet = 0;
        self->favorites = FALSE;
        g_clear_pointer (&self->season, g_free);
}

static void
list_page_finalize (GObject *object)
{
        GrListPage *self = GR_LIST_PAGE (object);

        clear_data (self);

        g_clear_object (&self->search);

        G_OBJECT_CLASS (gr_list_page_parent_class)->finalize (object);
}

static void
hide_heading (GrListPage *self)
{
        gtk_widget_hide (self->chef_grid);
        gtk_widget_hide (self->diet_description);
        gtk_widget_hide (self->heading);
}

static void
show_heading (GrListPage *self)
{
        if (self->chef) {
                gtk_widget_show (self->chef_grid);
                gtk_widget_show (self->heading);
        }
        else if (self->diet) {
                gtk_widget_show (self->diet_description);
                gtk_widget_show (self->heading);
        }
}

static void
search_started (GrRecipeSearch *search,
                GrListPage     *page)
{
        container_remove_all (GTK_CONTAINER (page->flow_box));
        hide_heading (page);
        page->count = 0;
}

static void
search_hits_added (GrRecipeSearch *search,
                   GList          *hits,
                   GrListPage     *page)
{
        GList *l;
        int count = page->count;

        for (l = hits; l; l = l->next) {
                GrRecipe *recipe = l->data;
                GtkWidget *tile;

                tile = gr_recipe_tile_new (recipe);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (page->flow_box), tile);

                page->count++;
        }

        if (count == 0 && page->count > 0)
                show_heading (page);
}

static void
search_hits_removed (GrRecipeSearch *search,
                     GList          *hits,
                     GrListPage     *page)
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
                 GrListPage     *page)
{
        show_heading (page);
        gtk_stack_set_visible_child_name (GTK_STACK (page->list_stack),
                                          page->count > 0 ? "list" : "empty");
}

static void
gr_list_page_init (GrListPage *page)
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
gr_list_page_class_init (GrListPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = list_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-list-page.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, top_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, flow_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, list_stack);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, empty_title);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, empty_subtitle);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, chef_grid);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, chef_image);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, chef_fullname);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, chef_description);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, heading);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, diet_description);
}

GtkWidget *
gr_list_page_new (void)
{
        GrListPage *page;

        page = g_object_new (GR_TYPE_LIST_PAGE, NULL);

        return GTK_WIDGET (page);
}

static const char *
get_category_title (GrDiets diet)
{
        switch (diet) {
        case GR_DIET_GLUTEN_FREE:
                return _("Gluten-free recipes");
        case GR_DIET_NUT_FREE:
                return _("Nut-free recipes");
        case GR_DIET_VEGAN:
                return _("Vegan recipes");
        case GR_DIET_VEGETARIAN:
                return _("Vegetarian recipes");
        case GR_DIET_MILK_FREE:
                return _("Milk-free recipes");
        default:
                return  _("Other dietary restrictions");
        }
}

static const char *
get_diet_name (GrDiets diet)
{
        switch (diet) {
        case GR_DIET_GLUTEN_FREE:
                return "gluten-free";
        case GR_DIET_NUT_FREE:
                return "nut-free";
        case GR_DIET_VEGAN:
                return "vegan";
        case GR_DIET_VEGETARIAN:
                return "vegetarian";
        case GR_DIET_MILK_FREE:
                return "milk-free";
        default:
                return "";
        }
}

void
gr_list_page_populate_from_diet (GrListPage *self,
                                 GrDiets     diet)
{
        char *tmp;
        g_autofree char *term = NULL;

        clear_data (self);
        self->diet = diet;

        gtk_widget_hide (self->chef_grid);
        gtk_widget_show (self->diet_description);
        gtk_widget_show (self->heading);

        gtk_label_set_label (GTK_LABEL (self->heading), gr_diet_get_label (diet));
        gtk_label_set_markup (GTK_LABEL (self->diet_description), gr_diet_get_description (diet));

        container_remove_all (GTK_CONTAINER (self->flow_box));
        tmp = g_strdup_printf (_("No %s found"), get_category_title (diet));
        gtk_label_set_label (GTK_LABEL (self->empty_title), tmp);
        g_free (tmp);
        gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("You could add one using the “New Recipe” button."));

        gr_recipe_search_stop (self->search);
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
        term = g_strconcat ("di:", get_diet_name (diet), NULL);
        gr_recipe_search_set_query (self->search, term);
}

void
gr_list_page_populate_from_chef (GrListPage *self,
                                 GrChef     *chef)
{
        GrRecipeStore *store;
        const char *id;
        char *tmp;
        const char *description;
        g_autofree char *term = NULL;

        g_object_ref (chef);
        clear_data (self);
        self->chef = chef;

        gtk_style_context_add_class (gtk_widget_get_style_context (self->chef_image),
                                     gr_chef_get_id (self->chef));

        gtk_widget_show (self->chef_grid);
        gtk_widget_show (self->heading);
        gtk_widget_hide (self->diet_description);

        gtk_label_set_label (GTK_LABEL (self->chef_fullname), gr_chef_get_fullname (chef));
        description = gr_chef_get_translated_description (chef);
        gtk_label_set_markup (GTK_LABEL (self->chef_description), description);

        tmp = g_strdup_printf (_("Recipes by %s"), gr_chef_get_name (chef));
        gtk_label_set_label (GTK_LABEL (self->heading), tmp);
        g_free (tmp);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        container_remove_all (GTK_CONTAINER (self->flow_box));
        tmp = g_strdup_printf (_("No recipes by chef %s found"), gr_chef_get_name (chef));
        gtk_label_set_label (GTK_LABEL (self->empty_title), tmp);
        g_free (tmp);
        if (g_strcmp0 (gr_chef_get_id (chef), gr_recipe_store_get_user_key (store)) == 0)
                gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("You could add one using the “New Recipe” button."));
        else
                gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Sorry about this."));

        gr_recipe_search_stop (self->search);
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");

        id = gr_chef_get_id (chef);
        term = g_strconcat ("by:", id, NULL);

        gr_recipe_search_set_query (self->search, term);
}

void
gr_list_page_populate_from_season (GrListPage *self,
                                   const char *season)
{
        char *tmp;
        g_autofree char *term = NULL;

        tmp = g_strdup (season);
        clear_data (self);
        self->season = tmp;

        gtk_widget_hide (self->chef_grid);
        gtk_widget_hide (self->heading);
        gtk_widget_hide (self->diet_description);

        container_remove_all (GTK_CONTAINER (self->flow_box));
        tmp = g_strdup_printf (_("No recipes for %s found"), gr_season_get_title (self->season));
        gtk_label_set_label (GTK_LABEL (self->empty_title), tmp);
        g_free (tmp);
        gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("You could add one using the “New Recipe” button."));

        gr_recipe_search_stop (self->search);
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
        term = g_strconcat ("se:", self->season, NULL);
        gr_recipe_search_set_query (self->search, term);
}

void
gr_list_page_populate_from_favorites (GrListPage *self)
{
        clear_data (self);
        self->favorites = TRUE;

        gtk_widget_hide (self->chef_grid);
        gtk_widget_hide (self->heading);
        gtk_widget_hide (self->diet_description);

        container_remove_all (GTK_CONTAINER (self->flow_box));
        gtk_label_set_label (GTK_LABEL (self->empty_title), _("No favorite recipes found"));
        gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Use the “Cook later” button to mark recipes as favorites."));

        gr_recipe_search_stop (self->search);
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
        gr_recipe_search_set_query (self->search, "is:favorite");
}

void
gr_list_page_repopulate (GrListPage *page)
{
        if (page->chef)
                gr_list_page_populate_from_chef (page, page->chef);
        else if (page->diet)
                gr_list_page_populate_from_diet (page, page->diet);
        else if (page->favorites)
                gr_list_page_populate_from_favorites (page);
        else if (page->season)
                gr_list_page_populate_from_season (page, page->season);
}

static void
connect_store_signals (GrListPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        /* FIXME: inefficient */
        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (gr_list_page_repopulate), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (gr_list_page_repopulate), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (gr_list_page_repopulate), page);
}

void
gr_list_page_clear (GrListPage *self)
{
        gr_recipe_search_stop (self->search);
        container_remove_all (GTK_CONTAINER (self->flow_box));
}
