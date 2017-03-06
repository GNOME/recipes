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
#include "gr-recipe-store.h"
#include "gr-cuisine.h"
#include "gr-shell-search-provider.h"
#include "gr-utils.h"
#include "gr-recipe-exporter.h"


struct _GrApp
{
        GtkApplication parent_instance;

        GrRecipeStore *store;
        GrShellSearchProvider *search_provider;
        GrRecipeExporter *exporter;

        GtkCssProvider *css_provider;
};

G_DEFINE_TYPE (GrApp, gr_app, GTK_TYPE_APPLICATION)


static void
gr_app_finalize (GObject *object)
{
        GrApp *self = GR_APP (object);

        g_clear_object (&self->store);
        g_clear_object (&self->search_provider);
        g_clear_object (&self->css_provider);

        G_OBJECT_CLASS (gr_app_parent_class)->finalize (object);
}

static void
gr_app_activate (GApplication *app)
{
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        if (!win)
                win = GTK_WINDOW (gr_window_new (GR_APP (app)));
        gtk_window_present (win);
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
        recipe = gr_recipe_store_get_recipe (app->store, id);
        gr_window_timer_expired (GR_WINDOW (win), recipe, step);
}

static void
chef_information_activated (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       app)
{
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_my_chef_information (GR_WINDOW (win));
}

static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       app)
{
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_about_dialog (GR_WINDOW (win));
}

static void
report_issue_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       app)
{
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_report_issue (GR_WINDOW (win));
}

static void
news_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_window_show_news (GR_WINDOW (win));
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
        g_application_quit (G_APPLICATION (app));
}

static void
import_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       app)
{
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gtk_window_present (win);
        gr_window_load_recipe (GR_WINDOW (win), NULL);
}

static void
export_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       app)
{
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gtk_window_present (win);
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
        recipe = gr_recipe_store_get_recipe (app->store, id);
        gr_window_show_recipe (GR_WINDOW (win), recipe);
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

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gr_app_activate (G_APPLICATION (app));
        gr_window_show_search (GR_WINDOW (win), search);
}

#define DEFAULT_LEVELS (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE)
#define INFO_LEVELS (G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)

static gboolean verbose_logging;

/* verbose_logging turns enables DEBUG and INFO messages just for our log domain.
 * We also respect the G_MESSAGES_DEBUG environment variable.
 */
static GLogWriterOutput
log_writer (GLogLevelFlags   log_level,
            const GLogField *fields,
            gsize            n_fields,
            gpointer         user_data)
{
        if (!(log_level & DEFAULT_LEVELS)) {
                const gchar *domains, *log_domain = NULL;
                gsize i;

                domains = g_getenv ("G_MESSAGES_DEBUG");

                if (verbose_logging && domains == NULL)
                        domains = G_LOG_DOMAIN;

                if ((log_level & INFO_LEVELS) == 0 || domains == NULL)
                        return G_LOG_WRITER_HANDLED;

                for (i = 0; i < n_fields; i++) {
                        if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0) {
                                log_domain = fields[i].value;
                                break;
                        }
                }

                if (!verbose_logging || strcmp (log_domain, G_LOG_DOMAIN) != 0) {
                        if (strcmp (domains, "all") != 0 &&
                            (log_domain == NULL || !strstr (domains, log_domain)))
                                return G_LOG_WRITER_HANDLED;
                }
        }

        if (g_log_writer_is_journald (fileno (stderr)) &&
            g_log_writer_journald (log_level, fields, n_fields, user_data) == G_LOG_WRITER_HANDLED)
                goto handled;

        if (g_log_writer_standard_streams (log_level, fields, n_fields, user_data) == G_LOG_WRITER_HANDLED)
                goto handled;

        return G_LOG_WRITER_UNHANDLED;

handled:
        if (log_level & G_LOG_LEVEL_ERROR)
                g_abort ();

        return G_LOG_WRITER_HANDLED;
}

static void
verbose_logging_activated (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       application)
{
        g_variant_get (parameter, "b", &verbose_logging);
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
        { "search", search_activated, "s", NULL, NULL },
        { "quit", quit_activated, NULL, NULL, NULL },
        { "report-issue", report_issue_activated, NULL, NULL, NULL },
        { "verbose-logging", verbose_logging_activated, "b", NULL, NULL }
};

static void
load_application_css (GrApp *app)
{
        gboolean dark;
        const char *css_file;
        const char *src_file;
        const char *resource;
        const char *path;
        g_autofree char *css = NULL;

        if (!app->css_provider) {
                app->css_provider = gtk_css_provider_new ();
                g_signal_connect_swapped (gtk_settings_get_default (), "notify::gtk-application-prefer-dark-theme",
                                          G_CALLBACK (load_application_css), app);
        }

        g_object_get (gtk_settings_get_default (),
                      "gtk-application-prefer-dark-theme", &dark,
                      NULL);

        if (dark) {
                css_file = "recipes-dark.css";
                src_file = "src/recipes-dark.css";
                resource = "resource:///org/gnome/Recipes/recipes-dark.css";
        }
        else {
                css_file = "recipes-light.css";
                src_file = "src/recipes-light.css";
                resource = "resource:///org/gnome/Recipes/recipes-light.css";
        }

        if (g_file_test (css_file, G_FILE_TEST_EXISTS))
                path = css_file;
        else if (g_file_test (src_file, G_FILE_TEST_EXISTS))
                path = src_file;
        else
                path = resource;

        g_info ("Loading application CSS from %s", path);

        css = gr_cuisine_get_css (path);

        gtk_css_provider_load_from_data (app->css_provider, css, -1, NULL);
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (app->css_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
gr_app_startup (GApplication *app)
{
        const gchar *quit_accels[2] = { "<Primary>Q", NULL };
        const gchar *search_accels[2] = { "<Primary>F", NULL };

        G_APPLICATION_CLASS (gr_app_parent_class)->startup (app);

        g_action_map_add_action_entries (G_ACTION_MAP (app),
                                         app_entries, G_N_ELEMENTS (app_entries),
                                         app);

#ifndef ENABLE_AUTOAR
        {
                GAction *action;

                action = g_action_map_lookup_action (G_ACTION_MAP (app), "import");
                g_object_set (action, "enabled", FALSE, NULL);
        }
#endif

        gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
        gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.search('')", search_accels);

        load_application_css (GR_APP (app));
}

static void
gr_app_open (GApplication  *app,
             GFile        **files,
             gint           n_files,
             const char    *hint)
{
        GtkWindow *win;

        if (n_files > 1)
                g_warning ("Can only open one file at a time.");

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        if (!win) {
                win = GTK_WINDOW (gr_window_new (GR_APP (app)));
                gtk_window_present (win);
        }

        gr_window_load_recipe (GR_WINDOW (win), files[0]);

        gtk_window_present (win);
}

static gboolean
gr_app_dbus_register (GApplication    *application,
                      GDBusConnection *connection,
                      const gchar     *object_path,
                      GError         **error)
{
        GrApp *app = GR_APP (application);

        app->search_provider = gr_shell_search_provider_new ();
        gr_shell_search_provider_setup (app->search_provider, app->store);

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

        g_log_set_writer_func (log_writer, NULL, NULL);

        self->store = gr_recipe_store_new ();

}

static int
gr_app_handle_local_options (GApplication *app,
                             GVariantDict *options)
{
        gboolean value;

        if (g_variant_dict_lookup (options, "version", "b", &value)) {
                g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
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

GrRecipeStore *
gr_app_get_recipe_store (GrApp *app)
{
        return app->store;
}
