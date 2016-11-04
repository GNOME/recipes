/* gr-big-cuisine-tile.c:
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

#include "gr-big-cuisine-tile.h"
#include "gr-cuisine.h"
#include "gr-window.h"
#include "gr-utils.h"


struct _GrBigCuisineTile
{
        GtkButton parent_instance;

	char *cuisine;

        GtkWidget *title;
        GtkWidget *subtitle;
};

G_DEFINE_TYPE (GrBigCuisineTile, gr_big_cuisine_tile, GTK_TYPE_BUTTON)

static void
show_details (GrBigCuisineTile *tile)
{
        GtkWidget *window;
        const char *title;

        title = gtk_label_get_label (GTK_LABEL (tile->title));

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);
        gr_window_show_cuisine (GR_WINDOW (window), tile->cuisine, title);
}

static void
big_cuisine_tile_set_cuisine (GrBigCuisineTile *tile,
                              const char    *cuisine)
{
        const char *title;
        const char *description;
        GtkStyleContext *context;

        g_free (tile->cuisine);
        tile->cuisine = g_strdup (cuisine);

        gr_cuisine_get_data (cuisine, &title, &description);
        if (title == NULL)
                return;

        gtk_label_set_label (GTK_LABEL (tile->title), title);
        gtk_label_set_label (GTK_LABEL (tile->subtitle), description);

        context = gtk_widget_get_style_context (GTK_WIDGET (tile));
        gtk_style_context_add_class (context, cuisine);
        context = gtk_widget_get_style_context (GTK_WIDGET (tile->title));
        gtk_style_context_add_class (context, cuisine);
        context = gtk_widget_get_style_context (GTK_WIDGET (tile->subtitle));
        gtk_style_context_add_class (context, cuisine);
}

static void
big_cuisine_tile_finalize (GObject *object)
{
        GrBigCuisineTile *tile = GR_BIG_CUISINE_TILE (object);

        g_clear_pointer (&tile->cuisine, g_free);

        G_OBJECT_CLASS (gr_big_cuisine_tile_parent_class)->finalize (object);
}

static void
gr_big_cuisine_tile_init (GrBigCuisineTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));

	tile->cuisine = NULL;
}

static void
gr_big_cuisine_tile_class_init (GrBigCuisineTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = big_cuisine_tile_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-big-cuisine-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrBigCuisineTile, title);
        gtk_widget_class_bind_template_child (widget_class, GrBigCuisineTile, subtitle);

        gtk_widget_class_bind_template_callback (widget_class, show_details);
}

GtkWidget *
gr_big_cuisine_tile_new (const char *cuisine)
{
        GrBigCuisineTile *tile;

        tile = g_object_new (GR_TYPE_BIG_CUISINE_TILE, NULL);
        big_cuisine_tile_set_cuisine (GR_BIG_CUISINE_TILE (tile), cuisine);

        return GTK_WIDGET (tile);
}
