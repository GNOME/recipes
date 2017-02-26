/* gr-spice-row.h:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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
#include <libgd/gd.h>

G_BEGIN_DECLS

#define GR_TYPE_SPICE_ROW (gr_spice_row_get_type())

G_DECLARE_FINAL_TYPE (GrSpiceRow, gr_spice_row, GR, SPICE_ROW, GtkListBoxRow)

GrSpiceRow   *gr_spice_row_new       (const char *spice,
                                      gboolean    less);

void         gr_spice_row_set_entry (GrSpiceRow      *row,
                                     GdTaggedEntry   *entry);

const char *gr_spice_row_get_spice       (GrSpiceRow *row);
char *      gr_spice_row_get_search_term (GrSpiceRow *row);
char *      gr_spice_row_get_label       (GrSpiceRow *row);

G_END_DECLS
