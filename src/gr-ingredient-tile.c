/* gr-ingredient-tile.c:
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

#include "gr-ingredient-tile.h"
#include "gr-ingredient.h"
#include "gr-utils.h"


struct _GrIngredientTile
{
        GtkBox parent_instance;

        char *ingredient;

        GtkWidget *label;
        GtkWidget *image;
};


G_DEFINE_TYPE (GrIngredientTile, gr_ingredient_tile, GTK_TYPE_BOX)

static void
ingredient_tile_finalize (GObject *object)
{
        GrIngredientTile *tile = GR_INGREDIENT_TILE (object);

        g_clear_pointer(&tile->ingredient, g_free);

        G_OBJECT_CLASS (gr_ingredient_tile_parent_class)->finalize (object);
}

static void
gr_ingredient_tile_init (GrIngredientTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));
}

static void
gr_ingredient_tile_class_init (GrIngredientTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = ingredient_tile_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredient-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrIngredientTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientTile, image);
}

static void
ingredient_tile_set_ingredient (GrIngredientTile *tile, const char *ingredient)
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

        gtk_label_set_label (GTK_LABEL (tile->label), ing);

        if (image_path != NULL &&
            g_file_test (image_path, G_FILE_TEST_EXISTS))
                css = g_strdup_printf ("image.ingredient {\n"
                                       "  background: url('%s');\n"
                                       "  background-size: 100%;\n"
                                       "  min-width: 128px;\n"
                                       "  min-height: 64px;\n"
                                       "}", image_path);
        else
                css = g_strdup_printf ("image.ingredient {\n"
                                       "  background: rgb(%d,%d,%d);\n"
                                       "  min-width: 128px;\n"
                                       "  min-height: 64px;\n"
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
gr_ingredient_tile_new (const char *ingredient)
{
        GrIngredientTile *tile;

        tile = g_object_new (GR_TYPE_INGREDIENT_TILE, NULL);
        ingredient_tile_set_ingredient (tile, ingredient);

        return GTK_WIDGET (tile);
}

const char *
gr_ingredient_tile_get_ingredient (GrIngredientTile *tile)
{
        return tile->ingredient;
}
