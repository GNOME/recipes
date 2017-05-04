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
        const char *p;
        g_autoptr(GString) s2 = NULL;
        g_auto(GStrv) strv = NULL;
        int i;

        s2 = g_string_new ("");
        for (p = s; *p; p = g_utf8_next_char (p)) {
                gunichar ch = g_utf8_get_char (p);
                if (ch == (gunichar)'"' || ch == (gunichar)'\\')
                        g_string_append_c (s2, '\\');
                g_string_append_unichar (s2, ch);
        }

        strv = g_strsplit (s2->str, "\n", -1);

        for (i = 0; strv[i]; i++) {
                if (strv[i][0] != 0)
                        g_print ("N_(\"%s\")\n", strv[i]);
        }
}

static void
emit_ingredients (const char *s,
                  gboolean    segments)
{
        g_auto(GStrv) strv = NULL;
        int i;

        strv = g_strsplit (s, "\n", -1);

        for (i = 0; strv[i]; i++) {
                g_auto(GStrv) fields = NULL;

                fields = g_strsplit (strv[i], "\t", -1);

                if (g_strv_length (fields) != 4) {
                        g_printerr ("Wrong number of fields; ignoring line: %s\n", strv[i]);
                        continue;
                }

                if (!segments)
                        g_print ("%s\n", fields[2]);
                else if (fields[3][0] != '\0')
                        g_print ("%s\n", fields[3]);
        }
}

static void
emit_segments (const char *s)
{
        g_auto(GStrv) strv = NULL;
        int i;

        strv = g_strsplit (s, "\n", -1);

        for (i = 0; strv[i]; i++) {
                g_auto(GStrv) fields = NULL;

                fields = g_strsplit (strv[i], "\t", -1);

                if (g_strv_length (fields) != 4) {
                        g_printerr ("Wrong number of fields; ignoring line: %s\n", strv[i]);
                        continue;
                }
                if (fields[3][0] != 0)
                        g_print ("N_(\"%s\")\n", fields[3]);
        }
}

int
main (int argc, char *argv[])
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_auto(GStrv) groups = NULL;
        gsize length;
        const char **keys;
        const char *recipe_keys[] = {
                "Name",
                "Description",
                "Instructions",
                "Notes",
                NULL,
        };
        const char *optional_keys[] = {
                "Notes",
                NULL,
                };
        const char *chef_keys[] = {
                "Description",
                NULL
        };
        gboolean extract_strings = TRUE;
        gboolean extract_chefs = FALSE;
        gboolean extract_ingredients = FALSE;
        gboolean extract_segments = FALSE;
        unsigned int i, j;

        if (argc > 2 && g_strcmp0 (argv[1], "--ingredients") == 0) {
                extract_strings = FALSE;
                extract_ingredients = TRUE;
                argv[1] = argv[2];
                argc--;
        }
        else if (argc > 2 && g_strcmp0 (argv[1], "--segments") == 0) {
                extract_strings = FALSE;
                extract_ingredients = TRUE;
                extract_segments = TRUE;
                argv[1] = argv[2];
                argc--;
        }
        else if (argc > 2 && g_strcmp0 (argv[1], "--chefs") == 0) {
                extract_strings = TRUE;
                extract_chefs = TRUE;
                argv[1] = argv[2];
                argc--;
        }

        if (argc != 2)
                return 1;

        setlocale (LC_ALL, "");

        keyfile = g_key_file_new ();

        if (!g_key_file_load_from_file (keyfile, argv[1], G_KEY_FILE_NONE, &error)) {
                g_printerr ("Failed to parse %s: %s\n", argv[1], error->message);
                return 1;
        }

        if (extract_strings)
                g_print ("#if 0\n\n");

        if (extract_chefs)
                keys = chef_keys;
        else
                keys = recipe_keys;

        groups = g_key_file_get_groups (keyfile, &length);
        for (i = 0; i < length; i++) {
                if (g_str_has_prefix (groups[i], "Metadata"))
                        continue;

                if (extract_ingredients) {
                        g_autofree char *s = NULL;
                        s = g_key_file_get_string (keyfile, groups[i], "Ingredients", &error);
                        if (!s) {
                                g_printerr ("Failed to get key '%s' for group '%s': %s\n", "Ingredients", groups[i], error->message);
                                g_clear_error (&error);
                                continue;
                        }
                        emit_ingredients (s, extract_segments);
                        continue;
                }

                if (extract_strings) {
                        for (j = 0; keys[j]; j++) {
                                g_autofree char *s = NULL;
                                s = g_key_file_get_string (keyfile, groups[i], keys[j], &error);
                                if (!s) {
                                        if (!g_strv_contains (optional_keys, keys[j]))
                                                g_printerr ("Failed to get key '%s' for group '%s': %s\n", keys[j], groups[i], error->message);
                                        g_clear_error (&error);
                                        continue;
                                }

                                emit_string (s);
                        }
                        if (!extract_chefs) {
                                g_autofree char *s = NULL;
                                s = g_key_file_get_string (keyfile, groups[i], "Ingredients", &error);
                                if (!s) {
                                        g_printerr ("Failed to get key '%s' for group '%s': %s\n", "Ingredients", groups[i], error->message);
                                        g_clear_error (&error);
                                }
                                else {
                                        emit_segments (s);
                                }
                        }
                }
        }

        if (extract_strings)
                g_print ("\n\n#endif\n");

        return 0;
}
