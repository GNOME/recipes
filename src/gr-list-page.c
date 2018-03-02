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
#include "gr-utils.h"
#include "gr-season.h"
#include "gr-category-tile.h"
#include "gr-image.h"
#include "gr-app.h"
#include "gr-settings.h"

struct _GrListPage
{
        GtkBox parent_instance;

        GrChef *chef;
        GrDiets diet;
        gboolean favorites;
        gboolean all;
        gboolean new;
        char *season;
        GList *recipes;
        GrImage *ri;
        GCancellable *cancellable;

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

        gboolean show_shared;

        int count;
        GrRecipeSearch *search;
};

G_DEFINE_TYPE (GrListPage, gr_list_page, GTK_TYPE_BOX)

static void connect_store_signals (GrListPage *page);

/* keep this function in sync with gr_list_page_repopulate */
static void
clear_data (GrListPage *self)
{
        if (self->chef) {
                gtk_style_context_remove_class (gtk_widget_get_style_context (self->chef_image),
                                                gr_chef_get_id (self->chef));
        }

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
        g_clear_object (&self->ri);
        g_clear_object (&self->chef);
        self->diet = 0;
        self->favorites = FALSE;
        self->all = FALSE;
        self->new = FALSE;
        g_clear_pointer (&self->season, g_free);
        g_list_free_full (self->recipes, g_object_unref);
        self->recipes = NULL;
}

static void
list_page_finalize (GObject *object)
{
        GrListPage *self = GR_LIST_PAGE (object);

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
        g_clear_object (&self->ri);
        g_clear_object (&self->chef);
        g_clear_pointer (&self->season, g_free);
        g_list_free_full (self->recipes, g_object_unref);
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
                gr_recipe_tile_set_show_shared (GR_RECIPE_TILE (tile), page->show_shared);
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
gr_list_page_set_sort (GrListPage *page,
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
sort_key_changed (GrListPage *page)
{
        GrSortKey sort;

        if (!gtk_widget_get_visible (GTK_WIDGET (page)))
                return;

        sort = g_settings_get_enum (gr_settings_get (), "sort-key");
        gr_list_page_set_sort (page, sort);
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

        g_signal_connect_swapped (gr_settings_get (), "changed::sort-key", G_CALLBACK (sort_key_changed), page);
        g_signal_connect (page, "notify::visible", G_CALLBACK (sort_key_changed), NULL);
}

static void
gr_list_page_class_init (GrListPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = list_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-list-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrListPage, top_box);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, flow_box);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, list_stack);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, empty_title);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, empty_subtitle);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, chef_grid);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, chef_image);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, chef_fullname);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, chef_description);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, heading);
        gtk_widget_class_bind_template_child (widget_class, GrListPage, diet_description);
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
                return _("Gluten-free Recipes");
        case GR_DIET_NUT_FREE:
                return _("Nut-free Recipes");
        case GR_DIET_VEGAN:
                return _("Vegan Recipes");
        case GR_DIET_VEGETARIAN:
                return _("Vegetarian Recipes");
        case GR_DIET_MILK_FREE:
                return _("Milk-free Recipes");
        case GR_DIET_HALAL:
                return _("Halal Recipes");
        default:
                return  _("Other Dietary Restrictions");
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
        case GR_DIET_HALAL:
                return "halal";
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

        self->show_shared = FALSE;

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
gr_list_page_set_show_shared (GrListPage *self,
                              gboolean    show_shared)
{
        self->show_shared = show_shared;
}

void
gr_list_page_populate_from_chef (GrListPage *self,
                                 GrChef     *chef,
                                 gboolean    show_shared)
{
        GrRecipeStore *store;
        const char *id;
        const char *name;
        const char *fullname;
        const char *description;
        const char *path;
        char *tmp;
        g_autofree char *term = NULL;

        self->show_shared = show_shared;

        g_object_ref (chef);
        clear_data (self);
        self->chef = chef;

        id = gr_chef_get_id (chef);
        name = gr_chef_get_name (chef) ? gr_chef_get_name (chef) : "";
        fullname = gr_chef_get_fullname (chef) ? gr_chef_get_fullname (chef) : "";
        description = gr_chef_get_translated_description (chef) ? gr_chef_get_translated_description (chef) : "";
        path = gr_chef_get_image (chef);

        gtk_image_clear (GTK_IMAGE (self->chef_image));
        if (path && path[0]) {
                self->ri = gr_image_new (gr_app_get_soup_session (GR_APP (g_application_get_default ())),
                                         gr_chef_get_id (chef),
                                         path);
                self->cancellable = g_cancellable_new ();

                gr_image_load (self->ri, 64, 64, FALSE, self->cancellable, gr_image_set_pixbuf, self->chef_image);
        }

        gtk_widget_show (self->chef_grid);
        gtk_widget_show (self->heading);
        gtk_widget_hide (self->diet_description);

        gtk_label_set_label (GTK_LABEL (self->chef_fullname), fullname);
        gtk_label_set_markup (GTK_LABEL (self->chef_description), description);

        tmp = g_strdup_printf (_("Recipes by %s"), name);
        gtk_label_set_label (GTK_LABEL (self->heading), tmp);
        g_free (tmp);

        store = gr_recipe_store_get ();

        container_remove_all (GTK_CONTAINER (self->flow_box));
        tmp = g_strdup_printf (_("No recipes by chef %s found"), name);
        gtk_label_set_label (GTK_LABEL (self->empty_title), tmp);
        g_free (tmp);
        if (g_strcmp0 (gr_chef_get_id (chef), gr_recipe_store_get_user_key (store)) == 0)
                gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("You could add one using the “New Recipe” button."));
        else
                gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Sorry about this."));

        gr_recipe_search_stop (self->search);
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");

        term = g_strconcat ("by:", id, NULL);

        gr_recipe_search_set_query (self->search, term);
}

void
gr_list_page_populate_from_season (GrListPage *self,
                                   const char *season)
{
        char *tmp;
        g_autofree char *term = NULL;

        self->show_shared = FALSE;

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
        self->show_shared = FALSE;

        clear_data (self);
        self->favorites = TRUE;

        gtk_widget_hide (self->chef_grid);
        gtk_widget_hide (self->heading);
        gtk_widget_hide (self->diet_description);

        container_remove_all (GTK_CONTAINER (self->flow_box));
        gtk_label_set_label (GTK_LABEL (self->empty_title), _("No favorite recipes found"));
        gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Use the ♥ button to mark recipes as favorites."));

        gr_recipe_search_stop (self->search);
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
        gr_recipe_search_set_query (self->search, "is:favorite");
}

void
gr_list_page_populate_from_all (GrListPage *self)
{
        self->show_shared = FALSE;

        clear_data (self);
        self->all = TRUE;

        gtk_widget_hide (self->chef_grid);
        gtk_widget_hide (self->heading);
        gtk_widget_hide (self->diet_description);

        container_remove_all (GTK_CONTAINER (self->flow_box));
        gtk_label_set_label (GTK_LABEL (self->empty_title), _("No recipes found"));
        gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Sorry about this."));

        gr_recipe_search_stop (self->search);
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
        gr_recipe_search_set_query (self->search, "is:any");
}

void
gr_list_page_populate_from_new (GrListPage *self)
{
        g_autoptr(GDateTime) now = NULL;
        g_autoptr(GDateTime) time = NULL;
        g_autofree char *timestamp = NULL;
        g_autofree char *query = NULL;
        const char *terms[2];

        self->show_shared = FALSE;

        clear_data (self);
        self->new = TRUE;

        gtk_widget_hide (self->chef_grid);
        gtk_widget_hide (self->heading);
        gtk_widget_hide (self->diet_description);

        container_remove_all (GTK_CONTAINER (self->flow_box));
        gtk_label_set_label (GTK_LABEL (self->empty_title), _("No new recipes"));
        gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Sorry about this."));

        gr_recipe_search_stop (self->search);
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
        now = g_date_time_new_now_utc ();
        time = g_date_time_add_weeks (now, -1);
        timestamp = date_time_to_string (time);
        query = g_strconcat ("ct:", timestamp, NULL);
        terms[0] = (const char*)query;
        terms[1] = NULL;
        gr_recipe_search_set_terms (self->search, terms);
}

void
gr_list_page_populate_from_list (GrListPage *self,
                                 GList      *recipes)
{
        GrRecipeStore *store;
        GList *l;
        gboolean empty;

        self->show_shared = FALSE;

        store = gr_recipe_store_get ();

        recipes = g_list_copy_deep (recipes, (GCopyFunc)g_object_ref, NULL);
        clear_data (self);
        self->recipes = recipes;

        gtk_widget_hide (self->chef_grid);
        gtk_widget_hide (self->heading);
        gtk_widget_hide (self->diet_description);

        container_remove_all (GTK_CONTAINER (self->flow_box));
        gtk_label_set_label (GTK_LABEL (self->empty_title), _("No imported recipes found"));
        gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Sorry about this."));
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "empty");

        empty = TRUE;

        for (l = self->recipes; l; l = l->next) {
                GrRecipe *recipe = l->data;
                g_autoptr(GrRecipe) r2 = NULL;

                r2 = gr_recipe_store_get_recipe (store, gr_recipe_get_id (recipe));
                if (r2 == recipe) {
                        GtkWidget *tile = gr_recipe_tile_new (recipe);
                        gtk_widget_show (tile);
                        gtk_container_add (GTK_CONTAINER (self->flow_box), tile);
                        empty = FALSE;
                }
        }

        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack),
                                          empty ? "empty" : "list");
}

/* keep function this in sync with clear_data */
void
gr_list_page_repopulate (GrListPage *page)
{
        if (page->chef)
                gr_list_page_populate_from_chef (page, page->chef, page->show_shared);
        else if (page->diet)
                gr_list_page_populate_from_diet (page, page->diet);
        else if (page->favorites)
                gr_list_page_populate_from_favorites (page);
        else if (page->season)
                gr_list_page_populate_from_season (page, page->season);
        else if (page->recipes)
                gr_list_page_populate_from_list (page, page->recipes);
        else if (page->all)
                gr_list_page_populate_from_all (page);
        else if (page->new)
                gr_list_page_populate_from_new (page);
}

static void
repopulate (GrListPage *page)
{
        if (gtk_widget_is_drawable (GTK_WIDGET (page)))
                gr_list_page_repopulate (page);
}

static void
connect_store_signals (GrListPage *page)
{
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (repopulate), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (repopulate), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (repopulate), page);
}

void
gr_list_page_clear (GrListPage *self)
{
        gr_recipe_search_stop (self->search);
        container_remove_all (GTK_CONTAINER (self->flow_box));
}
