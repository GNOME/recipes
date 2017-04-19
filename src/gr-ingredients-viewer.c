/* gr-ingredients-viewer.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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

#include <string.h>

#include "gr-ingredients-viewer.h"
#include "gr-ingredients-viewer-row.h"
#include "gr-ingredients-list.h"
#include "gr-utils.h"

#ifdef ENABLE_GSPELL
#include <gspell/gspell.h>
#endif


struct _GrIngredientsViewer
{
        GtkEventBox parent_instance;

        GtkWidget *title_stack;
        GtkWidget *title_entry;
        GtkWidget *title_label;
        GtkWidget *list;
        GtkWidget *add_button;

        char *title;
        char *ingredients;
        gboolean editable;

        GtkSizeGroup *group;

        GtkWidget *active_row;
};


G_DEFINE_TYPE (GrIngredientsViewer, gr_ingredients_viewer, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_TITLE,
        PROP_EDITABLE_TITLE,
        PROP_EDITABLE,
        PROP_INGREDIENTS
};

static void
gr_ingredients_viewer_finalize (GObject *object)
{
        GrIngredientsViewer *viewer = GR_INGREDIENTS_VIEWER (object);

        g_free (viewer->title);
        g_free (viewer->ingredients);

        g_clear_object (&viewer->group);

        G_OBJECT_CLASS (gr_ingredients_viewer_parent_class)->finalize (object);
}

static void
set_active_row (GrIngredientsViewer *viewer,
                GtkWidget           *row)
{
        if (viewer->active_row)
                g_object_set (viewer->active_row, "active", FALSE, NULL);
        viewer->active_row = row;
        if (viewer->active_row)
                g_object_set (viewer->active_row, "active", TRUE, NULL);
        // TODO: notify a property so other lists can unset theirs
}

static void
selected_rows_changed (GtkListBox          *list,
                       GrIngredientsViewer *viewer)
{
        GtkListBoxRow *row;

        row = gtk_list_box_get_selected_row (list);
        set_active_row (viewer, GTK_WIDGET (row));
}

static void
title_changed (GtkEntry            *entry,
               GrIngredientsViewer *viewer)
{
        // TODO mark page as unsaved
}

static void
gr_ingredients_viewer_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        GrIngredientsViewer *self = GR_INGREDIENTS_VIEWER (object);
        const char *visible;

        switch (prop_id)
          {
          case PROP_TITLE:
                g_value_set_string (value, self->title);
                break;

          case PROP_EDITABLE_TITLE:
                visible = gtk_stack_get_visible_child_name (GTK_STACK (self->title_stack));
                g_value_set_boolean (value, strcmp (visible, "entry") == 0);
                break;

          case PROP_EDITABLE:
                g_value_set_boolean (value, self->editable);
                break;

          case PROP_INGREDIENTS:
                g_value_set_string (value, self->ingredients);
                break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
delete_row (GrIngredientsViewerRow *row,
            GrIngredientsViewer    *viewer)
{
        gtk_widget_destroy (GTK_WIDGET (row));
        // TODO: mark page unsaved
}

static void
move_row (GrIngredientsViewerRow *row,
          int                     steps,
          GrIngredientsViewer    *viewer)
{
        GtkWidget *list;
        int index;

        list = gtk_widget_get_parent (GTK_WIDGET (row));
        index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row));
        gtk_list_box_unselect_row (GTK_LIST_BOX (list), GTK_LIST_BOX_ROW (row));

        g_object_ref (row);
        gtk_container_remove (GTK_CONTAINER (list), GTK_WIDGET (row));
        gtk_list_box_insert (GTK_LIST_BOX (list), GTK_WIDGET (row), index + steps);
        g_object_unref (row);

        gtk_list_box_select_row (GTK_LIST_BOX (list), GTK_LIST_BOX_ROW (row));
        // TODO: mark page unsaved
}

static void
add_row (GtkButton           *button,
         GrIngredientsViewer *viewer)
{
        GtkWidget *row;

        row = g_object_new (GR_TYPE_INGREDIENTS_VIEWER_ROW,
                            "amount", "",
                            "unit", "",
                            "ingredient", "",
                            "size-group", viewer->group,
                            "editable", viewer->editable,
                            NULL);
        g_signal_connect (row, "delete", G_CALLBACK (delete_row), viewer);
        g_signal_connect (row, "move", G_CALLBACK (move_row), viewer);

        gtk_container_add (GTK_CONTAINER (viewer->list), row);
        // TODO mark page as unsaved
}

static void
gr_ingredients_viewer_set_ingredients (GrIngredientsViewer *viewer,
                                       const char          *text)
{
        g_autoptr(GrIngredientsList) ingredients = NULL;
        g_auto(GStrv) ings = NULL;
        int i;

        g_free (viewer->ingredients);
        viewer->ingredients = g_strdup (text);

        container_remove_all (GTK_CONTAINER (viewer->list));

        ingredients = gr_ingredients_list_new (text);
        ings = gr_ingredients_list_get_ingredients (ingredients, viewer->title);
        for (i = 0; ings && ings[i]; i++) {
                g_autofree char *s = NULL;
                g_auto(GStrv) strv = NULL;
                const char *amount;
                const char *unit;
                GtkWidget *row;

                s = gr_ingredients_list_scale_unit (ingredients, viewer->title, ings[i], 1, 1);
                strv = g_strsplit (s, " ", 2);
                amount = strv[0];
                unit = strv[1] ? strv[1] : "";

                row = g_object_new (GR_TYPE_INGREDIENTS_VIEWER_ROW,
                                    "amount", amount,
                                    "unit", unit,
                                    "ingredient", ings[i],
                                    "size-group", viewer->group,
                                    "editable", viewer->editable,
                                    NULL);
                g_signal_connect (row, "delete", G_CALLBACK (delete_row), viewer);
                g_signal_connect (row, "move", G_CALLBACK (move_row), viewer);

                gtk_container_add (GTK_CONTAINER (viewer->list), row);
        }
}

static void
gr_ingredients_viewer_set_title (GrIngredientsViewer *viewer,
                                 const char          *title)
{
        g_free (viewer->title);
        viewer->title = g_strdup (title);

        gtk_label_set_label (GTK_LABEL (viewer->title_label), title);
        gtk_entry_set_text (GTK_ENTRY (viewer->title_entry), title);
}

static void
gr_ingredients_viewer_set_editable (GrIngredientsViewer *viewer,
                                    gboolean             editable)
{
        viewer->editable = editable;
        gtk_widget_set_visible (viewer->add_button, editable);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (viewer->list), editable ? GTK_SELECTION_SINGLE : GTK_SELECTION_NONE);
}

static void
gr_ingredients_viewer_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GrIngredientsViewer *self = GR_INGREDIENTS_VIEWER (object);

        switch (prop_id)
          {
          case PROP_TITLE:
                gr_ingredients_viewer_set_title (self, g_value_get_string (value));
                break;

          case PROP_EDITABLE_TITLE: {
                        gboolean editable;

                        editable = g_value_get_boolean (value);
                        gtk_stack_set_visible_child_name (GTK_STACK (self->title_stack),
                                                                     editable ? "entry" : "label");
                }
                break;

          case PROP_EDITABLE:
                gr_ingredients_viewer_set_editable (self, g_value_get_boolean (value));
                break;

          case PROP_INGREDIENTS:
                gr_ingredients_viewer_set_ingredients (self, g_value_get_string (value));
                break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_ingredients_viewer_init (GrIngredientsViewer *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->list), all_headers, self, NULL);

        self->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

#if defined(ENABLE_GSPELL) && defined(GSPELL_TYPE_ENTRY)
        {
                GspellEntry *gspell_entry;
                gspell_entry = gspell_entry_get_from_gtk_entry (GTK_ENTRY (self->title_entry));
                gspell_entry_basic_setup (gspell_entry);
        }
#endif

}

static void
gr_ingredients_viewer_class_init (GrIngredientsViewerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_ingredients_viewer_finalize;
        object_class->get_property = gr_ingredients_viewer_get_property;
        object_class->set_property = gr_ingredients_viewer_set_property;

        pspec = g_param_spec_boolean ("editable-title", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_EDITABLE_TITLE, pspec);

        pspec = g_param_spec_boolean ("editable", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_EDITABLE, pspec);

        pspec = g_param_spec_string ("title", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_TITLE, pspec);

        pspec = g_param_spec_string ("ingredients", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INGREDIENTS, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredients-viewer.ui");
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, title_stack);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, title_entry);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, title_label);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, list);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, add_button);

        gtk_widget_class_bind_template_callback (widget_class, title_changed);
        gtk_widget_class_bind_template_callback (widget_class, selected_rows_changed);
        gtk_widget_class_bind_template_callback (widget_class, add_row);
}
