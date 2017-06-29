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
        "tbsp", "tsp", "box", "pkg", "glass", "st", "pinch", "bunch",
        NULL
};

typedef struct {
        const char *unit;
        const char *abbreviation;
        const char *display_name;
        const char *plural;
        const char *measure;
} GrUnit;

static GrUnit units[] = {
        { "g",     NC_("unit abbreviation", "g"),     NC_("unit name", "gram"), NC_("unit plural", "grams"), NC_("unit measure", "weight") },
        { "kg",    NC_("unit abbreviation", "kg"),    NC_("unit name", "kilogram"), NC_("unit plural", "kilograms"), NC_("unit measure", "weight") },
        { "lb",    NC_("unit abbreviation", "lb"),    NC_("unit name", "pound"), NC_("unit plural", "pounds"), NC_("unit measure", "weight") },
        { "oz",    NC_("unit abbreviation", "oz"),    NC_("unit name", "ounce"), NC_("unit plural", "ounces"), NC_("unit measure", "weight") },
        { "l",     NC_("unit abbreviation", "l"),     NC_("unit name", "liter"), NC_("unit plural", "liters"), NC_("unit measure", "volume") },
        { "dl",    NC_("unit abbreviation", "dl"),    NC_("unit name", "deciliter"), NC_("unit plural", "deciliters"), NC_("unit measure", "volume") },
        { "ml",    NC_("unit abbreviation", "ml"),    NC_("unit name", "milliliter"), NC_("unit plural", "milliliters"), NC_("unit measure", "volume") },
        { "fl oz", NC_("unit abbreviation", "fl oz"), NC_("unit name", "fluid ounce"), NC_("unit plural", "fluid ounces"), NC_("unit measure", "volume") },
        { "fl. oz.", NC_("unit abbreviation", "fl oz"), NC_("unit name", "fluid ounce"), NC_("unit plural", "fluid ounces"), NC_("unit measure", "volume") },        
        { "pt",    NC_("unit abbreviation", "pt"),    NC_("unit name", "pint"), NC_("unit plural", "pints"), NC_("unit measure", "volume") },
        { "qt",    NC_("unit abbreviation", "qt"),    NC_("unit name", "quart"), NC_("unit plural", "quarts"), NC_("unit measure", "volume") },
        { "gal",   NC_("unit abbreviation", "gal"),   NC_("unit name", "gallon"), NC_("unit plural", "gallons"), NC_("unit measure", "volume") },
        { "cup",   NC_("unit abbreviation", "cup"),   NC_("unit name", "cup"), NC_("unit plural", "cups"), NC_("unit measure", "volume") },
        { "tbsp",  NC_("unit abbreviation", "tbsp"),  NC_("unit name", "tablespoon"), NC_("unit plural", "tablespoons"), NC_("unit measure", "volume") },
        { "tsp",   NC_("unit abbreviation", "tsp"),   NC_("unit name", "teaspoon"), NC_("unit plural", "teaspoons"), NC_("unit measure", "volume") },
        { "box",   NC_("unit abbreviation", "box"),   NC_("unit name", "box"), NC_("unit plural", "boxes"), NC_("unit measure", "quantity") },
        { "pkg",   NC_("unit abbreviation", "pkg"),   NC_("unit name", "package"), NC_("unit plural", "packages"), NC_("unit measure", "quantity") },
        { "glass", NC_("unit abbreviation", "glass"), NC_("unit name", "glass"), NC_("unit plural", "glasses"), NC_("unit measure", "quantity") },
        { "mm",    NC_("unit abbreviation", "mm"),    NC_("unit name", "millimeter"), NC_("unit plural", "millimeters"), NC_("unit measure", "length") },
        { "cm",    NC_("unit abbreviation", "cm"),    NC_("unit name", "centimeter"), NC_("unit plural", "centimeters"), NC_("unit measure", "length") },
        { "m",     NC_("unit abbreviation", "m"),     NC_("unit name", "meter"), NC_("unit plural", "meters"), NC_("unit measure", "length") },
        { "st",    NC_("unit abbreviation", "st"),     NC_("unit name", "stone"), NC_("unit plural", "stone") , NC_("unit measure", "weight")},
        { "pinch", NC_("unit abbreviation", "pinch"),     NC_("unit name", "pinch"), NC_("unit plural", "pinches"), NC_("unit measure", "volume") },
        { "bunch", NC_("unit abbreviation", "bunch"), NC_("unit name", "bunch"), NC_("unit plural", "bunches"), NC_("unit measure", "quantity") },

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
gr_unit_get_plural (const char *name)
{
        GrUnit *unit = find_unit (name);
        if (unit)
                return g_dpgettext2 (NULL, "unit plural", unit->plural);
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
gr_unit_get_measure (const char *name)
{
        GrUnit *unit = find_unit (name);
        if (unit)
                return g_dpgettext2 (NULL, "unit measure", unit->measure);
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

        for (i = 0; i < G_N_ELEMENTS (units); i++) {
                nu = units[i].unit;
                if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                        *input += strlen (nu);
                        return units[i].unit;
                }
        }

        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("I don’t know this unit: %s"), *input);

        return NULL;
}

