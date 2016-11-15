/* gr-ingredient-search-tile.c:
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

#include "gr-ingredient-search-tile.h"
#include "gr-ingredient.h"
#include "gr-utils.h"


struct _GrIngredientSearchTile
{
        GtkBox parent_instance;

        char *ingredient;
        gboolean exclude;

        GtkWidget *image;
        GtkWidget *button;
};

enum {
        REMOVE_TILE,
        TILE_CHANGED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GrIngredientSearchTile, gr_ingredient_search_tile, GTK_TYPE_BOX)

static void
toggle_exclusion (GtkButton *button, GrIngredientSearchTile *tile)
{
        tile->exclude = !tile->exclude;

        if (tile->exclude)
                gtk_button_set_label (button, gr_ingredient_get_negation (tile->ingredient));
        else
                gtk_button_set_label (button, tile->ingredient);

        g_signal_emit (tile, signals[TILE_CHANGED], 0);
}

static void
remove_tile (GtkButton *button, GrIngredientSearchTile *tile)
{
        g_signal_emit (tile, signals[REMOVE_TILE], 0);
}

static void
ingredient_search_tile_finalize (GObject *object)
{
        GrIngredientSearchTile *tile = GR_INGREDIENT_SEARCH_TILE (object);

        g_clear_pointer (&tile->ingredient, g_free);

        G_OBJECT_CLASS (gr_ingredient_search_tile_parent_class)->finalize (object);
}

static void
gr_ingredient_search_tile_init (GrIngredientSearchTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));
}

static void
gr_ingredient_search_tile_class_init (GrIngredientSearchTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = ingredient_search_tile_finalize;

        signals[REMOVE_TILE] = g_signal_new ("remove-tile",
                                        G_TYPE_FROM_CLASS (object_class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 0);

        signals[TILE_CHANGED] = g_signal_new ("tile-changed",
                                         G_TYPE_FROM_CLASS (object_class),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE, 0);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredient-search-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrIngredientSearchTile, button);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientSearchTile, image);

        gtk_widget_class_bind_template_callback (widget_class, toggle_exclusion);
        gtk_widget_class_bind_template_callback (widget_class, remove_tile);
}

static void
ingredient_search_tile_set_ingredient (GrIngredientSearchTile *tile, const char *ingredient)
{
        const char *ing;
        g_autofree char *image_path = NULL;
        GtkStyleContext *context;
        g_autofree char *css = NULL;
        g_autoptr(GtkCssProvider) provider = NULL;

        ing = gr_ingredient_find (ingredient);
        if (ing == NULL) {
                ing = ingredient;
                image_path = NULL;
        }
        else {
                image_path = gr_ingredient_get_image (ing);
        }

        g_free (tile->ingredient);
        tile->ingredient = g_strdup (ing);

        gtk_button_set_label (GTK_BUTTON (tile->button), ingredient);

        if (image_path != NULL &&
            g_file_test (image_path, G_FILE_TEST_EXISTS))
                css = g_strdup_printf ("image.ingredient {\n"
                	               "  background: url('%s');\n"
                        	       "  background-size: 100%;\n"
                        	       "  min-width: 96px;\n"
                        	       "  min-height: 48px;\n"
                               	       "}", image_path);
        else
                css = g_strdup_printf ("image.ingredient {\n"
                                       "  background: rgb(%d,%d,%d);\n"
                        	       "  min-width: 96px;\n"
                        	       "  min-height: 48px;\n"
                               	       "}",
                                       g_random_int_range (0, 255),
                                       g_random_int_range (0, 255),
                                       g_random_int_range (0, 255));

        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (provider, css, -1, NULL);
        context = gtk_widget_get_style_context (tile->image);
        gtk_style_context_add_provider (context,
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

GtkWidget *
gr_ingredient_search_tile_new (const char *ingredient)
{
        GrIngredientSearchTile *tile;

        tile = g_object_new (GR_TYPE_INGREDIENT_SEARCH_TILE, NULL);
        ingredient_search_tile_set_ingredient (tile, ingredient);

        return GTK_WIDGET (tile);
}

const char *
gr_ingredient_search_tile_get_ingredient (GrIngredientSearchTile *tile)
{
        return tile->ingredient;
}

gboolean
gr_ingredient_search_tile_get_excluded (GrIngredientSearchTile *tile)
{
        return tile->exclude;
}
