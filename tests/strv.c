/* strv.c
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
#include <glib.h>
#include "gr-utils.h"

static void
test_strv_prepend (void)
{
        char **strv;

        strv = g_new0 (char *, 3);
        strv[0] = g_strdup ("b");
        strv[1] = g_strdup ("c");

        strv_prepend (&strv, "a");

        g_assert (g_strv_length (strv) == 3);
        g_assert_cmpstr (strv[0], ==, "a");
        g_assert_cmpstr (strv[1], ==, "b");
        g_assert_cmpstr (strv[2], ==, "c");

        g_strfreev (strv);

        strv = g_new0 (char *, 1);

        strv_prepend (&strv, "a");

        g_assert (g_strv_length (strv) == 1);
        g_assert_cmpstr (strv[0], ==, "a");

        g_strfreev (strv);
}

static void
test_strv_remove (void)
{
        char **strv;

        strv = g_new0 (char *, 4);
        strv[0] = g_strdup ("b");
        strv[1] = g_strdup ("c");
        strv[2] = g_strdup ("b");

        strv_remove (&strv, "b");

        g_assert (g_strv_length (strv) == 1);
        g_assert_cmpstr (strv[0], ==, "c");

        strv_remove (&strv, "a");

        g_assert (g_strv_length (strv) == 1);
        g_assert_cmpstr (strv[0], ==, "c");

        strv_remove (&strv, "c");

        g_assert (g_strv_length (strv) == 0);

        g_strfreev (strv);
}

int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/strv/prepend", test_strv_prepend);
        g_test_add_func ("/strv/remove", test_strv_remove);

        return g_test_run ();
}
