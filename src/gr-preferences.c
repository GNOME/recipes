/* gr-preferences.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include "gr-preferences.h"


struct _GrPreferences
{
        GtkDialog parent_instance;
        GtkWidget *temperature_unit;
        GSettings *settings;
};

G_DEFINE_TYPE (GrPreferences, gr_preferences, GTK_TYPE_DIALOG)

static void
preferences_finalize (GObject *object)
{
        GrPreferences *prefs = GR_PREFERENCES (object);

        g_object_unref (prefs->settings);

        G_OBJECT_CLASS (gr_preferences_parent_class)->finalize (object);
}

static void
gr_preferences_init (GrPreferences *prefs)
{
        gtk_widget_init_template (GTK_WIDGET (prefs));
        prefs->settings = g_settings_new ("org.gnome.Recipes");

        g_settings_bind (prefs->settings, "temperature-unit",
                         prefs->temperature_unit, "active-id",
                         G_SETTINGS_BIND_DEFAULT);
}

static void
gr_preferences_class_init (GrPreferencesClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = preferences_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-preferences.ui");

        gtk_widget_class_bind_template_child (widget_class, GrPreferences, temperature_unit);
}

GtkWidget *
gr_preferences_new (void)
{
        gboolean use_header;

        g_object_get (gtk_settings_get_default (), "gtk-dialogs-use-header", &use_header, NULL);
        return GTK_WIDGET (g_object_new (GR_TYPE_PREFERENCES,
                                         "use-header-bar", use_header,
                                         NULL));
}
