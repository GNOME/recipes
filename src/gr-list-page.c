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
};

G_DEFINE_TYPE (GrListPage, gr_list_page, GTK_TYPE_BOX)

static void connect_store_signals (GrListPage *page);

static void
clear_data (GrListPage *self)
{
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

        G_OBJECT_CLASS (gr_list_page_parent_class)->finalize (object);
}

static void
gr_list_page_init (GrListPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
        connect_store_signals (page);
}

static void
gr_list_page_class_init (GrListPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = list_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-list-page.ui");

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
        const char *label;

	switch (diet) {
        case GR_DIET_GLUTEN_FREE:
                label = _("Gluten-free recipes");
                break;
        case GR_DIET_NUT_FREE:
                label = _("Nut-free recipes");
                break;
        case GR_DIET_VEGAN:
                label = _("Vegan recipes");
                break;
        case GR_DIET_VEGETARIAN:
                label = _("Vegetarian recipes");
                break;
        case GR_DIET_MILK_FREE:
                label = _("Milk-free recipes");
                break;
        default:
                label = _("Other dietary restrictions");
                break;
}

        return label;
}

void
gr_list_page_populate_from_diet (GrListPage *self,
                                 GrDiets     diet)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;
        gboolean filled;
        char *tmp;

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
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "empty");
        filled = FALSE;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keys = gr_recipe_store_get_recipe_keys (store, &length);
        for (i = 0; i < length; i++) {
                g_autoptr(GrRecipe) recipe = NULL;
                GtkWidget *tile;
                GrDiets diets;

        	recipe = gr_recipe_store_get (store, keys[i]);
                diets = gr_recipe_get_diets (recipe);
                if ((diets & diet) == 0)
                        continue;

                tile = gr_recipe_tile_new (recipe);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (self->flow_box), tile);
                filled = TRUE;
        }

        if (filled)
                gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
}

void
gr_list_page_populate_from_chef (GrListPage *self,
                                 GrChef     *chef)
{
        GrRecipeStore *store;
        const char *name;
        g_autofree char **keys = NULL;
        guint length;
        int i;
        gboolean filled;
        char *tmp;
        const char *image_path;
        GtkStyleContext *context;
        g_autofree char *css = NULL;
        g_autoptr(GtkCssProvider) provider = NULL;

        g_object_ref (chef);
        clear_data (self);
        self->chef = chef;

        gtk_widget_show (self->chef_grid);
        gtk_widget_show (self->heading);
        gtk_widget_hide (self->diet_description);

        gtk_label_set_label (GTK_LABEL (self->chef_fullname), gr_chef_get_fullname (chef));
        gtk_label_set_label (GTK_LABEL (self->chef_description), gr_chef_get_description (chef));

        image_path = gr_chef_get_image (chef);

        if (image_path != NULL && image_path[0] != '\0')
                css = g_strdup_printf ("image.chef {\n"
                                       "  background: url('%s');\n"
                                       "  background-size: 64px;\n"
                                       "  min-width: 64px;\n"
                                       "  min-height: 64px;\n"
                                       "}", image_path);
        else
                css = g_strdup_printf ("image.chef {\n"
                                       "  background: rgb(%d,%d,%d);\n"
                                       "  min-width: 64px;\n"
                                       "  min-height: 64px;\n"
                                       "}",
                                       g_random_int_range (0, 255),
                                       g_random_int_range (0, 255),
                                       g_random_int_range (0, 255));

        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (provider, css, -1, NULL);
        context = gtk_widget_get_style_context (self->chef_image);
        gtk_style_context_add_provider (context,
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        tmp = g_strdup_printf (_("Recipes by %s"), gr_chef_get_nickname (chef));
        gtk_label_set_label (GTK_LABEL (self->heading), tmp);
        g_free (tmp);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        container_remove_all (GTK_CONTAINER (self->flow_box));
        tmp = g_strdup_printf (_("No recipes by chef %s found"), gr_chef_get_nickname (chef));
        gtk_label_set_label (GTK_LABEL (self->empty_title), tmp);
        g_free (tmp);
        if (g_strcmp0 (gr_chef_get_name (chef), gr_recipe_store_get_user_key (store)) == 0)
                gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("You could add one using the “New Recipe” button."));
        else
                gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Sorry about this."));
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "empty");
        filled = FALSE;

        name = gr_chef_get_name (chef);

        keys = gr_recipe_store_get_recipe_keys (store, &length);
        for (i = 0; i < length; i++) {
                g_autoptr(GrRecipe) recipe = NULL;
                const char *author;
                GtkWidget *tile;

                recipe = gr_recipe_store_get (store, keys[i]);
                author = gr_recipe_get_author (recipe);

                if (g_strcmp0 (name, author) != 0)
                        continue;

                tile = gr_recipe_tile_new (recipe);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (self->flow_box), tile);

                filled = TRUE;
        }

        if (filled)
                gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
}

void
gr_list_page_populate_from_season (GrListPage *self,
                                   const char *season)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;
        gboolean filled;
        char *tmp;

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
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "empty");
        filled = FALSE;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keys = gr_recipe_store_get_recipe_keys (store, &length);
        for (i = 0; i < length; i++) {
                g_autoptr(GrRecipe) recipe = NULL;
                GtkWidget *tile;

        	recipe = gr_recipe_store_get (store, keys[i]);
                if (g_strcmp0 (self->season, gr_recipe_get_season (recipe)) != 0)
                        continue;

                tile = gr_recipe_tile_new (recipe);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (self->flow_box), tile);
                filled = TRUE;
        }

        if (filled)
                gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
}

void
gr_list_page_populate_from_favorites (GrListPage *self)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;
        gboolean filled;

        clear_data (self);
        self->favorites = TRUE;

        gtk_widget_hide (self->chef_grid);
        gtk_widget_hide (self->heading);
        gtk_widget_hide (self->diet_description);

        container_remove_all (GTK_CONTAINER (self->flow_box));
        gtk_label_set_label (GTK_LABEL (self->empty_title), _("No favorite recipes found"));
        gtk_label_set_label (GTK_LABEL (self->empty_subtitle), _("Use the “Cook later” button to mark recipes as favorites."));
        gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "empty");
        filled = FALSE;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keys = gr_recipe_store_get_recipe_keys (store, &length);
        for (i = 0; i < length; i++) {
                g_autoptr(GrRecipe) recipe = NULL;
                GtkWidget *tile;

                recipe = gr_recipe_store_get (store, keys[i]);
                if (!gr_recipe_store_is_favorite (store, recipe))
                        continue;

                tile = gr_recipe_tile_new (recipe);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (self->flow_box), tile);
                filled = TRUE;
        }

        if (filled)
                gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), "list");
}

static void
list_page_reload (GrListPage *page)
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
        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (list_page_reload), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (list_page_reload), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (list_page_reload), page);
}
