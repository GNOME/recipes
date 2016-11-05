/* gr-ingredients-list.c
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

#include <glib.h>
#include <glib/gi18n.h>

#include "gr-ingredients-list.h"
#include "gr-ingredient.h"


/* Parsing ingredients is tricky business. We operate under the following
 * assumptions:
 * - Each line is one ingredient
 * - The first word is the amount
 * - the amount can be written as 'a' or a decimal number, or a fraction
 * - The second word may be a unit, or it may be part of the name
 * - All remaining words make up the name of the integredient
 */

typedef struct {
        gboolean fraction;
        int num, denom;
        gdouble value;
        gchar *unit;
        gchar *name;
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
};

G_DEFINE_TYPE (GrIngredientsList, gr_ingredients_list, G_TYPE_OBJECT)

static void
skip_whitespace (char **line)
{
        while (g_ascii_isspace (**line))
                (*line)++;
}

static gboolean
parse_as_fraction (Ingredient  *ing,
                   char       **string,
                   GError     **error)
{
        guint64 num, denom;
        char *end = NULL;

        num = g_ascii_strtoull (*string, &end, 10);
        if (end[0] != '/') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a fraction"), string);
                return FALSE;
        }
        *string = end + 1;

        denom = g_ascii_strtoull (*string, &end, 10);
        if (end != NULL && end[0] != '\0' && !g_ascii_isspace (end[0])) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a fraction"), string);
                return FALSE;
        }
        *string = end;

        ing->fraction = TRUE;
        ing->num = num;
        ing->denom = denom;

        return TRUE;
}

typedef struct {
  const char *string;
  int value;
} NumberForm;

static NumberForm numberforms[] = {
  { "a dozen", 12 },
  { "a",        1 },
  { "an",       1 },
  { "one",      1 },
  { "two",      2 },
  { "three",    3 },
  { "four",     4 },
  { "five",     5 },
  { "six",      6 },
  { "seven",    7 },
  { "eight",    8 },
  { "nine",     9 },
  { "ten",     10 },
  { "eleven",  11 },
  { "twelve",  12 }
};

static gboolean
parse_as_number (Ingredient  *ing,
                 char       **string,
                 GError     **error)
{
        double value;
        gint64 ival;
        char *end = NULL;
        int i;

        for (i = 0; i < G_N_ELEMENTS (numberforms); i++) {
                if (g_str_has_prefix (*string, numberforms[i].string) &&
                    g_ascii_isspace ((*string)[strlen (numberforms[i].string)])) {
                        ing->fraction = TRUE;
                        ing->num = numberforms[i].value;
                        ing->denom = 1;
                        *string += strlen (numberforms[i].string);
                        return TRUE;
                }
        }

        ival = g_ascii_strtoll (*string, &end, 10);
        if (end == NULL || end[0] == '\0' || g_ascii_isspace (end[0])) {
                ing->fraction = TRUE;
                ing->num = ival;
                ing->denom = 1;
                *string = end;
                return TRUE;
        }

        if (strchr (*string, '/'))
                return parse_as_fraction (ing, string, error);

        value = g_strtod (*string, &end);

        if (end != NULL && end[0] != '\0' && !g_ascii_isspace (end[0])) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a number"), *string);
                return FALSE;
        }

        *string = end;
        ing->fraction = FALSE;
        ing->value = value;

        return TRUE;
}

typedef struct {
  const char *unit;
  const char *names[4];
} Unit;

static Unit units[] = {
  { "g",  { "g", "gram", "grams", NULL } },
  { "kg", { "kg", "kilogram", "kilograms", NULL } },
  { "l",  { "l", "liter", "liters", NULL } },
  { "lb", { "lb", "pound", "pounds", NULL } },
  { "box", { "box", "boxes", NULL, NULL } },
};

static gboolean
parse_as_unit (Ingredient  *ing,
               char       **string,
               GError     **error)
{
        int i, j;

        for (i = 0; i < G_N_ELEMENTS (units); i++) {
                for (j = 0; units[i].names[j]; j++) {
                        if (g_str_has_prefix (*string, units[i].names[j]) &&
                            g_ascii_isspace ((*string)[strlen (units[i].names[j])])) {
                                ing->unit = g_strdup (units[i].unit);
                                *string += strlen (units[i].names[j]);
                                return TRUE;
                        }
                }
        }

        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("I don't know this unit: %s"), string);

        return FALSE;

}

static gboolean
gr_ingredients_list_add_one (GrIngredientsList  *ingredients,
                             char               *line,
                             GError            **error)
{
        Ingredient *ing;
        const char *s;

        if (line[0] == '\0')
                return TRUE;

        ing = g_new0 (Ingredient, 1);

        line = g_strstrip (line);

        if (!parse_as_number (ing, &line, error)) {
                ingredient_free (ing);
                return FALSE;
        }

        skip_whitespace (&line);

        if (parse_as_unit (ing, &line, NULL))
                skip_whitespace (&line);

        s = gr_ingredient_find (line);
        if (s)
                ing->name = g_strdup (s);
        else
                ing->name = g_strdup (line);

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

        lines = g_strsplit (text, "\n", 0);

        for (i = 0; lines[i]; i++) {
                if (!gr_ingredients_list_add_one (ingredients, lines[i], error))
                        return FALSE;
        }

       return TRUE;

}

static void
ingredients_list_finalize (GObject *object)
{
        GrIngredientsList *self = GR_INGREDIENTS_LIST (object);

        g_list_free_full (self->ingredients, (GDestroyNotify)ingredient_free);

        G_OBJECT_CLASS (gr_ingredients_list_parent_class)->finalize (object);
}

static void
gr_ingredients_list_init (GrIngredientsList *ingredients)
{
}

static void
gr_ingredients_list_class_init (GrIngredientsListClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = ingredients_list_finalize;
}

GrIngredientsList *
gr_ingredients_list_new (const char *text)
{
        GrIngredientsList *ingredients;

        ingredients = g_object_new (GR_TYPE_INGREDIENTS_LIST, NULL);
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

int
gcd (int m, int n)
{
        int r;

        if (m == 0 && n == 0)
                return -1;

        if (m < 0) m = -m;
        if (n < 0) n = -n;

        while (n) {
                r = m % n;
                m = n;
                n = r;
        }

        return m;
}

typedef struct {
        int num;
        int denom;
        const char *ch;
} VulgarFraction;

static VulgarFraction fractions[] = {
        { 1,  4, "¼" },
        { 1,  2, "½" },
        { 3,  4, "¾" },
        { 1,  7, "⅐" },
        { 1,  9, "⅑" },
        { 1, 10, "⅒" },
        { 1,  3, "⅓" },
        { 2,  3, "⅔" },
        { 1,  5, "⅕" },
        { 2,  5, "⅖" },
        { 3,  5, "⅗" },
        { 4,  5, "⅘" },
        { 1,  6, "⅙" },
        { 5,  6, "⅚" },
        { 1,  8, "⅛" },
        { 3,  8, "⅜" },
        { 5,  8, "⅝" },
        { 7,  8, "⅞" }
};

static char *
format_fraction (int num, int denom)
{
        int i;

        for (i = 0; i < G_N_ELEMENTS (fractions); i++) {
                if (fractions[i].num == num && fractions[i].denom == denom)
                        return g_strdup (fractions[i].ch);
        }

        return g_strdup_printf ("%d/%d", num, denom);
}

static char *
format_scaled_number (Ingredient *ing, int num, int denom)
{
        if (ing->fraction) {
                int n = ing->num * num;
                int d = ing->denom * denom;
                int g = gcd (n, d);
                int integral;
                g_autofree char *fraction = NULL;

                n = n / g;
                d = d / g;

                if (d == 1)
                        return g_strdup_printf ("%d", n);
                else {
                        integral = n / d;
                        fraction = format_fraction (n % d, d);
                        if (integral == 0)
                                return g_strdup_printf ("%s", fraction);
                        else
                                return g_strdup_printf ("%d %s", integral, fraction);
                }
        }
        else {
                double value = ing->value * num / denom;

                return g_strdup_printf ("%g", value);
        }
}

static void
ingredient_scale (Ingredient *ing, int num, int denom, GString *s)
{
        g_autofree char *scaled = NULL;

        scaled = format_scaled_number (ing, num, denom);
        g_string_append (s, scaled);
        g_string_append (s, " ");
        if (ing->unit) {
                g_string_append (s, ing->unit);
                g_string_append (s, " ");
        }
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
gr_ingredients_list_get_ingredients (GrIngredientsList *ingredients)
{
        char **ret;
        int i;
        GList *l;

        ret = g_new0 (char *, g_list_length (ingredients->ingredients) + 1);
        for (i = 0, l = ingredients->ingredients; l; i++, l = l->next) {
                Ingredient *ing = (Ingredient *)l->data;
                ret[i] = g_strdup (ing->name);
        }

        return ret;
}
