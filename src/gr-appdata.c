/* gr-appdata.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include <string.h>
#include <stdlib.h>
#include <gr-appdata.h>

static void
release_info_free (gpointer data)
{
        ReleaseInfo *ri = data;

        g_free (ri->version);
        g_date_time_unref (ri->date);
        g_string_free (ri->news, TRUE);
        g_free (ri);
}

static int
version_compare (const char *v1,
                 const char *v2)
{
        const char *p1, *p2;
        char *q;
        int a1, a2;
        int i;

        p1 = v1;
        p2 = v2;
        for (i = 0; i < 3; i++) {
                a1 = (int)g_ascii_strtoll (p1, &q, 10);
                if (q) {
                        if (*q == '.') q++;
                        p1 = (const char *)q;
                }
                a2 = (int)g_ascii_strtoll (p2, &q, 10);
                if (q) {
                        if (*q == '.') q++;
                        p2 = (const char *)q;
                }

                if (a1 < a2) return -1;
                if (a1 > a2) return 1;
        }

        return 0;
}

static gboolean
version_between (const char *v,
                 const char *lower,
                 const char *upper)
{
        return version_compare (lower, v) <= 0 && version_compare (v, upper) <= 0;
}

typedef struct {
        GPtrArray *result;
        const char *old_version;
        const char *new_version;
        ReleaseInfo *ri;
        gboolean collect;
        GString *text;
} ParserData;

static const char *
find_attribute (const char  *name,
                const char **names,
                const char **values)
{
        int i;

        for (i = 0; names[i]; i++) {
                if (strcmp (name, names[i]) == 0)
                        return values[i];
        }

        return NULL;
}

static void
start_element (GMarkupParseContext  *context,
               const char           *element_name,
               const char          **attribute_names,
               const char          **attribute_values,
               gpointer              user_data,
               GError              **error)
{
        ParserData *data = user_data;

        if (strcmp (element_name, "release") == 0) {
                const char *value;
                g_auto(GStrv) dmy = NULL;

                data->ri = g_new0 (ReleaseInfo, 1);
                data->ri->news = g_string_new ("");

                value = find_attribute ("version", attribute_names, attribute_values);
                data->ri->version = g_strdup (value);

                value = find_attribute ("date", attribute_names, attribute_values);
                dmy = g_strsplit (value, "-", 3);
                if (g_strv_length (dmy) != 3) {
                        g_message ("Failed to parse release: %s", value);
                        data->ri->date = g_date_time_new_from_unix_utc (0);
                }
                else {
                        data->ri->date = g_date_time_new_utc (atoi (dmy[0]), atoi (dmy[1]), atoi (dmy[2]), 0, 0, 0);
                }
        }
        else if (strcmp (element_name, "p") == 0 ||
                 strcmp (element_name, "li") == 0) {
                if (data->ri) {
                        g_string_set_size (data->text, 0);
                        data->collect = TRUE;
                }
        }
}

static void
string_append_normalized (GString    *s,
                          const char *str)
{
        gboolean initial = TRUE;
        gboolean in_whitespace = FALSE;
        const char *p;

        for (p = str; *p; p = g_utf8_next_char (p)) {
                gunichar ch = g_utf8_get_char (p);

                if (g_unichar_isspace (ch)) {
                        in_whitespace = TRUE;
                        continue;
                }

                if (in_whitespace && !initial)
                        g_string_append_c (s, ' ');

                g_string_append_unichar (s, ch);
                in_whitespace = FALSE;
                initial = FALSE;
        }
}

static void
end_element (GMarkupParseContext  *context,
             const char           *element_name,
             gpointer              user_data,
             GError              **error)
{
        ParserData *data = user_data;

        if (strcmp (element_name, "release") == 0) {
                if (!version_between (data->ri->version, data->old_version, data->new_version))
                        release_info_free (data->ri);
                else
                        g_ptr_array_add (data->result, data->ri);
                data->ri = NULL;
        }
        else if (strcmp (element_name, "p") == 0) {
                if (data->collect) {
                        data->collect = FALSE;
                        if (data->ri->news->len > 0)
                                g_string_append (data->ri->news, "\n\n");
                        string_append_normalized (data->ri->news, data->text->str);
                }
        }
        else if (strcmp (element_name, "li") == 0) {
                if (data->collect) {
                        data->collect = TRUE;
                        if (data->ri->news->len > 0)
                                g_string_append (data->ri->news, "\n");
                        g_string_append (data->ri->news, " ∙ ");
                        string_append_normalized (data->ri->news, data->text->str);
                }
        }
}

static void
text (GMarkupParseContext  *context,
      const char           *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
        ParserData *data = user_data;

        if (data->collect)
                g_string_append_len (data->text, text, text_len);
}

static GMarkupParser parser = {
        start_element,
        end_element,
        text,
        NULL,
        NULL
};

static GPtrArray *
parse_appdata (const char *file,
               const char *new_version,
               const char *old_version)
{
        g_autoptr(GMarkupParseContext) context = NULL;
        g_autofree char *buffer = NULL;
        gsize length;
        g_autoptr(GError) error = NULL;
        ParserData data;

        data.result = g_ptr_array_new_with_free_func (release_info_free);
        data.new_version = new_version;
        data.old_version = old_version;
        data.ri = NULL;
        data.collect = FALSE;
        data.text = g_string_new ("");

        if (!g_file_get_contents (file, &buffer, &length, &error)) {
                g_message ("Failed to read %s: %s", file, error->message);
                goto out;
        }

        context = g_markup_parse_context_new (&parser, 0, &data, NULL);

        if (!g_markup_parse_context_parse (context, buffer, length, &error)) {
                g_message ("Failed to parse %s: %s", file, error->message);
                g_ptr_array_set_size (data.result, 0);
        }

out:
        g_string_free (data.text, TRUE);

        return data.result;
}

GPtrArray *
get_release_info (const char *new_version,
                  const char *old_version)
{
        g_autofree char *file = NULL;

        file = g_build_filename (DATADIR, "metainfo", "org.gnome.Recipes.appdata.xml", NULL);

        g_info ("Loading release information for version %s to %s from %s", old_version, new_version, file);

        return parse_appdata (file, new_version, old_version);
}
