/* gr-utils.c
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

#include "gr-utils.h"

#include "config.h"


GdkPixbuf *
load_pixbuf_at_size (const char *path,
                     int         width,
                     int         height)
{
        g_autoptr(GdkPixbuf) original = NULL;
        GdkPixbuf *pixbuf;
        int dest_x, dest_y, dest_width, dest_height;

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
        gdk_pixbuf_fill (pixbuf, 0x00000000);

        original = gdk_pixbuf_new_from_file_at_scale (path, width, height, TRUE, NULL);
        if (original) {
                dest_width = gdk_pixbuf_get_width (original);
                dest_height = gdk_pixbuf_get_height (original);
                dest_x = (width - dest_width) / 2;
                dest_y = (height - dest_height) / 2;

                gdk_pixbuf_composite (original, pixbuf,
                                      dest_x, dest_y, dest_width, dest_height,
                                      0, 0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
        }

        return pixbuf;
}

char *
get_data_dir (void)
{
        char *dir;

        dir = g_build_filename (g_get_user_data_dir (), "recipes", NULL);
        g_mkdir_with_parents (dir, 0755);

        return dir;
}

void
container_remove_all (GtkContainer *container)
{
        GList *children, *l;

        children = gtk_container_get_children (container);
        for (l = children; l; l = l->next) {
                gtk_container_remove (container, GTK_WIDGET (l->data));
        }
        g_list_free (children);
}

