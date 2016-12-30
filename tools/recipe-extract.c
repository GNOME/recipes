/* recipe-extract.c:
 *
 * Copyright (C) 2016 Matthias Clasen
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

#include <locale.h>

#include <glib.h>


static void
emit_string (const char *s)
{
        g_auto(GStrv) strv = NULL;
        int i;

        strv = g_strsplit (s, "\n", -1);

        for (i = 0; strv[i]; i++) {
                if (strv[i][0] != 0)
                        g_print ("N_(\"%s\")\n", strv[i]);
        }
}

int
main (int argc, char *argv[])
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_auto(GStrv) groups = NULL;
        gsize length;
        const char *keys[] = {
                "Name",
                "Description",
                "Instructions"
        };
        unsigned int i, j;

        if (argc != 2)
                return 1;

        setlocale (LC_ALL, "");

        keyfile = g_key_file_new ();

        if (!g_key_file_load_from_file (keyfile, argv[1], G_KEY_FILE_NONE, &error)) {
                g_print ("Failed to parse %s: %s\n", argv[1], error->message);
                return 1;
        }

        g_print ("#if 0\n\n");

        groups = g_key_file_get_groups (keyfile, &length);
        for (i = 0; i < length; i++) {
                if (g_str_has_prefix (groups[i], "TEST"))
                        continue;

                for (j = 0; j < G_N_ELEMENTS (keys); j++) {
                        g_autofree char *s = NULL;
                        s = g_key_file_get_string (keyfile, groups[i], keys[j], &error);
                        if (!s) {
                                g_print ("Failed to get key '%s' for group '%s': %s\n", keys[j], groups[i], error->message);
                                g_clear_error (&error);
                                continue;
                        }

                        emit_string (s);
                }
        }

        g_print ("\n\n#endif\n");

        return 0;
}
