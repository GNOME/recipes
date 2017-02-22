/* gr-cooking-page.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3.
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

#include <stdlib.h>

#include "gr-cooking-page.h"
#include "gr-cooking-view.h"
#include "gr-utils.h"
#include "gr-window.h"
#include "gr-recipe.h"


struct _GrCookingPage
{
        GtkBox parent_instance;

        GtkWidget *cooking_overlay;
        GtkWidget *overlay_revealer;
        GtkWidget *event_box;
        GtkWidget *next_revealer;
        GtkWidget *prev_revealer;
        GtkWidget *close_revealer;
        GtkWidget *cooking_view;
        GtkWidget *prev_step_button;
        GtkWidget *next_step_button;
        GtkWidget *done_button;
        GtkWidget *notification_revealer;
        GtkWidget *notification_label;

        GrRecipe *recipe;

        guint inhibit_cookie;
        guint keyval_seen;
        guint button_seen;

        guint hide_timeout;
};


G_DEFINE_TYPE (GrCookingPage, gr_cooking_page, GTK_TYPE_BOX)


GrCookingPage *
gr_cooking_page_new (void)
{
        return g_object_new (GR_TYPE_COOKING_PAGE, NULL);
}

static void
gr_cooking_page_finalize (GObject *object)
{
        GrCookingPage *self = GR_COOKING_PAGE (object);

        g_clear_object (&self->recipe);
        if (self->hide_timeout)
                g_source_remove (self->hide_timeout);

        G_OBJECT_CLASS (gr_cooking_page_parent_class)->finalize (object);
}

static void
gr_cooking_page_init (GrCookingPage *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        gtk_widget_add_events (GTK_WIDGET (self->event_box), GDK_POINTER_MOTION_MASK);
        gtk_widget_add_events (GTK_WIDGET (self->event_box), GDK_BUTTON_PRESS_MASK);
}

static int
get_cooking_overlay_count (void)
{
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        int count;

        keyfile = g_key_file_new ();

        path = g_build_filename (get_user_data_dir (), "cooking", NULL);
        if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error)) {
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                        g_error ("Failed to load cooking overlay count: %s", error->message);
                return 0;
        }

        count = g_key_file_get_integer (keyfile, "Cooking", "OverlayShown", &error);
        if (error) {
                g_error ("Failed to load cooking overlay count: %s", error->message);
                return 0;
        }

        return count;
}

static void
set_cooking_overlay_count (int count)
{
        g_autofree char *path = NULL;
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;

        keyfile = g_key_file_new ();

        path = g_build_filename (get_user_data_dir (), "cooking", NULL);

        g_key_file_set_integer (keyfile, "Cooking", "OverlayShown", count);

        if (!g_key_file_save_to_file (keyfile, path, &error)) {
                g_error ("Failed to save cooking overlay count: %s", error->message);
        }
}

static void
update_steppers (GrCookingPage *page)
{
        int step;
        int n_steps;

        step = gr_cooking_view_get_step (GR_COOKING_VIEW (page->cooking_view));
        n_steps = gr_cooking_view_get_n_steps (GR_COOKING_VIEW (page->cooking_view));

        gtk_widget_set_sensitive (page->prev_step_button, step - 1 >= 0);
        gtk_widget_set_sensitive (page->next_step_button, step + 1 <= n_steps - 1);
        gtk_widget_set_visible (page->done_button, step == n_steps - 1);
}

static void
hide_overlay (gpointer data,
              gboolean fade)
{
        GrCookingPage *page = data;

        if (!fade)
                gtk_revealer_set_transition_type (GTK_REVEALER (page->overlay_revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->overlay_revealer), FALSE);
        if (!fade)
                gtk_revealer_set_transition_type (GTK_REVEALER (page->overlay_revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
}

static gboolean
show_overlay (gpointer data)
{
        GrCookingPage *page = data;

        gtk_revealer_set_reveal_child (GTK_REVEALER (page->overlay_revealer), TRUE);

        return G_SOURCE_REMOVE;
}

static void
set_cooking (GrCookingPage *page,
             gboolean       cooking)
{
        int count;
        GtkWidget *window;
        GtkApplication *app;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        app = gtk_window_get_application (GTK_WINDOW (window));

        if (cooking) {
                hide_overlay (page, FALSE);
                count = get_cooking_overlay_count ();
                set_cooking_overlay_count (count + 1);
                if (count < 3)
                        g_timeout_add (1000, show_overlay, page);

                if (page->inhibit_cookie == 0) {
                        page->inhibit_cookie = gtk_application_inhibit (app,
                                                                        GTK_WINDOW (window),
                                                                        GTK_APPLICATION_INHIBIT_SUSPEND |
                                                                        GTK_APPLICATION_INHIBIT_IDLE,
                                                                        _("Cooking"));
                }

                gr_cooking_view_set_step (GR_COOKING_VIEW (page->cooking_view), 0);
                update_steppers (page);

                gr_window_set_fullscreen (GR_WINDOW (window), TRUE);
        }
        else {
                if (page->inhibit_cookie != 0) {
                        gtk_application_uninhibit (app, page->inhibit_cookie);
                        page->inhibit_cookie = 0;
                }

                gr_window_show_recipe (GR_WINDOW (window), page->recipe);
                update_steppers (page);

                gr_window_set_fullscreen (GR_WINDOW (window), FALSE);
        }
}

static void
stop_cooking (GrCookingPage *page)
{
        set_cooking (page, FALSE);
}

static void
prev_step (GrCookingPage *page)
{
        gr_cooking_view_prev_step (GR_COOKING_VIEW (page->cooking_view));
        update_steppers (page);
}

static void
next_step (GrCookingPage *page)
{
        gr_cooking_view_next_step (GR_COOKING_VIEW (page->cooking_view));
        update_steppers (page);
}


static gboolean
doubletap_timeout (gpointer data)
{
        GrCookingPage *page = data;

        if (page->keyval_seen) {
                page->keyval_seen = 0;
                next_step (page);
        }
        else if (page->button_seen) {
                page->button_seen = 0;
                next_step (page);
        }

        return G_SOURCE_REMOVE;
}

void
gr_cooking_page_start_cooking (GrCookingPage *page)
{
        set_cooking (page, TRUE);
}

static void
show_buttons (GrCookingPage *page)
{
        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (page->close_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (page->close_revealer), TRUE);

        if (gr_cooking_view_get_n_steps (GR_COOKING_VIEW (page->cooking_view)) < 2)
                return;

        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (page->next_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (page->next_revealer), TRUE);

        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (page->prev_revealer)))
                        gtk_revealer_set_reveal_child (GTK_REVEALER (page->prev_revealer), TRUE);
}

static void
hide_buttons (GrCookingPage *page)
{
        if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->close_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (page->close_revealer), FALSE);

        if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->next_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (page->next_revealer), FALSE);

        if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->prev_revealer)))
                        gtk_revealer_set_reveal_child (GTK_REVEALER (page->prev_revealer), FALSE);
}

static gboolean
hide_timeout (gpointer data)
{
        GrCookingPage *page = data;

        hide_buttons (page);

        page->hide_timeout = 0;

        return G_SOURCE_REMOVE;
}

static void
reset_hide_timeout (GrCookingPage *page)
{
        if (page->hide_timeout != 0) {
                g_source_remove (page->hide_timeout);
                page->hide_timeout = 0;
        }
        page->hide_timeout = g_timeout_add (5000, hide_timeout, page);
}

static gboolean
motion_notify (GtkWidget   *widget,
               GdkEvent    *event,
               GrCookingPage *page)
{
        show_buttons (page);
        reset_hide_timeout (page);

        return FALSE;
}

gboolean
gr_cooking_page_handle_event (GrCookingPage *page,
                              GdkEvent      *event)
{
        if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->overlay_revealer))) {
                hide_overlay (page, TRUE);

                return GDK_EVENT_STOP;
        }
        else if (event->type == GDK_KEY_PRESS) {
                GdkEventKey *e = (GdkEventKey *)event;
                if (e->keyval == GDK_KEY_Escape) {
                        stop_cooking (page);

                        return GDK_EVENT_STOP;
                }
                else if (e->keyval == GDK_KEY_F1) {
                        show_overlay (page);

                        return GDK_EVENT_STOP;
                }
                else if (e->keyval == GDK_KEY_Return) {
                        if (gtk_widget_get_visible (page->done_button))
                               stop_cooking (page);

                        return GDK_EVENT_STOP;
                }
                else if (e->keyval == GDK_KEY_space) {
                        next_step (page);

                        return GDK_EVENT_STOP;
                }
                else if (e->keyval == GDK_KEY_Left) {
                        prev_step (page);

                        return GDK_EVENT_STOP;
                }
                else if (e->keyval == GDK_KEY_Right) {
                        next_step (page);

                        return GDK_EVENT_STOP;
                }
                else if (e->keyval == page->keyval_seen) {
                        page->keyval_seen = 0;
                        prev_step (page);

                        return GDK_EVENT_STOP;
                }
                else {
                        GtkSettings *settings;
                        int time;

                        settings = gtk_widget_get_settings (GTK_WIDGET (page));
                        g_object_get (settings, "gtk-double-click-time", &time, NULL);
                        page->keyval_seen = e->keyval;
                        g_timeout_add (time, doubletap_timeout, page);

                        return GDK_EVENT_STOP;
                }
        }
        else if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton *e = (GdkEventButton *)event;

                if (e->button == GDK_BUTTON_SECONDARY) {
                        stop_cooking (page);

                        return GDK_EVENT_STOP;
                }
                else if (e->button == page->button_seen) {
                        page->button_seen = 0;
                        prev_step (page);

                        return GDK_EVENT_STOP;
                }
                else {
                        GtkSettings *settings;
                        int time;

                        settings = gtk_widget_get_settings (GTK_WIDGET (page));
                        g_object_get (settings, "gtk-double-click-time", &time, NULL);
                        page->button_seen = e->button;
                        g_timeout_add (time, doubletap_timeout, page);

                        return GDK_EVENT_STOP;
                }
        }

        return GDK_EVENT_PROPAGATE;
}

void
gr_cooking_page_show_notification (GrCookingPage *page,
                                   const char    *text)
{
        gtk_label_set_label (GTK_LABEL (page->notification_label), text);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->notification_revealer), TRUE);
}

static void
close_notification (GrCookingPage *page)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->notification_revealer), FALSE);
}

static void
gr_cooking_page_class_init (GrCookingPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = gr_cooking_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-cooking-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, cooking_overlay);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, overlay_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, cooking_view);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, prev_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, next_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, close_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, event_box);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, prev_step_button);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, next_step_button);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, done_button);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, notification_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, notification_label);

        gtk_widget_class_bind_template_callback (widget_class, prev_step);
        gtk_widget_class_bind_template_callback (widget_class, next_step);
        gtk_widget_class_bind_template_callback (widget_class, stop_cooking);
        gtk_widget_class_bind_template_callback (widget_class, motion_notify);
        gtk_widget_class_bind_template_callback (widget_class, close_notification);
}

void
gr_cooking_page_set_recipe (GrCookingPage *page,
                            GrRecipe      *recipe)
{
        g_autoptr(GArray) images = NULL;
        const char *instructions = NULL;

        g_set_object (&page->recipe, recipe);

        g_object_get (recipe, "images", &images, NULL);
        instructions = gr_recipe_get_translated_instructions (recipe);

        gr_cooking_view_set_images (GR_COOKING_VIEW (page->cooking_view), images, 0);
        gr_cooking_view_set_instructions (GR_COOKING_VIEW (page->cooking_view), instructions);
}
