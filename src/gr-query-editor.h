/* gr-query-editor.h:
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

#define GR_TYPE_QUERY_EDITOR (gr_query_editor_get_type())

G_DECLARE_FINAL_TYPE (GrQueryEditor, gr_query_editor, GR, QUERY_EDITOR, GtkSearchBar)

GrQueryEditor *gr_query_editor_new (void);

void           gr_query_editor_set_query (GrQueryEditor  *editor,
                                          const char     *query);
const char **  gr_query_editor_get_terms (GrQueryEditor  *editor);
void           gr_query_editor_set_terms (GrQueryEditor  *editor,
                                          const char    **query);

gboolean       gr_query_editor_handle_event (GrQueryEditor *editor,
                                             GdkEvent      *event);

G_END_DECLS
