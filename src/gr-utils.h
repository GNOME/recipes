/* gr-utils.h:
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

G_BEGIN_DECLS

GdkPixbuf *load_pixbuf_fit_size  (const char *path,
                                  int         width,
                                  int         height,
                                  gboolean    pad);
GdkPixbuf *load_pixbuf_fill_size (const char *path,
                                  int         width,
                                  int         height);

void pixbuf_blur (GdkPixbuf *src,
                  int        radius,
                  int        iterations);

const char *get_pkg_data_dir  (void);
const char *get_user_data_dir (void);
const char *get_user_cache_dir (void);

void    container_remove_all (GtkContainer *container);

char      * date_time_to_string   (GDateTime *dt);
GDateTime * date_time_from_string (const char *string);
char      * format_date_time_difference (GDateTime *end, GDateTime *start);

gboolean skip_whitespace (char **input);
gboolean space_or_nul    (char p);

char *translate_multiline_string (const char *s);

char *generate_id (const char *s, ...);

void start_recording (void);
void stop_recording (void);
void record_step (const char *blurb);

gboolean in_flatpak_sandbox (void);
gboolean portal_available (GtkWindow  *window,
                           const char *portal);

typedef void (*WindowHandleExported) (GtkWindow  *window,
                                      const char *handle,
                                      gpointer    user_data);

gboolean
window_export_handle (GtkWindow            *window,
                      WindowHandleExported  callback,
                      gpointer              user_data);

void
window_unexport_handle (GtkWindow *window);

char *import_image (const char *path);
char *rotate_image (const char *path,
                    int         angle);
void  remove_image (const char *path);

void strv_remove (char       ***strv_in,
                  const char   *s);
void strv_prepend (char       ***strv_in,
                   const char   *s);

const char *get_version (void);
