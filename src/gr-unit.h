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
        GR_UNIT_GRAM        = 1,
        GR_UNIT_KILOGRAM    = 2,
        GR_UNIT_POUND       = 3,
        GR_UNIT_OUNCE       = 4,
        GR_UNIT_LITER       = 5,
        GR_UNIT_DECILITER   = 6,
        GR_UNIT_MILLILITER  = 7,
        GR_UNIT_FLUID_OUNCE = 8,
        GR_UNIT_PINT        = 9,
        GR_UNIT_QUART       = 10,
        GR_UNIT_GALLON      = 11,
        GR_UNIT_CUP         = 12,
        GR_UNIT_TABLESPOON  = 13,
        GR_UNIT_TEASPOON    = 14,
        GR_UNIT_BOX         = 15,
        GR_UNIT_PACKAGES    = 16,
        GR_UNIT_GLASS       = 17,
        GR_UNIT_MILLIMETER  = 18,
        GR_UNIT_CENTIMETER  = 19,
        GR_UNIT_METER       = 20,
        GR_UNIT_STONE       = 21,
        GR_UNIT_PINCH       = 22,
        GR_UNIT_BUNCH       = 23

} GrUnitEnum;

const char *gr_unit_parse (char   **string,
                           GError **error);

const char **gr_unit_get_names (void);
const char  *gr_unit_get_abbreviation (const char *name);
const char  *gr_unit_get_display_name (const char *name);
const char  *gr_unit_get_plural (const char *name);
const char  *gr_unit_get_measure(const char *name);
int          gr_unit_get_id_number(const char *name);

G_END_DECLS
