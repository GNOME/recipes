/* gr-meal.c:
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "gr-meal.h"


static const char *names[] = {
        "entree",
        "snacks",
        "breakfast",
        "side",
        "dessert",
        "cake",
        "drinks",
        "pizza",
        "pasta",
        "salad",
        "other",
        NULL
};

static const char *titles[] = {
        N_("Main Course"),
        N_("Snacks"),
        N_("Breakfast"),
        N_("Side Dishes"),
        N_("Desserts"),
        N_("Cake and Baking"),
        N_("Drinks and Cocktails"),
        N_("Pizza"),
        N_("Pasta"),
        N_("Salad"),
        N_("Other")
};

const char **
gr_meal_get_names (int *length)
{
        if (length)
                *length = G_N_ELEMENTS (names) - 1;

        return names;
}

const char *
gr_meal_get_title (const char *name)
{
        int i;

        for (i = 0; i < G_N_ELEMENTS (names); i++) {
                if (g_strcmp0 (name, names[i]) == 0) {
                        return _(titles[i]);
                }
        }

        return NULL;
}
