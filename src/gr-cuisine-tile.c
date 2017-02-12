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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-cuisine-tile.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-cuisine.h"


struct _GrCuisineTile
{
        GtkButton parent_instance;

        char *cuisine;

        GtkWidget *title;
        GtkWidget *description;
};

G_DEFINE_TYPE (GrCuisineTile, gr_cuisine_tile, GTK_TYPE_BUTTON)

static void
show_details (GrCuisineTile *tile)
{
        GtkWidget *window;
        const char *title;

        gr_cuisine_get_data (tile->cuisine, NULL, &title, NULL);

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);
        gr_window_show_cuisine (GR_WINDOW (window), tile->cuisine, title);
}

static void
cuisine_tile_set_cuisine (GrCuisineTile *tile,
                          const char    *cuisine,
                          gboolean       big)
{
        const char *title;
        const char *description;
        GtkStyleContext *context;

        g_free (tile->cuisine);
        tile->cuisine = g_strdup (cuisine);

        gr_cuisine_get_data (cuisine, &title, NULL, &description);
        if (title == NULL)
                return;

        gtk_label_set_label (GTK_LABEL (tile->title), title);
        gtk_label_set_label (GTK_LABEL (tile->description), description);
        if (big) {
                gtk_label_set_lines (GTK_LABEL (tile->description), 3);
                gtk_label_set_width_chars (GTK_LABEL (tile->description), 85);
                gtk_label_set_max_width_chars (GTK_LABEL (tile->description), 85);
        }

        context = gtk_widget_get_style_context (GTK_WIDGET (tile));
        gtk_style_context_add_class (context, cuisine);
        if (big)
                gtk_style_context_add_class (context, "big");
        context = gtk_widget_get_style_context (GTK_WIDGET (tile->title));
        gtk_style_context_add_class (context, cuisine);
        if (big)
                gtk_style_context_add_class (context, "big");
        context = gtk_widget_get_style_context (GTK_WIDGET (tile->description));
        gtk_style_context_add_class (context, cuisine);
        if (big)
                gtk_style_context_add_class (context, "big");
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

        gtk_widget_class_bind_template_callback (widget_class, show_details);
}

GtkWidget *
gr_cuisine_tile_new (const char *cuisine,
                     gboolean    big)
{
        GrCuisineTile *tile;

        tile = g_object_new (GR_TYPE_CUISINE_TILE, NULL);
        cuisine_tile_set_cuisine (GR_CUISINE_TILE (tile), cuisine, big);

        return GTK_WIDGET (tile);
}
