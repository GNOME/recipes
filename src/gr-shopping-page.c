/* gr-shopping-page.c:
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

#include "gr-shopping-page.h"
#include "gr-shopping-tile.h"
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-utils.h"
#include "gr-ingredients-list.h"
#include "gr-window.h"
#include "gr-number.h"
#include "gr-shopping-list-printer.h"
#include "gr-shopping-list-formatter.h"
#include "gr-mail.h"
#include "gr-convert-units.h"
#include "gr-shopping-list-exporter.h"


struct _GrShoppingPage
{
        GtkBox parent_instance;

        GtkWidget *recipe_count_label;
        GtkWidget *recipe_list;
        GtkWidget *ingredients_count_label;
        GtkWidget *ingredients_list;
        GtkWidget *removed_list;
        GtkWidget *add_button;

        int ingredient_count;
        int recipe_count;
        GrRecipeSearch *search;
        GrShoppingListExporter *exporter;

        GtkSizeGroup *group;
        GHashTable *ingredients;

        GrShoppingListPrinter *printer;

        char *title;

        GtkWidget *active_row;
};

G_DEFINE_TYPE (GrShoppingPage, gr_shopping_page, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_TITLE,
        N_PROPS
};

static void connect_store_signals (GrShoppingPage *page);

static void
shopping_page_finalize (GObject *object)
{
        GrShoppingPage *self = GR_SHOPPING_PAGE (object);

        g_clear_object (&self->search);
        g_clear_object (&self->group);
        g_clear_pointer (&self->ingredients, g_hash_table_unref);
        g_clear_object (&self->printer);

        g_free (self->title);

        G_OBJECT_CLASS (gr_shopping_page_parent_class)->finalize (object);
}

static void
recount_ingredients (GrShoppingPage *page)
{
        GList *children;
        int count;
        g_autofree char *tmp = NULL;

        children = gtk_container_get_children (GTK_CONTAINER (page->ingredients_list));
        count = g_list_length (children);
        g_list_free (children);

        tmp = g_strdup_printf (ngettext ("%d ingredient marked for purchase",
                                         "%d ingredients marked for purchase", count),
                                         count);
        gtk_label_set_label (GTK_LABEL (page->ingredients_count_label), tmp);
}

static void
recount_recipes (GrShoppingPage *page)
{
        GList *children;
        int count;
        g_autofree char *tmp = NULL;
        GtkWidget *window;

        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));
        count = g_list_length (children);
        g_list_free (children);

        g_free (page->title);
        page->title = g_strdup_printf (ngettext ("Buy Ingredients (%d recipe)",
                                                 "Buy Ingredients (%d recipes)", count),
                                       count);
        g_object_notify (G_OBJECT (page), "title");

        tmp = g_strdup_printf (ngettext ("%d Recipe marked for preparation",
                                         "%d Recipes marked for preparation", count),
                               count);
        gtk_label_set_label (GTK_LABEL (page->recipe_count_label), tmp);
        if (count == 0) {
                GrRecipeStore *store;

                store = gr_recipe_store_get ();
                gr_recipe_store_clear_shopping_list (store);

                window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                gr_window_go_back (GR_WINDOW (window));
        }
}

typedef struct {
        double amount;
        GrUnit unit;
} Unit;

typedef struct {
        char *ingredient;
        GArray *units;
        gboolean removed;
} Ingredient;

static Ingredient *
ingredient_new (const char *ingredient)
{
        Ingredient *ing;
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        ing = g_new0 (Ingredient, 1);
        ing->ingredient = g_strdup (ingredient);
        ing->units = g_array_new (FALSE, TRUE, sizeof (Unit));

        ing->removed = gr_recipe_store_not_shopping_ingredient (store, ingredient);

        return ing;
}

static void
ingredient_free (gpointer data)
{
        Ingredient *ing = data;

        g_free (ing->ingredient);
        g_array_free (ing->units, TRUE);
        g_free (ing);
}

static void
ingredient_clear (Ingredient *ing)
{
        g_array_set_size (ing->units, 0);
}

static void
ingredient_add (Ingredient *ing,
                double      amount,
                GrUnit      unit)
{
        int i;
        Unit nu;

        for (i = 0; i < ing->units->len; i++) {
                Unit *u = &g_array_index (ing->units, Unit, i);

                if (u->unit == unit) {
                        u->amount += amount;
                        return;
                }
        }

        nu.amount = amount;
        nu.unit = unit;

        g_array_append_val (ing->units, nu);
}

static char *
ingredient_format_unit (Ingredient *ing)
{
        g_autoptr(GString) s = NULL;
        int i;
        GrPreferredUnit user_volume_unit = gr_convert_get_volume_unit();
        GrPreferredUnit user_weight_unit = gr_convert_get_weight_unit();
        GrUnit u1;
        double a1;
        GrDimension dimension;
        s = g_string_new ("");

        for (i = 0; i < ing->units->len; i++) {
                Unit *unit = &g_array_index (ing->units, Unit, i);
                double a = unit->amount;
                GrUnit u = unit->unit;

                dimension = gr_unit_get_dimension (u);

                if (dimension == GR_DIMENSION_VOLUME) {
                        gr_convert_volume (&a, &u, user_volume_unit);
                }
                else if (dimension == GR_DIMENSION_MASS) {
                        gr_convert_weight (&a, &u, user_weight_unit);
                }

                if (i == 0) {
                        u1 = u;
                        a1 = a;
                }
                else if (u == u1) {
                        a1 += a;
                }
                else {
                        if (s->len > 0)
                          g_string_append (s, ", ");
                        gr_convert_format (s, a, u);
                        g_warning ("conversion yielded different units (%s: %s vs %s)...why...", ing->ingredient, gr_unit_get_name (u), gr_unit_get_name (u1));
                }
        }

        if (s->len > 0)
          g_string_append (s, ", ");
        gr_convert_format (s, a1, u1);

        return g_strdup (s->str);
}

static void
add_removed_row (GrShoppingPage *page,
                 const char *unit,
                 const char *ing)
{
        GtkWidget *box;
        GtkWidget *unit_label;
        GtkWidget *ing_label;
        GtkWidget *row;

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_show (box);

        unit_label = gtk_label_new (unit);
        gtk_widget_show (unit_label);
        gtk_label_set_xalign (GTK_LABEL (unit_label), 0.0);
        g_object_set (unit_label, "margin", 10, NULL);
        gtk_style_context_add_class (gtk_widget_get_style_context (unit_label), "dim-label");
        gtk_container_add (GTK_CONTAINER (box), unit_label);
        gtk_size_group_add_widget (page->group, unit_label);

        ing_label = gtk_label_new (ing);
        gtk_widget_show (ing_label);
        gtk_label_set_xalign (GTK_LABEL (ing_label), 0.0);
        g_object_set (ing_label, "margin", 10, NULL);
        gtk_container_add (GTK_CONTAINER (box), ing_label);

        gtk_container_add (GTK_CONTAINER (page->removed_list), box);
        row = gtk_widget_get_parent (box);
        g_object_set_data (G_OBJECT (row), "unit", unit_label);
        g_object_set_data (G_OBJECT (row), "ing", ing_label);

        gtk_widget_show (page->add_button);
}

static void
remove_ingredient (GtkButton *button, GrShoppingPage *page)
{
        GtkWidget *row;
        GtkWidget *label;
        const char *name;
        const char *unit;
        Ingredient *ing;
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);

        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "ing"));
        name = gtk_label_get_label (GTK_LABEL (label));
        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "unit"));
        unit = gtk_label_get_label (GTK_LABEL (label));

        ing = (Ingredient *)g_hash_table_lookup (page->ingredients, name);
        ing->removed = TRUE;

        gr_recipe_store_remove_shopping_ingredient (store, name);

        add_removed_row (page, unit, name);

        page->active_row = NULL;
        gtk_widget_destroy (row);

        recount_ingredients (page);
}

static void
add_ingredient_row (GrShoppingPage *page,
                    const char *unit,
                    const char *ing)
{
        GtkWidget *box;
        GtkWidget *unit_label;
        GtkWidget *ing_label;
        GtkWidget *row;
        GtkWidget *stack;
        GtkWidget *button;
        GtkWidget *image;

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_show (box);

        unit_label = gtk_label_new (unit);
        gtk_widget_show (unit_label);
        gtk_label_set_xalign (GTK_LABEL (unit_label), 0.0);
        g_object_set (unit_label, "margin", 10, NULL);
        gtk_style_context_add_class (gtk_widget_get_style_context (unit_label), "dim-label");
        gtk_container_add (GTK_CONTAINER (box), unit_label);
        gtk_size_group_add_widget (page->group, unit_label);

        ing_label = gtk_label_new (ing);
        gtk_widget_show (ing_label);
        gtk_label_set_xalign (GTK_LABEL (ing_label), 0.0);
        g_object_set (ing_label, "margin", 10, NULL);
        gtk_container_add (GTK_CONTAINER (box), ing_label);

        stack = gtk_stack_new ();
        gtk_widget_set_halign (stack, GTK_ALIGN_END);
        gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_NONE);
        gtk_widget_show (stack);
        image = gtk_image_new ();
        gtk_widget_show (image);
        gtk_widget_set_opacity (image, 0);
        gtk_stack_add_named (GTK_STACK (stack), image, "empty");
        button = gtk_button_new ();
        gtk_widget_show (button);
        g_object_set (button, "margin", 4, NULL);
        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        g_signal_connect (button, "clicked", G_CALLBACK (remove_ingredient), page);
        image = gtk_image_new_from_icon_name ("user-trash-symbolic", 1);
        gtk_widget_show (image);
        gtk_container_add (GTK_CONTAINER (button), image);
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "circular");
        gtk_stack_add_named (GTK_STACK (stack), button, "buttons");
        gtk_box_pack_end (GTK_BOX (box), stack, TRUE, TRUE, 0);

        gtk_container_add (GTK_CONTAINER (page->ingredients_list), box);
        row = gtk_widget_get_parent (box);
        g_object_set_data (G_OBJECT (row), "unit", unit_label);
        g_object_set_data (G_OBJECT (row), "ing", ing_label);
        g_object_set_data (G_OBJECT (row), "buttons-stack", stack);
}

static void
set_active_row (GrShoppingPage *page,
                GtkWidget      *row)
{
        GtkWidget *stack;

        if (page->active_row) {
                stack = g_object_get_data (G_OBJECT (page->active_row), "buttons-stack");
                gtk_stack_set_visible_child_name (GTK_STACK (stack), "empty");
        }

        if (page->active_row == row) {
                page->active_row = NULL;
                return;
        }

        page->active_row = row;

        if (page->active_row) {
                stack = g_object_get_data (G_OBJECT (page->active_row), "buttons-stack");
                gtk_stack_set_visible_child_name (GTK_STACK (stack), "buttons");
        }
}

static void
row_activated (GtkListBox     *list,
               GtkListBoxRow  *row,
               GrShoppingPage *page)
{
        set_active_row (page, GTK_WIDGET (row));
}

static void
removed_row_activated (GtkListBox     *list,
                       GtkListBoxRow  *row,
                       GrShoppingPage *page)
{
        GtkWidget *popover;
        GtkWidget *label;
        const char *name;
        const char *unit;
        Ingredient *ing;
        GrRecipeStore *store;
        GList *children;

        store = gr_recipe_store_get ();

        popover = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_POPOVER);
        gtk_popover_popdown (GTK_POPOVER (popover));

        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "ing"));
        name = gtk_label_get_label (GTK_LABEL (label));
        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "unit"));
        unit = gtk_label_get_label (GTK_LABEL (label));

        ing = (Ingredient *)g_hash_table_lookup (page->ingredients, name);
        ing->removed = FALSE;

        gr_recipe_store_readd_shopping_ingredient (store, name);

        add_ingredient_row (page, unit, name);

        gtk_widget_destroy (GTK_WIDGET (row));

        recount_ingredients (page);

        children = gtk_container_get_children (GTK_CONTAINER (page->removed_list));
        if (children == NULL)
                gtk_widget_hide (page->add_button);
        g_list_free (children);
}

static void
add_ingredient (GrShoppingPage *page,
                double      amount,
                GrUnit      unit,
                const char *ingredient)
{
        Ingredient *ing;

        ing = g_hash_table_lookup (page->ingredients, ingredient);
        if (ing == NULL) {
                ing = ingredient_new (ingredient);
                g_hash_table_insert (page->ingredients, g_strdup (ingredient), ing);
        }

        ingredient_add (ing, amount, unit);
}

static void
collect_ingredients_from_recipe (GrShoppingPage *page,
                                 GrRecipe       *recipe,
                                 double          yield)
{
        g_autoptr(GrIngredientsList) il = NULL;
        g_autofree char **seg = NULL;
        int i, j;

        il = gr_ingredients_list_new (gr_recipe_get_ingredients (recipe));
        seg = gr_ingredients_list_get_segments (il);
        for (i = 0; seg[i]; i++) {
                g_auto(GStrv) ing = NULL;
                ing = gr_ingredients_list_get_ingredients (il, seg[i]);
                for (j = 0; ing[j]; j++) {
                        GrUnit unit;
                        double amount;

                        amount = gr_ingredients_list_get_amount (il, seg[i], ing[j]);
                        amount = amount * yield / gr_recipe_get_yield (recipe);
                        unit = gr_ingredients_list_get_unit (il, seg[i], ing[j]);
                        add_ingredient (page, amount, unit, ing[j]);
                }
        }
}

static void
collect_ingredients (GrShoppingPage *page)
{
        GList *children, *l;
        GHashTableIter iter;
        Ingredient *ing;

        g_hash_table_iter_init (&iter, page->ingredients);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&ing)) {
                ingredient_clear (ing);
        }

        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));

        for (l = children; l; l = l->next) {
                GtkWidget *tile = gtk_bin_get_child (GTK_BIN (l->data));
                GrRecipe *recipe;
                double yield;

                recipe = gr_shopping_tile_get_recipe (GR_SHOPPING_TILE (tile));
                yield = gr_shopping_tile_get_yield (GR_SHOPPING_TILE (tile));
                collect_ingredients_from_recipe (page, recipe, yield);
        }
        g_list_free (children);

        g_hash_table_iter_init (&iter, page->ingredients);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&ing)) {
                if (ing->units->len == 0)
                        g_hash_table_iter_remove (&iter);
        }

        container_remove_all (GTK_CONTAINER (page->ingredients_list));
        container_remove_all (GTK_CONTAINER (page->removed_list));
        g_hash_table_iter_init (&iter, page->ingredients);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&ing)) {
                g_autofree char *unit = NULL;
                unit = ingredient_format_unit (ing);
                if (ing->removed)
                        add_removed_row (page, unit, ing->ingredient);
                else
                        add_ingredient_row (page, unit, ing->ingredient);
        }
}

static void
yield_changed (GObject *object, GParamSpec *pspec, GrShoppingPage *page)
{
        GrShoppingTile *tile = GR_SHOPPING_TILE (object);
        GrRecipeStore *store;
        GrRecipe *recipe;
        double yield;

        recipe = gr_shopping_tile_get_recipe (tile);
        yield = gr_shopping_tile_get_yield (tile);

        store = gr_recipe_store_get ();

        gr_recipe_store_add_to_shopping (store, recipe, yield);
}

static void
search_started (GrRecipeSearch *search,
                GrShoppingPage *page)
{
        container_remove_all (GTK_CONTAINER (page->recipe_list));
        page->recipe_count = 0;
}

static void
search_hits_added (GrRecipeSearch *search,
                   GList          *hits,
                   GrShoppingPage *page)
{
        GrRecipeStore *store;
        GList *l;

        store = gr_recipe_store_get ();

        for (l = hits; l; l = l->next) {
                GrRecipe *recipe = l->data;
                GtkWidget *tile;
                double yield;

                yield = gr_recipe_store_get_shopping_yield (store, recipe);
                tile = gr_shopping_tile_new (recipe, yield);
                g_signal_connect (tile, "notify::yield", G_CALLBACK (yield_changed), page);
                gtk_container_add (GTK_CONTAINER (page->recipe_list), tile);
                page->recipe_count++;
        }
}

static void
search_hits_removed (GrRecipeSearch *search,
                     GList          *hits,
                     GrShoppingPage *page)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));
        for (l = children; l; l = l->next) {
                GtkWidget *item = l->data;
                GtkWidget *tile;
                GrRecipe *recipe;

                tile = gtk_bin_get_child (GTK_BIN (item));
                recipe = gr_shopping_tile_get_recipe (GR_SHOPPING_TILE (tile));
                if (g_list_find (hits, recipe)) {
                        gtk_container_remove (GTK_CONTAINER (page->recipe_list), item);
                        page->recipe_count--;
                }
        }
}

static void
search_finished (GrRecipeSearch *search,
                 GrShoppingPage *page)
{
        collect_ingredients (page);
        recount_ingredients (page);
        recount_recipes (page);
}

static void
clear_list (GrShoppingPage *page)
{
        GrRecipeStore *store;
        GtkWidget *window;

        store = gr_recipe_store_get ();

        container_remove_all (GTK_CONTAINER (page->ingredients_list));
        container_remove_all (GTK_CONTAINER (page->removed_list));
        container_remove_all (GTK_CONTAINER (page->recipe_list));

        gr_recipe_store_clear_shopping_list (store);

        g_hash_table_remove_all (page->ingredients);
        gtk_widget_hide (page->add_button);

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_go_back (GR_WINDOW (window));
}

GList *
get_recipes (GrShoppingPage *page)
{
        GList *children, *l, *recipes;

        recipes = NULL;
        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));
        for (l = children; l; l = l->next) {
                GtkWidget *tile = gtk_bin_get_child (GTK_BIN (l->data));
                GrRecipe *recipe;

                recipe = gr_shopping_tile_get_recipe (GR_SHOPPING_TILE (tile));
                recipes = g_list_append (recipes, g_object_ref (recipe));
        }
        g_list_free (children);

        return recipes;
}

GList *
get_ingredients (GrShoppingPage *page)
{
        GList *children, *l, *ingredients;

        ingredients = NULL;
        children = gtk_container_get_children (GTK_CONTAINER (page->ingredients_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkWidget *unit = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "unit"));
                GtkWidget *ing = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "ing"));
                ShoppingListItem *item;

                item = g_new (ShoppingListItem, 1);
                item->amount = g_strdup (gtk_label_get_label (GTK_LABEL (unit)));
                item->name = g_strdup (gtk_label_get_label (GTK_LABEL (ing)));
                ingredients = g_list_append (ingredients, item);
        }
        g_list_free (children);

        return ingredients;
}

void
item_free (gpointer data)
{
        ShoppingListItem *item = data;

        g_free (item->amount);
        g_free (item->name);
        g_free (item);
}

static void
print_list (GrShoppingPage *page)
{
        GList *recipes, *items;

        if (!page->printer) {
                GtkWidget *window;

                window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                page->printer = gr_shopping_list_printer_new (GTK_WINDOW (window));
        }

        recipes = get_recipes (page);
        items = get_ingredients (page);

        gr_shopping_list_printer_print (page->printer, recipes, items);

        g_list_free_full (recipes, g_object_unref);
        g_list_free_full (items, item_free);
}

static void
open_export_shopping_list_dialog (GrShoppingPage *page)
{
        GList *items;
        items = get_ingredients (page);
        if (!page->exporter) {
                GtkWidget *window;

                window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                page->exporter = gr_shopping_list_exporter_new (GTK_WINDOW (window));
        }
        gr_shopping_list_exporter_export (page->exporter, items);
}

static void
gr_shopping_page_init (GrShoppingPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
        connect_store_signals (page);

        page->search = gr_recipe_search_new ();
        g_signal_connect (page->search, "started", G_CALLBACK (search_started), page);
        g_signal_connect (page->search, "hits-added", G_CALLBACK (search_hits_added), page);
        g_signal_connect (page->search, "hits-removed", G_CALLBACK (search_hits_removed), page);
        g_signal_connect (page->search, "finished", G_CALLBACK (search_finished), page);

        page->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        page->ingredients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, ingredient_free);
}

static void
shopping_page_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
        GrShoppingPage *self = GR_SHOPPING_PAGE (object);

        switch (prop_id) {
        case PROP_TITLE:
                g_value_set_string (value, self->title);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
shopping_page_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
        GrShoppingPage *self = GR_SHOPPING_PAGE (object);

        switch (prop_id) {
        case PROP_TITLE:
                g_free (self->title);
                self->title = g_value_dup_string (value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gr_shopping_page_class_init (GrShoppingPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = shopping_page_finalize;
        object_class->get_property = shopping_page_get_property;
        object_class->set_property = shopping_page_set_property;

        pspec = g_param_spec_string ("title", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_TITLE, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-shopping-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrShoppingPage, recipe_count_label);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingPage, recipe_list);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingPage, ingredients_count_label);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingPage, ingredients_list);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingPage, removed_list);
        gtk_widget_class_bind_template_child (widget_class, GrShoppingPage, add_button);

        gtk_widget_class_bind_template_callback (widget_class, clear_list);
        gtk_widget_class_bind_template_callback (widget_class, print_list);
        gtk_widget_class_bind_template_callback (widget_class, open_export_shopping_list_dialog);
        gtk_widget_class_bind_template_callback (widget_class, row_activated);
        gtk_widget_class_bind_template_callback (widget_class, removed_row_activated);
}

GtkWidget *
gr_shopping_page_new (void)
{
        GrShoppingPage *page;

        page = g_object_new (GR_TYPE_SHOPPING_PAGE, NULL);

        return GTK_WIDGET (page);
}

void
gr_shopping_page_populate (GrShoppingPage *self)
{
        container_remove_all (GTK_CONTAINER (self->ingredients_list));
        container_remove_all (GTK_CONTAINER (self->removed_list));
        container_remove_all (GTK_CONTAINER (self->recipe_list));
        gr_recipe_search_stop (self->search);
        gr_recipe_search_set_query (self->search, "is:shopping");
}

static void
recipe_removed (GrShoppingPage *page,
                GrRecipe       *recipe)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));
        for (l = children; l; l = l->next) {
                GtkWidget *tile = gtk_bin_get_child (GTK_BIN (l->data));

                if (recipe == gr_shopping_tile_get_recipe (GR_SHOPPING_TILE (tile))) {
                        gtk_widget_destroy (GTK_WIDGET (l->data));

                        collect_ingredients (page);
                        recount_ingredients (page);
                        recount_recipes (page);

                        break;
                }
        }
        g_list_free (children);
}

static void
recipe_added (GrShoppingPage *page,
              GrRecipe       *recipe)
{
        GrRecipeStore *store;
        GList *children, *l;
        double yield;

        if (!gtk_widget_is_drawable (GTK_WIDGET (page)))
                return;

        store = gr_recipe_store_get ();

        yield = gr_recipe_store_get_shopping_yield (store, recipe);

        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));
        for (l = children; l; l = l->next) {
                GtkWidget *tile = gtk_bin_get_child (GTK_BIN (l->data));

                if (recipe == gr_shopping_tile_get_recipe (GR_SHOPPING_TILE (tile))) {
                        gr_shopping_tile_set_yield (GR_SHOPPING_TILE (tile), yield);
                        break;
                }
        }
        g_list_free (children);

        if (l == NULL) {
                GtkWidget *tile;

                tile = gr_shopping_tile_new (recipe, yield);
                g_signal_connect (tile, "notify::yield", G_CALLBACK (yield_changed), page);
                gtk_container_add (GTK_CONTAINER (page->recipe_list), tile);
        }

        collect_ingredients (page);
        recount_ingredients (page);
        recount_recipes (page);
}

static void
recipe_changed (GrShoppingPage *page,
                GrRecipe       *recipe)
{
        GrRecipeStore *store;

        if (!gtk_widget_is_drawable (GTK_WIDGET (page)))
                return;

        store = gr_recipe_store_get ();

        if (gr_recipe_store_is_in_shopping (store, recipe))
                recipe_added (page, recipe);
        else
                recipe_removed (page, recipe);
}

static void
connect_store_signals (GrShoppingPage *page)
{
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (recipe_removed), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (recipe_changed), page);
}
