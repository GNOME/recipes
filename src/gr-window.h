/* gr-window.h
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

#pragma once

#include "gr-app.h"
#include "gr-recipe.h"
#include "gr-chef.h"

G_BEGIN_DECLS

#define GR_TYPE_WINDOW (gr_window_get_type())

G_DECLARE_FINAL_TYPE (GrWindow, gr_window, GR, WINDOW, GtkApplicationWindow)

GrWindow *gr_window_new (GrApp *app);

void      gr_window_show_recipe (GrWindow *window,
                                 GrRecipe *recipe);
void      gr_window_edit_recipe (GrWindow *window,
                                 GrRecipe *recipe);

void      gr_window_go_back   (GrWindow   *window);
void      gr_window_show_diet (GrWindow   *window,
                               const char *title,
                               GrDiets     diet);
void      gr_window_show_chef (GrWindow   *window,
                               GrChef     *chef);
void      gr_window_show_cuisine (GrWindow   *window,
                                  const char *cuisine,
                                  const char *title);

G_END_DECLS
