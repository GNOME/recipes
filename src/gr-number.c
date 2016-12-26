/* gr-number.c
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
#include <gio/gio.h>

#include "gr-number.h"
#include "gr-utils.h"

static int
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

void
gr_number_set_fraction (GrNumber *number,
                        int       num,
                        int       denom)
{
        int g;

        if (denom < 0) {
                num = -num;
                denom = -denom;
        }
        g = gcd (num, denom);
        number->fraction = TRUE;
        number->num = num / g;
        number->denom = denom / g;
        number->value = ((double) num) / ((double) denom);
}

void
gr_number_set_float (GrNumber *number,
                     double    value)
{
        number->fraction = FALSE;
        number->num = 0;
        number->denom = 0;
        number->value = value;
}

GrNumber *
gr_number_new_fraction (int num, int denom)
{
        GrNumber *number;

        number = g_new (GrNumber, 1);
        gr_number_set_fraction (number, num, denom);

        return number;
}

GrNumber *
gr_number_new_float (double value)
{
        GrNumber *number;

        number = g_new (GrNumber, 1);
        gr_number_set_float (number, value);

        return number;
}

void
gr_number_multiply (GrNumber *a1,
                    GrNumber *a2,
                    GrNumber *b)
{
        if (a1->fraction && a2->fraction)
                gr_number_set_fraction (b, a1->num * a2->num, a1->denom * a2->denom);
        else
                gr_number_set_float (b, a1->value * a2->value);
}

void
gr_number_add (GrNumber *a1,
               GrNumber *a2,
               GrNumber *b)
{
        if (a1->fraction && a2->fraction)
                gr_number_set_fraction (b, a1->num * a2->denom + a2->num * a1->denom, a1->denom * a2->denom);
        else
                gr_number_set_float (b, a1->value + a2->value);
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

/* a workaround for poor availability of OpenType frak support in our fonts */
static gboolean
parse_as_vulgar_fraction (GrNumber  *number,
                          char     **input,
                          GError   **error)
{
        int i;

        for (i = 0; i < G_N_ELEMENTS (fractions); i++) {
                const char *vf = fractions[i].ch;
                if (g_str_has_prefix (*input, vf) &&
                    space_or_nul ((*input)[strlen (vf)])) {
                        gr_number_set_fraction (number, fractions[i].num, fractions[i].denom);
                        *input += strlen (vf);

                        return TRUE;
                }
        }

        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("Could not parse %s as a fraction"), *input);

        return FALSE;
}

static gboolean
parse_as_fraction (GrNumber  *number,
                   char     **input,
                   GError   **error)
{
        char *orig = *input;
        guint64 num, denom;
        char *end = NULL;

        num = g_ascii_strtoull (orig, &end, 10);
        if (end == NULL || end == orig || end[0] != '/') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a fraction"), *input);
                return FALSE;
        }

        orig = end + 1;

        denom = g_ascii_strtoull (orig, &end, 10);
        if (end != NULL && !space_or_nul (end[0])) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a fraction"), *input);
                return FALSE;
        }

        *input = end;

        gr_number_set_fraction (number, (int)num, (int)denom);

        return TRUE;
}

static gboolean
parse_as_integer (GrNumber  *number,
                  char     **input,
                  gboolean   require_whitespace,
                  GError   **error)
{
        guint64 num;
        char *end = NULL;

        num = g_ascii_strtoull (*input, &end, 10);
        if (end == *input ||
            (require_whitespace && end != NULL && !space_or_nul (end[0]))) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a integer"), *input);
               return FALSE;
        }
        *input = end;

        gr_number_set_fraction (number, (int)num, 1);

        return TRUE;
}

static gboolean
parse_as_float (GrNumber  *number,
                char     **input,
                GError   **error)
{
        double value;
        char *end = NULL;

        value = g_strtod (*input, &end);

        if (end != NULL && !space_or_nul (end[0])) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a float"), *input);
                return FALSE;
        }
        *input = end;

        gr_number_set_float (number, value);

        return TRUE;
}

gboolean
gr_number_parse (GrNumber  *number,
                 char     **input,
                 GError   **error)
{
        char *orig = *input;

        if (parse_as_vulgar_fraction (number, input, NULL)) {
                return TRUE;
        }

        if (parse_as_fraction (number, input, NULL)) {
                return TRUE;
        }

        if (parse_as_integer (number, &orig, FALSE, NULL)) {
                gboolean valid;
                GrNumber n;

                valid = skip_whitespace (&orig);

                if (parse_as_vulgar_fraction (&n, &orig, NULL) ||
                    parse_as_fraction (&n, &orig, NULL)) {
                        gr_number_add (number, &n, number);
                        valid = TRUE;
                }

                if (valid || *orig == '\0') {
                        *input = orig;
                        return TRUE;
                }
        }

        if (parse_as_float (number, input, NULL)) {
                return TRUE;
        }

        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("Could not parse %s as a number"), *input);

        return FALSE;
}

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

char *
gr_number_format (GrNumber *number)
{
        if (number->fraction) {
                if (number->denom == 1)
                        return g_strdup_printf ("%d", number->num);
                else {
                        int integral;
                        g_autofree char *fraction = NULL;

                        if (number->denom == 0)
                                return g_strdup ("");

                        integral = number->num / number->denom;
                        fraction = format_fraction (number->num % number->denom, number->denom);
                        if (integral == 0)
                                return g_strdup_printf ("%s", fraction);
                        else
                                return g_strdup_printf ("%d %s", integral, fraction);
                }
        }
        else {
                return g_strdup_printf ("%g", number->value);
        }
}
