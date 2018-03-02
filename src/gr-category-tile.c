/* gr-category-tile.c:
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

#include "gr-category-tile.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-diet.h"


struct _GrCategoryTile
{
        GtkButton parent_instance;

        GrDiets diet;
        char *category;

        GtkWidget *label;
        GtkWidget *image;
};

G_DEFINE_TYPE (GrCategoryTile, gr_category_tile, GTK_TYPE_BUTTON)

static char *
get_category_color (GrDiets diets)
{
        switch (diets) {
        case GR_DIET_GLUTEN_FREE: return g_strdup_printf ("color-tile%d", 0);
        case GR_DIET_NUT_FREE:    return g_strdup_printf ("color-tile%d", 1);
        case GR_DIET_VEGAN:       return g_strdup_printf ("color-tile%d", 2);
        case GR_DIET_VEGETARIAN:  return g_strdup_printf ("color-tile%d", 3);
        case GR_DIET_MILK_FREE:   return g_strdup_printf ("color-tile%d", 4);
        case GR_DIET_HALAL:       return g_strdup_printf ("color-tile%d", 5);
        default:                  return g_strdup_printf ("color-tile%d", 6);
        }
}

static char *
get_category_color_for_label (const char *label)
{
        return g_strdup_printf ("color-tile%d", g_str_hash (label) % 13);
}

static void
category_tile_set_category (GrCategoryTile *tile,
                            GrDiets         diet)
{
        tile->diet = diet;
        gtk_label_set_label (GTK_LABEL (tile->label), gr_diet_get_label (diet));
}

static void
category_tile_finalize (GObject *object)
{
        GrCategoryTile *tile = GR_CATEGORY_TILE (object);

        g_free (tile->category);

        G_OBJECT_CLASS (gr_category_tile_parent_class)->finalize (object);
}

static void
gr_category_tile_init (GrCategoryTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));
        tile->diet = 0;
}

static void
gr_category_tile_class_init (GrCategoryTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = category_tile_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-category-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrCategoryTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrCategoryTile, image);
}

GtkWidget *
gr_category_tile_new (GrDiets diet)
{
        GrCategoryTile *tile;
        g_autofree char *color = NULL;

        tile = g_object_new (GR_TYPE_CATEGORY_TILE, NULL);
        category_tile_set_category (tile, diet);
        color = get_category_color (diet);
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (tile)), color);

        return GTK_WIDGET (tile);
}

GtkWidget *
gr_category_tile_new_with_label (const char *category,
                                 const char *label)
{
        GrCategoryTile *tile;
        g_autofree char *color = NULL;

        tile = g_object_new (GR_TYPE_CATEGORY_TILE, NULL);
        gtk_label_set_label (GTK_LABEL (tile->label), label);
        tile->category = g_strdup (category);

        color = get_category_color_for_label (label);
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (tile)), color);

        return GTK_WIDGET (tile);
}

GrDiets
gr_category_tile_get_diet (GrCategoryTile *tile)
{
        return tile->diet;
}

const char *
gr_category_tile_get_category (GrCategoryTile *tile)
{
        return tile->category;
}

const char *
gr_category_tile_get_label (GrCategoryTile *tile)
{
        return gtk_label_get_label (GTK_LABEL (tile->label));
}
