/* gr-recipe-small-tile.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gr-recipe-small-tile.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-images.h"


struct _GrRecipeSmallTile
{
        GtkButton parent_instance;

        GrRecipe *recipe;

        GtkWidget *label;
        GtkWidget *author;
        GtkWidget *image;
        GtkWidget *box;
        GtkWidget *serves_label;
        GtkWidget *popover;
        GtkWidget *serves_spin;
        GtkWidget *remove_button;

        int serves;
};

G_DEFINE_TYPE (GrRecipeSmallTile, gr_recipe_small_tile, GTK_TYPE_BUTTON)

enum {
        PROP_0,
        PROP_SERVES,
        N_PROPS
};

void
gr_recipe_small_tile_set_serves (GrRecipeSmallTile *tile,
                                 int                serves)
{
        g_autofree char *tmp = NULL;

        if (tile->serves == serves)
                return;

        tile->serves = serves;

        tmp = g_strdup_printf ("%d", serves);
        gtk_label_set_label (GTK_LABEL (tile->serves_label), tmp);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (tile->serves_spin), serves);

        g_object_notify (G_OBJECT (tile), "serves");
}

static void
recipe_small_tile_set_recipe (GrRecipeSmallTile *tile,
                              GrRecipe          *recipe)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        g_set_object (&tile->recipe, recipe);

        if (tile->recipe) {
                const char *name;
                const char *author;
                g_autoptr(GrChef) chef = NULL;
                g_autofree char *tmp = NULL;
                g_autoptr(GArray) images = NULL;

                name = gr_recipe_get_translated_name (recipe);
                author = gr_recipe_get_author (recipe);
                chef = gr_recipe_store_get_chef (store, author);

                gtk_label_set_label (GTK_LABEL (tile->label), name);
                tmp = g_strdup_printf (_("by %s"), chef ? gr_chef_get_name (chef) : _("Anonymous"));
                gtk_label_set_label (GTK_LABEL (tile->author), tmp);

                g_object_get (recipe, "images", &images, NULL);
                if (images->len > 0) {
                        int index;
                        GrImage *ri;
                        g_autoptr(GdkPixbuf) pixbuf = NULL;

                        index = gr_recipe_get_default_image (recipe);
                        if (index < 0 || index >= images->len)
                                index = 0;

                        ri = &g_array_index (images, GrImage, index);
                        pixbuf = load_pixbuf_fill_size (ri->path, 64, 64);
                        gtk_image_set_from_pixbuf (GTK_IMAGE (tile->image), pixbuf);
                }

        }
}

static void
tile_clicked (GrRecipeSmallTile *tile)
{
        gtk_popover_popup (GTK_POPOVER (tile->popover));
}

static void
serves_value_changed (GrRecipeSmallTile *tile)
{
        int serves;

        serves = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (tile->serves_spin));
        gr_recipe_small_tile_set_serves (tile, serves);
}

static void
remove_recipe (GrRecipeSmallTile *tile)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        gr_recipe_store_remove_from_shopping (store, tile->recipe);
}

static void
recipe_small_tile_finalize (GObject *object)
{
        GrRecipeSmallTile *tile = GR_RECIPE_SMALL_TILE (object);

        g_clear_object (&tile->recipe);

        G_OBJECT_CLASS (gr_recipe_small_tile_parent_class)->finalize (object);
}

static void
gr_recipe_small_tile_init (GrRecipeSmallTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));
        gr_recipe_small_tile_set_serves (tile, 1);
}

static void
recipe_small_tile_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GrRecipeSmallTile *self = GR_RECIPE_SMALL_TILE (object);

        switch (prop_id) {
        case PROP_SERVES:
                g_value_set_int (value, self->serves);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
recipe_small_tile_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GrRecipeSmallTile *self = GR_RECIPE_SMALL_TILE (object);

        switch (prop_id) {
        case PROP_SERVES:
                gr_recipe_small_tile_set_serves (self, g_value_get_int (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gr_recipe_small_tile_class_init (GrRecipeSmallTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = recipe_small_tile_finalize;
        object_class->get_property = recipe_small_tile_get_property;
        object_class->set_property = recipe_small_tile_set_property;

        pspec = g_param_spec_int ("serves", NULL, NULL,
                                  0, G_MAXINT, 1,
                                  G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_SERVES, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-recipe-small-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, author);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, image);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, box);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, serves_label);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, popover);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, serves_spin);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, remove_button);

        gtk_widget_class_bind_template_callback (widget_class, tile_clicked);
        gtk_widget_class_bind_template_callback (widget_class, serves_value_changed);
        gtk_widget_class_bind_template_callback (widget_class, remove_recipe);
}

GtkWidget *
gr_recipe_small_tile_new (GrRecipe *recipe,
                          int       serves)
{
        GrRecipeSmallTile *tile;

        tile = g_object_new (GR_TYPE_RECIPE_SMALL_TILE, NULL);
        recipe_small_tile_set_recipe (tile, recipe);
        gr_recipe_small_tile_set_serves (tile, serves);

        return GTK_WIDGET (tile);
}

GrRecipe *
gr_recipe_small_tile_get_recipe (GrRecipeSmallTile *tile)
{
        return tile->recipe;
}

int
gr_recipe_small_tile_get_serves (GrRecipeSmallTile *tile)
{
        return tile->serves;
}
