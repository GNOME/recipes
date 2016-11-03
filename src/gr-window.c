/* gr-window.c
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
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
#include "gr-window.h"
#include "gr-details-page.h"
#include "gr-edit-page.h"
#include "gr-list-page.h"
#include "gr-cuisine-page.h"
#include "gr-search-page.h"
#include "gr-ingredients-page.h"

struct _GrWindow
{
        GtkApplicationWindow parent_instance;

        GtkWidget *search_button;
        GtkWidget *search_bar;
        GtkWidget *search_entry;
        GtkWidget *header_stack;
        GtkWidget *main_stack;
        GtkWidget *details_header;
        GtkWidget *details_page;
        GtkWidget *edit_header;
        GtkWidget *edit_page;
        GtkWidget *list_header;
        GtkWidget *list_page;
        GtkWidget *search_header;
        GtkWidget *search_button2;
        GtkWidget *search_page;
        GtkWidget *cuisines_page;
        GtkWidget *cuisine_header;
        GtkWidget *cuisine_page;
	GtkWidget *ingredients_page;

        GQueue *back_entry_stack;
};

G_DEFINE_TYPE (GrWindow, gr_window, GTK_TYPE_APPLICATION_WINDOW)

typedef struct {
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
maybe_stop_search (GtkButton *button, GParamSpec *pspec, GrWindow *window)
{
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
                stop_search (window);
}

static void
switch_to_search (GrWindow *window)
{
    	gtk_header_bar_set_title (GTK_HEADER_BAR (window->search_header), _("Search"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->search_button2), TRUE);
     	gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "search");
     	gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "search");
}

static void
search_changed (GrWindow *window)
{
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

        if (strcmp (visible, "search") != 0) {
		save_back_entry (window);
		switch_to_search (window);
        }

        gr_search_page_update_search (GR_SEARCH_PAGE (window->search_page),
                                      gtk_entry_get_text (GTK_ENTRY (window->search_entry)));
}

static void
search_mode_enabled (GrWindow *window, GParamSpec *pspec)
{
        const char *visible;
	g_autofree char *terms = NULL;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (window->main_stack));

	if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (window->search_bar)))
		return;

        if (strcmp (visible, "ingredients") != 0)
		return;

	save_back_entry (window);
	switch_to_search (window);

	terms = gr_ingredients_page_get_search_terms (GR_INGREDIENTS_PAGE (window->ingredients_page));
	gtk_entry_set_text (GTK_ENTRY (window->search_entry), terms);
}

static gboolean
window_keypress_handler (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GrWindow *window = GR_WINDOW (widget);
        GtkWidget *w;

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

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_button);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_bar);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_entry);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, header_stack);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, main_stack);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, details_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, details_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, edit_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, edit_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, list_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, list_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_button2);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, search_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, cuisines_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, cuisine_header);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, cuisine_page);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrWindow, ingredients_page);

        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), new_recipe);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), go_back);
        gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), add_recipe);
}

static void
gr_window_init (GrWindow *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

	self->back_entry_stack = g_queue_new ();

        g_object_bind_property (self->search_button, "active",
                                self->search_bar, "search-mode-enabled",
                                G_BINDING_BIDIRECTIONAL);

        g_signal_connect_swapped (self->search_bar, "notify::search-mode-enabled", G_CALLBACK (search_mode_enabled), self);
        g_signal_connect_swapped (self->search_entry, "search-changed", G_CALLBACK (search_changed), self);
        g_signal_connect_swapped (self->search_entry, "stop-search", G_CALLBACK (stop_search), self);
        g_signal_connect (self->search_button2, "notify::active", G_CALLBACK (maybe_stop_search), self);
	g_signal_connect_after (self, "key-press-event", G_CALLBACK (window_keypress_handler), NULL);
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
        g_autofree char *name = NULL;

	save_back_entry (window);

        gr_details_page_set_recipe (GR_DETAILS_PAGE (window->details_page), recipe);

	gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (window->search_bar), FALSE);

        g_object_get (recipe, "name", &name, NULL);
        gtk_header_bar_set_title (GTK_HEADER_BAR (window->details_header), name);

        gtk_stack_set_visible_child_name (GTK_STACK (window->header_stack), "details");
        gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "details");
}

void
gr_window_edit_recipe (GrWindow *window,
                       GrRecipe *recipe)
{
        g_autofree char *name = NULL;

	save_back_entry (window);

        gr_edit_page_edit (GR_EDIT_PAGE (window->edit_page), recipe);

        g_object_get (recipe, "name", &name, NULL);
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
                     GrChef  *chef)
{
	g_autofree char *title = NULL;
	g_autofree char *name = NULL;

	save_back_entry (window);

	g_object_get (chef, "name", &name, NULL);
	title = g_strdup_printf (_("Recipes by %s"), name);
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

