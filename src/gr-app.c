/* gr-app.c:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
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

#include <stdlib.h>

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "gr-app.h"
#include "gr-window.h"
#include "gr-chef-dialog.h"
#include "gr-cuisine.h"
#include "gr-shell-search-provider.h"
#include "gr-utils.h"
#include "gr-logging.h"


struct _GrApp
{
        GtkApplication parent_instance;

        SoupSession *session;
        GrShellSearchProvider *search_provider;
        GtkCssProvider *css_provider;
};

G_DEFINE_TYPE (GrApp, gr_app, GTK_TYPE_APPLICATION)


static void
gr_app_finalize (GObject *object)
{
        GrApp *self = GR_APP (object);

        g_clear_object (&self->session);
        g_clear_object (&self->search_provider);
        g_clear_object (&self->css_provider);

        G_OBJECT_CLASS (gr_app_parent_class)->finalize (object);
}

static void
gr_app_activate (GApplication *app)
{
        GtkWindow *win;
        gboolean new_window = FALSE;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        if (!win) {
                win = GTK_WINDOW (gr_window_new (GR_APP (app)));
                new_window = TRUE;
        }

        gtk_window_present (win);

        if (new_window)
                gr_window_show_surprise (GR_WINDOW (win));
}

static void
timer_expired (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       application)
{
        GrApp *app = GR_APP (application);
        GtkWindow *win;
        const char *id;
        int step;
        g_autoptr(GrRecipe) recipe = NULL;

        g_variant_get (parameter, "(&si)", &id, &step);

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        recipe = gr_recipe_store_get_recipe (gr_recipe_store_get (), id);
        gr_window_timer_expired (GR_WINDOW (win), recipe, step);
}

static void
chef_information_activated (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       app)
{
        GtkWindow *win;

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_my_chef_information (GR_WINDOW (win));
}

static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       app)
{
        GtkWindow *win;

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_about_dialog (GR_WINDOW (win));
}

static void
news_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
        GtkWindow *win;

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_news (GR_WINDOW (win));
}

static void
import_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       app)
{
        GtkWindow *win;

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_load_recipe (GR_WINDOW (win), NULL);
}

static void
export_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       app)
{
        GtkWindow *win;

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_save_all (GR_WINDOW (win));
}

static void
details_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       application)
{
        GrApp *app = GR_APP (application);
        GtkWindow *win;
        const char *id, *search;
        g_autoptr(GrRecipe) recipe = NULL;

        g_variant_get (parameter, "(&s&s)", &id, &search);

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        recipe = gr_recipe_store_get_recipe (gr_recipe_store_get (), id);
        gr_window_show_recipe (GR_WINDOW (win), recipe);
}

static void
category_activated (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       application)
{
        GrApp *app = GR_APP (application);
        GrWindow *win;
        const char *category;

        g_variant_get (parameter, "&s", &category);

        gr_app_activate (G_APPLICATION (app));
        win = GR_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (app)));
        if (g_strcmp0 (category, "mine") == 0)
                gr_window_show_mine (win);
        else if (g_strcmp0 (category, "favorites") == 0)
                gr_window_show_favorites (win);
        else if (g_strcmp0 (category, "all") == 0)
                gr_window_show_all (win);
        else if (g_strcmp0 (category, "new") == 0)
                gr_window_show_new (win);
        else if (g_strcmp0 (category, "gluten-free") == 0)
                gr_window_show_diet (win, GR_DIET_GLUTEN_FREE);
        else if (g_strcmp0 (category, "nut-free") == 0)
                gr_window_show_diet (win, GR_DIET_NUT_FREE);
        else if (g_strcmp0 (category, "vegan") == 0)
                gr_window_show_diet (win, GR_DIET_VEGAN);
        else if (g_strcmp0 (category, "vegetarian") == 0)
                gr_window_show_diet (win, GR_DIET_VEGETARIAN);
        else if (g_strcmp0 (category, "milk-free") == 0)
                gr_window_show_diet (win, GR_DIET_MILK_FREE);
        else
                g_print (_("Supported categories: %s\n"),
                           "mine, favorites, all, "
                           "new, gluten-free, nut-free, "
                           "vegan, vegetarian, milk-free");
}

static void
search_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       application)
{
        GrApp *app = GR_APP (application);
        GtkWindow *win;
        const char *search;

        g_variant_get (parameter, "&s", &search);

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_search (GR_WINDOW (win), search);
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
        GtkWindow *win;

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gtk_window_close (win);
}

static void
report_issue_activated (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       app)
{
        GtkWindow *win;

        gr_app_activate (G_APPLICATION (app));
        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_report_issue (GR_WINDOW (win));
}

static void
help_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
        GList uris = { (gpointer)"help:org.gnome.Recipes", NULL, NULL };
        g_autoptr(GAppInfo) info = NULL;
        g_autoptr(GError) error = NULL;

        info = g_app_info_get_default_for_uri_scheme ("help");
        if (info == NULL)
                g_warning ("Failed to get a launcher for the URI scheme 'help:'");
        else if (!g_app_info_launch_uris (info, &uris, NULL, &error))
                g_warning ("Failed to launch %s: %s", g_app_info_get_name (info), error->message);
}

static void
verbose_logging_activated (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       application)
{
        gboolean verbose;

        g_variant_get (parameter, "b", &verbose);
        gr_set_verbose_logging (verbose);
}

static GActionEntry app_entries[] =
{
        { "timer-expired", timer_expired, "(si)", NULL, NULL },
        { "chef-information", chef_information_activated, NULL, NULL, NULL },
        { "about", about_activated, NULL, NULL, NULL },
        { "news", news_activated, NULL, NULL, NULL },
        { "import", import_activated, NULL, NULL, NULL },
        { "export", export_activated, NULL, NULL, NULL },
        { "details", details_activated, "(ss)", NULL, NULL },
        { "category", category_activated, "s", NULL, NULL },
        { "search", search_activated, "s", NULL, NULL },
        { "quit", quit_activated, NULL, NULL, NULL },
        { "report-issue", report_issue_activated, NULL, NULL, NULL },
        { "help", help_activated, NULL, NULL, NULL },
        { "verbose-logging", verbose_logging_activated, "b", NULL, NULL }
};

static void
setup_actions_and_accels (GApplication *application)
{
	struct {
		const char *detailed_action;
		const char *accelerators[2];
	} accels[] = {
		{ "app.quit",       { "<Primary>q", NULL } },
		{ "app.search('')", { "<Primary>f", NULL } },
		{ "win.copy",       { "<Primary>c", NULL } },
		{ "win.paste",      { "<Primary>v", NULL } },
	};
        int i;

        g_action_map_add_action_entries (G_ACTION_MAP (application),
                                         app_entries, G_N_ELEMENTS (app_entries),
                                         application);

#ifndef ENABLE_AUTOAR
        {
                GAction *action;

                action = g_action_map_lookup_action (G_ACTION_MAP (application), "import");
                g_object_set (action, "enabled", FALSE, NULL);
        }
#endif

        for (i = 0; i < G_N_ELEMENTS (accels); i++)
                gtk_application_set_accels_for_action (GTK_APPLICATION (application),
                                                       accels[i].detailed_action,
                                                       accels[i].accelerators);
}

static void
load_application_menu (GApplication *application)
{
        g_autoptr(GtkBuilder) builder = NULL;
        GObject *menu;

        builder = gtk_builder_new_from_resource ("/org/gnome/Recipes/menus.ui");
        if (strcmp (G_OBJECT_TYPE_NAME (gdk_display_get_default ()), "GdkQuartzDisplay") == 0) {
                menu = gtk_builder_get_object (builder, "menubar");
                gtk_application_set_menubar (GTK_APPLICATION (application), G_MENU_MODEL (menu));
        }
        else {
                menu = gtk_builder_get_object (builder, "app-menu");
                gtk_application_set_app_menu (GTK_APPLICATION (application), G_MENU_MODEL (menu));
        }
}

static void
load_application_css (GApplication *application)
{
        GrApp *app = GR_APP (application);
        gboolean dark;
        g_autofree char *path = NULL;
        g_autofree char *css = NULL;

        if (!app->css_provider) {
                app->css_provider = gtk_css_provider_new ();
                g_signal_connect_swapped (gtk_settings_get_default (), "notify::gtk-application-prefer-dark-theme",
                                          G_CALLBACK (load_application_css), app);
        }

        g_object_get (gtk_settings_get_default (),
                      "gtk-application-prefer-dark-theme", &dark,
                      NULL);

        path = g_strdup_printf ("resource:///org/gnome/Recipes/recipes-%s.css", dark ? "dark" : "light");
        g_info ("Loading application CSS from %s", path);

        css = gr_cuisine_get_css (path);

        gtk_css_provider_load_from_data (app->css_provider, css, -1, NULL);
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (app->css_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
gr_app_startup (GApplication *application)
{
        G_APPLICATION_CLASS (gr_app_parent_class)->startup (application);

        setup_actions_and_accels (application);
        load_application_menu (application);
        load_application_css (application);
}

static void
gr_app_open (GApplication  *application,
             GFile        **files,
             gint           n_files,
             const char    *hint)
{
        GrApp *app = GR_APP (application);
        GtkWindow *win;

        if (n_files > 1)
                g_warning ("Can only open one file at a time.");

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        if (!win) {
                win = GTK_WINDOW (gr_window_new (app));
                gtk_window_present (win);
        }

        gr_window_load_recipe (GR_WINDOW (win), files[0]);

        gtk_window_present (win);
}

static gboolean
gr_app_dbus_register (GApplication     *application,
                      GDBusConnection  *connection,
                      const gchar      *object_path,
                      GError          **error)
{
        GrApp *app = GR_APP (application);

        app->search_provider = gr_shell_search_provider_new ();

        return gr_shell_search_provider_register (app->search_provider, connection, error);
}

static void
gr_app_dbus_unregister (GApplication    *application,
                        GDBusConnection *connection,
                        const gchar     *object_path)
{
        GrApp *app = GR_APP (application);

        if (app->search_provider != NULL) {
                gr_shell_search_provider_unregister (app->search_provider);
                g_clear_object (&app->search_provider);
        }
}

static void
gr_app_init (GrApp *self)
{
        g_application_add_main_option (G_APPLICATION (self),
                                       "version", 'v',
                                       G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
                                       _("Print the version and exit"), NULL);
        g_application_add_main_option (G_APPLICATION (self),
                                       "verbose", 0,
                                       G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
                                       _("Turn on verbose logging"), NULL);
        g_application_add_main_option (G_APPLICATION (self),
                                       "category", 0,
                                       G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING,
                                       _("Show a category"), ("CATEGORY"));

        g_log_set_writer_func (gr_log_writer, NULL, NULL);

        self->session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT,
                                                       PACKAGE_NAME "/" PACKAGE_VERSION,
                                                       NULL);
}

static int
gr_app_handle_local_options (GApplication *app,
                             GVariantDict *options)
{
        gboolean value;
        const char *category;

        if (g_variant_dict_lookup (options, "version", "b", &value)) {
                g_print ("%s %s\n", PACKAGE_NAME, get_version ());
                return 0;
        }

        if (g_variant_dict_lookup (options, "verbose", "b", &value)) {
                g_autoptr(GError) error = NULL;

                if (!g_application_register (app, NULL, &error)) {
                        g_printerr ("Failed to register: %s\n", error->message);
                        return 1;
                }

                g_action_group_activate_action (G_ACTION_GROUP (app),
                                                "verbose-logging",
                                                g_variant_new_boolean (TRUE));
        }

        if (g_variant_dict_lookup (options, "category", "&s", &category)) {
                g_autoptr(GError) error = NULL;

                if (!g_application_register (app, NULL, &error)) {
                        g_printerr ("Failed to register: %s\n", error->message);
                        return 1;
                }

                g_action_group_activate_action (G_ACTION_GROUP (app),
                                                "category",
                                                g_variant_new_string (category));
        }

        return -1;
}

static void
gr_app_class_init (GrAppClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

        object_class->finalize = gr_app_finalize;

        application_class->startup = gr_app_startup;
        application_class->activate = gr_app_activate;
        application_class->handle_local_options = gr_app_handle_local_options;
        application_class->open = gr_app_open;
        application_class->dbus_register = gr_app_dbus_register;
        application_class->dbus_unregister = gr_app_dbus_unregister;
}

GrApp *
gr_app_new (void)
{
        return g_object_new (GR_TYPE_APP,
                             "application-id", "org.gnome.Recipes",
                             "flags", G_APPLICATION_HANDLES_OPEN | G_APPLICATION_CAN_OVERRIDE_APP_ID,
                             NULL);
}

SoupSession *
gr_app_get_soup_session (GrApp *app)
{
        return app->session;
}
