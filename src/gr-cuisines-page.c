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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-cuisines-page.h"
#include "gr-recipe.h"
#include "gr-recipe-store.h"
#include "gr-utils.h"
#include "gr-cuisine-tile.h"
#include "gr-cuisine.h"
#include "gr-season.h"
#include "gr-category-tile.h"
#include "gr-window.h"


struct _GrCuisinesPage
{
        GtkBox parent_instance;

        GtkWidget *cuisines_box;
        GtkWidget *cuisines_box2;
        GtkWidget *cuisines_more;
        GtkWidget *cuisines_expander_image;
        GtkWidget *seasonal_box;
        GtkWidget *seasonal_box2;
        GtkWidget *seasonal_more;
        GtkWidget *seasonal_expander_image;

        char *featured;
};

G_DEFINE_TYPE (GrCuisinesPage, gr_cuisines_page, GTK_TYPE_BOX)

static void
cuisines_page_finalize (GObject *object)
{
        GrCuisinesPage *page = GR_CUISINES_PAGE (object);

        g_clear_pointer (&page->featured, g_free);

        G_OBJECT_CLASS (gr_cuisines_page_parent_class)->finalize (object);
}

static void
set_cuisines_expanded (GrCuisinesPage *page,
                       gboolean        expanded)
{
        gtk_revealer_set_transition_duration (GTK_REVEALER (page->cuisines_more), expanded ? 250 : 0);

        gtk_revealer_set_reveal_child (GTK_REVEALER (page->cuisines_more), expanded);
        gtk_image_set_from_icon_name (GTK_IMAGE (page->cuisines_expander_image),
                                      expanded ? "pan-up-symbolic" : "pan-down-symbolic", 1);
}

static void
set_seasonal_expanded (GrCuisinesPage *page,
                       gboolean        expanded)
{
        gtk_revealer_set_transition_duration (GTK_REVEALER (page->seasonal_more), expanded ? 250 : 0);

        gtk_revealer_set_reveal_child (GTK_REVEALER (page->seasonal_more), expanded);
        gtk_image_set_from_icon_name (GTK_IMAGE (page->seasonal_expander_image),
                                      expanded ? "pan-up-symbolic" : "pan-down-symbolic", 1);
}

void
gr_cuisines_page_unexpand (GrCuisinesPage *page)
{
        GtkRevealerTransitionType transition;

        transition = gtk_revealer_get_transition_type (GTK_REVEALER (page->seasonal_more));
        gtk_revealer_set_transition_type (GTK_REVEALER (page->seasonal_more),
                                          GTK_REVEALER_TRANSITION_TYPE_NONE);
        set_seasonal_expanded (page, FALSE);
        gtk_revealer_set_transition_type (GTK_REVEALER (page->seasonal_more), transition);

        transition = gtk_revealer_get_transition_type (GTK_REVEALER (page->cuisines_more));
        gtk_revealer_set_transition_type (GTK_REVEALER (page->cuisines_more),
                                          GTK_REVEALER_TRANSITION_TYPE_NONE);
        set_cuisines_expanded (page, FALSE);
        gtk_revealer_set_transition_type (GTK_REVEALER (page->cuisines_more), transition);
}

static void
cuisines_expander_button_clicked (GrCuisinesPage *page)
{
        gboolean expanded;

        expanded = gtk_revealer_get_reveal_child (GTK_REVEALER (page->cuisines_more));
        set_cuisines_expanded (page, !expanded);
}
static void
season_expander_button_clicked (GrCuisinesPage *page)
{
        gboolean expanded;

        expanded = gtk_revealer_get_reveal_child (GTK_REVEALER (page->seasonal_more));
        set_seasonal_expanded (page, !expanded);
}

static void
cuisine_clicked (GrCategoryTile *tile,
                 GrCuisinesPage *page)
{
        GtkWidget *window;
        const char *cuisine;
        const char *title;

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);

        cuisine = gr_category_tile_get_category (tile);
        title = gr_category_tile_get_label (tile);

        gr_window_show_cuisine (GR_WINDOW (window), cuisine, title);
}

static void
populate_cuisines (GrCuisinesPage *page)
{
        GtkWidget *tile;
        const char **all_cuisines;
        g_autofree char **cuisines = NULL;
        int length;
        int i, j;
        GrRecipeStore *store;
        int tiles;
        g_autoptr(GHashTable) seen = NULL;
        g_autofree char **names = NULL;
        guint len;

        container_remove_all (GTK_CONTAINER (page->cuisines_box));
        container_remove_all (GTK_CONTAINER (page->cuisines_box2));

        seen = g_hash_table_new (g_str_hash, g_str_equal);

        store = gr_recipe_store_get ();

        all_cuisines = gr_cuisine_get_names (&length);
        cuisines = g_new0 (char *, g_strv_length ((char **)all_cuisines) + 1);
        for (i = 0, j = 0; all_cuisines[i]; i++) {
                if (!gr_recipe_store_has_cuisine (store, all_cuisines[i]))
                        continue;

                cuisines[j++] = (char *)all_cuisines[i];
        }

        if (page->featured && !g_strv_contains ((const char * const *)cuisines, page->featured))
                g_clear_pointer (&page->featured, g_free);

        length = g_strv_length (cuisines);

        if (length == 0) {
                g_warning ("No cuisines found");
                return;
        }

        if (!page->featured && length > 0) {
                int pos;
                pos = g_random_int_range (0, length);
                page->featured = g_strdup (cuisines[pos]);
        }

        tile = gr_cuisine_tile_new (page->featured, TRUE);
        gtk_widget_show (tile);
        gtk_widget_set_halign (tile, GTK_ALIGN_FILL);
        gtk_grid_attach (GTK_GRID (page->cuisines_box), tile, 0, 0, 2, 1);
        g_hash_table_add (seen, page->featured);

        tiles = 0;
        for (i = 0; i < length; i++) {
                if (strcmp (page->featured, cuisines[i]) == 0)
                        continue;

                if (tiles < 4) {
                        tile = gr_cuisine_tile_new (cuisines[i], FALSE);
                        gtk_widget_show (tile);
                        gtk_widget_set_halign (tile, GTK_ALIGN_FILL);
                        gtk_grid_attach (GTK_GRID (page->cuisines_box), tile, tiles % 2, 1 + tiles / 2, 1, 1);
                }
                else {
                        const char *title;

                        gr_cuisine_get_data (cuisines[i], &title, NULL, NULL);
                        tile = gr_category_tile_new_with_label (cuisines[i], title);
                        gtk_widget_show (tile);
                        g_signal_connect (tile, "clicked", G_CALLBACK (cuisine_clicked), page);
                        gtk_container_add (GTK_CONTAINER (page->cuisines_box2), tile);
                }
                tiles++;

                g_hash_table_add (seen, cuisines[i]);
        }

        names = gr_recipe_store_get_all_cuisines (store, &len);
        for (i = 0; i < len; i++) {
               g_autofree char *tmp = NULL;
               const char *title;

                tmp = g_strstrip (g_strdup (names[i]));
                if (strcmp (tmp, "") == 0)
                        continue;

                if (g_hash_table_contains (seen, tmp))
                        continue;

                gr_cuisine_get_data (tmp, &title, NULL, NULL);

                tile = gr_category_tile_new_with_label (tmp, title);
                gtk_widget_show (tile);
                g_signal_connect (tile, "clicked", G_CALLBACK (cuisine_clicked), page);
                gtk_container_add (GTK_CONTAINER (page->cuisines_box2), tile);
        }
}

static void
seasonal_clicked (GrCategoryTile *tile,
                  GrCuisinesPage *page)
{
        GtkWidget *window;
        const char *season;
        const char *title;

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);

        season = gr_category_tile_get_category (tile);
        title = gr_category_tile_get_label (tile);

        gr_window_show_season (GR_WINDOW (window), season, title);
}

static void
populate_seasonal (GrCuisinesPage *self)
{
        int i;
        GtkWidget *tile;
        const char * const *names;
        int length;

        container_remove_all (GTK_CONTAINER (self->seasonal_box));
        container_remove_all (GTK_CONTAINER (self->seasonal_box2));

        names = gr_season_get_names (&length);
        for (i = 0; i < length; i++) {
                tile = gr_category_tile_new_with_label (names[i], gr_season_get_title (names[i]));
                gtk_widget_show (tile);
                g_signal_connect (tile, "clicked", G_CALLBACK (seasonal_clicked), self);
                if (i < 3)
                        gtk_container_add (GTK_CONTAINER (self->seasonal_box), tile);
                else
                        gtk_container_add (GTK_CONTAINER (self->seasonal_box2), tile);
        }
}

static void connect_store_signals (GrCuisinesPage *page);

static void
gr_cuisines_page_init (GrCuisinesPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
        populate_cuisines (page);
        populate_seasonal (page);
        connect_store_signals (page);
}

static void
gr_cuisines_page_class_init (GrCuisinesPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = cuisines_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-cuisines-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrCuisinesPage, cuisines_box);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinesPage, cuisines_box2);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinesPage, cuisines_more);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinesPage, cuisines_expander_image);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinesPage, seasonal_box);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinesPage, seasonal_box2);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinesPage, seasonal_more);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinesPage, seasonal_expander_image);

         gtk_widget_class_bind_template_callback (widget_class, season_expander_button_clicked);
         gtk_widget_class_bind_template_callback (widget_class, cuisines_expander_button_clicked);
}

GtkWidget *
gr_cuisines_page_new (void)
{
        GrCuisinesPage *page;

        page = g_object_new (GR_TYPE_CUISINES_PAGE, NULL);

        return GTK_WIDGET (page);
}

void
gr_cuisines_page_refresh (GrCuisinesPage *page)
{
        populate_cuisines (page);
        populate_seasonal (page);
}

static void
cuisines_page_reload (GrCuisinesPage *page)
{
        if (gtk_widget_is_drawable (GTK_WIDGET (page)))
                gr_cuisines_page_refresh (page);
}

static void
connect_store_signals (GrCuisinesPage *page)
{
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (cuisines_page_reload), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (cuisines_page_reload), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (cuisines_page_reload), page);
        g_signal_connect_swapped (store, "reloaded", G_CALLBACK (gr_cuisines_page_refresh), page);
}
