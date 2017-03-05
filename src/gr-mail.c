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
#include "gr-utils.h"

typedef struct {
        GtkWindow *window;
        char *handle;
        char *address;
        char *subject;
        char *body;
        char **attachments;
        GTask *task;
} MailData;

static void
mail_data_free (gpointer data)
{
        MailData *md = data;

        if (md->handle) {
                window_unexport_handle (md->window);
                g_free (md->handle);
        }
        g_free (md->address);
        g_free (md->subject);
        g_free (md->body);
        g_strfreev (md->attachments);
        g_object_unref (md->task);
        g_free (md);
}

static void
launch_uri_done (GObject      *source,
                 GAsyncResult *result,
                 gpointer      data)
{
        MailData *md = data;
        GError *error = NULL;

        if (!g_app_info_launch_default_for_uri_finish (result, &error))
                g_task_return_error (md->task, error);
        else
                g_task_return_boolean (md->task, TRUE);

        mail_data_free (data);
}

static void
send_mail_using_mailto (MailData *md)
{
        g_autoptr(GString) url = NULL;
        g_autofree char *encoded_subject = NULL;
        g_autofree char *encoded_body = NULL;
        int i;
        GdkDisplay *display;
        GAppLaunchContext *context;

        encoded_subject = g_uri_escape_string (md->subject, NULL, FALSE);
        encoded_body = g_uri_escape_string (md->body, NULL, FALSE);

        url = g_string_new ("mailto:");

        g_string_append_printf (url, "\"%s\"", md->address);
        g_string_append_printf (url, "?subject=%s", encoded_subject);
        g_string_append_printf (url, "&body=%s", encoded_body);

        for (i = 0; md->attachments[i]; i++) {
                g_autofree char *path = NULL;

                path = g_uri_escape_string (md->attachments[i], NULL, FALSE);
                g_string_append_printf (url, "&attach=%s", path);
        }

        display = gtk_widget_get_display (GTK_WIDGET (md->window));
        context = G_APP_LAUNCH_CONTEXT (gdk_display_get_app_launch_context (display));

        if (md->handle)
                g_app_launch_context_setenv (context, "PARENT_WINDOW_ID", md->handle);

        g_app_info_launch_default_for_uri_async (url->str,
                                                 G_APP_LAUNCH_CONTEXT (context),
                                                 NULL,
                                                 launch_uri_done,
                                                 md);
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
compose_mail_done (GObject      *source,
                   GAsyncResult *result,
                   gpointer      data)
{
        g_autoptr(GVariant) reply = NULL;
        GError *error = NULL;
        MailData *md = data;

        reply = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), result, &error);
        if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE) ||
            g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
                g_info ("Email portal not present, falling back to mailto: url");
                send_mail_using_mailto (md);
                return;
        }

        if (error)
                g_task_return_error (md->task, error);
        else
                g_task_return_boolean (md->task, TRUE);

        mail_data_free (md);
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle_str,
                        gpointer    data)
{
        MailData *md = data;
        GVariantBuilder opt_builder;
        GDBusProxy *proxy;

        md->handle = g_strdup (handle_str);

        proxy = get_mail_portal_proxy ();

        if (proxy == NULL) {
                g_info ("Email portal not present, falling back to mailto: url");
                send_mail_using_mailto (md);
                return;
        }

        g_variant_builder_init (&opt_builder, G_VARIANT_TYPE ("a{sv}"));
        g_variant_builder_add (&opt_builder, "{sv}", "address", g_variant_new_string (md->address));
        g_variant_builder_add (&opt_builder, "{sv}", "subject", g_variant_new_string (md->subject));
        g_variant_builder_add (&opt_builder, "{sv}", "body", g_variant_new_string (md->body));
        if (md->attachments)
                g_variant_builder_add (&opt_builder, "{sv}", "attachments", g_variant_new_strv ((const char * const *)md->attachments, -1));

        g_dbus_proxy_call (proxy,
                           "ComposeEmail",
                           g_variant_new ("(sa{sv})",
                                          handle_str,
                                          &opt_builder),
                           G_DBUS_CALL_FLAGS_NONE,
                           G_MAXINT,
                           NULL,
                           compose_mail_done,
                           data);
}

void
gr_send_mail (GtkWindow   *window,
              const char  *address,
              const char  *subject,
              const char  *body,
              const char **attachments,
              GAsyncReadyCallback   callback,
              gpointer              user_data)
{
        MailData *data;
        const char *empty_strv[1] = { NULL };

        data = g_new0 (MailData, 1);
        data->window = window;
        data->address = g_strdup (address ? address : "");
        data->subject = g_strdup (subject ? subject : "");
        data->body = g_strdup (body ? body : "");
        data->attachments = g_strdupv (attachments ? (char **)attachments : (char **)empty_strv);
        data->task = g_task_new (NULL, NULL, callback, user_data);

        window_export_handle (window, window_handle_exported, data);
}

gboolean
gr_send_mail_finish (GAsyncResult  *result,
                     GError       **error)
{
        return g_task_propagate_boolean (G_TASK (result), error);
}
