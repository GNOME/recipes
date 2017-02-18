/* gr-image-viewer.c:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3.
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

#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gi18n.h>
#include <gio/gunixoutputstream.h>

#include "gr-image-viewer.h"
#include "gr-images.h"
#include "gr-utils.h"
#include "gr-window.h"


struct _GrImageViewer
{
        GtkEventBox parent_instance;

        GtkWidget *overlay;
        GtkWidget *image1;
        GtkWidget *image2;
        GtkWidget *stack;
        GtkWidget *event_box;
        GtkWidget *next_revealer;
        GtkWidget *prev_revealer;
        GtkWidget *preview_revealer;
        GtkWidget *preview_list;

        GArray *images;
        int index;

        guint hide_timeout;

        GtkGesture *gesture;
};


G_DEFINE_TYPE (GrImageViewer, gr_image_viewer, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_IMAGES,
        PROP_INDEX,
        N_PROPS
};

static guint activate_signal;

GrImageViewer *
gr_image_viewer_new (void)
{
        return g_object_new (GR_TYPE_IMAGE_VIEWER, NULL);
}

static void
remove_hide_timeout (GrImageViewer *viewer)
{
       if (viewer->hide_timeout != 0) {
                g_source_remove (viewer->hide_timeout);
                viewer->hide_timeout = 0;
       }
}

static void
gr_image_viewer_finalize (GObject *object)
{
        GrImageViewer *viewer = GR_IMAGE_VIEWER (object);

        g_clear_pointer (&viewer->images, g_array_unref);
        remove_hide_timeout (viewer);
        g_clear_object (&viewer->gesture);

        G_OBJECT_CLASS (gr_image_viewer_parent_class)->finalize (object);
}

static void
set_current_image (GrImageViewer *viewer)
{
        GtkFlowBoxChild *child;

        if (viewer->index >= viewer->images->len) {
                gtk_stack_set_visible_child_name (GTK_STACK (viewer->stack), "placeholder");
                return;
        }

        if (viewer->images->len > viewer->index) {
                GrRotatedImage *ri = NULL;
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                const char *vis;

                ri = &g_array_index (viewer->images, GrRotatedImage, viewer->index);
                pixbuf = load_pixbuf_fill_size (ri->path, ri->angle, 360, 240);

                vis = gtk_stack_get_visible_child_name (GTK_STACK (viewer->stack));
                if (strcmp (vis, "image1") == 0) {
                        gtk_image_set_from_pixbuf (GTK_IMAGE (viewer->image2), pixbuf);
                        gtk_stack_set_visible_child_name (GTK_STACK (viewer->stack), "image2");
                }
                else {
                        gtk_image_set_from_pixbuf (GTK_IMAGE (viewer->image1), pixbuf);
                        gtk_stack_set_visible_child_name (GTK_STACK (viewer->stack), "image1");
                }
        }

        child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (viewer->preview_list), viewer->index);
        if (child)
                gtk_flow_box_select_child (GTK_FLOW_BOX (viewer->preview_list), child);

        g_object_notify (G_OBJECT (viewer), "index");
}

static void
populate_preview (GrImageViewer *viewer)
{
        int i;

        container_remove_all (GTK_CONTAINER (viewer->preview_list));

        for (i = 0; i < viewer->images->len; i++) {
                GrRotatedImage *ri = &g_array_index (viewer->images, GrRotatedImage, i);
                g_autoptr(GdkPixbuf) pb = load_pixbuf_fill_size (ri->path, ri->angle, 60, 40);
                GtkWidget *image;

                image = gtk_image_new_from_pixbuf (pb);
                gtk_widget_show (image);
                gtk_container_add (GTK_CONTAINER (viewer->preview_list), image);
        }
}

static void
show_buttons (GrImageViewer *viewer)
{
        if (!viewer->images || viewer->images->len < 2)
                return;

        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (viewer->next_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (viewer->next_revealer), TRUE);

        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (viewer->prev_revealer)))
                        gtk_revealer_set_reveal_child (GTK_REVEALER (viewer->prev_revealer), TRUE);
}

static void
hide_buttons (GrImageViewer *viewer)
{
        if (gtk_revealer_get_child_revealed (GTK_REVEALER (viewer->next_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (viewer->next_revealer), FALSE);

        if (gtk_revealer_get_child_revealed (GTK_REVEALER (viewer->prev_revealer)))
                        gtk_revealer_set_reveal_child (GTK_REVEALER (viewer->prev_revealer), FALSE);
}

static void
show_preview (GrImageViewer *viewer)
{
        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (viewer->preview_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (viewer->preview_revealer), TRUE);
}

static void
hide_preview (GrImageViewer *viewer)
{
        if (gtk_revealer_get_child_revealed (GTK_REVEALER (viewer->preview_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (viewer->preview_revealer), FALSE);
}

static void
toggle_preview (GrImageViewer *viewer)
{
        if (!viewer->images || viewer->images->len < 2)
                return;

        if (gtk_revealer_get_child_revealed (GTK_REVEALER (viewer->preview_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (viewer->preview_revealer), FALSE);
        else
                gtk_revealer_set_reveal_child (GTK_REVEALER (viewer->preview_revealer), TRUE);
}

static void
hide_controls (GrImageViewer *viewer)
{
        hide_buttons (viewer);
        hide_preview (viewer);
}

static void
show_controls (GrImageViewer *viewer)
{
        show_buttons (viewer);
        show_preview (viewer);
}

static gboolean
hide_timeout (gpointer data)
{
        GrImageViewer *viewer = data;

        hide_controls (viewer);

        viewer->hide_timeout = 0;

        return G_SOURCE_REMOVE;
}

static void
reset_hide_timeout (GrImageViewer *viewer)
{
        remove_hide_timeout (viewer);
        viewer->hide_timeout = g_timeout_add (5000, hide_timeout, viewer);
}

static gboolean
enter_leave_notify (GtkWidget     *widget,
                    GdkEvent      *event,
                    GrImageViewer *viewer)
{
        if (((GdkEventCrossing *)event)->detail != GDK_NOTIFY_VIRTUAL) // FIXME
                        return FALSE;

        if (event->type == GDK_ENTER_NOTIFY) {
                show_buttons (viewer);
                reset_hide_timeout (viewer);
        }
        else {
                hide_controls (viewer);
                remove_hide_timeout (viewer);
        }

        return FALSE;
}

static gboolean
motion_notify (GtkWidget     *widget,
               GdkEvent      *event,
               GrImageViewer *viewer)
{
        show_buttons (viewer);
        reset_hide_timeout (viewer);

        return FALSE;
}

static void
button_press (GtkGesture *gesture,
              int         n_press,
              double      x,
              double      y,
              GrImageViewer *viewer)
{
        int button;

        button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

        if (button == GDK_BUTTON_PRIMARY) {
                g_signal_emit (viewer, activate_signal, 0);
        }
        else {
                toggle_preview (viewer);
                gtk_widget_grab_focus (viewer->event_box);
        }
}

static void
prev_image (GrImageViewer *viewer)
{
        viewer->index = (viewer->index + viewer->images->len - 1) % viewer->images->len;
        set_current_image (viewer);
}

static void
next_image (GrImageViewer *viewer)
{
        viewer->index = (viewer->index + 1) % viewer->images->len;
        set_current_image (viewer);
}

static void
preview_selected (GrImageViewer *viewer)
{
        GList *l;
        GtkFlowBoxChild *child;

        l = gtk_flow_box_get_selected_children (GTK_FLOW_BOX (viewer->preview_list));
        if (!l)
                return;

        child = l->data;
        g_list_free (l);
        if (viewer->index != gtk_flow_box_child_get_index (child)) {
                viewer->index = gtk_flow_box_child_get_index (child);
                set_current_image (viewer);
        }
}

static void
add_image (GrImageViewer  *viewer,
           GrRotatedImage *ri,
           gboolean        select)
{
        g_array_append_vals (viewer->images, ri, 1);
        ri = &g_array_index (viewer->images, GrRotatedImage, viewer->images->len - 1);
        ri->path = g_strdup (ri->path);

        populate_preview (viewer);
        if (select)
                viewer->index = viewer->images->len - 1;
        set_current_image (viewer);

        g_object_notify (G_OBJECT (viewer), "images");
}

static void
image_received (GtkClipboard *clipboard,
                GdkPixbuf    *pixbuf,
                gpointer      data)
{
        GrImageViewer *viewer = data;

        if (pixbuf) {
                GrRotatedImage ri;
                const char *dir;
                g_autofree char *path = NULL;
                int fd;
                g_autoptr(GOutputStream) stream = NULL;
                g_autoptr(GError) error = NULL;

                dir = get_user_data_dir ();
                g_mkdir_with_parents (dir, S_IRWXU | S_IRWXG | S_IRWXO);

                path = g_build_filename (dir, "clipboardXXXXXX.png", NULL);
                fd = g_mkstemp (path);
                stream = g_unix_output_stream_new (fd, TRUE);

                if (!gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, &error, NULL)) {
                        g_message ("Saving clipboard contents failed: %s", error->message);
                        return;
                }

                ri.path = g_strdup (path);
                ri.angle = 0;

                add_image (viewer, &ri, TRUE);
        }
}

static gboolean
key_press_event (GtkWidget     *widget,
                 GdkEvent      *event,
                 GrImageViewer *viewer)
{
        GdkEventKey *key = (GdkEventKey *)event;

        if (key->keyval == GDK_KEY_space) {
                show_controls (viewer);
                return TRUE;
        }
        if (key->keyval == GDK_KEY_Escape) {
                hide_controls (viewer);
                return TRUE;
        }
        else if (key->keyval == GDK_KEY_Left) {
                prev_image (viewer);
                return TRUE;
        }
        else if (key->keyval == GDK_KEY_Right) {
                next_image (viewer);
                return TRUE;
        }
        else if (key->keyval == GDK_KEY_v && (key->state & GDK_CONTROL_MASK) != 0) {
                GtkClipboard *clipboard;

                clipboard = gtk_widget_get_clipboard (GTK_WIDGET (viewer), GDK_SELECTION_CLIPBOARD);
                gtk_clipboard_request_image (clipboard, image_received, viewer);
                return TRUE;
        }

        return FALSE;
}

static void
gr_image_viewer_init (GrImageViewer *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
        gtk_widget_add_events (GTK_WIDGET (self->event_box), GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_POINTER_MOTION_MASK);
        gtk_widget_add_events (GTK_WIDGET (self->event_box), GDK_BUTTON_PRESS_MASK);

        g_signal_connect (self->event_box, "enter-notify-event", G_CALLBACK (enter_leave_notify), self);
        g_signal_connect (self->event_box, "leave-notify-event", G_CALLBACK (enter_leave_notify), self);
        g_signal_connect (self->event_box, "motion-notify-event", G_CALLBACK (motion_notify), self);
        g_signal_connect (self->event_box, "key-press-event", G_CALLBACK (key_press_event), self);

        self->gesture = gtk_gesture_multi_press_new (self->event_box);
        gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->gesture), 0);
        gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->gesture), GTK_PHASE_BUBBLE);
        g_signal_connect (self->gesture, "pressed", G_CALLBACK (button_press), self);
        self->images = gr_rotated_image_array_new ();
}

static void
gr_image_viewer_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        GrImageViewer *self = GR_IMAGE_VIEWER (object);

        switch (prop_id)
          {
          case PROP_IMAGES:
                  g_value_set_boxed (value, self->images);
                  break;

          case PROP_INDEX:
                  g_value_set_int (value, self->index);
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_image_viewer_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        GrImageViewer *self = GR_IMAGE_VIEWER (object);

        switch (prop_id)
          {
          case PROP_IMAGES:
                  gr_image_viewer_set_images (self,
                                              (GArray *) g_value_get_boxed (value),
                                              gr_image_viewer_get_index (self));
                  break;

          case PROP_INDEX:
                  gr_image_viewer_show_image (self, g_value_get_int (value));
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_image_viewer_class_init (GrImageViewerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_image_viewer_finalize;
        object_class->get_property = gr_image_viewer_get_property;
        object_class->set_property = gr_image_viewer_set_property;

        pspec = g_param_spec_boxed ("images", NULL, NULL,
                                    G_TYPE_ARRAY,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_IMAGES, pspec);

        pspec = g_param_spec_int ("index", NULL, NULL,
                                  0, G_MAXINT, 0,
                                  G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INDEX, pspec);

        activate_signal = g_signal_new ("activate",
                                        G_TYPE_FROM_CLASS (object_class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 0);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-image-viewer.ui");
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, image1);
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, image2);
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, stack);
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, event_box);
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, overlay);
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, prev_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, next_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, preview_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrImageViewer, preview_list);
        gtk_widget_class_bind_template_callback (widget_class, prev_image);
        gtk_widget_class_bind_template_callback (widget_class, next_image);
        gtk_widget_class_bind_template_callback (widget_class, preview_selected);
}

void
gr_image_viewer_set_images (GrImageViewer *viewer,
                            GArray        *images,
                            int            index)
{
        int i;

        g_object_freeze_notify (G_OBJECT (viewer));

        g_array_remove_range (viewer->images, 0, viewer->images->len);
        g_object_notify (G_OBJECT (viewer), "images");

        for (i = 0; i < images->len; i++) {
                GrRotatedImage *ri = &g_array_index (images, GrRotatedImage, i);

                g_array_append_vals (viewer->images, ri, 1);
                ri = &g_array_index (viewer->images, GrRotatedImage, viewer->images->len - 1);
                ri->path = g_strdup (ri->path);
        }

        populate_preview (viewer);
        viewer->index = index;
        set_current_image (viewer);
        hide_controls (viewer);

        g_object_thaw_notify (G_OBJECT (viewer));
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

                g_snprintf (buf, 10, "%d", i);
                filename = g_strconcat (basename, buf, ".", suffix, NULL);
                if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                        return filename;
                g_free (filename);
        }

        return NULL;
}

static char *
import_image (const char *path)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        g_autoptr(GdkPixbuf) oriented = NULL;
        g_autoptr(GError) error = NULL;
        char *imported;
        GdkPixbufFormat *format;
        g_autofree char *format_name = NULL;

        // FIXME: this could be much more efficient
        pixbuf = gdk_pixbuf_new_from_file (path, &error);
        if (pixbuf== NULL) {
                g_message ("Failed to load image '%s': %s", path, error->message);
                return NULL;
        }

        imported = get_import_name (path);
        format = gdk_pixbuf_get_file_info (path, NULL, NULL);
        if (gdk_pixbuf_format_is_writable (format))
                format_name = gdk_pixbuf_format_get_name (format);
        else
                format_name = g_strdup ("png");

        g_print ("Loading %s (format %s), importing to %s\n", path, format_name, imported);
        oriented = gdk_pixbuf_apply_embedded_orientation (pixbuf);

        if (!gdk_pixbuf_save (oriented, imported, format_name, &error, NULL)) {
                g_message ("Failed to import image: %s", error->message);
                return NULL;
        }

        return imported;
}

static void
file_chooser_response (GtkNativeDialog *self,
                       gint             response_id,
                       GrImageViewer   *viewer)
{
        if (response_id == GTK_RESPONSE_ACCEPT) {
                GSList *names, *l;

                names = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (self));
                for (l = names; l; l = l->next) {
                        GrRotatedImage ri;

                        ri.path = import_image (l->data);
                        ri.angle = 0;

                        add_image (viewer, &ri, TRUE);

                        g_free (ri.path);
                }
                g_slist_free_full (names, g_free);

                show_controls (viewer);
        }
}

static void
open_filechooser (GrImageViewer *viewer)
{
        GtkWidget *window;
        GtkFileChooserNative *chooser;
        g_autoptr(GtkFileFilter) filter = NULL;

        window = gtk_widget_get_ancestor (GTK_WIDGET (viewer), GTK_TYPE_APPLICATION_WINDOW);

        if (in_flatpak_sandbox () && !portals_available ()) {
                GtkWidget *dialog;

                dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                                 GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_OK,
                                                 _("Missing the desktop portals needed to open files from inside a flatpak sandbox. Please install xdg-desktop-portal and xdg-desktop-portal-gtk on your system."));
                g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
                gtk_widget_show (dialog);
                return;
        }

        chooser = gtk_file_chooser_native_new (_("Select an Image"),
                                               GTK_WINDOW (window),
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               _("Open"),
                                               _("Cancel"));
        gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (chooser), TRUE);

        filter = gtk_file_filter_new ();
        gtk_file_filter_set_name (filter, _("Image files"));
        gtk_file_filter_add_mime_type (filter, "image/*");
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);
        gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

        g_signal_connect (chooser, "response", G_CALLBACK (file_chooser_response), viewer);

        gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

void
gr_image_viewer_add_image (GrImageViewer *viewer)
{
        open_filechooser (viewer);
}

void
gr_image_viewer_remove_image (GrImageViewer *viewer)
{
        g_array_remove_index (viewer->images, viewer->index);

        if (viewer->index < viewer->images->len) {
                populate_preview (viewer);
                set_current_image (viewer);
        }
        else if (viewer->index > 0) {
                viewer->index -= 1;
                populate_preview (viewer);
                set_current_image (viewer);
        }

        if (viewer->images->len == 0) {
                gtk_stack_set_visible_child_name (GTK_STACK (viewer->stack), "placeholder");
                hide_controls (viewer);
        }
        else
                show_controls (viewer);

        g_object_notify (G_OBJECT (viewer), "images");
}

void
gr_image_viewer_rotate_image (GrImageViewer *viewer,
                              int            angle)
{
        GrRotatedImage *ri;

        g_assert (angle == 0 || angle == 90 || angle == 180 || angle == 270);

        ri = &g_array_index (viewer->images, GrRotatedImage, viewer->index);
        ri->angle = (ri->angle + angle) % 360;

        populate_preview (viewer);
        set_current_image (viewer);

        g_object_notify (G_OBJECT (viewer), "images");
}

void
gr_image_viewer_show_image (GrImageViewer *viewer,
                            int            idx)
{
        if (viewer->images->len > 0) {
                viewer->index = idx % viewer->images->len;
                set_current_image (viewer);
        }
}

int
gr_image_viewer_get_index (GrImageViewer *viewer)
{
        return viewer->images->len;
}
