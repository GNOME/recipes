/* gr-mail.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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

#include "gr-mail.h"

gboolean
gr_send_mail (const char  *address,
              const char  *subject,
              const char  *body,
              const char **attachments,
              GError     **error)
{
        g_autoptr(GString) url = NULL;
        g_autofree char *encoded_subject = NULL;
        g_autofree char *encoded_body = NULL;
        int i;

        encoded_subject = g_uri_escape_string (subject ? subject : "", NULL, FALSE);
        encoded_body = g_uri_escape_string (body ? body : "", NULL, FALSE);

        url = g_string_new ("mailto:");

        g_string_append_printf (url, "\"%s\"", address ? address : "");
        g_string_append_printf (url, "?subject=%s", encoded_subject);
        g_string_append_printf (url, "&body=%s", encoded_body);

        for (i = 0; attachments[i]; i++) {
                g_autofree char *path = NULL;

                path = g_uri_escape_string (attachments[i], NULL, FALSE);
                g_string_append_printf (url, "&attach=%s", path);
        }

        return g_app_info_launch_default_for_uri (url->str, NULL, error);
}
