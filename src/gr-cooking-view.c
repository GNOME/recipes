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

#include "gr-cooking-view.h"
#include "gr-recipe.h"
#include "gr-recipe-formatter.h"
#include "gr-images.h"
#include "gr-utils.h"
#include "gr-timer.h"
#include "gr-window.h"

typedef struct
{
        char *heading;
        char *label;
        GrTimer *timer;
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

static void step_timer_complete (GrTimer *timer, GrCookingView *view);

static StepData *
step_data_new (const char *heading,
               const char *string,
               guint64     duration,
               int         image,
               gpointer    page)
{
        StepData *d;

        d = g_new (StepData, 1);
        d->heading = g_strdup (heading);
        d->label = g_strdup (string);
        if (duration > 0) {
                d->timer = g_object_new (GR_TYPE_TIMER,
                                         "name", "Step",
                                         "duration", duration,
                                         "active", FALSE,
                                         NULL);
                d->handler = g_signal_connect (d->timer, "complete", G_CALLBACK (step_timer_complete), page);
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

        GArray *images;
        char *instructions;

        GPtrArray *steps;
        int step;

        gboolean wide;
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

        G_OBJECT_CLASS (gr_cooking_view_parent_class)->finalize (object);
}

static void
gr_cooking_view_init (GrCookingView *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->steps = g_ptr_array_new_with_free_func (step_data_free);
        self->step = -1;
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

                g_object_get (s->timer, "active", &active, NULL);
                g_object_set (view->cooking_timer, "timer", s->timer, NULL);
                gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "timer");
        }
        else if (0 <= s->image && s->image < view->images->len) {
                GrRotatedImage *ri = NULL;
                g_autoptr(GdkPixbuf) pixbuf = NULL;

                ri = &g_array_index (view->images, GrRotatedImage, s->image);
                if (view->wide)
                        pixbuf = load_pixbuf_fill_size (ri->path, ri->angle, 640, 480);
                else
                        pixbuf = load_pixbuf_fill_size (ri->path, ri->angle, 320, 240);
                gtk_image_set_from_pixbuf (GTK_IMAGE (view->cooking_image), pixbuf);
                gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "image");
        }
        else {
                gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "empty");
        }
}

static void
step_timer_complete (GrTimer *timer, GrCookingView *view)
{
        gtk_stack_set_visible_child_name (GTK_STACK (view->cooking_stack), "complete");
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
                gr_timer_continue (s->timer);
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

        steps = gr_recipe_parse_instructions (view->instructions);

        g_ptr_array_set_size (view->steps, 0);
        for (i = 0; i < steps->len; i++) {
                GrRecipeStep *step;
                g_autofree char *title = NULL;

                step = g_ptr_array_index (steps, i);
                title = g_strdup_printf (_("Step %d/%d"), i + 1, steps->len);

                g_ptr_array_add (view->steps,
                                 step_data_new (title, step->text, step->timer, step->image, view));
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
