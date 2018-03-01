/* gr-recipe.c:
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

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-recipe.h"
#include "gr-recipe-store.h"
#include "gr-image.h"
#include "gr-utils.h"
#include "types.h"

/* Note:
 *
 *  If any fields are added to a recipe, there are several places
 *  that need to be kept in sync:
 *  - save_recipes() in gr-recipe-store.c
 *  - load_recipes() in gr-recipe-store.c
 *  - the GrRecipeExporter code
 *  - the GrRecipeImporter code
 */

struct _GrRecipe
{
        GObject parent_instance;

        char *id;
        char *name;
        char *author;
        char *description;
        GPtrArray *images;
        int default_image;

        char *cuisine;
        char *season;
        char *category;
        char *prep_time;
        char *cook_time;
        char *ingredients;
        char *instructions;
        char *notes;
        GrDiets diets;
        GDateTime *ctime;
        GDateTime *mtime;

        char *cf_name;
        char *cf_description;
        char *cf_ingredients;

        gboolean garlic;
        int spiciness;

        gboolean readonly;
        gboolean contributed;

        char *translated_name;
        char *translated_description;
        char *translated_instructions;
        char *translated_notes;

        double yield;
        char *yield_unit;
};

G_DEFINE_TYPE (GrRecipe, gr_recipe, G_TYPE_OBJECT)

enum {
        PROP_0,
        PROP_ID,
        PROP_NAME,
        PROP_AUTHOR,
        PROP_DESCRIPTION,
        PROP_IMAGES,
        PROP_DEFAULT_IMAGE,
        PROP_CUISINE,
        PROP_SEASON,
        PROP_CATEGORY,
        PROP_PREP_TIME,
        PROP_COOK_TIME,
        PROP_YIELD,
        PROP_YIELD_UNIT,
        PROP_INGREDIENTS,
        PROP_INSTRUCTIONS,
        PROP_SPICINESS,
        PROP_NOTES,
        PROP_DIETS,
        PROP_CTIME,
        PROP_MTIME,
        PROP_READONLY,
        PROP_CONTRIBUTED,
        N_PROPS
};

static char * gr_recipe_get_chef_fullname (GrRecipe *self);

static void
gr_recipe_finalize (GObject *object)
{
        GrRecipe *self = GR_RECIPE (object);

        g_free (self->id);
        g_free (self->name);
        g_free (self->author);
        g_free (self->description);
        g_free (self->cuisine);
        g_free (self->season);
        g_free (self->category);
        g_free (self->prep_time);
        g_free (self->cook_time);
        g_free (self->ingredients);
        g_free (self->instructions);
        g_free (self->notes);
        g_ptr_array_unref (self->images);

        g_free (self->cf_name);
        g_free (self->cf_description);
        g_free (self->cf_ingredients);
        g_date_time_unref (self->mtime);
        g_date_time_unref (self->ctime);

        g_free (self->translated_name);
        g_free (self->translated_description);
        g_free (self->translated_instructions);
        g_free (self->translated_notes);

        g_free (self->yield_unit);

        G_OBJECT_CLASS (gr_recipe_parent_class)->finalize (object);
}

static void
gr_recipe_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
        GrRecipe *self = GR_RECIPE (object);

        switch (prop_id) {
        case PROP_ID:
                g_value_set_string (value, self->id);
                break;

        case PROP_AUTHOR:
                g_value_set_string (value, self->author);
                break;

        case PROP_NAME:
                g_value_set_string (value, self->name);
                break;

        case PROP_DESCRIPTION:
                g_value_set_string (value, self->description);
                break;

        case PROP_IMAGES:
                g_value_set_boxed (value, self->images);
                break;

        case PROP_DEFAULT_IMAGE:
                g_value_set_int (value, self->default_image);
                break;

        case PROP_CATEGORY:
                g_value_set_string (value, self->category);
                break;

        case PROP_CUISINE:
                g_value_set_string (value, self->cuisine);
                break;

        case PROP_SEASON:
                g_value_set_string (value, self->season);
                break;

        case PROP_PREP_TIME:
                g_value_set_string (value, self->prep_time);
                break;

        case PROP_COOK_TIME:
                g_value_set_string (value, self->cook_time);
                break;

        case PROP_SPICINESS:
                g_value_set_int (value, self->spiciness);
                break;

        case PROP_INGREDIENTS:
                g_value_set_string (value, self->ingredients);
                break;

        case PROP_INSTRUCTIONS:
                g_value_set_string (value, self->instructions);
                break;

        case PROP_NOTES:
                g_value_set_string (value, self->notes);
                break;

        case PROP_DIETS:
                g_value_set_flags (value, self->diets);
                break;

        case PROP_CTIME:
                g_value_set_boxed (value, self->ctime);
                break;

        case PROP_MTIME:
                g_value_set_boxed (value, self->mtime);
                break;

        case PROP_READONLY:
                g_value_set_boolean (value, self->readonly);
                break;

        case PROP_CONTRIBUTED:
                g_value_set_boolean (value, self->contributed);
                break;

        case PROP_YIELD:
                g_value_set_double (value, self->yield);
                break;

        case PROP_YIELD_UNIT:
                g_value_set_string (value, self->yield_unit);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static char *
gr_recipe_get_chef_fullname (GrRecipe *self)
{
	GrRecipeStore *store;
	g_autoptr(GrChef) chef = NULL;

	store = gr_recipe_store_get ();
	chef = gr_recipe_store_get_chef (store, self->author);
	if (chef) {
		const char *fullname = gr_chef_get_fullname (chef);

                if (fullname)
			return g_utf8_casefold (fullname, -1);
	}

        return NULL;
}

static void
gr_recipe_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
        GrRecipe *self = GR_RECIPE (object);

        if (self->readonly && prop_id != PROP_NOTES) {
                g_error ("trying to modify a readonly recipe");
                return;
        }

        switch (prop_id) {
        case PROP_ID:
                g_free (self->id);
                self->id = g_value_dup_string (value);
                break;

        case PROP_AUTHOR:
                g_free (self->author);
                self->author = g_value_dup_string (value);
                break;

        case PROP_NAME:
                g_clear_pointer (&self->name, g_free);
                g_clear_pointer (&self->translated_name, g_free);
                g_clear_pointer (&self->cf_name, g_free);
                self->name = g_value_dup_string (value);
                if (self->name) {
                        self->translated_name = translate_multiline_string (self->name);
                        self->cf_name = g_utf8_casefold (self->translated_name, -1);
                }
                break;

        case PROP_DESCRIPTION:
                g_clear_pointer (&self->description, g_free);
                g_clear_pointer (&self->translated_description, g_free);
                g_clear_pointer (&self->cf_description, g_free);
                self->description = g_value_dup_string (value);
                if (self->description) {
                        self->translated_description = translate_multiline_string (self->description);
                        self->cf_description = g_utf8_casefold (self->translated_description, -1);
                }
                break;

        case PROP_IMAGES:
                if (self->images)
                        g_ptr_array_unref (self->images);
                self->images = g_ptr_array_ref ((GPtrArray *) g_value_get_boxed (value));
                break;

        case PROP_DEFAULT_IMAGE:
                self->default_image = g_value_get_int (value);
                break;

        case PROP_CATEGORY:
                g_free (self->category);
                self->category = g_value_dup_string (value);
                break;

        case PROP_CUISINE:
                g_free (self->cuisine);
                self->cuisine = g_value_dup_string (value);
                break;

        case PROP_SEASON:
                g_free (self->season);
                self->season = g_value_dup_string (value);
                break;

        case PROP_PREP_TIME:
                g_free (self->prep_time);
                self->prep_time = g_value_dup_string (value);
                break;

        case PROP_COOK_TIME:
                g_free (self->cook_time);
                self->cook_time = g_value_dup_string (value);
                break;

        case PROP_SPICINESS:
                self->spiciness = g_value_get_int (value);
                break;

        case PROP_INGREDIENTS:
                g_clear_pointer (&self->ingredients, g_free);
                g_clear_pointer (&self->cf_ingredients, g_free);
                self->garlic = FALSE;

                self->ingredients = g_value_dup_string (value);
                if (self->ingredients) {
                        g_autofree char *cf_garlic = NULL;
                        self->cf_ingredients = g_utf8_casefold (self->ingredients, -1);
                        cf_garlic = g_utf8_casefold ("Garlic", -1);
                        self->garlic = (strstr (self->cf_ingredients, cf_garlic) != NULL);
                }
                break;

        case PROP_INSTRUCTIONS:
                g_clear_pointer (&self->instructions, g_free);
                g_clear_pointer (&self->translated_instructions, g_free);
                self->instructions = g_value_dup_string (value);
                if (self->instructions)
                        self->translated_instructions = translate_multiline_string (self->instructions);
                break;

        case PROP_NOTES:
                g_clear_pointer (&self->notes, g_free);
                g_clear_pointer (&self->translated_notes, g_free);
                self->notes = g_value_dup_string (value);
                if (self->notes)
                        self->translated_notes = translate_multiline_string (self->notes);
                break;

        case PROP_DIETS:
                self->diets = g_value_get_flags (value);
                break;

        case PROP_CTIME:
                g_date_time_unref (self->ctime);
                self->ctime = g_value_get_boxed (value);
                g_date_time_ref (self->ctime);
                break;

        case PROP_MTIME:
                g_date_time_unref (self->mtime);
                self->mtime = g_value_get_boxed (value);
                g_date_time_ref (self->mtime);
                break;

        case PROP_READONLY:
                self->readonly = g_value_get_boolean (value);
                break;

        case PROP_CONTRIBUTED:
                self->contributed = g_value_get_boolean (value);
                break;

        case PROP_YIELD:
                self->yield = g_value_get_double (value);
                break;

        case PROP_YIELD_UNIT:
                g_free (self->yield_unit);
                self->yield_unit = g_value_dup_string (value);
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

        pspec = g_param_spec_string ("id", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_ID, pspec);

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

        pspec = g_param_spec_boxed ("images", NULL, NULL,
                                    G_TYPE_PTR_ARRAY,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_IMAGES, pspec);

        pspec = g_param_spec_int ("default-image", NULL, NULL,
                                  0, G_MAXINT, 0,
                                  G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_DEFAULT_IMAGE, pspec);

        pspec = g_param_spec_string ("category", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_CATEGORY, pspec);

        pspec = g_param_spec_string ("cuisine", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_CUISINE, pspec);

        pspec = g_param_spec_string ("season", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_SEASON, pspec);

        pspec = g_param_spec_int ("spiciness", NULL, NULL,
                                  0, 100, 0,
                                  G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_SPICINESS, pspec);

        pspec = g_param_spec_string ("prep-time", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_PREP_TIME, pspec);

        pspec = g_param_spec_string ("cook-time", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_COOK_TIME, pspec);

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

        pspec = g_param_spec_boxed ("ctime", NULL, NULL,
                                    G_TYPE_DATE_TIME,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_CTIME, pspec);

        pspec = g_param_spec_boxed ("mtime", NULL, NULL,
                                    G_TYPE_DATE_TIME,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_MTIME, pspec);

        pspec = g_param_spec_boolean ("readonly", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_READONLY, pspec);

        pspec = g_param_spec_boolean ("contributed", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_CONTRIBUTED, pspec);

        pspec = g_param_spec_double ("yield", NULL, NULL,
                                     0.0, G_MAXDOUBLE, 0.0,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_YIELD, pspec);

        pspec = g_param_spec_string ("yield-unit", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_YIELD_UNIT, pspec);
}

static void
gr_recipe_init (GrRecipe *self)
{
        self->ctime = g_date_time_new_now_utc ();
        self->mtime = g_date_time_new_now_utc ();
        self->images = gr_image_array_new ();
        self->yield = 0.0;
}

GrRecipe *
gr_recipe_new (void)
{
        return g_object_new (GR_TYPE_RECIPE, NULL);
}

const char *
gr_recipe_get_id (GrRecipe *recipe)
{
        return recipe->id;
}

const char *
gr_recipe_get_name (GrRecipe *recipe)
{
        return recipe->name;
}

const char *
gr_recipe_get_translated_name (GrRecipe *recipe)
{
        return recipe->translated_name;
}

const char *
gr_recipe_get_author (GrRecipe *recipe)
{
        return recipe->author;
}

const char *
gr_recipe_get_description (GrRecipe *recipe)
{
        return recipe->description;
}

const char *
gr_recipe_get_translated_description (GrRecipe *recipe)
{
        return recipe->translated_description;
}

const char *
gr_recipe_get_translated_notes (GrRecipe *recipe)
{
        return recipe->translated_notes;
}

const char *
gr_recipe_get_cuisine (GrRecipe *recipe)
{
        return recipe->cuisine;
}

const char *
gr_recipe_get_season (GrRecipe *recipe)
{
        return recipe->season;
}

const char *
gr_recipe_get_category (GrRecipe *recipe)
{
        return recipe->category;
}

const char *
gr_recipe_get_prep_time (GrRecipe *recipe)
{
        return recipe->prep_time;
}

const char *
gr_recipe_get_cook_time (GrRecipe *recipe)
{
        return recipe->cook_time;
}

GrDiets
gr_recipe_get_diets (GrRecipe *recipe)
{
        return recipe->diets;
}

const char *
gr_recipe_get_ingredients (GrRecipe *recipe)
{
        return recipe->ingredients;
}

const char *
gr_recipe_get_instructions (GrRecipe *recipe)
{
        return recipe->instructions;
}

const char *
gr_recipe_get_translated_instructions (GrRecipe *recipe)
{
        return recipe->translated_instructions;
}

const char *
gr_recipe_get_notes (GrRecipe *recipe)
{
        return recipe->notes;
}

gboolean
gr_recipe_contains_garlic (GrRecipe *recipe)
{
        return recipe->garlic;
}

gboolean
gr_recipe_contains_dairy (GrRecipe *recipe)
{
        return (recipe->diets & GR_DIET_MILK_FREE) == 0 &&
               (recipe->diets & GR_DIET_VEGAN) == 0;
}

gboolean
gr_recipe_contains_gluten (GrRecipe *recipe)
{
        return (recipe->diets & GR_DIET_GLUTEN_FREE) == 0;
}

int
gr_recipe_get_spiciness (GrRecipe *recipe)
{
        return recipe->spiciness;
}

GDateTime *
gr_recipe_get_ctime (GrRecipe *recipe)
{
        return recipe->ctime;
}

GDateTime *
gr_recipe_get_mtime (GrRecipe *recipe)
{
        return recipe->mtime;
}

int
gr_recipe_get_default_image (GrRecipe *recipe)
{
        return recipe->default_image;
}

gboolean
gr_recipe_is_readonly (GrRecipe *recipe)
{
        return recipe->readonly;
}

gboolean
gr_recipe_is_contributed (GrRecipe *recipe)
{
        return recipe->contributed;
}

GPtrArray *
gr_recipe_get_images (GrRecipe *recipe)
{
        return recipe->images;
}

double
gr_recipe_get_yield (GrRecipe *recipe)
{
        return recipe->yield;
}

const char *
gr_recipe_get_yield_unit (GrRecipe *recipe)
{
        return recipe->yield_unit;
}

/* terms are assumed to be g_utf8_casefold'ed where appropriate */
gboolean
gr_recipe_matches (GrRecipe    *recipe,
                   const char **terms)
{
        int i;
        g_autofree char *cf_fullname = NULL;

        cf_fullname = gr_recipe_get_chef_fullname (recipe);

        for (i = 0; terms[i]; i++) {
                if (g_str_has_prefix (terms[i], "i+:")) {
                        if (!recipe->ingredients || strstr (recipe->ingredients, terms[i] + 3) == NULL) {
                                return FALSE;
                        }
                        continue;
                }
                else if (g_str_has_prefix (terms[i], "i-:")) {
                        if (recipe->ingredients && strstr (recipe->ingredients, terms[i] + 3) != NULL) {
                                return FALSE;
                        }
                        continue;
                }
                else if (g_str_has_prefix (terms[i], "by:")) {
                        if (!recipe->author || strcmp (recipe->author, terms[i] + 3) != 0) {
                                return FALSE;
                        }
                        continue;
                }
                else if (g_str_has_prefix (terms[i], "se:")) {
                        if (!recipe->season || strcmp (recipe->season , terms[i] + 3) != 0) {
                                return FALSE;
                        }
                        continue;
                }
                else if (g_str_has_prefix (terms[i], "me:")) {
                        if (!recipe->category || strstr (recipe->category, terms[i] + 3) == NULL) {
                                return FALSE;
                        }
                        continue;
                }
                else if (g_str_has_prefix (terms[i], "di:")) {
                        struct { GrDiets diet; const char *term; } diets[] = {
                                { GR_DIET_GLUTEN_FREE, "gluten-free" },
                                { GR_DIET_NUT_FREE,    "nut-free" },
                                { GR_DIET_VEGAN,       "vegan" },
                                { GR_DIET_VEGETARIAN,  "vegetarian" },
                                { GR_DIET_MILK_FREE,   "milk-free" },
                                { GR_DIET_HALAL,       "halal" },
                                { 0, NULL }
                        };
                        GrDiets d = 0;
                        int j;

                        for (j = 0; diets[j].term; j++) {
                                if (strstr (diets[j].term, terms[i] + 3)) {
                                        d |= diets[j].diet;
                                }
                        }

                        if (!(recipe->diets & d))
                                return FALSE;

                        continue;
                }
                else if (g_str_has_prefix (terms[i], "na:")) {
                        if (recipe->cf_name && strstr (recipe->cf_name, terms[i] + 3) == NULL)
                                return FALSE;

                        continue;

                }
                else if (g_str_has_prefix (terms[i], "s+:")) {
                        int level;

                        level = atoi (terms[i] + 3);
                        if (recipe->spiciness < level)
                                return FALSE;

                        continue;
                }
                else if (g_str_has_prefix (terms[i], "s-:")) {
                        int level;

                        level = atoi (terms[i] + 3);
                        if (recipe->spiciness > level)
                                return FALSE;

                        continue;
                }

                if (recipe->cf_name && strstr (recipe->cf_name, terms[i]) != NULL)
                        continue;

                if (recipe->cf_description && strstr (recipe->cf_description, terms[i]) != NULL)
                        continue;

                if (recipe->cf_ingredients && strstr (recipe->cf_ingredients, terms[i]) != NULL)
                        continue;

                if (cf_fullname && strstr (cf_fullname, terms[i]) != NULL)
                        continue;

                return FALSE;
        }

        return TRUE;
}
