/* gr-chef-dialog.c:
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

#ifdef ENABLE_GSPELL
#include <gspell/gspell.h>
#endif

#include "gr-chef-dialog.h"
#include "gr-chef.h"
#include "gr-recipe-store.h"
#include "gr-window.h"
#include "gr-utils.h"
#include "gr-chef-tile.h"
#include "gr-image.h"


struct _GrChefDialog
{
        GtkDialog parent_instance;

        GtkWidget *fullname;
        GtkWidget *name;
        GtkWidget *description;
        GtkWidget *image;
        GtkWidget *button;
        GtkWidget *error_revealer;
        GtkWidget *error_label;
        GtkWidget *create_button;
        GtkWidget *chef_popover;
        GtkWidget *chef_list;
        GtkWidget *save_button;

        GPtrArray *additions;
        GPtrArray *removals;

        GrImage *ri;
        GCancellable *cancellable;

        GrChef *chef;
};

G_DEFINE_TYPE (GrChefDialog, gr_chef_dialog, GTK_TYPE_DIALOG)

static int done_signal;

static void
dismiss_error (GrChefDialog *self)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->error_revealer), FALSE);
}

static void
update_image (GrChefDialog *self)
{
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);

        if (self->ri) {
                self->cancellable = g_cancellable_new ();

                gr_image_load (self->ri, 64, 64, TRUE, self->cancellable, gr_image_set_pixbuf, self->image);
                gtk_style_context_remove_class (gtk_widget_get_style_context (self->image), "dim-label");
        }
        else {
                gtk_image_set_from_icon_name (GTK_IMAGE (self->image), "camera-photo-symbolic", 1);
                gtk_image_set_pixel_size (GTK_IMAGE (self->image), 24);
                gtk_style_context_add_class (gtk_widget_get_style_context (self->image), "dim-label");
        }
}

static void
persist_changes (GrChefDialog *self)
{
        int i;

        g_ptr_array_set_size (self->additions, 0);
        for (i = 0; i < self->removals->len; i++)
                remove_image (g_ptr_array_index (self->removals, i));
        g_ptr_array_set_size (self->removals, 0);
}


static void
revert_changes (GrChefDialog *self)
{
        int i;

        g_ptr_array_set_size (self->removals, 0);
        for (i = 0; i < self->additions->len; i++)
                remove_image (g_ptr_array_index (self->additions, i));
        g_ptr_array_set_size (self->additions, 0);
}

static void
field_changed (GrChefDialog *self)
{
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self),
                                           GTK_RESPONSE_APPLY,
                                           TRUE);
}

static void
file_chooser_response (GtkNativeDialog *self,
                       gint             response_id,
                       GrChefDialog   *prefs)
{
        if (response_id == GTK_RESPONSE_ACCEPT) {
                g_autofree char *path = NULL;

                path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self));

                if (prefs->ri)
                        g_ptr_array_add (prefs->removals, g_strdup (gr_image_get_path (prefs->ri)));

                g_clear_object (&prefs->ri);
                prefs->ri = gr_image_new (gr_app_get_soup_session (GR_APP (g_application_get_default ())),
                                          "local",
import_image (path));

                if (prefs->ri)
                        g_ptr_array_add (prefs->additions, g_strdup (gr_image_get_path (prefs->ri)));

                update_image (prefs);
                field_changed (prefs);
        }
}

static void
image_button_clicked (GrChefDialog *self)
{
        GtkFileChooserNative *chooser;
        g_autoptr(GtkFileFilter) filter = NULL;

        if (!portal_available (GTK_WINDOW (self), "org.freedesktop.portal.FileChooser"))
                return;

        chooser = gtk_file_chooser_native_new (_("Select an Image"),
                                               GTK_WINDOW (self),
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               _("Open"),
                                               _("Cancel"));
        gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (chooser), TRUE);

        filter = gtk_file_filter_new ();
        gtk_file_filter_set_name (filter, _("Image files"));
        gtk_file_filter_add_mime_type (filter, "image/*");
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

        g_signal_connect (chooser, "response", G_CALLBACK (file_chooser_response), self);

        gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}

static void
gr_chef_dialog_finalize (GObject *object)
{
        GrChefDialog *self = GR_CHEF_DIALOG (object);

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
        g_clear_object (&self->ri);

        revert_changes (self);
        g_clear_pointer (&self->removals, g_ptr_array_unref);

        g_clear_object (&self->chef);

        G_OBJECT_CLASS (gr_chef_dialog_parent_class)->finalize (object);
}

static gboolean
save_chef_dialog (GrChefDialog  *self,
                  GError        **error)
{
        GrRecipeStore *store;
        const char *id;
        const char *name;
        const char *fullname;
        const char *image_path;
        g_autofree char *description = NULL;
        GtkTextBuffer *buffer;
        GtkTextIter start, end;
        gboolean ret;

        if (gr_chef_is_readonly (self->chef))
                return TRUE;

        id = gr_chef_get_id (self->chef);
        name = gtk_entry_get_text (GTK_ENTRY (self->name));
        fullname = gtk_entry_get_text (GTK_ENTRY (self->fullname));
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->description));
        gtk_text_buffer_get_bounds (buffer, &start, &end);
        description = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
        image_path = self->ri ? gr_image_get_path (self->ri) : NULL;

        store = gr_recipe_store_get ();

        if (id != NULL && id[0] != '\0'){
                g_object_set (self->chef,
                              "fullname", fullname,
                              "name", name,
                              "description", description,
                              "image-path", image_path,
                              NULL);
                ret = gr_recipe_store_update_chef (store, self->chef, id, error);
        }
        else {
                g_auto(GStrv) strv = NULL;
                g_autofree char *new_id = NULL;

                strv = g_strsplit (fullname, " ", -1);
                if (strv[1])
                        new_id = generate_id (strv[0], "_", strv[1], NULL);
                else
                        new_id = generate_id (strv[0], NULL);

                g_object_set (self->chef,
                              "id", new_id,
                              "fullname", fullname,
                              "name", name,
                              "description", description,
                              "image-path", image_path,
                              NULL);

                ret = gr_recipe_store_add_chef (store, self->chef, error);
        }

        return ret;
}

static void
save_chef (GrChefDialog *self)
{
        g_autoptr(GError) error = NULL;

        if (!save_chef_dialog (self, &error)) {
                revert_changes (self);
                gtk_label_set_label (GTK_LABEL (self->error_label), error->message);
                gtk_revealer_set_reveal_child (GTK_REVEALER (self->error_revealer), TRUE);
                return;
        }

        persist_changes (self);

        g_signal_emit (self, done_signal, 0, self->chef);
}

static void
close_dialog (GrChefDialog *self)
{
        revert_changes (self);
        g_signal_emit (self, done_signal, 0, NULL);
}

static void
gr_chef_dialog_init (GrChefDialog *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->additions = g_ptr_array_new_with_free_func (g_free);
        self->removals = g_ptr_array_new_with_free_func (g_free);

#ifdef ENABLE_GSPELL
        {
                GspellTextView *gspell_view;

                gspell_view = gspell_text_view_get_from_gtk_text_view (GTK_TEXT_VIEW (self->description));
                gspell_text_view_basic_setup (gspell_view);
        }
#endif
}

static void
gr_chef_dialog_set_chef (GrChefDialog *self,
                         GrChef       *chef)
{
        gboolean same_chef;
        GrRecipeStore *store;

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
        g_clear_object (&self->ri);

        revert_changes (self);

        store = gr_recipe_store_get ();

        same_chef = self->chef != NULL && self->chef == chef;

        if (gr_chef_is_readonly (chef) &&
            strcmp (gr_chef_get_id (chef), gr_recipe_store_get_user_key (store)) == 0)
                g_object_set (chef, "readonly", FALSE, NULL);

        if (g_set_object (&self->chef, chef)) {
                const char *fullname;
                const char *name;
                const char *description;
                const char *image_path;

                fullname = gr_chef_get_fullname (chef);
                name = gr_chef_get_name (chef);
                description = gr_chef_get_description (chef);
                image_path = gr_chef_get_image (chef);

                gtk_entry_set_text (GTK_ENTRY (self->fullname), fullname ? fullname : "");
                gtk_entry_set_text (GTK_ENTRY (self->name), name ? name : "");
                gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->description)),
                                          description ? description : "", -1);

                self->ri = gr_image_new (gr_app_get_soup_session (GR_APP (g_application_get_default ())), gr_chef_get_id (chef), image_path);


                if (gr_chef_is_readonly (chef)) {
                        gtk_widget_set_sensitive (self->fullname, FALSE);
                        gtk_widget_set_sensitive (self->name, FALSE);
                        gtk_widget_set_sensitive (self->description, FALSE);
                        gtk_widget_set_sensitive (self->button, FALSE);
                }
                else {
                        gtk_widget_set_sensitive (self->fullname, TRUE);
                        gtk_widget_set_sensitive (self->name, TRUE);
                        gtk_widget_set_sensitive (self->description, TRUE);
                        gtk_widget_set_sensitive (self->button, TRUE);
                }

                update_image (self);
        }

        gtk_dialog_set_response_sensitive (GTK_DIALOG (self),
                                           GTK_RESPONSE_APPLY,
                                           same_chef);
}

static void
chef_selected (GrChefDialog  *dialog,
               GtkListBoxRow *row)
{
        GrChef *chef;

        chef = g_object_get_data (G_OBJECT (row), "chef");
        gr_chef_dialog_set_chef (dialog, chef);
        gtk_popover_popdown (GTK_POPOVER (dialog->chef_popover));
}

static void
gr_chef_dialog_class_init (GrChefDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = gr_chef_dialog_finalize;

        done_signal = g_signal_new ("done",
                                    G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL,
                                    NULL,
                                    G_TYPE_NONE, 1, GR_TYPE_CHEF);

        gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                     "/org/gnome/Recipes/gr-chef-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, fullname);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, name);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, description);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, image);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, button);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, error_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, error_label);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, create_button);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, chef_popover);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, chef_list);
        gtk_widget_class_bind_template_child (widget_class, GrChefDialog, save_button);

        gtk_widget_class_bind_template_callback (widget_class, dismiss_error);
        gtk_widget_class_bind_template_callback (widget_class, image_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, chef_selected);
        gtk_widget_class_bind_template_callback (widget_class, save_chef);
        gtk_widget_class_bind_template_callback (widget_class, close_dialog);
        gtk_widget_class_bind_template_callback (widget_class, field_changed);
}

static void
add_chef_row (GrChefDialog *dialog,
              GrChef       *chef)
{
        GtkWidget *row;

        row = gtk_label_new ("");
        g_object_set (row, "margin", 10, NULL);

        if (chef) {
                gtk_label_set_label (GTK_LABEL (row), gr_chef_get_fullname (chef));
                gtk_label_set_xalign (GTK_LABEL (row), 0.0);
        }
        else {
                g_autofree char *tmp = NULL;

                tmp = g_strdup_printf ("<b>%s</b>", _("New Chef"));
                gtk_label_set_markup (GTK_LABEL (row), tmp);
        }

        gtk_widget_show (row);

        gtk_container_add (GTK_CONTAINER (dialog->chef_list), row);
        row = gtk_widget_get_parent (row);
        if (chef)
                g_object_set_data_full (G_OBJECT (row), "chef", g_object_ref (chef), g_object_unref);
        else
                g_object_set_data_full (G_OBJECT (row), "chef", g_object_new (GR_TYPE_CHEF,
                                                                              "id", "",
                                                                              "name", "",
                                                                              "fullname", "",
                                                                              "description", "",
                                                                              "image-path", "",
                                                                              "readonly", FALSE,
                                                                              NULL), g_object_unref);
}

static void
populate_chef_list (GrChefDialog *dialog)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;

        store = gr_recipe_store_get ();
        keys = gr_recipe_store_get_chef_keys (store, &length);

        for (i = 0; keys[i]; i++) {
                g_autoptr(GrChef) chef = gr_recipe_store_get_chef (store, keys[i]);

                if (g_strcmp0 (gr_chef_get_id (chef), gr_recipe_store_get_user_key (store)) == 0)
                        add_chef_row (dialog, chef);
                else if (!gr_chef_is_readonly (chef))
                        add_chef_row (dialog, chef);
        }

        add_chef_row (dialog, NULL);
}

static void
gr_chef_dialog_can_create (GrChefDialog *dialog,
                           gboolean      create)
{
        if (create) {
                gtk_widget_show (dialog->create_button);
                populate_chef_list (dialog);
        }
        else {
                gtk_widget_hide (dialog->create_button);
                gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                                   GTK_RESPONSE_APPLY,
                                                   !gr_chef_is_readonly (dialog->chef));
        }
}

GrChefDialog *
gr_chef_dialog_new (GrChef   *chef,
                    gboolean  create)
{
        GrChefDialog *dialog;
        gboolean use_header_bar;

        g_object_get (gtk_settings_get_default (),
                      "gtk-dialogs-use-header", &use_header_bar,
                      NULL);

        dialog = g_object_new (GR_TYPE_CHEF_DIALOG,
                               "use-header-bar", use_header_bar,
                               NULL);

        gtk_widget_realize (GTK_WIDGET (dialog));
        gdk_window_set_functions (gtk_widget_get_window (GTK_WIDGET (dialog)),
                                  GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_MAXIMIZE);

        gr_chef_dialog_set_chef (dialog, chef);
        gr_chef_dialog_can_create (dialog, create);

        return dialog;
}

GrChef *
gr_chef_dialog_get_chef (GrChefDialog *dialog)
{
        return dialog->chef;
}
