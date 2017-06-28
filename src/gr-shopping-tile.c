/* gr-shopping-tile.c:
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

#include "gr-shopping-tile.h"
#include "gr-recipe-store.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-image.h"


struct _GrShoppingTile
{
        GtkButton parent_instance;

        GrRecipe *recipe;

        GtkWidget *label;
        GtkWidget *author;
        GtkWidget *image;
        GtkWidget *box;
        GtkWidget *yield_label;
        GtkWidget *popover;
        GtkWidget *yield_spin;
        GtkWidget *yield_unit_label;
        GtkWidget *remove_button;

        GCancellable *cancellable;

        double yield;
};

G_DEFINE_TYPE (GrShoppingTile, gr_shopping_tile, GTK_TYPE_BUTTON)

enum {
        PROP_0,
        PROP_YIELD,
        N_PROPS
};

void
gr_shopping_tile_set_yield (GrShoppingTile *tile,
                            double          yield)
{
        g_autofree char *tmp = NULL;

        if (tile->yield == yield)
                return;

        tile->yield = yield;

        tmp = gr_number_format (yield);
        gtk_label_set_label (GTK_LABEL (tile->yield_label), tmp);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (tile->yield_spin), yield);

        g_object_notify (G_OBJECT (tile), "yield");
}

static void
shopping_tile_set_recipe (GrShoppingTile *tile,
                          GrRecipe       *recipe)
{
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        g_cancellable_cancel (tile->cancellable);
        g_clear_object (&tile->cancellable);

        g_set_object (&tile->recipe, recipe);

        if (tile->recipe) {
                const char *name;
                const char *author;
                const char *yield;
                g_autoptr(GrChef) chef = NULL;
                g_autofree char *tmp = NULL;
                GPtrArray *images;

                name = gr_recipe_get_translated_name (recipe);
                author = gr_recipe_get_author (recipe);
                yield = gr_recipe_get_yield_unit (recipe);
                chef = gr_recipe_store_get_chef (store, author);

                gtk_label_set_label (GTK_LABEL (tile->label), name);
                tmp = g_strdup_printf (_("by %s"), chef ? gr_chef_get_name (chef) : _("Anonymous"));
                gtk_label_set_label (GTK_LABEL (tile->author), tmp);
                gtk_label_set_label (GTK_LABEL (tile->yield_unit_label), yield && yield[0] ? yield : _("servings"));

                images = gr_recipe_get_images (recipe);
                if (images->len > 0) {
                        int index;
                        GrImage *ri;

                        tile->cancellable = g_cancellable_new ();

                        index = gr_recipe_get_default_image (recipe);
                        if (index < 0 || index >= images->len)
                                index = 0;

                        ri = g_ptr_array_index (images, index);
                        gr_image_load (ri, 64, 64, FALSE, tile->cancellable, gr_image_set_pixbuf, tile->image);
                }

        }
}

static void
tile_clicked (GrShoppingTile *tile)
{
        gtk_popover_popup (GTK_POPOVER (tile->popover));
}

static void
yield_spin_value_changed (GrShoppingTile *tile)
{
        double yield;

        yield = gtk_spin_button_get_value (GTK_SPIN_BUTTON (tile->yield_spin));
        gr_shopping_tile_set_yield (tile, yield);
}

static int
yield_spin_input (GtkSpinButton *spin,
                  double        *new_val)
{
        char *text;

        text = (char *)gtk_entry_get_text (GTK_ENTRY (spin));

        if (!gr_number_parse (new_val, &text, NULL)) {
                *new_val = 0.0;
                return GTK_INPUT_ERROR;
        }

        return TRUE;
}

static int
yield_spin_output (GtkSpinButton *spin)
{
        GtkAdjustment *adj;
        g_autofree char *text = NULL;

        adj = gtk_spin_button_get_adjustment (spin);
        text = gr_number_format (gtk_adjustment_get_value (adj));
        gtk_entry_set_text (GTK_ENTRY (spin), text);

        return TRUE;
}

static void
remove_recipe (GrShoppingTile *tile)
{
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        gr_recipe_store_remove_from_shopping (store, tile->recipe);
}

static void
shopping_tile_finalize (GObject *object)
{
        GrShoppingTile *tile = GR_SHOPPING_TILE (object);

        g_cancellable_cancel (tile->cancellable);
        g_clear_object (&tile->cancellable);
        g_clear_object (&tile->recipe);

        G_OBJECT_CLASS (gr_shopping_tile_parent_class)->finalize (object);
}

static void
gr_shopping_tile_init (GrShoppingTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));
        gr_shopping_tile_set_yield (tile, 1.0);
}

static void
shopping_tile_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
        GrShoppingTile *self = GR_SHOPPING_TILE (object);

        switch (prop_id) {
        case PROP_YIELD:
                g_value_set_double (value, self->yield);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
shopping_tile_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
        GrShoppingTile *self = GR_SHOPPING_TILE (object);

        switch (prop_id) {
        case PROP_YIELD:
                gr_shopping_tile_set_yield (self, g_value_get_double (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gr_shopping_tile_class_init (GrShoppingTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = shopping_tile_finalize;
        object_class->get_property = shopping_tile_get_property;
        object_class->set_property = shopping_tile_set_property;

        pspec = g_param_spec_double ("yield", NULL, NULL,
                                     0.0, G_MAXDOUBLE, 1.0,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_YIELD, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-shopping-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, author);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, image);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, box);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, yield_label);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, popover);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, yield_spin);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, yield_unit_label);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingTile, remove_button);

        gtk_widget_class_bind_template_callback (widget_class, tile_clicked);
        gtk_widget_class_bind_template_callback (widget_class, yield_spin_value_changed);
        gtk_widget_class_bind_template_callback (widget_class, yield_spin_input);
        gtk_widget_class_bind_template_callback (widget_class, yield_spin_output);
        gtk_widget_class_bind_template_callback (widget_class, remove_recipe);
}

GtkWidget *
gr_shopping_tile_new (GrRecipe *recipe,
                      double    yield)
{
        GrShoppingTile *tile;

        tile = g_object_new (GR_TYPE_SHOPPING_TILE, NULL);
        shopping_tile_set_recipe (tile, recipe);
        gr_shopping_tile_set_yield (tile, yield);

        return GTK_WIDGET (tile);
}

GrRecipe *
gr_shopping_tile_get_recipe (GrShoppingTile *tile)
{
        return tile->recipe;
}

double
gr_shopping_tile_get_yield (GrShoppingTile *tile)
{
        return tile->yield;
}
