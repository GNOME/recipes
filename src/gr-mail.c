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
              GList       *attachments,
              GError     **error)
{
        g_autoptr(GString) url = NULL;
        GList *l;
        g_autofree char *encoded_subject = NULL;
        g_autofree char *encoded_body = NULL;
        const char *argv[4];
        g_autoptr(GDBusConnection) bus = NULL;
        g_autoptr(GVariant) result = NULL;

        encoded_subject = g_uri_escape_string (subject ? subject : "", NULL, FALSE);
        encoded_body = g_uri_escape_string (body ? body : "", NULL, FALSE);

        url = g_string_new ("mailto:");

        g_string_append_printf (url, "\"%s\"", address ? address : "");
        g_string_append_printf (url, "?subject=%s", encoded_subject);
        g_string_append_printf (url, "&body=%s", encoded_body);

        for (l = attachments; l; l = l->next) {
                g_autofree char *path = g_uri_escape_string (l->data, NULL, FALSE);
                g_string_append_printf (url, "&attach=%s", path);
        }

        argv[0] = "/usr/bin/evolution";
        argv[1] = "--component=mail";
        argv[2] = url->str;
        argv[3] = NULL;

        bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

        result = g_dbus_connection_call_sync (bus,
                                     "org.freedesktop.Flatpak",
                                     "/org/freedesktop/Flatpak/Development",
                                     "org.freedesktop.Flatpak.Development",
                                     "HostCommand",
                                     g_variant_new ("(^ay^aay@a{uh}@a{ss}u)",
                                                    g_get_home_dir (),
                                                    argv,
                                                    g_variant_new_array (G_VARIANT_TYPE ("{uh}"), NULL, 0),
                                                    g_variant_new_array (G_VARIANT_TYPE ("{ss}"), NULL, 0),
                                                    0),
                                     G_VARIANT_TYPE ("(u)"),
                                     0,
                                     G_MAXINT,
                                     NULL,
                                     error);

        return result != NULL;
}
