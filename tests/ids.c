/* ids.c
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com#}#>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more &details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "gr-utils.h"

static GString *string;

static void
test_line (const char *line)
{
        g_autoptr(GError) error = NULL;
        g_auto(GStrv) strv = NULL;

        g_string_append_printf (string, "INPUT '%s'\n", line);

        strv = g_strsplit (line, "\t", 0);
        switch (g_strv_length (strv)) {
                case 1:
                        g_string_append_printf (string, "OUTPUT %s\n", generate_id (strv[0], NULL));
                        break;
                case 2:
                        g_string_append_printf (string, "OUTPUT %s\n", generate_id (strv[0], strv[1], NULL));
                        break;
                case 3:
                        g_string_append_printf (string, "OUTPUT %s\n", generate_id (strv[0], strv[1], strv[2], NULL));
                        break;
        }

        g_string_append (string, "\n");
}

static void
test_file (const char *filename)
{
        g_autofree char *contents = NULL;
        gsize length;
        g_autoptr(GError) error = NULL;
        g_auto(GStrv) lines = NULL;
        int i;

        if (!g_file_get_contents (filename, &contents, &length, &error)) {
                fprintf (stderr, "%s\n", error->message);
                return;
        }

        lines = g_strsplit (contents, "\n", -1);
        for (i = 0; lines[i]; i++) {
                if (lines[i][0] != 0 && lines[i][0] != '#')
                        test_line (lines[i]);
        }
}

static char *
get_expected_filename (const char *filename)
{
        char *f, *p, *expected;

        f = g_strdup (filename);
        p = strstr (f, ".in");
        if (p)
                *p = 0;
        expected = g_strconcat (f, ".expected", NULL);

        g_free (f);

        return expected;
}

static void
test_parse (gconstpointer d)
{
        const char *filename = d;
        char *expected_file;
        char *expected;
        GError *error = NULL;

        expected_file = get_expected_filename (filename);

        string = g_string_sized_new (0);

        test_file (filename);

        g_file_get_contents (expected_file, &expected, NULL, &error);
        g_assert_no_error (error);
        g_assert_cmpstr (string->str, ==, expected);
        g_free (expected);

        g_string_free (string, TRUE);

        g_free (expected_file);
}

int main (int argc, char *argv[])
{
        GDir *dir;
        GError *error;
        const char *name;
        char *path;

        g_setenv ("LC_ALL", "en_US.UTF-8", TRUE);
        setlocale (LC_ALL, "");

        g_test_init (&argc, &argv, NULL);

        /* allow to easily generate expected output for new test cases */
        if (argc > 1) {
                string = g_string_sized_new (0);
                test_file (argv[1]);
                g_print ("%s", string->str);
                return 0;
        }

        error = NULL;
        path = g_test_build_filename (G_TEST_DIST, "id-data", NULL);
        dir = g_dir_open (path, 0, &error);
        g_free (path);
        g_assert_no_error (error);
        while ((name = g_dir_read_name (dir)) != NULL) {
                if (!g_str_has_suffix (name, ".in"))
                        continue;

                path = g_strdup_printf ("/id/parse/%s", name);
                g_test_add_data_func_full (path, g_test_build_filename (G_TEST_DIST, "id-data", name, NULL),
                                           test_parse, g_free);
                g_free (path);
        }
        g_dir_close (dir);

  return g_test_run ();
}
