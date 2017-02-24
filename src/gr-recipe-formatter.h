/* gr-recipe-formatter.h:
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

#pragma once

#include <gtk/gtk.h>
#include "gr-recipe.h"

G_BEGIN_DECLS

char *gr_recipe_format (GrRecipe *recipe);

typedef struct {
        char *text;
        int image;
        guint64 timer;
        char *title;
} GrRecipeStep;

GPtrArray *gr_recipe_parse_instructions (const char *instructions,
                                         gboolean    format_for_display);

G_END_DECLS
