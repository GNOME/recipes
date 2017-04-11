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
#include <libsoup/soup.h>

G_BEGIN_DECLS

#define GR_TYPE_IMAGE (gr_image_get_type())

G_DECLARE_FINAL_TYPE (GrImage, gr_image, GR, IMAGE, GObject)

GrImage    *gr_image_new         (SoupSession       *session,
                                  const char        *id,
                                  const char        *path);
void        gr_image_set_id      (GrImage           *image,
                                  const char        *id);
void        gr_image_set_path    (GrImage           *image,
                                  const char        *path);
const char *gr_image_get_path    (GrImage           *image);
char       *gr_image_get_cache_path (GrImage        *image);
GdkPixbuf  *gr_image_load_sync   (GrImage           *image,
                                  int                 width,
                                  int                 height,
                                  gboolean            fit);

typedef void (*GrImageCallback) (GrImage   *ri,
                                 GdkPixbuf *pixbuf,
                                 gpointer   data);

void        gr_image_load        (GrImage            *ri,
                                  int                 width,
                                  int                 height,
                                  gboolean            fit,
                                  GCancellable       *cancellable,
                                  GrImageCallback     callback,
                                  gpointer            data);

void        gr_image_set_pixbuf  (GrImage   *ri,
                                  GdkPixbuf *pixbuf,
                                  gpointer   data);

GPtrArray *gr_image_array_new (void);

G_END_DECLS
