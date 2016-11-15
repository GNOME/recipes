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

GdkPixbuf *load_pixbuf_fit_size (const char *path,
			     	 int         angle,
                                 int         width,
                                 int         height);

GdkPixbuf *load_pixbuf_fill_size (const char *path,
				  int         angle,
                                  int         width,
                                  int         height);

const char *get_pkg_data_dir  (void);
const char *get_user_data_dir (void);

void container_remove_all (GtkContainer *container);

G_END_DECLS
