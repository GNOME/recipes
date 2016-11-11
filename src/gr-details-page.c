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

#include <stdlib.h>
#include <math.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-details-page.h"
#include "gr-recipe-store.h"
#include "gr-app.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-ingredients-list.h"
#include "gr-timer.h"


typedef struct {
        gboolean ingredients;
        gboolean preheat;
        gboolean instructions;
        GrTimer *timer;
} CookingData;

static void
timer_complete (GrTimer *timer)
{
	GApplication *app;
	GNotification *notification;
        g_autofree char *body = NULL;

	app = g_application_get_default ();

        body = g_strdup_printf (_("Your cooking timer for “%s” has expired."),
                                gr_timer_get_name (timer));

	notification = g_notification_new (_("Time is up!"));
	g_notification_set_body (notification, body);

	g_application_send_notification (app, "timer", notification);
}

static CookingData *
cooking_data_new (const char *name)
{
        CookingData *cd;

        cd = g_new (CookingData, 1);
        cd->ingredients = FALSE;
        cd->preheat = FALSE;
        cd->instructions = FALSE;
        cd->timer = gr_timer_new (name);

        g_signal_connect (cd->timer, "complete", G_CALLBACK (timer_complete), NULL);

        return cd;
}

static void
cooking_data_free (gpointer data)
{
        CookingData *cd = data;

        g_object_unref (cd->timer);
        g_free (cd);
}

struct _GrDetailsPage
{
        GtkBox parent_instance;

        GrRecipe *recipe;
        GrChef *chef;
        GrIngredientsList *ingredients;
        GHashTable *cooking;

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
        GtkWidget *timer;
        GtkWidget *timer_stack;
        GtkWidget *timer_popover;
        GtkWidget *time_spin;
        GtkWidget *favorite_button;
        GtkWidget *duration_stack;
        GtkWidget *remaining_time_label;
};

G_DEFINE_TYPE (GrDetailsPage, gr_details_page, GTK_TYPE_BOX)

static void connect_store_signals (GrDetailsPage *page);

static void
timer_active_changed (GrTimer       *timer,
                      GParamSpec    *pspec,
                      GrDetailsPage *page)
{
        if (strcmp (gr_timer_get_name (timer), gr_recipe_get_name (page->recipe)) != 0)
                return;

        if (gr_timer_get_active (timer)) {
		gtk_stack_set_visible_child_name (GTK_STACK (page->timer_stack), "timer");
		gtk_stack_set_visible_child_name (GTK_STACK (page->duration_stack), "stop");
        }
        else {
        	gtk_stack_set_visible_child_name (GTK_STACK (page->timer_stack), "icon");
		gtk_stack_set_visible_child_name (GTK_STACK (page->duration_stack), "start");
        }
}

static void
timer_remaining_changed (GrTimer       *timer,
                         GParamSpec    *pspec,
                         GrDetailsPage *page)
{
        guint64 remaining;
        guint hours, minutes, seconds;
        g_autofree char *buf;

        g_print ("time remaining changed\n");
        if (strcmp (gr_timer_get_name (timer), gr_recipe_get_name (page->recipe)) != 0)
                return;

        remaining = gr_timer_get_remaining (timer);
        g_print ("%ld\n", remaining);

        seconds = remaining / (1000 * 1000);
        g_print ("seconds %d\n", seconds);

        hours = seconds / (60 * 60);
        seconds -= hours * 60 * 60;
        minutes = seconds / 60;
        seconds -= minutes * 60;

        g_print ("%d %d %d\n", hours, minutes, seconds);
        buf = g_strdup_printf ("%02d:%02d:%02d", hours, minutes, seconds);
        gtk_label_set_label (GTK_LABEL (page->remaining_time_label), buf);
}

static void
set_cooking (GrDetailsPage *page,
             gboolean       cooking)
{
        const char *name;
        CookingData *cd;

        name = gr_recipe_get_name (page->recipe);

        cd = g_hash_table_lookup (page->cooking, name);

	if (cooking) {
                if (!cd) {
                        cd = cooking_data_new (name);
                        g_hash_table_insert (page->cooking, g_strdup (name), cd);
                }

		g_object_set (page->ingredients_check, "active", cd->ingredients, NULL);
		g_object_set (page->preheat_check, "active", cd->preheat, NULL);
		g_object_set (page->instructions_check, "active", cd->instructions, NULL);
                g_object_set (page->timer, "timer", cd->timer, NULL);
                g_signal_connect (cd->timer, "notify::active", G_CALLBACK (timer_active_changed), page);
                timer_active_changed (cd->timer, NULL, page);
                g_signal_connect (cd->timer, "notify::remaining", G_CALLBACK (timer_remaining_changed), page);
                timer_remaining_changed (cd->timer, NULL, page);
		gtk_revealer_set_reveal_child (GTK_REVEALER (page->cooking_revealer), TRUE);
	}
	else {
                if (cd) {
                        g_signal_handlers_disconnect_by_func (cd->timer, G_CALLBACK (timer_active_changed), page);
                        g_signal_handlers_disconnect_by_func (cd->timer, G_CALLBACK (timer_remaining_changed), page);
                        g_hash_table_remove (page->cooking, name);
                }

 		g_object_set (page->timer, "timer", NULL, NULL);
		gtk_revealer_set_reveal_child (GTK_REVEALER (page->cooking_revealer), FALSE);
	}
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
        serves = gr_recipe_get_serves (page->recipe);

        text = gr_ingredients_list_scale (page->ingredients, new_value, serves);

        gtk_label_set_label (GTK_LABEL (page->ingredients_label), text);
}

static void
start_or_stop_timer (GrDetailsPage *page)
{
        int minutes;
        const char *name;
        CookingData *cd;

        name = gr_recipe_get_name (page->recipe);

        cd = g_hash_table_lookup (page->cooking, name);

        g_assert (cd && cd->timer);

	if (gr_timer_get_active (cd->timer)) {
                g_object_set (cd->timer, "active", FALSE, NULL);
	}
	else {

                minutes = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page->time_spin));
		g_object_set (cd->timer,
                              "duration", minutes * 60 * 1000 * 1000,
                              "active", TRUE,
                              NULL);
	}

	gtk_popover_popdown (GTK_POPOVER (page->timer_popover));
}

static int
time_spin_input (GtkSpinButton *spin_button,
                 double        *new_val)
{
  	const char *text;
  	char **str;
  	gboolean found = FALSE;
  	int hours;
  	int minutes;
  	char *endh;
  	char *endm;

  	text = gtk_entry_get_text (GTK_ENTRY (spin_button));
  	str = g_strsplit (text, ":", 2);

  	if (g_strv_length (str) == 2) {
      		hours = strtol (str[0], &endh, 10);
      		minutes = strtol (str[1], &endm, 10);
      		if (!*endh && !*endm &&
                    0 <= hours && hours < 24 &&
                    0 <= minutes && minutes < 60) {
                        *new_val = hours * 60 + minutes;
                        found = TRUE;
                }
        }
        else {
                minutes = strtol (str[0], &endm, 10);
                if (!*endm) {
                        *new_val = minutes;
                        found = TRUE;
                }
        }

        g_strfreev (str);

        if (!found) {
                *new_val = 0.0;
                return GTK_INPUT_ERROR;
        }

        g_print ("new value %f\n", *new_val);

        return TRUE;
}

static int
time_spin_output (GtkSpinButton *spin_button)
{
        GtkAdjustment *adjustment;
        char *buf;
        double hours;
        double minutes;

        adjustment = gtk_spin_button_get_adjustment (spin_button);
        hours = gtk_adjustment_get_value (adjustment) / 60.0;
        minutes = (hours - floor (hours)) * 60.0;
        buf = g_strdup_printf ("%02.0f:%02.0f", floor (hours), floor (minutes + 0.5));
        if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
                gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
        g_free (buf);

        return TRUE;
}

static void
check_clicked (GtkWidget     *button,
               GrDetailsPage *page)
{
        CookingData *cd;
        const char *name;
        gboolean active;

        name = gr_recipe_get_name (page->recipe);
        cd = g_hash_table_lookup (page->cooking, name);

        g_assert (cd);

        g_object_get (button, "active", &active, NULL);

        if (button == page->ingredients_check)
                cd->ingredients = active;
        else if (button == page->preheat_check)
                cd->preheat = active;
        else if (button == page->instructions_check)
                cd->instructions = active;
}

static void
cook_it_later (GrDetailsPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->favorite_button)))
                gr_recipe_store_add_favorite (store, page->recipe);
        else
                gr_recipe_store_remove_favorite (store, page->recipe);
}

static void
details_page_finalize (GObject *object)
{
        GrDetailsPage *self = GR_DETAILS_PAGE (object);

        g_clear_object (&self->recipe);
        g_clear_object (&self->chef);
        g_clear_object (&self->ingredients);
        g_clear_pointer (&self->cooking, g_hash_table_unref);

        G_OBJECT_CLASS (gr_details_page_parent_class)->finalize (object);
}

static void
gr_details_page_init (GrDetailsPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
        connect_store_signals (page);
        page->cooking = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, cooking_data_free);
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
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, timer);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, timer_stack);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, timer_popover);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, time_spin);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, favorite_button);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, duration_stack);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, remaining_time_label);

        gtk_widget_class_bind_template_callback (widget_class, edit_recipe);
        gtk_widget_class_bind_template_callback (widget_class, delete_recipe);
        gtk_widget_class_bind_template_callback (widget_class, more_recipes);
        gtk_widget_class_bind_template_callback (widget_class, serves_value_changed);
        gtk_widget_class_bind_template_callback (widget_class, start_or_stop_timer);
        gtk_widget_class_bind_template_callback (widget_class, time_spin_input);
        gtk_widget_class_bind_template_callback (widget_class, time_spin_output);
        gtk_widget_class_bind_template_callback (widget_class, check_clicked);
        gtk_widget_class_bind_template_callback (widget_class, cook_it_later);
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
        const char *name;
        const char *author;
        const char *description;
        const char *prep_time;
        const char *cook_time;
        int serves;
        const char *ingredients;
        const char *instructions;
        const char *notes;
        char *tmp;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GrIngredientsList) ing = NULL;
        g_autoptr(GArray) images = NULL;
        gboolean cooking;
        gboolean favorite;
        int left, width;

        g_set_object (&page->recipe, recipe);

        name = gr_recipe_get_name (recipe);
        author = gr_recipe_get_author (recipe);
        description = gr_recipe_get_description (recipe);
        serves = gr_recipe_get_serves (recipe);
        prep_time = gr_recipe_get_prep_time (recipe);
        cook_time = gr_recipe_get_cook_time (recipe);
        ingredients = gr_recipe_get_ingredients (recipe);
        instructions = gr_recipe_get_instructions (recipe);
        notes = gr_recipe_get_notes (recipe);

        g_object_get (recipe, "images", &images, NULL);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        chef = gr_recipe_store_get_chef (store, author);
        g_set_object (&page->chef, chef);

        ing = gr_ingredients_list_new (ingredients);
        g_set_object (&page->ingredients, ing);

        if (images->len >= 2) {
                width = 2;
                left = -1;
        }
        else {
                width = 1;
                left = 0;
        }
        gtk_container_child_set (GTK_CONTAINER (gtk_widget_get_parent (page->recipe_image)),
                                 page->recipe_image,
                                 "width", width,
                                 "left-attach", left,
                                 NULL);

        g_object_set (page->recipe_image, "images", images, NULL);
        gtk_label_set_label (GTK_LABEL (page->prep_time_label), prep_time);
        gtk_label_set_label (GTK_LABEL (page->cook_time_label), cook_time);
        gtk_label_set_label (GTK_LABEL (page->ingredients_label), ingredients);
        gtk_label_set_label (GTK_LABEL (page->instructions_label), instructions);
        gtk_label_set_label (GTK_LABEL (page->notes_label), notes);

        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->serves_spin), serves);
        gtk_widget_set_sensitive (page->serves_spin, ing != NULL);

        gtk_label_set_label (GTK_LABEL (page->description_label), description);

        if (page->chef)
                tmp = g_strdup_printf (_("About GNOME chef %s:\n%s"),
                                       author,
                                       gr_chef_get_description (page->chef));
        else
                tmp = g_strdup_printf (_("A recipe by GNOME chef %s"), author);
        gtk_label_set_label (GTK_LABEL (page->chef_label), tmp);
        g_free (tmp);

        tmp = g_strdup_printf (_("More recipes by %s"), author);
        gtk_button_set_label (GTK_BUTTON (page->chef_link), tmp);
        g_free (tmp);

        cooking = g_hash_table_lookup (page->cooking, name) != NULL;
        set_cooking (page, cooking);

        favorite = gr_recipe_store_is_favorite (store, recipe);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->favorite_button), favorite);
}

static void
details_page_reload (GrDetailsPage *page,
                     GrRecipe      *recipe)
{
        const char *name;
        const char *new_name;

        name = gr_recipe_get_name (page->recipe);
        new_name = gr_recipe_get_name (recipe);
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
	return gtk_revealer_get_reveal_child (GTK_REVEALER (page->cooking_revealer));
}

void
gr_details_page_set_cooking (GrDetailsPage *page,
			     gboolean       cooking)
{
	set_cooking (page, cooking);
}

