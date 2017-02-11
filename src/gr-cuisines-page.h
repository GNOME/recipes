/* gr-cuisines-page.h:
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

G_BEGIN_DECLS

#define GR_TYPE_CUISINES_PAGE (gr_cuisines_page_get_type ())

G_DECLARE_FINAL_TYPE (GrCuisinesPage, gr_cuisines_page, GR, CUISINES_PAGE, GtkBox)

GtkWidget       *gr_cuisines_page_new      (void);

void             gr_cuisines_page_refresh  (GrCuisinesPage *page);
void             gr_cuisines_page_unexpand (GrCuisinesPage *page);


G_END_DECLS
