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
#include "gr-images.h"
#include "gr-utils.h"
#include "gr-timer.h"
#include "gr-window.h"
#include "gr-cooking-page.h"
#include "gr-timer-widget.h"

typedef struct
{
        GrCookingView *view;
        char *heading;
        char *label;
        GrTimer *timer;
        GtkWidget *mini_timer;
        gulong handler;
        guint64 duration;
        int image;
} StepData;

static void
step_data_free (gpointer data)
{
        StepData *d = data;

        if (d->timer)
                g_signal_handler_disconnect (d->timer, d->handler);
        g_clear_object (&d->timer);
        g_free (d->heading);
        g_free (d->label);
        g_free (d);
}

static void step_timer_complete (GrTimer *timer, StepData *step);

static StepData *
step_data_new (int         num,
               int         n_steps,
               const char *label,
               guint64     duration,
               int         image,
               gpointer    view)
{
        StepData *d;

        d = g_new (StepData, 1);
        d->view = view;
        d->heading = g_strdup_printf (_("Step %d/%d"), num, n_steps);
        d->label = g_strdup (label);
        if (duration > 0) {
                g_autofree char *name = NULL;

                name = g_strdup_printf (_("Step %d"), num);
                d->timer = g_object_new (GR_TYPE_TIMER,
                                         "name", name,
                                         "duration", duration,
                                         "active", FALSE,
                                         NULL);
                d->handler = g_signal_connect (d->timer, "complete", G_CALLBACK (step_timer_complete), d);
        }
        else {
                d->timer = NULL;
                d->handler = 0;
        }
        d->duration = duration;
        d->image = image;

        return d;
}

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

        GArray *images;
        char *instructions;

        GPtrArray *steps;
        int step;

        gboolean wide;

#ifdef ENABLE_CANBERRA
        ca_context *c;
#endif
};


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

        g_clear_pointer (&self->images, g_array_unref);
        g_clear_pointer (&self->instructions, g_free);
        g_clear_pointer (&self->steps, g_ptr_array_unref);

#ifdef ENABLE_CANBERRA
        ca_context_destroy (self->c);
#endif

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
setup_step (GrCookingView *view)
{
        StepData *s;

        if (!view->images)
                return;

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

        if (s->timer) {
                gboolean active;
                guint64 remaining;

                gtk_widget_show (view->cooking_stack);
                gtk_widget_set_halign (view->text_box, GTK_ALIGN_START);

                active = gr_timer_get_active (s->timer);
                remaining = gr_timer_get_remaining (s->timer);
                g_object_set (view->cooking_timer, "timer", s->timer, NULL);

                if (active || remaining > 0)
                        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "timer");
                else
                        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "complete");
        }
        else if (0 <= s->image && s->image < view->images->len) {
                GrImage *ri = NULL;
                g_autoptr(GdkPixbuf) pixbuf = NULL;

                gtk_widget_show (view->cooking_stack);
                gtk_widget_set_halign (view->text_box, GTK_ALIGN_START);
                ri = &g_array_index (view->images, GrImage, s->image);
                if (view->wide)
                        pixbuf = load_pixbuf_fill_size (ri->path, 0, 640, 480);
                else
                        pixbuf = load_pixbuf_fill_size (ri->path, 0, 320, 240);
                gtk_image_set_from_pixbuf (GTK_IMAGE (view->cooking_image), pixbuf);
                gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "image");
        }
        else {
                gtk_widget_hide (view->cooking_stack);
                gtk_widget_set_halign (view->text_box, GTK_ALIGN_CENTER);
                gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "empty");
        }
}

static void
play_complete_sound (StepData *step)
{
#ifdef ENABLE_CANBERRA
        GrCookingView *view = step->view;
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
send_complete_notification (StepData *step)
{
        GrCookingView *view = step->view;
        GtkWidget *page;

        page = gtk_widget_get_ancestor (GTK_WIDGET (view), GR_TYPE_COOKING_PAGE);
        if (page) {
                g_autofree char *text = NULL;

                text = g_strdup_printf (_("Timer for “%s” has expired."),
                                        gr_timer_get_name (step->timer));
                gr_cooking_page_show_notification (GR_COOKING_PAGE (page), text);
        }
}

static void
step_timer_complete (GrTimer *timer, StepData *step)
{
        GrCookingView *view = step->view;
        StepData *current;

        current = g_ptr_array_index (view->steps, view->step);
        if (step == current)
                gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "complete");
        else
                send_complete_notification (step);

        if (step->mini_timer)
                gtk_widget_destroy (step->mini_timer);

        play_complete_sound (step);
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
                data = step_data_new (i + 1, steps->len, step->text, step->timer, step->image, view);
                g_ptr_array_add (view->steps, data);

                if (view->timer_box && data->timer) {
                        GtkWidget *box;
                        GtkWidget *tw;
                        GtkWidget *label;

                        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
                        tw = g_object_new (GR_TYPE_TIMER_WIDGET,
                                           "timer", data->timer,
                                           "size", 32,
                                           "visible", TRUE,
                                           NULL);
                        label = gtk_label_new (gr_timer_get_name (data->timer));
                        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
                        gtk_widget_show (label);
                        gtk_style_context_add_class (gtk_widget_get_style_context (label), "cooking-heading");

                        gtk_container_add (GTK_CONTAINER (box), tw);
                        gtk_container_add (GTK_CONTAINER (box), label);
                        gtk_container_add (GTK_CONTAINER (view->timer_box), box);
                        g_signal_connect_swapped (data->timer, "notify::active", G_CALLBACK (gtk_widget_show), box);
                        data->mini_timer = box;
                }
        }
}

void
gr_cooking_view_set_images (GrCookingView *view,
                            GArray        *images,
                            int            index)
{
        view->images = g_array_ref (images);

        setup_steps (view);
        set_step (view, index);
}

void
gr_cooking_view_set_instructions (GrCookingView *view,
                                  const char    *instructions)
{
        g_free (view->instructions);
        view->instructions = g_strdup (instructions);

        setup_steps (view);
        set_step (view, 0);
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
gr_cooking_view_next_step (GrCookingView *view)
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
gr_cooking_view_set_timer_box (GrCookingView *view,
                               GtkWidget     *box)
{
        view->timer_box = box;
}
