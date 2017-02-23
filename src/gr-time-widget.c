/* gr-time-widget.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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

#include <math.h>
#include "gr-timer.h"
#include "gr-time-widget.h"
#include "gr-timer-widget.h"

struct _GrTimeWidget
{
        GtkOverlay parent_instance;

        int size;
        GrTimer *timer;
        gulong remaining_handler;
        gulong active_handler;

        GrTimerWidget *timer_widget;
        GtkWidget *time_remaining;
        GtkWidget *timer_button_stack;
        GtkWidget *pause_stack;
};

G_DEFINE_TYPE (GrTimeWidget, gr_time_widget, GTK_TYPE_OVERLAY)

enum {
        PROP_0,
        PROP_TIMER,
        PROP_SIZE,
        N_PROPS
};

GrTimeWidget *
gr_time_widget_new (void)
{
        return g_object_new (GR_TYPE_TIME_WIDGET, NULL);
}

static void
update_buttons (GrTimeWidget *self)
{
        gboolean active;
        guint64 duration;
        guint64 remaining;

        active = gr_timer_get_active (self->timer);
        duration = gr_timer_get_duration (self->timer);
        remaining = gr_timer_get_remaining (self->timer);

        if (duration == remaining)
                gtk_stack_set_visible_child_name (GTK_STACK (self->timer_button_stack), "start");
        else {
                gtk_stack_set_visible_child_name (GTK_STACK (self->timer_button_stack), "active");
                if (active)
                        gtk_stack_set_visible_child_name (GTK_STACK (self->pause_stack), "pause");
                else
                        gtk_stack_set_visible_child_name (GTK_STACK (self->pause_stack), "resume");
        }
}

static void
remaining_changed (GrTimeWidget *self)
{
        guint64 remaining;
        int seconds;
        int minutes;
        int hours;
        g_autofree char *str = NULL;

        remaining = gr_timer_get_remaining (self->timer);
        seconds = (int)(remaining / G_TIME_SPAN_SECOND);
        minutes = seconds / 60;
        seconds = seconds - 60 * minutes;
        hours = minutes / 60;
        minutes = minutes - 60 * hours;

        str = g_strdup_printf ("%02d∶%02d∶%02d", hours, minutes, seconds);
        gtk_label_set_label (GTK_LABEL (self->time_remaining), str);
        gtk_widget_queue_draw (GTK_WIDGET (self));

        update_buttons (self);
}

static void
set_timer (GrTimeWidget *self,
           GrTimer      *timer)
{
        GrTimer *old = self->timer;

        if (g_set_object (&self->timer, timer)) {
                if (self->remaining_handler) {
                        g_signal_handler_disconnect (old, self->remaining_handler);
                        self->remaining_handler = 0;
                }
                if (self->active_handler) {
                        g_signal_handler_disconnect (old, self->active_handler);
                        self->active_handler = 0;
                }
                if (timer) {
                        self->remaining_handler = g_signal_connect_swapped (timer, "notify::remaining", G_CALLBACK (remaining_changed), self);
                        self->active_handler = g_signal_connect_swapped (timer, "notify::active", G_CALLBACK (update_buttons), self);
                        remaining_changed (self);
                }
                g_object_set (self->timer_widget, "timer", timer, NULL);
                g_object_notify (G_OBJECT (self), "timer");
        }
}

static void
set_size (GrTimeWidget *self,
          int           size)
{
        self->size = size;

        g_object_set (self->timer_widget, "size", size, NULL);
        g_object_notify (G_OBJECT (self), "size");
}

static void
timer_start (GrTimeWidget *self)
{
        gr_timer_start (self->timer);
        gtk_stack_set_visible_child_name (GTK_STACK (self->timer_button_stack), "active");
        gtk_stack_set_visible_child_name (GTK_STACK (self->pause_stack), "pause");
}

static void
timer_pause (GrTimeWidget *self)
{
        if (gr_timer_get_active (self->timer)) {
                gr_timer_stop (self->timer);
                gtk_stack_set_visible_child_name (GTK_STACK (self->pause_stack), "resume");
        }
        else {
                gr_timer_start (self->timer);
                gtk_stack_set_visible_child_name (GTK_STACK (self->pause_stack), "pause");
        }
}

static void
timer_reset (GrTimeWidget *self)
{
        gr_timer_reset (self->timer);
        gtk_stack_set_visible_child_name (GTK_STACK (self->timer_button_stack), "start");
}

static void
gr_time_widget_finalize (GObject *object)
{
        GrTimeWidget *self = GR_TIME_WIDGET (object);

        if (self->remaining_handler)
                g_signal_handler_disconnect (self->timer, self->remaining_handler);
        if (self->active_handler)
                g_signal_handler_disconnect (self->timer, self->active_handler);
        g_clear_object (&self->timer);

        G_OBJECT_CLASS (gr_time_widget_parent_class)->finalize (object);
}

static void
gr_time_widget_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
        GrTimeWidget *self = GR_TIME_WIDGET (object);

        switch (prop_id)
          {
          case PROP_TIMER:
                  g_value_set_object (value, self->timer);
                  break;

          case PROP_SIZE:
                  g_value_set_int (value, self->size);
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_time_widget_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
        GrTimeWidget *self = GR_TIME_WIDGET (object);

        switch (prop_id)
          {
          case PROP_TIMER:
                  set_timer (self, g_value_get_object (value));
                  break;

          case PROP_SIZE:
                  set_size (self, g_value_get_int (value));
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_time_widget_class_init (GrTimeWidgetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = gr_time_widget_finalize;
        object_class->get_property = gr_time_widget_get_property;
        object_class->set_property = gr_time_widget_set_property;

        g_object_class_install_property (object_class,
                                         PROP_TIMER,
                                         g_param_spec_object ("timer", NULL, NULL,
                                                              GR_TYPE_TIMER,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_SIZE,
                                         g_param_spec_int ("size", NULL, NULL,
                                                           1, G_MAXINT, 32,
                                                           G_PARAM_READWRITE));

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-time-widget.ui");
        gtk_widget_class_bind_template_child (widget_class, GrTimeWidget, timer_widget);
        gtk_widget_class_bind_template_child (widget_class, GrTimeWidget, time_remaining);
        gtk_widget_class_bind_template_child (widget_class, GrTimeWidget, timer_button_stack);
        gtk_widget_class_bind_template_child (widget_class, GrTimeWidget, pause_stack);

        gtk_widget_class_bind_template_callback (widget_class, timer_start);
        gtk_widget_class_bind_template_callback (widget_class, timer_pause);
        gtk_widget_class_bind_template_callback (widget_class, timer_reset);
}

static void
gr_time_widget_init (GrTimeWidget *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));

        self->size = 32;
}
