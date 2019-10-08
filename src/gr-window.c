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

#include <gtk/gtk.h>

#include "gr-window.h"
#include "gr-details-page.h"
#include "gr-chef-dialog.h"
#include "gr-edit-page.h"
#include "gr-list-page.h"
#include "gr-cuisine-page.h"
#include "gr-cooking-page.h"
#include "gr-search-page.h"
#include "gr-shopping-page.h"
#include "gr-recipes-page.h"
#include "gr-cuisines-page.h"
#include "gr-image-page.h"
#include "gr-query-editor.h"
#include "gr-recipe-store.h"
#include "gr-recipe-importer.h"
#include "gr-recipe-exporter.h"
#include "gr-about-dialog.h"
#include "gr-account.h"
#include "gr-utils.h"
#include "gr-appdata.h"
#include "gr-settings.h"
#include "gr-shopping-list-exporter.h"


struct _GrWindow
{
        GtkApplicationWindow parent_instance;

        GtkWidget *header;
        GtkWidget *header_start_stack;
        GtkWidget *header_title_stack;
        GtkWidget *header_end_stack;
        GtkWidget *back_button;
        GtkWidget *search_button;
        GtkWidget *cooking_button;
        GtkWidget *save_button;
        GtkWidget *search_bar;
        GtkWidget *main_stack;
        GtkWidget *recipes_page;
        GtkWidget *details_page;
        GtkWidget *edit_page;
        GtkWidget *list_page;
        GtkWidget *transient_list_page;
        GtkWidget *chef_page;
        GtkWidget *shopping_page;
        GtkWidget *search_page;
        GtkWidget *cuisines_page;
        GtkWidget *cuisine_page;
        GtkWidget *image_page;
        GtkWidget *cooking_page;

        GtkWidget *sort_by_label;
        GtkWidget *sort_by_name_button;
        GtkWidget *sort_by_recency_button;
        GtkWidget *sort_popover;

        GtkWidget *undo_revealer;
        GtkWidget *undo_label;
        GrRecipe  *undo_recipe;
        guint undo_timeout_id;

        GtkWidget *remind_revealer;
        GtkWidget *remind_label;
        GrRecipe  *remind_recipe;
        guint remind_timeout_id;

        GtkWidget *shopping_added_revealer;
        guint shopping_timeout_id;

        GtkWidget *shopping_done_revealer;
        guint shopping_done_timeout_id;
        GList *shopping_done_list;
        char **removed_ingredients;

        GtkWidget *export_done_revealer;
        guint export_done_timeout_id;

        GObject *file_chooser;
        GrRecipeImporter *importer;
        GrRecipeExporter *exporter;

        GtkWidget *chef_dialog;
        GtkWidget *about_dialog;
        GtkWidget *news_dialog;

        GList *dialogs;

        GQueue *back_entry_stack;
        gboolean is_fullscreen;
};

G_DEFINE_TYPE (GrWindow, gr_window, GTK_TYPE_APPLICATION_WINDOW)


static void
configure_window (GrWindow *window,
                  const char *title,
                  const char *start,
                  const char *middle,
                  const char *end,
                  const char *main)
{
        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), title);
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), start);
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), middle);
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), end);
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), main);
}


/* Back button theory:
 *
 * We maintain a stack of entries so we can go back multiple steps if
 * the need arises. For each step, we need to record enough information
 * to properly repopulate the page, including the headerbar and the
 * state of the search bar.
 *
 * We don't want a full browser-style history here. Instead, navigation
 * works somewhat stratified:
 *
 * - The 'main views': recipes, cuisines, search. You move between these
 *   without any back information getting recorded.
 *
 * - Hierarchical navigation for categories, chefs and similar
 *   'list-of-recipes' pages, as well as for the cuisine view. If you go
 *   from a main view to a list page to an individual recipe, you can
 *   retrace your steps with the back button.
 *
 * - Some lists are 'transient': they don't get added to the stack.
 *   Examples are 'Imported recipes' and 'Ready to cook'. If you go from
 *   these to an individual recipe and back, you end up on a main view
 *
 * - Individual recipe pages: details, edit and new. If you save a recipe,
 *   we move you to the details page. Apart from that, these pages are
 *   not participating in the back stack. If you navigate from a details
 *   page to the chef page, the back button will bring you to the main view,
 *   not back to the recipe you came from.
 */
typedef struct
{
        char *page;
        char *header_start_child;
        char *header_title_child;
        char *header_end_child;
        char *header_title;
        char **search;
} BackEntry;

static void
back_entry_free (BackEntry *entry)
{
        g_free (entry->page);
        g_free (entry->header_start_child);
        g_free (entry->header_title_child);
        g_free (entry->header_end_child);
        g_free (entry->header_title);
        g_strfreev (entry->search);
        g_free (entry);
}

static void
save_back_entry (GrWindow *window)
{
        BackEntry *entry;
        const char *page;

        page = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));
        if (strcmp (page, "details") == 0 ||
            strcmp (page, "cooking") == 0 ||
            strcmp (page, "image") == 0 ||
            strcmp (page, "transient") == 0 ||
            strcmp (page, "shopping") == 0 ||
            strcmp (page, "edit") == 0)
                return;

        entry = g_new (BackEntry, 1);
        entry->page = g_strdup (page);
        entry->header_start_child = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->header_start_stack)));
        entry->header_title_child = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->header_title_stack)));
        entry->header_end_child = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->header_end_stack)));
        entry->header_title = g_strdup (gtk_header_bar_get_title (GTK_HEADER_BAR (window->header)));

        if (strcmp (entry->page, "search") == 0)
                entry->search = g_strdupv ((char **)gr_query_editor_get_terms (GR_QUERY_EDITOR (window->search_bar)));
        else
                entry->search = NULL;

        g_queue_push_head (window->back_entry_stack, entry);
}

static void
go_back (GrWindow *window)
{
        BackEntry *entry;

        entry = g_queue_pop_head (window->back_entry_stack);

        gr_window_set_fullscreen (window, FALSE);

        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);

        configure_window (window,
                          entry->header_title,
                          entry->header_start_child,
                          entry->header_title_child,
                          entry->header_end_child,
                          entry->page);

        if (strcmp (entry->page, "search") == 0) {
                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), TRUE);
                gr_query_editor_set_terms (GR_QUERY_EDITOR (window->search_bar), (const char **)entry->search);
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
        gtk_widget_grab_focus (window->edit_page);

        configure_window (window, _("Add a New Recipe"), "back", "title", "edit", "edit");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "edit");
        gtk_widget_set_sensitive (window->save_button,FALSE);
}

static void
add_recipe (GrWindow *window)
{
        if (gr_edit_page_save (GR_EDIT_PAGE (window->edit_page))) {
                GrRecipe *recipe;

                recipe = gr_edit_page_get_recipe (GR_EDIT_PAGE (window->edit_page));
                gr_window_show_recipe (window, recipe);

                if (gr_recipe_is_contributed (recipe))
                        gr_window_offer_contribute (window, recipe);

                gr_edit_page_clear (GR_EDIT_PAGE (window->edit_page));
        }
}

void
gr_window_set_fullscreen (GrWindow *window,
                          gboolean  fullscreen)
{
        if (fullscreen)
                gtk_window_fullscreen (GTK_WINDOW (window));
        else
                gtk_window_unfullscreen (GTK_WINDOW (window));
        window->is_fullscreen = fullscreen;
}

static void
stop_search (GrWindow *window)
{
        gr_window_go_back (window);
}

static void
search_mode_changed (GrWindow *window)
{
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (window->search_bar)) &&
            strcmp (visible, "search") == 0) {
                stop_search (window);
        }
}

static void
switch_to_search (GrWindow *window)
{
        configure_window (window, "", "main", "main", "list", "search");
}

void
gr_window_show_search (GrWindow   *window,
                       const char *search)
{
        g_auto(GStrv) terms = NULL;

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->search_button), TRUE);
        switch_to_search (window);
        terms = g_new0 (char *, 2);
        terms[0] = g_strdup (search);
        gr_query_editor_set_terms (GR_QUERY_EDITOR (window->search_bar), (const char **)terms);
}

static void search_changed (GrWindow *window);

static void
visible_page_changed (GrWindow *window)
{
        const char *visible;
        gboolean searchable;
        gboolean fullscreen;
        GAction *action;
        const char *actions[] = {
                "chef-information",
                "about",
                "news",
                "import",
                "export",
                "details",
                "quit",
                "report-issue"
        };
        int i;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        searchable = strcmp (visible, "recipes") == 0 ||
                     strcmp (visible, "cuisines") == 0 ||
                     strcmp (visible, "search") == 0;
        fullscreen = strcmp (visible, "image") == 0 ||
                     strcmp (visible, "cooking") == 0;

        action = g_action_map_lookup_action (G_ACTION_MAP (g_application_get_default ()), "search");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), searchable);

        for (i = 0; i < G_N_ELEMENTS (actions); i++) {
                action = g_action_map_lookup_action (G_ACTION_MAP (g_application_get_default ()), actions[i]);
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !fullscreen);
        }

        if (strcmp (visible, "search") != 0) {
                g_signal_handlers_block_by_func (window->search_bar, search_changed, window);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->search_button), FALSE);
                g_signal_handlers_unblock_by_func (window->search_bar, search_changed, window);
        }

        if (strcmp (visible, "edit") != 0) {
                gr_edit_page_clear (GR_EDIT_PAGE (window->edit_page));
        }

        if (strcmp (visible, "chef") == 0) {
                gr_list_page_repopulate (GR_LIST_PAGE (window->chef_page));
        }
        else {
                gr_list_page_clear (GR_LIST_PAGE (window->chef_page));
        }

        if (strcmp (visible, "list") == 0) {
                gr_list_page_repopulate (GR_LIST_PAGE (window->list_page));
        }
        else {
                gr_list_page_clear (GR_LIST_PAGE (window->list_page));
        }

        if (strcmp (visible, "recipes") == 0) {
                gr_recipes_page_refresh (GR_RECIPES_PAGE (window->recipes_page));
                gr_recipes_page_unexpand (GR_RECIPES_PAGE (window->recipes_page));
                gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), _("Recipes"));
                gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "main");
        }

        if (strcmp (visible, "cuisines") == 0) {
                gr_cuisines_page_refresh (GR_CUISINES_PAGE (window->cuisines_page));
                gr_cuisines_page_unexpand (GR_CUISINES_PAGE (window->cuisines_page));
                gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), _("Cuisines"));
                gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "main");
        }
}

static void
search_changed (GrWindow *window)
{
        const char *visible;
        const char **terms;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "search") != 0)
                switch_to_search (window);

        terms = gr_query_editor_get_terms (GR_QUERY_EDITOR (window->search_bar));
        gr_search_page_update_search (GR_SEARCH_PAGE (window->search_page), terms);
}

static void
start_cooking (GrWindow *window)
{
        GrRecipe *recipe;

        recipe = gr_details_page_get_recipe (GR_DETAILS_PAGE (window->details_page));
        gr_cooking_page_set_recipe (GR_COOKING_PAGE (window->cooking_page), recipe);
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "cooking");
        gr_cooking_page_start_cooking (GR_COOKING_PAGE (window->cooking_page));
}

void
gr_window_timer_expired (GrWindow *window,
                         GrRecipe *recipe,
                         int       step)
{
        gr_cooking_page_set_recipe (GR_COOKING_PAGE (window->cooking_page), recipe);
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "cooking");
        gr_cooking_page_timer_expired (GR_COOKING_PAGE (window->cooking_page), step);
}

static gboolean
window_keypress_handler_before (GtkWidget *widget,
                                GdkEvent  *event,
                                gpointer   data)
{
        GrWindow *window = GR_WINDOW (widget);
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "cooking") == 0)
                return gr_cooking_page_handle_event (GR_COOKING_PAGE (window->cooking_page), event);

        return GDK_EVENT_PROPAGATE;
}

static gboolean
window_keypress_handler_after (GtkWidget *widget,
                               GdkEvent  *event,
                               gpointer   data)
{
        GrWindow *window = GR_WINDOW (widget);
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "recipes") == 0 ||
            strcmp (visible, "cuisines") == 0 ||
            strcmp (visible, "search") == 0)
                return gr_query_editor_handle_event (GR_QUERY_EDITOR (window->search_bar), event);

        return GDK_EVENT_PROPAGATE;
}

static gboolean
window_buttonpress_handler (GtkWidget *widget,
                            GdkEvent  *event,
                            gpointer   data)
{
        GrWindow *window = GR_WINDOW (widget);
        const char *visible;
        GdkEventButton *e = (GdkEventButton *) event;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "cooking") == 0)
          return gr_cooking_page_handle_event (GR_COOKING_PAGE (window->cooking_page), event);

        /* handle mouse back button like a click on our actual back button */
        if (e->button == 8 &&
            gtk_widget_can_activate_accel (window->back_button,
                                           g_signal_lookup ("clicked", GTK_TYPE_BUTTON))) {
                gr_window_go_back (window);
                return GDK_EVENT_STOP;
        }

        return GDK_EVENT_PROPAGATE;
}

static void
window_mapped_handler (GtkWidget *widget)
{
        GrWindow *window = GR_WINDOW (widget);

        gr_recipes_page_unexpand (GR_RECIPES_PAGE (window->recipes_page));
        gr_cuisines_page_unexpand (GR_CUISINES_PAGE (window->cuisines_page));
}

static void
response_cb (GtkDialog *dialog,
             int        response,
             gpointer   data)
{
        GrWindow *window = data;

        if (response == GTK_RESPONSE_YES)
                gtk_widget_destroy (GTK_WIDGET (window));
        else
                gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
window_delete_handler (GtkWidget *widget)
{
        GrWindow *window = GR_WINDOW (widget);
        const char *visible;
        gboolean unsaved;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));
        g_object_get (window->edit_page, "unsaved", &unsaved, NULL);

        if (strcmp (visible, "edit") == 0 && unsaved) {
                GtkWidget *dialog;
                GrRecipe *recipe;
                g_autofree char *text = NULL;

                recipe = gr_edit_page_get_recipe (GR_EDIT_PAGE (window->edit_page));
                if (recipe)
                        text = g_strdup_printf (_("The “%s” recipe has unsaved changes.\nClose the window anyway?"),
                                                gr_recipe_get_name (recipe));
                else
                        text = g_strdup (_("The new recipe has not been saved.\nClose the window anyway?"));

                dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                                 GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_QUESTION,
                                                 GTK_BUTTONS_YES_NO,
                                                 "%s", text);
                g_signal_connect (dialog, "response", G_CALLBACK (response_cb), window);
                gtk_widget_show (dialog);

                return TRUE;
        }

        return FALSE;
}

GrWindow *
gr_window_new (GrApp *app)
{
        return g_object_new (GR_TYPE_WINDOW, "application", app, NULL);
}

typedef struct {
        GrRecipe *recipe;
        double yield;
} ShoppingListEntry;

static void
shopping_list_entry_free (gpointer data)
{
        ShoppingListEntry *entry = data;

        g_object_unref (entry->recipe);
        g_free (entry);
}

static void
gr_window_finalize (GObject *object)
{
        GrWindow *self = (GrWindow *)object;

        g_queue_free_full (self->back_entry_stack, (GDestroyNotify)back_entry_free);

        g_clear_object (&self->importer);
        g_clear_object (&self->exporter);

        g_clear_object (&self->undo_recipe);
        if (self->undo_timeout_id) {
                g_source_remove (self->undo_timeout_id);
                self->undo_timeout_id = 0;
        }
        g_clear_object (&self->remind_recipe);
        if (self->remind_timeout_id) {
                g_source_remove (self->remind_timeout_id);
                self->remind_timeout_id = 0;
        }
        if (self->shopping_timeout_id) {
                g_source_remove (self->shopping_timeout_id);
                self->shopping_timeout_id = 0;
        }

        g_list_free_full (self->shopping_done_list, shopping_list_entry_free);
        g_strfreev (self->removed_ingredients);

        if (self->shopping_done_timeout_id) {
                g_source_remove (self->shopping_done_timeout_id);
                self->shopping_done_timeout_id = 0;
        }
        if (self->export_done_timeout_id) {
                g_source_remove (self->export_done_timeout_id);
                self->export_done_timeout_id = 0;
        }

        g_list_free (self->dialogs);

        G_OBJECT_CLASS (gr_window_parent_class)->finalize (object);
}

static void
close_undo (GrWindow *window)
{
        if (window->undo_timeout_id) {
                g_source_remove (window->undo_timeout_id);
                window->undo_timeout_id = 0;
        }

        g_clear_object (&window->undo_recipe);

        gtk_revealer_set_reveal_child (GTK_REVEALER (window->undo_revealer), FALSE);
}

static void
do_undo (GrWindow *window)
{
        GrRecipeStore *store;
        g_autoptr(GrRecipe) recipe = NULL;

        recipe = g_object_ref (window->undo_recipe);

        store = gr_recipe_store_get ();

        gr_recipe_store_add_recipe (store, window->undo_recipe, NULL);
        close_undo (window);

        gr_window_show_recipe (window, recipe);
}

static gboolean
undo_timeout (gpointer data)
{
        GrWindow *window = data;

        close_undo (window);

        return G_SOURCE_REMOVE;
}

void
gr_window_offer_undelete (GrWindow *window,
                          GrRecipe *recipe)
{
        g_autofree char *tmp = NULL;

        g_set_object (&window->undo_recipe, recipe);
        tmp = g_strdup_printf (_("Recipe “%s” deleted"), gr_recipe_get_name (recipe));
        gtk_label_set_label (GTK_LABEL (window->undo_label), tmp);

        gtk_revealer_set_reveal_child (GTK_REVEALER (window->undo_revealer), TRUE);
        window->undo_timeout_id = g_timeout_add_seconds (10, undo_timeout, window);
}

static void
close_remind (GrWindow *window)
{
        if (window->remind_timeout_id) {
                g_source_remove (window->remind_timeout_id);
                window->remind_timeout_id = 0;
        }

        g_clear_object (&window->remind_recipe);

        gtk_revealer_set_reveal_child (GTK_REVEALER (window->remind_revealer), FALSE);
}

static void
do_remind (GrWindow *window)
{
        g_autoptr(GrRecipe) recipe = NULL;

        recipe = g_object_ref (window->remind_recipe);

        close_remind (window);

        gr_window_show_recipe (window, recipe);
        gr_details_page_contribute_recipe (GR_DETAILS_PAGE (window->details_page));
}

static gboolean
remind_timeout (gpointer data)
{
        GrWindow *window = data;

        close_remind (window);

        return G_SOURCE_REMOVE;
}

void
gr_window_offer_contribute (GrWindow *window,
                            GrRecipe *recipe)
{
        g_autofree char *tmp = NULL;

        g_set_object (&window->remind_recipe, recipe);
        tmp = g_strdup_printf (_("You updated your contributed “%s” recipe. Send an update?"), gr_recipe_get_name (recipe));
        gtk_label_set_label (GTK_LABEL (window->remind_label), tmp);

        gtk_revealer_set_reveal_child (GTK_REVEALER (window->remind_revealer), TRUE);
        window->remind_timeout_id = g_timeout_add_seconds (10, remind_timeout, window);
}

static void
close_shopping_added (GrWindow *window)
{
        if (window->shopping_timeout_id) {
                g_source_remove (window->shopping_timeout_id);
                window->shopping_timeout_id = 0;
        }

        gtk_revealer_set_reveal_child (GTK_REVEALER (window->shopping_added_revealer), FALSE);
}

static gboolean
shopping_timeout (gpointer data)
{
        GrWindow *window = data;

        close_shopping_added (window);

        return G_SOURCE_REMOVE;
}

static void
do_shopping_list (GrWindow *window)
{
        close_shopping_added (window);
        gr_window_show_shopping (window);
}

void
gr_window_offer_shopping (GrWindow *window)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (window->shopping_added_revealer), TRUE);
        window->shopping_timeout_id = g_timeout_add_seconds (10, shopping_timeout, window);
}

static void
shopping_title_changed (GrWindow *window)
{
        const char *page;

        page = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (page, "shopping") == 0) {
                g_autofree char *title = NULL;

                g_object_get (window->shopping_page, "title", &title, NULL);
                gtk_window_set_title (GTK_WINDOW (window), title);
        }
}

static void
close_shopping_done (GrWindow *window)
{
        if (window->shopping_done_timeout_id) {
                g_source_remove (window->shopping_done_timeout_id);
                window->shopping_done_timeout_id = 0;
        }

        g_list_free_full (window->shopping_done_list, shopping_list_entry_free);
        window->shopping_done_list = NULL;
        g_strfreev (window->removed_ingredients);
        window->removed_ingredients = NULL;

        gtk_revealer_set_reveal_child (GTK_REVEALER (window->shopping_done_revealer), FALSE);
}

static gboolean
shopping_done_timeout (gpointer data)
{
        GrWindow *window = data;

        close_shopping_done (window);

        return G_SOURCE_REMOVE;
}

static void
back_to_shopping (GrWindow *window)
{
        GrRecipeStore *store;
        GList *l;
        GList *items;
        int i;
        GrShoppingListExporter *exporter = NULL;

        store = gr_recipe_store_get ();

        for (l = window->shopping_done_list; l; l = l->next) {
                ShoppingListEntry *entry = l->data;
                gr_recipe_store_add_to_shopping (store, entry->recipe, entry->yield);
        }
        items = get_ingredients (GR_SHOPPING_PAGE (window->shopping_page));
        if (!exporter) {
                GtkWidget *shopping_window;

                shopping_window = gtk_widget_get_ancestor (GTK_WIDGET (window->shopping_page), GTK_TYPE_APPLICATION_WINDOW);
                exporter = gr_shopping_list_exporter_new (GTK_WINDOW (shopping_window));
                do_undo_in_todoist (exporter, items);
        }
        for (i = 0; window->removed_ingredients && window->removed_ingredients[i]; i++) {
                gr_recipe_store_remove_shopping_ingredient (store, window->removed_ingredients[i]);
        }

        close_shopping_done (window);
        gr_window_show_shopping (window);
}

static void
gr_window_offer_cooking (GrWindow *window)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (window->shopping_done_revealer), TRUE);
        window->shopping_done_timeout_id = g_timeout_add_seconds (10, shopping_done_timeout, window);
}

static void gr_window_show_transient_list (GrWindow   *window,
                                           const char *title,
                                           GList      *recipes);

static void
done_shopping (GrWindow *window)
{
        GList *recipes, *l;
        GrRecipeStore *store;
        const char **removed_ingredients;
        GrShoppingListExporter *exporter = NULL;

        store = gr_recipe_store_get ();

        recipes = gr_recipe_store_get_shopping_list (store);
        removed_ingredients = gr_recipe_store_get_removed_shopping_ingredients (store);

        if (recipes->next == NULL)
                gr_window_show_recipe (window, recipes->data);
        else
                gr_window_show_transient_list (window, _("Ready to Cook!"), recipes);

        gr_window_offer_cooking (window);

        g_list_free_full (window->shopping_done_list, shopping_list_entry_free);
        window->shopping_done_list = NULL;

        for (l = recipes; l; l = l->next) {
                GrRecipe *recipe = l->data;
                ShoppingListEntry *entry;

                entry = g_new (ShoppingListEntry, 1);
                entry->recipe = g_object_ref (recipe);
                entry->yield = gr_recipe_store_get_shopping_yield (store, recipe);
                window->shopping_done_list = g_list_append (window->shopping_done_list, entry);
        }
        g_list_free_full (recipes, g_object_unref);

        g_strfreev (window->removed_ingredients);
        window->removed_ingredients = g_strdupv ((char **)removed_ingredients);

        gr_recipe_store_clear_shopping_list (store);
        if (!exporter) {
                GtkWidget *shopping_window;

                shopping_window = gtk_widget_get_ancestor (GTK_WIDGET (window->shopping_page), GTK_TYPE_APPLICATION_WINDOW);
                exporter = gr_shopping_list_exporter_new (GTK_WINDOW (shopping_window));
                done_shopping_in_todoist (exporter);
        }
}


static void
close_export_done (GrWindow *window)
{
    if (window->export_done_timeout_id) {
            g_source_remove (window->export_done_timeout_id);
            window->export_done_timeout_id = 0;
    }

    gtk_revealer_set_reveal_child (GTK_REVEALER (window->export_done_revealer), FALSE);
}

static gboolean
export_done_timeout (gpointer data)
{
        GrWindow *window = data;

        close_export_done (window);

        return G_SOURCE_REMOVE;
}

void
gr_window_confirm_shopping_exported (GrWindow *window)
{
    gtk_revealer_set_reveal_child (GTK_REVEALER (window->export_done_revealer), TRUE);
    window->export_done_timeout_id = g_timeout_add_seconds (10, export_done_timeout, window);
}

static void
make_save_sensitive (GrEditPage *edit_page,
                     GParamSpec *pspec,
                     gpointer    data)
{
        GrWindow *window = GR_WINDOW (data);
        gboolean unsaved;

        g_object_get (G_OBJECT (edit_page), "unsaved", &unsaved, NULL);
        if (unsaved)
                gtk_widget_set_sensitive (window->save_button,TRUE);
        else
                gtk_widget_set_sensitive (window->save_button,FALSE);
}

static void
update_sort_menu (GrWindow *window)
{
        switch (g_settings_get_enum (gr_settings_get (), "sort-key")) {
        case SORT_BY_NAME:
                g_object_set (window->sort_by_name_button, "active", TRUE, NULL);
                g_object_set (window->sort_by_recency_button, "active", FALSE, NULL);
                g_object_set (window->sort_by_label, "label", _("Sorted by Name"), NULL);
                break;
        case SORT_BY_RECENCY:
                g_object_set (window->sort_by_name_button, "active", FALSE, NULL);
                g_object_set (window->sort_by_recency_button, "active", TRUE, NULL);
                g_object_set (window->sort_by_label, "label", _("Sorted by Recency"), NULL);
                break;
        default:
                g_assert_not_reached ();
        }
}

static void
sort_clicked (GtkWidget *button,
              GrWindow  *window)
{
        GSettings *settings = gr_settings_get ();

        gtk_popover_popdown (GTK_POPOVER (window->sort_popover));
        g_settings_set_enum (settings, "sort-key",
                             button == window->sort_by_name_button ? SORT_BY_NAME : SORT_BY_RECENCY);
        update_sort_menu (window);
}

static void
gr_window_class_init (GrWindowClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = gr_window_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-window.ui");

        gtk_widget_class_bind_template_child (widget_class, GrWindow, header);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, header_start_stack);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, header_title_stack);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, header_end_stack);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, back_button);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, search_button);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, cooking_button);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, save_button);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, search_bar);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, main_stack);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, recipes_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, details_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, edit_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, list_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, transient_list_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, chef_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, shopping_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, search_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, cuisines_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, cuisine_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, image_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, cooking_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, undo_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, undo_label);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, remind_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, remind_label);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, shopping_added_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, shopping_done_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, export_done_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, sort_by_label);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, sort_by_name_button);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, sort_by_recency_button);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, sort_popover);

        gtk_widget_class_bind_template_callback (widget_class, new_recipe);
        gtk_widget_class_bind_template_callback (widget_class, go_back);
        gtk_widget_class_bind_template_callback (widget_class, add_recipe);
        gtk_widget_class_bind_template_callback (widget_class, visible_page_changed);
        gtk_widget_class_bind_template_callback (widget_class, start_cooking);
        gtk_widget_class_bind_template_callback (widget_class, search_changed);
        gtk_widget_class_bind_template_callback (widget_class, stop_search);
        gtk_widget_class_bind_template_callback (widget_class, search_mode_changed);
        gtk_widget_class_bind_template_callback (widget_class, window_keypress_handler_before);
        gtk_widget_class_bind_template_callback (widget_class, window_keypress_handler_after);
        gtk_widget_class_bind_template_callback (widget_class, window_buttonpress_handler);
        gtk_widget_class_bind_template_callback (widget_class, window_mapped_handler);
        gtk_widget_class_bind_template_callback (widget_class, window_delete_handler);
        gtk_widget_class_bind_template_callback (widget_class, do_undo);
        gtk_widget_class_bind_template_callback (widget_class, close_undo);
        gtk_widget_class_bind_template_callback (widget_class, do_remind);
        gtk_widget_class_bind_template_callback (widget_class, close_remind);
        gtk_widget_class_bind_template_callback (widget_class, do_shopping_list);
        gtk_widget_class_bind_template_callback (widget_class, close_shopping_added);
        gtk_widget_class_bind_template_callback (widget_class, shopping_title_changed);
        gtk_widget_class_bind_template_callback (widget_class, done_shopping);
        gtk_widget_class_bind_template_callback (widget_class, close_shopping_done);
        gtk_widget_class_bind_template_callback (widget_class, back_to_shopping);
        gtk_widget_class_bind_template_callback (widget_class, make_save_sensitive);
        gtk_widget_class_bind_template_callback (widget_class, sort_clicked);
        gtk_widget_class_bind_template_callback (widget_class, close_export_done);
}

static GtkClipboard *
get_clipboard (GtkWidget *widget)
{
        return gtk_widget_get_clipboard (widget, gdk_atom_intern_static_string ("CLIPBOARD"));
}

static void
copy_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
        GtkWindow *window = GTK_WINDOW (user_data);
        GtkWidget *focus;

        focus = gtk_window_get_focus (window);
        if (GTK_IS_TEXT_VIEW (focus))
                gtk_text_buffer_copy_clipboard (gtk_text_view_get_buffer (GTK_TEXT_VIEW (focus)),
                                                get_clipboard (focus));
        else if (GTK_IS_ENTRY (focus))
                gtk_editable_copy_clipboard (GTK_EDITABLE (focus));
}

static void
paste_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
        GtkWindow *window = GTK_WINDOW (user_data);
        GtkWidget *focus;

        focus = gtk_window_get_focus (window);
        if (GTK_IS_TEXT_VIEW (focus))
                gtk_text_buffer_paste_clipboard (gtk_text_view_get_buffer (GTK_TEXT_VIEW (focus)),
                                                 get_clipboard (focus),
                                                 NULL,
                                                 TRUE);
        else if (GTK_IS_ENTRY (focus))
                gtk_editable_paste_clipboard (GTK_EDITABLE (focus));
}

static GActionEntry entries[] = {
        { "copy", copy_activated, NULL, NULL, NULL },
        { "paste", paste_activated, NULL, NULL, NULL }
};

static void
gr_window_init (GrWindow *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
        self->back_entry_stack = g_queue_new ();

        g_action_map_add_action_entries (G_ACTION_MAP (self),
                                         entries, G_N_ELEMENTS (entries),
                                         self);

        update_sort_menu (self);
}

void
gr_window_go_back (GrWindow *window)
{
        if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (window->search_bar))) {
                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);
        }
        else if (g_queue_is_empty (window->back_entry_stack)) {
                configure_window (window, _("Recipes"), "main", "main", "main", "recipes");
        }
        else {
                go_back (window);
        }
}

void
gr_window_show_recipe (GrWindow *window,
                       GrRecipe *recipe)
{
        save_back_entry (window);

        gr_details_page_set_recipe (GR_DETAILS_PAGE (window->details_page), recipe);

        g_signal_handlers_block_by_func (window->search_bar, search_mode_changed, window);
        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);
        g_signal_handlers_unblock_by_func (window->search_bar, search_mode_changed, window);

        configure_window (window, gr_recipe_get_translated_name (recipe), "back", "title", "details", "details");
}

void
gr_window_edit_recipe (GrWindow *window,
                       GrRecipe *recipe)
{
        save_back_entry (window);

        gr_edit_page_edit (GR_EDIT_PAGE (window->edit_page), recipe);
        gtk_widget_grab_focus (window->edit_page);

        configure_window (window, gr_recipe_get_translated_name (recipe), "back", "title", "edit", "edit");
        gtk_widget_set_sensitive (window->save_button, FALSE);
}

static void
done_cb (GrRecipeImporter *importer,
         GList            *recipes,
         GrWindow         *window)
{
        if (recipes)
                gr_window_show_transient_list (window, _("Imported Recipes"), recipes);
}

static void
do_import (GrWindow *window,
           GFile    *file)
{
        if (!window->importer) {
                window->importer = gr_recipe_importer_new (GTK_WINDOW (window));
                g_signal_connect (window->importer, "done", G_CALLBACK (done_cb), window);
        }

        gr_recipe_importer_import_from (window->importer, file);
}

static void
file_chooser_import_response (GtkNativeDialog *self,
                              int              response_id,
                              GrWindow        *window)
{
        if (response_id == GTK_RESPONSE_ACCEPT) {
                g_autoptr(GFile) file = NULL;
                file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self));
                do_import (window, file);
        }

        gtk_native_dialog_destroy (self);
        window->file_chooser = NULL;
}

static void
chef_done (GrChefDialog *dialog,
           GrChef       *chef,
           GrWindow     *window)
{
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
present_my_chef_dialog (GrChef   *chef,
                        gpointer  data)
{
        GrWindow *window = data;

        window->chef_dialog = (GtkWidget *)gr_chef_dialog_new (chef, FALSE);
        g_object_add_weak_pointer (G_OBJECT (window->chef_dialog), (gpointer *)&window->chef_dialog);
        gtk_window_set_title (GTK_WINDOW (window->chef_dialog), _("Chef Information"));
        g_signal_connect (window->chef_dialog, "done", G_CALLBACK (chef_done), window);
        gr_window_present_dialog (window, GTK_WINDOW (window->chef_dialog));
}

void
gr_window_show_my_chef_information (GrWindow *window)
{
        if (window->chef_dialog)
                return;

        gr_ensure_user_chef (GTK_WINDOW (window), present_my_chef_dialog, window);
}

void
gr_window_load_recipe (GrWindow *window,
                       GFile    *file)
{
        if (file) {
                do_import (window, file);
                return;
        }

        if (window->file_chooser)
                return;

        if (in_flatpak_sandbox () &&
            !portal_available (GTK_WINDOW (window), "org.freedesktop.portal.FileChooser"))
                return;

        window->file_chooser = (GObject *)gtk_file_chooser_native_new (_("Select a recipe file"),
                                                                       GTK_WINDOW (window),
                                                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                                                       _("Open"),
                                                                       _("Cancel"));
        gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (window->file_chooser), TRUE);

        g_signal_connect (window->file_chooser, "response",
                          G_CALLBACK (file_chooser_import_response), window);

        gtk_native_dialog_show (GTK_NATIVE_DIALOG (window->file_chooser));
}

static void
export_done (GrRecipeExporter *exporter,
             GFile            *file,
             GtkWidget        *window)
{
        GtkWidget *dialog;
        g_autofree char *path = NULL;

        path = g_file_get_path (file);
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_OK,
                                         _("Recipes have been exported to “%s”"), path);
        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_widget_show (dialog);
}

static void
do_export (GrWindow *window,
           GFile    *file)
{
        if (!window->exporter) {
                window->exporter = gr_recipe_exporter_new (GTK_WINDOW (window));
                g_signal_connect (window->exporter, "done", G_CALLBACK (export_done), window);
        }

        gr_recipe_exporter_export_all (window->exporter, file);
}

static void
file_chooser_export_response (GtkNativeDialog *self,
                              int              response_id,
                              GrWindow        *window)
{
        if (response_id == GTK_RESPONSE_ACCEPT) {
                g_autoptr(GFile) file = NULL;
                file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self));
                do_export (window, file);
        }

        gtk_native_dialog_destroy (self);
        window->file_chooser = NULL;
}

void
gr_window_save_all (GrWindow *window)
{
        if (window->file_chooser)
                return;

        window->file_chooser = (GObject *)gtk_file_chooser_native_new (_("Select a file"),
                                                                       GTK_WINDOW (window),
                                                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                                                       _("_Export"),
                                                                       _("_Cancel"));
        gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (window->file_chooser), TRUE);
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (window->file_chooser), "all-recipes.gnome-recipes-export");

        g_signal_connect (window->file_chooser, "response",
                          G_CALLBACK (file_chooser_export_response), window);

        gtk_native_dialog_show (GTK_NATIVE_DIALOG (window->file_chooser));
}

void
gr_window_show_diet (GrWindow   *window,
                     GrDiets     diet)
{
        save_back_entry (window);

        gr_list_page_populate_from_diet (GR_LIST_PAGE (window->list_page), diet);
        configure_window (window, gr_diet_get_label (diet), "back", "title", "list", "list");
}

void
gr_window_show_chef (GrWindow *window,
                     GrChef   *chef)
{
        g_autofree char *title = NULL;

        save_back_entry (window);

        gr_list_page_populate_from_chef (GR_LIST_PAGE (window->chef_page), chef,  FALSE);
        title = g_strdup_printf (_("Chefs: %s"), gr_chef_get_fullname (chef));
        configure_window (window, title, "back", "title", "list", "chef");
}

void
gr_window_show_favorites (GrWindow *window)
{
        save_back_entry (window);

        gr_list_page_populate_from_favorites (GR_LIST_PAGE (window->list_page));
        configure_window (window, _("Favorite Recipes"), "back", "title", "list", "list");
}

void
gr_window_show_all (GrWindow *window)
{
        save_back_entry (window);

        gr_list_page_populate_from_all (GR_LIST_PAGE (window->list_page));
        configure_window (window, _("All Recipes"), "back", "title", "list", "list");
}

void
gr_window_show_new (GrWindow *window)
{
        save_back_entry (window);

        gr_list_page_populate_from_new (GR_LIST_PAGE (window->list_page));
        configure_window (window, _("New Recipes"), "back", "title", "list", "list");
}

void
gr_window_show_list (GrWindow   *window,
                     const char *title,
                     GList      *recipes)
{
        save_back_entry (window);

        gr_list_page_populate_from_list (GR_LIST_PAGE (window->list_page), recipes);
        configure_window (window, title, "back", "title", "list", "list");
}

static void
gr_window_show_transient_list (GrWindow   *window,
                               const char *title,
                               GList      *recipes)
{
        save_back_entry (window);

        gr_list_page_populate_from_list (GR_LIST_PAGE (window->transient_list_page), recipes);
        configure_window (window, title, "back", "title", "list", "transient");
}

void
gr_window_show_shopping (GrWindow *window)
{
        save_back_entry (window);

        configure_window (window, _("Buy Ingredients"), "back", "title", "shopping", "shopping");
        gr_shopping_page_populate (GR_SHOPPING_PAGE (window->shopping_page));
}

static void
do_show_mine (GrChef   *chef,
              gpointer  data)
{
        GrWindow *window = data;

        save_back_entry (window);

        gr_list_page_populate_from_chef (GR_LIST_PAGE (window->chef_page), chef, TRUE);
        configure_window (window, _("My Recipes"), "back", "title", "list", "chef");
}

void
gr_window_show_mine (GrWindow *window)
{
        gr_ensure_user_chef (GTK_WINDOW (window), do_show_mine, window);
}

void
gr_window_show_cuisine (GrWindow   *window,
                        const char *cuisine,
                        const char *title)
{
        save_back_entry (window);

        gr_cuisine_page_set_cuisine (GR_CUISINE_PAGE (window->cuisine_page), cuisine);
        configure_window (window, title, "back", "title", "list", "cuisine");
}

void
gr_window_show_season (GrWindow   *window,
                       const char *season,
                       const char *title)
{
        save_back_entry (window);

        gr_list_page_populate_from_season (GR_LIST_PAGE (window->list_page), season);
        configure_window (window, title, "back", "title", "list", "list");
}

void
gr_window_show_image (GrWindow  *window,
                      GPtrArray *images,
                      int        index)
{
        if (images && images->len > 0) {
                gr_image_page_set_images (GR_IMAGE_PAGE (window->image_page), images);
                gr_image_page_show_image (GR_IMAGE_PAGE (window->image_page), index);
                gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "image");
                gtk_widget_grab_focus (window->image_page);
                gr_window_set_fullscreen (window, TRUE);
        }
        else {
                gr_window_set_fullscreen (window, FALSE);
                gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "details");
        }
}

static void
dialog_unmapped_cb (GtkWidget *dialog,
                    GrWindow  *window)
{
        window->dialogs = g_list_remove (window->dialogs, dialog);
}

void
gr_window_present_dialog (GrWindow  *window,
                          GtkWindow *dialog)
{
        GtkWindow *parent;

        if (window->dialogs)
                parent = window->dialogs->data;
        else
                parent = GTK_WINDOW (window);

        gtk_window_set_transient_for (dialog, parent);
        gtk_window_set_modal (dialog, TRUE);

        window->dialogs = g_list_prepend (window->dialogs, dialog);
        g_signal_connect (dialog, "unmap",
                          G_CALLBACK (dialog_unmapped_cb), window);

        gtk_window_present (dialog);
}

static void
about_response (GtkWidget *dialog,
                int        response,
                GrWindow  *window)
{
        if (response != GTK_RESPONSE_NONE) {
                gtk_widget_destroy (dialog);
                window->about_dialog = NULL;
        }
}

void
gr_window_show_about_dialog (GrWindow *window)
{
        if (window->about_dialog)
                return;

        window->about_dialog = (GtkWidget *)gr_about_dialog_new ();
        g_signal_connect (window->about_dialog, "response", G_CALLBACK (about_response), window);
        gr_window_present_dialog (window, GTK_WINDOW (window->about_dialog));
}

static gboolean
news_dialog_closed (GtkWidget *widget,
                    GdkEvent  *event,
                    gpointer   data)
{
        GrWindow *window = data;

        window->news_dialog = NULL;

        return FALSE;
}

static GtkWidget *
gr_release_info_new (const char *version,
                     GDateTime  *date,
                     const char *news)
{
        GtkWidget *box;
        GtkWidget *heading;
        GtkWidget *image;
        GtkWidget *label;
        GtkStyleContext *context;
        g_autofree char *title = NULL;

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

        heading = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_halign (heading, GTK_ALIGN_CENTER);
        image = gtk_image_new_from_icon_name ("org.gnome.Recipes-symbolic", 1);
        gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
        gtk_container_add (GTK_CONTAINER (heading), image);
        /* TRANSLATORS: %s gets replaced by a version number */
        title = g_strdup_printf (_("Recipes %s"), version);
        label = gtk_label_new (title);
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
        context = gtk_widget_get_style_context (label);
        gtk_style_context_add_class (context, "heading");
        gtk_style_context_add_class (context, "welcome");
        gtk_container_add (GTK_CONTAINER (heading), label);
        gtk_container_add (GTK_CONTAINER (box), heading);

        if (date) {
                g_autofree char *formatted = NULL;
                g_autofree char *release = NULL;
                g_autoptr(GDateTime) now = g_date_time_new_now_utc ();

                if (g_date_time_compare (date, now) < 0) {
                        formatted = g_date_time_format (date, "%F");
                        /* TRANSLATORS: %s gets replaced by a date */
                        release = g_strdup_printf (_("Released: %s"), formatted);
                }
                else {
                        release = g_strdup_printf (_("Unreleased"));
                }
                label = gtk_label_new (release);
                gtk_label_set_xalign (GTK_LABEL (label), 0.0);
                gtk_container_add (GTK_CONTAINER (box), label);
        }

        if (news) {
                g_auto(GStrv) para = NULL;
                int i;

                para = g_strsplit (news, "\n\n", -1);

                for (i = 0; para[i]; i++) {
                        label = gtk_label_new (para[i]);
                        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
                        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                        gtk_label_set_max_width_chars (GTK_LABEL (label), 55);
                        gtk_label_set_width_chars (GTK_LABEL (label), 55);
                        gtk_container_add (GTK_CONTAINER (box), label);
                }
        }

        return box;
}

void
gr_window_show_news (GrWindow *window)
{
        g_autoptr(GPtrArray) news = NULL;
        g_autoptr(GtkBuilder) builder = NULL;
        GtkWindow *dialog;
        GtkWidget *box;
        int i;

        if (window->news_dialog)
                return;

        news = get_release_info (PACKAGE_VERSION, "1.0.0");
        if (news->len == 0)
                return;

        builder = gtk_builder_new_from_resource ("/org/gnome/Recipes/recipe-whats-new-dialog.ui");
        dialog = GTK_WINDOW (gtk_builder_get_object (builder, "dialog"));
        gtk_window_set_transient_for (dialog, GTK_WINDOW (window));

        window->news_dialog = (GtkWidget *)dialog;

        gtk_widget_realize (GTK_WIDGET (dialog));
        gdk_window_set_functions (gtk_widget_get_window (GTK_WIDGET (dialog)),
                                  GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_MAXIMIZE);

        box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));

        for (i = 0; i < news->len; i++) {
                ReleaseInfo *ri = g_ptr_array_index (news, i);
                GtkWidget *info;

                info = gr_release_info_new (ri->version, ri->date, ri->news->str);
                gtk_container_add (GTK_CONTAINER (box), info);
        }

        gtk_widget_show_all (box);
        g_signal_connect (dialog, "delete-event", G_CALLBACK (news_dialog_closed), window);
        gr_window_present_dialog (GR_WINDOW (window), dialog);
}

void
gr_window_show_report_issue (GrWindow *window)
{
        const char *uri = "https://gitlab.gnome.org/GNOME/recipes/issues/";

        if (in_flatpak_sandbox () &&
            !portal_available (GTK_WINDOW (window), "org.freedesktop.portal.OpenURI"))
                return;

        gtk_show_uri_on_window (GTK_WINDOW (window), uri, GDK_CURRENT_TIME, NULL);
}

/* Show the surprise dialog between the first day of GUADEC and
 * the end of the year, but only once.
 */
static gboolean
should_show_surprise (void)
{
        g_autoptr(GDateTime) now = NULL;
        g_autoptr(GDateTime) start = NULL;
        g_autoptr(GDateTime) end = NULL;

        if (g_getenv ("SHOW_SURPRISE"))
                return TRUE;

        if (!g_settings_get_boolean (gr_settings_get (), "show-surprise-dialog"))
                return FALSE;

        now = g_date_time_new_now_utc ();
        start = g_date_time_new_utc (2017, 7, 27, 0, 0, 0);
        end = g_date_time_new_utc (2017, 12, 31, 0, 0, 0);

        if (g_date_time_compare (start, now) < 0 && g_date_time_compare (now, end) < 0)
                return TRUE;

        return FALSE;
}

void
surprise_dialog_closed (GtkWidget *window)
{
        g_settings_set_boolean (gr_settings_get (), "show-surprise-dialog", FALSE);
        gtk_widget_destroy (window);
}

void
gr_window_show_surprise (GrWindow *window)
{
        g_autoptr(GtkBuilder) builder = NULL;
        GtkWindow *dialog;
        GtkWidget *button;
        GtkWidget *label;

        if (!should_show_surprise ())
                return;

        builder = gtk_builder_new_from_resource ("/org/gnome/Recipes/recipe-surprise.ui");
        dialog = GTK_WINDOW (gtk_builder_get_object (builder, "dialog"));
        label = GTK_WIDGET (gtk_builder_get_object (builder, "label"));
        button = GTK_WIDGET (gtk_builder_get_object (builder, "close_button"));
        g_signal_connect_swapped (button, "clicked", G_CALLBACK (surprise_dialog_closed), dialog);
        gtk_window_set_transient_for (dialog, GTK_WINDOW (window));
        gr_window_present_dialog (window, dialog);
        pango_layout_set_spacing (gtk_label_get_layout (GTK_LABEL (label)), - pango_units_from_double (10.0));
}
