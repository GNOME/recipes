/* gr-window.h:
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

#include "gr-app.h"
#include "gr-recipe.h"
#include "gr-chef.h"

G_BEGIN_DECLS

#define GR_TYPE_WINDOW (gr_window_get_type())

G_DECLARE_FINAL_TYPE (GrWindow, gr_window, GR, WINDOW, GtkApplicationWindow)

GrWindow       *gr_window_new                        (GrApp      *app);

void            gr_window_present_dialog             (GrWindow   *window,
                                                      GtkWindow  *dialog);
void            gr_window_set_fullscreen             (GrWindow   *window,
                                                      gboolean    fullscreen);
void            gr_window_go_back                    (GrWindow   *window);

void            gr_window_show_recipe                (GrWindow   *window,
                                                      GrRecipe   *recipe);
void            gr_window_edit_recipe                (GrWindow   *window,
                                                      GrRecipe   *recipe);
void            gr_window_show_search                (GrWindow   *window,
                                                      const char *terms);
void            gr_window_show_diet                  (GrWindow   *window,
                                                      GrDiets     diet);
void            gr_window_show_chef                  (GrWindow   *window,
                                                      GrChef     *chef);
void            gr_window_show_mine                  (GrWindow   *window);
void            gr_window_show_favorites             (GrWindow   *window);
void            gr_window_show_all                   (GrWindow   *window);
void            gr_window_show_new                   (GrWindow   *window);
void            gr_window_show_list                  (GrWindow   *window,
                                                      const char *title,
                                                      GList      *recipes);
void            gr_window_show_shopping              (GrWindow   *window);
void            gr_window_show_cuisine               (GrWindow   *window,
                                                      const char *cuisine,
                                                      const char *title);
void            gr_window_show_season                (GrWindow   *window,
                                                      const char *season,
                                                      const char *title);
void            gr_window_show_image                 (GrWindow   *window,
                                                      GPtrArray  *images,
                                                      int         index);

void            gr_window_offer_undelete             (GrWindow   *window,
                                                      GrRecipe   *recipe);
void            gr_window_offer_contribute           (GrWindow   *window,
                                                      GrRecipe   *recipe);
void            gr_window_offer_shopping             (GrWindow   *window);

void            gr_window_show_my_chef_information   (GrWindow   *window);
void            gr_window_show_about_dialog          (GrWindow   *window);
void            gr_window_show_report_issue          (GrWindow   *window);
void            gr_window_show_news                  (GrWindow   *window);
void            gr_window_show_surprise              (GrWindow   *window);

void            gr_window_load_recipe                (GrWindow   *window,
                                                      GFile      *file);
void            gr_window_save_all                   (GrWindow   *window);
void            gr_window_timer_expired              (GrWindow   *window,
                                                      GrRecipe   *recipe,
                                                      int         step);
void            gr_window_confirm_shopping_exported  (GrWindow *window);

G_END_DECLS
