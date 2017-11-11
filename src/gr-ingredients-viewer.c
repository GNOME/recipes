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
 #include "gr-ingredient.h"
 #include "gr-utils.h"
 #include "gr-number.h"
 #include "gr-convert-units.h"
 #include "gr-unit.h"
 
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
        GtkWidget *add_row_box;

        char *title;
        gboolean editable;

        GtkSizeGroup *group;

        GtkWidget *active_row;

        GtkWidget *drag_row;
        GtkWidget *row_before;
        GtkWidget *row_after;

        double scale;
};


G_DEFINE_TYPE (GrIngredientsViewer, gr_ingredients_viewer, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_TITLE,
        PROP_EDITABLE_TITLE,
        PROP_EDITABLE,
        PROP_ACTIVE,
        PROP_INGREDIENTS,
        PROP_SCALE_NUM,
        PROP_SCALE_DENOM,
        PROP_SCALE
};

enum {
        DELETE,
        N_SIGNALS
};

static int signals[N_SIGNALS] = { 0, };

static void
gr_ingredients_viewer_finalize (GObject *object)
{
        GrIngredientsViewer *viewer = GR_INGREDIENTS_VIEWER (object);

        g_free (viewer->title);

        g_clear_object (&viewer->group);

        G_OBJECT_CLASS (gr_ingredients_viewer_parent_class)->finalize (object);
}

static void
set_active_row (GrIngredientsViewer *viewer,
                GtkWidget           *row)
{
        gboolean was_active = FALSE;
        gboolean active = FALSE;

        if (viewer->active_row == row)
                return;

        if (viewer->active_row) {
                g_object_set (viewer->active_row, "active", FALSE, NULL);
                was_active = TRUE;
        }
        viewer->active_row = row;
        if (viewer->active_row) {
                g_object_set (viewer->active_row, "active", TRUE, NULL);
                active = TRUE;
        }
        if (active != was_active)
                g_object_notify (G_OBJECT (viewer), "active");
}

static void
row_activated (GtkListBox          *list,
               GtkListBoxRow       *row,
               GrIngredientsViewer *viewer)
{
        set_active_row (viewer, GTK_WIDGET (row));
}

static void
title_changed (GtkEntry            *entry,
               GParamSpec          *pspec,
               GrIngredientsViewer *viewer)
{
        g_free (viewer->title);
        viewer->title = g_strdup (gtk_entry_get_text (entry));

        g_object_notify (G_OBJECT (viewer), "title");
}

static char *
collect_ingredients (GrIngredientsViewer *viewer)
{
        GString *s;
        GList *children, *l;

        set_active_row (viewer, NULL);

        s = g_string_new ("");

        children = gtk_container_get_children (GTK_CONTAINER (viewer->list));
        for (l = children; l; l = l->next) {
                GrIngredientsViewerRow *row = l->data;
                double amount;
                GrUnit unit;
                g_autofree char *ingredient = NULL;
                const char *id;
                const char *unit_name;

                g_object_get (row,
                              "value", &amount,
                              "unit", &unit,
                              "ingredient", &ingredient,
                              NULL);

                id = gr_ingredient_get_id (ingredient);

                unit_name = gr_unit_get_abbreviation (unit);

                if (s->len > 0)
                        g_string_append (s, "\n");
                g_string_append_printf (s, "%g\t%s\t%s\t%s",
                                        amount, unit_name, id ? id : ingredient, viewer->title);
        }
        g_list_free (children);

        return g_string_free (s, FALSE);
}

GtkWidget *
gr_ingredients_viewer_has_error (GrIngredientsViewer *viewer)
{
        GList *children, *l;

        set_active_row (viewer, NULL);

        children = gtk_container_get_children (GTK_CONTAINER (viewer->list));
        for (l = children; l; l = l->next) {
                GrIngredientsViewerRow *row = l->data;
                GtkWidget *error_field;

                error_field = gr_ingredients_viewer_row_has_error (row);
                if (error_field)
                        return error_field;
        }

        return NULL;
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

          case PROP_INGREDIENTS: {
                        g_autofree char *ingredients = NULL;
                        ingredients = collect_ingredients (self);
                        g_value_set_string (value, ingredients);
                }
                break;

          case PROP_ACTIVE:
                g_value_set_boolean (value, self->active_row != NULL);
                break;

          case PROP_SCALE:
                g_value_set_double (value, self->scale);
                break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
delete_row (GrIngredientsViewerRow *row,
            GrIngredientsViewer    *viewer)
{
        if ((GtkWidget*)row == viewer->active_row)
                set_active_row (viewer, NULL);
        gtk_widget_destroy (GTK_WIDGET (row));
        g_object_notify (G_OBJECT (viewer), "ingredients");
}

static void
move_row (GtkWidget           *source,
          GtkWidget           *target,
          GrIngredientsViewer *viewer)
{
        GtkWidget *source_parent;
        GtkWidget *target_parent;
        int index;

        index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (target));

        source_parent = gtk_widget_get_parent (source);
        target_parent = gtk_widget_get_parent (target);

        g_object_ref (source);
        gtk_container_remove (GTK_CONTAINER (source_parent), source);
        gtk_list_box_insert (GTK_LIST_BOX (target_parent), source, index);
        g_object_unref (source);

        gtk_list_box_select_row (GTK_LIST_BOX (target_parent), GTK_LIST_BOX_ROW (source));

        g_object_notify (G_OBJECT (viewer), "ingredients");
}

static void
edit_ingredient_row (GrIngredientsViewerRow *row,
                     GrIngredientsViewer    *viewer)
{
        set_active_row (viewer, GTK_WIDGET (row));
        g_object_notify (G_OBJECT (viewer), "ingredients");
}

static void
add_row (GrIngredientsViewer *viewer)
{
        GtkWidget *row;

        row = g_object_new (GR_TYPE_INGREDIENTS_VIEWER_ROW,
                            "value", 0.0,
                            "unit", GR_UNIT_UNKNOWN,
                            "ingredient", "",
                            "size-group", viewer->group,
                            "editable", viewer->editable,
                            NULL);
        g_signal_connect (row, "delete", G_CALLBACK (delete_row), viewer);
        g_signal_connect (row, "move", G_CALLBACK (move_row), viewer);
        g_signal_connect (row, "notify::ingredient" , G_CALLBACK(edit_ingredient_row), viewer);

        gtk_container_add (GTK_CONTAINER (viewer->list), row);
        gtk_widget_grab_focus (row);
        g_object_notify (G_OBJECT (viewer), "ingredients");
}

static void
remove_list (GtkButton           *button,
             GrIngredientsViewer *viewer)
{
        g_signal_emit (viewer, signals[DELETE], 0);
}

static void
gr_ingredients_viewer_set_ingredients (GrIngredientsViewer *viewer,
                                       const char          *text)
{
        g_autoptr(GrIngredientsList) ingredients = NULL;
        g_auto(GStrv) ings = NULL;
        int i;

        container_remove_all (GTK_CONTAINER (viewer->list));

        ingredients = gr_ingredients_list_new (text);
        ings = gr_ingredients_list_get_ingredients (ingredients, viewer->title);
        for (i = 0; ings && ings[i]; i++) {
                double amount;
                GrUnit unit;
                GtkWidget *row;
                g_autoptr(GString) s = NULL;
                s = g_string_new ("");
                double scale = viewer->scale;

                unit = gr_ingredients_list_get_unit (ingredients, viewer->title, ings[i]);
                amount = gr_ingredients_list_get_amount (ingredients, viewer->title, ings[i]) * scale;

                row = g_object_new (GR_TYPE_INGREDIENTS_VIEWER_ROW,
                                    "unit", unit,
                                    "value", amount,
                                    "ingredient", ings[i],
                                    "size-group", viewer->group,
                                    "editable", viewer->editable,
                                    NULL);

                g_signal_connect (row, "delete", G_CALLBACK (delete_row), viewer);
                g_signal_connect (row, "move", G_CALLBACK (move_row), viewer);
                g_signal_connect (row, "edit", G_CALLBACK (edit_ingredient_row), viewer);

                gtk_container_add (GTK_CONTAINER (viewer->list), row);
        }
}

static void
gr_ingredients_viewer_set_title (GrIngredientsViewer *viewer,
                                 const char          *title)
{
        const char *display_title;

        display_title = g_dgettext (GETTEXT_PACKAGE "-data", title);
        gtk_label_set_label (GTK_LABEL (viewer->title_label), display_title);
        gtk_entry_set_text (GTK_ENTRY (viewer->title_entry), display_title);

        g_free (viewer->title);
        viewer->title = g_strdup (title);
}

static GtkTargetEntry entries[] = {
        { "GTK_LIST_BOX_ROW", GTK_TARGET_SAME_APP, 0 }
};

void
gr_ingredients_viewer_set_drag_row (GrIngredientsViewer *viewer,
                                    GtkWidget           *row)
{
        if (viewer->drag_row) {
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->drag_row), "drag-hover");
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->drag_row), "drag-row");
        }

        viewer->drag_row = row;

        if (viewer->drag_row)
                gtk_style_context_add_class (gtk_widget_get_style_context (viewer->drag_row), "drag-row");
}

static GtkListBoxRow *
get_last_row (GtkListBox *list)
{
        int i;
        GtkListBoxRow *row = NULL;

        for (i = 0; ; i++) {
                GtkListBoxRow *tmp = gtk_list_box_get_row_at_index (list, i);
                if (tmp == NULL)
                        return row;
                row = tmp;
        }
        return row;
}

static GtkListBoxRow *
get_row_before (GtkListBox    *list,
                GtkListBoxRow *row)
{
        int pos = gtk_list_box_row_get_index (row);
        return gtk_list_box_get_row_at_index (list, pos - 1);
}

static GtkListBoxRow *
get_row_after (GtkListBox    *list,
               GtkListBoxRow *row)
{
        int pos = gtk_list_box_row_get_index (row);
        return gtk_list_box_get_row_at_index (list, pos + 1);
}

static void
drag_data_received (GtkWidget        *widget,
                    GdkDragContext   *context,
                    gint              x,
                    gint              y,
                    GtkSelectionData *selection_data,
                    guint             info,
                    guint32           time,
                    gpointer          data)
{
        GrIngredientsViewer *viewer = GR_INGREDIENTS_VIEWER (data);
        GtkWidget *row_before;
        GtkWidget *row_after;
        GtkWidget *parent;
        GtkWidget *row;
        GtkWidget *source;
        int pos;

        row_before = viewer->row_before;
        row_after = viewer->row_after;

        viewer->row_before = NULL;
        viewer->row_after = NULL;

        if (row_before)
                gtk_style_context_remove_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
        if (row_after)
                gtk_style_context_remove_class (gtk_widget_get_style_context (row_after), "drag-hover-top");

        row = (gpointer)* (gpointer*)gtk_selection_data_get_data (selection_data);
        source = gtk_widget_get_ancestor (row, GTK_TYPE_LIST_BOX_ROW);

        gtk_style_context_remove_class (gtk_widget_get_style_context (source), "drag-row");
        gtk_style_context_remove_class (gtk_widget_get_style_context (source), "drag-hover");

        if (source == row_after)
                return;

        g_object_ref (source);
        gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (source)), source);

        if (row_after) {
                pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row_after));
                parent = gtk_widget_get_parent (row_after);
        }
        else {
                pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row_before)) + 1;
                parent = gtk_widget_get_parent (row_before);
        }

        gtk_list_box_insert (GTK_LIST_BOX (parent), source, pos);
        g_object_unref (source);
}

static gboolean
drag_motion (GtkWidget      *widget,
             GdkDragContext *context,
             int             x,
             int             y,
             guint           time,
             gpointer        data)
{
        GrIngredientsViewer *viewer = GR_INGREDIENTS_VIEWER (data);
        GtkAllocation alloc;
        GtkWidget *row;
        int hover_row_y;
        int hover_row_height;

        row = GTK_WIDGET (gtk_list_box_get_row_at_y (GTK_LIST_BOX (widget), y));

        if (viewer->drag_row)
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->drag_row), "drag-hover");

        if (viewer->row_before)
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->row_before), "drag-hover-bottom");
        if (viewer->row_after)
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->row_after), "drag-hover-top");

        if (row) {
                gtk_widget_get_allocation (row, &alloc);
                hover_row_y = alloc.y;
                hover_row_height = alloc.height;

                if (y < hover_row_y + hover_row_height/2) {
                        viewer->row_after = row;
                        viewer->row_before = GTK_WIDGET (get_row_before (GTK_LIST_BOX (widget), GTK_LIST_BOX_ROW (row)));
                }
                else {
                        viewer->row_before = row;
                        viewer->row_after = GTK_WIDGET (get_row_after (GTK_LIST_BOX (widget), GTK_LIST_BOX_ROW (row)));
                }
        }
        else {
                viewer->row_before = GTK_WIDGET (get_last_row (GTK_LIST_BOX (widget)));
                viewer->row_after = NULL;
        }

        if (viewer->drag_row &&
            (viewer->drag_row == viewer->row_before ||
             viewer->drag_row == viewer->row_after)) {
                gtk_style_context_add_class (gtk_widget_get_style_context (viewer->drag_row), "drag-hover");
                return FALSE;
        }

        if (viewer->row_before)
                gtk_style_context_add_class (gtk_widget_get_style_context (viewer->row_before), "drag-hover-bottom");
        if (viewer->row_after)
                gtk_style_context_add_class (gtk_widget_get_style_context (viewer->row_after), "drag-hover-top");

        return TRUE;
}

static void
drag_leave (GtkWidget      *widget,
            GdkDragContext *context,
            guint           time,
            gpointer        data)
{
        GrIngredientsViewer *viewer = GR_INGREDIENTS_VIEWER (data);

        if (viewer->drag_row)
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->drag_row), "drag-hover");
        if (viewer->row_before)
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->row_before), "drag-hover-bottom");
        if (viewer->row_after)
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->row_after), "drag-hover-top");
}

static void
gr_ingredients_viewer_set_editable (GrIngredientsViewer *viewer,
                                    gboolean             editable)
{
        GList *children, *l;

        viewer->editable = editable;
        gtk_widget_set_visible (viewer->add_row_box, editable);

        children = gtk_container_get_children (GTK_CONTAINER (viewer->list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;

                if (GR_IS_INGREDIENTS_VIEWER_ROW (row))
                        g_object_set (row, "editable", viewer->editable, NULL);
        }
        g_list_free (children);

        if (editable) {
                gtk_style_context_add_class (gtk_widget_get_style_context (viewer->list), "editable");
                gtk_drag_dest_set (viewer->list,
                                   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                                   entries, 1, GDK_ACTION_MOVE);
                g_signal_connect (viewer->list, "drag-data-received",
                                  G_CALLBACK (drag_data_received), viewer);
                g_signal_connect (viewer->list, "drag-motion",
                                  G_CALLBACK (drag_motion), viewer);
                g_signal_connect (viewer->list, "drag-leave",
                                  G_CALLBACK (drag_leave), viewer);
        }
        else {
                gtk_style_context_remove_class (gtk_widget_get_style_context (viewer->list), "editable");
                gtk_drag_dest_unset (viewer->list);
                g_signal_handlers_disconnect_by_func (viewer->list, drag_data_received, viewer);
                g_signal_handlers_disconnect_by_func (viewer->list, drag_motion, viewer);
                g_signal_handlers_disconnect_by_func (viewer->list, drag_leave, viewer);
        }
}

static void
gr_ingredients_viewer_set_active (GrIngredientsViewer *viewer,
                                  gboolean             active)
{
        if (!active)
                set_active_row (viewer, NULL);
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

          case PROP_ACTIVE:
                gr_ingredients_viewer_set_active (self, g_value_get_boolean (value));
                break;

          case PROP_SCALE:
                self->scale = g_value_get_double (value);
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

        self->scale = 1.0;
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

        pspec = g_param_spec_boolean ("active", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_ACTIVE, pspec);

        pspec = g_param_spec_string ("title", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_TITLE, pspec);

        pspec = g_param_spec_string ("ingredients", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INGREDIENTS, pspec);

        pspec = g_param_spec_int ("scale-num", NULL, NULL,
                                  1, G_MAXINT, 1,
                                  G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_SCALE_NUM, pspec);

        pspec = g_param_spec_int ("scale-denom", NULL, NULL,
                                  1, G_MAXINT, 1,
                                  G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_SCALE_DENOM, pspec);

        pspec = g_param_spec_double ("scale", NULL, NULL,
                                  0.0, G_MAXDOUBLE, 1.0,
                                  G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_SCALE, pspec);

        signals[DELETE] = g_signal_new ("delete",
                                        G_TYPE_FROM_CLASS (object_class),
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 0);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredients-viewer.ui");
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, title_stack);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, title_entry);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, title_label);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, list);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, add_row_box);

        gtk_widget_class_bind_template_callback (widget_class, title_changed);
        gtk_widget_class_bind_template_callback (widget_class, row_activated);
        gtk_widget_class_bind_template_callback (widget_class, add_row);
        gtk_widget_class_bind_template_callback (widget_class, remove_list);
}
