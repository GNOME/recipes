/* gr-images.h:
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct {
        char *path;
        int angle;
        gboolean dark_text;
} GrRotatedImage;

GArray *gr_rotated_image_array_new (void);

#define GR_TYPE_IMAGES (gr_images_get_type())

G_DECLARE_FINAL_TYPE (GrImages, gr_images, GR, IMAGES, GtkBox)

GrImages       *gr_images_new          (void);

void            gr_images_add_image    (GrImages *image);
void            gr_images_remove_image (GrImages *image);
void            gr_images_rotate_image (GrImages *image,
                                        gint      angle);

G_END_DECLS
