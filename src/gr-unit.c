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
         GrUnit unit;
         GrDimension dimension;
         const char *name;
         const char *abbreviation;
         const char *display_name;
         const char *plural;
 } GrUnitData;
 
 static GrUnitData units[] = {
         { GR_UNIT_UNKNOWN,     GR_DIMENSION_NONE,      "",        "",                                "",                       "" },
         { GR_UNIT_NONE,        GR_DIMENSION_NONE,      "",        "",                                "",                       "" },
         { GR_UNIT_NUMBER,      GR_DIMENSION_DISCRETE,  "",        "",                                "",                       "" },
         { GR_UNIT_GRAM,        GR_DIMENSION_MASS,     "g",       NC_("unit abbreviation", "g"),     NC_("unit name", "gram"), NC_("unit plural", "grams") },
         { GR_UNIT_KILOGRAM,    GR_DIMENSION_MASS,     "kg",      NC_("unit abbreviation", "kg"),    NC_("unit name", "kilogram"), NC_("unit plural", "kilograms") },
         { GR_UNIT_POUND,       GR_DIMENSION_MASS,     "lb",      NC_("unit abbreviation", "lb"),    NC_("unit name", "pound"), NC_("unit plural", "pounds") },
         { GR_UNIT_OUNCE,       GR_DIMENSION_VOLUME,   "oz",      NC_("unit abbreviation", "oz"),    NC_("unit name", "ounce"), NC_("unit plural", "ounces") },
         { GR_UNIT_LITER,       GR_DIMENSION_VOLUME,   "l",       NC_("unit abbreviation", "l"),     NC_("unit name", "liter"), NC_("unit plural", "liters") },
         { GR_UNIT_DECILITER,   GR_DIMENSION_VOLUME,   "dl",      NC_("unit abbreviation", "dl"),    NC_("unit name", "deciliter"), NC_("unit plural", "deciliters") },
         { GR_UNIT_MILLILITER,  GR_DIMENSION_VOLUME,   "ml",      NC_("unit abbreviation", "ml"),    NC_("unit name", "milliliter"), NC_("unit plural", "milliliters") },
         { GR_UNIT_FLUID_OUNCE, GR_DIMENSION_VOLUME,   "fl oz",   NC_("unit abbreviation", "fl oz"), NC_("unit name", "fluid ounce"), NC_("unit plural", "fluid ounces") },
         { GR_UNIT_FLUID_OUNCE, GR_DIMENSION_VOLUME,   "fl. oz.", NC_("unit abbreviation", "fl oz"), NC_("unit name", "fluid ounce"), NC_("unit plural", "fluid ounces") },
         { GR_UNIT_PINT,        GR_DIMENSION_VOLUME,   "pt",      NC_("unit abbreviation", "pt"),    NC_("unit name", "pint"), NC_("unit plural", "pints") },
         { GR_UNIT_QUART,       GR_DIMENSION_VOLUME,   "qt",      NC_("unit abbreviation", "qt"),    NC_("unit name", "quart"), NC_("unit plural", "quarts") },
         { GR_UNIT_GALLON,      GR_DIMENSION_VOLUME,   "gal",     NC_("unit abbreviation", "gal"),   NC_("unit name", "gallon"), NC_("unit plural", "gallons") },
         { GR_UNIT_CUP,         GR_DIMENSION_VOLUME,   "cup",     NC_("unit abbreviation", "cup"),   NC_("unit name", "cup"), NC_("unit plural", "cups") },
         { GR_UNIT_TABLESPOON,  GR_DIMENSION_VOLUME,   "tbsp",    NC_("unit abbreviation", "tbsp"),  NC_("unit name", "tablespoon"), NC_("unit plural", "tablespoons") },
         { GR_UNIT_TEASPOON,    GR_DIMENSION_VOLUME,   "tsp",     NC_("unit abbreviation", "tsp"),   NC_("unit name", "teaspoon"), NC_("unit plural", "teaspoons") },
         { GR_UNIT_BOX,         GR_DIMENSION_DISCRETE, "box",     NC_("unit abbreviation", "box"),   NC_("unit name", "box"), NC_("unit plural", "boxes") },
         { GR_UNIT_PACKAGE,     GR_DIMENSION_DISCRETE, "pkg",     NC_("unit abbreviation", "pkg"),   NC_("unit name", "package"), NC_("unit plural", "packages") },
         { GR_UNIT_GLASS,       GR_DIMENSION_DISCRETE, "glass",   NC_("unit abbreviation", "glass"), NC_("unit name", "glass"), NC_("unit plural", "glasses") },
         { GR_UNIT_MILLIMETER,  GR_DIMENSION_LENGTH,   "mm",      NC_("unit abbreviation", "mm"),    NC_("unit name", "millimeter"), NC_("unit plural", "millimeters") },
         { GR_UNIT_CENTIMETER,  GR_DIMENSION_LENGTH,   "cm",      NC_("unit abbreviation", "cm"),    NC_("unit name", "centimeter"), NC_("unit plural", "centimeters") },
         { GR_UNIT_METER,       GR_DIMENSION_LENGTH,   "m",       NC_("unit abbreviation", "m"),     NC_("unit name", "meter"), NC_("unit plural", "meters") },
         { GR_UNIT_STONE,       GR_DIMENSION_MASS,     "st",      NC_("unit abbreviation", "st"),     NC_("unit name", "stone"), NC_("unit plural", "stone") },
         { GR_UNIT_PINCH,       GR_DIMENSION_DISCRETE, "pinch",   NC_("unit abbreviation", "pinch"),     NC_("unit name", "pinch"), NC_("unit plural", "pinches") },
         { GR_UNIT_BUNCH,       GR_DIMENSION_DISCRETE, "bunch",   NC_("unit abbreviation", "bunch"), NC_("unit name", "bunch"), NC_("unit plural", "bunches") },
 };
 
 const char **
 gr_unit_get_names (void)
 {
         return (const char **)unit_names;
 }
 
 static GrUnitData *
 find_unit (GrUnit unit)
 {
         int i;
         for (i = 0; i < G_N_ELEMENTS (units); i++) {
                 if (units[i].unit == unit)
                         return &(units[i]);
         }
         return NULL;
 }
 
 const char *
 gr_unit_get_name (GrUnit unit)
 {
         GrUnitData *data = find_unit (unit);
         if (data)
                 return data->name;
         return NULL;
 }
 
 const char *
 gr_unit_get_display_name (GrUnit unit)
 {
         GrUnitData *data = find_unit (unit);
         if (data)
                 return g_dpgettext2 (NULL, "unit name", data->display_name);
         return gr_unit_get_name (unit);
 }
 
 const char *
 gr_unit_get_plural (GrUnit unit)
 {
         GrUnitData *data = find_unit (unit);
         if (data)
                 return g_dpgettext2 (NULL, "unit plural", data->plural);
         return gr_unit_get_display_name (unit);
 
 }
 
 const char *
 gr_unit_get_abbreviation (GrUnit unit)
 {
         GrUnitData *data = find_unit (unit);
         if (data)
                 return g_dpgettext2 (NULL, "unit abbreviation", data->abbreviation);
         return gr_unit_get_display_name (unit);
 }
 
 GrDimension
 gr_unit_get_dimension (GrUnit unit)
 {
         GrUnitData *data = find_unit (unit);
         if (data)
                 return data->dimension;
         return GR_DIMENSION_NONE;
 }
 
 GrUnit
 gr_unit_parse (char   **input,
                GError **error)
 {
         int i;
         const char *nu;
 
         for (i = 1; i < G_N_ELEMENTS (units); i++) {
                 nu = g_dpgettext2 (NULL, "unit name", units[i].display_name);
                 if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                         *input += strlen (nu);
                         return units[i].unit;
                 }
         }
 
         for (i = 1; i < G_N_ELEMENTS (units); i++) {
                 nu = g_dpgettext2 (NULL, "unit abbreviation", units[i].abbreviation);
                 if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                         *input += strlen (nu);
                         return units[i].unit;
                 }
         }
 
         for (i = 1; i < G_N_ELEMENTS (units); i++) {
                 nu = units[i].name;
                 if (g_str_has_prefix (*input, nu) && space_or_nul ((*input)[strlen (nu)])) {
                         *input += strlen (nu);
                         return units[i].unit;
                 }
         }
 
         g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                      _("I don’t know this unit: %s"), *input);
 
         return GR_UNIT_UNKNOWN;
 }
 
