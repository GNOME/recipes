/* gr-cooking-view.c:
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

#ifdef ENABLE_CANBERRA
#include <canberra.h>
#endif

#include "gr-cooking-view.h"
#include "gr-recipe.h"
#include "gr-recipe-formatter.h"
#include "gr-image.h"
#include "gr-utils.h"
#include "gr-timer.h"
#include "gr-window.h"
#include "gr-cooking-page.h"
#include "gr-timer-widget.h"

struct _GrCookingView
{
        GtkBox parent_instance;

        GtkWidget *cooking_heading;
        GtkWidget *cooking_label;
        GtkWidget *cooking_image;
        GtkWidget *cooking_stack;
        GtkWidget *cooking_timer;
        GtkWidget *text_box;
        GtkWidget *timer_box;

        GPtrArray *images;
        char *id;
        char *instructions;

        GPtrArray *steps;
        int step;

        gboolean wide;

#ifdef ENABLE_CANBERRA
        ca_context *c;
#endif

        GList *timers;

        GCancellable *cancellable;
};

typedef struct
{
        GrCookingView *view;
        int num;
        char *heading;
        char *label;
        GrTimer *timer;
        gulong handler;
        guint64 duration;
        int image;
        GtkWidget *mini_timer;
} StepData;

static void
step_data_free (gpointer data)
{
        StepData *d = data;

        if (d->handler)
                g_signal_handler_disconnect (d->timer, d->handler);

        g_clear_object (&d->timer);
        g_free (d->heading);
        g_free (d->label);
        g_free (d);
}

typedef struct {
        GrTimer *timer;
        GrCookingView *view;
        gboolean use_system;
        gulong handler;
        char *id;
        int num;
} TimerData;

static void
timer_data_free (gpointer data)
{
        TimerData *td = data;

        g_free (td->id);

        g_free (td);
}

static void timer_complete (GrTimer *timer, TimerData *data);
static void go_to_step (StepData *data);

static void
timer_active (GrTimer    *timer,
              GParamSpec *pspec,
              StepData   *d)
{
        TimerData *td = g_object_get_data (G_OBJECT (timer), "timer-data");
        GrCookingView *view = td->view;

        g_assert (timer == d->timer);
        g_assert (timer == td->timer);

        if (d->mini_timer)
                gtk_widget_show (d->mini_timer);

        view->timers = g_list_prepend (view->timers, g_object_ref (timer));

        g_signal_handler_disconnect (timer, d->handler);
        d->handler = 0;

        td->handler = g_signal_connect (timer, "complete",
                                        G_CALLBACK (timer_complete), td);
}

static StepData *
step_data_new (int            num,
               int            n_steps,
               const char    *text,
               guint64        duration,
               const char    *title,
               int            image,
               GrCookingView *view)
{
        StepData *d;

        d = g_new0 (StepData, 1);
        d->view = view;
        d->num = num;
        d->heading = g_strdup_printf (_("Step %d/%d"), num + 1, n_steps);
        d->label = g_strdup (text);
        if (duration > 0) {
                g_autofree char *name = NULL;
                TimerData *td;

                name = g_strdup_printf (_("Step %d"), num + 1);
                d->timer = g_object_new (GR_TYPE_TIMER,
                                         "name", title ? title : name,
                                         "duration", duration,
                                         "active", FALSE,
                                         NULL);

                td = g_new0 (TimerData, 1);
                td->timer = d->timer;
                td->view = view;
                td->use_system = FALSE;
                td->id = g_strdup (view->id);
                td->num = num;

                g_object_set_data_full (G_OBJECT (d->timer), "timer-data", td, timer_data_free);

                d->handler = g_signal_connect (d->timer, "notify::active",
                                               G_CALLBACK (timer_active), d);

                if (view->timer_box) {
                        GtkWidget *button;
                        GtkWidget *box;
                        GtkWidget *tw;
                        GtkWidget *label;

                        button = gtk_button_new ();
                        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
                        gtk_widget_set_focus_on_click (button, FALSE);
                        gtk_style_context_add_class (gtk_widget_get_style_context (button), "osd");
                        g_signal_connect_swapped (button, "clicked", G_CALLBACK (go_to_step), d);

                        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
                        gtk_widget_show (box);
                        tw = g_object_new (GR_TYPE_TIMER_WIDGET,
                                           "timer", d->timer,
                                           "size", 32,
                                           "visible", TRUE,
                                           NULL);
                        label = gtk_label_new (gr_timer_get_name (d->timer));
                        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
                        gtk_widget_show (label);
                        gtk_style_context_add_class (gtk_widget_get_style_context (label), "cooking-heading");

                        gtk_container_add (GTK_CONTAINER (box), tw);
                        gtk_container_add (GTK_CONTAINER (box), label);
                        gtk_container_add (GTK_CONTAINER (button), box);
                        gtk_container_add (GTK_CONTAINER (view->timer_box), button);

                        g_signal_connect_object (d->timer, "complete",
                                                 G_CALLBACK (gtk_widget_destroy), button,
                                                 G_CONNECT_SWAPPED);

                        d->mini_timer = button;
                }
        }

        d->duration = duration;
        d->image = image;

        return d;
}


G_DEFINE_TYPE (GrCookingView, gr_cooking_view, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_WIDE,
        N_PROPS
};

GrCookingView *
gr_cooking_view_new (void)
{
        return g_object_new (GR_TYPE_COOKING_VIEW, NULL);
}

static void
gr_cooking_view_finalize (GObject *object)
{
        GrCookingView *self = GR_COOKING_VIEW (object);

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);

        g_clear_pointer (&self->id, g_free);
        g_clear_pointer (&self->images, g_ptr_array_unref);
        g_clear_pointer (&self->instructions, g_free);
        g_clear_pointer (&self->steps, g_ptr_array_unref);

#ifdef ENABLE_CANBERRA
        ca_context_destroy (self->c);
#endif

        g_list_free_full (self->timers, g_object_unref);

        G_OBJECT_CLASS (gr_cooking_view_parent_class)->finalize (object);
}

static void
gr_cooking_view_init (GrCookingView *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->steps = g_ptr_array_new_with_free_func (step_data_free);
        self->step = -1;

#ifdef ENABLE_CANBERRA
        ca_context_create (&self->c);
        ca_context_change_props (self->c,
                                 CA_PROP_APPLICATION_NAME, _("GNOME Recipes"),
                                 CA_PROP_APPLICATION_ID, "org.gnome.Recipes",
                                 CA_PROP_APPLICATION_ICON_NAME, "org.gnome.Recipes",
                                 NULL);
#endif
}

static void
set_pixbuf (GrImage   *ri,
            GdkPixbuf *pixbuf,
            gpointer   data)
{
        gtk_image_set_from_pixbuf (GTK_IMAGE (data), pixbuf);
}

static void
setup_step (GrCookingView *view)
{
        StepData *s;

        if (!view->images)
                return;

        g_cancellable_cancel (view->cancellable);
        g_clear_object (&view->cancellable);

        s = g_ptr_array_index (view->steps, view->step);

        if (s->heading && s->heading[0]) {
                gtk_label_set_label (GTK_LABEL (view->cooking_heading), s->heading);
                gtk_widget_show (view->cooking_heading);
        }
        else {
                gtk_widget_hide (view->cooking_heading);
        }

        if (s->label && s->label[0]) {
                gtk_label_set_label (GTK_LABEL (view->cooking_label), s->label);
                gtk_widget_show (view->cooking_label);
        }
        else {
                gtk_widget_hide (view->cooking_label);
        }

        g_object_set (view->cooking_timer, "timer", s->timer, NULL);
        if (s->timer) {
                gboolean active;
                guint64 remaining;

                gtk_widget_show (view->cooking_stack);
                gtk_widget_set_halign (view->text_box, GTK_ALIGN_START);

                active = gr_timer_get_active (s->timer);
                remaining = gr_timer_get_remaining (s->timer);

                if (active || remaining > 0)
                        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "timer");
                else
                        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "complete");
        }
        else if (0 <= s->image && s->image < view->images->len) {
                GrImage *ri = NULL;
                g_autoptr(GdkPixbuf) pixbuf = NULL;

                view->cancellable = g_cancellable_new ();

                gtk_widget_show (view->cooking_stack);
                gtk_widget_set_halign (view->text_box, GTK_ALIGN_START);
                ri = g_ptr_array_index (view->images, s->image);
                gr_image_load (ri,
                               view->wide ? 640 : 320,
                               view->wide ? 480 : 240,
                               FALSE,
                               view->cancellable,
                               set_pixbuf,
                               view->cooking_image);
                gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "image");
        }
        else {
                gtk_widget_hide (view->cooking_stack);
                gtk_widget_set_halign (view->text_box, GTK_ALIGN_CENTER);
                gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "empty");
        }
}

static void
set_step (GrCookingView *view,
          int            step)
{
        if (step < 0)
                step = 0;
        else if (step >= view->steps->len)
                step = view->steps->len - 1;

        if (view->step != step) {
                view->step = step;
                setup_step (view);
        }
}

static void
go_to_step (StepData *data)
{
        set_step (data->view, data->num);
}

static void
play_complete_sound (TimerData *td)
{
#ifdef ENABLE_CANBERRA
        GrCookingView *view = td->view;
        GtkWidget *page;

        page = gtk_widget_get_ancestor (GTK_WIDGET (view), GR_TYPE_COOKING_PAGE);
        if (page) {
                g_autofree char *path;
                path = g_build_filename (get_pkg_data_dir (), "sounds", "complete.oga", NULL);
                ca_context_play (view->c, 0,
                                 CA_PROP_MEDIA_ROLE, "alert",
                                 CA_PROP_MEDIA_FILENAME, path,
                                 CA_PROP_MEDIA_NAME, _("A cooking timer has expired"),
                                 CA_PROP_CANBERRA_CACHE_CONTROL, "permanent",
                                 NULL);
        }
#endif
}

static void
show_in_app_notification (GrTimer   *timer,
                          TimerData *td)
{
        GrCookingView *view = td->view;
        GtkWidget *page;

        page = gtk_widget_get_ancestor (GTK_WIDGET (view), GR_TYPE_COOKING_PAGE);
        if (page) {
                g_autofree char *text = NULL;

                text = g_strdup_printf (_("Timer for “%s” has expired."),
                                        gr_timer_get_name (timer));
                gr_cooking_page_show_notification (GR_COOKING_PAGE (page), text, td->num);
        }
}

static void
show_system_notification (GrTimer   *timer,
                          TimerData *td)
{
        GApplication *app = g_application_get_default ();
        g_autoptr(GNotification) notification = NULL;
        g_autofree char *body = NULL;

        notification = g_notification_new (_("Timer is up!"));

        body = g_strdup_printf (_("Timer for “%s” has expired."), gr_timer_get_name (timer));
        g_notification_set_body (notification, body);

        g_notification_set_default_action_and_target (notification, "app.timer-expired",
                                                      "(si)", td->id, td->num);

        g_application_send_notification (app, "timer", notification);
}

static void
timer_complete (GrTimer   *timer,
                TimerData *td)
{
        GrCookingView *view = td->view;

        if (td->use_system) {
                show_system_notification (timer, td);
        }
        else {
                if (view->step == td->num)
                        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "complete");
                else
                        show_in_app_notification (timer, td);
        }

        play_complete_sound (td);

        g_signal_handler_disconnect (timer, td->handler);
        td->handler = 0;

        view->timers = g_list_remove (view->timers, timer);
        g_object_unref (timer);
}

static void
step_timer_start (GrCookingView *view)
{
        StepData *s;

        s = g_ptr_array_index (view->steps, view->step);

        gr_timer_start (s->timer);
        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "timer");
}

static void
step_timer_pause (GrCookingView *view)
{
        StepData *s;

        s = g_ptr_array_index (view->steps, view->step);

        if (gr_timer_get_active (s->timer))
                gr_timer_stop (s->timer);
        else
                gr_timer_start (s->timer);
}

static void
step_timer_reset (GrCookingView *view)
{
        StepData *s;

        s = g_ptr_array_index (view->steps, view->step);

        gr_timer_reset (s->timer);
}

static void
gr_cooking_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        GrCookingView *self = GR_COOKING_VIEW (object);

        switch (prop_id) {
        case PROP_WIDE:
                g_value_set_boolean (value, self->wide);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
set_wide (GrCookingView *view,
          gboolean       wide)
{
        if (view->wide == wide)
                return;

        view->wide = wide;

        gtk_label_set_max_width_chars (GTK_LABEL (view->cooking_label), wide ? 40 : 20);
        gtk_widget_set_size_request (gtk_widget_get_parent (view->cooking_timer),
                                     wide ? 400 : 320,
                                     wide ? 400 : 320);

        g_object_notify (G_OBJECT (view), "wide");
}

static void
gr_cooking_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        GrCookingView *self = GR_COOKING_VIEW (object);

        switch (prop_id) {
        case PROP_WIDE:
                set_wide (self, g_value_get_boolean (value));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gr_cooking_view_class_init (GrCookingViewClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_cooking_view_finalize;
        object_class->set_property = gr_cooking_view_set_property;
        object_class->get_property = gr_cooking_view_get_property;

        pspec = g_param_spec_boolean ("wide", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_WIDE, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-cooking-view.ui");

        gtk_widget_class_bind_template_child (widget_class, GrCookingView, cooking_heading);
        gtk_widget_class_bind_template_child (widget_class, GrCookingView, cooking_label);
        gtk_widget_class_bind_template_child (widget_class, GrCookingView, cooking_image);
        gtk_widget_class_bind_template_child (widget_class, GrCookingView, cooking_stack);
        gtk_widget_class_bind_template_child (widget_class, GrCookingView, cooking_timer);
        gtk_widget_class_bind_template_child (widget_class, GrCookingView, text_box);

        gtk_widget_class_bind_template_callback (widget_class, step_timer_start);
        gtk_widget_class_bind_template_callback (widget_class, step_timer_pause);
        gtk_widget_class_bind_template_callback (widget_class, step_timer_reset);
}

static void
setup_steps (GrCookingView *view)
{
        g_autoptr(GPtrArray) steps = NULL;
        int i;

        if (!view->instructions || !view->images)
                return;

        view->step = -1;

        steps = gr_recipe_parse_instructions (view->instructions, TRUE);

        g_ptr_array_set_size (view->steps, 0);
        for (i = 0; i < steps->len; i++) {
                GrRecipeStep *step;
                StepData *data;

                step = g_ptr_array_index (steps, i);
                data = step_data_new (i, steps->len, step->text, step->timer, step->title, step->image, view);
                g_ptr_array_add (view->steps, data);
        }
}

void
gr_cooking_view_set_data (GrCookingView *view,
                          const char    *id,
                          const char    *instructions,
                          GPtrArray     *images)
{
        g_free (view->id);
        view->id = g_strdup (id);
        g_free (view->instructions);
        view->instructions = g_strdup (instructions);
        g_clear_pointer (&view->images, g_ptr_array_unref);
        view->images = g_ptr_array_ref (images);

        gtk_widget_hide (view->cooking_heading);
        gtk_widget_hide (view->cooking_label);
        gtk_widget_hide (view->cooking_stack);
        gtk_widget_set_halign (view->text_box, GTK_ALIGN_CENTER);
        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "empty");

        setup_steps (view);
}

int
gr_cooking_view_get_n_steps (GrCookingView *view)
{
        return view->steps->len;
}

int
gr_cooking_view_get_step (GrCookingView *view)
{
        return view->step;
}

void
gr_cooking_view_set_step (GrCookingView *view,
                          int            step)
{
        set_step (view, step);
}

void
gr_cooking_view_timer_expired (GrCookingView *view,
                               int            step)
{
        set_step (view, step);
        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "complete");
}

void
gr_cooking_view_start (GrCookingView *view)
{
        set_step (view, 0);
}

void
gr_cooking_view_stop (GrCookingView *view,
                      gboolean       stop_timers)
{
        g_object_set (view->cooking_timer, "timer", NULL, NULL);
        if (view->timer_box)
                container_remove_all (GTK_CONTAINER (view->timer_box));

        g_clear_pointer (&view->instructions, g_free);
        g_clear_pointer (&view->images, g_ptr_array_unref);
        g_ptr_array_set_size (view->steps, 0);

        if (stop_timers) {
                g_list_foreach (view->timers, (GFunc)gr_timer_reset, NULL);
                g_list_free_full (view->timers, g_object_unref);
                view->timers = NULL;
        }
        else {
                GList *l;

                for (l = view->timers; l; l = l->next) {
                        TimerData *td = g_object_get_data (G_OBJECT (l->data), "timer-data");
                        td->use_system = TRUE;
                }
        }
}

void
gr_cooking_view_forward (GrCookingView *view)
{
        StepData *s;

        s = g_ptr_array_index (view->steps, view->step);

        if (s->timer &&
            !gr_timer_get_active (s->timer) &&
             gr_timer_get_remaining (s->timer) > 0) {
                gr_timer_start (s->timer);
                return;
        }

        set_step (view, view->step + 1);
}

void
gr_cooking_view_prev_step (GrCookingView *view)
{
        set_step (view, view->step - 1);
}

void
gr_cooking_view_next_step (GrCookingView *view)
{
        set_step (view, view->step + 1);
}

void
gr_cooking_view_set_timer_box (GrCookingView *view,
                               GtkWidget     *box)
{
        view->timer_box = box;
}

gboolean
gr_cooking_view_has_active_timers (GrCookingView *view)
{
        return view->timers != NULL;
}
