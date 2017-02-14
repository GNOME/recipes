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

typedef struct {
        char *address;
        char *subject;
        char *body;
        char **attachments;
} MailData;

static void
mail_data_free (gpointer data)
{
        MailData *md = data;

        g_free (md->address);
        g_free (md->subject);
        g_free (md->body);
        g_strfreev (md->attachments);
        g_free (md);
}

static void
send_mail_using_mailto (MailData *data)
{
        g_autoptr(GString) url = NULL;
        g_autofree char *encoded_subject = NULL;
        g_autofree char *encoded_body = NULL;
        int i;

        encoded_subject = g_uri_escape_string (data->subject, NULL, FALSE);
        encoded_body = g_uri_escape_string (data->body, NULL, FALSE);

        url = g_string_new ("mailto:");

        g_string_append_printf (url, "\"%s\"", data->address);
        g_string_append_printf (url, "?subject=%s", encoded_subject);
        g_string_append_printf (url, "&body=%s", encoded_body);

        for (i = 0; data->attachments[i]; i++) {
                g_autofree char *path = NULL;

                path = g_uri_escape_string (data->attachments[i], NULL, FALSE);
                g_string_append_printf (url, "&attach=%s", path);
        }

        g_app_info_launch_default_for_uri (url->str, NULL, NULL);

        mail_data_free (data);
}

static GDBusProxy *
get_mail_portal_proxy (void)
{
        static GDBusProxy *proxy = NULL;

        if (proxy == NULL)
                proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       "org.freedesktop.portal.Desktop",
                                                       "/org/freedesktop/portal/desktop",
                                                       "org.freedesktop.portal.Email",
                                                       NULL, NULL);

        return proxy;
}

static void
callback (GObject      *source,
          GAsyncResult *result,
          gpointer      data)
{
        g_autoptr(GVariant) reply = NULL;
        g_autoptr(GError) error = NULL;
        MailData *md = data;

        reply = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), result, &error);
        if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE) ||
            g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
                /* Email portal not present, fall back to mailto: */
                send_mail_using_mailto (md);
                return;
        }

        mail_data_free (md);
}

static void
send_mail_using_mail_portal (MailData *data)
{
        GVariantBuilder opt_builder;
        GDBusProxy *proxy;

        proxy = get_mail_portal_proxy ();

        g_variant_builder_init (&opt_builder, G_VARIANT_TYPE ("a{sv}"));
        g_variant_builder_add (&opt_builder, "{sv}", "address", g_variant_new_string (data->address));
        g_variant_builder_add (&opt_builder, "{sv}", "subject", g_variant_new_string (data->subject));
        g_variant_builder_add (&opt_builder, "{sv}", "body", g_variant_new_string (data->body));
        g_variant_builder_add (&opt_builder, "{sv}", "attachments", g_variant_new_strv ((const char * const *)data->attachments, -1));

        g_dbus_proxy_call (proxy,
                           "ComposeEmail",
                           g_variant_new ("(sa{sv})",
                                          "",
                                          &opt_builder),
                           G_DBUS_CALL_FLAGS_NONE,
                           G_MAXINT,
                           NULL,
                           callback,
                           data);
}

gboolean
gr_send_mail (const char  *address,
              const char  *subject,
              const char  *body,
              const char **attachments,
              GError     **error)
{
        MailData *data;

        data = g_new (MailData, 1);
        data->address = g_strdup (address ? address : "");
        data->subject = g_strdup (subject ? subject : "");
        data->body = g_strdup (body ? body : "");
        data->attachments = g_strdupv ((char **)attachments);

        send_mail_using_mail_portal (data);

        return TRUE;
}
