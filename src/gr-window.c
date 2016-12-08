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
#include "gr-cuisines-page.h"
#include "gr-ingredients-page.h"
#include "gr-query-editor.h"
#include "gr-recipe-importer.h"


struct _GrWindow
{
        GtkApplicationWindow parent_instance;

        GtkWidget *header;
        GtkWidget *header_start_stack;
        GtkWidget *header_title_stack;
        GtkWidget *header_end_stack;
        GtkWidget *search_button;
        GtkWidget *cooking_button;
        GtkWidget *search_bar;
        GtkWidget *main_stack;
        GtkWidget *recipes_page;
        GtkWidget *details_page;
        GtkWidget *edit_page;
        GtkWidget *list_page;
        GtkWidget *search_page;
        GtkWidget *cuisines_page;
        GtkWidget *cuisine_page;
        GtkWidget *ingredients_page;

        GrRecipeImporter *importer;

        GQueue *back_entry_stack;
};

G_DEFINE_TYPE (GrWindow, gr_window, GTK_TYPE_APPLICATION_WINDOW)

typedef struct
{
        gchar *page;
        gchar *header_start_child;
        gchar *header_title_child;
        gchar *header_end_child;
        gchar *header_title;
        gchar *search;
} BackEntry;

static void
back_entry_free (BackEntry *entry)
{
        g_free (entry->page);
        g_free (entry->header_start_child);
        g_free (entry->header_title_child);
        g_free (entry->header_end_child);
        g_free (entry->header_title);
        g_free (entry->search);
        g_free (entry);
}

static void
save_back_entry (GrWindow *window)
{
        BackEntry *entry;

        entry = g_new (BackEntry, 1);
        entry->page = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack)));
        entry->header_start_child = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->header_start_stack)));
        entry->header_title_child = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->header_title_stack)));
        entry->header_end_child = g_strdup (gtk_stack_get_visible_child_name (GTK_STACK (window->header_end_stack)));
        entry->header_title = g_strdup (gtk_header_bar_get_title (GTK_HEADER_BAR (window->header)));

        if (strcmp (entry->page, "search") == 0)
                entry->search = g_strdup (gr_query_editor_get_query (GR_QUERY_EDITOR (window->search_bar)));
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

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), entry->header_title);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), entry->header_start_child);
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), entry->header_title_child);
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), entry->header_end_child);

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), entry->page);

        if (strcmp (entry->page, "search") == 0) {
                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), TRUE);
                gr_query_editor_set_query (GR_QUERY_EDITOR (window->search_bar), entry->search);
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

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), _("Add a new recipe"));

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "edit");

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

#if 0
static void
maybe_stop_search (GtkButton  *button,
                   GParamSpec *pspec,
                   GrWindow   *window)
{
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
                stop_search (window);
}
#endif

static void
switch_to_search (GrWindow *window)
{
        save_back_entry (window);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), _("Search"));

     	gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
     	gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
     	gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "search");

     	gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "search");
}

void
gr_window_show_search (GrWindow   *window,
                       const char *search)
{
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->search_button), TRUE);
        switch_to_search (window);
        gr_query_editor_set_query (GR_QUERY_EDITOR (window->search_bar), search);
}

static void
visible_page_changed (GrWindow *window)
{
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "search") != 0) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->search_button), FALSE);
        }

        if (strcmp (visible, "recipes") == 0) {
                gr_recipes_page_unexpand (GR_RECIPES_PAGE (window->recipes_page));
        }

        if (strcmp (visible, "cuisines") == 0) {
                gr_cuisines_page_unexpand (GR_CUISINES_PAGE (window->cuisines_page));
        }
}

static void
search_changed (GrWindow *window)
{
        const char *visible;
        const char *terms;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "search") != 0)
                switch_to_search (window);

        terms = gr_query_editor_get_query (GR_QUERY_EDITOR (window->search_bar));
        gr_search_page_update_search (GR_SEARCH_PAGE (window->search_page), terms);
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
                        } else {
                                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);
                        }
                        return GDK_EVENT_PROPAGATE;
                }
        }

        return gtk_search_bar_handle_event (GTK_SEARCH_BAR (window->search_bar), event);
}

static void
window_mapped_handler (GtkWidget *widget)
{
        GrWindow *window = GR_WINDOW (widget);

        gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
        gr_recipes_page_unexpand (GR_RECIPES_PAGE (window->recipes_page));
        gr_cuisines_page_unexpand (GR_CUISINES_PAGE (window->cuisines_page));
}

static void
hide_or_show_header_end_stack (GObject    *object,
                               GParamSpec *pspec,
                               GrWindow   *window)
{
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->header_end_stack));
        gtk_widget_set_visible (window->header_end_stack, strcmp (visible, "list") != 0);
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

        g_clear_object (&self->importer);

        G_OBJECT_CLASS (gr_window_parent_class)->finalize (object);
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
        gtk_widget_class_bind_template_child (widget_class, GrWindow, search_button);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, cooking_button);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, search_bar);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, main_stack);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, recipes_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, details_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, edit_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, list_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, search_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, cuisines_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, cuisine_page);
        gtk_widget_class_bind_template_child (widget_class, GrWindow, ingredients_page);

        gtk_widget_class_bind_template_callback (widget_class, new_recipe);
        gtk_widget_class_bind_template_callback (widget_class, go_back);
        gtk_widget_class_bind_template_callback (widget_class, add_recipe);
        gtk_widget_class_bind_template_callback (widget_class, visible_page_changed);
        gtk_widget_class_bind_template_callback (widget_class, start_or_stop_cooking);
        gtk_widget_class_bind_template_callback (widget_class, hide_or_show_header_end_stack);
        gtk_widget_class_bind_template_callback (widget_class, search_changed);
        gtk_widget_class_bind_template_callback (widget_class, stop_search);
        gtk_widget_class_bind_template_callback (widget_class, window_keypress_handler);
        gtk_widget_class_bind_template_callback (widget_class, window_mapped_handler);
}

static void
gr_window_init (GrWindow *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
        self->back_entry_stack = g_queue_new ();
}

void
gr_window_go_back (GrWindow *window)
{
        if (g_queue_is_empty (window->back_entry_stack)) {
        	gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "main");
        	gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "main");
        	gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "search");

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
        save_back_entry (window);

        gr_details_page_set_recipe (GR_DETAILS_PAGE (window->details_page), recipe);

        update_cooking_button (window, gr_details_page_is_cooking (GR_DETAILS_PAGE (window->details_page)));

        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), gr_recipe_get_name (recipe));

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "details");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "details");
}

void
gr_window_edit_recipe (GrWindow *window,
                       GrRecipe *recipe)
{
        save_back_entry (window);

        gr_edit_page_edit (GR_EDIT_PAGE (window->edit_page), recipe);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), gr_recipe_get_name (recipe));

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "edit");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "edit");
}

static void
done_cb (GrRecipeImporter *importer,
         GrRecipe         *recipe,
         GrWindow         *window)
{
        if (recipe)
                gr_window_show_recipe (window, recipe);
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
file_chooser_response (GtkNativeDialog *self,
                       int              response_id,
                       GrWindow        *window)
{
        if (response_id == GTK_RESPONSE_ACCEPT) {
                g_autoptr(GFile) file = NULL;
                file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self));
                do_import (window, file);
        }
}

void
gr_window_load_recipe (GrWindow *window,
                       GFile    *file)
{
        GtkFileChooserNative *chooser;

        if (file) {
                do_import (window, file);
                return;
        }

        chooser = gtk_file_chooser_native_new (_("Select a recipe file"),
                                               GTK_WINDOW (window),
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               _("Open"),
                                               _("Cancel"));
        gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (chooser), TRUE);

        g_signal_connect (chooser, "response", G_CALLBACK (file_chooser_response), window);

        gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

void
gr_window_show_diet (GrWindow   *window,
                     const char *title,
                     GrDiets     diet)
{
        save_back_entry (window);

        gr_list_page_populate_from_diet (GR_LIST_PAGE (window->list_page), diet);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), title);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "list");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_chef (GrWindow *window,
                     GrChef   *chef)
{
        g_autofree char *title = NULL;

        save_back_entry (window);

        gr_list_page_populate_from_chef (GR_LIST_PAGE (window->list_page), chef);

        title = g_strdup_printf (_("Recipes by %s"), gr_chef_get_name (chef));
        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), title);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "list");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_favorites (GrWindow *window)
{
        save_back_entry (window);

        gr_list_page_populate_from_favorites (GR_LIST_PAGE (window->list_page));

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), _("Favorite recipes"));

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "list");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_myself (GrWindow *window)
{
        const char *name;
        g_autoptr(GrChef) chef = NULL;
        GrRecipeStore *store;

        save_back_entry (window);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        name = gr_recipe_store_get_user_key (store);
        chef = gr_recipe_store_get_chef (store, name);
        gr_list_page_populate_from_chef (GR_LIST_PAGE (window->list_page), chef);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header ), _("My own recipes"));

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "list");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_cuisine (GrWindow   *window,
                        const char *cuisine,
                        const char *title)
{
	save_back_entry (window);

        gr_cuisine_page_set_cuisine (GR_CUISINE_PAGE (window->cuisine_page), cuisine);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), title);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "list");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "cuisine");
}

void
gr_window_show_season (GrWindow   *window,
                       const char *season,
                       const char *title)
{
	save_back_entry (window);

        gr_list_page_populate_from_season (GR_LIST_PAGE (window->list_page), season);

        gtk_header_bar_set_title (GTK_HEADER_BAR (window->header), title);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_start_stack), "back");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_title_stack), "title");
        gtk_stack_set_visible_child_name (GTK_STACK (window->header_end_stack), "list");

        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "list");
}

void
gr_window_show_search_by_ingredients (GrWindow   *window,
                                      const char *ingredient)
{
        g_autofree char *term = NULL;

        switch_to_search (window);
        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), TRUE);
        term = g_strconcat ("i+:", ingredient, NULL);
        gr_query_editor_set_query (GR_QUERY_EDITOR (window->search_bar), term);
}
