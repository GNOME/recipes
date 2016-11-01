/* gr-ingredients.c
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
#include "gr-ingredients.h"

static gboolean
parse_as_fraction (char    *string,
                   GError **error)
{
        g_auto(GStrv) parts = NULL;
        guint64 num, denom;
        char *end = NULL;

        parts = g_strsplit (string, "/", 2);

        num = g_ascii_strtoull (parts[0], &end, 10);
        if (end != NULL && end[0] != '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a fraction"), string);
                return FALSE;
        }

        denom = g_ascii_strtoull (parts[1], &end, 10);
        if (end != NULL && end[0] != '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a fraction"), string);
                return FALSE;
        }

        return TRUE;
}

static gboolean
parse_as_number (char    *string,
                 GError **error)
{
        gdouble value;
        char *end = NULL;

        if (strchr (string, '/'))
                return parse_as_fraction (string, error);

        value = g_strtod (string, &end);

        if (end != NULL && end[0] != '\0') {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Could not parse %s as a number"), string);
                return FALSE;
        }

        return TRUE;
}

static const char * const units[] = {
        "g",
        "kg",
        "l",
        "liter",
        "liters",
        "pound",
        "pounds",
        "box",
        "boxes",
        NULL
};

static gboolean
parse_as_unit (char    *string,
               GError **error)
{
        if (!g_strv_contains (units, string)) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("I don't know this unit: %s"), string);
                return FALSE;
        }

        return TRUE;
}

static gboolean
gr_ingredients_validate_one (char   *line,
                             GError **error)
{
        g_auto(GStrv) strings = NULL;

        line = g_strstrip (line);
        strings = g_strsplit (line, " ", 3);

        if (!parse_as_number (strings[0], error))
                return FALSE;

        if (g_strv_length (strings) > 2 &&
            !parse_as_unit (strings[1], error))
                return FALSE;

        return TRUE;
}

gboolean
gr_ingredients_validate (const char  *text,
                         GError     **error)
{
        g_auto(GStrv) lines = NULL;
        int i;

        lines = g_strsplit (text, "\n", 0);

        for (i = 0; lines[i]; i++) {
                if (!gr_ingredients_validate_one (lines[i], error))
                        return FALSE;
        }

       return TRUE;
}
