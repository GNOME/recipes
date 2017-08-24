/* gr-shopping-list-exporter.c
 *
 * Copyright (C) 2017 Ekta Nandwani
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
#define GOA_API_IS_SUBJECT_TO_CHANGE

#include "config.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <goa/goa.h>
#include <rest/oauth2-proxy.h>
#include <json-glib/json-glib.h>

#include "gr-shopping-list-exporter.h"
#include "gr-recipe-store.h"
#include "gr-shopping-page.h"
#include "gr-shopping-list-formatter.h"
#include "gr-mail.h"
#include "gr-window.h"

#define TODOIST_URL "https://todoist.com/API/v7/sync"

static gboolean get_todoist_account (GrShoppingListExporter *exporter);
static gboolean get_project_id (GrShoppingListExporter *exporter);

struct _GrShoppingListExporter
{
        GObject parent_instance;
        GtkWindow *window;

        gchar *access_token;
        GoaObject *account_object;
        const gchar *sync_token;
        glong project_id;
        gboolean delete_items;

        GtkWidget *dialog;
        GtkWidget *export_button;
        GtkWidget *cancel_button;
        GtkWidget *back_button;
        GtkWidget *todoist_row;
        GtkWidget *email_row;
        GtkWidget *accounts_box;
        GtkWidget *providers_box;
        GtkWidget *dialog_stack;
        GtkWidget *header_start_stack;
        GtkWidget *header;
        GtkWidget *providers_list;
        GtkWidget *accounts_list;
        GtkWidget *account_row_selected;
        GtkWidget *todoist_provider_row;

        GList *ingredients;
};


G_DEFINE_TYPE (GrShoppingListExporter, gr_shopping_list_exporter, G_TYPE_OBJECT);

static void
gr_shopping_list_exporter_finalize (GObject *object)
{
        GrShoppingListExporter *self = GR_SHOPPING_LIST_EXPORTER (object);

        g_free (self->access_token);
        g_list_free_full (self->ingredients, g_object_unref);
        G_OBJECT_CLASS (gr_shopping_list_exporter_parent_class)->finalize (object);
}


static void
gr_shopping_list_exporter_class_init (GrShoppingListExporterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gr_shopping_list_exporter_finalize;
}

static void
gr_shopping_list_exporter_init (GrShoppingListExporter *self)
{
}

GrShoppingListExporter *
gr_shopping_list_exporter_new (GtkWindow *parent)
{
        GrShoppingListExporter *exporter;

        exporter = g_object_new (GR_TYPE_SHOPPING_LIST_EXPORTER, NULL);
        exporter->window = parent;
        return exporter;
}

static void
remove_items_callback (RestProxyCall *call,
		       GError *error,
		       GObject *obj,
		       GrShoppingListExporter *exporter)
{

	guint status_code;

	status_code = rest_proxy_call_get_status_code (call);

	if (status_code != 200) {
		g_warning ("Couldn't complete items in todoist");
	}

}

static void
remove_items (GrShoppingListExporter *exporter, GList *items)
{
	RestProxy *proxy;
	RestProxyCall *call;
	GError *error;
	g_autofree gchar *uuid = g_uuid_string_random ();

	GList *l;
	GString *commands;
	commands = g_string_new ("");
	if (exporter->delete_items)
		g_string_append_printf (commands , "[{\"type\": \"item_delete\", \"uuid\": \"%s\", \"args\": {\"ids\": [", uuid);
	else
		g_string_append_printf (commands , "[{\"type\": \"item_complete\", \"uuid\": \"%s\", \"args\": {\"ids\": [", uuid);
	
	for (l = items; l != NULL; l = l->next) {
		JsonObject *object;
		double id;
		glong item_id;

		object = json_node_get_object (l->data);
		id = json_object_get_double_member (object, "id");
		item_id = (glong) id;
		g_string_append_printf (commands, "%ld,", item_id);
	}
	commands = g_string_truncate (commands, commands->len-1);
	g_string_append_printf (commands, "]}}]");

	error = NULL;

	proxy = rest_proxy_new (TODOIST_URL, FALSE);
	call = rest_proxy_new_call (proxy);
	rest_proxy_call_set_method (call, "POST");
	rest_proxy_call_add_header (call, "content-type", "application/x-www-form-urlencoded");
	rest_proxy_call_add_param (call, "token", exporter->access_token);

	if (!exporter->sync_token)
		rest_proxy_call_add_param (call, "sync_token", "\'*\'");
	else
		rest_proxy_call_add_param (call, "sync_token", exporter->sync_token);

	rest_proxy_call_add_param (call, "commands", commands->str);

	if (!rest_proxy_call_async (call, (RestProxyCallAsyncCallback) remove_items_callback,
				    NULL, exporter, &error))
	{
	    g_warning ("Couldn't execute RestProxyCall");
	    goto out;
	}
	out:
	  g_object_unref (proxy);
	  g_object_unref (call);
}

static void
get_project_data_callback (RestProxyCall *call,
			   GError *error,
			   GObject *obj,
			   GrShoppingListExporter *exporter)
{
	JsonObject *object = NULL;
	JsonParser *parser = NULL;
	GError *parse_error;
	const gchar *payload;
	guint status_code;
	gsize payload_length;

	JsonArray *json_items;
	GList *items;

	status_code = rest_proxy_call_get_status_code (call);

	if (status_code != 200) {
		g_warning ("status code %d", status_code);
		goto out;
	}

	parser = json_parser_new ();
	payload = rest_proxy_call_get_payload (call);
	payload_length = rest_proxy_call_get_payload_length (call);
	if (!json_parser_load_from_data (parser, payload, payload_length, &parse_error)) {
		g_clear_error (&parse_error);
		g_warning ("Couldn't load payload");
		goto out;
	}

	object = json_node_dup_object (json_parser_get_root (parser));

	if (!object) {
		g_warning ("No Data found");
		goto out;
	}

	json_items = json_object_get_array_member (object, "items");
	items = json_array_get_elements (json_items);
	if (items)
	{
		remove_items (exporter, items);
	}
	out:
	  if (parser)
		g_object_unref (parser);
	  if (object)
		json_object_unref (object);
}

static void
get_project_data (GrShoppingListExporter *exporter)
{
	RestProxy *proxy;
	RestProxyCall *call;
	GError *error;
	const gchar *id;
	id = g_strdup_printf ("%ld", exporter->project_id);

	proxy = rest_proxy_new ("https://todoist.com/api/v7/projects/get_data", FALSE);
	call = rest_proxy_new_call (proxy);
	rest_proxy_call_set_method (call, "POST");
	rest_proxy_call_add_header (call, "content-type", "application/x-www-form-urlencoded");
	rest_proxy_call_add_param (call, "token", exporter->access_token);
	rest_proxy_call_add_param (call, "project_id", id);

	if (!rest_proxy_call_async (call, (RestProxyCallAsyncCallback) get_project_data_callback,
				    NULL, exporter, &error))
	{
		g_warning ("Couldn't execute RestProxyCall");
		goto out;
	}

	out:
	  g_object_unref (proxy);
	  g_object_unref (call);
}

static void
switch_dialog_contents (GrShoppingListExporter *exporter)
{
	if (gtk_stack_get_visible_child (GTK_STACK (exporter->dialog_stack)) == exporter->accounts_box) {
		if (!get_todoist_account (exporter))
			gtk_widget_set_visible (exporter->todoist_row, FALSE);
		gtk_widget_set_visible (exporter->export_button, FALSE);
		gtk_stack_set_visible_child_name (GTK_STACK (exporter->dialog_stack), "providers_box");
		gtk_stack_set_visible_child_name (GTK_STACK (exporter->header_start_stack), "back");
		gtk_header_bar_set_title (GTK_HEADER_BAR (exporter->header), _("Add Account"));
	}
	else {
		if (get_todoist_account (exporter))
			gtk_widget_set_visible (exporter->todoist_row, TRUE);
		gtk_widget_set_visible (exporter->export_button, TRUE);
		gtk_stack_set_visible_child (GTK_STACK (exporter->dialog_stack), exporter->accounts_box);
		gtk_stack_set_visible_child_name (GTK_STACK (exporter->header_start_stack), "cancel_button");
		gtk_header_bar_set_title (GTK_HEADER_BAR (exporter->header), _("Export Ingredients"));
	}
}

static void
close_dialog (GrShoppingListExporter *exporter)
{
        gtk_widget_destroy (exporter->dialog);
}

static void
export_shopping_list_callback (RestProxyCall *call,
			       GError *error,
			       GObject *obj,
			       GrShoppingListExporter *exporter)
{
	JsonObject *object;
	JsonParser *parser;
	GError *parse_error;
	const gchar *payload;
	guint status_code;
	gsize payload_length;
	const gchar *sync_token;

	parse_error = NULL;
	status_code = rest_proxy_call_get_status_code (call);
	parser = json_parser_new ();

	if (status_code != 200) {
		g_warning ("Couldn't export shopping list");
		goto out;
	}

	payload = rest_proxy_call_get_payload (call);
	payload_length = rest_proxy_call_get_payload_length (call);

	if (!json_parser_load_from_data (parser, payload, payload_length, &parse_error)) {
		g_clear_error (&parse_error);
		goto out;
	}

	object = json_node_dup_object (json_parser_get_root (parser));

	if (!object) {
		g_warning ("Export returned empty json");
		goto out;
	}
	sync_token = json_object_get_string_member (object, "sync_token");
	exporter->sync_token = sync_token;

	out:
	  g_object_unref (parser);
	  if (exporter->dialog)
		close_dialog (exporter);
	  gr_window_confirm_shopping_exported (GR_WINDOW (exporter->window));
}

static void
export_shopping_list_to_todoist (GrShoppingListExporter *exporter)
{
	RestProxy *proxy;
	RestProxyCall *call;
	GError *error;

	GList *list;
	GString *commands;
	commands = g_string_new ("");
	error = NULL;
	GString *commands_arg;

	if (exporter->ingredients) {
		for (list = exporter->ingredients; list != NULL; list = list->next) {
			GString *s;

			ShoppingListItem *item = list->data;
			s = g_string_new ("");
			g_string_append_printf (s, "%s %s", item->amount, item->name);
			g_autofree gchar *uuid = g_uuid_string_random ();
			g_autofree gchar *temp_id = g_uuid_string_random ();

			g_string_append_printf (commands, "{\"type\": \"item_add\", \"temp_id\":\"%s\", \"uuid\":\"%s\", "
			                             "\"args\":{\"content\":\"%s\",\"project_id\":%ld}},",
			                             temp_id, uuid, s->str, exporter->project_id);
		}
	}

	commands = g_string_truncate (commands, commands->len-1);
	commands_arg = g_string_new ("[");
	g_string_append_printf (commands_arg, "%s]", commands->str);

	proxy = rest_proxy_new (TODOIST_URL, FALSE);
	call = rest_proxy_new_call (proxy);
	rest_proxy_call_set_method (call, "POST");
	rest_proxy_call_add_header (call, "content-type", "application/x-www-form-urlencoded");
	rest_proxy_call_add_param (call, "token", exporter->access_token);

	if (!exporter->sync_token)
		rest_proxy_call_add_param (call, "sync_token", "\'*\'");
	else
		rest_proxy_call_add_param (call, "sync_token", exporter->sync_token);

	rest_proxy_call_add_param (call, "commands", commands_arg->str);

	if (!rest_proxy_call_async (call, (RestProxyCallAsyncCallback) export_shopping_list_callback,
				    NULL, exporter, &error))
	{
	    g_warning ("Couldn't execute RestProxyCall");
	    goto out;
	}
	out:
	  g_object_unref (proxy);
	  g_object_unref (call);

}

static void
get_selected_account (GtkListBox *list, GrShoppingListExporter *exporter)
{
	exporter->account_row_selected = GTK_WIDGET (gtk_list_box_get_selected_row (list));
}

static gboolean
get_todoist_account (GrShoppingListExporter *exporter)
{

	g_autoptr (GoaClient) client = NULL;
	g_autoptr (GList) accounts = NULL;
	GList *l;
	GoaAccount *account;
	GError *error;

	client = goa_client_new_sync (NULL, &error);

	if (!client) {
		g_warning ("Could not create GoaClient: %s", error->message);
		return 0;
	}

	accounts = goa_client_get_accounts (client);
	for (l = accounts; l != NULL; l = l->next) {
		const gchar *provider_type;

		account = goa_object_get_account (GOA_OBJECT (l->data));
		provider_type = goa_account_get_provider_name (account);

		if (g_strcmp0 (provider_type, "Todoist") == 0) {
			exporter->account_object = GOA_OBJECT (l->data);
			return TRUE;
		}
	}
	return FALSE;
}

static void
get_access_token (GrShoppingListExporter *exporter)
{

	gchar *access_token;
	GoaOAuth2Based *oauth2 = NULL;
	GError *error;

	error = NULL;
	oauth2 = goa_object_get_oauth2_based (GOA_OBJECT (exporter->account_object));

	if (!goa_oauth2_based_call_get_access_token_sync (oauth2, &access_token, NULL, NULL, &error)) {
		g_warning ("Access token not found!");
		goto out;
	}
	exporter->access_token = access_token;
	out:
          g_clear_object (&exporter->account_object);

}

void
done_shopping_in_todoist (GrShoppingListExporter *exporter)
{
	gboolean project_present = FALSE;
	exporter->delete_items = FALSE;
	if (get_todoist_account (exporter)) {
		get_access_token (exporter);
		if (!exporter->project_id)
			project_present = get_project_id (exporter);
		if (project_present)
			get_project_data (exporter);
	}
}

static void
add_project_id (GrShoppingListExporter *exporter)
{

	RestProxy *proxy;
	RestProxyCall *call;
	g_autofree gchar *uuid;
	g_autofree gchar *temp_id;
	guint status_code;
	GError *error;
	gchar *list_title = _("Shopping List from Recipes");

	JsonObject *object;
	JsonParser *parser;
	GError *parse_error;
	const gchar *payload;
	JsonArray *projects;

	gsize payload_length;
	const gchar *sync_token;
	g_autoptr (GList) lists = NULL;
	GList *l;
	GString *project_add_commands;

	project_add_commands = g_string_new ("");
	uuid = g_uuid_string_random ();
	temp_id = g_uuid_string_random ();
	proxy = rest_proxy_new (TODOIST_URL, FALSE);
	call = rest_proxy_new_call (proxy);
	rest_proxy_call_set_method (call, "POST");
	rest_proxy_call_add_header (call, "content-type", "application/x-www-form-urlencoded");
	rest_proxy_call_add_param (call, "token", exporter->access_token);
	rest_proxy_call_add_param (call, "resource_types", "[\"projects\"]" );

	if (!exporter->sync_token)
		rest_proxy_call_add_param (call, "sync_token", "\'*\'");
	else
		rest_proxy_call_add_param (call, "sync_token", exporter->sync_token);
	g_string_append_printf (project_add_commands, "[{\"type\": \"project_add\", \"temp_id\":\"%s\", \"uuid\":\"%s\", "
				"\"args\":{\"name\":\"%s\"}}]", temp_id, uuid , list_title);

	rest_proxy_call_add_param (call, "commands", project_add_commands->str);

	if (!rest_proxy_call_sync (call, &error)) {
		g_clear_error (&error);
		goto out;
	}

	status_code = rest_proxy_call_get_status_code (call);

	if (status_code != 200) {
		g_warning ("status code %d", status_code);
		goto out;
	}

	parser = json_parser_new ();
	payload = rest_proxy_call_get_payload (call);
	payload_length = rest_proxy_call_get_payload_length (call);
	if (!json_parser_load_from_data (parser, payload, payload_length, &parse_error)) {
		g_clear_error (&parse_error);
		g_warning ("Couldn't load payload");
		goto out;
	}

	object = json_node_dup_object (json_parser_get_root (parser));

	if (!object) {
		g_warning ("No Data found");
		goto out;
	}

	projects = json_object_get_array_member (object, "projects");
	lists = json_array_get_elements (projects);
	sync_token = json_object_get_string_member (object, "sync_token");
	exporter->sync_token = sync_token;

	for (l = lists; l != NULL; l = l->next) {
		JsonObject *object;
		const gchar *name;
		double id;

		object = json_node_get_object (l->data);
		name = json_object_get_string_member (object, "name");

		if (strcmp (name, list_title) == 0) {
			id = json_object_get_double_member (object, "id");
			exporter->project_id = (glong) id;
		}
	}
	out:
	  g_object_unref (proxy);
	  g_object_unref (call);
}

static gboolean
get_project_id (GrShoppingListExporter *exporter)
{
	RestProxy *proxy;
	RestProxyCall *call;
	GError *error;
	gchar *list_title = _("Shopping List from Recipes");

	JsonObject *object;
	g_autoptr (JsonParser) parser = NULL;
	GError *parse_error;
	const gchar *payload;
	guint status_code;
	gsize payload_length;
	const gchar *sync_token;
	g_autoptr (GList) lists = NULL;
	GList *l;
	JsonArray *projects;


	proxy = rest_proxy_new (TODOIST_URL, FALSE);
	call = rest_proxy_new_call (proxy);
	rest_proxy_call_set_method (call, "POST");
	rest_proxy_call_add_header (call, "content-type", "application/x-www-form-urlencoded");
	rest_proxy_call_add_param (call, "token", exporter->access_token);

	if (!exporter->sync_token)
		rest_proxy_call_add_param (call, "sync_token", "\'*\'");
	else
		rest_proxy_call_add_param (call, "sync_token", exporter->sync_token);

	rest_proxy_call_add_param (call, "resource_types", "[\"projects\"]");

	if (!rest_proxy_call_sync (call, &error)) {
		g_clear_error (&error);
		goto out;
	}

	parse_error = NULL;
	parser = json_parser_new ();
	status_code = rest_proxy_call_get_status_code (call);

	if (status_code != 200) {
		g_warning ("Couldn't export shopping list");
		goto out;
	}

	payload = rest_proxy_call_get_payload (call);
	payload_length = rest_proxy_call_get_payload_length (call);

	if (!json_parser_load_from_data (parser, payload, payload_length, &parse_error)) {
		g_clear_error (&parse_error);
		goto out;
	}

	object = json_node_dup_object (json_parser_get_root (parser));
	if (!object) {
		g_warning ("No Data found");
		goto out;
	}

	projects = json_object_get_array_member (object, "projects");
	lists = json_array_get_elements (projects);

	for (l = lists; l != NULL; l = l->next) {
		JsonObject *object;
		const gchar *name;
		double id;

		object = json_node_get_object (l->data);
		name = json_object_get_string_member (object, "name");

		if (strcmp (name, list_title) == 0) {
			id = json_object_get_double_member (object, "id");
			exporter->project_id = (glong) id;
			break;
		}
	}

	sync_token = json_object_get_string_member (object, "sync_token");
	exporter->sync_token = sync_token;

	out:
	  g_object_unref (proxy);
	  g_object_unref (call);
	  if (exporter->project_id)
		return TRUE;
	  else
		return FALSE;
}

static void
file_chooser_response (GtkNativeDialog  *self,
                       int               response_id,
                       GrShoppingListExporter   *exporter)
{
	GrRecipeStore *store;
	if (response_id == GTK_RESPONSE_ACCEPT) {
		GList *recipes, *items;
		g_autoptr (GFile) file = NULL;
		g_autofree char *text = NULL;

		store = gr_recipe_store_get ();
		recipes = gr_recipe_store_get_shopping_list (store);

		items = exporter->ingredients;
		exporter->ingredients = NULL;

		text = gr_shopping_list_format (recipes, items);

		g_list_free_full (recipes, g_object_unref);
		g_list_free_full (items, item_free);

		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self));
		g_file_replace_contents (file, text, -1, NULL, FALSE, 0, NULL, NULL, NULL);
        }
        gtk_native_dialog_destroy (self);
}

static void
mail_done (GObject      *source,
           GAsyncResult *result,
           gpointer      data)
{
        GrShoppingListExporter *exporter = data;
        g_autoptr (GError) error = NULL;

        if (!gr_send_mail_finish (result, &error)) {
                GObject *file_chooser;
                GtkWidget *window;

                g_info ("Sending mail failed: %s", error->message);

                window = gtk_widget_get_ancestor (GTK_WIDGET (exporter->dialog), GTK_TYPE_APPLICATION_WINDOW);
                file_chooser = (GObject *)gtk_file_chooser_native_new (_("Save the shopping list"),
                                                                       GTK_WINDOW (window),
                                                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                                                       _("Save"),
                                                                       _("Cancel"));
                gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (file_chooser), TRUE);

                g_signal_connect (file_chooser, "response", G_CALLBACK (file_chooser_response), exporter);
                gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser));
                return;
        }
        close_dialog (exporter);
}

static void
share_list (GrShoppingListExporter *exporter)
{
        GList *recipes, *items;
        g_autofree char *text = NULL;
        GtkWidget *window;
        GrRecipeStore *store;

        store = gr_recipe_store_get ();
        recipes = gr_recipe_store_get_shopping_list (store);
        items = exporter->ingredients;

        text = gr_shopping_list_format (recipes, items);
        window = GTK_WIDGET (exporter->window);

        gr_send_mail (GTK_WINDOW (window),
                      NULL, _("Shopping List"), text, NULL,
                      mail_done, exporter);

        g_list_free_full (recipes, g_object_unref);
        g_list_free_full (items, item_free);
}

static void
initialize_export (GrShoppingListExporter *exporter)
{
	gboolean project_present;
	exporter->delete_items = TRUE;
	if (exporter->account_row_selected == exporter->todoist_row) {
		if (!exporter->access_token) {
			if (!exporter->account_object) {
				get_todoist_account (exporter);
			}
			get_access_token (exporter);
		}
		project_present = get_project_id (exporter);
		if (project_present)
			get_project_data (exporter);
		else
			add_project_id (exporter);
		export_shopping_list_to_todoist (exporter);
	}
	else if (exporter->account_row_selected == exporter->email_row)
	{
		share_list(exporter);
	}
}

void
do_undo_in_todoist (GrShoppingListExporter *exporter, GList *items)
{
	if (!exporter->ingredients)
		exporter->ingredients = items;

	initialize_export (exporter);
}


static GVariant*
build_dbus_parameters (const gchar *action,
                       const gchar *arg)
{
	GVariantBuilder builder;
	GVariant *array[1], *params2[3];

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

	if (!action && !arg) {
		g_variant_builder_add (&builder, "v", g_variant_new_string (""));
	}
	else {
		if (action)
			g_variant_builder_add (&builder, "v", g_variant_new_string (action));
		if (arg)
			g_variant_builder_add (&builder, "v", g_variant_new_string (arg));
	}

	array[0] = g_variant_new ("v", g_variant_new ("(sav)", "online-accounts", &builder));

	params2[0] = g_variant_new_string ("launch-panel");
	params2[1] = g_variant_new_array (G_VARIANT_TYPE ("v"), array, 1);
	params2[2] = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

	return g_variant_new_tuple (params2, 3);
}

static void
add_todoist_account (GtkListBox    *list,
                     GtkListBoxRow *row,
                     GrShoppingListExporter *exporter)
{
	const char *action = "add";
	const char *arg = "todoist";
	if (GTK_WIDGET (row) == exporter->todoist_provider_row) {
		GDBusProxy *proxy;

		proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL,
						       "org.gnome.ControlCenter", "/org/gnome/ControlCenter",
						       "org.gtk.Actions", NULL, NULL);
		if (!proxy) {
			g_warning ("Couldn't open Online Accounts panel");
			return;
		}

		g_dbus_proxy_call_sync (proxy, "Activate", build_dbus_parameters (action, arg),
					G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
		g_clear_object (&proxy);
	}
}


static void
show_export_dialog (GrShoppingListExporter *exporter)
{

        g_autoptr (GtkBuilder) builder = NULL;
        GObject *add_service;
        GtkWidget *providers_list;

        builder = gtk_builder_new_from_resource ("/org/gnome/Recipes/shopping-list-exporter-dialog.ui");
        exporter->dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
        exporter->todoist_row = GTK_WIDGET (gtk_builder_get_object (builder, "todoist_account_row"));
        exporter->todoist_provider_row = GTK_WIDGET (gtk_builder_get_object (builder, "todoist_provider_row"));
        exporter->email_row = GTK_WIDGET (gtk_builder_get_object (builder, "email_account_row"));
        add_service = gtk_builder_get_object (builder, "add_service");
        providers_list = GTK_WIDGET (gtk_builder_get_object (builder, "providers_list"));

        exporter->export_button = GTK_WIDGET (gtk_builder_get_object (builder, "export_button"));
        exporter->cancel_button = GTK_WIDGET (gtk_builder_get_object (builder, "cancel_button"));
        exporter->back_button = GTK_WIDGET (gtk_builder_get_object (builder, "back_button"));
        exporter->accounts_box = GTK_WIDGET (gtk_builder_get_object (builder, "accounts_box"));
        exporter->providers_box = GTK_WIDGET (gtk_builder_get_object (builder, "providers_box"));
        exporter->dialog_stack = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_stack"));
        exporter->header_start_stack = GTK_WIDGET (gtk_builder_get_object (builder, "header_start_stack"));
        exporter->header =  GTK_WIDGET (gtk_builder_get_object (builder, "header"));
        exporter->providers_list =  GTK_WIDGET (gtk_builder_get_object (builder, "providers_list"));
        exporter->accounts_list =  GTK_WIDGET (gtk_builder_get_object (builder, "accounts_list"));

	g_signal_connect_swapped (add_service, "activate-link", G_CALLBACK (switch_dialog_contents), exporter);
        g_signal_connect_swapped (exporter->back_button, "clicked", G_CALLBACK (switch_dialog_contents), exporter);
        g_signal_connect_swapped (exporter->export_button, "clicked", G_CALLBACK (initialize_export), exporter);
        g_signal_connect_swapped (exporter->cancel_button, "clicked", G_CALLBACK (close_dialog), exporter);
        g_signal_connect (exporter->accounts_list, "selected-rows-changed", G_CALLBACK (get_selected_account), exporter);
        g_signal_connect (providers_list, "row-activated", G_CALLBACK (add_todoist_account), exporter);

        if (get_todoist_account (exporter)) {
		gtk_widget_set_visible (exporter->todoist_row, TRUE);
        }

        gtk_window_set_transient_for (GTK_WINDOW (exporter->dialog), GTK_WINDOW (exporter->window));
        gtk_widget_show (exporter->dialog);
}

void
gr_shopping_list_exporter_export (GrShoppingListExporter *exporter, GList *items)
{
        show_export_dialog (exporter);
        exporter->ingredients = items;
}
