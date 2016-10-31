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
 * GNU General Public License for more edit.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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


struct _GrListPage
{
        GtkBox     parent_instance;

        GtkWidget *flow_box;
};

G_DEFINE_TYPE (GrListPage, gr_list_page, GTK_TYPE_BOX)

static void
list_page_finalize (GObject *object)
{
        GrListPage *self = GR_LIST_PAGE (object);

        G_OBJECT_CLASS (gr_list_page_parent_class)->finalize (object);
}

static void
gr_list_page_init (GrListPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
}

static void
gr_list_page_class_init (GrListPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = list_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-list-page.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrListPage, flow_box);
}

GtkWidget *
gr_list_page_new (void)
{
        GrListPage *page;

        page = g_object_new (GR_TYPE_LIST_PAGE, NULL);

        return GTK_WIDGET (page);
}

void
gr_list_page_populate_from_diet (GrListPage *self, GrDiets diet)
{
	GrRecipeStore *store;
	g_autofree char **keys = NULL;
	guint length;
	int i;

	container_remove_all (GTK_CONTAINER (self->flow_box));

	store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
	
	keys = gr_recipe_store_get_keys (store, &length);	
	for (i = 0; i < length; i++) {
		g_autoptr(GrRecipe) recipe = NULL;
		GtkWidget *tile;
		GrDiets diets;

		recipe = gr_recipe_store_get (store, keys[i]);
		g_object_get (recipe, "diets", &diets, NULL);
		if ((diets & diet) == 0)
			continue;

		tile = gr_recipe_tile_new (recipe);
		gtk_widget_show (tile);
		gtk_container_add (GTK_CONTAINER (self->flow_box), tile);
	}
}

void
gr_list_page_populate_from_chef (GrListPage *self, GrAuthor *chef)
{
	GrRecipeStore *store;
	g_autofree char *name = NULL;
	g_autofree char **keys = NULL;
	guint length;
	int i;

	container_remove_all (GTK_CONTAINER (self->flow_box));

	g_object_get (chef, "name", &name, NULL);

	store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
	
	keys = gr_recipe_store_get_keys (store, &length);	
	for (i = 0; i < length; i++) {
		g_autoptr(GrRecipe) recipe = NULL;
		g_autofree char *author = NULL;
		GtkWidget *tile;

		recipe = gr_recipe_store_get (store, keys[i]);
		g_object_get (recipe, "author", &author, NULL);
		if (g_strcmp0 (name, author) != 0)
			continue;

		tile = gr_recipe_tile_new (recipe);
		gtk_widget_show (tile);
		gtk_container_add (GTK_CONTAINER (self->flow_box), tile);
	}
}
