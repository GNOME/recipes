/* gr-chef-tile.c:
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

#include "gr-chef-tile.h"
#include "gr-utils.h"
#include "gr-window.h"
#include "gr-image.h"


struct _GrChefTile
{
        GtkBox parent_instance;

        GrChef *chef;

        GtkWidget *label;
        GtkWidget *image;

        GrImage *ri;
        GCancellable *cancellable;
};


G_DEFINE_TYPE (GrChefTile, gr_chef_tile, GTK_TYPE_BOX)

static void
chef_tile_finalize (GObject *object)
{
        GrChefTile *tile = GR_CHEF_TILE (object);

        g_cancellable_cancel (tile->cancellable);
        g_clear_object (&tile->cancellable);
        g_clear_object (&tile->chef);
        g_clear_object (&tile->ri);

        G_OBJECT_CLASS (gr_chef_tile_parent_class)->finalize (object);
}

static void
gr_chef_tile_init (GrChefTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));
}

static void
gr_chef_tile_class_init (GrChefTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = chef_tile_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-chef-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrChefTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrChefTile, image);
}

void
gr_chef_tile_set_chef (GrChefTile *tile,
                       GrChef     *chef)
{
        g_cancellable_cancel (tile->cancellable);
        g_clear_object (&tile->cancellable);

        g_clear_object (&tile->ri);

        g_set_object (&tile->chef, chef);

        if (tile->chef) {
                const char *name;
                const char *path;

                name = gr_chef_get_fullname (chef);
                gtk_label_set_label (GTK_LABEL (tile->label), name);

                path = gr_chef_get_image (chef);
                if (path && path[0]) {
                        tile->ri = gr_image_new (gr_app_get_soup_session (GR_APP (g_application_get_default ())),
                                                 gr_chef_get_id (chef),
                                                 path);
                        tile->cancellable = g_cancellable_new ();

                        gr_image_load (tile->ri, 64, 64, FALSE, tile->cancellable, gr_image_set_pixbuf, tile->image);
                }
        }
}

GtkWidget *
gr_chef_tile_new (GrChef *chef)
{
        GrChefTile *tile;

        tile = g_object_new (GR_TYPE_CHEF_TILE, NULL);
        gr_chef_tile_set_chef (tile, chef);

        return GTK_WIDGET (tile);
}

GrChef *
gr_chef_tile_get_chef (GrChefTile *tile)
{
        return tile->chef;
}
