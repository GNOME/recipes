/* ingredients.c
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com#}#>
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
#include <locale.h>
#include <glib.h>
#include "gr-ingredients-list.h"
#include "gr-ingredients-list.c"
#include "gr-ingredient.h"
#include "gr-ingredient.c"
#include "gr-number.h"
#include "gr-number.c"
#include "gr-unit.h"
#include "gr-unit.c"
#include "gr-utils.h"
#include "gr-utils.c"

static GString *string;

static int
test_file (const char *filename)
{
        g_autofree char *contents = NULL;
        gsize  length;
        g_autoptr(GError) error = NULL;

        if (!g_file_get_contents (filename, &contents, &length, &error)) {
                fprintf (stderr, "%s\n", error->message);
                return 1;
        }

        if (!gr_ingredients_list_validate (contents, &error)) {
                g_string_append_printf (string, "ERROR %s\n", error->message);
        }
        else {
                g_autoptr(GrIngredientsList) ingredients = NULL;
                GList *l;

                ingredients = gr_ingredients_list_new (contents);
                for (l = ingredients->ingredients; l; l = l->next) {
                        Ingredient *ing = (Ingredient *)l->data;
                        g_string_append_printf (string, "AMOUNT %f\n", ing->amount);
                        g_string_append_printf (string, "UNIT %s\n", gr_unit_get_name (ing->unit));
                        g_string_append_printf (string, "NAME %s\n", ing->name);
                        g_string_append_printf (string, "\n");
                }
        }

        return 0;
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
        gboolean valid_input;
        GError *error = NULL;
        int res;

        valid_input = strstr (filename, "valid") != NULL;
        expected_file = get_expected_filename (filename);

        string = g_string_sized_new (0);

        res = test_file (filename);
        g_assert_cmpint (res, ==, valid_input ? 0 : 1);

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
        path = g_test_build_filename (G_TEST_DIST, "ingredients-data", NULL);
        dir = g_dir_open (path, 0, &error);
        g_free (path);
        g_assert_no_error (error);
        while ((name = g_dir_read_name (dir)) != NULL) {
                if (!g_str_has_suffix (name, ".in"))
                        continue;

                path = g_strdup_printf ("/ingredients/parse/%s", name);
                g_test_add_data_func_full (path, g_test_build_filename (G_TEST_DIST, "ingredients-data", name, NULL),
                                           test_parse, g_free);
                g_free (path);
        }
        g_dir_close (dir);

  return g_test_run ();
}
