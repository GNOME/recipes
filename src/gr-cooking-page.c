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
#include "gr-settings.h"
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
        GtkWidget *mini_timer_box;
        GtkWidget *confirm_revealer;
        int notification_step;

        GrRecipe *recipe;

        guint inhibit_cookie;
        guint keyval_seen;

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

        gr_cooking_view_set_timer_box (GR_COOKING_VIEW (self->cooking_view), self->mini_timer_box);
}

static guint
get_cooking_overlay_count (void)
{
        GSettings *settings = gr_settings_get ();
        return g_settings_get_uint (settings, "cooking");
}

static void
set_cooking_overlay_count (uint count)
{
        GSettings *settings = gr_settings_get ();
        g_settings_set_uint (settings, "cooking", count);
}

static void
update_steppers (GrCookingPage *page)
{
        int step;
        int n_steps;
        gboolean active_timers;

        step = gr_cooking_view_get_step (GR_COOKING_VIEW (page->cooking_view));
        n_steps = gr_cooking_view_get_n_steps (GR_COOKING_VIEW (page->cooking_view));
        active_timers = gr_cooking_view_has_active_timers (GR_COOKING_VIEW (page->cooking_view));

        gtk_widget_set_sensitive (page->prev_step_button, step - 1 >= 0);
        gtk_widget_set_sensitive (page->next_step_button, step + 1 <= n_steps - 1);

        gtk_widget_set_visible (page->done_button, step == n_steps - 1 && !active_timers);
}

static void
on_child_revealed (GrCookingPage *page)
{
        if (!gtk_revealer_get_reveal_child (GTK_REVEALER (page->overlay_revealer)))
                    gtk_widget_hide (page->overlay_revealer);
}

static void
hide_overlay (GrCookingPage *page,
              gboolean       fade)
{
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

        gtk_widget_show (page->overlay_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->overlay_revealer), TRUE);

        return G_SOURCE_REMOVE;
}

static void
on_confirm_revealed (GrCookingPage *page)
{
        if (!gtk_revealer_get_reveal_child (GTK_REVEALER (page->confirm_revealer)))
                    gtk_widget_hide (page->confirm_revealer);
}

static void
hide_confirm (GrCookingPage *page,
              gboolean       fade)
{
        if (!fade)
                gtk_revealer_set_transition_type (GTK_REVEALER (page->confirm_revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->confirm_revealer), FALSE);
        if (!fade)
                gtk_revealer_set_transition_type (GTK_REVEALER (page->confirm_revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
}

static void
show_confirm (GrCookingPage *page)
{
        gtk_widget_show (page->confirm_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->confirm_revealer), TRUE);
}

static void
leave_cooking_mode (GrCookingPage *page,
                    gboolean       stop_timers)
{
        GtkWidget *window;
        GtkApplication *app;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        app = gtk_window_get_application (GTK_WINDOW (window));

        if (page->inhibit_cookie != 0) {
                gtk_application_uninhibit (app, page->inhibit_cookie);
                page->inhibit_cookie = 0;
        }

        gr_window_show_recipe (GR_WINDOW (window), page->recipe);
        gr_cooking_view_stop (GR_COOKING_VIEW (page->cooking_view), FALSE);
        gr_window_set_fullscreen (GR_WINDOW (window), FALSE);
}

static void
confirm_close (GrCookingPage *page)
{
        show_confirm (page);
}

static void
keep_cooking (GrCookingPage *page)
{
        hide_confirm (page, TRUE);
}

static void
stop_timers (GrCookingPage *page)
{
        leave_cooking_mode (page, TRUE);
}

static void
continue_timers (GrCookingPage *page)
{
        leave_cooking_mode (page, FALSE);
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
                hide_confirm (page, FALSE);
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

                gr_cooking_view_start (GR_COOKING_VIEW (page->cooking_view));
                update_steppers (page);

                gr_window_set_fullscreen (GR_WINDOW (window), TRUE);
        }
        else {
                gboolean has_timers;

                has_timers = gr_cooking_view_has_active_timers (GR_COOKING_VIEW (page->cooking_view));

                if (has_timers)
                        confirm_close (page);
                else
                        leave_cooking_mode (page, TRUE);
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

static void
forward (GrCookingPage *page)
{
        gr_cooking_view_forward (GR_COOKING_VIEW (page->cooking_view));
        update_steppers (page);
}

static gboolean
doubletap_timeout (gpointer data)
{
        GrCookingPage *page = data;

        if (page->keyval_seen) {
                page->keyval_seen = 0;
                forward (page);
        }

        return G_SOURCE_REMOVE;
}

void
gr_cooking_page_start_cooking (GrCookingPage *page)
{
        set_cooking (page, TRUE);
}

void
gr_cooking_page_timer_expired (GrCookingPage *page,
                               int            step)
{
        set_cooking (page, TRUE);
        gr_cooking_view_timer_expired (GR_COOKING_VIEW (page->cooking_view), step);
}

static void
show_buttons (GrCookingPage *page)
{
        update_steppers (page);

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
        update_steppers (page);

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
                        forward (page);

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

                if (e->button == 8) {
                        prev_step (page);

                        return GDK_EVENT_STOP;
                }
                else if (e->button == 9) {
                        next_step (page);

                        return GDK_EVENT_STOP;
                }
        }

        return GDK_EVENT_PROPAGATE;
}

void
gr_cooking_page_show_notification (GrCookingPage *page,
                                   const char    *text,
                                   int            step)
{
        gtk_label_set_label (GTK_LABEL (page->notification_label), text);
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->notification_revealer), TRUE);
        page->notification_step = step;
}

static void
close_notification (GrCookingPage *page)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->notification_revealer), FALSE);
        update_steppers (page);
}

static void
goto_timer (GrCookingPage *page)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (page->notification_revealer), FALSE);
        gr_cooking_view_set_step (GR_COOKING_VIEW (page->cooking_view), page->notification_step);
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
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, mini_timer_box);
        gtk_widget_class_bind_template_child (widget_class, GrCookingPage, confirm_revealer);

        gtk_widget_class_bind_template_callback (widget_class, prev_step);
        gtk_widget_class_bind_template_callback (widget_class, next_step);
        gtk_widget_class_bind_template_callback (widget_class, stop_cooking);
        gtk_widget_class_bind_template_callback (widget_class, motion_notify);
        gtk_widget_class_bind_template_callback (widget_class, close_notification);
        gtk_widget_class_bind_template_callback (widget_class, on_child_revealed);
        gtk_widget_class_bind_template_callback (widget_class, goto_timer);
        gtk_widget_class_bind_template_callback (widget_class, on_confirm_revealed);
        gtk_widget_class_bind_template_callback (widget_class, keep_cooking);
        gtk_widget_class_bind_template_callback (widget_class, stop_timers);
        gtk_widget_class_bind_template_callback (widget_class, continue_timers);
}

void
gr_cooking_page_set_recipe (GrCookingPage *page,
                            GrRecipe      *recipe)
{
        GPtrArray *images;
        const char *id;
        const char *instructions;

        g_set_object (&page->recipe, recipe);

        images = gr_recipe_get_images (recipe);
        id = gr_recipe_get_id (recipe);
        instructions = gr_recipe_get_translated_instructions (recipe);

        container_remove_all (GTK_CONTAINER (page->mini_timer_box));

        gr_cooking_view_set_data (GR_COOKING_VIEW (page->cooking_view), id, instructions, images);
}
