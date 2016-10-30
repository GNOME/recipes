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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-category-tile.h"

struct _GrCategoryTile
{
        GtkButton        parent_instance;

        char *category;
        GtkWidget  *label;
        GtkWidget  *image;
};

G_DEFINE_TYPE (GrCategoryTile, gr_category_tile, GTK_TYPE_BUTTON)

const char *
gr_category_tile_get_category (GrCategoryTile *tile)
{
        return tile->category;
}

void
gr_category_tile_set_category (GrCategoryTile *tile, const char *category)
{
        g_free (tile->category);
        tile->category = g_strdup (category);
        gtk_label_set_label (GTK_LABEL (tile->label), tile->category);
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
gr_category_tile_new (const char *category)
{
        GrCategoryTile *tile;

        tile = g_object_new (GR_TYPE_CATEGORY_TILE, NULL);
        gr_category_tile_set_category (tile, category);

        return GTK_WIDGET (tile);
}
