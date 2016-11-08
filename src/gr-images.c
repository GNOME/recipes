/* gr-images.c
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

#include "gr-images.h"
#include "gr-utils.h"

struct _GrImages
{
	GtkBox parent_instance;

        GtkWidget *stack;
        GtkWidget *switcher;
        GtkWidget *image1;
        GtkWidget *image2;

	char **images;
        int *angles;
};

G_DEFINE_TYPE (GrImages, gr_images, GTK_TYPE_BOX)

enum {
	PROP_0,
        PROP_IMAGES,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

GrImages *
gr_images_new (void)
{
	return g_object_new (GR_TYPE_IMAGES, NULL);
}

static void
set_thumb_image (GtkImage   *image,
                 const char *path,
                 int         angle)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;

        pixbuf = load_pixbuf_fill_size (path, angle, 48, 32);
        gtk_image_set_from_pixbuf (image, pixbuf);
        g_object_set_data_full (G_OBJECT (image), "path", g_strdup (path), g_free);
        g_object_set_data (G_OBJECT (image), "angle", GINT_TO_POINTER (angle));
}

static void
add_image (GrImages   *images,
           const char *path,
           gboolean    select)
{
        GtkWidget *image;
        char **paths;
        int *angles;
        int length;
        int i;

        image = gtk_image_new ();
        gtk_widget_show (image);
        gtk_container_add (GTK_CONTAINER (images->switcher), image);

        set_thumb_image (GTK_IMAGE (image), path, 0);

        length = g_strv_length (images->images);
        paths = g_new (char*, length + 2);
        angles = g_new (int, length + 2);
        for (i = 0; i < length; i++) {
                paths[i] = images->images[i];
                angles[i] = images->angles[i];
        }
        paths[length] = g_strdup (path);
        paths[length + 1] = NULL;
        angles[length] = 0;
        angles[length + 1] = 0;

        g_free (images->images);
        images->images = paths;

        g_free (images->angles);
        images->angles = angles;

        if (length >= 1)
                gtk_widget_show (images->switcher);

        if (length == 0)
                gtk_list_box_select_row (GTK_LIST_BOX (images->switcher),
                                         gtk_list_box_get_row_at_index (GTK_LIST_BOX (images->switcher), 0));
        else if (select)
                gtk_list_box_select_row (GTK_LIST_BOX (images->switcher),
                                         GTK_LIST_BOX_ROW (gtk_widget_get_parent (image)));

        g_object_notify (G_OBJECT (images), "images");
}

static void
set_images (GrImages    *images,
            const char **paths)
{
        int i;

        g_object_freeze_notify (G_OBJECT (images));

        container_remove_all (GTK_CONTAINER (images->switcher));
        g_strfreev (images->images);
        images->images = g_new0 (char *, 1);
        g_free (images->angles);
        images->angles = NULL;

        g_object_notify (G_OBJECT (images), "images");

        if (g_strv_length ((char **)paths) == 0) {
                gtk_widget_hide (images->switcher);
                gtk_stack_set_visible_child_name (GTK_STACK (images->stack), "placeholder");
        }

        for (i = 0; paths[i]; i++) {
                add_image (images, paths[i], FALSE);
        }

        g_object_thaw_notify (G_OBJECT (images));
}

static void
file_chooser_response (GtkNativeDialog *self,
                       gint             response_id,
                       GrImages        *images)
{
        if (response_id == GTK_RESPONSE_ACCEPT) {
                g_autofree char *path = NULL;
                path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self));
                add_image (images, path, TRUE);
        }
}

static void
open_filechooser (GrImages *images)
{
        GtkWidget *window;
        GtkFileChooserNative *chooser;
        g_autoptr(GtkFileFilter) filter = NULL;

        window = gtk_widget_get_ancestor (GTK_WIDGET (images), GTK_TYPE_APPLICATION_WINDOW);
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

        g_signal_connect (chooser, "response", G_CALLBACK (file_chooser_response), images);

        gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

void
gr_images_add_image (GrImages *images)
{
        open_filechooser (images);
}

void
gr_images_remove_image (GrImages *images)
{
        GtkListBoxRow *row;
        g_auto(GStrv) paths = NULL;
        int idx;
        int i;
        int length;

        row = gtk_list_box_get_selected_row (GTK_LIST_BOX (images->switcher));
        if (row == NULL)
                return;

        idx = gtk_list_box_row_get_index (row);

        gtk_container_remove (GTK_CONTAINER (images->switcher), GTK_WIDGET (row));

        g_free (images->images[idx]);
        for (i = idx; images->images[i]; i++) {
                images->images[i] = images->images[i+1];
                images->angles[i] = images->angles[i+1];
        }

        length = g_strv_length (images->images);

        if (idx < length) {
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (images->switcher), idx);
                gtk_list_box_select_row (GTK_LIST_BOX (images->switcher), row);
        }
        else if (idx > 0) {
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (images->switcher), idx - 1);
                gtk_list_box_select_row (GTK_LIST_BOX (images->switcher), row);
        }

        if (length == 0)
                gtk_stack_set_visible_child_name (GTK_STACK (images->stack), "placeholder");
        if (length <= 1)
                gtk_widget_hide (images->switcher);

        g_object_notify (G_OBJECT (images), "images");
}

static void
row_selected (GtkListBox    *switcher,
              GtkListBoxRow *row,
              GrImages      *images)
{
        GtkWidget *image;
        const char *path;
        const char *vis;
        g_autoptr (GdkPixbuf) pixbuf = NULL;
        int angle;

        if (row == NULL)
                return;

        path = (const char *) g_object_get_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (row))), "path");
        angle = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (row))), "angle"));
        pixbuf = load_pixbuf_fit_size (path, angle, 128, 128);

        vis = gtk_stack_get_visible_child_name (GTK_STACK (images->stack));
        if (strcmp (vis, "image1") == 0) {
                gtk_image_set_from_pixbuf (GTK_IMAGE (images->image2), pixbuf);
                gtk_stack_set_visible_child_name (GTK_STACK (images->stack), "image2");
        }
        else {
                gtk_image_set_from_pixbuf (GTK_IMAGE (images->image1), pixbuf);
                gtk_stack_set_visible_child_name (GTK_STACK (images->stack), "image1");
        }
}

void
gr_images_rotate_image (GrImages *images,
                        int       angle)
{
        GtkListBoxRow *row;
        GtkWidget *image;
        int idx;

        g_assert (angle == 0 || angle == 90 || angle == 180 || angle == 270);

        row = gtk_list_box_get_selected_row (GTK_LIST_BOX (images->switcher));
        if (row == NULL)
                return;

        idx = gtk_list_box_row_get_index (row);
        images->angles[idx] = (images->angles[idx] + angle) % 360;
        g_print ("angle now %d]n", images->angles[idx]);

        image = gtk_bin_get_child (GTK_BIN (row));
        set_thumb_image (GTK_IMAGE (image), images->images[idx], images->angles[idx]);

        row_selected (GTK_LIST_BOX (images->switcher), row, images);
}

static void
gr_images_finalize (GObject *object)
{
	GrImages *self = (GrImages *)object;

        g_strfreev (self->images);

	G_OBJECT_CLASS (gr_images_parent_class)->finalize (object);
}

static void
gr_images_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
	GrImages *self = GR_IMAGES (object);

	switch (prop_id)
	  {
          case PROP_IMAGES:
                  g_value_set_boxed (value, self->images);
                  break;

	  default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	  }
}

static void
gr_images_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
	GrImages *self = GR_IMAGES (object);

	switch (prop_id)
	  {
          case PROP_IMAGES:
                  set_images (self, (const char **) g_value_get_boxed (value));
                  break;

	  default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	  }
}

static void
gr_images_class_init (GrImagesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

	object_class->finalize = gr_images_finalize;
	object_class->get_property = gr_images_get_property;
	object_class->set_property = gr_images_set_property;

        pspec = g_param_spec_boxed ("images", NULL, NULL,
                                    G_TYPE_STRV,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_IMAGES, pspec);


	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-images.ui");

        gtk_widget_class_bind_template_child (widget_class, GrImages, stack);
        gtk_widget_class_bind_template_child (widget_class, GrImages, switcher);
        gtk_widget_class_bind_template_child (widget_class, GrImages, image1);
        gtk_widget_class_bind_template_child (widget_class, GrImages, image2);

        gtk_widget_class_bind_template_callback (widget_class, row_selected);
}

static void
gr_images_init (GrImages *self)
{
	gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "placeholder");

        self->images = g_new0 (char *, 1);
}
