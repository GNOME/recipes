/* gr-ingredients-list.h:
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

#pragma once

#include <gtk/gtk.h>

#include "gr-unit.h"

G_BEGIN_DECLS

#define GR_TYPE_INGREDIENTS_LIST (gr_ingredients_list_get_type ())

G_DECLARE_FINAL_TYPE (GrIngredientsList, gr_ingredients_list, GR, INGREDIENTS_LIST, GObject)

GrIngredientsList *gr_ingredients_list_new             (const char         *text);

gboolean           gr_ingredients_list_validate        (const char         *text,
                                                        GError            **error);
char              *gr_ingredients_list_scale           (GrIngredientsList  *ingredients,
                                                        int                 num,
                                                        int                 denom);
char              *gr_ingredients_list_scale_unit      (GrIngredientsList  *ingredients,
                                                        const char         *segment,
                                                        const char         *ingredient,
                                                        double              scale);
char             **gr_ingredients_list_get_segments    (GrIngredientsList  *ingredients);
char             **gr_ingredients_list_get_ingredients (GrIngredientsList  *ingredients,
                                                        const char         *segment);
GrUnit             gr_ingredients_list_get_unit        (GrIngredientsList  *list,
                                                        const char         *segment,
                                                        const char         *ingredient);
double             gr_ingredients_list_get_amount      (GrIngredientsList  *list,
                                                        const char         *segment,
                                                        const char         *ingredient);

G_END_DECLS
