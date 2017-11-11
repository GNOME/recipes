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


typedef struct {
        int num;
        int denom;
        double value;
} Approximation;

/* These are all candidates with 'nice' denominators
 * (i.e. 2, 3, 4, 5, 6, 8, 10, 12, 15, 16), sorted
 * by size.
 */
static Approximation approx[] = {
  {  0,  1, 0.0/1.0 },
  {  1, 16, 1.0/16.0 },
  {  1, 15, 1.0/15.0 },
  {  1, 12, 1.0/12.0 },
  {  1, 10, 1.0/10.0 },
  {  1,  8, 1.0/8.0 },
  {  2, 15, 2.0/15.0 },
  {  1,  6, 1.0/6.0 },
  {  3, 16, 3.0/16.0 },
  {  1,  5, 1.0/5.0 },
  {  1,  4, 1.0/4.0 },
  {  4, 15, 4.0/15.0 },
  {  3, 10, 3.0/10.0 },
  {  5, 16, 5.0/16.0 },
  {  1,  3, 1.0/3.0 },
  {  3,  8, 3.0/8.0 },
  {  2,  5, 2.0/5.0 },
  {  5, 12, 5.0/12.0 },
  {  7, 16, 7.0/16.0 },
  {  7, 15, 7.0/15.0 },
  {  1,  2, 1.0/2.0 },
  {  8, 15, 8.0/15.0 },
  {  9, 16, 9.0/16.0 },
  {  7, 12, 7.0/12.0 },
  {  3,  5, 3.0/5.0 },
  {  5,  8, 5.0/8.0 },
  {  2,  3, 2.0/3.0 },
  { 11, 16, 11.0/16.0 },
  {  7, 10, 7.0/10.0 },
  { 11, 15, 11.0/15.0 },
  {  3,  4, 3.0/4.0 },
  {  4,  5, 4.0/5.0 },
  { 13, 16, 13.0/16.0 },
  {  5,  6, 5.0/6.0 },
  { 13, 15, 13.0/15.0 },
  {  7,  8, 7.0/8.0 },
  {  9, 10, 9.0/10.0 },
  { 11, 12, 11.0/12.0 },
  { 14, 15, 14.0/15.0 },
  { 15, 16, 15.0/16.0 },
  {  1,  1, 1.0/1.0 }
};

static void
rational_approximation (double  input,
                        int    *num,
                        int    *denom)
{
        int i;

        for (i = 0; i < G_N_ELEMENTS (approx); i++) {
                if (approx[i].value >= input) {
                        if (i > 0 && input - approx[i - 1].value < approx[i].value - input)
                                i--;
                        break;
                }
        }
        *num = approx[i].num;
        *denom = approx[i].denom;
}

typedef struct {
        int num;
        int denom;
        const char *ch;
        double value;
} VulgarFraction;

/* a workaround for poor availability of OpenType frak support in our fonts */
static VulgarFraction fractions[] = {
        { 1,  2, "½", 1.0/2.0 },
        { 1,  3, "⅓", 1.0/3.0 },
        { 2,  3, "⅔", 2.0/3.0 },
        { 1,  4, "¼", 1.0/4.0 },
        { 3,  4, "¾", 3.0/4.0 },
        { 1,  5, "⅕", 1.0/5.0 },
        { 2,  5, "⅖", 2.0/5.0 },
        { 3,  5, "⅗", 3.0/5.0 },
        { 4,  5, "⅘", 4.0/5.0 },
        { 1,  6, "⅙", 1.0/6.0 },
        { 5,  6, "⅚", 5.0/6.0 },
        { 1,  7, "⅐", 1.0/7.0 },
        { 1,  8, "⅛", 1.0/8.0 },
        { 3,  8, "⅜", 3.0/8.0 },
        { 5,  8, "⅝", 5.0/8.0 },
        { 7,  8, "⅞", 7.0/8.0 },
        { 1,  9, "⅑", 1.0/9.0 },
        { 1, 10, "⅒", 1.0/10.0 }
};

static gboolean
parse_as_vulgar_fraction (double    *number,
                          char     **input,
                          GError   **error)
{
        int i;

        for (i = 0; i < G_N_ELEMENTS (fractions); i++) {
                const char *vf = fractions[i].ch;
                if (g_str_has_prefix (*input, vf) &&
                    space_or_nul ((*input)[strlen (vf)])) {
                        *number = fractions[i].value;
                        *input += strlen (vf);
                        return TRUE;
                }
        }

        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("Could not parse %s as a fraction"), *input);

        return FALSE;
}

static const char *sup[] = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹" };
static const char *sub[] = { "₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉" };

static gboolean
parse_as_fancy_fraction (double    *number,
                         char     **input,
                         GError   **error)
{
        char *p;
        int num, denom;
        int i;

        num = 0;
        denom = 0;
        p = *input;
        while (*p) {
                for (i = 0; i < 10; i++) {
                        if (strncmp (p, sup[i], strlen (sup[i])) == 0) {
                                num = 10 * num + i;
                                p += strlen (sup[i]);
                                break;
                        }
                }
                if (i == 10)
                        break;
        }
        if (strncmp (p, "⁄", strlen ("⁄")) != 0)
                return FALSE;
        p += strlen ("⁄");
        while (*p) {
                for (i = 0; i < 10; i++) {
                        if (strncmp (p, sub[i], strlen (sub[i])) == 0) {
                                denom = 10 * denom + i;
                                p += strlen (sub[i]);
                                break;
                        }
                }
                if (i == 10)
                        break;
        }
        if (*p != '\0' && *p != ' ')
                return FALSE;

        if (num == 0 || denom == 0)
                return FALSE;

        *number = (double)num/(double)denom;
        *input = p;

        return TRUE;
}

static gboolean
parse_as_ascii_fraction (double    *number,
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
        *number = (double)num / (double)MAX(denom, 1);

        return TRUE;
}

static gboolean
parse_as_integer (double    *number,
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
        *number = (double)num;

        return TRUE;
}

static gboolean
parse_as_float (double    *number,
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
        *number = value;

        return TRUE;
}

gboolean
gr_number_parse (double    *number,
                 char     **input,
                 GError   **error)
{
        char *orig = *input;

        if (parse_as_vulgar_fraction (number, input, NULL) ||
            parse_as_fancy_fraction (number, input, NULL) ||
            parse_as_ascii_fraction (number, input, NULL)) {
                return TRUE;
        }

        if (parse_as_integer (number, &orig, FALSE, NULL)) {
                gboolean valid;
                char *endofint;
                double n;

                endofint = orig;
                valid = skip_whitespace (&orig);

                if (parse_as_vulgar_fraction (&n, &orig, NULL) ||
                    parse_as_fancy_fraction (&n, &orig, NULL) ||
                    parse_as_ascii_fraction (&n, &orig, NULL)) {
                        *number = *number + n;
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
gr_number_format (double number)
{
        double integral;
        int num, denom;

        integral = floor (number);
        number -= integral;

        rational_approximation (number, &num, &denom);

        if (num == 1 && denom == 1) {
                integral += 1;
                num = denom = 0;
        }

        return format_fraction ((int)integral, num, denom);
}
