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
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-recipe-small-tile.h"
#include "gr-app.h"
#include "gr-utils.h"
#include "gr-ingredients-list.h"
#include "gr-window.h"
#include "gr-number.h"
#include "gr-shopping-list-printer.h"
#include "gr-shopping-list-formatter.h"
#include "gr-mail.h"


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

        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));
        count = g_list_length (children);
        g_list_free (children);

        g_free (page->title);
        page->title = g_strdup_printf (ngettext ("Buy Ingredients (%d recipe)",
                                                 "Buy Ingredients (%d recipes)", page->recipe_count),
                                       page->recipe_count);
        g_object_notify (G_OBJECT (page), "title");

        tmp = g_strdup_printf (ngettext ("%d Recipe marked for preparation",
                                         "%d Recipes marked for preparation", count),
                               count);
        gtk_label_set_label (GTK_LABEL (page->recipe_count_label), tmp);
}

typedef struct {
        GrNumber amount;
        char *unit;
} Unit;

typedef struct {
        char *ingredient;
        GArray *units;
        gboolean removed;
} Ingredient;

static void
clear_unit (gpointer data)
{
        Unit *unit = data;

        g_free (unit->unit);
}

static Ingredient *
ingredient_new (const char *ingredient)
{
        Ingredient *ing;

        ing = g_new0 (Ingredient, 1);
        ing->ingredient = g_strdup (ingredient);
        ing->units = g_array_new (FALSE, TRUE, sizeof (Unit));
        g_array_set_clear_func (ing->units, clear_unit);

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
                GrNumber   *amount,
                const char *unit)
{
        int i;
        Unit nu;

        for (i = 0; i < ing->units->len; i++) {
                Unit *u = &g_array_index (ing->units, Unit, i);

                if (g_strcmp0 (u->unit, unit) == 0) {
                        gr_number_add (&u->amount, amount, &u->amount);
                        return;
                }
        }

        nu.amount = *amount;
        nu.unit = g_strdup (unit);

        g_array_append_val (ing->units, nu);
}

static char *
ingredient_format_unit (Ingredient *ing)
{
        g_autoptr(GString) s = NULL;
        int i;

        s = g_string_new ("");

        for (i = 0; i < ing->units->len; i++) {
                Unit *u = &g_array_index (ing->units, Unit, i);
                g_autofree char *num = NULL;
                if (s->len > 0)
                        g_string_append (s, ", ");
                num = gr_number_format (&(u->amount));
                g_string_append (s, num);
                g_string_append (s, " ");
                if (u->unit)
                        g_string_append (s, u->unit);
        }

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
}

static void
remove_ingredient (GtkButton *button, GrShoppingPage *page)
{
        GtkWidget *row;
        GtkWidget *label;
        const char *name;
        const char *unit;
        Ingredient *ing;

        row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);

        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "ing"));
        name = gtk_label_get_label (GTK_LABEL (label));
        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "unit"));
        unit = gtk_label_get_label (GTK_LABEL (label));

        ing = (Ingredient *)g_hash_table_lookup (page->ingredients, name);
        ing->removed = TRUE;

        add_removed_row (page, unit, name);

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
selected_rows_changed (GtkListBox     *list,
                       GrShoppingPage *page)
{
        GtkListBoxRow *row;

        row = gtk_list_box_get_selected_row (list);
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

        popover = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_POPOVER);
        gtk_popover_popdown (GTK_POPOVER (popover));

        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "ing"));
        name = gtk_label_get_label (GTK_LABEL (label));
        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "unit"));
        unit = gtk_label_get_label (GTK_LABEL (label));

        ing = (Ingredient *)g_hash_table_lookup (page->ingredients, name);
        ing->removed = FALSE;

        add_ingredient_row (page, unit, name);

        gtk_widget_destroy (GTK_WIDGET (row));

        recount_ingredients (page);
}

static void
add_ingredient (GrShoppingPage *page,
                GrNumber *amount,
                const char *unit,
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
                                 int             serves)
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
                        const char *unit;
                        GrNumber *amount, *num;
                        amount = gr_ingredients_list_get_amount (il, seg[i], ing[j]);
                        num = gr_number_new_fraction (serves, gr_recipe_get_serves (recipe));
                        gr_number_multiply (num, amount, amount);
                        g_free (num);
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
                int serves;

                recipe = gr_recipe_small_tile_get_recipe (GR_RECIPE_SMALL_TILE (tile));
                g_object_get (tile, "serves", &serves, NULL);
                collect_ingredients_from_recipe (page, recipe, serves);
        }
        g_list_free (children);

        g_hash_table_iter_init (&iter, page->ingredients);
        while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&ing)) {
                if (ing->units->len == 0)
                        g_hash_table_iter_remove (&iter);
        }

        container_remove_all (GTK_CONTAINER (page->ingredients_list));
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
recipes_changed (GrShoppingPage *page)
{
        collect_ingredients (page);
        recount_ingredients (page);
        recount_recipes (page);
}

static void
search_started (GrRecipeSearch *search,
                GrShoppingPage     *page)
{
        container_remove_all (GTK_CONTAINER (page->recipe_list));
        page->recipe_count = 0;
}

static void
search_hits_added (GrRecipeSearch *search,
                   GList          *hits,
                   GrShoppingPage *page)
{
        GList *l;

        for (l = hits; l; l = l->next) {
                GrRecipe *recipe = l->data;
                GtkWidget *tile;
                tile = gr_recipe_small_tile_new (recipe);
                g_signal_connect_swapped (tile, "notify::serves", G_CALLBACK (recipes_changed), page);
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
                recipe = gr_recipe_small_tile_get_recipe (GR_RECIPE_SMALL_TILE (tile));
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
        GList *children, *l;
        GrRecipeStore *store;
        GtkWidget *window;
        GList *recipes;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));
        recipes = NULL;
        for (l = children; l; l = l->next) {
                GtkWidget *tile = gtk_bin_get_child (GTK_BIN (l->data));
                GrRecipe *recipe = gr_recipe_small_tile_get_recipe (GR_RECIPE_SMALL_TILE (tile));
                recipes = g_list_prepend (recipes, g_object_ref (recipe));
        }
        g_list_free (children);

        container_remove_all (GTK_CONTAINER (page->ingredients_list));
        container_remove_all (GTK_CONTAINER (page->removed_list));
        container_remove_all (GTK_CONTAINER (page->recipe_list));

        for (l = recipes; l; l = l->next) {
                GrRecipe *recipe = l->data;
                gr_recipe_store_remove_from_shopping (store, recipe);
        }
        g_list_free_full (recipes, g_object_unref);

        g_hash_table_remove_all (page->ingredients);

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_go_back (GR_WINDOW (window));
}

static GList *
get_recipes (GrShoppingPage *page)
{
        GList *children, *l, *recipes;

        recipes = NULL;
        children = gtk_container_get_children (GTK_CONTAINER (page->recipe_list));
        for (l = children; l; l = l->next) {
                GtkWidget *tile = gtk_bin_get_child (GTK_BIN (l->data));
                GrRecipe *recipe;

                recipe = gr_recipe_small_tile_get_recipe (GR_RECIPE_SMALL_TILE (tile));
                recipes = g_list_append (recipes, g_object_ref (recipe));
        }
        g_list_free (children);

        return recipes;
}

static GList *
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

static void
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
share_list (GrShoppingPage *page)
{
        GList *recipes, *items;
        g_autofree char *text = NULL;
        g_autoptr(GError) error = NULL;

        recipes = get_recipes (page);
        items = get_ingredients (page);

        text = gr_shopping_list_format (recipes, items);
        if (!gr_send_mail (NULL, _("Shopping List"), text, NULL, &error)) {
                GtkWidget *window;
                GtkWidget *error_dialog;

                g_message ("Sharing the shopping list failed: %s", error->message);

                window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
                error_dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                                       GTK_DIALOG_MODAL |
                                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                                       GTK_MESSAGE_ERROR,
                                                       GTK_BUTTONS_CLOSE,
                                                       _("Could not share shopping list"));
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (error_dialog),
                                                          _("There was an error sending the shopping "
                                                            "list by e-mail.\n"));
                g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
                gtk_widget_show (error_dialog);

        }

        g_list_free_full (recipes, g_object_unref);
        g_list_free_full (items, item_free);
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

        gtk_list_box_set_header_func (GTK_LIST_BOX (page->ingredients_list),
                                      all_headers, page, NULL);
        gtk_list_box_set_header_func (GTK_LIST_BOX (page->removed_list),
                                      all_headers, page, NULL);
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

        gtk_widget_class_bind_template_callback (widget_class, clear_list);
        gtk_widget_class_bind_template_callback (widget_class, print_list);
        gtk_widget_class_bind_template_callback (widget_class, share_list);
        gtk_widget_class_bind_template_callback (widget_class, selected_rows_changed);
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
        container_remove_all (GTK_CONTAINER (self->recipe_list));
        gr_recipe_search_stop (self->search);
        gr_recipe_search_set_query (self->search, "is:shopping");
}

static void
connect_store_signals (GrShoppingPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        /* FIXME: inefficient */
        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (gr_shopping_page_populate), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (gr_shopping_page_populate), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (gr_shopping_page_populate), page);
}
