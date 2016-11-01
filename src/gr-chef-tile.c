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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-chef-tile.h"
#include "gr-utils.h"
#include "gr-window.h"

struct _GrChefTile
{
        GtkBox      parent_instance;

	GrChef     *chef;

        GtkWidget  *label;
        GtkWidget  *button;
};


G_DEFINE_TYPE (GrChefTile, gr_chef_tile, GTK_TYPE_BOX)

static void
show_list (GrChefTile *tile)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);
        gr_window_show_chef (GR_WINDOW (window), tile->chef);
}

static void
chef_tile_finalize (GObject *object)
{
        GrChefTile *tile = GR_CHEF_TILE (object);

	g_clear_object (&tile->chef);

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
        GParamSpec *pspec;

        object_class->finalize = chef_tile_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-chef-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrChefTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrChefTile, button);

        gtk_widget_class_bind_template_callback (widget_class, show_list);
}

static void
chef_tile_set_chef (GrChefTile *tile, GrChef *chef)
{
	g_autofree char *name = NULL;
        g_autofree char *image_path = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	GtkStyleContext *context;
	g_autofree char *css = NULL;
	g_autoptr(GtkCssProvider) provider = NULL;

	g_set_object (&tile->chef, chef);

	g_object_get (chef,
                      "name", &name,
                      "image-path", &image_path,
                      NULL);

	gtk_label_set_label (GTK_LABEL (tile->label), name);

	if (image_path != NULL && image_path[0] != '\0')
	  	css = g_strdup_printf ("button.chef {\n"
                	               "  background: url('%s');\n"
                        	       "  background-size: 64px;\n"
                        	       "  min-width: 64px;\n"
                        	       "  min-height: 64px;\n"
                               	       "}", image_path);
	else
		css = g_strdup_printf ("button.chef {\n"
				       "  background: rgb(%d,%d,%d);\n"
                        	       "  min-width: 64px;\n"
                        	       "  min-height: 64px;\n"
                               	       "}", 
                                       g_random_int_range (0, 255), 
                                       g_random_int_range (0, 255), 
                                       g_random_int_range (0, 255));

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (provider, css, -1, NULL);
	context = gtk_widget_get_style_context (tile->button);
	gtk_style_context_add_provider (context,
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

GtkWidget *
gr_chef_tile_new (GrChef *chef)
{
        GrChefTile *tile;

        tile = g_object_new (GR_TYPE_CHEF_TILE, NULL);
	chef_tile_set_chef (tile, chef);

        return GTK_WIDGET (tile);
}
