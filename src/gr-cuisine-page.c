/* gr-cuisine-page.c:
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

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-cuisine-page.h"
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-cuisine.h"
#include "gr-recipe-tile.h"
#include "gr-app.h"
#include "gr-utils.h"
#include "gr-meal.h"


typedef struct
{
        const char *name;
        GtkWidget *item;
        GtkWidget *label;
        GtkWidget *box;
        gboolean filled;
} Category;

struct _GrCuisinePage
{
        GtkBox     parent_instance;

        char *cuisine;

        GtkWidget *sidebar;
        GtkWidget *scrolled_window;
        GtkWidget *category_box;
        GtkWidget *stack;
        GtkWidget *scroll_to_row;

        int n_categories;
        Category *categories;
        Category *other;
};

G_DEFINE_TYPE (GrCuisinePage, gr_cuisine_page, GTK_TYPE_BOX)

static void connect_store_signals (GrCuisinePage *page);

static gboolean
scroll_in_idle (gpointer data)
{
        GrCuisinePage *page = data;
        GtkWidget *item;
        GtkAdjustment *adj;
        GtkAllocation alloc;
        double page_increment, value;
        Category *category;
        gboolean dummy;

        if (page->scroll_to_row == NULL)
                return G_SOURCE_REMOVE;

        item = gtk_bin_get_child (GTK_BIN (page->scroll_to_row));
        category = (Category *)g_object_get_data (G_OBJECT (item), "category");

        if (!category)
                return G_SOURCE_REMOVE;

        adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (page->scrolled_window));
        gtk_widget_get_allocation (category->label, &alloc);
        page_increment = gtk_adjustment_get_page_increment (adj);
        value = gtk_adjustment_get_value (adj);
        gtk_adjustment_set_page_increment (adj, alloc.y - value);
        g_signal_emit_by_name (page->scrolled_window, "scroll-child", GTK_SCROLL_PAGE_FORWARD, FALSE, &dummy);
        gtk_adjustment_set_page_increment (adj, page_increment);

        return G_SOURCE_REMOVE;
}

static void
row_selected (GrCuisinePage *page,
              GtkListBoxRow *row)
{
        page->scroll_to_row = GTK_WIDGET (row);
        g_idle_add (scroll_in_idle, page);
}

static void
cuisine_page_finalize (GObject *object)
{
        GrCuisinePage *self = GR_CUISINE_PAGE (object);

        g_clear_pointer (&self->cuisine, g_free);
        g_clear_pointer (&self->categories, g_free);

        G_OBJECT_CLASS (gr_cuisine_page_parent_class)->finalize (object);
}

static void
populate_initially (GrCuisinePage *self)
{
        const char **names;
        const char *title;
        int length;
        int i;

        names = gr_meal_get_names (&length);

        self->n_categories = length;
        self->categories = g_new (Category, length);

        for (i = 0; i < length; i++) {
                GtkWidget *item, *label, *box;

                title = gr_meal_get_title (names[i]);

                item = gtk_label_new (title);
                gtk_label_set_xalign (GTK_LABEL (item), 0);
                gtk_widget_show (item);
                gtk_style_context_add_class (gtk_widget_get_style_context (item), "sidebar");
                gtk_list_box_insert (GTK_LIST_BOX (self->sidebar), item, -1);

                label = gtk_label_new (title);
                gtk_label_set_xalign (GTK_LABEL (label), 0);
                gtk_style_context_add_class (gtk_widget_get_style_context (label), "heading");
                gtk_widget_show (label);
                gtk_container_add (GTK_CONTAINER (self->category_box), label);

                box = gtk_flow_box_new ();
                gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (box), TRUE);
                gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (box), 20);
                gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (box), 20);
                gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (box), 3);
                gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (box), 3);
                gtk_widget_show (box);
                gtk_container_add (GTK_CONTAINER (self->category_box), box);

                self->categories[i].name = names[i];
                self->categories[i].item = item;
                self->categories[i].label = label;
                self->categories[i].box = box;

                g_object_set_data (G_OBJECT (item), "category", &self->categories[i]);

                if (strcmp (names[i], "other") == 0)
                        self->other = &self->categories[i];
        }
}

static gboolean
filter_sidebar (GtkListBoxRow *row,
                gpointer       user_data)
{
        GtkWidget *item;
        Category *category;

        item = gtk_bin_get_child (GTK_BIN (row));
        category = (Category *)g_object_get_data (G_OBJECT (item), "category");
        if (category)
                return category->filled;

        return TRUE;
}

static void
gr_cuisine_page_init (GrCuisinePage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));

        populate_initially (page);
        connect_store_signals (page);

        gtk_list_box_set_filter_func (GTK_LIST_BOX (page->sidebar), filter_sidebar, page, NULL);
}

static void
gr_cuisine_page_class_init (GrCuisinePageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = cuisine_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-cuisine-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrCuisinePage, sidebar);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinePage, scrolled_window);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinePage, category_box);
        gtk_widget_class_bind_template_child (widget_class, GrCuisinePage, stack);

        gtk_widget_class_bind_template_callback (widget_class, row_selected);
}

GtkWidget *
gr_cuisine_page_new (void)
{
        GrCuisinePage *page;

        page = g_object_new (GR_TYPE_CUISINE_PAGE, NULL);

        return GTK_WIDGET (page);
}

#if 0
static gboolean
unselect (gpointer data)
{
        GrCuisinePage *self = data;

        gtk_list_box_unselect_all (GTK_LIST_BOX (self->sidebar));

        return G_SOURCE_REMOVE;
}
#endif
void
gr_cuisine_page_set_cuisine (GrCuisinePage *self,
                             const char    *cuisine)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i, j;
        gboolean has_recipe = FALSE;

        if (self->cuisine != cuisine) {
                g_free (self->cuisine);
                self->cuisine = g_strdup (cuisine);
        }

        for (i = 0; i < self->n_categories; i++) {
                container_remove_all (GTK_CONTAINER (self->categories[i].box));
                gtk_widget_hide (self->categories[i].label);
                gtk_widget_hide (self->categories[i].box);
                self->categories[i].filled = FALSE;
        }

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keys = gr_recipe_store_get_recipe_keys (store, &length);
        for (j = 0; j < length; j++) {
                g_autoptr(GrRecipe) recipe = NULL;
                const char *cuisine2;
                const char *category;
                GtkWidget *tile;
                Category *c;

                recipe = gr_recipe_store_get_recipe (store, keys[j]);
                cuisine2 = gr_recipe_get_cuisine (recipe);
                category = gr_recipe_get_category (recipe);

                if (g_strcmp0 (cuisine, cuisine2) != 0)
                        continue;

                c = self->other;
                for (i = 0; i < self->n_categories; i++) {
                        if (strcmp (self->categories[i].name, category) == 0) {
                                c = &self->categories[i];
                                break;
                        }
                }

                gtk_widget_show (c->label);
                gtk_widget_show (c->box);
                self->categories[i].filled = TRUE;

                tile = gr_recipe_tile_new (recipe);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (c->box), tile);

                has_recipe = TRUE;
        }

        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), has_recipe ? "cuisine" : "empty");

        gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->sidebar));
        gtk_list_box_unselect_all (GTK_LIST_BOX (self->sidebar));
        //g_idle_add (unselect, self);
}

static void
cuisine_page_reload (GrCuisinePage *page)
{
        gr_cuisine_page_set_cuisine (page, page->cuisine);
}

static void
connect_store_signals (GrCuisinePage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        /* FIXME: inefficient */
        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (cuisine_page_reload), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (cuisine_page_reload), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (cuisine_page_reload), page);
}
