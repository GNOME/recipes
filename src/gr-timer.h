/* gr-timer.h:
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GR_TYPE_TIMER (gr_timer_get_type())

G_DECLARE_FINAL_TYPE (GrTimer, gr_timer, GR, TIMER, GObject)

GrTimer    *gr_timer_new            (const char *name);

const char *gr_timer_get_name       (GrTimer    *timer);
gboolean    gr_timer_get_active     (GrTimer    *timer);
guint64     gr_timer_get_start_time (GrTimer    *timer);
guint64     gr_timer_get_duration   (GrTimer    *timer);
guint64     gr_timer_get_remaining  (GrTimer    *timer);
void        gr_timer_start          (GrTimer    *timer);
void        gr_timer_stop           (GrTimer    *timer);
void        gr_timer_reset          (GrTimer    *timer);

G_END_DECLS

