/* gr-images.c:
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

#include "config.h"

#include "gr-images.h"

static void
gr_rotated_image_clear (gpointer data)
{
        GrRotatedImage *image = data;

        g_clear_pointer (&image->path, g_free);
        image->angle = 0;
}

GArray *
gr_rotated_image_array_new (void)
{
        GArray *a;

        a = g_array_new (TRUE, TRUE, sizeof (GrRotatedImage));
        g_array_set_clear_func (a, gr_rotated_image_clear);

        return a;
}
