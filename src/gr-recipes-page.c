/* gr-recipes-page.c:
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

#include "gr-recipe.h"
#include "gr-recipes-page.h"
#include "gr-recipe-tile.h"
#include "gr-category-tile.h"
#include "gr-chef-tile.h"
#include "gr-recipe-store.h"
#include "gr-utils.h"
#include "gr-window.h"


struct _GrRecipesPage
{
        GtkBox parent_instance;

        GtkWidget *today_box;
        GtkWidget *pick_box;
        GtkWidget *diet_box;
        GtkWidget *chefs_box;
        GtkWidget *categories_expander_image;
        GtkWidget *diet_more;
        GtkWidget *diet_box2;
        GtkWidget *scrolled_win;
        GtkWidget *shopping_tile;
        GtkWidget *shopping_list;
        GtkWidget *shopping_time;

        guint shopping_timeout;
};

G_DEFINE_TYPE (GrRecipesPage, gr_recipes_page, GTK_TYPE_BOX)

static void populate_recipes_from_store (GrRecipesPage *page);
static void populate_shopping_from_store (GrRecipesPage *page);
static void populate_categories_from_store (GrRecipesPage *page);
static void populate_chefs_from_store (GrRecipesPage *page);
static void connect_store_signals (GrRecipesPage *page);

static void
show_chef_list (GtkFlowBox      *box,
                GtkFlowBoxChild *child,
                GrRecipesPage   *page)
{
        GtkWidget *tile;
        GtkWidget *window;
        GrChef *chef;

        tile = gtk_bin_get_child (GTK_BIN (child));
        chef = gr_chef_tile_get_chef (GR_CHEF_TILE (tile));
        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);
        gr_window_show_chef (GR_WINDOW (window), chef);
}

static void
set_categories_expanded (GrRecipesPage *page,
                         gboolean       expanded)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->diet_more), expanded);
        gtk_image_set_from_icon_name (GTK_IMAGE (page->categories_expander_image),
                                      expanded ? "pan-up-symbolic" : "pan-down-symbolic", 1);
}

static gboolean update_shopping_time (gpointer data);

void
gr_recipes_page_unexpand (GrRecipesPage *page)
{
        GtkRevealerTransitionType transition;

        transition = gtk_revealer_get_transition_type (GTK_REVEALER (page->diet_more));
        gtk_revealer_set_transition_type (GTK_REVEALER (page->diet_more),
                                          GTK_REVEALER_TRANSITION_TYPE_NONE);

        set_categories_expanded (page, FALSE);

        gtk_revealer_set_transition_type (GTK_REVEALER (page->diet_more), transition);

        update_shopping_time (page);
}

static void
expander_button_clicked (GrRecipesPage *page)
{
        gboolean expanded;

        expanded = gtk_revealer_get_reveal_child (GTK_REVEALER (page->diet_more));
        set_categories_expanded (page, !expanded);
}

static void
recipes_page_finalize (GObject *object)
{
        GrRecipesPage *page = GR_RECIPES_PAGE (object);

        if (page->shopping_timeout != 0) {
                g_source_remove (page->shopping_timeout);
                page->shopping_timeout = 0;
        }

        G_OBJECT_CLASS (gr_recipes_page_parent_class)->finalize (object);
}

static void
shopping_tile_clicked (GrRecipesPage *page)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_show_shopping (GR_WINDOW (window));
}

static void
gr_recipes_page_init (GrRecipesPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));

        populate_recipes_from_store (page);
        populate_shopping_from_store (page);
        populate_categories_from_store (page);
        populate_chefs_from_store (page);
        connect_store_signals (page);

        page->shopping_timeout = g_timeout_add_seconds (300, update_shopping_time, page);
}

static void
gr_recipes_page_class_init (GrRecipesPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = recipes_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-recipes-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, today_box);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, pick_box);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, diet_box);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, chefs_box);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, categories_expander_image);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, diet_box2);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, diet_more);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, scrolled_win);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, shopping_tile);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, shopping_list);
        gtk_widget_class_bind_template_child (widget_class, GrRecipesPage, shopping_time);

        gtk_widget_class_bind_template_callback (widget_class, show_chef_list);
        gtk_widget_class_bind_template_callback (widget_class, expander_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, shopping_tile_clicked);
}

GtkWidget *
gr_recipes_page_new (void)
{
        GrRecipesPage *page;

        page = g_object_new (GR_TYPE_RECIPES_PAGE, NULL);

        return GTK_WIDGET (page);
}

static void
category_clicked (GrCategoryTile *tile,
                  GrRecipesPage  *page)
{
        GtkWidget *window;
        GrDiets diet;
        const char *name;

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);

        diet = gr_category_tile_get_diet (tile);
        name = gr_category_tile_get_category (tile);

        if (diet)
                gr_window_show_diet (GR_WINDOW (window), diet);
        else if (strcmp (name, "favorites") == 0)
                gr_window_show_favorites (GR_WINDOW (window));
        else if (strcmp (name, "mine") == 0)
                gr_window_show_mine (GR_WINDOW (window));
        else if (strcmp (name, "all") == 0)
                gr_window_show_all (GR_WINDOW (window));
        else if (strcmp (name, "new") == 0)
                gr_window_show_new (GR_WINDOW (window));
}

static void
populate_categories_from_store (GrRecipesPage *self)
{
        int i;
        struct {
                const char *id;
                const char *name;
        } tiles[] = {
                { "mine",      N_("My Recipes")  },
                { "favorites", N_("Favorites")   },
                { "all",       N_("All Recipes") },
                { "new",       N_("New Recipes") },
        };
        GrDiets diets[6] = {
                GR_DIET_GLUTEN_FREE,
                GR_DIET_NUT_FREE,
                GR_DIET_VEGAN,
                GR_DIET_VEGETARIAN,
                GR_DIET_MILK_FREE,
                GR_DIET_HALAL
        };
        GtkWidget *tile;

        container_remove_all (GTK_CONTAINER (self->diet_box));
        container_remove_all (GTK_CONTAINER (self->diet_box2));

        for (i = 0; i < G_N_ELEMENTS (tiles); i++) {
                tile = gr_category_tile_new_with_label (tiles[i].id, _(tiles[i].name));
                gtk_container_add (GTK_CONTAINER (self->diet_box), tile);
                g_signal_connect (tile, "clicked", G_CALLBACK (category_clicked), self);
        }

        for (i = 0; i < G_N_ELEMENTS (diets); i++) {
                tile = gr_category_tile_new (diets[i]);
                g_signal_connect (tile, "clicked", G_CALLBACK (category_clicked), self);

                if (i + G_N_ELEMENTS (tiles) < 6)
                        gtk_container_add (GTK_CONTAINER (self->diet_box), tile);
                else
                        gtk_container_add (GTK_CONTAINER (self->diet_box2), tile);
        }
}

static gboolean
update_shopping_time (gpointer data)
{
        GrRecipesPage *self = data;
        GrRecipeStore *store;
        GDateTime *change;
        g_autoptr(GDateTime) now = NULL;

        store = gr_recipe_store_get ();

        now = g_date_time_new_now_utc ();
        change = gr_recipe_store_last_shopping_change (store);
        if (change) {
                g_autofree char *text = NULL;
                g_autofree char *tmp = NULL;

                text = format_date_time_difference (now, change);
                tmp = g_strconcat (_("Last edited:"), " ", text, NULL);
                gtk_label_set_label (GTK_LABEL (self->shopping_time), tmp);
        }
        else {
                gtk_label_set_label (GTK_LABEL (self->shopping_time), "");
        }

        return G_SOURCE_CONTINUE;
}

static void
populate_recipes_from_store (GrRecipesPage *self)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;
        int todays;
        int picks;

        container_remove_all (GTK_CONTAINER (self->today_box));
        container_remove_all (GTK_CONTAINER (self->pick_box));

        store = gr_recipe_store_get ();

        keys = gr_recipe_store_get_recipe_keys (store, &length);

        /* scramble the keys so we don't always get the same picks */
        for (i = 0; i < length; i++) {
                int r;
                char *tmp;

                r = g_random_int_range (0, length);

                tmp = keys[i];
                keys[i] = keys[r];
                keys[r] = tmp;
        }

        todays = 0;
        picks = 0;
        for (i = 0; (i < length) && (todays < 3 || picks < 3); i++) {
                g_autoptr(GrRecipe) recipe = NULL;
                GtkWidget *tile;

                recipe = gr_recipe_store_get_recipe (store, keys[i]);

                if (todays < 3 && gr_recipe_store_recipe_is_todays (store, recipe)) {
                        if (todays == 0) {
                                tile = gr_recipe_tile_new_wide (recipe);
                                gtk_grid_attach (GTK_GRID (self->today_box), tile, 0, 0, 2, 1);
                                todays += 2;
                        }
                        else {
                                tile = gr_recipe_tile_new (recipe);
                                gtk_grid_attach (GTK_GRID (self->today_box), tile, todays, 0, 1, 1);
                                todays += 1;
                        }
                }
                else if (picks < 3 && gr_recipe_store_recipe_is_pick (store, recipe)) {
                        tile = gr_recipe_tile_new (recipe);
                        gtk_grid_attach (GTK_GRID (self->pick_box), tile, picks, 0, 1, 1);
                        picks++;
                }
        }
}

static void
populate_shopping_from_store (GrRecipesPage *self)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int shopping;
        int i;
        g_autofree char *shop1 = NULL;
        g_autofree char *shop2 = NULL;
        char *tmp;

        store = gr_recipe_store_get ();

        keys = gr_recipe_store_get_recipe_keys (store, &length);

        shopping = 0;
        for (i = 0; i < length; i++) {
                g_autoptr(GrRecipe) recipe = NULL;

                recipe = gr_recipe_store_get_recipe (store, keys[i]);

                if (gr_recipe_store_is_in_shopping (store, recipe)) {
                        if (shopping == 0)
                                shop1 = g_markup_escape_text (gr_recipe_get_name (recipe), -1);
                        else if (shopping == 1)
                                shop2 = g_markup_escape_text (gr_recipe_get_name (recipe), -1);
                        shopping++;
                }
        }

        if (shopping == 1)
                tmp = g_strdup_printf (_("Buy ingredients: <b>%s</b>"), shop1);
        else if (shopping == 2)
                tmp = g_strdup_printf (_("Buy ingredients: <b>%s and %s</b>"), shop1, shop2);
        else
                tmp = g_strdup_printf (ngettext ("Buy ingredients: <b>%s, %s and %d other</b>",
                                                 "Buy ingredients: <b>%s, %s and %d others</b>", shopping - 2), shop1, shop2, shopping - 2);
        gtk_label_set_label (GTK_LABEL (self->shopping_list), tmp);
        g_free (tmp);

        gtk_widget_set_visible (self->shopping_tile, shopping > 0);

        update_shopping_time (self);
}

void
gr_recipes_page_refresh (GrRecipesPage *self)
{
        populate_shopping_from_store (self);
}


static void
repopulate_recipes (GrRecipesPage *self)
{
        if (gtk_widget_is_drawable (GTK_WIDGET (self)))
                gr_recipes_page_refresh (self);
}

static void
populate_chefs_from_store (GrRecipesPage *self)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;
        int count;

        container_remove_all (GTK_CONTAINER (self->chefs_box));

        store = gr_recipe_store_get ();

        keys = gr_recipe_store_get_chef_keys (store, &length);
        /* scramble the keys so we don't always get the same picks */
        for (i = 0; i < length; i++) {
                int r;
                char *tmp;

                r = g_random_int_range (0, length);

                tmp = keys[i];
                keys[i] = keys[r];
                keys[r] = tmp;
        }

        count = 0;
        for (i = 0; i < length && count < 6; i++) {
                g_autoptr(GrChef) chef = NULL;

                chef = gr_recipe_store_get_chef (store, keys[i]);

                if (gr_recipe_store_chef_is_featured (store, chef) &&
                    gr_recipe_store_has_chef (store, chef)) {
                        GtkWidget *tile;
                        tile = gr_chef_tile_new (chef);
                        gtk_widget_show (tile);
                        gtk_container_add (GTK_CONTAINER (self->chefs_box), tile);

                        count++;
                }
        }
}

static void
refresh_chefs (GrRecipesPage *self)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (self->chefs_box));
        for (l = children; l; l = l->next) {
                GtkWidget *child = l->data;
                GtkWidget *tile = gtk_bin_get_child (GTK_BIN (child));
                gr_chef_tile_set_chef (GR_CHEF_TILE (tile), gr_chef_tile_get_chef (GR_CHEF_TILE (tile)));
        }
        g_list_free (children);
}

static void
reloaded (GrRecipesPage *self)
{
        populate_recipes_from_store (self);
        populate_shopping_from_store (self);
        populate_chefs_from_store (self);
}

static void
connect_store_signals (GrRecipesPage *page)
{
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (repopulate_recipes), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (repopulate_recipes), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (repopulate_recipes), page);
        g_signal_connect_swapped (store, "chefs-changed", G_CALLBACK (refresh_chefs), page);
        g_signal_connect_swapped (store, "reloaded", G_CALLBACK (reloaded), page);
}
