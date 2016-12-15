/* gr-image-editor.c:
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

#include "gr-image-editor.h"
#include "gr-utils.h"

static void
gr_rotated_image_clear (gpointer data)
{
        GrRotatedImage *image = data;

        g_clear_pointer (&image->path, g_free);
        image->angle = 0;
        image->dark_text = FALSE;
}

GArray *
gr_rotated_image_array_new (void)
{
        GArray *a;

        a = g_array_new (TRUE, TRUE, sizeof (GrRotatedImage));
        g_array_set_clear_func (a, gr_rotated_image_clear);

        return a;
}

struct _GrImageEditor
{
        GtkBox parent_instance;

        GtkWidget *stack;
        GtkWidget *switcher;
        GtkWidget *image1;
        GtkWidget *image2;

        GArray *images;
        gboolean flip;
};

G_DEFINE_TYPE (GrImageEditor, gr_image_editor, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_FLIP,
        PROP_IMAGES,
        N_PROPS
};

GrImageEditor *
gr_image_editor_new (void)
{
        return g_object_new (GR_TYPE_IMAGE_EDITOR, NULL);
}

static void
set_thumb_image (GtkImage       *image,
                 GrRotatedImage *ri)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;

        pixbuf = load_pixbuf_fill_size (ri->path, ri->angle, 48, 32);
        gtk_image_set_from_pixbuf (image, pixbuf);
        g_object_set_data_full (G_OBJECT (image), "path", g_strdup (ri->path), g_free);
        g_object_set_data (G_OBJECT (image), "angle", GINT_TO_POINTER (ri->angle));
}

static void
add_image (GrImageEditor  *editor,
           GrRotatedImage *ri,
           gboolean        select)
{
        GtkWidget *image;

        image = gtk_image_new ();
        gtk_widget_show (image);
        gtk_container_add (GTK_CONTAINER (editor->switcher), image);

        set_thumb_image (GTK_IMAGE (image), ri);

        g_array_append_vals (editor->images, ri, 1);
        ri = &g_array_index (editor->images, GrRotatedImage, editor->images->len - 1);
        ri->path = g_strdup (ri->path);

        if (editor->images->len > 1)
                gtk_widget_show (editor->switcher);

        if (editor->images->len == 0)
                gtk_list_box_select_row (GTK_LIST_BOX (editor->switcher),
                                         gtk_list_box_get_row_at_index (GTK_LIST_BOX (editor->switcher), 0));
        else if (select)
                gtk_list_box_select_row (GTK_LIST_BOX (editor->switcher),
                                         GTK_LIST_BOX_ROW (gtk_widget_get_parent (image)));

        g_object_notify (G_OBJECT (editor), "images");
}

static void
set_images (GrImageEditor *editor,
            GArray        *array)
{
        int i;

        g_object_freeze_notify (G_OBJECT (editor));

        container_remove_all (GTK_CONTAINER (editor->switcher));
        g_array_remove_range (editor->images, 0, editor->images->len);

        g_object_notify (G_OBJECT (editor), "images");

        gtk_widget_hide (editor->switcher);

        for (i = 0; i < array->len; i++) {
                GrRotatedImage *ri = &g_array_index (array, GrRotatedImage, i);
                add_image (editor, ri, FALSE);
        }

        if (array->len == 0)
                gtk_stack_set_visible_child_name (GTK_STACK (editor->stack), "placeholder");
        else
                gtk_list_box_select_row (GTK_LIST_BOX (editor->switcher),
                                         gtk_list_box_get_row_at_index (GTK_LIST_BOX (editor->switcher), 0));

        g_object_thaw_notify (G_OBJECT (editor));
}

static void
set_flip (GrImageEditor *editor,
          gboolean       flip)
{
        if (editor->flip == flip)
                return;

        editor->flip = flip;

        gtk_container_child_set (GTK_CONTAINER (editor), editor->switcher, "pack-type", flip ? GTK_PACK_END : GTK_PACK_START, NULL);
        gtk_container_child_set (GTK_CONTAINER (editor), editor->stack, "pack-type", flip ? GTK_PACK_END : GTK_PACK_START, NULL);

        g_object_notify (G_OBJECT (editor), "flip");
}

static void
file_chooser_response (GtkNativeDialog *self,
                       gint             response_id,
                       GrImageEditor   *editor)
{
        if (response_id == GTK_RESPONSE_ACCEPT) {
                GrRotatedImage ri;
                const char *dark;

                ri.path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self));
                ri.angle = 0;

                dark = gtk_file_chooser_get_choice (GTK_FILE_CHOOSER (self), "dark");

                ri.dark_text = g_strcmp0 (dark, "true") == 0;

                add_image (editor, &ri, TRUE);

                g_free (ri.path);
        }
}

static void
open_filechooser (GrImageEditor *editor)
{
        GtkWidget *window;
        GtkFileChooserNative *chooser;
        g_autoptr(GtkFileFilter) filter = NULL;

        window = gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_APPLICATION_WINDOW);
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
        gtk_file_chooser_add_choice (GTK_FILE_CHOOSER (chooser), "dark", _("Use dark text"), NULL, NULL);

        g_signal_connect (chooser, "response", G_CALLBACK (file_chooser_response), editor);

        gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

void
gr_image_editor_add_image (GrImageEditor *editor)
{
        open_filechooser (editor);
}

void
gr_image_editor_remove_image (GrImageEditor *editor)
{
        GtkListBoxRow *row;
        g_auto(GStrv) paths = NULL;
        int idx;

        row = gtk_list_box_get_selected_row (GTK_LIST_BOX (editor->switcher));
        if (row == NULL)
                return;

        idx = gtk_list_box_row_get_index (row);

        gtk_container_remove (GTK_CONTAINER (editor->switcher), GTK_WIDGET (row));
        g_array_remove_index (editor->images, idx);

        if (idx < editor->images->len) {
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (editor->switcher), idx);
                gtk_list_box_select_row (GTK_LIST_BOX (editor->switcher), row);
        }
        else if (idx > 0) {
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (editor->switcher), idx - 1);
                gtk_list_box_select_row (GTK_LIST_BOX (editor->switcher), row);
        }

        if (editor->images->len == 0)
                gtk_stack_set_visible_child_name (GTK_STACK (editor->stack), "placeholder");
        if (editor->images->len <= 1)
                gtk_widget_hide (editor->switcher);

        g_object_notify (G_OBJECT (editor), "images");
}

static void
row_selected (GtkListBox    *switcher,
              GtkListBoxRow *row,
              GrImageEditor *editor)
{
        const char *path;
        const char *vis;
        g_autoptr (GdkPixbuf) pixbuf = NULL;
        int angle;

        if (row == NULL)
                return;

        path = (const char *) g_object_get_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (row))), "path");
        angle = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (row))), "angle"));
        pixbuf = load_pixbuf_fit_size (path, angle, 128, 128, TRUE);

        vis = gtk_stack_get_visible_child_name (GTK_STACK (editor->stack));
        if (strcmp (vis, "image1") == 0) {
                gtk_image_set_from_pixbuf (GTK_IMAGE (editor->image2), pixbuf);
                gtk_stack_set_visible_child_name (GTK_STACK (editor->stack), "image2");
        }
        else {
                gtk_image_set_from_pixbuf (GTK_IMAGE (editor->image1), pixbuf);
                gtk_stack_set_visible_child_name (GTK_STACK (editor->stack), "image1");
        }
}

void
gr_image_editor_rotate_image (GrImageEditor *editor,
                              int            angle)
{
        GtkListBoxRow *row;
        GtkWidget *image;
        int idx;
        GrRotatedImage *ri;

        g_assert (angle == 0 || angle == 90 || angle == 180 || angle == 270);

        row = gtk_list_box_get_selected_row (GTK_LIST_BOX (editor->switcher));
        if (row == NULL)
                return;

        idx = gtk_list_box_row_get_index (row);

        ri = &g_array_index (editor->images, GrRotatedImage, idx);
        ri->angle = (ri->angle + angle) % 360;

        image = gtk_bin_get_child (GTK_BIN (row));
        set_thumb_image (GTK_IMAGE (image), ri);

        row_selected (GTK_LIST_BOX (editor->switcher), row, editor);
}

static void
gr_image_editor_finalize (GObject *object)
{
        GrImageEditor *self = (GrImageEditor *)object;

        g_array_free (self->images, TRUE);

        G_OBJECT_CLASS (gr_image_editor_parent_class)->finalize (object);
}

static void
gr_image_editor_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        GrImageEditor *self = GR_IMAGE_EDITOR (object);

        switch (prop_id)
          {
          case PROP_FLIP:
                  g_value_set_boolean (value, self->flip);
                  break;

          case PROP_IMAGES:
                  g_value_set_boxed (value, self->images);
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_image_editor_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        GrImageEditor *self = GR_IMAGE_EDITOR (object);

        switch (prop_id)
          {
          case PROP_FLIP:
                  set_flip (self, g_value_get_boolean (value));
                  break;

          case PROP_IMAGES:
                  set_images (self, (GArray *) g_value_get_boxed (value));
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_image_editor_class_init (GrImageEditorClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_image_editor_finalize;
        object_class->get_property = gr_image_editor_get_property;
        object_class->set_property = gr_image_editor_set_property;

        pspec = g_param_spec_boxed ("images", NULL, NULL,
                                    G_TYPE_ARRAY,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_IMAGES, pspec);

        pspec = g_param_spec_boolean ("flip", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_FLIP, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-image-editor.ui");

        gtk_widget_class_bind_template_child (widget_class, GrImageEditor, stack);
        gtk_widget_class_bind_template_child (widget_class, GrImageEditor, switcher);
        gtk_widget_class_bind_template_child (widget_class, GrImageEditor, image1);
        gtk_widget_class_bind_template_child (widget_class, GrImageEditor, image2);

        gtk_widget_class_bind_template_callback (widget_class, row_selected);
}

static void
gr_image_editor_init (GrImageEditor *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "placeholder");

        self->images = gr_rotated_image_array_new ();
}
