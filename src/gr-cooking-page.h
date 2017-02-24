/* gr-cooking-page.h
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

#define GR_TYPE_COOKING_PAGE (gr_cooking_page_get_type())

G_DECLARE_FINAL_TYPE (GrCookingPage, gr_cooking_page, GR, COOKING_PAGE, GtkBox)

GrCookingPage *gr_cooking_page_new        (void);
void           gr_cooking_page_set_recipe (GrCookingPage *page,
                                           GrRecipe      *recipe);
void           gr_cooking_page_start_cooking (GrCookingPage *page);
void           gr_cooking_page_timer_expired (GrCookingPage *page,
                                              int            step);

gboolean       gr_cooking_page_handle_event (GrCookingPage *page,
                                             GdkEvent      *event);

void           gr_cooking_page_show_notification (GrCookingPage *page,
                                                  const char    *text,
                                                  int            step);

G_END_DECLS

