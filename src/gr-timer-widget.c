/* gr-timer-widget.c:
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

#include <math.h>
#include "gr-timer.h"
#include "gr-timer-widget.h"

struct _GrTimerWidget
{
        GtkWidget parent_instance;

        GrTimer *timer;
        int size;
        guint tick_id;
        gulong handler_id;
};

G_DEFINE_TYPE (GrTimerWidget, gr_timer_widget, GTK_TYPE_WIDGET)

enum {
        PROP_0,
        PROP_TIMER,
        PROP_SIZE,
        N_PROPS
};

GrTimerWidget *
gr_timer_widget_new (void)
{
        return g_object_new (GR_TYPE_TIMER_WIDGET, NULL);
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
        GrTimerWidget *timer = GR_TIMER_WIDGET (widget);

        gtk_widget_queue_resize (GTK_WIDGET (timer));

        return G_SOURCE_CONTINUE;
}

static void
timer_active_changed (GrTimer       *timer,
                      GParamSpec    *pspec,
                      GrTimerWidget *self)
{
        gboolean active = FALSE;

        if (timer)
                active = gr_timer_get_active (timer);

        if (active && self->tick_id == 0) {
                self->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, NULL, NULL);
        }
        else if (!active && self->tick_id != 0) {
                gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_id);
                self->tick_id = 0;
        }

        gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_timer (GrTimerWidget *self,
           GrTimer       *timer)
{
        if (self->timer == timer)
                return;

        if (self->timer) {
                g_signal_handler_disconnect (self->timer, self->handler_id);
                self->handler_id = 0;
                g_clear_object (&self->timer);
        }
        g_set_object (&self->timer, timer);
        if (self->timer) {
                self->handler_id = g_signal_connect (self->timer, "notify::active",
                                                     G_CALLBACK (timer_active_changed), self);
        }

        timer_active_changed (self->timer, NULL, self);

        g_object_notify (G_OBJECT (self), "timer");
}

static void
set_size (GrTimerWidget *timer,
          int            size)
{
        timer->size = size;
        gtk_widget_queue_resize (GTK_WIDGET (timer));
        g_object_notify (G_OBJECT (timer), "size");
}

static void
gr_timer_widget_finalize (GObject *object)
{
        GrTimerWidget *self = GR_TIMER_WIDGET (object);

        g_clear_object (&self->timer);

        G_OBJECT_CLASS (gr_timer_widget_parent_class)->finalize (object);
}

static void
gr_timer_widget_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        GrTimerWidget *self = GR_TIMER_WIDGET (object);

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
gr_timer_widget_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        GrTimerWidget *self = GR_TIMER_WIDGET (object);

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
gr_timer_widget_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
        GrTimerWidget *timer = GR_TIMER_WIDGET (widget);

        *minimum = *natural = timer->size;
}

static void
gr_timer_widget_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
        GrTimerWidget *timer = GR_TIMER_WIDGET (widget);

        *minimum = *natural = timer->size;
}

static gboolean
gr_timer_widget_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
        GrTimerWidget *timer = GR_TIMER_WIDGET (widget);
        GtkStyleContext *context;
        gint width, height;
        double xc, yc;
        double radius;
        double angle1, angle2;
        guint64 now;
        gboolean active = FALSE;

        if (timer->timer)
                active = gr_timer_get_active (timer->timer);

  	context = gtk_widget_get_style_context (widget);
  	width = gtk_widget_get_allocated_width (widget);
  	height = gtk_widget_get_allocated_height (widget);

  	gtk_render_background (context, cr, 0, 0, width, height);
  	gtk_render_frame (context, cr, 0, 0, width, height);

        now = g_get_monotonic_time ();

        cairo_set_source_rgb (cr, 0, 0, 0);
        xc = width / 2;
        yc = height / 2;
        radius = width / 2;

        if (active) {
        	angle1 = ((now - gr_timer_get_start_time (timer->timer)) * 2 * M_PI / gr_timer_get_duration (timer->timer)) + (3 * M_PI / 2);
        	angle2 = 3 * M_PI / 2;

                cairo_arc (cr, xc, yc, radius, angle1, angle2);
        	cairo_line_to (cr, xc, yc);
        }
        else {
                cairo_arc (cr, xc, yc, radius, 0, 2 * M_PI);
        }

        cairo_close_path (cr);
        cairo_fill (cr);

  	return FALSE;
}

static void
gr_timer_widget_class_init (GrTimerWidgetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = gr_timer_widget_finalize;
        object_class->get_property = gr_timer_widget_get_property;
        object_class->set_property = gr_timer_widget_set_property;

        widget_class->get_preferred_width = gr_timer_widget_get_preferred_width;
        widget_class->get_preferred_height = gr_timer_widget_get_preferred_height;
        widget_class->draw = gr_timer_widget_draw;

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
}

static void
gr_timer_widget_init (GrTimerWidget *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_set_can_focus (GTK_WIDGET (self), FALSE);
        self->size = 32;
}
