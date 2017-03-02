/* gr-utils.c:
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
#include <stdarg.h>
#include <sys/stat.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include "gr-utils.h"
#include "gr-app.h"

/* load image to fit in width x height while preserving
 * aspect ratio, filling seams with transparency
 */
GdkPixbuf *
load_pixbuf_fit_size (const char *path,
                      int         width,
                      int         height,
                      gboolean    pad)
{
        g_autoptr(GdkPixbuf) original = NULL;
        GdkPixbuf *pixbuf;
        int dest_x, dest_y, dest_width, dest_height;

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
        gdk_pixbuf_fill (pixbuf, 0x00000000);

        original = gdk_pixbuf_new_from_file_at_size (path, width, height, NULL);
        if (!original) {
                g_warning ("Failed to load image %s", path);
                return pixbuf;
        }

        if (pad) {
                dest_width = gdk_pixbuf_get_width (original);
                dest_height = gdk_pixbuf_get_height (original);
                dest_x = (width - dest_width) / 2;
                dest_y = (height - dest_height) / 2;

                gdk_pixbuf_composite (original, pixbuf,
                                      dest_x, dest_y, dest_width, dest_height,
                                      dest_x, dest_y, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
        }
        else {
                g_set_object (&pixbuf, original);
        }

        return pixbuf;
}

/* load image to fill width x height while preserving
 * aspect ratio, cutting off overshoots
 */
GdkPixbuf *
load_pixbuf_fill_size (const char *path,
                       int         width,
                       int         height)
{
        g_autoptr(GdkPixbuf) original = NULL;
        int x, y;

        original = gdk_pixbuf_new_from_file_at_scale (path, -1, height, TRUE, NULL);
        if (!original) {
                GdkPixbuf *pixbuf;
                g_warning ("Failed to load image %s", path);
                pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
                gdk_pixbuf_fill (pixbuf, 0x00000000);
                return pixbuf;
        }

        if (gdk_pixbuf_get_width (original) < width) {
                g_autoptr(GdkPixbuf) pb1 = NULL;
                pb1 = gdk_pixbuf_new_from_file_at_scale (path, width, -1, TRUE, NULL);
                g_set_object (&original, pb1);
        }

        g_assert (gdk_pixbuf_get_width (original) >= width &&
                  gdk_pixbuf_get_height (original) >= height);

        x = (gdk_pixbuf_get_width (original) - width) / 2;
        y = (gdk_pixbuf_get_height (original) - height) / 2;

        if (x == 0 && y == 0)
                return g_object_ref (original);
        else
                return gdk_pixbuf_new_subpixbuf (original, x, y, width, height);
}

const char *
get_user_data_dir (void)
{
        static char *dir = NULL;

        if (!dir) {
                if (in_flatpak_sandbox ())
                        dir = g_strdup (g_get_user_data_dir ());
                else
                        dir = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME, NULL);
                g_mkdir_with_parents (dir, S_IRWXU | S_IRWXG | S_IRWXO);
        }

        return (const char *)dir;
}

const char *
get_old_user_data_dir (void)
{
        static char *dir = NULL;

        if (!dir)
                dir = g_build_filename (g_get_user_data_dir (), "recipes", NULL);

        return (const char *)dir;
}

const char *
get_pkg_data_dir (void)
{
        static char *dir = NULL;

        if (!dir) {
                const char * const *dirs;

                dirs = g_get_system_data_dirs ();

                if (dirs)
                        dir = g_build_filename (dirs[0], PACKAGE_NAME, NULL);
                else
                        dir = (char *)PKGDATADIR;
        }

        return (const char *)dir;
}

void
container_remove_all (GtkContainer *container)
{
        GList *children, *l;

        children = gtk_container_get_children (container);
        for (l = children; l; l = l->next) {
                gtk_container_remove (container, GTK_WIDGET (l->data));
        }
        g_list_free (children);
}

static void
gr_utils_widget_css_parsing_error_cb (GtkCssProvider *provider,
                                      GtkCssSection *section,
                                      GError *error,
                                      gpointer user_data)
{
        g_warning ("CSS parse error %u:%u: %s",
                   gtk_css_section_get_start_line (section),
                   gtk_css_section_get_start_position (section),
                   error->message);
}

static void
gr_utils_widget_set_css_internal (GtkWidget *widget,
                                  const gchar *class_name,
                                  const gchar *css)
{
        GtkStyleContext *context;
        g_autoptr(GtkCssProvider) provider = NULL;

        g_debug ("using custom CSS %s", css);

        /* set the custom CSS class */
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_add_class (context, class_name);

        /* set up custom provider and store on the widget */
        provider = gtk_css_provider_new ();
        g_signal_connect (provider, "parsing-error",
                          G_CALLBACK (gr_utils_widget_css_parsing_error_cb), NULL);
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_data (provider, css, -1, NULL);
        g_object_set_data_full (G_OBJECT (widget),
                                "custom-css-provider",
                                g_object_ref (provider),
                                g_object_unref);
}

void
gr_utils_widget_set_css_simple (GtkWidget *widget,
                                const      gchar *css)
{
        g_autofree gchar *class_name = NULL;
        g_autoptr(GString) str = NULL;

        /* remove custom class if NULL */
        class_name = g_strdup_printf ("themed-widget_%p", widget);
        if (css == NULL) {
                GtkStyleContext *context = gtk_widget_get_style_context (widget);
                gtk_style_context_remove_class (context, class_name);
                return;
        }
        str = g_string_sized_new (1024);
        g_string_append_printf (str, ".%s {\n", class_name);
        g_string_append_printf (str, "%s\n", css);
        g_string_append (str, "}");

        gr_utils_widget_set_css_internal (widget, class_name, str->str);
}

char *
date_time_to_string (GDateTime *dt)
{
        return g_strdup_printf ("%d-%d-%d %d:%d:%d",
                                g_date_time_get_year (dt),
                                g_date_time_get_month (dt),
                                g_date_time_get_day_of_month (dt),
                                g_date_time_get_hour (dt),
                                g_date_time_get_minute (dt),
                                g_date_time_get_second (dt));
}

GDateTime *
date_time_from_string (const char *string)
{
        g_auto(GStrv) s = NULL;
        g_auto(GStrv) s1 = NULL;
        g_auto(GStrv) s2 = NULL;
        int year, month, day, hour, minute, second;
        char *endy, *endm, *endd, *endh, *endmi, *ends;

        s = g_strsplit (string, " ", -1);
        if (g_strv_length (s) != 2)
                return NULL;

        s1 = g_strsplit (s[0], "-", -1);
        if (g_strv_length (s1) != 3)
                return NULL;

        s2 = g_strsplit (s[1], ":", -1);
        if (g_strv_length (s1) != 3)
                return NULL;

        year = strtol (s1[0], &endy, 10);
        month = strtol (s1[1], &endm, 10);
        day = strtol (s1[2], &endd, 10);

        hour = strtol (s2[0], &endh, 10);
        minute = strtol (s2[1], &endmi, 10);
        second = strtol (s2[2], &ends, 10);

        if (!*endy && !*endm && !*endd &&
            !*endh && !*endmi && !*ends &&
            0 < month && month <= 12 &&
            0 < day && day <= 31 &&
            0 <= hour && hour < 24 &&
            0 <= minute && minute < 60 &&
            0 <= second && second < 60)
                    return g_date_time_new_utc (year, month, day,
                                                hour, minute, second);

        return NULL;
}

gboolean
skip_whitespace (char **input)
{
        char *in = *input;

        while (*input && g_unichar_isspace (g_utf8_get_char (*input))) {
                *input = g_utf8_next_char (*input);
        }

        return in != *input;
}

gboolean
space_or_nul (char p)
{
        return (p == '\0' || g_ascii_isspace (p));
}

/* this is for translating strings that have been extracted using recipe-extract */
char *
translate_multiline_string (const char *s)
{
        g_auto(GStrv) strv = NULL;
        int i;
        GString *out;

        if (s == NULL)
                return NULL;

        out = g_string_new ("");

        strv = g_strsplit (s, "\n", -1);

        for (i = 0; strv[i]; i++) {
                if (i > 0)
                        g_string_append (out, "\n");
                if (strv[i][0] != 0)
                        g_string_append (out, g_dgettext (GETTEXT_PACKAGE "-data", strv[i]));
       }

        return g_string_free (out, FALSE);
}

char *
generate_id (const char *first_string, ...)
{
        va_list ap;

        const char *s;
        const char *q;
        GString *str;

        va_start (ap, first_string);

        str = g_string_new ("");

        s = first_string;
        while (s) {
                for (q = s; *q; q = g_utf8_find_next_char (q, NULL)) {
                        gunichar ch = g_utf8_get_char (q);
                        if (ch > 128 ||
                            g_ascii_isalnum (ch) || g_ascii_isdigit (ch) ||
                                 (char)ch == '-' || (char)ch == '_')
                                g_string_append_unichar (str, ch);
                        else
                                g_string_append_c (str, '_');
                }

                s = va_arg (ap, const char *);
        }

        va_end (ap);

        return g_string_free (str, FALSE);
}

static gint64 start_time;

void
start_recording (void)
{
        start_time = g_get_monotonic_time ();
}

void
record_step (const char *blurb)
{
        if (start_time != 0)
                g_print ("%0.3f %s\n", 0.001 * (g_get_monotonic_time () - start_time), blurb);
}

void
stop_recording (void)
{
        start_time = 0;
}

char *
format_date_time_difference (GDateTime *end,
                             GDateTime *start)
{
        int y1, m1, d1, y2, m2, d2, delta;
        GTimeSpan span;
        int i;

        span = g_date_time_difference (end, start);
        g_date_time_get_ymd (start, &y1, &m1, &d1);
        g_date_time_get_ymd (end, &y2, &m2, &d2);

        if (y1 + 1 < y2)
                return g_strdup_printf (_("more than a year ago"));
        else if (y1 < y2) {
                delta = 12 - m1 + m2;
                return g_strdup_printf (ngettext ("%d month ago", "%d months ago", delta), delta);
        }
        else if (m1 < m2) {
                delta = m2 - m1;
                return g_strdup_printf (ngettext ("%d month ago", "%d months ago", delta), delta);
        }
        else if (d1 < d2) {
                delta = d2 - d1;
                return g_strdup_printf (ngettext ("%d day ago", "%d days ago", delta), delta);
        }
        else if (span < 5 * G_TIME_SPAN_MINUTE)
                return g_strdup_printf (_("just now"));
        else if (span < 15 * G_TIME_SPAN_MINUTE)
                return g_strdup_printf (_("10 minutes ago"));
        else if (span < 45 * G_TIME_SPAN_MINUTE)
                return g_strdup_printf (_("half an hour ago"));
        else {
                for (i = 1; i < 23; i++) {
                        if (span < i * G_TIME_SPAN_HOUR + 30 * G_TIME_SPAN_MINUTE) {
                                return g_strdup_printf (ngettext ("%d hour ago", "%d hours ago", i), i);
                                break;
                        }
                }
        }

        return g_strdup (_("some time ago"));
}

gboolean
in_flatpak_sandbox (void)
{
        return g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

gboolean
portals_available (void)
{
        static GDBusProxy *portal = NULL;
        g_autofree char *owner = NULL;

        if (portal == NULL) {
                g_autoptr(GDBusConnection) bus = NULL;

                bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
		if (!bus)
			return FALSE;

                portal = g_dbus_proxy_new_sync (bus,
                                                0,
                                                NULL,
                                                "org.freedesktop.portal.Desktop",
                                                "/org/freedesktop/portal/desktop",
                                                "org.freedesktop.portal.FileChooser",
                                                NULL,
                                                NULL);
        }

        owner = g_dbus_proxy_get_name_owner (portal);

        return owner != NULL;
}

void
all_headers (GtkListBoxRow *row,
             GtkListBoxRow *before,
             gpointer       user_data)
{
        GtkWidget *header;

        header = gtk_list_box_row_get_header (row);
        if (header)
                return;

        header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_list_box_row_set_header (row, header);
}

#ifdef GDK_WINDOWING_WAYLAND
typedef struct {
        GtkWindow *window;
        WindowHandleExported callback;
        gpointer user_data;
} WaylandWindowHandleExportedData;

static void
wayland_window_handle_exported (GdkWindow  *window,
                                const char *wayland_handle_str,
                                gpointer    user_data)
{
        WaylandWindowHandleExportedData *data = user_data;
        char *handle_str;

        handle_str = g_strdup_printf ("wayland:%s", wayland_handle_str);
        data->callback (data->window, handle_str, data->user_data);
        g_free (handle_str);

        g_free (data);
}
#endif

gboolean
window_export_handle (GtkWindow            *window,
                      WindowHandleExported  callback,
                      gpointer              user_data)
{
#ifdef GDK_WINDOWING_X11
        if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window)))) {
                GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
                char *handle_str;
                guint32 xid = (guint32) gdk_x11_window_get_xid (gdk_window);

                handle_str = g_strdup_printf ("x11:%x", xid);
                callback (window, handle_str, user_data);

                return TRUE;
        }
#endif
#ifdef GDK_WINDOWING_WAYLAND
        if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window)))) {
                GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
                WaylandWindowHandleExportedData *data;

                data = g_new0 (WaylandWindowHandleExportedData, 1);
                data->window = window;
                data->callback = callback;
                data->user_data = user_data;

                if (!gdk_wayland_window_export_handle (gdk_window,
                                                       wayland_window_handle_exported,
                                                       data,
                                                       g_free)) {
                        g_free (data);
                        return FALSE;
                }
                else {
                        return TRUE;
                }
        }
#endif

        g_warning ("Couldn't export handle, unsupported windowing system");
        callback (window, "", user_data);

        return FALSE;
}

void
window_unexport_handle (GtkWindow *window)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
      GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

      gdk_wayland_window_unexport_handle (gdk_window);
    }
#endif
}

static char *
get_import_name (const char *path)
{
        g_autofree char *dir = NULL;
        g_autofree char *basename = NULL;
        char *filename;
        char *dot;
        char *suffix;
        int i;

        dir = g_build_filename (get_user_data_dir (), "images", NULL);
        g_mkdir_with_parents (dir, 0755);

        basename = g_path_get_basename (path);

        filename = g_build_filename (dir, basename, NULL);
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                return filename;

        g_free (filename);

        dot = strrchr (basename, '.');
        if (dot) {
                suffix = dot + 1;
                *dot = '\0';
        }
        else {
                suffix = NULL;
        }
        for (i = 0; i < 100; i++) {
                char buf[10];
                g_autofree char *newbase;

                g_snprintf (buf, 10, "%d", i);
                newbase = g_strconcat (basename, buf, ".", suffix, NULL);
                filename = g_build_filename (dir, newbase, NULL);
                if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                        return filename;
                g_free (filename);
        }

        return NULL;
}

char *
import_image (const char *path)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        g_autoptr(GdkPixbuf) oriented = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *imported = NULL;
        GdkPixbufFormat *format;
        g_autofree char *format_name = NULL;

        // FIXME: this could be much more efficient
        pixbuf = gdk_pixbuf_new_from_file (path, &error);
        if (pixbuf == NULL) {
                g_message ("Failed to load image '%s': %s", path, error->message);
                return NULL;
        }

        imported = get_import_name (path);
        format = gdk_pixbuf_get_file_info (path, NULL, NULL);
        if (gdk_pixbuf_format_is_writable (format))
                format_name = gdk_pixbuf_format_get_name (format);
        else
                format_name = g_strdup ("png");

        g_debug ("Loading %s (format %s), importing to %s\n", path, format_name, imported);
        oriented = gdk_pixbuf_apply_embedded_orientation (pixbuf);

        if (!gdk_pixbuf_save (oriented, imported, format_name, &error, NULL)) {
                g_message ("Failed to import image: %s", error->message);
                return NULL;
        }

        return g_strdup (imported);
}

char *
rotate_image (const char *path,
              int         angle)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        g_autoptr(GdkPixbuf) oriented = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *imported  = NULL;
        GdkPixbufFormat *format;
        g_autofree char *format_name = NULL;

        pixbuf = gdk_pixbuf_new_from_file (path, &error);
        if (pixbuf == NULL) {
                g_message ("Failed to load image '%s': %s", path, error->message);
                return NULL;
        }

        imported = get_import_name (path);
        format = gdk_pixbuf_get_file_info (path, NULL, NULL);
        if (gdk_pixbuf_format_is_writable (format))
                format_name = gdk_pixbuf_format_get_name (format);
        else
                format_name = g_strdup ("png");

        oriented = gdk_pixbuf_rotate_simple (pixbuf, angle);

        if (!gdk_pixbuf_save (oriented, imported, format_name, &error, NULL)) {
                g_message ("Failed to save rotated image: %s", error->message);
                return NULL;
        }

        return g_strdup (imported);
}

void
remove_image (const char *path)
{
        if (g_str_has_prefix (path, get_user_data_dir ())) {
                g_debug ("Removing image %s", path);
                g_remove (path);
        }
        else {
                g_print ("Not removing image %s", path);
        }
}
