/* gr-shopping-list-formatter.c:
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

#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include "gr-shopping-list-formatter.h"
#include "gr-ingredients-list.h"
#include "gr-image.h"
#include "gr-chef.h"
#include "gr-recipe-store.h"
#include "gr-utils.h"

char *
gr_shopping_list_format (GList *recipes,
                         GList *items)
{
        GString *s;
        GList *l;

        s = g_string_new ("");

        g_string_append_printf (s, "*** %s ***\n", _("Shopping List"));
        g_string_append (s, "\n");
        g_string_append_printf (s, "%s\n", _("For the following recipes:"));
        for (l = recipes; l; l = l->next) {
                GrRecipe *recipe = l->data;
                g_string_append_printf (s, "%s\n", gr_recipe_get_translated_name (recipe));
        }

        for (l = items; l; l = l->next) {
                ShoppingListItem *item = l->data;
                g_string_append (s, "\n");
                g_string_append_printf (s, "%s %s", item->amount, item->name);
        }

        return g_string_free (s, FALSE);
}
