/* gr-recipe-tile.c:
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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gr-recipe-tile.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-images.h"


struct _GrRecipeTile
{
        GtkButton parent_instance;

        GrRecipe *recipe;

        gboolean wide;

        GtkWidget *label;
        GtkWidget *author;
        GtkWidget *image;
        GtkWidget *box;
};

G_DEFINE_TYPE (GrRecipeTile, gr_recipe_tile, GTK_TYPE_BUTTON)

static void
show_details (GrRecipeTile *tile)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);
        gr_window_show_recipe (GR_WINDOW (window), tile->recipe);
}

static void
add_recipe_css (GrRecipe *recipe,
                GString  *css)
{
        const char *id;
        g_autoptr(GArray) images = NULL;

        id = gr_recipe_get_id (recipe);

        g_object_get (recipe, "images", &images, NULL);

        if (images->len > 0) {
                GrImage *ri;
                int index;

                index = gr_recipe_get_default_image (recipe);
                if (index < 0 || index >= images->len)
                        index = 0;

                ri = &g_array_index (images, GrImage, index);

                g_string_append_printf (css, "image.recipe.small.%s,\nbox.recipe.%s {\n", id, id);
                g_string_append_printf (css, "  background: url('%s');\n"
                                             "  background-size: cover;\n"
                                             "  background-position: center;\n", ri->path);
                g_string_append (css, "}\n\n");
        }
}

static GtkCssProvider *provider = NULL;

void
gr_recipe_tile_recreate_css (void)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        g_autoptr(GString) css = NULL;
        int i;

return;
        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        keys = gr_recipe_store_get_recipe_keys (store, &length);

        css = g_string_new ("");

        for (i = 0; i < length; i++) {
                g_autoptr (GrRecipe) recipe = NULL;
                recipe = gr_recipe_store_get_recipe (store, keys[i]);
                add_recipe_css (recipe, css);
        }

        if (provider == NULL) {
                provider = gtk_css_provider_new ();
                gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                           GTK_STYLE_PROVIDER (provider),
                                                           GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }

        gtk_css_provider_load_from_data (provider, css->str, css->len, NULL);
}

static void
recipe_tile_set_recipe (GrRecipeTile *tile,
                        GrRecipe     *recipe)
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
                g_autoptr(GArray) images = NULL;

                elem = gr_recipe_get_id (tile->recipe);
                gtk_style_context_add_class (gtk_widget_get_style_context (tile->box), elem);

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
                        int width;
                        int height;

                        width = tile->wide ? 538 : 258;
                        height = 200;

                        index = gr_recipe_get_default_image (recipe);
                        if (index < 0 || index >= images->len)
                                index = 0;

                        ri = &g_array_index (images, GrImage, index);
                        pixbuf = load_pixbuf_fill_size (ri->path, width, height);
                        gtk_image_set_from_pixbuf (GTK_IMAGE (tile->image), pixbuf);
                }
        }
}

static void
recipe_tile_finalize (GObject *object)
{
        GrRecipeTile *tile = GR_RECIPE_TILE (object);

        g_clear_object (&tile->recipe);

        G_OBJECT_CLASS (gr_recipe_tile_parent_class)->finalize (object);
}

static void
gr_recipe_tile_init (GrRecipeTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));
        tile->wide = FALSE;
}

static void
gr_recipe_tile_class_init (GrRecipeTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = recipe_tile_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-recipe-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrRecipeTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeTile, author);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeTile, image);
        gtk_widget_class_bind_template_child (widget_class, GrRecipeTile, box);

        gtk_widget_class_bind_template_callback (widget_class, show_details);
}

GtkWidget *
gr_recipe_tile_new (GrRecipe *recipe)
{
        GrRecipeTile *tile;

        tile = g_object_new (GR_TYPE_RECIPE_TILE, NULL);
        recipe_tile_set_recipe (GR_RECIPE_TILE (tile), recipe);

        return GTK_WIDGET (tile);
}

GtkWidget *
gr_recipe_tile_new_wide (GrRecipe *recipe)
{
        GrRecipeTile *tile;

        tile = g_object_new (GR_TYPE_RECIPE_TILE, NULL);
        tile->wide = TRUE;

        recipe_tile_set_recipe (GR_RECIPE_TILE (tile), recipe);

        return GTK_WIDGET (tile);
}

GrRecipe *
gr_recipe_tile_get_recipe (GrRecipeTile *tile)
{
        return tile->recipe;
}
