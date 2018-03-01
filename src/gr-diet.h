/* gr-diet.h:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
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

G_BEGIN_DECLS

typedef enum { /*< flags >*/
        GR_DIET_GLUTEN_FREE   =  1,
        GR_DIET_NUT_FREE      =  2,
        GR_DIET_VEGAN         =  4,
        GR_DIET_VEGETARIAN    =  8,
        GR_DIET_MILK_FREE     = 16,
        GR_DIET_HALAL         = 32
} GrDiets;


const char      *gr_diet_get_label       (GrDiets diet);
const char      *gr_diet_get_description (GrDiets diet);

G_END_DECLS
