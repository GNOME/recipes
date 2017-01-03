/* gr-image-viewer.h
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3.
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

#define GR_TYPE_IMAGE_VIEWER (gr_image_viewer_get_type())

G_DECLARE_FINAL_TYPE (GrImageViewer, gr_image_viewer, GR, IMAGE_VIEWER, GtkBox)

GrImageViewer *gr_image_viewer_new          (void);
void           gr_image_viewer_set_images   (GrImageViewer *viewer,
                                             GArray        *images);

void           gr_image_viewer_add_image    (GrImageViewer *viewer);
void           gr_image_viewer_remove_image (GrImageViewer *viewer);
void           gr_image_viewer_rotate_image (GrImageViewer *viewer,
                                             int            angle);

void           gr_image_viewer_show_image (GrImageViewer *viewer,
                                           int            idx);

G_END_DECLS

