/* gr-cooking-view.h
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

#pragma once

#include <gtk/gtk.h>

#include "gr-recipe.h"

G_BEGIN_DECLS

#define GR_TYPE_COOKING_VIEW (gr_cooking_view_get_type ())

G_DECLARE_FINAL_TYPE (GrCookingView, gr_cooking_view, GR, COOKING_VIEW, GtkBox)

GrCookingView *gr_cooking_view_new           (void);
void           gr_cooking_view_set_data      (GrCookingView *view,
                                              const char    *recipe_id,
                                              const char    *instructions,
                                              GPtrArray     *images);
int            gr_cooking_view_get_n_steps   (GrCookingView *view);
int            gr_cooking_view_get_step      (GrCookingView *view);
void           gr_cooking_view_set_step      (GrCookingView *view,
                                              int            step);
void           gr_cooking_view_start         (GrCookingView *view);
void           gr_cooking_view_stop          (GrCookingView *view,
                                              gboolean       stop_timers);
void           gr_cooking_view_forward       (GrCookingView *view);
void           gr_cooking_view_next_step     (GrCookingView *view);
void           gr_cooking_view_prev_step     (GrCookingView *view);
void           gr_cooking_view_set_timer_box (GrCookingView *view,
                                              GtkWidget     *box);
void           gr_cooking_view_timer_expired (GrCookingView *view,
                                              int            step);
gboolean       gr_cooking_view_has_active_timers (GrCookingView *view);

G_END_DECLS

