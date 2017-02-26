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

#include <appstream-glib.h>
#include <gr-appdata.h>

static void
release_info_free (gpointer data)
{
        ReleaseInfo *ri = data;

        g_free (ri->version);
        g_date_time_unref (ri->date);
        g_free (ri->news);
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

GPtrArray *
get_release_info (const char *new_version,
                  const char *old_version)
{
        GPtrArray *news;
        g_autoptr(AsApp) app = NULL;
        GPtrArray *releases = NULL;
        unsigned int i;
        g_autofree char *file = NULL;
        g_autoptr(GError) error = NULL;

        file = g_build_filename (DATADIR, "appdata", "org.gnome.Recipes.appdata.xml", NULL);

        news = g_ptr_array_new_with_free_func (release_info_free);

        app = as_app_new ();
        if (!as_app_parse_file (app, file, 0, &error)) {
                g_warning ("Failed to parse %s: %s", file, error->message);
                return news;
        }

        releases = as_app_get_releases (app);
        for (i = 0; i < releases->len; i++) {
                AsRelease *rel = g_ptr_array_index (releases, i);
                ReleaseInfo *ri;
                const char *tmp;

                if (!version_between (as_release_get_version (rel), old_version, new_version))
                        continue;

                ri = g_new0 (ReleaseInfo, 1);
                g_ptr_array_insert (news, -1, ri);

                ri->version = g_strdup (as_release_get_version (rel));

                if (as_release_get_timestamp (rel) > 0)
                        ri->date = g_date_time_new_from_unix_utc ((gint64) as_release_get_timestamp (rel));

                tmp = as_release_get_description (rel, NULL);
                if (tmp != NULL)
                        ri->news = as_markup_convert (tmp, AS_MARKUP_CONVERT_FORMAT_SIMPLE, NULL);
                else {
                        g_warning ("Failed to convert markup in %s", file);
                }
        }

        return news;
}
