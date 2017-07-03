/* gr-account.c:
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

#include "gr-account.h"
#include "gr-recipe-store.h"
#include "gr-utils.h"

#include <glib/gi18n.h>

typedef struct {
        GtkWindow *window;
        AccountInformationCallback callback;
        gpointer data;
        GDestroyNotify destroy;
        GDBusConnection *connection;
        guint response_id;
        char *handle;
} CallbackData;

static void
free_callback_data (gpointer data)
{
        CallbackData *cbdata = data;

        window_unexport_handle (cbdata->window);

        if (cbdata->destroy)
                cbdata->destroy (cbdata->data);

        if (cbdata->response_id)
                g_dbus_connection_signal_unsubscribe (cbdata->connection, cbdata->response_id);

        g_clear_object (&cbdata->connection);
        g_free (cbdata->handle);

        g_free (cbdata);
}

static void
account_response (GDBusConnection *connection,
                  const char      *sender_name,
                  const char      *object_path,
                  const char      *interface_name,
                  const char      *signal_name,
                  GVariant        *parameters,
                  gpointer         user_data)
{
        CallbackData *cbdata = user_data;
        guint32 response;
        GVariant *options;

        g_variant_get (parameters, "(u@a{sv})", &response, &options);

        if (response == 0) {
                const char *id;
                const char *name;
                const char *uri;
                g_autofree char *image_path = NULL;

                g_variant_lookup (options, "id", "&s", &id);
                g_variant_lookup (options, "name", "&s", &name);
                g_variant_lookup (options, "image", "&s", &uri);

                if (uri && uri[0]) {
                        g_autoptr(GFile) source = NULL;
                        g_autoptr(GFile) dest = NULL;
                        g_autofree char *orig_dest = NULL;
                        g_autofree char *destpath = NULL;
                        int i;

                        source = g_file_new_for_uri (uri);
                        orig_dest = g_build_filename (get_user_data_dir (), id, NULL);
                        destpath = g_strdup (orig_dest);
                        for (i = 1; i < 10; i++) {
                                if (!g_file_test (destpath, G_FILE_TEST_EXISTS))
                                        break;
                                g_free (destpath);
                                destpath = g_strdup_printf ("%s%d", orig_dest, i);
                        }
                        dest = g_file_new_for_path (destpath);
                        if (g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL))
                                image_path = g_strdup (destpath);
                }

                cbdata->callback (id, name, image_path, cbdata->data, NULL);
        }
        else {
                g_autoptr(GError) error = NULL;

                g_info ("Got an error from the Account portal");
                g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Got an error from Account portal"));

                cbdata->callback (NULL, NULL, NULL, cbdata->data, error);
        }

        free_callback_data (cbdata);
}

static void
window_handle_exported (GtkWindow  *window,
                        const char *handle_str,
                        gpointer    user_data)
{
        CallbackData *cbdata = user_data;
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) ret = NULL;
        const char *handle;
        GVariantBuilder opt_builder;
        g_autofree char *token = NULL;
        g_autofree char *sender = NULL;
        int i;

        cbdata->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (!cbdata->connection) {
                g_info ("Could not talk to D-Bus: %s", error->message);
                cbdata->callback (NULL, NULL, NULL, cbdata->data, error);
                free_callback_data (cbdata);
                return;
	}

        token = g_strdup_printf ("app%d", g_random_int_range (0, G_MAXINT));
        sender = g_strdup (g_dbus_connection_get_unique_name (cbdata->connection) + 1);
        for (i = 0; sender[i]; i++)
                if (sender[i] == '.')
                        sender[i] = '_';

        cbdata->handle = g_strdup_printf ("/org/fredesktop/portal/desktop/request/%s/%s", sender, token);

        cbdata->response_id =
                g_dbus_connection_signal_subscribe (cbdata->connection,
                                                    "org.freedesktop.portal.Desktop",
                                                    "org.freedesktop.portal.Request",
                                                    "Response",
                                                    cbdata->handle,
                                                    NULL,
                                                    G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                    account_response,
                                                    user_data, NULL);

        g_variant_builder_init (&opt_builder, G_VARIANT_TYPE ("a{sv}"));
        g_variant_builder_add (&opt_builder, "{sv}", "handle_token", g_variant_new_string (token));
        g_variant_builder_add (&opt_builder, "{sv}", "reason", g_variant_new_string (_("Allow your personal information to be included with recipes you share with your friends.")));

        ret = g_dbus_connection_call_sync (cbdata->connection,
                                           "org.freedesktop.portal.Desktop",
                                           "/org/freedesktop/portal/desktop",
                                           "org.freedesktop.portal.Account",
                                           "GetUserInformation",
                                           g_variant_new ("(sa{sv})", handle_str, &opt_builder),
                                           G_VARIANT_TYPE ("(o)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           G_MAXINT,
                                           NULL,
                                           &error);

        if (!ret) {
                g_info ("Could not talk to Account portal: %s", error->message);
                cbdata->callback (NULL, NULL, NULL, cbdata->data, error);
                free_callback_data (cbdata);
                return;
        }

        g_variant_get (ret, "(&o)", &handle);

        if (strcmp (cbdata->handle, handle) != 0) {
                g_free (cbdata->handle);
                cbdata->handle = g_strdup (handle);
                g_dbus_connection_signal_unsubscribe (cbdata->connection, cbdata->response_id);
                cbdata->response_id =
                        g_dbus_connection_signal_subscribe (cbdata->connection,
                                                            "org.freedesktop.portal.Desktop",
                                                            "org.freedesktop.portal.Request",
                                                            "Response",
                                                            handle,
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                            account_response,
                                                            user_data, NULL);
        }
}

void
gr_account_get_information (GtkWindow                  *window,
                            AccountInformationCallback  callback,
                            gpointer                    data,
                            GDestroyNotify              destroy)
{
        CallbackData *cbdata;

        cbdata = g_new (CallbackData, 1);

        cbdata->window = window;
        cbdata->callback = callback;
        cbdata->data = data;
        cbdata->destroy = destroy;

        window_export_handle (window, window_handle_exported, cbdata);
}

typedef struct {
        UserChefCallback callback;
        gpointer data;
} UserChefData;

static void
got_account_info (const char  *id,
                  const char  *name,
                  const char  *image_path,
                  gpointer     data,
                  GError      *error)
{
        UserChefData *cbdata = data;
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;
        g_autoptr(GError) local_error = NULL;

        store = gr_recipe_store_get ();

        if (error) {
                g_info ("Failed to get account information: %s", error->message);
		id = g_get_user_name ();
		name = g_get_real_name ();
		image_path = NULL;
	}

        chef = gr_chef_new ();
        g_object_set (chef,
                      "id", id,
                      "fullname", name,
                      "image-path", image_path,
                      NULL);

        if (!gr_recipe_store_update_user (store, chef, &local_error))
                g_warning ("Failed to update chef for user: %s", local_error->message);

        if (cbdata->callback)
                cbdata->callback (chef, cbdata->data);
        g_free (cbdata);
}

void
gr_ensure_user_chef (GtkWindow        *window,
                     UserChefCallback  callback,
                     gpointer          data)
{
        GrRecipeStore *store;
        g_autoptr(GrChef) chef = NULL;
        UserChefData *cbdata;

        store = gr_recipe_store_get ();

        chef = gr_recipe_store_get_chef (store, gr_recipe_store_get_user_key (store));
        if (chef) {
                if (callback)
                        callback (chef, data);
                return;
        }

        cbdata = g_new0 (UserChefData, 1);
        cbdata->callback = callback;
        cbdata->data = data;

        gr_account_get_information (window, got_account_info, cbdata, NULL);
}
