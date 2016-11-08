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

#include "config.h"

#include "gr-utils.h"

/* load image rotated by angle to fit in width x height while preserving
 * aspect ratio, filling seams with transparency
 */
GdkPixbuf *
load_pixbuf_fit_size (const char *path,
                      int         angle,
                      int         width,
                      int         height)
{
        g_autoptr(GdkPixbuf) original = NULL;
        GdkPixbuf *pixbuf;
        int dest_x, dest_y, dest_width, dest_height;

        int load_width, load_height;

        if (angle == 90 || angle == 270) {
                load_width = height;
                load_height = width;
        }
        else {
                load_width = width;
                load_height = height;
        }

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
        gdk_pixbuf_fill (pixbuf, 0x00000000);

        original = gdk_pixbuf_new_from_file_at_size (path, load_width, load_height, NULL);
        if (original) {
                if (angle != 0) {
                        g_autoptr(GdkPixbuf) pb = NULL;
                        pb = gdk_pixbuf_rotate_simple (original, angle);
                        g_set_object (&original, pb);
                }

                dest_width = gdk_pixbuf_get_width (original);
                dest_height = gdk_pixbuf_get_height (original);
                dest_x = (width - dest_width) / 2;
                dest_y = (height - dest_height) / 2;

                gdk_pixbuf_composite (original, pixbuf,
                                      dest_x, dest_y, dest_width, dest_height,
                                      dest_x, dest_y, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
        }

        return pixbuf;
}

/* load image rotated by angle to fill width x height while preserving
 * aspect ratio, cutting off overshoots
 */
GdkPixbuf *
load_pixbuf_fill_size (const char *path,
                       int         angle,
                       int         width,
                       int         height)
{
        g_autoptr(GdkPixbuf) original = NULL;
        int x, y;
        int load_width, load_height;

        if (angle == 90 || angle == 270) {
                load_width = height;
                load_height = width;
        }
        else {
                load_width = width;
                load_height = height;
        }

        original = gdk_pixbuf_new_from_file_at_scale (path, -1, load_height, TRUE, NULL);
        if (angle != 0) {
                g_autoptr(GdkPixbuf) pb = NULL;
                pb = gdk_pixbuf_rotate_simple (original, angle);
                g_set_object (&original, pb);
        }

        if (gdk_pixbuf_get_width (original) < width) {
                g_autoptr(GdkPixbuf) pb1 = NULL;
                pb1 = gdk_pixbuf_new_from_file_at_scale (path, load_width, -1, TRUE, NULL);
                g_set_object (&original, pb1);
                if (angle != 0) {
                        g_autoptr(GdkPixbuf) pb = NULL;
                        pb = gdk_pixbuf_rotate_simple (original, angle);
                        g_set_object (&original, pb);
                }
        }

        g_print ("fit '%s' in %dx%d: %dx%d\n", path, width, height,
                 gdk_pixbuf_get_width (original), gdk_pixbuf_get_height (original));

        g_assert (gdk_pixbuf_get_width (original) >= width &&
                  gdk_pixbuf_get_height (original) >= height);

        x = (gdk_pixbuf_get_width (original) - width) / 2;
        y = (gdk_pixbuf_get_height (original) - height) / 2;

        if (x == 0 && y == 0)
                return g_object_ref (original);
        else
                return gdk_pixbuf_new_subpixbuf (original, x, y, width, height);
}

const char *
get_user_data_dir (void)
{
        static char *dir = NULL;

        if (!dir) {
                dir = g_build_filename (g_get_user_data_dir (), "recipes", NULL);
                g_mkdir_with_parents (dir, 0755);
        }

        return (const char *)dir;
}

const char *
get_pkg_data_dir (void)
{
        return PKG_DATA_DIR;
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
