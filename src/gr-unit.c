/* gr-unit.c
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

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gr-unit.h"

static const char * const names[] = {
        "g", "kg", "lb", "oz", "l", "dl", "ml", "fl oz", "pt", "qt", "gal", "cup",
        "tbsp", "tsp", "box", "pkg",
        NULL
};

typedef struct {
        const char *unit;
        const char *abbreviation;
        const char *display_name;
        const char *plural;
} GrUnit;

static GrUnit units[] = {
        { "g",  NC_("unit", "g"), NC_("unit", "gram"), NC_("unit", "grams") },
        { "kg", NC_("unit", "kg"), NC_("unit", "kilogram"), NC_("unit", "kilograms") },
        { "lb", NC_("unit", "lb"), NC_("unit", "pound"), NC_("unit", "pounds") },
        { "oz", NC_("unit", "oz"), NC_("unit", "ounce"), NC_("unit", "ounces") },
        { "l",  NC_("unit", "l"), NC_("unit", "liter"), NC_("unit", "liters") },
        { "dl", NC_("unit", "dl"), NC_("unit", "deciliter"), NC_("unit", "deciliters") },
        { "ml", NC_("unit", "ml"), NC_("unit", "milliliter"), NC_("unit", "milliliters") },
        { "fl oz", NC_("unit", "fl oz"), NC_("unit", "fluid ounce"), NC_("unit", "fluid ounces") },
        { "pt", NC_("unit", "pt"), NC_("unit", "pint"), NC_("unit", "pints") },
        { "qt", NC_("unit", "qt"), NC_("unit", "quart"), NC_("unit", "quarts") },
        { "gal", NC_("unit", "gal"), NC_("unit", "gallon"), NC_("unit", "gallons") },
        { "cup", NC_("unit", "cup"), NC_("unit", "cup"), NC_("unit", "cups") },
        { "tbsp", NC_("unit", "tbsp"), NC_("unit", "tablespoon"), NC_("unit", "tablespoons") },
        { "tsp", NC_("unit", "tsp"), NC_("unit", "teaspoon"), NC_("unit", "teaspoons") },
        { "box", NC_("unit", "box"), NC_("unit", "box"), NC_("unit", "boxes") },
        { "pkg", NC_("unit", "pkg"), NC_("unit", "package"), NC_("unit", "packages") },
};

const char **
gr_unit_get_names (void)
{
        return (const char **)names;
}

static GrUnit *
find_unit (const char *name)
{
        int i;
        for (i = 0; i < G_N_ELEMENTS (units); i++) {
                if (strcmp (units[i].unit, name) == 0)
                        return &(units[i]);
        }
        return NULL;
}

const char *
gr_unit_get_display_name (const char *name)
{
        GrUnit *unit = find_unit (name);
        if (unit)
                return g_dpgettext2 (NULL, "unit", unit->display_name);
        return NULL;
}

const char *
gr_unit_get_plural (const char *name)
{
        GrUnit *unit = find_unit (name);
        if (unit)
                return g_dpgettext2 (NULL, "unit", unit->plural);
        return NULL;
}

const char *
gr_unit_get_abbreviation (const char *name)
{
        GrUnit *unit = find_unit (name);
        if (unit)
                return g_dpgettext2 (NULL, "unit", unit->abbreviation);
        return NULL;
}

static gboolean
space_or_nul (char p)
{
        return (p == '\0' || g_ascii_isspace (p));
}

const char *
gr_unit_parse (char   **input,
               GError **error)
{
        int i;
        const char *nu;

        for (i = 0; i < G_N_ELEMENTS (units); i++) {
                nu = g_dpgettext2 (NULL, "unit", units[i].plural);
                if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                        *input += strlen (nu);
                        return units[i].unit;
                }
        }

        for (i = 0; i < G_N_ELEMENTS (units); i++) {
                nu = g_dpgettext2 (NULL, "unit", units[i].display_name);
                if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                        *input += strlen (nu);
                        return units[i].unit;
                }
        }

        for (i = 0; i < G_N_ELEMENTS (units); i++) {
                nu = g_dpgettext2 (NULL, "unit", units[i].abbreviation);
                if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                        *input += strlen (nu);
                        return units[i].unit;
                }
        }

        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("I don’t know this unit: %s"), *input);

        return NULL;
}

