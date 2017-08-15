/* gr-shopping-page.h:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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

#define GR_TYPE_SHOPPING_PAGE (gr_shopping_page_get_type ())

G_DECLARE_FINAL_TYPE (GrShoppingPage, gr_shopping_page, GR, SHOPPING_PAGE, GtkBox)

GtkWidget      *gr_shopping_page_new      (void);

GList * get_ingredients (GrShoppingPage *page);

void            gr_shopping_page_populate (GrShoppingPage *self);

gboolean		gr_send_mail_finish (GAsyncResult *result, GError **error);

void			item_free (gpointer data);

G_END_DECLS
