/* gr-image.c:
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

#include <libsoup/soup.h>

#include "gr-image.h"
#include "gr-utils.h"


typedef struct {
        GtkImage *image;
        int width;
        int height;
        gboolean fit;
        GCancellable *cancellable;
        GrImageCallback callback;
        gpointer data;
} TaskData;

static void
task_data_free (gpointer data)
{
        TaskData *td = data;

        g_clear_object (&td->cancellable);

        g_free (td);
}

struct _GrImage
{
        GObject parent_instance;
        char *id;
        char *path;

        SoupSession *session;
        SoupMessage *thumbnail_message;
        SoupMessage *image_message;
        GList *pending;
};

G_DEFINE_TYPE (GrImage, gr_image, G_TYPE_OBJECT)

static void
gr_image_finalize (GObject *object)
{
        GrImage *ri = GR_IMAGE (object);

        if (ri->thumbnail_message)
                soup_session_cancel_message (ri->session,
                                             ri->thumbnail_message,
                                             SOUP_STATUS_CANCELLED);
        g_clear_object (&ri->thumbnail_message);
        if (ri->image_message)
                soup_session_cancel_message (ri->session,
                                             ri->image_message,
                                             SOUP_STATUS_CANCELLED);
        g_clear_object (&ri->image_message);
        g_clear_object (&ri->session);
        g_free (ri->path);
        g_free (ri->id);

        g_list_free_full (ri->pending, task_data_free);
        ri->pending = NULL;

        G_OBJECT_CLASS (gr_image_parent_class)->finalize (object);
}

static void
gr_image_class_init (GrImageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_image_finalize;
}

static void
gr_image_init (GrImage *image)
{
}

GrImage *
gr_image_new (SoupSession *session,
              const char  *id,
              const char  *path)
{
        GrImage *image;

        image = g_object_new (GR_TYPE_IMAGE, NULL);
        image->session = g_object_ref (session);
        gr_image_set_id (image, id);
        gr_image_set_path (image, path);

        return image;
}

void
gr_image_set_id (GrImage    *image,
                 const char *id)
{
        g_free (image->id);
        image->id = g_strdup (id);
}

void
gr_image_set_path (GrImage    *image,
                   const char *path)
{
        g_free (image->path);
        image->path = g_strdup (path);
}

const char *
gr_image_get_path (GrImage *image)
{
        return image->path;
}

GPtrArray *
gr_image_array_new (void)
{
        return g_ptr_array_new_with_free_func (g_object_unref);
}

static GdkPixbuf *
load_pixbuf (const char *path,
             int         width,
             int         height,
             gboolean    fit)
{
        GdkPixbuf *pixbuf;

        if (fit)
                pixbuf = load_pixbuf_fit_size (path, width, height, FALSE);
        else
                pixbuf = load_pixbuf_fill_size (path, width, height);

        return pixbuf;
}

#define BASE_URL "https://static.gnome.org/recipes/v1"

static char *
get_image_url (GrImage *ri)
{
        g_autofree char *basename = NULL;

        basename = g_path_get_basename (ri->path);
        return g_strconcat (BASE_URL, "/images/", ri->id, "/", basename, NULL);
}

static char *
get_thumbnail_url (GrImage *ri)
{
        g_autofree char *basename = NULL;

        basename = g_path_get_basename (ri->path);
        return g_strconcat (BASE_URL, "/thumbnails/", ri->id, "/", basename, NULL);
}

static char *
get_image_cache_path (GrImage *ri)
{
        char *filename;
        g_autofree char *cache_dir = NULL;
        g_autofree char *basename = NULL;

        basename = g_path_get_basename (ri->path);
        filename = g_build_filename (get_user_cache_dir (), "images", ri->id,  basename, NULL);
        cache_dir = g_path_get_dirname (filename);
        g_mkdir_with_parents (cache_dir, 0755);

        return filename;
}

char *
gr_image_get_cache_path (GrImage *ri)
{
        if (ri->path[0] == '/')
                return g_strdup (ri->path);
        else
                return get_image_cache_path (ri);
}

static char *
get_thumbnail_cache_path (GrImage *ri)
{
        char *filename;
        g_autofree char *cache_dir = NULL;
        g_autofree char *basename = NULL;

        basename = g_path_get_basename (ri->path);
        filename = g_build_filename (get_user_cache_dir (), "thumbnails", ri->id, basename, NULL);
        cache_dir = g_path_get_dirname (filename);
        g_mkdir_with_parents (cache_dir, 0755);

        return filename;
}

static gboolean
should_try_load (const char *path)
{
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileInfo) info = NULL;
        gboolean result = TRUE;

        // We create negative cache entries as almost empty files, and we
        // limit our requests to once per day
        // And we want to retry cached images after 28 days, in case
        // they changed

        file = g_file_new_for_path (path);
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                  G_FILE_QUERY_INFO_NONE,
                                  NULL,
                                  NULL);
        if (info &&
            g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
                goffset size;
                g_autoptr(GDateTime) now = NULL;
                g_autoptr(GDateTime) mtime = NULL;

                size = g_file_info_get_size (info);
                now = g_date_time_new_now_utc ();
                mtime = g_file_info_get_modification_date_time (info);

                if (size == 6) {
                        result = g_date_time_difference (now, mtime) > G_TIME_SPAN_DAY;
                }
                else {
                        result = g_date_time_difference (now, mtime) > 28 * G_TIME_SPAN_DAY;
                }
                g_debug ("Cached %s for %s is %s",
                         size == 6 ? "failure" : "image",
                         path,
                         result ? "old, trying again" : "new enough");
        }

        return result;
}

static void
write_negative_cache_entry (const char *path)
{
        if (!g_file_set_contents (path, "failed", 6, NULL))
                g_warning ("Failed to write a negative cache entry for %s", path);
}

static void
update_image_timestamp (const char *path)
{
        g_autoptr(GFile) file = NULL;
        g_autoptr(GDateTime) now = NULL;
        guint64 mtime;

        now = g_date_time_new_now_utc ();
        mtime = (guint64)g_date_time_to_unix (now);

        file = g_file_new_for_path (path);
        g_file_set_attribute_uint64 (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, mtime, 0, NULL, NULL);
        g_debug ("updating timestamp for %s", path);
}

static void
set_modified_request (SoupMessage *msg,
                      const char  *path)
{
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileInfo) info = NULL;

        file = g_file_new_for_path (path);
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                  G_FILE_QUERY_INFO_NONE,
                                  NULL,
                                  NULL);
        if (info &&
            g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
                g_autoptr(GDateTime) mtime = NULL;
                g_autofree char *mod_date = NULL;

                mtime = g_file_info_get_modification_date_time (info);
                mod_date = g_date_time_format (mtime, "%a, %d %b %Y %H:%M:%S %Z");
                soup_message_headers_append (msg->request_headers, "If-Modified-Since", mod_date);
        }
}

static void
set_image (SoupSession *session,
           SoupMessage *msg,
           gpointer     data)
{
        GrImage *ri = data;
        g_autofree char *cache_path = NULL;
        GList *l;

        if (msg->status_code == SOUP_STATUS_CANCELLED || ri->session == NULL) {
                g_debug ("Message cancelled");
                return;
        }

        if (msg == ri->thumbnail_message)
                cache_path = get_thumbnail_cache_path (ri);
        else
                cache_path = get_image_cache_path (ri);

        if (msg->status_code == SOUP_STATUS_NOT_MODIFIED) {
                g_debug ("Image not modified");
                update_image_timestamp (cache_path);
        }
        else if (msg->status_code == SOUP_STATUS_OK) {
                g_debug ("Saving image to %s", cache_path);
                if (!g_file_set_contents (cache_path, msg->response_body->data, msg->response_body->length, NULL)) {
                        g_debug ("Saving image to %s failed", cache_path);
                        goto out;
                }
        }
        else {
                g_autofree char *url = get_image_url (ri);
                g_debug ("Got status %d, record failure to load %s", msg->status_code, url);
                write_negative_cache_entry (cache_path);
                goto out;
        }

        g_debug ("Loading image for %s", ri->path);

        l = ri->pending;
        while (l) {
                GList *next = l->next;
                TaskData *td = l->data;
                g_autoptr(GdkPixbuf) pixbuf = NULL;

                if (g_cancellable_is_cancelled (td->cancellable)) {
                        ri->pending = g_list_remove (ri->pending, td);
                        task_data_free (td);
                }
                else if (msg == ri->thumbnail_message &&
                    (td->width > 150 || td->height > 150)) {
                        g_autoptr(GdkPixbuf) tmp = NULL;
                        int w, h;

                        if (td->width < td->height) {
                                h = 150;
                                w = 150 * td->width / td->height;
                        }
                        else {
                                w = 150;
                                h = 150 * td->height / td->width;
                        }

                        tmp = load_pixbuf (cache_path, w, h, td->fit);
                        pixbuf = gdk_pixbuf_scale_simple (tmp, td->width, td->height, GDK_INTERP_BILINEAR);
                        pixbuf_blur (pixbuf, 5, 3);
                        td->callback (ri, pixbuf, td->data);
                }
                else {
                        pixbuf = load_pixbuf (cache_path, td->width, td->height, td->fit);
                        td->callback (ri, pixbuf, td->data);

                        ri->pending = g_list_remove (ri->pending, td);
                        task_data_free (td);
                }

                l = next;
        }

out:
        if (msg == ri->thumbnail_message)
                g_clear_object (&ri->thumbnail_message);
        else
                g_clear_object (&ri->image_message);

        if (ri->thumbnail_message || ri->image_message)
                return;

        g_list_free_full (ri->pending, task_data_free);
        ri->pending = NULL;
}

static void
gr_image_load_full (GrImage         *ri,
                    int              width,
                    int              height,
                    gboolean         fit,
                    gboolean         do_thumbnail,
                    GCancellable    *cancellable,
                    GrImageCallback  callback,
                    gpointer         data)
{
        TaskData *td;
        g_autofree char *image_cache_path = NULL;
        g_autofree char *thumbnail_cache_path = NULL;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        gboolean need_image;
        gboolean need_thumbnail;
        g_autofree char *local_path = NULL;

        /* We store images in local recipes with an absolute path nowadays.
         * We used to store them as a relative path starting with images/,
         * so try that case as well.
         */
        if (ri->path == NULL) {
                g_warning ("No image path");
                return;
        }

        if (ri->path[0] == '/')
                local_path = g_strdup (ri->path);
        else if (g_str_has_prefix (ri->path, "images/"))
                local_path = g_build_filename (get_user_data_dir (), ri->path, NULL);

        if (local_path) {
                pixbuf = load_pixbuf (local_path, width, height, fit);
                if (pixbuf) {
                        g_debug ("Use local image for %s", ri->path);
                        callback (ri, pixbuf, data);
                        return;
                }
        }

        image_cache_path = get_image_cache_path (ri);
        thumbnail_cache_path = get_thumbnail_cache_path (ri);

        need_thumbnail = do_thumbnail && should_try_load (thumbnail_cache_path);
        need_image = should_try_load (image_cache_path);

        if (width <= 150 && height <= 150) {
                pixbuf = load_pixbuf (thumbnail_cache_path, width, height, fit);
                need_image = FALSE;
        }
        else {
                pixbuf = load_pixbuf (image_cache_path, width, height, fit);
        }

        if (pixbuf) {
                 g_debug ("Use cached %s for %s",
                          width <= 150 && height <= 150 ? "thumbnail" : "image",
                          ri->path);
                callback (ri, pixbuf, data);
        }
        else if (do_thumbnail) {
                int w = 150, h = 150;

                if (width < height)
                        w = 150 * width / height;
                else
                        h = 150 * height / width;

                pixbuf = load_pixbuf (thumbnail_cache_path, w, h, fit);
                if (pixbuf) {
                        g_autoptr(GdkPixbuf) blurred = NULL;

                        g_debug ("Use cached blurred thumbnail for %s", ri->path);
                        blurred = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
                        pixbuf_blur (blurred, 5, 3);
                        callback (ri, blurred, data);
                        need_image = TRUE;
                }
        }

        if (!need_thumbnail && !need_image)
                return;

        td = g_new0 (TaskData, 1);
        td->width = width;
        td->height = height;
        td->fit = fit;
        td->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
        td->callback = callback;
        td->data = data;

        ri->pending = g_list_prepend (ri->pending, td);

        if (need_thumbnail && ri->thumbnail_message == NULL) {
                g_autofree char *url = NULL;
                g_autoptr(SoupURI) base_uri = NULL;

                url = get_thumbnail_url (ri);
                base_uri = soup_uri_new (url);
                ri->thumbnail_message = soup_message_new_from_uri (SOUP_METHOD_GET, base_uri);
                set_modified_request (ri->thumbnail_message, thumbnail_cache_path);
                g_debug ("Load thumbnail for %s from %s", ri->path, url);
                soup_session_queue_message (ri->session, g_object_ref (ri->thumbnail_message), set_image, ri);
                if (width > 150 || height > 150)
                        need_image = TRUE;
        }

        if (need_image && ri->image_message == NULL) {
                g_autofree char *url = NULL;
                g_autoptr(SoupURI) base_uri = NULL;

                url = get_image_url (ri);
                base_uri = soup_uri_new (url);
                ri->image_message = soup_message_new_from_uri (SOUP_METHOD_GET, base_uri);
                set_modified_request (ri->image_message, image_cache_path);
                g_debug ("Load image for %s from %s", ri->path, url);
                soup_session_queue_message (ri->session, g_object_ref (ri->image_message), set_image, ri);
        }
}

void
gr_image_load (GrImage         *ri,
               int              width,
               int              height,
               gboolean         fit,
               GCancellable    *cancellable,
               GrImageCallback  callback,
               gpointer         data)
{
        gr_image_load_full (ri, width, height, fit, TRUE, cancellable, callback, data);
}

void
gr_image_set_pixbuf (GrImage   *ri,
                     GdkPixbuf *pixbuf,
                     gpointer   data)
{
        gtk_image_set_from_pixbuf (GTK_IMAGE (data), pixbuf);
}

typedef struct {
        GdkPixbuf *pixbuf;
        GMainLoop *loop;
} PbData;

static void
set_pixbuf (GrImage   *ri,
            GdkPixbuf *pixbuf,
            gpointer   data)
{
        PbData *pbd = data;

        g_main_loop_quit (pbd->loop);
        g_set_object (&pbd->pixbuf, pixbuf);
}

GdkPixbuf  *
gr_image_load_sync (GrImage   *ri,
                    int        width,
                    int        height,
                    gboolean   fit)
{
        PbData data;

        data.pixbuf = NULL;
        data.loop = g_main_loop_new (NULL, FALSE);

        gr_image_load_full (ri, width, height, fit, FALSE, NULL, set_pixbuf, &data);
        if (data.pixbuf == NULL)
                g_main_loop_run (data.loop);
        g_main_loop_unref (data.loop);

        if (!data.pixbuf) {
                g_autoptr(GtkIconInfo) info = NULL;

                info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
                                                   "org.gnome.Recipes",
                                                    256,
                                                    GTK_ICON_LOOKUP_FORCE_SIZE);
                data.pixbuf = load_pixbuf (gtk_icon_info_get_filename (info), width, height, fit);

        }

        return data.pixbuf;
}
