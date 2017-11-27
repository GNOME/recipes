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

#define _GNU_SOURCE 1
#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>

#include "gr-mail.h"
#include "gr-utils.h"

#ifndef O_PATH
#define O_PATH 0
#endif

typedef struct {
        GtkWindow *window;
        char *handle;
        char *address;
        char *subject;
        char *body;
        char **attachments;
        GTask *task;
        char *response_handle;
        guint response_signal_id;
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
        g_free (md->response_handle);
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
        g_autoptr(GVariant) prop = NULL;
        guint32 version;

        if (proxy == NULL) {
                proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       "org.freedesktop.portal.Desktop",
                                                       "/org/freedesktop/portal/desktop",
                                                       "org.freedesktop.portal.Email",
                                                       NULL, NULL);
        }

        prop = g_dbus_proxy_get_cached_property (proxy, "version");
        g_variant_get (prop, "u", &version);
        if (version < 2) {
                g_info ("Email portal version too old (%d, need 2)", version);
                return NULL;
        }

        return proxy;
}

static void
compose_mail_response (GDBusConnection *connection,
                       const char *sender_name,
                       const char *object_path,
                       const char *interface_name,
                       const char *signal_name,
                       GVariant *parameters,
                       gpointer user_data)
{
        MailData *md = user_data;
        guint32 response;
        GVariant *options;

        g_variant_get (parameters, "(u@a{sv})", &response, &options);

        if (response == 0)
                g_message ("Email success");
        else if (response == 1)
                g_message ("Email canceled");
        else
                g_message ("Email error");

        if (md->response_signal_id != 0) {
                g_dbus_connection_signal_unsubscribe (connection, md->response_signal_id);
                md->response_signal_id = 0;
        }

        // FIXME: currently, the portal returns 1 when it should return 2, so we just
        // check for != 0 for now
        if (response != 0) {
                g_info ("Falling back to mailto: url");
                send_mail_using_mailto (md);
                return;
        }

        g_task_return_boolean (md->task, TRUE);
        mail_data_free (md);
}

static void
compose_mail_done (GObject      *source,
                   GAsyncResult *result,
                   gpointer      data)
{
        g_autoptr(GVariant) reply = NULL;
        GError *error = NULL;
        MailData *md = data;
        const char *handle;

        reply = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), result, &error);
        if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE) ||
            g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
                g_info ("Email portal not present, falling back to mailto: url");
                send_mail_using_mailto (md);
                return;
        }

        if (error) {
                g_task_return_error (md->task, error);
                mail_data_free (md);
                return;
        }

        g_variant_get (reply, "(&o)", &handle);
        if (strcmp (md->response_handle, handle) != 0) {
                g_free (md->response_handle);
                md->response_handle = g_strdup (handle);
                g_dbus_connection_signal_unsubscribe (g_dbus_proxy_get_connection (G_DBUS_PROXY (source)),
                                                      md->response_signal_id);

                md->response_signal_id =
                        g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (G_DBUS_PROXY (source)),
                                                            "org.freedesktop.portal.Desktop",
                                                            "org.freedesktop.portal.Request",
                                                            "Response",
                                                            handle,
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                            compose_mail_response,
                                                            md, NULL);
        }
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle_str,
                        gpointer    data)
{
        MailData *md = data;
        GVariantBuilder opt_builder;
        GDBusProxy *proxy;
        g_autofree char *token = NULL;
        g_autofree char *sender = NULL;
        int i;
        g_autoptr(GUnixFDList) fd_list = NULL;

        md->handle = g_strdup (handle_str);

        proxy = get_mail_portal_proxy ();

        if (proxy == NULL) {
                g_info ("Email portal not present, falling back to mailto: url");
                send_mail_using_mailto (md);
                return;
        }


        token = g_strdup_printf ("app%d", g_random_int_range (0, G_MAXINT));
        sender = g_strdup (g_dbus_connection_get_unique_name (g_dbus_proxy_get_connection (proxy)) + 1);
        for (i = 0; sender[i]; i++)
                if (sender[i] == '.')
                        sender[i] = '_';

        md->response_handle = g_strdup_printf ("/org/fredesktop/portal/desktop/request/%s/%s", sender, token);
        md->response_signal_id =
                g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (proxy),
                                                    "org.freedesktop.portal.Desktop",
                                                    "org.freedesktop.portal.Request",
                                                    "Response",
                                                    md->response_handle,
                                                    NULL,
                                                    G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                    compose_mail_response,
                                                    md, NULL);

        g_variant_builder_init (&opt_builder, G_VARIANT_TYPE ("a{sv}"));
        g_variant_builder_add (&opt_builder, "{sv}", "handle_token", g_variant_new_string (token));
        g_variant_builder_add (&opt_builder, "{sv}", "address", g_variant_new_string (md->address));
        g_variant_builder_add (&opt_builder, "{sv}", "subject", g_variant_new_string (md->subject));
        g_variant_builder_add (&opt_builder, "{sv}", "body", g_variant_new_string (md->body));

        if (md->attachments) {
                GVariantBuilder attach_fds;

                fd_list = g_unix_fd_list_new ();
                g_variant_builder_init (&attach_fds, G_VARIANT_TYPE ("ah"));

                for (i = 0; md->attachments[i]; i++) {
                        g_autoptr(GError) error = NULL;
                        int fd;
                        int fd_in;

                        fd = g_open (md->attachments[i], O_PATH | O_CLOEXEC);
                        if (fd == -1) {
                                g_warning ("Failed to open %s, skipping", md->attachments[i]);
                                continue;
                        }
                        fd_in = g_unix_fd_list_append (fd_list, fd, &error);
                        if (error) {
                                g_warning ("Failed to add %s to request, skipping: %s", md->attachments[i], error->message);
                                continue;
                        }
                        g_variant_builder_add (&attach_fds, "h", fd_in);
                }

                g_variant_builder_add (&opt_builder, "{sv}", "attachment_fds", g_variant_builder_end (&attach_fds));
        }

        g_dbus_proxy_call_with_unix_fd_list (proxy,
                                             "ComposeEmail",
                                             g_variant_new ("(sa{sv})",
                                                            handle_str,
                                                            &opt_builder),
                                             G_DBUS_CALL_FLAGS_NONE,
                                             G_MAXINT,
                                             fd_list,
                                             NULL,
                                             compose_mail_done,
                                             data);
}

void
gr_send_mail (GtkWindow            *window,
              const char           *address,
              const char           *subject,
              const char           *body,
              const char          **attachments,
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
