/* gr-chef.c:
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

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-chef.h"
#include "gr-utils.h"
#include "types.h"


struct _GrChef
{
        GObject parent_instance;

        char *id;
        char *name;
        char *fullname;
        char *description;
        char *image_path;

        char *translated_description;

        gboolean readonly;
};

G_DEFINE_TYPE (GrChef, gr_chef, G_TYPE_OBJECT)

enum {
        PROP_0,
        PROP_ID,
        PROP_NAME,
        PROP_FULLNAME,
        PROP_DESCRIPTION,
        PROP_IMAGE_PATH,
        PROP_READONLY,
        N_PROPS
};

static void
gr_chef_finalize (GObject *object)
{
        GrChef *self = GR_CHEF (object);

        g_free (self->id);
        g_free (self->name);
        g_free (self->fullname);
        g_free (self->description);
        g_free (self->image_path);
        g_free (self->translated_description);

        G_OBJECT_CLASS (gr_chef_parent_class)->finalize (object);
}

static void
gr_chef_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
        GrChef *self = GR_CHEF (object);

        switch (prop_id) {
        case PROP_ID:
                g_value_set_string (value, self->id);
                break;

        case PROP_NAME:
                g_value_set_string (value, self->name);
                break;

        case PROP_FULLNAME:
                g_value_set_string (value, self->fullname);
                break;

        case PROP_DESCRIPTION:
                g_value_set_string (value, self->description);
                break;

        case PROP_IMAGE_PATH:
                g_value_set_string (value, self->image_path);
                break;

        case PROP_READONLY:
                g_value_set_boolean (value, self->readonly);
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

        switch (prop_id) {
        case PROP_ID:
                g_free (self->id);
                self->id = g_value_dup_string (value);
                break;

        case PROP_NAME:
                g_free (self->name);
                self->name = g_value_dup_string (value);
                break;

        case PROP_FULLNAME:
                g_free (self->fullname);
                self->fullname = g_value_dup_string (value);
                break;

        case PROP_DESCRIPTION:
                g_clear_pointer (&self->description, g_free);
                g_clear_pointer (&self->translated_description, g_free);
                self->description = g_value_dup_string (value);
                if (self->description)
                        self->translated_description = translate_multiline_string (self->description);
                break;

        case PROP_IMAGE_PATH:
                g_free (self->image_path);
                self->image_path = g_value_dup_string (value);
                break;

        case PROP_READONLY:
                self->readonly = g_value_get_boolean (value);
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

        pspec = g_param_spec_string ("id", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_ID, pspec);

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

        pspec = g_param_spec_boolean ("readonly", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_READONLY, pspec);
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
gr_chef_get_id (GrChef *chef)
{
        return chef->id;
}

const char *
gr_chef_get_name (GrChef *chef)
{
        if (!chef->name && chef->fullname) {
                g_auto(GStrv) strv = NULL;
                strv = g_strsplit (chef->fullname, " ", 0);
                chef->name = g_strdup (strv[0]);
        }

        return chef->name;
}

const char *
gr_chef_get_fullname (GrChef *chef)
{
        return chef->fullname;
}

const char *
gr_chef_get_description (GrChef *chef)
{
        return chef->description;
}

const char *
gr_chef_get_translated_description (GrChef *chef)
{
        return chef->translated_description;
}

const char *
gr_chef_get_image (GrChef *chef)
{
        return chef->image_path;
}

gboolean
gr_chef_is_readonly (GrChef *chef)
{
        return chef->readonly;
}

