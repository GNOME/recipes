/* gr-cuisine-tile.c:
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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gr-cuisine-tile.h"
#include "gr-window.h"
#include "gr-utils.h"

struct _GrCuisineTile
{
        GtkButton parent_instance;

	char *cuisine;

        GtkWidget *title;
        GtkWidget *description;
        GtkWidget *description2;
};

G_DEFINE_TYPE (GrCuisineTile, gr_cuisine_tile, GTK_TYPE_BUTTON)

static void
show_details (GrCuisineTile *tile)
{
        GtkWidget *window;
        const char *title;

        title = gtk_label_get_label (GTK_LABEL (tile->title));

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);
        gr_window_show_cuisine (GR_WINDOW (window), tile->cuisine, title);
}

typedef struct {
        const char *name;
        const char *title;
        const char *description;
        const char *image_path;
} Cuisine;

static Cuisine cuisines[] = {
        { "american", N_("American"), N_("American cuisine has burgers"), NULL },
        { "chinese", N_("Chinese"), N_("Good stuff"), NULL },
        { "indian", N_("Indian"), N_("Spice stuff"), NULL },
        { "italian", N_("Italian"), N_("Pizza, pasta, pesto - we love it all. Top it off with an esspresso and a gelato. Perfect!"), NULL },
        { "french", N_("French"), N_("Yep"), NULL },
        { "mexican", N_("Mexican"), N_("Tacos"), NULL },
        { "turkish", N_("Turkish"), N_("Yummy"), NULL },
        { "mediterranean", N_("Mediterranean"), N_("The mediterranean quisine has a lot to offer, and is legendary for being very healthy too. Expect to see olives, yoghurt and garlic."), NULL },
};


static void
cuisine_tile_set_cuisine (GrCuisineTile *tile,
                          const char    *cuisine)
{
        Cuisine *c = NULL;
        const char *path;
        int i;

	g_free (tile->cuisine);
	tile->cuisine = g_strdup (cuisine);

        for (i = 0; i < G_N_ELEMENTS(cuisines); i++) {
                if (g_strcmp0 (cuisine, cuisines[i].name) == 0) {
                        c = &cuisines[i];
                        break;
                }
        }

        if (c == NULL)
                return;

        gtk_label_set_label (GTK_LABEL (tile->title), _(c->title));
        gtk_label_set_label (GTK_LABEL (tile->description), _(c->description));
        gtk_label_set_label (GTK_LABEL (tile->description2), _(c->description));

        path = c->image_path;
        if (path == NULL)
                path = "resource:/org/gnome/Recipes/italian.png";

        if (path) {
	        GtkStyleContext *context;
        	g_autofree char *css = NULL;
	        g_autoptr(GtkCssProvider) provider = NULL;
		gsize length;

                css = g_strdup_printf ("button.cuisine {\n"
                                       "  background: url('%s');\n"
                                       "  background-repeat: no-repeat;\n"
                                       "  background-size: 100%;\n"
                                       "}", path);
        	provider = gtk_css_provider_new ();
	        gtk_css_provider_load_from_data (provider, css, -1, NULL);
        	context = gtk_widget_get_style_context (GTK_WIDGET (tile));
	        gtk_style_context_add_provider (context,
        	                                GTK_STYLE_PROVIDER (provider),
                	                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
}

static void
cuisine_tile_finalize (GObject *object)
{
        GrCuisineTile *tile = GR_CUISINE_TILE (object);

        g_clear_pointer (&tile->cuisine, g_free);

        G_OBJECT_CLASS (gr_cuisine_tile_parent_class)->finalize (object);
}

static void
gr_cuisine_tile_init (GrCuisineTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));

	tile->cuisine = NULL;
}

static void
gr_cuisine_tile_class_init (GrCuisineTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = cuisine_tile_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-cuisine-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrCuisineTile, title);
        gtk_widget_class_bind_template_child (widget_class, GrCuisineTile, description);
        gtk_widget_class_bind_template_child (widget_class, GrCuisineTile, description2);

        gtk_widget_class_bind_template_callback (widget_class, show_details);
}

GtkWidget *
gr_cuisine_tile_new (const char *cuisine)
{
        GrCuisineTile *tile;

        tile = g_object_new (GR_TYPE_CUISINE_TILE, NULL);
        cuisine_tile_set_cuisine (GR_CUISINE_TILE (tile), cuisine);

        return GTK_WIDGET (tile);
}
