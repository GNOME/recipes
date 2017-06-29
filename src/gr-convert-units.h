/* gr-chef.h:
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

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgd/gd.h>
#include "gr-recipe.h"

G_BEGIN_DECLS

typedef enum {
        GR_TEMPERATURE_UNIT_CELSIUS    = 0,
        GR_TEMPERATURE_UNIT_FAHRENHEIT = 1,
        GR_TEMPERATURE_UNIT_LOCALE     = 2
} GrTemperatureUnit;

typedef enum {
        GR_VOLUME_UNIT_METRIC    = 0,
        GR_VOLUME_UNIT_IMPERIAL = 1,
        GR_VOLUME_UNIT_LOCALE     = 2
} GrVolumeUnit;

int             get_temperature_unit        (void);
void            convert_temp                (int *num, int *unit, int user_unit); 
void            convert_volume              (double *amount, char **unit); 




G_END_DECLS