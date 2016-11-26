/* gr-window.c:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include <libgd/gd.h>

#include "gr-window.h"
#include "gr-details-page.h"
#include "gr-edit-page.h"
#include "gr-list-page.h"
#include "gr-cuisine-page.h"
#include "gr-search-page.h"
#include "gr-recipes-page.h"
#include "gr-ingredient.h"
#include "gr-category.h"
#include "gr-ingredient-row.h"
#include "gr-diet-row.h"
#include "gr-meal-row.h"
#include "gr-ingredients-page.h"
#include "gr-ingredients-search-page.h"


struct _GrWindow
{
        GtkApplicationWindow parent_instance;

        GtkWidget *search_button_stack;
        GtkWidget *recipes_search_button;
        GtkWidget *recipes_search_button2;
        GtkWidget *ingredients_search_button;
        GtkWidget *ingredients_search_button2;
        GtkWidget *search_bar;
        GtkWidget *search_entry;
        GtkWidget *header_stack;
        GtkWidget *main_stack;
        GtkWidget *recipes_page;
        GtkWidget *details_header;
        GtkWidget *details_page;
        GtkWidget *edit_header;
        GtkWidget *edit_page;
        GtkWidget *list_header;
        GtkWidget *list_page;
        GtkWidget *search_page;
        GtkWidget *cuisines_page;
        GtkWidget *cuisine_header;
        GtkWidget *cuisine_page;
        GtkWidget *ingredients_page;
        GtkWidget *ingredients_search_page;
        GtkWidget *cooking_button;
        GtkWidget *meal_search_revealer;
        GtkWidget *meal_search_button;
        GtkWidget *meal_list;
        GtkWidget *diet_search_revealer;
        GtkWidget *diet_search_button;
        GtkWidget *diet_list;
        GtkWidget *ing_search_revealer;
        GtkWidget *ing_search_button;
        GtkWidget *ingredients_list;

        GQueue *back_entry_stack;
};

G_DEFINE_TYPE (GrWindow, gr_window, GTK_TYPE_APPLICATION_WINDOW)

typedef struct
{
        gchar *page;
        gchar *header;
        gchar *search;
} BackEntry;

static void
back_entry_free (BackEntry *entry)
{
        g_free (entry->page);
        g_free (entry->header);
        g_free (entry->search);
        g_free (entry);
}

static void
save_back_entry (GrWindow *window)
{
        BackEntry *entry;

        entry = g_new (BackEntry, 1);
        entry->page = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack)));
        entry->header = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->header_stack)));
        if (strcmp (entry->page, "search") == 0)
                entry->search = g_strdup (gtk_entry_get_text (GTK_ENTRY (window->search_entry)));
        else
                entry->search = NULL;

        g_queue_push_head (window->back_entry_stack, entry);
}

static void
go_back (GrWindow *window)
{
        BackEntry *entry;

        entry = g_queue_pop_head (window->back_entry_stack);

        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), entry->header);
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), entry->page);
        if (strcmp (entry->page, "search") == 0) {
                gtk_entry_set_text (GTK_ENTRY (window->search_entry), entry->search);
                gtk_editable_set_position (GTK_EDITABLE (window->search_entry), -1);
                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), TRUE);
        }
        else {
                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);
        }

        back_entry_free (entry);
}

static void
new_recipe (GrWindow *window)
{
        save_back_entry (window);

        gr_edit_page_clear (GR_EDIT_PAGE (window->edit_page));

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->edit_header), "Add a new recipe");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "edit");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "edit");
}

static void
add_recipe (GrWindow *window)
{
        if (gr_edit_page_save (GR_EDIT_PAGE (window->edit_page)))
                go_back (window);
}

static void
stop_search (GrWindow *window)
{
        gr_window_go_back (window);
}

static void
maybe_stop_search (GtkButton  *button,
                   GParamSpec *pspec,
                   GrWindow   *window)
{
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
                stop_search (window);
}

static void
switch_to_search (GrWindow *window)
{
        save_back_entry (window);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->recipes_search_button2), TRUE);
     	gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "search");
     	gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "search");
}

static void
switch_to_ingredients_search (GrWindow *window)
{
        save_back_entry (window);
     	gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "ingredients-search");
     	gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "ingredients-search");
}

static void
ingredients_search_clicked (GtkButton *button,
                            GrWindow  *window)
{
        if ((GtkWidget *)button == window->ingredients_search_button) {
                gr_ingredients_search_page_set_ingredient (GR_INGREDIENTS_SEARCH_PAGE (window->ingredients_search_page), NULL);
                switch_to_ingredients_search (window);
        }
        else
                gr_window_go_back (window);
}

static void
visible_page_changed (GrWindow *window)
{
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "ingredients") == 0) {
                g_signal_handlers_block_by_func (window->ingredients_search_button, ingredients_search_clicked, window);
	        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->ingredients_search_button), FALSE);
                gtk_stack_set_visible_child_name (GTK_STACK (window->search_button_stack), "ingredients");
                g_signal_handlers_unblock_by_func (window->ingredients_search_button, ingredients_search_clicked, window);
        }
        else if (strcmp (visible, "ingredients-search") == 0) {
                g_signal_handlers_block_by_func (window->ingredients_search_button2, ingredients_search_clicked, window);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->ingredients_search_button2), TRUE);
                gtk_stack_set_visible_child_name (GTK_STACK (window->search_button_stack), "ingredients");
                g_signal_handlers_unblock_by_func (window->ingredients_search_button2, ingredients_search_clicked, window);
        }
        else {
                gtk_stack_set_visible_child_name (GTK_STACK (window->search_button_stack), "recipes");
        }

        if (strcmp (visible, "search") != 0) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->recipes_search_button), FALSE);
        }

        if (strcmp (visible, "recipes") == 0) {
                gr_recipes_page_set_categories_expanded (GR_RECIPES_PAGE (window->recipes_page), FALSE);
        }
}

static void
search_changed (GrWindow *window)
{
        const char *visible;
        g_autoptr(GString) s = NULL;
        GList *children, *l;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "search") != 0)
                switch_to_search (window);

        s = g_string_new (gtk_entry_get_text (GTK_ENTRY (window->search_entry)));

        children = gtk_container_get_children (GTK_CONTAINER (window->meal_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                g_autofree char *term = NULL;

                if (!GR_IS_MEAL_ROW (row))
                        continue;

                term = gr_meal_row_get_search_term (GR_MEAL_ROW (row));
                if (term) {
                        g_string_append_c (s, ' ');
                        g_string_append (s, term);
                }
        }
        g_list_free (children);

        children = gtk_container_get_children (GTK_CONTAINER (window->diet_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                g_autofree char *term = NULL;

                if (!GR_IS_DIET_ROW (row))
                        continue;

                term = gr_diet_row_get_search_term (GR_DIET_ROW (row));
                if (term) {
                        g_string_append_c (s, ' ');
                        g_string_append (s, term);
                }
        }
        g_list_free (children);

        children = gtk_container_get_children (GTK_CONTAINER (window->ingredients_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                g_autofree char *term = NULL;

                if (!GR_IS_INGREDIENT_ROW (row))
                        continue;

                term = gr_ingredient_row_get_search_term (GR_INGREDIENT_ROW (row));
                if (term) {
                        g_string_append_c (s, ' ');
                        g_string_append (s, term);
                }
        }
        g_list_free (children);

        gr_search_page_update_search (GR_SEARCH_PAGE (window->search_page), s->str);
}

static void
search_mode_enabled (GrWindow   *window,
                     GParamSpec *pspec)
{
        const char *visible;
        g_autofree char *terms = NULL;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (window->search_bar)))
                return;

        if (strcmp (visible, "ingredients") != 0)
               return;

       switch_to_search (window);

       terms = gr_ingredients_page_get_search_terms (GR_INGREDIENTS_PAGE (window->ingredients_page));
       gtk_entry_set_text (GTK_ENTRY (window->search_entry), terms);
}

static void
update_cooking_button (GrWindow *window,
                       gboolean  cooking)
{
        GtkStyleContext *context;

        context = gtk_widget_get_style_context (window->cooking_button);

        if (cooking) {
                gtk_button_set_label (GTK_BUTTON (window->cooking_button), _("Stop cooking"));
                gtk_style_context_remove_class (context, "suggested-action");
        }
        else {
                gtk_button_set_label (GTK_BUTTON (window->cooking_button), _("Start cooking"));
                gtk_style_context_add_class (context, "suggested-action");
        }
}


static void
start_or_stop_cooking (GrWindow *window)
{
        gboolean cooking;

        cooking = gr_details_page_is_cooking (GR_DETAILS_PAGE (window->details_page));
        cooking = !cooking;
        gr_details_page_set_cooking (GR_DETAILS_PAGE (window->details_page), cooking);
        update_cooking_button (window, cooking);
}

static gboolean
window_keypress_handler (GtkWidget *widget,
                         GdkEvent  *event,
                         gpointer   data)
{
        GrWindow *window = GR_WINDOW (widget);
        GtkWidget *w;
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));
        if (strcmp (visible, "ingredients") == 0) {
                gr_ingredients_page_scroll (GR_INGREDIENTS_PAGE (window->ingredients_page),
                                            ((GdkEventKey*)event)->string);
                return GDK_EVENT_STOP;
        }

        if (strcmp (visible, "recipes") != 0 &&
            strcmp (visible, "cuisines") != 0 &&
            strcmp (visible, "search") != 0)
                return GDK_EVENT_PROPAGATE;

        /* handle ctrl+f shortcut */
        if (event->type == GDK_KEY_PRESS) {
                GdkEventKey *e = (GdkEventKey *) event;
                if ((e->state & GDK_CONTROL_MASK) > 0 && e->keyval == GDK_KEY_f) {
                        if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (window->search_bar))) {
                                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), TRUE);
                                gtk_widget_grab_focus (window->search_entry);
                        } else {
                                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);
                        }
                        return GDK_EVENT_PROPAGATE;
                }
        }

        return gtk_search_bar_handle_event (GTK_SEARCH_BAR (window->search_bar), event);
}

static void
tag_clicked (GdTaggedEntry    *entry,
             GdTaggedEntryTag *tag,
             GrWindow         *self)
{
        GtkWidget *row;
        gboolean include, exclude;

        row = GTK_WIDGET (g_object_get_data (G_OBJECT (tag), "row"));
        if (GR_IS_INGREDIENT_ROW (row)) {
                g_object_get (row, "include", &include, "exclude", &exclude, NULL);
                g_object_set (row, "include", !include, "exclude", !exclude, NULL);
        }
}

static void
tag_button_clicked (GdTaggedEntry    *entry,
                    GdTaggedEntryTag *tag,
                    GrWindow         *self)
{
        GtkWidget *row;

        row = GTK_WIDGET (g_object_get_data (G_OBJECT (tag), "row"));
        if (GR_IS_INGREDIENT_ROW (row)) {
                g_object_set (row, "include", FALSE, "exclude", FALSE, NULL);
        }
        else {
                g_object_set (row, "include", FALSE, NULL);
        }
}

static void
show_meal_search_list (GrWindow *self)
{
        gtk_widget_hide (self->meal_search_button);
        gtk_widget_show (self->meal_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->meal_search_revealer), TRUE);
}

static void
hide_meal_search_list (GrWindow *self,
                       gboolean  animate)
{
        gtk_widget_show (self->meal_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->meal_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->meal_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->meal_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
show_diet_search_list (GrWindow *self)
{
        gtk_widget_hide (self->diet_search_button);
        gtk_widget_show (self->diet_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->diet_search_revealer), TRUE);
}

static void
hide_diet_search_list (GrWindow *self,
                       gboolean  animate)
{
        gtk_widget_show (self->diet_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->diet_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->diet_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->diet_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
show_ingredients_search_list (GrWindow *self)
{
        gtk_widget_hide (self->ing_search_button);
        gtk_widget_show(self->ing_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->ing_search_revealer), TRUE);
}

static void
hide_ingredients_search_list (GrWindow *self,
                              gboolean  animate)
{
        gtk_widget_show (self->ing_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->ing_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->ing_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->ing_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
meal_search_button_clicked (GtkButton *button,
                            GrWindow  *self)
{
        hide_diet_search_list (self, TRUE);
        hide_ingredients_search_list (self, TRUE);
        show_meal_search_list (self);
}

static void
diet_search_button_clicked (GtkButton *button,
                            GrWindow  *self)
{
        hide_meal_search_list (self, TRUE);
        hide_ingredients_search_list (self, TRUE);
        show_diet_search_list (self);
}

static void
ing_search_button_clicked (GtkButton *button,
                           GrWindow  *self)
{
        hide_meal_search_list (self, TRUE);
        hide_diet_search_list (self, TRUE);
        show_ingredients_search_list (self);
}

static void
search_popover_notify (GObject    *object,
                       GParamSpec *pspec,
                       GrWindow   *self)
{
        hide_meal_search_list (self, FALSE);
        hide_diet_search_list (self, FALSE);
        hide_ingredients_search_list (self, FALSE);
}

GrWindow *
gr_window_new (GrApp *app)
{
        return g_object_new (GR_TYPE_WINDOW, "application", app, NULL);
}

static void
gr_window_finalize (GObject *object)
{
        GrWindow *self = (GrWindow *)object;

        g_queue_free_full (self->back_entry_stack, (GDestroyNotify)back_entry_free);

        G_OBJECT_CLASS (gr_window_parent_class)->finalize (object);
}

static void
gr_window_class_init (GrWindowClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_window_finalize;

        gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                     "/org/gnome/Recipes/gr-window.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_button_stack);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, recipes_search_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, recipes_search_button2);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, ingredients_search_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, ingredients_search_button2);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_bar);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_entry);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, header_stack);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, main_stack);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, recipes_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, details_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, details_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, edit_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, edit_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, list_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, list_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, cuisines_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, cuisine_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, cuisine_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, ingredients_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, ingredients_search_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, cooking_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, meal_search_revealer);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, meal_search_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, meal_list);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, diet_search_revealer);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, diet_search_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, diet_list);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, ing_search_revealer);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, ing_search_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, ingredients_list);

        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), new_recipe);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), go_back);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), add_recipe);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), visible_page_changed);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), start_or_stop_cooking);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), tag_clicked);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), tag_button_clicked);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), meal_search_button_clicked);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), diet_search_button_clicked);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), ing_search_button_clicked);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), search_popover_notify);
}

static void
ing_row_activated (GtkListBox *list, GtkListBoxRow *row, GrWindow *self)
{
        gboolean include, exclude;

        if (!GR_IS_INGREDIENT_ROW (row)) {
                GList *children, *l;

                children = gtk_container_get_children (GTK_CONTAINER (list));
                for (l = children; l; l = l->next) {
                        row = l->data;
                        if (GR_IS_INGREDIENT_ROW (row)) {
                                g_object_set (row, "include", FALSE, "exclude", FALSE, NULL);
                        }
                }

                g_list_free (children);

                hide_ingredients_search_list (self, TRUE);

                return;
        }

        g_object_get (row, "include", &include, "exclude", &exclude, NULL);

        if (!include && !exclude)
                g_object_set (row, "include", TRUE, "exclude", FALSE, NULL);
        else if (include && !exclude)
                g_object_set (row, "include", FALSE, "exclude", TRUE, NULL);
        else
                g_object_set (row, "include", FALSE, "exclude", FALSE, NULL);
}

static void
ing_header_func (GtkListBoxRow *row,
                 GtkListBoxRow *before,
                 gpointer       data)
{
        GrWindow *self = data;

        if (before != NULL && !GR_IS_INGREDIENT_ROW (before))
                gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
        else
                gtk_list_box_row_set_header (row, NULL);
}

static void
populate_ingredients_list (GrWindow *self)
{
        int i;
        const char **ingredients;
        GtkWidget *row;

        row = gtk_label_new (_("Anything"));
        g_object_set (row, "margin", 6, NULL);
        gtk_label_set_xalign (GTK_LABEL (row), 0);
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->ingredients_list), row);

        ingredients = gr_ingredient_get_names (NULL);
        for (i = 0; ingredients[i]; i++) {
                row = GTK_WIDGET (gr_ingredient_row_new (ingredients[i]));
                gtk_widget_show (row);
                gtk_container_add (GTK_CONTAINER (self->ingredients_list), row);
                gr_ingredient_row_set_entry (GR_INGREDIENT_ROW (row),
                                             GD_TAGGED_ENTRY (self->search_entry));
        }

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->ingredients_list),
                                      ing_header_func, self, NULL);

        g_signal_connect (self->ingredients_list, "row-activated", G_CALLBACK (ing_row_activated), self);
}

static void
diet_row_activated (GtkListBox *list, GtkListBoxRow *row, GrWindow *self)
{
        gboolean include;

        if (!GR_IS_DIET_ROW (row)) {
                GList *children, *l;

                children = gtk_container_get_children (GTK_CONTAINER (list));
                for (l = children; l; l = l->next) {
                        row = l->data;
                        if (GR_IS_DIET_ROW (row)) {
                                g_object_set (row, "include", FALSE, NULL);
                        }
                }
                g_list_free (children);

                hide_diet_search_list (self, TRUE);

                return;
        }

        g_object_get (row, "include", &include, NULL);
        g_object_set (row, "include", !include, NULL);
}

static void
diet_header_func (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       data)
{
        GrWindow *self = data;

        if (before != NULL && !GR_IS_DIET_ROW (before))
                gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
        else
                gtk_list_box_row_set_header (row, NULL);
}

static void
populate_diets_list (GrWindow *self)
{
        int i;
        GtkWidget *row;

        row = gtk_label_new (_("No restrictions"));
        g_object_set (row, "margin", 6, NULL);
        gtk_label_set_xalign (GTK_LABEL (row), 0);
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_GLUTEN_FREE));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->search_entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_NUT_FREE));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->search_entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_VEGAN));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->search_entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_VEGETARIAN));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->search_entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_MILK_FREE));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->search_entry));

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->diet_list),
                                      diet_header_func, self, NULL);

        g_signal_connect (self->diet_list, "row-activated", G_CALLBACK (diet_row_activated), self);
}

static void
meal_row_activated (GtkListBox *list, GtkListBoxRow *row, GrWindow *self)
{
        gboolean include;

        if (!GR_IS_MEAL_ROW (row)) {
                GList *children, *l;

                children = gtk_container_get_children (GTK_CONTAINER (list));
                for (l = children; l; l = l->next) {
                        row = l->data;
                        if (GR_IS_MEAL_ROW (row)) {
                                g_object_set (row, "include", FALSE, NULL);
                        }
                }
                g_list_free (children);

                hide_meal_search_list (self, TRUE);

                return;
        }

        g_object_get (row, "include", &include, NULL);
        g_object_set (row, "include", !include, NULL);
}

static void
meal_header_func (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       data)
{
        GrWindow *self = data;

        if (before != NULL && !GR_IS_MEAL_ROW (before))
                gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
        else
                gtk_list_box_row_set_header (row, NULL);
}

static void
populate_meals_list (GrWindow *self)
{
        int i;
        const char **names;
        GtkWidget *row;
        int length;

        row = gtk_label_new (_("Any meal"));
        g_object_set (row, "margin", 6, NULL);
        gtk_label_set_xalign (GTK_LABEL (row), 0);
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->meal_list), row);

        names = gr_category_get_names (&length);
        for (i = 0; i < length; i++) {
                row = GTK_WIDGET (gr_meal_row_new (names[i]));
                gtk_widget_show (row);
                gtk_container_add (GTK_CONTAINER (self->meal_list), row);
                gr_meal_row_set_entry (GR_MEAL_ROW (row), GD_TAGGED_ENTRY (self->search_entry));
        }

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->meal_list),
                                      meal_header_func, self, NULL);

        g_signal_connect (self->meal_list, "row-activated", G_CALLBACK (meal_row_activated), self);
}

static void
gr_window_init (GrWindow *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->back_entry_stack = g_queue_new ();

        g_object_bind_property (self->recipes_search_button, "active",
                                self->search_bar, "search-mode-enabled",
                                G_BINDING_BIDIRECTIONAL);

        g_signal_connect_swapped (self->search_bar, "notify::search-mode-enabled", G_CALLBACK (search_mode_enabled), self);
        g_signal_connect_swapped (self->search_entry, "search-changed", G_CALLBACK (search_changed), self);
        g_signal_connect_swapped (self->search_entry, "stop-search", G_CALLBACK (stop_search), self);
        g_signal_connect (self->recipes_search_button2, "notify::active", G_CALLBACK (maybe_stop_search), self);
        g_signal_connect (self->ingredients_search_button, "clicked", G_CALLBACK (ingredients_search_clicked), self);
        g_signal_connect (self->ingredients_search_button2, "clicked", G_CALLBACK (ingredients_search_clicked), self);
        g_signal_connect_after (self, "key-press-event", G_CALLBACK (window_keypress_handler), NULL);

        gtk_search_bar_connect_entry (GTK_SEARCH_BAR (self->search_bar), GTK_ENTRY (self->search_entry));

        populate_meals_list (self);
        populate_diets_list (self);
        populate_ingredients_list (self);
}

void
gr_window_go_back (GrWindow *window)
{
        if (g_queue_is_empty (window->back_entry_stack)) {
        	gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "recipes");
        	gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "recipes");
        }
        else {
                go_back (window);
        }
}

void
gr_window_show_recipe (GrWindow *window,
                       GrRecipe *recipe)
{
        const char *name;

        save_back_entry (window);

        gr_details_page_set_recipe (GR_DETAILS_PAGE (window->details_page), recipe);

        update_cooking_button (window, gr_details_page_is_cooking (GR_DETAILS_PAGE (window->details_page)));

        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);

        name = gr_recipe_get_name (recipe);
        gtk_header_bar_set_title (GTK_HEADER_BAR (window->details_header), name);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "details");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "details");
}

void
gr_window_edit_recipe (GrWindow *window,
                       GrRecipe *recipe)
{
        const char *name;

        save_back_entry (window);

        gr_edit_page_edit (GR_EDIT_PAGE (window->edit_page), recipe);

        name = gr_recipe_get_name (recipe);
        gtk_header_bar_set_title (GTK_HEADER_BAR (window->edit_header), name);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "edit");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "edit");
}

void
gr_window_show_diet (GrWindow   *window,
                     const char *title,
                     GrDiets     diet)
{
        save_back_entry (window);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->list_header), title);
        gr_list_page_populate_from_diet (GR_LIST_PAGE (window->list_page), diet);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "list");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_chef (GrWindow *window,
                     GrChef   *chef)
{
        g_autofree char *title = NULL;
        const char *name;

        save_back_entry (window);

        name = gr_chef_get_name (chef);
        title = g_strdup_printf (_("Recipes by %s"), name);
        gtk_header_bar_set_title (GTK_HEADER_BAR (window->list_header), title);
        gr_list_page_populate_from_chef (GR_LIST_PAGE (window->list_page), chef);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "list");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_favorites (GrWindow *window)
{
        const char *title;

        save_back_entry (window);

        title = _("Favorite recipes");
        gtk_header_bar_set_title (GTK_HEADER_BAR (window->list_header), title);
        gr_list_page_populate_from_favorites (GR_LIST_PAGE (window->list_page));

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "list");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_myself (GrWindow *window)
{
        const char *title;
        const char *name;
        g_autoptr(GrChef) chef = NULL;
        GrRecipeStore *store;

        save_back_entry (window);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        name = gr_recipe_store_get_user_key (store);
        title = _("My own recipes");
        chef = gr_recipe_store_get_chef (store, name);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->list_header), title);
        gr_list_page_populate_from_chef (GR_LIST_PAGE (window->list_page), chef);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "list");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_cuisine (GrWindow   *window,
                        const char *cuisine,
                        const char *title)
{
	save_back_entry (window);
        gr_cuisine_page_set_cuisine (GR_CUISINE_PAGE (window->cuisine_page), cuisine);
        gtk_header_bar_set_title (GTK_HEADER_BAR (window->cuisine_header), title);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "cuisine");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "cuisine");
}

void
gr_window_show_search_by_ingredients (GrWindow   *window,
                                      const char *ingredient)
{
        gr_ingredients_search_page_set_ingredient (GR_INGREDIENTS_SEARCH_PAGE (window->ingredients_search_page), ingredient);
        switch_to_ingredients_search (window);
}
