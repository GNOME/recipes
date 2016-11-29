/* gr-timer.c:
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

struct _GrTimer
{
        GtkWidget parent_instance;

        char *name;
        gboolean active;
        guint64 duration;
        guint64 start_time;
        guint64 remaining;
        guint completion_id;
        guint remaining_id;
};

G_DEFINE_TYPE (GrTimer, gr_timer, G_TYPE_OBJECT)

enum {
        PROP_0,
        PROP_NAME,
        PROP_ACTIVE,
        PROP_DURATION,
        PROP_REMAINING,
        N_PROPS
};

enum {
        COMPLETE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

GrTimer *
gr_timer_new (const char *name)
{
        return g_object_new (GR_TYPE_TIMER,
                             "name", name,
                             NULL);
}

const char *
gr_timer_get_name (GrTimer *timer)
{
        return timer->name;
}

guint64
gr_timer_get_start_time (GrTimer *timer)
{
        return timer->start_time;
}

guint64
gr_timer_get_duration (GrTimer *timer)
{
        return timer->duration;
}

guint64
gr_timer_get_remaining (GrTimer *timer)
{
        return timer->remaining;
}

gboolean
gr_timer_get_active (GrTimer *timer)
{
        return timer->active;
}

static void set_active (GrTimer  *timer,
                        gboolean  active);

static gboolean
timer_complete (gpointer data)
{
        GrTimer *timer = data;

        set_active (timer, FALSE);
        g_signal_emit (timer, signals[COMPLETE], 0);

        return G_SOURCE_REMOVE;
}

static gboolean
remaining_update (gpointer data)
{
        GrTimer *timer = data;
        guint64 now;

        now = g_get_monotonic_time ();

        timer->remaining = timer->start_time + timer->duration - now;

        g_object_notify (G_OBJECT (timer), "remaining");

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
                timer->completion_id = g_timeout_add (timer->duration / 1000, timer_complete, timer);
                timer->remaining_id = g_timeout_add_seconds (1, remaining_update, timer);
        }
        else if (timer->completion_id) {
                g_source_remove (timer->completion_id);
                timer->completion_id = 0;
                g_source_remove (timer->remaining_id);
                timer->remaining_id = 0;
        }

        g_object_notify (G_OBJECT (timer), "active");
}

static void
set_duration (GrTimer *timer,
              guint    duration)
{
        timer->duration = duration;
        g_object_notify (G_OBJECT (timer), "duration");
}

static void
gr_timer_finalize (GObject *object)
{
        GrTimer *timer = GR_TIMER (object);

        if (timer->completion_id)
                g_source_remove (timer->completion_id);
        if (timer->remaining_id)
                g_source_remove (timer->remaining_id);
        g_free (timer->name);

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
          case PROP_NAME:
                  g_value_set_string (value, self->name);
                  break;

          case PROP_ACTIVE:
                  g_value_set_boolean (value, self->active);
                  break;

          case PROP_DURATION:
                  g_value_set_uint64 (value, self->duration);
                  break;

          case PROP_REMAINING:
                  g_value_set_uint64 (value, self->remaining);
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
          case PROP_NAME:
                  self->name = g_value_dup_string (value);
                  break;

          case PROP_ACTIVE:
                  set_active (self, g_value_get_boolean (value));
                  break;

          case PROP_DURATION:
                  set_duration (self, g_value_get_uint64 (value));
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_timer_class_init (GrTimerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_timer_finalize;
        object_class->get_property = gr_timer_get_property;
        object_class->set_property = gr_timer_set_property;

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
                                         PROP_REMAINING,
                                         g_param_spec_uint64 ("remaining", NULL, NULL,
                                                              0, G_MAXUINT64, 0,
                                                              G_PARAM_READABLE));
        g_object_class_install_property (object_class,
                                         PROP_NAME,
                                         g_param_spec_string ("name", NULL, NULL,
                                                              NULL,
                                                              G_PARAM_READWRITE));
}

static void
gr_timer_init (GrTimer *self)
{
        self->active = FALSE;
        self->duration = 0;
}
