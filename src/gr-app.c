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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-app.h"
#include "gr-window.h"
#include "gr-preferences.h"
#include "gr-recipe-store.h"
#include "gr-cuisine.h"
#include "gr-shell-search-provider.h"

struct _GrApp
{
        GtkApplication parent_instance;

        GrRecipeStore *store;
        GrShellSearchProvider *search_provider;
};

G_DEFINE_TYPE (GrApp, gr_app, GTK_TYPE_APPLICATION)


static void
gr_app_finalize (GObject *object)
{
        GrApp *self = GR_APP (object);

        g_clear_object (&self->store);

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
preferences_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       app)
{
        GrPreferences *prefs;
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        prefs = gr_preferences_new (win);
        gtk_window_present (GTK_WINDOW (prefs));
}

static GtkWidget *
find_child_with_name (GtkWidget  *parent,
                      const char *name)
{
        GList *children, *l;
        GtkWidget *result = NULL;

        children = gtk_container_get_children (GTK_CONTAINER (parent));
        for (l = children; l; l = l->next) {
                GtkWidget *child = l->data;

                if (g_strcmp0 (gtk_buildable_get_name (GTK_BUILDABLE (child)), name) == 0) {
                        result = child;
                        break;
                }
        }
        g_list_free (children);

        if (result == NULL)
                g_warning ("Didn't find %s in GtkAboutDialog\n", name);
        return result;
}

static void
add_built_logo (GtkAboutDialog *about)
{
        GtkWidget *content;
        GtkWidget *box;
        GtkWidget *stack;
        GtkWidget *page_vbox;
        GtkWidget *license_label;
        GtkWidget *copyright_label;
        GtkWidget *image;

        content = gtk_dialog_get_content_area (GTK_DIALOG (about));
        box = find_child_with_name (content, "box");
        stack = find_child_with_name (box, "stack");
        page_vbox = find_child_with_name (stack, "page_vbox");
        license_label = find_child_with_name (page_vbox, "license_label");
        copyright_label = find_child_with_name (page_vbox, "copyright_label");

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_show (box);
        image = gtk_image_new_from_resource ("/org/gnome/Recipes/built-with-builder.png");
        gtk_widget_set_valign (image, GTK_ALIGN_END);
        gtk_widget_show (image);
        gtk_box_pack_start (GTK_BOX (box), image, FALSE, TRUE, 0);

        g_object_ref (license_label);
        g_object_ref (copyright_label);

        gtk_container_remove (GTK_CONTAINER (page_vbox), license_label);
        gtk_container_remove (GTK_CONTAINER (page_vbox), copyright_label);

        gtk_box_pack_start (GTK_BOX (box), license_label, TRUE, TRUE, 0);
        gtk_label_set_justify (GTK_LABEL (license_label), GTK_JUSTIFY_LEFT);
        gtk_widget_set_valign (license_label, GTK_ALIGN_END);

        gtk_container_add (GTK_CONTAINER (page_vbox), copyright_label);
        gtk_container_add (GTK_CONTAINER (page_vbox), box);

        g_object_unref (license_label);
        g_object_unref (copyright_label);
}

static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       app)
{
        GtkWindow *win;
        const char *authors[] = {
                "Emel Elvin Yıldız",
                "Matthias Clasen",
                "Jakub Steiner",
                "Christian Hergert",
                "Matthew Leeds",
                "Mohammed Sadiq",
                "Sam Hewitt",
                NULL
        };
        const char *recipe_authors[] = {
                "Ray Strode",
                "Bastian Ilsø",
                "Frederik Fyksen",
                NULL
        };

        g_autoptr(GdkPixbuf) logo = NULL;
        static gboolean first_time = TRUE;

        logo = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                         "org.gnome.Recipes",
                                         256,
                                         GTK_ICON_LOOKUP_FORCE_SIZE,
                                         NULL);

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gtk_show_about_dialog (GTK_WINDOW (win),
                               "program-name", "GNOME Recipes",
#if MICRO_VERSION % 2 == 1
                               "version", COMMIT_ID,
#else
                               "version", PACKAGE_VERSION,
#endif
                               "copyright", "© 2016 Matthias Clasen",
                               "license-type", GTK_LICENSE_GPL_3_0,
                               "comments", _("GNOME loves to cook"),
                               "authors", authors,
                               "translator-credits", _("translator-credits"),
                               "logo", logo,
                               "title", _("About GNOME Recipes"),
                               "website", "https://wiki.gnome.org/Apps/Recipes",
                               "website-label", _("Learn more about GNOME Recipes"),
                               NULL);

        if (first_time) {
                GtkAboutDialog *dialog;

                first_time = FALSE;

                dialog = GTK_ABOUT_DIALOG (g_object_get_data (G_OBJECT (win), "gtk-about-dialog"));
                gtk_about_dialog_add_credit_section (dialog, _("Recipes by"), recipe_authors);
                add_built_logo (dialog);
        }

}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
        g_application_quit (G_APPLICATION (app));
}

static void
timer_expired (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       app)
{
        GtkWindow *win;
        const char *id;
        g_autoptr(GrRecipe) recipe = NULL;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        id = g_variant_get_string (parameter, NULL);
        recipe = gr_recipe_store_get_recipe (GR_APP (app)->store, id);
        if (recipe)
                gr_window_show_recipe (GR_WINDOW (win), recipe);
        gtk_window_present (win);
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

static GActionEntry app_entries[] =
{
        { "preferences", preferences_activated, NULL, NULL, NULL },
        { "about", about_activated, NULL, NULL, NULL },
        { "import", import_activated, NULL, NULL, NULL },
        { "details", details_activated, "(ss)", NULL, NULL },
        { "search", search_activated, "s", NULL, NULL },
        { "timer-expired", timer_expired, "s", NULL, NULL },
        { "quit", quit_activated, NULL, NULL, NULL }
};

static void
gr_app_startup (GApplication *app)
{
        const gchar *quit_accels[2] = { "<Ctrl>Q", NULL };
        g_autoptr(GtkCssProvider) css_provider = NULL;
        g_autoptr(GFile) file = NULL;
        g_autofree char *css = NULL;
        const char *path;

        G_APPLICATION_CLASS (gr_app_parent_class)->startup (app);

        g_action_map_add_action_entries (G_ACTION_MAP (app),
                                         app_entries, G_N_ELEMENTS (app_entries),
                                         app);
        gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                               "app.quit",
                                               quit_accels);

        css_provider = gtk_css_provider_new ();
        if (g_file_test ("recipes.css", G_FILE_TEST_EXISTS)) {
                path = "recipes.css";
                file = g_file_new_for_path (path);
        }
        else if (g_file_test ("src/recipes.css", G_FILE_TEST_EXISTS)) {
                path = "src/recipes.css";
                file = g_file_new_for_path (path);
        }
        else {
                path = "resource:///org/gnome/Recipes/recipes.css";
                file = g_file_new_for_uri (path);
        }
        g_message ("Load CSS from: %s", path);
        gtk_css_provider_load_from_file (css_provider, file, NULL);
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (css_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (css_provider);

        css_provider = gtk_css_provider_new ();
        css = gr_cuisine_get_css ();
        gtk_css_provider_load_from_data (css_provider, css, -1, NULL);
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (css_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
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
        if (!win)
                win = GTK_WINDOW (gr_window_new (GR_APP (app)));

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
        self->store = gr_recipe_store_new ();
}

static void
gr_app_class_init (GrAppClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

        object_class->finalize = gr_app_finalize;

        application_class->startup = gr_app_startup;
        application_class->activate = gr_app_activate;
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
