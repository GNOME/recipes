/* gr-shell-search-provider.h:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <gio/gio.h>
#include "gr-recipe-store.h"

G_BEGIN_DECLS

#define GR_TYPE_SHELL_SEARCH_PROVIDER gr_shell_search_provider_get_type()

G_DECLARE_FINAL_TYPE (GrShellSearchProvider, gr_shell_search_provider, GR, SHELL_SEARCH_PROVIDER, GObject)

GrShellSearchProvider   *gr_shell_search_provider_new           (void);

gboolean                 gr_shell_search_provider_register      (GrShellSearchProvider  *self,
                                                                 GDBusConnection        *connection,
                                                                 GError                **error);
void                     gr_shell_search_provider_unregister    (GrShellSearchProvider  *self);

G_END_DECLS
