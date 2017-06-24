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
#include <math.h>

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

/*
 * http://www.ics.uci.edu/~eppstein/numth/frap.c
 *
 * find rational approximation to given real number
 * David Eppstein / UC Irvine / 8 Aug 1993
 *
 * With corrections from Arno Formella, May 2008
 *
 * based on the theory of continued fractions
 * if x = a1 + 1/(a2 + 1/(a3 + 1/(a4 + ...)))
 * then best approximation is found by truncating this series
 * (with some adjustments in the last term).
 *
 * Note the fraction can be recovered as the first column of the matrix
 *  ( a1 1 ) ( a2 1 ) ( a3 1 ) ...
 *  ( 1  0 ) ( 1  0 ) ( 1  0 )
 * Instead of keeping the sequence of continued fraction terms,
 * we just keep the last partial product of these matrices.
*/
static void
rational_approximation (double  input,
                        int     max_denom,
                        int    *num,
                        int    *denom)
{
        long m[2][2];
        double x, n0, d0, n1, d1;
        long ai;

        x = input;

        m[0][0] = m[1][1] = 1;
        m[0][1] = m[1][0] = 0;

        while (m[1][0] * (ai = (long)x) + m[1][1] <= max_denom) {
                long t;

                t = m[0][0] * ai + m[0][1];
                m[0][1] = m[0][0];
                m[0][0] = t;
                t = m[1][0] * ai + m[1][1];
                m[1][1] = m[1][0];
                m[1][0] = t;

                if (x == (double)ai)
                        break;

                x = 1 / (x - (double)ai);

                if (x > (double)0x7fffffff)
                        break;
        }

        n0 = (double)m[0][0];
        d0 = (double)m[1][0];

        ai = (max_denom - m[1][1]) / m[1][0];
        m[0][0] = m[0][0] * ai + m[0][1];
        m[1][0] = m[1][0] * ai + m[1][1];

        n1 = (double)m[0][0];
        d1 = (double)m[1][0];

        if (fabs (input - n0 / d0) < fabs (input - n1 / d1)) {
                *num = (int)n0;
                *denom = (int)d0;
        } else {
                *num = (int)n1;
                *denom = (int)d1;
        }
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

/* a workaround for poor availability of OpenType frak support in our fonts */
static VulgarFraction fractions[] = {
        { 1,  2, "½" },
        { 1,  3, "⅓" },
        { 2,  3, "⅔" },
        { 1,  4, "¼" },
        { 3,  4, "¾" },
        { 1,  5, "⅕" },
        { 2,  5, "⅖" },
        { 3,  5, "⅗" },
        { 4,  5, "⅘" },
        { 1,  6, "⅙" },
        { 5,  6, "⅚" },
        { 1,  7, "⅐" },
        { 1,  8, "⅛" },
        { 3,  8, "⅜" },
        { 5,  8, "⅝" },
        { 7,  8, "⅞" },
        { 1,  9, "⅑" },
        { 1, 10, "⅒" }
};

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
        gint64 num, denom;
        char *end = NULL;

        num = g_ascii_strtoll (orig, &end, 10);
        if (end == NULL || end == orig || end[0] != '/') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a fraction"), *input);
                return FALSE;
        }

        orig = end + 1;

        denom = g_ascii_strtoll (orig, &end, 10);
        if (end != NULL && !space_or_nul (end[0])) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a fraction"), *input);
                return FALSE;
        }
        if (denom == 0)
                denom = 1;

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
        gint64 num;
        char *end = NULL;

        num = g_ascii_strtoll (*input, &end, 10);
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

        if (*input == end ||
            (end != NULL && !space_or_nul (end[0]))) {
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
                char *endofint;
                GrNumber n;

                endofint = orig;
                valid = skip_whitespace (&orig);

                if (parse_as_vulgar_fraction (&n, &orig, NULL) ||
                    parse_as_fraction (&n, &orig, NULL)) {
                        gr_number_add (number, &n, number);
                        *input = orig;
                        return TRUE;
                }

                if (valid || *orig == '\0') {
                        *input = endofint;
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

static const char *sup[] = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹" };
static const char *sub[] = { "₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉" };

static void
append_digits (GString    *s,
               const char *digits[],
               int         n)
{
        if (n == 0)
                return;

        append_digits (s, digits, n / 10);
        g_string_append (s, digits[n % 10]);
}

static char *
format_fraction (int integral,
                 int num,
                 int denom)
{
        int i;
        GString *s;

        s = g_string_new ("");

        if (integral != 0)
                g_string_append_printf (s, "%d", integral);

        if (num == 0)
                goto out;

        if (integral != 0)
                g_string_append (s, " ");

        for (i = 0; i < G_N_ELEMENTS (fractions); i++) {
                if (fractions[i].num == num && fractions[i].denom == denom) {
                        g_string_append (s, fractions[i].ch);
                        goto out;
                }
        }

        append_digits (s, sup, num);
        g_string_append (s, "⁄");
        append_digits (s, sub, denom);

out:
        return g_string_free (s, FALSE);
}

char *
gr_number_format (GrNumber *number)
{
        if (number->fraction)
                return format_fraction (0, number->num, number->denom);
        else {
                double integral, rem;
                int num, denom;

                integral = floor (number->value);
                rem = number->value - integral;

                if (rational_approximation (rem, 20, &num, &denom))
                        return format_fraction ((int)integral, num, denom);
                else
                        return g_strdup_printf ("%g", number->value);
        }
}
