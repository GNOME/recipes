/* gr-list-page.h:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
#include "gr-chef.h"

G_BEGIN_DECLS

#define GR_TYPE_LIST_PAGE (gr_list_page_get_type ())

G_DECLARE_FINAL_TYPE (GrListPage, gr_list_page, GR, LIST_PAGE, GtkBox)

GtkWidget      *gr_list_page_new                     (void);

void            gr_list_page_populate_from_diet      (GrListPage *self,
                                                      GrDiets     diet);
void            gr_list_page_populate_from_chef      (GrListPage *self,
                                                      GrChef     *chef,
                                                      gboolean    show_shared);
void            gr_list_page_populate_from_season    (GrListPage *self,
                                                      const char *season);
void            gr_list_page_populate_from_favorites (GrListPage *self);
void            gr_list_page_populate_from_list      (GrListPage *self,
                                                      GList      *recipes);
void            gr_list_page_populate_from_all       (GrListPage *self);
void            gr_list_page_populate_from_new       (GrListPage *self);
void            gr_list_page_clear                   (GrListPage *self);
void            gr_list_page_repopulate              (GrListPage *self);
void            gr_list_page_set_show_shared         (GrListPage *self,
                                                      gboolean    show_shared);

typedef enum {
        SORT_BY_NAME,
        SORT_BY_RECENCY
} GrSortKey;

G_END_DECLS
