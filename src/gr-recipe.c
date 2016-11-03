/* gr-recipe.c
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
#include <glib/gi18n.h>
#include "gr-recipe.h"
#include "types.h"
#include <string.h>
#include <gtk/gtk.h>

typedef struct
{
        char *name;
        char *author;
        char *description;
	char *image_path;

        char *cuisine;
        char *category;
        char *prep_time;
        char *cook_time;
        int serves;
        char *ingredients;
        char *instructions;
        char *notes;
	GrDiets diets;

        char *cf_name;
        char *cf_description;
        char *cf_ingredients;
} GrRecipePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GrRecipe, gr_recipe, G_TYPE_OBJECT)

enum {
        PROP_0,
        PROP_NAME,
        PROP_AUTHOR,
        PROP_DESCRIPTION,
        PROP_IMAGE_PATH,
        PROP_CUISINE,
        PROP_CATEGORY,
        PROP_PREP_TIME,
        PROP_COOK_TIME,
        PROP_SERVES,
        PROP_INGREDIENTS,
        PROP_INSTRUCTIONS,
        PROP_NOTES,
        PROP_DIETS,
        N_PROPS
};

static void
gr_recipe_finalize (GObject *object)
{
        GrRecipe *self = GR_RECIPE (object);
        GrRecipePrivate *priv = gr_recipe_get_instance_private (self);

        g_free (priv->name);
        g_free (priv->author);
        g_free (priv->description);
        g_free (priv->cuisine);
        g_free (priv->category);
        g_free (priv->prep_time);
        g_free (priv->cook_time);
        g_free (priv->ingredients);
        g_free (priv->instructions);
        g_free (priv->notes);
        g_free (priv->image_path);

        g_free (priv->cf_name);
        g_free (priv->cf_description);
        g_free (priv->cf_ingredients);

        G_OBJECT_CLASS (gr_recipe_parent_class)->finalize (object);
}

static void
gr_recipe_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
        GrRecipe *self = GR_RECIPE (object);
        GrRecipePrivate *priv = gr_recipe_get_instance_private (self);

        switch (prop_id)
          {
          case PROP_AUTHOR:
                g_value_set_string (value, priv->author);
                break;

          case PROP_NAME:
                g_value_set_string (value, priv->name);
                break;

          case PROP_DESCRIPTION:
                g_value_set_string (value, priv->description);
                break;

          case PROP_IMAGE_PATH:
                g_value_set_string (value, priv->image_path);
                break;

          case PROP_CATEGORY:
                g_value_set_string (value, priv->category);
                break;

          case PROP_CUISINE:
                g_value_set_string (value, priv->cuisine);
                break;

          case PROP_PREP_TIME:
                g_value_set_string (value, priv->prep_time);
                break;

          case PROP_COOK_TIME:
                g_value_set_string (value, priv->cook_time);
                break;

          case PROP_SERVES:
                g_value_set_int (value, priv->serves);
                break;

          case PROP_INGREDIENTS:
                g_value_set_string (value, priv->ingredients);
                break;

          case PROP_INSTRUCTIONS:
                g_value_set_string (value, priv->instructions);
                break;

          case PROP_NOTES:
                g_value_set_string (value, priv->notes);
                break;

          case PROP_DIETS:
                g_value_set_flags (value, priv->diets);
                break;

          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_recipe_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
        GrRecipe *self = GR_RECIPE (object);
        GrRecipePrivate *priv = gr_recipe_get_instance_private (self);

        switch (prop_id)
          {
          case PROP_AUTHOR:
                  g_free (priv->author);
                  priv->author = g_value_dup_string (value);
                  break;

          case PROP_NAME:
                  g_free (priv->name);
                  priv->name = g_value_dup_string (value);
                  priv->cf_name = g_utf8_casefold (priv->name, -1);
                  break;

          case PROP_DESCRIPTION:
                  g_free (priv->description);
                  priv->description = g_value_dup_string (value);
                  priv->cf_description = g_utf8_casefold (priv->description, -1);
                  break;

          case PROP_IMAGE_PATH:
                  priv->image_path = g_value_dup_string (value);
                  break;

          case PROP_CATEGORY:
                  priv->category = g_value_dup_string (value);
                  break;

          case PROP_CUISINE:
                  priv->cuisine = g_value_dup_string (value);
                  break;

          case PROP_PREP_TIME:
                  priv->prep_time = g_value_dup_string (value);
                  break;

          case PROP_COOK_TIME:
                  priv->cook_time = g_value_dup_string (value);
                  break;

          case PROP_SERVES:
                  priv->serves = g_value_get_int (value);
                  break;

          case PROP_INGREDIENTS:
                  priv->ingredients = g_value_dup_string (value);
                  priv->cf_ingredients = g_utf8_casefold (priv->ingredients, -1);
                  break;

          case PROP_INSTRUCTIONS:
                  priv->instructions = g_value_dup_string (value);
                  break;

          case PROP_NOTES:
                  priv->notes = g_value_dup_string (value);
                  break;

          case PROP_DIETS:
                  priv->diets = g_value_get_flags (value);
                  break;

          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_recipe_class_init (GrRecipeClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_recipe_finalize;
        object_class->get_property = gr_recipe_get_property;
        object_class->set_property = gr_recipe_set_property;

        pspec = g_param_spec_string ("author", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_AUTHOR, pspec);

        pspec = g_param_spec_string ("name", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_NAME, pspec);

        pspec = g_param_spec_string ("description", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

        pspec = g_param_spec_string ("image-path", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_IMAGE_PATH, pspec);

        pspec = g_param_spec_string ("category", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_CATEGORY, pspec);

        pspec = g_param_spec_string ("cuisine", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_CUISINE, pspec);

        pspec = g_param_spec_string ("prep-time", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_PREP_TIME, pspec);

        pspec = g_param_spec_string ("cook-time", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_COOK_TIME, pspec);

        pspec = g_param_spec_int ("serves", NULL, NULL,
                                  0, 100, 0,
                                  G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_SERVES, pspec);

        pspec = g_param_spec_string ("ingredients", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INGREDIENTS, pspec);

        pspec = g_param_spec_string ("instructions", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INSTRUCTIONS, pspec);

        pspec = g_param_spec_string ("notes", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_NOTES, pspec);

        pspec = g_param_spec_flags ("diets", NULL, NULL,
                                    GR_TYPE_DIETS, 0,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_DIETS, pspec);
}

static void
gr_recipe_init (GrRecipe *self)
{
        GrRecipePrivate *priv = gr_recipe_get_instance_private (self);

        priv->author = g_strdup ("Anonymous");
        priv->name = g_strdup ("Plain Bagel");
        priv->description = g_strdup ("Just a plain bagel, not much to say");
        priv->category = g_strdup ("entree");
        priv->cuisine = g_strdup ("american");
        priv->prep_time = g_strdup ("no time at all");
        priv->cook_time = g_strdup ("no time at all");
        priv->serves = 1;
	priv->diets = 0;
}

GrRecipe *
gr_recipe_new (void)
{
        return g_object_new (GR_TYPE_RECIPE, NULL);
}

/* term is assumed to be g_utf8_casefold'ed */
gboolean
gr_recipe_matches (GrRecipe *self, const char *term)
{
        GrRecipePrivate *priv = gr_recipe_get_instance_private (self);
	g_auto(GStrv) terms = NULL;
	int i;

	terms = g_strsplit (term, " ", -1);

	for (i = 0; terms[i]; i++) {
		if (g_str_has_prefix (term, "i:")) {
	        	if (!priv->cf_ingredients || strstr (priv->cf_ingredients, term + 2) == NULL)
        		        return FALSE;
			continue;
		}

        	if (priv->cf_name && strstr (priv->cf_name, term) == NULL)
                	continue;

        	if (priv->cf_description && strstr (priv->cf_description, term) == NULL)
                	continue;

        	if (priv->cf_ingredients && strstr (priv->cf_ingredients, term) == NULL)
                	continue;

		return FALSE;
	}

        return TRUE;
}
