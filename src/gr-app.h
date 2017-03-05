/* gr-app.h:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
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

#define GR_TYPE_APP (gr_app_get_type())

G_DECLARE_FINAL_TYPE (GrApp, gr_app, GR, APP, GtkApplication)

GrApp         *gr_app_new              (void);
SoupSession   *gr_app_get_soup_session (GrApp *app);

G_END_DECLS
