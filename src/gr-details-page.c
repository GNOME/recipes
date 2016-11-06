/* gr-details-page.c:
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

#include "gr-details-page.h"
#include "gr-recipe-store.h"
#include "gr-app.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-ingredients-list.h"


struct _GrDetailsPage
{
        GtkBox parent_instance;

        GrRecipe *recipe;
        GrChef *chef;
        GrIngredientsList *ingredients;
	gboolean cooking;

        GtkWidget *recipe_image;
        GtkWidget *prep_time_label;
        GtkWidget *cook_time_label;
        GtkWidget *serves_spin;
        GtkWidget *description_label;
        GtkWidget *chef_label;
        GtkWidget *chef_link;
        GtkWidget *ingredients_label;
        GtkWidget *instructions_label;
        GtkWidget *notes_label;
        GtkWidget *cooking_revealer;
        GtkWidget *ingredients_check;
        GtkWidget *preheat_check;
        GtkWidget *instructions_check;
        GtkWidget *timer_button;
        GtkWidget *timer;
        GtkWidget *timer_stack;
        GtkWidget *timer_popover;
};

G_DEFINE_TYPE (GrDetailsPage, gr_details_page, GTK_TYPE_BOX)

static void connect_store_signals (GrDetailsPage *page);

static void
set_cooking (GrDetailsPage *page,
             gboolean       cooking)
{
	if (cooking == page->cooking)
		return;

	page->cooking = cooking;
	gtk_revealer_set_reveal_child (GTK_REVEALER (page->cooking_revealer), cooking);
}

static void
start_or_stop_cooking (GtkButton     *button,
                       GrDetailsPage *page)
{
	if (page->cooking) {
		gtk_revealer_set_reveal_child (GTK_REVEALER (page->cooking_revealer), FALSE);
		gtk_button_set_label (button, _("Start cooking"));
	}
	else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->ingredients_check), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->preheat_check), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->instructions_check), FALSE);
		gtk_revealer_set_reveal_child (GTK_REVEALER (page->cooking_revealer), TRUE);
		gtk_button_set_label (button, _("Stop cooking"));
	}

	page->cooking = !page->cooking;
}

static void
delete_recipe (GrDetailsPage *page)
{
        GrRecipeStore *store;
        GtkWidget *window;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        gr_recipe_store_remove (store, page->recipe);
        g_set_object (&page->recipe, NULL);
        g_set_object (&page->chef, NULL);

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_go_back (GR_WINDOW (window));
}

static void
edit_recipe (GrDetailsPage *page)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_edit_recipe (GR_WINDOW (window), page->recipe);
}

static gboolean
more_recipes (GrDetailsPage *page)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_show_chef (GR_WINDOW (window), page->chef);

        return TRUE;
}

static void
serves_value_changed (GrDetailsPage *page)
{
        int serves;
        int new_value;
        g_autofree char *text = NULL;

        new_value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page->serves_spin));
        g_object_get (page->recipe, "serves", &serves, NULL);

        text = gr_ingredients_list_scale (page->ingredients, new_value, serves);

        gtk_label_set_label (GTK_LABEL (page->ingredients_label), text);
}

static void
start_timer (GrDetailsPage *page)
{
	gboolean active;

	g_object_get (page->timer, "active", &active, NULL);
	if (active) {
		g_object_set (page->timer, "active", FALSE, NULL);
		gtk_stack_set_visible_child_name (GTK_STACK (page->timer_stack), "icon");
		gtk_button_set_label (GTK_BUTTON (page->timer_button), _("Start"));
	}
	else {
		g_object_set (page->timer, "active", TRUE, NULL);
		gtk_stack_set_visible_child_name (GTK_STACK (page->timer_stack), "timer");
		gtk_button_set_label (GTK_BUTTON (page->timer_button), _("Stop"));
	}
	gtk_popover_popdown (GTK_POPOVER (page->timer_popover));
}

static void
timer_complete (GrDetailsPage *page)
{
	GApplication *app;
	GNotification *notification;

	gtk_stack_set_visible_child_name (GTK_STACK (page->timer_stack), "icon");
	gtk_button_set_label (GTK_BUTTON (page->timer_button), _("Start"));

	app = g_application_get_default ();

g_print ("sending notification\n");
	notification = g_notification_new (_("Time is up!"));
	g_notification_set_body (notification, _("Your cooking timer has expired."));

	g_application_send_notification (app, "timer", notification);
}

static void
details_page_finalize (GObject *object)
{
        GrDetailsPage *self = GR_DETAILS_PAGE (object);

        g_clear_object (&self->recipe);
        g_clear_object (&self->chef);
        g_clear_object (&self->ingredients);

        G_OBJECT_CLASS (gr_details_page_parent_class)->finalize (object);
}

static void
gr_details_page_init (GrDetailsPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
        connect_store_signals (page);
}

static void
gr_details_page_class_init (GrDetailsPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = details_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-details-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, recipe_image);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, prep_time_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, cook_time_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, serves_spin);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, description_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, chef_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, chef_link);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, ingredients_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, instructions_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, notes_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, cooking_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, ingredients_check);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, instructions_check);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, preheat_check);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, timer_button);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, timer);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, timer_stack);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, timer_popover);

        gtk_widget_class_bind_template_callback (widget_class, edit_recipe);
        gtk_widget_class_bind_template_callback (widget_class, delete_recipe);
        gtk_widget_class_bind_template_callback (widget_class, more_recipes);
        gtk_widget_class_bind_template_callback (widget_class, start_or_stop_cooking);
        gtk_widget_class_bind_template_callback (widget_class, serves_value_changed);
        gtk_widget_class_bind_template_callback (widget_class, start_timer);
        gtk_widget_class_bind_template_callback (widget_class, timer_complete);
}

GtkWidget *
gr_details_page_new (void)
{
        GrDetailsPage *page;

        page = g_object_new (GR_TYPE_DETAILS_PAGE, NULL);

        return GTK_WIDGET (page);
}


void
gr_details_page_set_recipe (GrDetailsPage *page,
                            GrRecipe      *recipe)
{
        g_autofree char *image_path = NULL;
        g_autofree char *prep_time = NULL;
        g_autofree char *cook_time = NULL;
        int serves;
        g_autofree char *description = NULL;
        g_autofree char *author = NULL;
        g_autofree char *ingredients = NULL;
        g_autofree char *instructions = NULL;
        g_autofree char *notes = NULL;
        char *tmp;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;
        g_autofree char *author_desc = NULL;
        g_autoptr(GrIngredientsList) ing = NULL;

        g_set_object (&page->recipe, recipe);

        g_object_get (recipe,
                      "image-path", &image_path,
                      "prep-time", &prep_time,
                      "cook-time", &cook_time,
                      "serves", &serves,
                      "description", &description,
                      "author", &author,
                      "ingredients", &ingredients,
                      "instructions", &instructions,
                      "notes", &notes,
                      NULL);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        chef = gr_recipe_store_get_chef (store, author);
        g_set_object (&page->chef, chef);

        ing = gr_ingredients_list_new (ingredients);
        g_set_object (&page->ingredients, ing);

        if (image_path) {
                pixbuf = load_pixbuf_at_size (image_path, 256, 256);
                gtk_image_set_from_pixbuf (GTK_IMAGE (page->recipe_image), pixbuf);
        }
        gtk_label_set_label (GTK_LABEL (page->prep_time_label), prep_time);
        gtk_label_set_label (GTK_LABEL (page->cook_time_label), cook_time);
        gtk_label_set_label (GTK_LABEL (page->ingredients_label), ingredients);
        gtk_label_set_label (GTK_LABEL (page->instructions_label), instructions);
        gtk_label_set_label (GTK_LABEL (page->notes_label), notes);

        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->serves_spin), serves);
        gtk_widget_set_sensitive (page->serves_spin, ing != NULL);

        gtk_label_set_label (GTK_LABEL (page->description_label), description);

        if (page->chef)
                g_object_get (page->chef, "description", &author_desc, NULL);

        if (author_desc)
                tmp = g_strdup_printf (_("About GNOME chef %s:\n%s"), author, author_desc);
        else
                tmp = g_strdup_printf (_("A recipe by GNOME chef %s"), author);
        gtk_label_set_label (GTK_LABEL (page->chef_label), tmp);
        g_free (tmp);

        tmp = g_strdup_printf (_("More recipes by %s"), author);
        gtk_button_set_label (GTK_BUTTON (page->chef_link), tmp);
        g_free (tmp);
}

static void
details_page_reload (GrDetailsPage *page, GrRecipe *recipe)
{
        g_autofree char *name;
        g_autofree char *new_name;

        g_object_get (page->recipe, "name", &name, NULL);
        g_object_get (recipe, "name", &new_name, NULL);
        if (strcmp (name, new_name) == 0)
                gr_details_page_set_recipe (page, recipe);
}

static void
connect_store_signals (GrDetailsPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (details_page_reload), page);
}

gboolean
gr_details_page_is_cooking (GrDetailsPage *page)
{
	return page->cooking;
}

void
gr_details_page_set_cooking (GrDetailsPage *page,
			     gboolean       cooking)
{
	set_cooking (page, cooking);
}

