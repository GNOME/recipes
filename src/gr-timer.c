/*
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

#include <math.h>
#include "gr-timer.h"

struct _GrTimer
{
	GtkWidget parent_instance;

	gboolean active;
        guint64 duration;
	guint64 start_time;
        guint tick_id;
	int size;
};

G_DEFINE_TYPE (GrTimer, gr_timer, GTK_TYPE_WIDGET)

enum {
	PROP_0,
	PROP_ACTIVE,
	PROP_DURATION,
	PROP_SIZE,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum {
	COMPLETE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

GrTimer *
gr_timer_new (void)
{
	return g_object_new (GR_TYPE_TIMER, NULL);
}

static void set_active (GrTimer  *timer,
                        gboolean  active);

static void
timer_complete (GrTimer *timer)
{
	set_active (timer, FALSE);
	g_signal_emit (timer, signals[COMPLETE], 0);
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
        GrTimer *timer = GR_TIMER (widget);
        guint64 now;

        now = gdk_frame_clock_get_frame_time (frame_clock);

        gtk_widget_queue_resize (widget);

        if (now - timer->start_time >= timer->duration) {
		timer_complete (timer);
	}

        return G_SOURCE_CONTINUE;
}

static void
set_active (GrTimer  *timer,
            gboolean  active)
{
        if (timer->active == active)
                return;

        timer->active = active;

        if (active) {
		timer->start_time = g_get_monotonic_time ();
                timer->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (timer), tick_cb, NULL, NULL);
        }
        else {
                if (timer->tick_id) {
                        gtk_widget_remove_tick_callback (GTK_WIDGET (timer), timer->tick_id);
                        timer->tick_id = 0;
                }
        }

	gtk_widget_queue_draw (GTK_WIDGET (timer));

        g_object_notify (G_OBJECT (timer), "active");
}

static void
set_duration (GrTimer *timer,
              guint  duration)
{
        timer->duration = duration;
        g_object_notify (G_OBJECT (timer), "duration");
}

static void
set_size (GrTimer *timer,
          int      size)
{
	timer->size = size;
	gtk_widget_queue_resize (GTK_WIDGET (timer));
	g_object_notify (G_OBJECT (timer), "size");
}

static void
gr_timer_finalize (GObject *object)
{
	GrTimer *self = (GrTimer *)object;

	G_OBJECT_CLASS (gr_timer_parent_class)->finalize (object);
}

static void
gr_timer_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
	GrTimer *self = GR_TIMER (object);

	switch (prop_id)
	  {
          case PROP_ACTIVE:
                  g_value_set_boolean (value, self->active);
                  break;

          case PROP_DURATION:
                  g_value_set_uint64 (value, self->duration);
                  break;

          case PROP_SIZE:
                  g_value_set_int (value, self->size);
                  break;

	  default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	  }
}

static void
gr_timer_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
	GrTimer *self = GR_TIMER (object);

	switch (prop_id)
	  {
          case PROP_ACTIVE:
                  set_active (self, g_value_get_boolean (value));
                  break;

          case PROP_DURATION:
                  set_duration (self, g_value_get_uint64 (value));
                  break;

          case PROP_SIZE:
                  set_size (self, g_value_get_int (value));
                  break;

	  default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	  }
}

static void
gr_timer_get_preferred_width (GtkWidget *widget,
                              gint      *minimum,
                              gint      *natural)
{
	GrTimer *timer = GR_TIMER (widget);

	*minimum = *natural = timer->size;
}

static void
gr_timer_get_preferred_height (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
	GrTimer *timer = GR_TIMER (widget);

	*minimum = *natural = timer->size;
}

static gboolean
gr_timer_draw (GtkWidget *widget, cairo_t *cr)
{
	GrTimer *timer = GR_TIMER (widget);
	GtkStyleContext *context;
  	gint width, height;
        double xc, yc;
        double radius;
        double angle1, angle2;
        guint64 now;

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

	if (timer->active) {
        	angle1 = ((now - timer->start_time) * 2 * M_PI / timer->duration) + (3 * M_PI / 2);
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
gr_timer_class_init (GrTimerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = gr_timer_finalize;
	object_class->get_property = gr_timer_get_property;
	object_class->set_property = gr_timer_set_property;

	widget_class->get_preferred_width = gr_timer_get_preferred_width;
	widget_class->get_preferred_height = gr_timer_get_preferred_height;
	widget_class->draw = gr_timer_draw;

	signals[COMPLETE] = g_signal_new ("complete",
                                          G_TYPE_FROM_CLASS (object_class),
                                          G_SIGNAL_RUN_LAST,
					  0,
                  			  NULL, NULL,
                  		          NULL,
                  			  G_TYPE_NONE, 0);

        g_object_class_install_property (object_class,
                                         PROP_ACTIVE,
                                         g_param_spec_boolean ("active", NULL, NULL,
                                                               FALSE,
                                                               G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_DURATION,
                                         g_param_spec_uint64 ("duration", NULL, NULL,
                                                              0, G_MAXUINT64, 0,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_SIZE,
                                         g_param_spec_int ("size", NULL, NULL,
                                                           1, G_MAXINT, 32,
                                                           G_PARAM_READWRITE));
}

static void
gr_timer_init (GrTimer *self)
{
	gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
	gtk_widget_set_can_focus (GTK_WIDGET (self), FALSE);
	self->size = 32;
	self->active = FALSE;
	self->duration = 0;
}
