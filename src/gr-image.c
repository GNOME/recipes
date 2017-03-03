/* gr-image.c:
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

#include "config.h"

#include "gr-image.h"

struct _GrImage
{
        GObject parent_instance;
        char *path;
};

G_DEFINE_TYPE (GrImage, gr_image, G_TYPE_OBJECT)

static void
gr_image_finalize (GObject *object)
{
        GrImage *image = GR_IMAGE (object);

        g_free (image->path);

        G_OBJECT_CLASS (gr_image_parent_class)->finalize (object);
}

static void
gr_image_class_init (GrImageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_image_finalize;
}

static void
gr_image_init (GrImage *image)
{
}

GrImage *
gr_image_new (const char *path)
{
        GrImage *image;

        image = g_object_new (GR_TYPE_IMAGE, NULL);
        gr_image_set_path (image, path);

        return image;
}

void
gr_image_set_path (GrImage    *image,
                   const char *path)
{
        g_free (image->path);
        image->path = g_strdup (path);
}

const char *
gr_image_get_path (GrImage *image)
{
        return image->path;
}

GPtrArray *
gr_image_array_new (void)
{
        return g_ptr_array_new_with_free_func (g_object_unref);
}
