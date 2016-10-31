/* gr-cuisines-page.c:
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

#include "gr-recipe.h"
#include "gr-cuisines-page.h"
#include "gr-recipe-store.h"
#include "gr-app.h"
#include "gr-utils.h"
#include "gr-cuisine-tile.h"


struct _GrCuisinesPage
{
        GtkBox        parent_instance;

        GtkWidget *top_tile;
        GtkWidget *flow_box;
};

G_DEFINE_TYPE (GrCuisinesPage, gr_cuisines_page, GTK_TYPE_BOX)

static void
cuisines_page_finalize (GObject *object)
{
        G_OBJECT_CLASS (gr_cuisines_page_parent_class)->finalize (object);
}

static void
gr_cuisines_page_init (GrCuisinesPage *page)
{
	GtkWidget *tile;
        const char *cuisines[] = {
                "american",
                "chinese",
                "indian",
                "italian",
                "french",
                "mexican",
                "mediterranean",
                "turkish"
        };
        int i;

        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));

        for (i = 0; i < G_N_ELEMENTS(cuisines); i++) {
	        tile = gr_cuisine_tile_new (cuisines[i]);
	        gtk_widget_show (tile);
	        gtk_container_add (GTK_CONTAINER (page->flow_box), tile);
        }
}

static void
gr_cuisines_page_class_init (GrCuisinesPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = cuisines_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-cuisines-page.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrCuisinesPage, top_tile);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrCuisinesPage, flow_box);
}

GtkWidget *
gr_cuisines_page_new (void)
{
        GrCuisinesPage *page;

        page = g_object_new (GR_TYPE_CUISINES_PAGE, NULL);

        return GTK_WIDGET (page);
}
