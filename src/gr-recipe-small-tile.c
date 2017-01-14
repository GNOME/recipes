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
        GtkWidget *check;
};

G_DEFINE_TYPE (GrRecipeSmallTile, gr_recipe_small_tile, GTK_TYPE_BUTTON)

enum {
        PROP_0,
        PROP_ACTIVE,
        N_PROPS
};

static void
recipe_small_tile_set_recipe (GrRecipeSmallTile *tile,
                              GrRecipe          *recipe)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        if (tile->recipe) {
                const char *elem;
                elem = gr_recipe_get_id (tile->recipe);
                gtk_style_context_remove_class (gtk_widget_get_style_context (tile->box), elem);
        }

        g_set_object (&tile->recipe, recipe);

        if (tile->recipe) {
                const char *elem;
                const char *name;
                const char *author;
                g_autoptr(GrChef) chef = NULL;
                g_autofree char *tmp = NULL;

                elem = gr_recipe_get_id (tile->recipe);
                gtk_style_context_add_class (gtk_widget_get_style_context (tile->box), elem);
                gtk_style_context_add_class (gtk_widget_get_style_context (tile->image), elem);

                name = gr_recipe_get_translated_name (recipe);
                author = gr_recipe_get_author (recipe);
                chef = gr_recipe_store_get_chef (store, author);

                gtk_label_set_label (GTK_LABEL (tile->label), name);
                tmp = g_strdup_printf (_("by %s"), chef ? gr_chef_get_name (chef) : _("Anonymous"));
                gtk_label_set_label (GTK_LABEL (tile->author), tmp);
        }
}

static void
tile_clicked (GrRecipeSmallTile *tile)
{
        gboolean active;

        active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tile->check));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tile->check), !active);
}

static void
check_active_notify (GrRecipeSmallTile *tile)
{
        g_object_notify (G_OBJECT (tile), "active");
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
}

static void
recipe_small_tile_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GrRecipeSmallTile *self = GR_RECIPE_SMALL_TILE (object);

        switch (prop_id) {
        case PROP_ACTIVE:
                g_value_set_boolean (value, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check)));;
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
        case PROP_ACTIVE:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->check), g_value_get_boolean (value));
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

        pspec = g_param_spec_boolean ("active", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_ACTIVE, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-recipe-small-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, author);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, image);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, box);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeSmallTile, check);

        gtk_widget_class_bind_template_callback (widget_class, tile_clicked);
        gtk_widget_class_bind_template_callback (widget_class, check_active_notify);
}

GtkWidget *
gr_recipe_small_tile_new (GrRecipe *recipe)
{
        GrRecipeSmallTile *tile;

        tile = g_object_new (GR_TYPE_RECIPE_SMALL_TILE, NULL);
        recipe_small_tile_set_recipe (GR_RECIPE_SMALL_TILE (tile), recipe);

        return GTK_WIDGET (tile);
}

GrRecipe *
gr_recipe_small_tile_get_recipe (GrRecipeSmallTile *tile)
{
        return tile->recipe;
}
