/* gr-ingredients-list.c:
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

#include <glib.h>
#include <glib/gi18n.h>

#include "gr-ingredients-list.h"
#include "gr-ingredient.h"
#include "gr-number.h"
#include "gr-unit.h"


/* Parsing ingredients is tricky business. We operate under the following
 * assumptions:
 * - Each line is one ingredient
 * - The first word is the amount
 * - the amount can be written as 'a' or a decimal number, or a fraction
 * - The second word may be a unit, or it may be part of the name
 * - All remaining words make up the name of the integredient
 */

typedef struct
{
        GrNumber amount;
        gchar *unit;
        gchar *name;
        gchar *segment;
} Ingredient;

static void
ingredient_free (Ingredient *ing)
{
        g_free (ing->name);
        g_free (ing->unit);
        g_free (ing);
}

struct _GrIngredientsList
{
        GObject parent_instance;

        GList *ingredients;
        GHashTable *segments;
};

G_DEFINE_TYPE (GrIngredientsList, gr_ingredients_list, G_TYPE_OBJECT)

static void
skip_whitespace (char **line)
{
        while (g_ascii_isspace (**line))
                (*line)++;
}

static gboolean
gr_ingredients_list_add_one (GrIngredientsList  *ingredients,
                             char               *line,
                             char               *segment,
                             GError            **error)
{
        Ingredient *ing;
        const char *s;

        if (line[0] == '\0')
                return TRUE;

        ing = g_new0 (Ingredient, 1);

        line = g_strstrip (line);

        if (!gr_number_parse (&ing->amount, &line, error)) {
                ing->amount.fraction = TRUE;
                ing->amount.num = 0;
                ing->amount.denom = 0;
                ing->amount.value = 0.0;
        }

        skip_whitespace (&line);

        s = gr_unit_parse (&line, NULL);
        ing->unit = g_strdup (s);
        if (s)
                skip_whitespace (&line);

        s = gr_ingredient_find (line);
        if (s)
                ing->name = g_strdup (s);
        else
                ing->name = g_strdup (line);

        ing->segment = g_strdup (segment);

        ingredients->ingredients = g_list_append (ingredients->ingredients, ing);

        return TRUE;
}

static gboolean
gr_ingredients_list_populate (GrIngredientsList  *ingredients,
                              const char         *text,
                              GError            **error)
{
        g_auto(GStrv) lines = NULL;
        int i;
        g_autofree char *segment = NULL;

        lines = g_strsplit (text, "\n", 0);

        for (i = 0; lines[i]; i++) {
                char *p;
                p = strrchr (lines[i], '\t');
                if (p) {
                        segment = g_strdup (p + 1);
                        *p = 0;
                }
                else
                        segment = NULL;

                if (segment)
                        g_hash_table_add (ingredients->segments, g_strdup (segment));
                if (!gr_ingredients_list_add_one (ingredients, lines[i],
                                                  (char *)(segment ? segment : ""), error)) {
                        g_message ("Failed to parse ingredients line '%s'", lines[i]);
                }
        }

        if (g_hash_table_size (ingredients->segments) == 0)
                g_hash_table_add (ingredients->segments, g_strdup (""));

        return TRUE;

}

static gboolean
gr_ingredients_list_populate_new_format (GrIngredientsList  *ingredients,
                                         const char         *text,
                                         GError            **error)
{
        g_auto(GStrv) lines = NULL;
        int i;

        lines = g_strsplit (text, "\n", 0);

        for (i = 0; lines[i]; i++) {
                g_auto(GStrv) fields = NULL;
                char *amount;
                char *unit;
                char *ingredient;
                char *segment;
                const char *u;
                Ingredient *ing;
                g_autoptr(GError) local_error = NULL;

                fields = g_strsplit (lines[i], "\t", 0);
                if (g_strv_length (fields) != 4) {
                        g_warning ("wrong number of fields, ignoring line '%s'", lines[i]);
                        continue;
                }

                amount = fields[0];
                unit = fields[1];
                ingredient = fields[2];
                segment = fields[3];

                ing = g_new0 (Ingredient, 1);
                gr_number_set_fraction (&ing->amount, 0, 0);
                if (amount[0] != '\0' &&
                    !gr_number_parse (&ing->amount, &amount, &local_error)) {
                        g_message ("failed to parse amount '%s': %s", amount, local_error->message);
                        g_free (ing);
                        continue;
                }

                u = "";
                if (unit[0] != '\0' &&
                    ((u = gr_unit_parse (&unit, &local_error)) == NULL)) {
                        g_message ("failed to unit amount in '%s': %s", unit, local_error->message);
                        g_free (ing);
                        continue;
                }

                ing->unit = g_strdup (u);
                ing->name = g_strdup (ingredient);
                ing->segment = g_strdup (segment);

                g_hash_table_add (ingredients->segments, g_strdup (segment));
                ingredients->ingredients = g_list_append (ingredients->ingredients, ing);
        }

        return TRUE;
}

static void
ingredients_list_finalize (GObject *object)
{
        GrIngredientsList *self = GR_INGREDIENTS_LIST (object);

        g_list_free_full (self->ingredients, (GDestroyNotify)ingredient_free);
        g_hash_table_unref (self->segments);

        G_OBJECT_CLASS (gr_ingredients_list_parent_class)->finalize (object);
}

static void
gr_ingredients_list_init (GrIngredientsList *ingredients)
{
        ingredients->segments = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
gr_ingredients_list_class_init (GrIngredientsListClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = ingredients_list_finalize;
}

static gboolean
is_new_format (const char *text)
{
        int tabs;
        const char *p;

        // In the new format, we separate fields by tabs, so there will be >1 tab per line
        tabs = 0;
        p = text;
        while (*p != '\0' && *p != '\n') {
                if (*p == '\t')
                        tabs++;
                p++;
        }

        return tabs > 1;
}

GrIngredientsList *
gr_ingredients_list_new (const char *text)
{
        GrIngredientsList *ingredients;

        ingredients = g_object_new (GR_TYPE_INGREDIENTS_LIST, NULL);
        if (is_new_format (text))
                gr_ingredients_list_populate_new_format (ingredients, text, NULL);
        else
                gr_ingredients_list_populate (ingredients, text, NULL);

        return ingredients;
}

gboolean
gr_ingredients_list_validate (const char  *text,
                              GError     **error)
{
        g_autoptr(GrIngredientsList) ingredients = NULL;

        ingredients = g_object_new (GR_TYPE_INGREDIENTS_LIST, NULL);

        return gr_ingredients_list_populate (ingredients, text, error);
}

static void
ingredient_scale_unit (Ingredient *ing, int num, int denom, GString *s)
{
        g_autofree char *scaled = NULL;
        GrNumber *snum;

        snum = gr_number_new_fraction (num, denom);
        gr_number_multiply (&ing->amount, snum, snum);
        scaled = gr_number_format (snum);
        g_free (snum);

        g_string_append (s, scaled);
        if (ing->unit) {
                g_string_append (s, " ");
                g_string_append (s, ing->unit);
        }
}

static void
ingredient_scale (Ingredient *ing, int num, int denom, GString *s)
{
        ingredient_scale_unit (ing, num, denom, s);
        g_string_append (s, " ");
        g_string_append (s, ing->name);
        g_string_append (s, "\n");
}

char *
gr_ingredients_list_scale (GrIngredientsList *ingredients,
                           int                num,
                           int                denom)
{
        GString *s;
        GList *l;

        s = g_string_new ("");

        for (l = ingredients->ingredients; l; l = l->next) {
                Ingredient *ing = (Ingredient *)l->data;

                ingredient_scale (ing, num, denom, s);
        }

        return g_string_free (s, FALSE);
}

char **
gr_ingredients_list_get_segments (GrIngredientsList *ingredients)
{
        return (char **)g_hash_table_get_keys_as_array (ingredients->segments, NULL);
}

char **
gr_ingredients_list_get_ingredients (GrIngredientsList *ingredients,
                                     const char        *segment)
{
        char **ret;
        int i;
        GList *l;

        ret = g_new0 (char *, g_list_length (ingredients->ingredients) + 1);
        for (i = 0, l = ingredients->ingredients; l; l = l->next) {
                Ingredient *ing = (Ingredient *)l->data;
                if (g_strcmp0 (segment, ing->segment) == 0)
                        ret[i++] = g_strdup (ing->name);
        }

        return ret;
}

char *
gr_ingredients_list_scale_unit (GrIngredientsList *ingredients,
                                const char        *segment,
                                const char        *name,
                                int                num,
                                int                denom)
{
        GList *l;

        for (l = ingredients->ingredients; l; l = l->next) {
                Ingredient *ing = (Ingredient *)l->data;

                if (g_strcmp0 (segment, ing->segment) == 0 &&
                    g_strcmp0 (name, ing->name) == 0) {
                        GString *s;

                        s = g_string_new ("");
                        ingredient_scale_unit (ing, num, denom, s);

                        return g_string_free (s, FALSE);
                }
        }

        return NULL;
}
