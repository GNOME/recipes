/* gr-account.h:
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

#include "gr-chef.h"

G_BEGIN_DECLS

typedef void (* AccountInformationCallback) (const char  *id,
                                             const char  *name,
                                             const char  *image_path,
                                             gpointer     data,
                                             GError      *error);

void    gr_account_get_information (GtkWindow                  *window,
                                    AccountInformationCallback  callback,
                                    gpointer                    data,
                                    GDestroyNotify              destroy);

typedef void (*UserChefCallback) (GrChef   *chef,
                                  gpointer  data);

void    gr_ensure_user_chef (GtkWindow        *window,
                             UserChefCallback  callback,
                             gpointer          data);

G_END_DECLS
