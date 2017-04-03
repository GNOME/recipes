/* gr-shell-search-provider.c:
 *
 * Copyright (C) 2016 Matthias Clasen
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>

#include <gio/gio.h>
#include <string.h>
#include <glib/gi18n.h>

#include "gr-shell-search-provider-dbus.h"
#include "gr-shell-search-provider.h"
#include "gr-image.h"
#include "gr-utils.h"


typedef struct {
        GrShellSearchProvider *provider;
        GDBusMethodInvocation *invocation;
        GrRecipeSearch *search;
        GList *results;
} PendingSearch;

struct _GrShellSearchProvider {
        GObject parent;

        GrShellSearchProvider2 *skeleton;
        GrRecipeStore *store;
        GCancellable *cancellable;

        GHashTable *metas_cache;
};

G_DEFINE_TYPE (GrShellSearchProvider, gr_shell_search_provider, G_TYPE_OBJECT)

static void
pending_search_free (PendingSearch *search)
{
        g_object_unref (search->invocation);
        g_object_unref (search->search);
        g_slice_free (PendingSearch, search);
}

static void
finished_cb (GrRecipeSearch *search,
             gpointer        user_data)
{
        PendingSearch *pending_search = user_data;
        GVariantBuilder builder;
        GList *l;

        if (pending_search->results == NULL) {
                g_dbus_method_invocation_return_value (pending_search->invocation, g_variant_new ("(as)", NULL));
                pending_search_free (pending_search);
                g_application_release (g_application_get_default ());
                return;
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
        for (l = pending_search->results; l; l = l->next) {
                g_variant_builder_add (&builder, "s", l->data);
        }
        g_dbus_method_invocation_return_value (pending_search->invocation, g_variant_new ("(as)", &builder));

        pending_search_free (pending_search);
        g_application_release (g_application_get_default ());
}

static void
hits_added_cb (GrRecipeSearch *search,
               GList          *hits,
               PendingSearch  *pending_search)
{
        GList *l;

        for (l = hits; l; l = l->next) {
                GrRecipe *recipe = l->data;

                pending_search->results = g_list_prepend (pending_search->results, (gpointer)gr_recipe_get_id (recipe));
        }
}

static void
execute_search (GrShellSearchProvider  *self,
                GDBusMethodInvocation  *invocation,
                gchar                 **terms)
{
        PendingSearch *pending_search;
        g_autofree gchar *string = NULL;

        string = g_strjoinv (" ", terms);

        if (self->cancellable != NULL) {
                g_cancellable_cancel (self->cancellable);
                g_clear_object (&self->cancellable);
        }

        /* don't attempt searches for a single character */
        if (g_strv_length (terms) == 1 &&
            g_utf8_strlen (terms[0], -1) == 1) {
                g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
                return;
        }

        pending_search = g_slice_new (PendingSearch);
        pending_search->provider = self;
        pending_search->invocation = g_object_ref (invocation);
        pending_search->results = NULL;

        g_application_hold (g_application_get_default ());
        self->cancellable = g_cancellable_new ();

        pending_search->search = gr_recipe_search_new ();
        g_signal_connect (pending_search->search, "hits-added",
                          G_CALLBACK (hits_added_cb), pending_search);
        g_signal_connect (pending_search->search, "finished",
                          G_CALLBACK (finished_cb), pending_search);

        gr_recipe_search_set_query (pending_search->search, string);
}

static gboolean
handle_get_initial_result_set (GrShellSearchProvider2  *skeleton,
                               GDBusMethodInvocation   *invocation,
                               gchar                  **terms,
                               gpointer                 user_data)
{
        GrShellSearchProvider *self = user_data;

        g_debug ("****** GetInitialResultSet");
        execute_search (self, invocation, terms);
        return TRUE;
}

static gboolean
handle_get_subsearch_result_set (GrShellSearchProvider2  *skeleton,
                                 GDBusMethodInvocation   *invocation,
                                 gchar                  **previous_results,
                                 gchar                  **terms,
                                 gpointer                 user_data)
{
        GrShellSearchProvider *self = user_data;

        g_debug ("****** GetSubSearchResultSet");
        execute_search (self, invocation, terms);
        return TRUE;
}

static GdkPixbuf *
gr_recipe_get_pixbuf (GrRecipe *recipe)
{
        GPtrArray *images;

        images = gr_recipe_get_images (recipe);
        if (images->len > 0) {
                int index = gr_recipe_get_default_image (recipe);
                GrImage *ri = g_ptr_array_index (images, index);
                return gr_image_load_sync (ri, 64, 64, FALSE);
        }

        return NULL;
}

static gboolean
handle_get_result_metas (GrShellSearchProvider2 *skeleton,
                         GDBusMethodInvocation   *invocation,
                         gchar                  **results,
                         gpointer                 user_data)
{
        GrShellSearchProvider *self = user_data;
        GVariantBuilder meta;
        GVariant *meta_variant;
        GdkPixbuf *pixbuf;
        gint i;
        GVariantBuilder builder;

        g_debug ("****** GetResultMetas");

        for (i = 0; results[i]; i++) {
                g_autoptr(GrRecipe) recipe = NULL;

                if (g_hash_table_lookup (self->metas_cache, results[i]))
                        continue;

                recipe = gr_recipe_store_get_recipe (self->store, results[i]);
                if (recipe == NULL) {
                        g_warning ("failed to find %s", results[i]);
                        continue;
                }

                g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));
                g_variant_builder_add (&meta, "{sv}", "id", g_variant_new_string (gr_recipe_get_id (recipe)));
                g_variant_builder_add (&meta, "{sv}", "name", g_variant_new_string (gr_recipe_get_translated_name (recipe)));
                pixbuf = gr_recipe_get_pixbuf (recipe);
                if (pixbuf != NULL)
                        g_variant_builder_add (&meta, "{sv}", "icon", g_icon_serialize (G_ICON (pixbuf)));
                g_variant_builder_add (&meta, "{sv}", "description", g_variant_new_string (gr_recipe_get_translated_description (recipe)));
                meta_variant = g_variant_builder_end (&meta);
                g_hash_table_insert (self->metas_cache, g_strdup (gr_recipe_get_id (recipe)), g_variant_ref_sink (meta_variant));

        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));
        for (i = 0; results[i]; i++) {
                meta_variant = (GVariant*)g_hash_table_lookup (self->metas_cache, results[i]);
                if (meta_variant == NULL)
                        continue;
                g_variant_builder_add_value (&builder, meta_variant);
        }

        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(aa{sv})", &builder));

        return TRUE;
}

static gboolean
handle_activate_result (GrShellSearchProvider2  *skeleton,
                        GDBusMethodInvocation   *invocation,
                        gchar                   *result,
                        gchar                  **terms,
                        guint32                  timestamp,
                        gpointer                 user_data)
{
        GApplication *app = g_application_get_default ();
        g_autofree gchar *string = NULL;

        string = g_strjoinv (" ", terms);

        g_action_group_activate_action (G_ACTION_GROUP (app), "details",
                                        g_variant_new ("(ss)", result, string));

        gr_shell_search_provider2_complete_activate_result (skeleton, invocation);

        return TRUE;
}

static gboolean
handle_launch_search (GrShellSearchProvider2  *skeleton,
                      GDBusMethodInvocation   *invocation,
                      gchar                  **terms,
                      guint32                  timestamp,
                      gpointer                 user_data)
{
        GApplication *app = g_application_get_default ();
        g_autofree gchar *string = g_strjoinv (" ", terms);

        g_action_group_activate_action (G_ACTION_GROUP (app), "search",
                                        g_variant_new ("s", string));

        gr_shell_search_provider2_complete_launch_search (skeleton, invocation);

        return TRUE;
}

gboolean
gr_shell_search_provider_register (GrShellSearchProvider  *self,
                                   GDBusConnection        *connection,
                                   GError                **error)
{
        return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                                 connection,
                                                 "/org/gnome/Recipes/SearchProvider",
                                                 error);
}

void
gr_shell_search_provider_unregister (GrShellSearchProvider *self)
{
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->skeleton));
}

static void
search_provider_dispose (GObject *obj)
{
        GrShellSearchProvider *self = GR_SHELL_SEARCH_PROVIDER (obj);

        if (self->cancellable != NULL) {
                g_cancellable_cancel (self->cancellable);
                g_clear_object (&self->cancellable);
        }

        if (self->metas_cache != NULL) {
                g_hash_table_destroy (self->metas_cache);
                self->metas_cache = NULL;
        }

        g_clear_object (&self->store);
        g_clear_object (&self->skeleton);

        G_OBJECT_CLASS (gr_shell_search_provider_parent_class)->dispose (obj);
}

static void
gr_shell_search_provider_init (GrShellSearchProvider *self)
{
        self->store = g_object_ref (gr_recipe_store_get ());
        self->metas_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free, (GDestroyNotify) g_variant_unref);

        self->skeleton = gr_shell_search_provider2_skeleton_new ();

        g_signal_connect (self->skeleton, "handle-get-initial-result-set",
                          G_CALLBACK (handle_get_initial_result_set), self);
        g_signal_connect (self->skeleton, "handle-get-subsearch-result-set",
                          G_CALLBACK (handle_get_subsearch_result_set), self);
        g_signal_connect (self->skeleton, "handle-get-result-metas",
                          G_CALLBACK (handle_get_result_metas), self);
        g_signal_connect (self->skeleton, "handle-activate-result",
                          G_CALLBACK (handle_activate_result), self);
        g_signal_connect (self->skeleton, "handle-launch-search",
                          G_CALLBACK (handle_launch_search), self);
}

static void
gr_shell_search_provider_class_init (GrShellSearchProviderClass *klass)
{
        GObjectClass *oclass = G_OBJECT_CLASS (klass);

        oclass->dispose = search_provider_dispose;
}

GrShellSearchProvider *
gr_shell_search_provider_new (void)
{
        return g_object_new (gr_shell_search_provider_get_type (), NULL);
}
