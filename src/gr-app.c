/* gr-app.c
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
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


struct _GrApp
{
        GtkApplication parent_instance;

        GrRecipeStore *store;
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
                NULL
        };
        g_autoptr(GdkPixbuf) logo = NULL;

        logo = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                         "org.gnome.Recipes",
                                         256,
                                         GTK_ICON_LOOKUP_FORCE_SIZE,
                                         NULL);

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        gtk_show_about_dialog (GTK_WINDOW (win),
                               "program-name", "GNOME Recipes",
                               "version", PACKAGE_VERSION,
                               "copyright", "© 2016 Matthias Clasen",
                               "license-type", GTK_LICENSE_GPL_3_0,
                               "comments", _("GNOME loves to cook"),
                               "authors", authors,
                               "logo", logo,
                               "title", _("About GNOME Recipes"),
                               NULL);

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
        const char *name;
        g_autoptr(GrRecipe) recipe = NULL;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        name = g_variant_get_string (parameter, NULL);
        recipe = gr_recipe_store_get (GR_APP (app)->store, name);
        if (recipe)
                gr_window_show_recipe (GR_WINDOW (win), recipe);
        gtk_window_present (win);
}

static GActionEntry app_entries[] =
{
        { "preferences", preferences_activated, NULL, NULL, NULL },
        { "about", about_activated, NULL, NULL, NULL },
        { "timer-expired", timer_expired, "s", NULL, NULL },
        { "quit", quit_activated, NULL, NULL, NULL }
};

static void
gr_app_startup (GApplication *app)
{
        const gchar *quit_accels[2] = { "<Ctrl>Q", NULL };
        g_autoptr(GtkCssProvider) css_provider = NULL;
        g_autofree char *css = NULL;

        G_APPLICATION_CLASS (gr_app_parent_class)->startup (app);

        g_action_map_add_action_entries (G_ACTION_MAP (app),
                                         app_entries, G_N_ELEMENTS (app_entries),
                                         app);
        gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                               "app.quit",
                                               quit_accels);

        css_provider = gtk_css_provider_new ();
        if (g_file_test ("recipes.css", G_FILE_TEST_EXISTS))
          gtk_css_provider_load_from_path (css_provider, "recipes.css", NULL);
        else
          gtk_css_provider_load_from_resource (css_provider, "/org/gnome/Recipes/recipes.css");
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
gr_app_activate (GApplication *app)
{
        GtkWindow *win;

        win = gtk_application_get_active_window (GTK_APPLICATION (app));
        if (!win)
                win = GTK_WINDOW (gr_window_new (GR_APP (app)));
        gtk_window_present (win);
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
}

GrApp *
gr_app_new (void)
{
        return g_object_new (GR_TYPE_APP,
                             "application-id", "org.gnome.Recipes",
                             NULL);
}

GrRecipeStore *
gr_app_get_recipe_store (GrApp *app)
{
        return app->store;
}
