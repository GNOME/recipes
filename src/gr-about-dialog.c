/* gr-about-dialog.c:
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

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "gr-about-dialog.h"
#include "gr-utils.h"
#include "gr-recipe-store.h"

#include <stdlib.h>

#ifdef GDK_WINDOWING_QUARTZ
#include <sys/sysctl.h>
#endif

struct _GrAboutDialog
{
        GtkAboutDialog parent_instance;

        GtkWidget *logo_image;
};

G_DEFINE_TYPE (GrAboutDialog, gr_about_dialog, GTK_TYPE_ABOUT_DIALOG)

static void
gr_about_dialog_finalize (GObject *object)
{
        G_OBJECT_CLASS (gr_about_dialog_parent_class)->finalize (object);
}

static void
gr_about_dialog_init (GrAboutDialog *self)
{
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "about");
}

static void
gr_about_dialog_class_init (GrAboutDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_about_dialog_finalize;
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
builder_info (GtkButton *button,
              GtkWidget *about)
{
        const char *uri = "http://wiki.gnome.org/Apps/Builder";
        g_autoptr(GError) error = NULL;

        gtk_show_uri_on_window (GTK_WINDOW (about), uri, GDK_CURRENT_TIME, &error);
        if (error)
                g_warning ("Unable to show '%s': %s", uri, error->message);
}

static void
add_built_logo (GrAboutDialog *about)
{
        GtkWidget *content;
        GtkWidget *box;
        GtkWidget *stack;
        GtkWidget *page_vbox;
        GtkWidget *license_label;
        GtkWidget *copyright_label;
        GtkWidget *button;
        GtkWidget *image;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        g_autoptr(GtkCssProvider) provider = NULL;
        g_autofree char *css = NULL;
        const char *path;

        content = gtk_dialog_get_content_area (GTK_DIALOG (about));
        box = find_child_with_name (content, "box");
        stack = find_child_with_name (box, "stack");
        page_vbox = find_child_with_name (stack, "page_vbox");
        license_label = find_child_with_name (page_vbox, "license_label");
        copyright_label = find_child_with_name (page_vbox, "copyright_label");

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_show (box);
        button = gtk_button_new ();
        g_signal_connect (button, "clicked", G_CALLBACK (builder_info), about);
        gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
        gtk_box_pack_start (GTK_BOX (box), button, FALSE, TRUE, 0);
        gtk_widget_set_valign (button, GTK_ALIGN_END);
        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text (button, _("Learn more about Builder"));
        gtk_widget_show (button);

        image = g_object_new (g_type_from_name ("GtkIcon"), NULL);
        path = "/org/gnome/Recipes/built-with-builder-symbolic.symbolic.png";
        pixbuf = gdk_pixbuf_new_from_resource (path, NULL);
        css = g_strdup_printf (".built-with-builder {\n"
              "  -gtk-icon-source: -gtk-recolor(url('resource://%s'));\n"
              "  min-width: %dpx;\n"
              "  min-height: %dpx;\n"
              "}", path, gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf));
        gtk_style_context_add_class (gtk_widget_get_style_context (image), "built-with-builder");
        provider = gtk_css_provider_new ();
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (provider), 900);
        gtk_css_provider_load_from_data (provider, css, -1, NULL);

        gtk_widget_show (image);
        gtk_container_add (GTK_CONTAINER (button), image);

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

#ifndef GDK_WINDOWING_QUARTZ
static char *
find_string_between (const char *buffer,
                     const char *before,
                     const char *after)
{
        char *start, *end;

        start = end = NULL;
        if ((start = strstr (buffer, before)) != NULL) {
                start += strlen (before);
                end = strstr (start, after);
        }

        if (start != NULL && end != NULL)
                return g_strndup (start, end - start);

        return NULL;
}
#endif

static void
get_os_information (char **os_name,
                    char **os_type,
                    char **desktop,
                    char **version)
{
        *os_name = NULL;
        *os_type = NULL;
        *desktop = NULL;
        *version = NULL;

        if (GLIB_SIZEOF_VOID_P == 8)
                *os_type = g_strdup ("64-bit");
        else
                *os_type = g_strdup ("32-bit");

#ifdef GDK_WINDOWING_QUARTZ
	{
	char str[256];
	size_t size = sizeof(str);

	sysctlbyname ("kern.osrelease", str, &size, NULL, 0);
	*os_name = g_strdup_printf ("OS X %s", str);
	}
#else
	{
        g_autofree char *buffer;
        g_autofree char *buffer2;
        const char *value;

        if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
                *os_name = find_string_between (buffer, "PRETTY_NAME=\"", "\"");

        value = g_getenv ("XDG_CURRENT_DESKTOP");
        if (value) {
                g_auto(GStrv) strv = g_strsplit (value, ":", 0);
                *desktop = g_strdup (strv[0]);
        }

        if (g_file_get_contents ("/usr/share/gnome/gnome-version.xml", &buffer2, NULL, NULL)) {
                g_autofree char *platform = NULL;
                g_autofree char *minor = NULL;
                g_autofree char *micro = NULL;

                platform = find_string_between (buffer2, "<platform>", "</platform>");
                minor = find_string_between (buffer2, "<minor>", "</minor>");
                micro = find_string_between (buffer2, "<micro>", "</micro>");

                if (platform && minor && micro)
                        *version = g_strconcat (platform, ".", minor, ".", micro, NULL);
        }
	}
#endif

        if (!*os_name)
                *os_name = g_strdup (_("Unknown"));
}

static void
get_flatpak_information (char **flatpak_version,
                         char **app_id,
                         char **app_arch,
                         char **app_branch,
                         char **app_commit,
                         char **runtime_id,
                         char **runtime_arch,
                         char **runtime_branch,
                         char **runtime_commit)
{
        g_autoptr(GKeyFile) keyfile = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *path = NULL;
        char *p;
        g_auto(GStrv) strv = NULL;

        *flatpak_version = NULL;
        *app_id = NULL;
        *app_arch = NULL;
        *app_branch = NULL;
        *app_commit = NULL;
        *runtime_id = NULL;
        *runtime_arch = NULL;
        *runtime_branch = NULL;
        *runtime_commit = NULL;

        keyfile = g_key_file_new ();
        if (!g_key_file_load_from_file (keyfile, "/.flatpak-info", G_KEY_FILE_NONE, &error)) {
                goto error;
        }

        *flatpak_version = g_key_file_get_string (keyfile, "Instance", "flatpak-version", &error);
        if (*flatpak_version == NULL)
                goto error;

        path = g_key_file_get_string (keyfile, "Instance", "app-path", &error);
        if (path == NULL)
                goto error;

        p = strstr (path, "app/");
        if (p == NULL)
                goto error;

        p += strlen ("app/");
        strv = g_strsplit (p, "/", -1);

        if (g_strv_length (strv) < 4)
                goto error;

        *app_id = g_strdup (strv[0]);
        *app_arch = g_strdup (strv[1]);
        *app_branch = g_strdup (strv[2]);
        *app_commit = g_strdup (strv[3]);

        path = g_key_file_get_string (keyfile, "Instance", "runtime-path", &error);
        if (path == NULL)
                goto error;

        p = strstr (path, "runtime/");
        if (p == NULL)
                goto error;

        p += strlen ("runtime/");
        strv = g_strsplit (p, "/", -1);

        if (g_strv_length (strv) < 4)
                goto error;

        *runtime_id = g_strdup (strv[0]);
        *runtime_arch = g_strdup (strv[1]);
        *runtime_branch = g_strdup (strv[2]);
        *runtime_commit = g_strdup (strv[3]);

        return;

error:
        g_message ("Failed to load runtime information: %s", error ? error->message : "");

        if (!*flatpak_version)
                *flatpak_version = g_strdup (_("Unknown"));
        if (!*app_id)
                *app_id = g_strdup (_("Unknown"));
        if (!*app_arch)
                *app_arch = g_strdup (_("Unknown"));
        if (!*app_branch)
                *app_branch = g_strdup (_("Unknown"));
        if (!*app_commit)
                *app_commit = g_strdup (_("Unknown"));
        if (!*runtime_id)
                *runtime_id = g_strdup (_("Unknown"));
        if (!*runtime_arch)
                *runtime_arch = g_strdup (_("Unknown"));
        if (!*runtime_branch)
                *runtime_branch = g_strdup (_("Unknown"));
        if (!*runtime_commit)
                *runtime_commit = g_strdup (_("Unknown"));
}

static void
text_buffer_append (GtkTextBuffer *buffer,
                    const char    *text)
{
        GtkTextIter iter;

        gtk_text_buffer_get_end_iter (buffer, &iter);
        gtk_text_buffer_insert (buffer, &iter, text, -1);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
text_buffer_append_printf (GtkTextBuffer *buffer,
                           const char    *format,
                           ...)
{
        va_list args;
        char *buf;
        int len;

        va_start (args, format);
        len = g_vasprintf (&buf, format, args);
        if (len >= 0) {
                text_buffer_append (buffer, buf);
                g_free (buf);
        }
        va_end (args);
}
#pragma GCC diagnostic pop

static void
text_buffer_append_link (GtkTextView   *view,
                         GtkTextBuffer *buffer,
                         const char    *name,
                         const char    *uri)
{
        GdkRGBA color;
        GtkTextTag *tag;
        GtkTextIter iter;
        GtkStateFlags state = gtk_widget_get_state_flags (GTK_WIDGET (view));
        GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (view));

        gtk_style_context_get_color (context, state | GTK_STATE_FLAG_LINK, &color);

        tag = gtk_text_buffer_create_tag (buffer, NULL,
                                          "foreground-rgba", &color,
                                          "underline", PANGO_UNDERLINE_SINGLE,
                                          NULL);
        g_object_set_data_full (G_OBJECT (tag), "uri", g_strdup (uri), g_free);
        gtk_text_buffer_get_end_iter (buffer, &iter);
        gtk_text_buffer_insert_with_tags (buffer, &iter, name, -1, tag, NULL);
}


static void
populate_system_tab (GtkTextView *view)
{
        GtkTextBuffer *buffer;
        PangoTabArray *tabs;
        GtkTextIter start, end;

        tabs = pango_tab_array_new (3, TRUE);
        pango_tab_array_set_tab (tabs, 0, PANGO_TAB_LEFT, 20);
        pango_tab_array_set_tab (tabs, 1, PANGO_TAB_LEFT, 150);
        pango_tab_array_set_tab (tabs, 2, PANGO_TAB_LEFT, 230);
        gtk_text_view_set_tabs (view, tabs);
        pango_tab_array_free (tabs);

        buffer = gtk_text_view_get_buffer (view);

        if (in_flatpak_sandbox ()) {
                g_autofree char *flatpak_version = NULL;
                g_autofree char *app_id = NULL;
                g_autofree char *app_arch = NULL;
                g_autofree char *app_branch = NULL;
                g_autofree char *app_commit = NULL;
                g_autofree char *runtime_id = NULL;
                g_autofree char *runtime_arch = NULL;
                g_autofree char *runtime_branch = NULL;
                g_autofree char *runtime_commit = NULL;

                get_flatpak_information (&flatpak_version,
                                         &app_id, &app_arch, &app_branch, &app_commit,
                                         &runtime_id, &runtime_arch, &runtime_branch, &runtime_commit);

                text_buffer_append (buffer, "\n");
                text_buffer_append (buffer, _("Flatpak"));
                text_buffer_append (buffer, "\n");

                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "Version"), flatpak_version);

                text_buffer_append (buffer, "\n");
                text_buffer_append (buffer, _("Application"));
                text_buffer_append (buffer, "\n");

                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "ID"), app_id);
                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "Architecture"), app_arch);
                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "Branch"), app_branch);
                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "Commit"), app_commit);

                text_buffer_append (buffer, "\n");
                text_buffer_append (buffer, _("Runtime"));
                text_buffer_append (buffer, "\n");

                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "ID"), runtime_id);
                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "Architecture"), runtime_arch);
                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "Branch"), runtime_branch);
                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("Flatpak metadata", "Commit"), runtime_commit);

                text_buffer_append (buffer, "\n");
                text_buffer_append (buffer, _("Bundled libraries"));
                text_buffer_append (buffer, "\n");

#ifdef ENABLE_AUTOAR
                text_buffer_append_printf (buffer, "\tgnome-autoar\t%s\t", AUTOAR_VERSION);
                text_buffer_append_link (view, buffer, "LGPLv2", "http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html");
                text_buffer_append (buffer, "\n");
#endif
#ifdef ENABLE_GSPELL
                text_buffer_append_printf (buffer, "\tgspell\t%s\t", GSPELL_VERSION);
                text_buffer_append_link (view, buffer, "LGPLv2", "http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html");
                text_buffer_append (buffer, "\n");
#endif
#ifdef ENABLE_CANBERRA
                text_buffer_append_printf (buffer, "\tlibcanberra\t%s\t", CANBERRA_VERSION);
                text_buffer_append_link (view, buffer, "LGPLv2", "http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html");
                text_buffer_append (buffer, "\n");
#endif
                text_buffer_append_printf (buffer, "\tlibgd\t%s\t", LIBGD_INFO);
                text_buffer_append_link (view, buffer, "LGPLv2", "http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html");
                text_buffer_append (buffer, "\n");
        }
        else {
                g_autofree char *os_name = NULL;
                g_autofree char *os_type = NULL;
                g_autofree char *desktop = NULL;
                g_autofree char *version = NULL;

                get_os_information (&os_name, &os_type, &desktop, &version);

                text_buffer_append (buffer, _("OS"));
                text_buffer_append (buffer, "\n");

                text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("OS metadata", "Name"), os_name);
		if (os_type)
                	text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("OS metadata", "Type"), os_type);

                if (desktop && version)
                        text_buffer_append_printf (buffer, "\t%s\t%s %s\n", C_("OS metadata", "Desktop"), desktop, version);
                else if (desktop)
                        text_buffer_append_printf (buffer, "\t%s\t%s\n", C_("OS metadata", "Desktop"), desktop);

                text_buffer_append (buffer, "\n");
                text_buffer_append (buffer, _("System libraries"));
                text_buffer_append (buffer, "\n");
                text_buffer_append_printf (buffer, "\tGLib\t%d.%d.%d\n",
                                           glib_major_version,
                                           glib_minor_version,
                                           glib_micro_version);
                text_buffer_append_printf (buffer, "\tGTK+\t%d.%d.%d\n",
                                           gtk_get_major_version (),
                                           gtk_get_minor_version (),
                                           gtk_get_micro_version ());
#ifdef ENABLE_AUTOAR
                text_buffer_append_printf (buffer, "\tgnome-autoar\t%s\n", AUTOAR_VERSION);
#endif
#ifdef ENABLE_GSPELL
                text_buffer_append_printf (buffer, "\tgspell\t%s\n", GSPELL_VERSION);
#endif
#ifdef ENABLE_CANBERRA
                text_buffer_append_printf (buffer, "\tlibcanberra\t%s\n", CANBERRA_VERSION);
#endif

                text_buffer_append (buffer, "\n");
                text_buffer_append (buffer, _("Bundled libraries"));
                text_buffer_append (buffer, "\n");

                text_buffer_append_printf (buffer, "\tlibgd\t%s\t", LIBGD_INFO);
                text_buffer_append_link (view, buffer, "LGPLv2", "http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html");
                text_buffer_append (buffer, "\n");
       }

        gtk_text_buffer_create_tag (buffer, "smaller", "scale", PANGO_SCALE_SMALL, NULL);
        gtk_text_buffer_get_bounds (buffer, &start, &end);
        gtk_text_buffer_apply_tag_by_name (buffer, "smaller", &start, &end);
}

static void
follow_if_link (GrAboutDialog *about,
                GtkTextView   *text_view,
                GtkTextIter   *iter)
{
        GSList *tags = NULL, *tagp = NULL;
        gchar *uri = NULL;

        tags = gtk_text_iter_get_tags (iter);
        for (tagp = tags; tagp != NULL && !uri; tagp = tagp->next) {
                GtkTextTag *tag = tagp->data;

                uri = g_object_get_data (G_OBJECT (tag), "uri");
                if (uri)
                        gtk_show_uri_on_window (GTK_WINDOW (about), uri, GDK_CURRENT_TIME, NULL);
        }

        g_slist_free (tags);
}

static gboolean
text_view_key_press_event (GtkWidget     *text_view,
                           GdkEventKey   *event,
                           GrAboutDialog *about)
{
        GtkTextIter iter;
        GtkTextBuffer *buffer;

        switch (event->keyval)
        {
        case GDK_KEY_Return:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_KP_Enter:
                buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
                gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                                  gtk_text_buffer_get_insert (buffer));
                follow_if_link (about, GTK_TEXT_VIEW (text_view), &iter);
                break;

        default:
                break;
        }

        return FALSE;
}

static gboolean
text_view_event_after (GtkWidget     *text_view,
                       GdkEvent      *event,
                       GrAboutDialog *about)
{
        GtkTextIter start, end, iter;
        GtkTextBuffer *buffer;
        GdkEventButton *button_event;
        gint x, y;

        if (event->type != GDK_BUTTON_RELEASE)
                return FALSE;

        button_event = (GdkEventButton *)event;

        if (button_event->button != GDK_BUTTON_PRIMARY)
                return FALSE;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

        /* we shouldn't follow a link if the user has selected something */
        gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
        if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
                return FALSE;

        gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                               GTK_TEXT_WINDOW_WIDGET,
                                               button_event->x, button_event->y, &x, &y);

        gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y);

        follow_if_link (about, GTK_TEXT_VIEW (text_view), &iter);

        return FALSE;
}

static void
set_cursor_if_appropriate (GrAboutDialog *about,
                           GtkTextView   *text_view,
                           GdkDevice     *device,
                           gint           x,
                           gint           y)
{
        GSList *tags = NULL, *tagp = NULL;
        GtkTextIter iter;
        gboolean hovering_over_link = FALSE;
        gboolean was_hovering = FALSE;

        gtk_text_view_get_iter_at_location (text_view, &iter, x, y);

        tags = gtk_text_iter_get_tags (&iter);
        for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
                GtkTextTag *tag = tagp->data;
                gchar *uri = g_object_get_data (G_OBJECT (tag), "uri");

                if (uri != NULL) {
                        hovering_over_link = TRUE;
                        break;
                }
        }

        was_hovering = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (about), "hovering-over-link"));
        if (hovering_over_link != was_hovering) {
                GdkCursor *cursor;

                g_object_set_data (G_OBJECT (about), "hovering-over-link", GINT_TO_POINTER (hovering_over_link));

                if (hovering_over_link)
                        cursor = GDK_CURSOR (g_object_get_data (G_OBJECT (about), "pointer-cursor"));
                else
                        cursor = GDK_CURSOR (g_object_get_data (G_OBJECT (about), "text-cursor"));

                gdk_window_set_device_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), device, cursor);
        }

        g_slist_free (tags);
}

static gboolean
text_view_motion_notify_event (GtkWidget      *text_view,
                               GdkEventMotion *event,
                               GrAboutDialog  *about)
{
        gint x, y;

        gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                               GTK_TEXT_WINDOW_WIDGET,
                                               event->x, event->y, &x, &y);

        set_cursor_if_appropriate (about, GTK_TEXT_VIEW (text_view), event->device, x, y);

        gdk_event_request_motions (event);

        return FALSE;
}

static void
toggle_system (GtkToggleButton *button,
               gpointer         user_data)
{
        GtkAboutDialog *about = user_data;
        GtkWidget *credits_button;
        GtkWidget *stack;
        gboolean show_system;
        guint signal_id;

        if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (about), "in_page_changed")))
                return;

        stack = GTK_WIDGET (g_object_get_data (G_OBJECT (about), "stack"));
        credits_button = GTK_WIDGET (g_object_get_data (G_OBJECT (about), "credits_button"));

        signal_id = g_signal_lookup ("toggled", GTK_TYPE_TOGGLE_BUTTON);
        g_signal_handlers_block_matched (credits_button,
                                         G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                                         signal_id, 0, NULL, NULL, about);

        show_system = gtk_toggle_button_get_active (button);
        gtk_stack_set_visible_child_name (GTK_STACK (stack), show_system ? "system" : "main");

        g_signal_handlers_unblock_matched (credits_button,
                                           G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                                           signal_id, 0, NULL, NULL, about);
}

static void
page_changed (GObject *stack, GParamSpec *pspec, gpointer user_data)
{
        GrAboutDialog *about = user_data;
        GtkWidget *credits_button;
        GtkWidget *system_button;
        const char *visible;

        if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (about), "in_page_changed")))
                return;

        g_object_set_data (G_OBJECT (about), "in_page_changed", GINT_TO_POINTER (TRUE));

        credits_button = GTK_WIDGET (g_object_get_data (G_OBJECT (about), "credits_button"));
        system_button = GTK_WIDGET (g_object_get_data (G_OBJECT (about), "system_button"));

        visible = gtk_stack_get_visible_child_name (GTK_STACK (stack));

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (credits_button),
                                      strcmp (visible, "credits") == 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (system_button),
                                      strcmp (visible, "system") == 0);

        g_object_set_data (G_OBJECT (about), "in_page_changed", GINT_TO_POINTER (FALSE));
}

static void
page_changed_for_image (GObject *stack, GParamSpec *pspec, gpointer user_data)
{
        GrAboutDialog *about = user_data;
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (stack));

        if (strcmp (visible, "credits") == 0 || strcmp (visible, "system") == 0)
                gtk_style_context_add_class (gtk_widget_get_style_context (about->logo_image), "small");
        else
                gtk_style_context_remove_class (gtk_widget_get_style_context (about->logo_image), "small");

}

static void
tweak_image (GrAboutDialog *about)
{
        GtkWidget *content;
        GtkWidget *box;
        GtkWidget *stack;
        GtkWidget *image;
        GtkWidget *name_label;
        GtkWidget *page_vbox;

        content = gtk_dialog_get_content_area (GTK_DIALOG (about));
        box = find_child_with_name (content, "box");
        image = find_child_with_name (box, "logo_image");
        gtk_style_context_add_class (gtk_widget_get_style_context (image), "logo-image");
        about->logo_image = image;
        gtk_image_clear (GTK_IMAGE (image));

        stack = find_child_with_name (box, "stack");
        page_vbox = find_child_with_name (stack, "page_vbox");
        gtk_widget_set_valign (page_vbox, GTK_ALIGN_END);

        name_label = find_child_with_name (box, "name_label");
        g_object_ref (name_label);
        gtk_container_remove (GTK_CONTAINER (box), name_label);
        gtk_box_pack_start (GTK_BOX (page_vbox), name_label, FALSE, TRUE, 0);
        gtk_box_reorder_child (GTK_BOX (page_vbox), name_label, 0);
        g_object_unref (name_label);

        gtk_box_set_spacing (GTK_BOX (box), 0);

        g_signal_connect (stack, "notify::visible-child-name", G_CALLBACK (page_changed_for_image), about);
}

static void
add_system_tab (GrAboutDialog *about)
{
        GtkWidget *content;
        GtkWidget *box;
        GtkWidget *stack;
        GtkWidget *sw;
        GtkWidget *view;
        GdkCursor *cursor;
        gboolean use_header_bar;

        content = gtk_dialog_get_content_area (GTK_DIALOG (about));
        box = find_child_with_name (content, "box");
        stack = find_child_with_name (box, "stack");

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                             GTK_SHADOW_IN);
        gtk_widget_show (sw);
        view = gtk_text_view_new ();
        gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
        gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 10);
        gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 10);
        gtk_text_view_set_top_margin (GTK_TEXT_VIEW (view), 10);
        gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (view), 10);

        cursor = gdk_cursor_new_from_name (gdk_display_get_default (), "pointer");
        g_object_set_data_full (G_OBJECT (about), "pointer-cursor", cursor, g_object_unref);

        cursor = gdk_cursor_new_from_name (gdk_display_get_default (), "text");
        g_object_set_data_full (G_OBJECT (about), "text-cursor", cursor, g_object_unref);

        g_signal_connect (view, "event-after", G_CALLBACK (text_view_event_after), about);
        g_signal_connect (view, "key-press-event", G_CALLBACK (text_view_key_press_event), about);
        g_signal_connect (view, "motion-notify-event", G_CALLBACK (text_view_motion_notify_event), about);
        gtk_widget_show (view);
        gtk_container_add (GTK_CONTAINER (sw), view);

        gtk_stack_add_titled (GTK_STACK (stack), sw, "system", _("System"));

        populate_system_tab (GTK_TEXT_VIEW (view));

        g_object_get (about, "use-header-bar", &use_header_bar, NULL);
        if (!use_header_bar) {
                GtkWidget *button;
                GtkWidget *action_area;
                GList *children, *l;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                action_area = gtk_dialog_get_action_area (GTK_DIALOG (about));
G_GNUC_END_IGNORE_DEPRECATIONS
                children = gtk_container_get_children (GTK_CONTAINER (action_area));
                for (l = children; l; l = l->next) {
                        GtkWidget *child = l->data;
                        gboolean secondary;

                        gtk_container_child_get (GTK_CONTAINER (action_area), child, "secondary", &secondary, NULL);

                        if (gtk_widget_get_visible (child) && secondary) {
                                g_object_set_data (G_OBJECT (about), "credits_button", child);
                                break;
                        }
                }
                g_list_free (children);

                button = gtk_toggle_button_new_with_label (_("System"));
                gtk_widget_show (button);
                gtk_dialog_add_action_widget (GTK_DIALOG (about), button, GTK_RESPONSE_NONE);
                gtk_container_child_set (GTK_CONTAINER (gtk_widget_get_parent (button)),
                                         button,
                                         "secondary", TRUE,
                                         NULL);
                g_object_set_data (G_OBJECT (about), "stack", stack);
                g_object_set_data (G_OBJECT (about), "system_button", button);

                g_signal_connect (button, "toggled", G_CALLBACK (toggle_system), about);
                g_signal_connect (stack, "notify::visible-child-name", G_CALLBACK (page_changed), about);
        }
}

static int
compare_strings (gconstpointer a,
                 gconstpointer b,
                 gpointer      data)
{
        const char * const *sa = a;
        const char * const *sb = b;

        return strcmp (*sa, *sb);
}

GrAboutDialog *
gr_about_dialog_new (void)
{
        GrAboutDialog *about;
        const char *authors[] = {
                "Emel Elvin Yıldız",
                "Matthias Clasen",
                "Jakub Steiner",
                "Christian Hergert",
                "Matthew Leeds",
                "Mohammed Sadiq",
                "Sam Hewitt",
                "Nirbheek Chauhan",
                "Ekta Nandwani",
                "Maithili Bhide",
                "Paxana Amanda Xander",
                NULL
        };
        const char *documenters[] = {
                "Paul Cutler",
                NULL
        };
        g_autofree char **recipe_authors = NULL;
        guint length;
        g_autoptr(GdkPixbuf) logo = NULL;
        GrRecipeStore *store;

        logo = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                         "org.gnome.Recipes",
                                         256,
                                         GTK_ICON_LOOKUP_FORCE_SIZE,
                                         NULL);

        about = g_object_new (GR_TYPE_ABOUT_DIALOG,
                              "program-name", _("Recipes"),
                              "version", get_version (),
                              "copyright", "© 2016, 2017 Matthias Clasen",
                              "license-type", GTK_LICENSE_GPL_3_0,
                              "comments", _("GNOME loves to cook"),
                              "authors", authors,
                              "documenters", documenters,
                              "translator-credits", _("translator-credits"),
                              "logo", logo,
                              "website", "https://wiki.gnome.org/Apps/Recipes",
                              "website-label", _("Learn more about Recipes"),
                              NULL);

        store = gr_recipe_store_get ();
        recipe_authors = gr_recipe_store_get_contributors (store, &length);
        g_qsort_with_data (recipe_authors, length, sizeof (char *), compare_strings, NULL);
        gtk_about_dialog_add_credit_section (GTK_ABOUT_DIALOG (about),
                                             _("Recipes by"), (const char **)recipe_authors);

        tweak_image (about);
        add_built_logo (about);
        add_system_tab (about);

	gtk_widget_realize (GTK_WIDGET (about));
	gdk_window_set_functions (gtk_widget_get_window (GTK_WIDGET (about)),
                                  GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_MAXIMIZE);

        return about;
}
