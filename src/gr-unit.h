/* gr-unit.h
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

 #pragma once
 
 #include <glib.h>
 
 G_BEGIN_DECLS
 
 typedef enum {
         GR_UNIT_UNKNOWN,
         GR_UNIT_NONE,
         GR_UNIT_NUMBER,
         GR_UNIT_GRAM,
         GR_UNIT_KILOGRAM,
         GR_UNIT_POUND,
         GR_UNIT_OUNCE,
         GR_UNIT_LITER,
         GR_UNIT_DECILITER,
         GR_UNIT_MILLILITER,
         GR_UNIT_FLUID_OUNCE,
         GR_UNIT_PINT,
         GR_UNIT_QUART,
         GR_UNIT_GALLON,
         GR_UNIT_CUP,
         GR_UNIT_TABLESPOON,
         GR_UNIT_TEASPOON,
         GR_UNIT_BOX,
         GR_UNIT_PACKAGE,
         GR_UNIT_GLASS,
         GR_UNIT_MILLIMETER,
         GR_UNIT_CENTIMETER,
         GR_UNIT_METER,
         GR_UNIT_STONE,
         GR_UNIT_PINCH,
         GR_UNIT_BUNCH,
         GR_LAST_UNIT = GR_UNIT_BUNCH
 } GrUnit;
 
 typedef enum {
        GR_DIMENSION_NONE,
        GR_DIMENSION_DISCRETE,
        GR_DIMENSION_VOLUME,
        GR_DIMENSION_MASS,
        GR_DIMENSION_LENGTH
 } GrDimension;
 
 GrUnit       gr_unit_parse (char   **string,
                             GError **error);
 
 const char **gr_unit_get_names (void);
 const char  *gr_unit_get_name (GrUnit unit);
 const char  *gr_unit_get_abbreviation (GrUnit unit);
 const char  *gr_unit_get_display_name (GrUnit unit);
 const char  *gr_unit_get_plural (GrUnit unit);
 GrDimension  gr_unit_get_dimension (GrUnit unit);
 
 G_END_DECLS
 
