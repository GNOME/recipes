/* gr-shopping-list-exporter.h:
 *
 * Copyright (C) 2017 Ekta Nandwani
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
#include "gr-recipe.h"
#include "gr-shopping-list-printer.h"

G_BEGIN_DECLS

#define GR_TYPE_SHOPPING_LIST_EXPORTER (gr_shopping_list_exporter_get_type())

G_DECLARE_FINAL_TYPE (GrShoppingListExporter, gr_shopping_list_exporter, GR, SHOPPING_LIST_EXPORTER, GObject)

GrShoppingListExporter *gr_shopping_list_exporter_new    (GtkWindow        *window);

void					gr_shopping_list_exporter_export (GrShoppingListExporter *exporter, GList *items);

void					done_shopping_in_todoist (GrShoppingListExporter *exporter);

void					do_undo_in_todoist (GrShoppingListExporter *exporter, GList *items);
G_END_DECLS