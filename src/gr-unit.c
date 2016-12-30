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
#include "gr-utils.h"

static const char * const unit_names[] = {
        "g", "kg", "lb", "oz", "l", "dl", "ml", "fl oz", "pt", "qt", "gal", "cup",
        "tbsp", "tsp", "box", "pkg",
        NULL
};

typedef struct {
        const char *unit;
        const char *abbreviation;
        const char *display_name;
} GrUnit;

static GrUnit units[] = {
        { "g",     NC_("unit abbreviation", "g"),     NC_("unit name", "gram") },
        { "kg",    NC_("unit abbreviation", "kg"),    NC_("unit name", "kilogram") },
        { "lb",    NC_("unit abbreviation", "lb"),    NC_("unit name", "pound") },
        { "oz",    NC_("unit abbreviation", "oz"),    NC_("unit name", "ounce") },
        { "l",     NC_("unit abbreviation", "l"),     NC_("unit name", "liter") },
        { "dl",    NC_("unit abbreviation", "dl"),    NC_("unit name", "deciliter") },
        { "ml",    NC_("unit abbreviation", "ml"),    NC_("unit name", "milliliter") },
        { "fl oz", NC_("unit abbreviation", "fl oz"), NC_("unit name", "fluid ounce") },
        { "pt",    NC_("unit abbreviation", "pt"),    NC_("unit name", "pint") },
        { "qt",    NC_("unit abbreviation", "qt"),    NC_("unit name", "quart") },
        { "gal",   NC_("unit abbreviation", "gal"),   NC_("unit name", "gallon") },
        { "cup",   NC_("unit abbreviation", "cup"),   NC_("unit name", "cup") },
        { "tbsp",  NC_("unit abbreviation", "tbsp"),  NC_("unit name", "tablespoon") },
        { "tsp",   NC_("unit abbreviation", "tsp"),   NC_("unit name", "teaspoon") },
        { "box",   NC_("unit abbreviation", "box"),   NC_("unit name", "box") },
        { "pkg",   NC_("unit abbreviation", "pkg"),   NC_("unit name", "package") },
};

const char **
gr_unit_get_names (void)
{
        return (const char **)unit_names;
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
                return g_dpgettext2 (NULL, "unit name", unit->display_name);
        return NULL;
}

const char *
gr_unit_get_abbreviation (const char *name)
{
        GrUnit *unit = find_unit (name);
        if (unit)
                return g_dpgettext2 (NULL, "unit abbreviation", unit->abbreviation);
        return NULL;
}

const char *
gr_unit_parse (char   **input,
               GError **error)
{
        int i;
        const char *nu;

        for (i = 0; i < G_N_ELEMENTS (units); i++) {
                nu = g_dpgettext2 (NULL, "unit name", units[i].display_name);
                if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                        *input += strlen (nu);
                        return units[i].unit;
                }
        }

        for (i = 0; i < G_N_ELEMENTS (units); i++) {
                nu = g_dpgettext2 (NULL, "unit abbreviation", units[i].abbreviation);
                if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                        *input += strlen (nu);
                        return units[i].unit;
                }
        }

        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("I don’t know this unit: %s"), *input);

        return NULL;
}

