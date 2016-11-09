/* gr-chef.c
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

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-chef.h"
#include "types.h"


typedef struct
{
        char *name;
        char *fullname;
        char *description;
        char *image_path;
} GrChefPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GrChef, gr_chef, G_TYPE_OBJECT)

enum {
        PROP_0,
        PROP_NAME,
        PROP_FULLNAME,
        PROP_DESCRIPTION,
        PROP_IMAGE_PATH,
        N_PROPS
};

static void
gr_chef_finalize (GObject *object)
{
        GrChef *self = GR_CHEF (object);
        GrChefPrivate *priv = gr_chef_get_instance_private (self);

        g_free (priv->name);
        g_free (priv->fullname);
        g_free (priv->description);
        g_free (priv->image_path);

        G_OBJECT_CLASS (gr_chef_parent_class)->finalize (object);
}

static void
gr_chef_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
        GrChef *self = GR_CHEF (object);
        GrChefPrivate *priv = gr_chef_get_instance_private (self);

        switch (prop_id) {
        case PROP_NAME:
                g_value_set_string (value, priv->name);
                break;

        case PROP_FULLNAME:
                g_value_set_string (value, priv->fullname);
                break;

        case PROP_DESCRIPTION:
                g_value_set_string (value, priv->description);
                break;

        case PROP_IMAGE_PATH:
                g_value_set_string (value, priv->image_path);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gr_chef_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
        GrChef *self = GR_CHEF (object);
        GrChefPrivate *priv = gr_chef_get_instance_private (self);

        switch (prop_id) {
        case PROP_NAME:
                g_free (priv->name);
                priv->name = g_value_dup_string (value);
                break;

        case PROP_FULLNAME:
                g_free (priv->fullname);
                priv->fullname = g_value_dup_string (value);
                break;

        case PROP_DESCRIPTION:
                g_free (priv->description);
                priv->description = g_value_dup_string (value);
                break;

        case PROP_IMAGE_PATH:
                priv->image_path = g_value_dup_string (value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gr_chef_class_init (GrChefClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_chef_finalize;
        object_class->get_property = gr_chef_get_property;
        object_class->set_property = gr_chef_set_property;

        pspec = g_param_spec_string ("name", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_NAME, pspec);

        pspec = g_param_spec_string ("fullname", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_FULLNAME, pspec);

        pspec = g_param_spec_string ("description", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

        pspec = g_param_spec_string ("image-path", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_IMAGE_PATH, pspec);
}

static void
gr_chef_init (GrChef *self)
{
}

GrChef *
gr_chef_new (void)
{
        return g_object_new (GR_TYPE_CHEF, NULL);
}

const char *
gr_chef_get_name (GrChef *chef)
{
        GrChefPrivate *priv = gr_chef_get_instance_private (chef);

        return priv->name;
}

const char *
gr_chef_get_fullname (GrChef *chef)
{
        GrChefPrivate *priv = gr_chef_get_instance_private (chef);

        return priv->fullname;
}

const char *
gr_chef_get_description (GrChef *chef)
{
        GrChefPrivate *priv = gr_chef_get_instance_private (chef);

        return priv->description;
}

const char *
gr_chef_get_image (GrChef *chef)
{
        GrChefPrivate *priv = gr_chef_get_instance_private (chef);

        return priv->image_path;
}
