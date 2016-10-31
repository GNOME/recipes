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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include "gr-app.h"
#include "gr-utils.h"


struct _GrRecipesPage
{
        GtkBox        parent_instance;

        GtkWidget *today_box;
        GtkWidget *pick_box;
        GtkWidget *diet_box;
        GtkWidget *chefs_box;
};

G_DEFINE_TYPE (GrRecipesPage, gr_recipes_page, GTK_TYPE_BOX)

static void populate_static (GrRecipesPage *page);
static void populate_recipes_from_store (GrRecipesPage *page);
static void populate_authors_from_store (GrRecipesPage *page);
static void connect_store_signals (GrRecipesPage *page);

static void
recipes_page_finalize (GObject *object)
{
        G_OBJECT_CLASS (gr_recipes_page_parent_class)->finalize (object);
}

static void
gr_recipes_page_init (GrRecipesPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));

        populate_static (page);
        populate_recipes_from_store (page);
        populate_authors_from_store (page);
        connect_store_signals (page);
}

static void
gr_recipes_page_class_init (GrRecipesPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = recipes_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-recipes-page.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrRecipesPage, today_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrRecipesPage, pick_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrRecipesPage, diet_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrRecipesPage, chefs_box);
}

GtkWidget *
gr_recipes_page_new (void)
{
        GrRecipesPage *page;

        page = g_object_new (GR_TYPE_RECIPES_PAGE, NULL);

        return GTK_WIDGET (page);
}


/* Some filler data */
static void
populate_static (GrRecipesPage *self)
{
        int i;
	GrDiets diets[5] = {
                GR_DIET_GLUTEN_FREE,
                GR_DIET_NUT_FREE,
                GR_DIET_VEGAN,
                GR_DIET_VEGETARIAN,
                GR_DIET_MILK_FREE
        };

        for (i = 0; i < 5; i++) {
                GtkWidget *tile;

                tile = gr_category_tile_new (diets[i]);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (self->diet_box), tile);
        }
}

static void
populate_recipes_from_store (GrRecipesPage *self)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;

        container_remove_all (GTK_CONTAINER (self->today_box));
        container_remove_all (GTK_CONTAINER (self->pick_box));

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keys = gr_recipe_store_get_keys (store, &length);

        for (i = 0; i < length; i++) {
                g_autoptr(GrRecipe) recipe = NULL;
                GtkWidget *tile;

                recipe = gr_recipe_store_get (store, keys[i]);

                if (gr_recipe_store_is_todays (store, recipe)) {
                        tile = gr_recipe_tile_new (recipe);
                        gtk_widget_show (tile);
                        gtk_container_add (GTK_CONTAINER (self->today_box), tile);
                }
                else if (gr_recipe_store_is_pick (store, recipe)) {
                        tile = gr_recipe_tile_new (recipe);
                        gtk_widget_show (tile);
                        gtk_container_add (GTK_CONTAINER (self->pick_box), tile);
                }
        }
}

static void
populate_authors_from_store (GrRecipesPage *self)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;

        container_remove_all (GTK_CONTAINER (self->chefs_box));

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keys = gr_recipe_store_get_author_keys (store, &length);
        for (i = 0; i < length; i++) {
                g_autoptr(GrAuthor) author = NULL;

                author = gr_recipe_store_get_author (store, keys[i]);

		if (gr_recipe_store_author_is_featured (store, author)) {
			GtkWidget *tile;
			tile = gr_chef_tile_new (author);
			gtk_widget_show (tile);
			gtk_container_add (GTK_CONTAINER (self->chefs_box), tile);
		}
	}
}

static void
connect_store_signals (GrRecipesPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        /* FIXME: inefficient */
        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (populate_recipes_from_store), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (populate_recipes_from_store), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (populate_recipes_from_store), page);
        g_signal_connect_swapped (store, "authors-changed", G_CALLBACK (populate_authors_from_store), page);
}
