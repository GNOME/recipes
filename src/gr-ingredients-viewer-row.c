/* gr-ingredients-viewer-row.c:
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

#include <glib.h>
#include <glib/gi18n.h>

#include "gr-ingredients-viewer-row.h"
#include "gr-ingredients-viewer.h"
#include "gr-ingredient.h"
#include "gr-unit.h"


struct _GrIngredientsViewerRow
{
        GtkListBoxRow parent_instance;

        GtkWidget *buttons_stack;
        GtkWidget *unit_stack;
        GtkWidget *unit_label;
        GtkWidget *unit_entry;
        GtkWidget *ingredient_stack;
        GtkWidget *ingredient_label;
        GtkWidget *ingredient_entry;
        GtkWidget *ebox;

        char *amount;
        char *unit;
        char *ingredient;

        gboolean editable;
        gboolean active;

        GtkSizeGroup *group;
};

G_DEFINE_TYPE (GrIngredientsViewerRow, gr_ingredients_viewer_row, GTK_TYPE_LIST_BOX_ROW)

enum {
        PROP_0,
        PROP_AMOUNT,
        PROP_UNIT,
        PROP_INGREDIENT,
        PROP_SIZE_GROUP,
        PROP_EDITABLE,
        PROP_ACTIVE
};

enum {
        DELETE,
        MOVE,
        EDIT,
        N_SIGNALS
};

static int signals[N_SIGNALS] = { 0, };

static void
gr_ingredients_viewer_row_finalize (GObject *object)
{
        GrIngredientsViewerRow *self = GR_INGREDIENTS_VIEWER_ROW (object);

        g_free (self->amount);
        g_free (self->unit);
        g_free (self->ingredient);

        g_clear_object (&self->group);

        G_OBJECT_CLASS (gr_ingredients_viewer_row_parent_class)->finalize (object);
}

static void
gr_ingredients_viewer_row_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
        GrIngredientsViewerRow *self = GR_INGREDIENTS_VIEWER_ROW (object);

        switch (prop_id)
          {
          case PROP_AMOUNT:
                g_value_set_string (value, self->amount);
                break;

          case PROP_UNIT:
                g_value_set_string (value, self->unit);
                break;

          case PROP_INGREDIENT:
                g_value_set_string (value, self->ingredient);
                break;

          case PROP_SIZE_GROUP:
                g_value_set_object (value, self->group);
                break;

          case PROP_EDITABLE:
                g_value_set_boolean (value, self->editable);
                break;

          case PROP_ACTIVE:
                g_value_set_boolean (value, self->active);
                break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
update_unit (GrIngredientsViewerRow *row)
{
        g_autofree char *tmp = NULL;
        const char *amount;
        const char *space;
        const char *unit;

        amount = row->amount ? row->amount : "";
        space = amount[0] ? " " : "";
        unit = row->unit ? row->unit : "";
        tmp = g_strdup_printf ("%s%s%s", amount, space, unit);
        gtk_label_set_label (GTK_LABEL (row->unit_label), tmp);
}


static void
gr_ingredients_viewer_row_set_amount (GrIngredientsViewerRow *row,
                                      const char             *amount)
{
        g_free (row->amount);
        row->amount = g_strdup (amount);
        update_unit (row);
}

static void
gr_ingredients_viewer_row_set_unit (GrIngredientsViewerRow *row,
                                    const char             *unit)
{
        g_free (row->unit);
        row->unit = g_strdup (unit);
        update_unit (row);
}

static void
gr_ingredients_viewer_row_set_ingredient (GrIngredientsViewerRow *row,
                                          const char             *ingredient)
{
        g_free (row->ingredient);
        row->ingredient = g_strdup (ingredient);
        gtk_label_set_label (GTK_LABEL (row->ingredient_label), ingredient);
}

static void
gr_ingredients_viewer_row_set_size_group (GrIngredientsViewerRow *row,
                                          GtkSizeGroup           *group)
{
        if (row->group)
                gtk_size_group_remove_widget (row->group, row->unit_stack);
        g_set_object (&row->group, group);
        if (row->group)
                gtk_size_group_add_widget (row->group, row->unit_stack);
}

static void
gr_ingredients_viewer_row_set_editable (GrIngredientsViewerRow *row,
                                        gboolean                editable)
{
        row->editable = editable;
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), editable);
}

static void save_row (GrIngredientsViewerRow *row);

static void
gr_ingredients_viewer_row_set_active (GrIngredientsViewerRow *row,
                                      gboolean                active)
{
        if (row->active && !active)
                save_row (row);

        row->active = active;
        gtk_stack_set_visible_child_name (GTK_STACK (row->buttons_stack), active ? "buttons" : "empty");
}

static void
gr_ingredients_viewer_row_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
        GrIngredientsViewerRow *self = GR_INGREDIENTS_VIEWER_ROW (object);

        switch (prop_id)
          {
          case PROP_AMOUNT:
                gr_ingredients_viewer_row_set_amount (self, g_value_get_string (value));
                break;

          case PROP_UNIT:
                gr_ingredients_viewer_row_set_unit (self, g_value_get_string (value));
                break;

          case PROP_INGREDIENT:
                gr_ingredients_viewer_row_set_ingredient (self, g_value_get_string (value));
                break;

          case PROP_SIZE_GROUP:
                gr_ingredients_viewer_row_set_size_group (self, g_value_get_object (value));
                break;

          case PROP_EDITABLE:
                gr_ingredients_viewer_row_set_editable (self, g_value_get_boolean (value));
                break;

          case PROP_ACTIVE:
                gr_ingredients_viewer_row_set_active (self, g_value_get_boolean (value));
                break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
emit_delete (GrIngredientsViewerRow *row)
{
        g_signal_emit (row, signals[DELETE], 0);
}

static void
drag_handle_clicked (GrIngredientsViewerRow *row)
{
}

static void
edit_ingredient (GrIngredientsViewerRow *row)
{
        g_autofree char *tmp = NULL;
        const char *amount;
        const char *space;
        const char *unit;

        amount = row->amount ? row->amount : "";
        space = amount[0] ? " " : "";
        unit = row->unit ? row->unit : "";
        tmp = g_strdup_printf ("%s%s%s", amount, space, unit);

        gtk_entry_set_text (GTK_ENTRY (row->unit_entry), tmp);
        gtk_entry_set_text (GTK_ENTRY (row->ingredient_entry), row->ingredient);

        gtk_stack_set_visible_child_name (GTK_STACK (row->unit_stack), "unit_entry");
        gtk_stack_set_visible_child_name (GTK_STACK (row->ingredient_stack), "ingredient_entry");
        g_signal_emit (row, signals[EDIT], 0);

}

static void
parse_unit (const char  *text,
            char       **amount,
            char       **unit)
{
        g_autofree char *tmp = NULL;
        g_autofree char **strv = NULL;

        tmp = g_strstrip (g_strdup (text));
        strv = g_strsplit (tmp, " ", 2);

        g_clear_pointer (amount, g_free);
        g_clear_pointer (unit, g_free);

        *amount = strv[0];
        if (g_strv_length (strv) > 1)
                *unit = strv[1];
}

static void
save_row (GrIngredientsViewerRow *row)
{
        const char *visible;

        visible = gtk_stack_get_visible_child_name (GTK_STACK (row->unit_stack));
        if (strcmp (visible, "unit_entry") == 0) {
                parse_unit (gtk_entry_get_text (GTK_ENTRY (row->unit_entry)), &row->amount, &row->unit);
                update_unit (row);
                gtk_stack_set_visible_child_name (GTK_STACK (row->unit_stack), "unit_label");
        }

        visible = gtk_stack_get_visible_child_name (GTK_STACK (row->ingredient_stack));
        if (strcmp (visible, "ingredient_entry") == 0) {
                row->ingredient = g_strdup (gtk_entry_get_text (GTK_ENTRY (row->ingredient_entry)));
                gtk_label_set_label (GTK_LABEL (row->ingredient_label), row->ingredient);
                gtk_stack_set_visible_child_name (GTK_STACK (row->ingredient_stack), "ingredient_label");
        }
}

static gboolean
entry_key_press (GrIngredientsViewerRow *row,
                 GdkEventKey            *event)
{
        if (event->keyval == GDK_KEY_Escape) {
                gtk_stack_set_visible_child_name (GTK_STACK (row->unit_stack), "unit_label");
                gtk_stack_set_visible_child_name (GTK_STACK (row->ingredient_stack), "ingredient_label");
                return GDK_EVENT_STOP;
        }

        return GDK_EVENT_PROPAGATE;
}

static void
gr_ingredients_viewer_row_class_init (GrIngredientsViewerRowClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_ingredients_viewer_row_finalize;
        object_class->get_property = gr_ingredients_viewer_row_get_property;
        object_class->set_property = gr_ingredients_viewer_row_set_property;

        pspec = g_param_spec_string ("amount", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_AMOUNT, pspec);

        pspec = g_param_spec_string ("unit", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_UNIT, pspec);

        pspec = g_param_spec_string ("ingredient", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INGREDIENT, pspec);

        pspec = g_param_spec_object ("size-group", NULL, NULL,
                                     GTK_TYPE_SIZE_GROUP,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_SIZE_GROUP, pspec);

        pspec = g_param_spec_boolean ("editable", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_EDITABLE, pspec);

        pspec = g_param_spec_boolean ("active", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE|G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_ACTIVE, pspec);

        signals[DELETE] = g_signal_new ("delete",
                                        G_TYPE_FROM_CLASS (object_class),
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 0);

        signals[MOVE] = g_signal_new ("move",
                                      G_TYPE_FROM_CLASS (object_class),
                                      G_SIGNAL_RUN_FIRST,
                                      0,
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 1, GR_TYPE_INGREDIENTS_VIEWER_ROW);
                signals[EDIT] = g_signal_new ("edit",
                                      G_TYPE_FROM_CLASS (object_class),
                                      G_SIGNAL_RUN_FIRST,
                                      0,
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 0);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredients-viewer-row.ui");

        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, unit_stack);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, unit_label);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, unit_entry);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, ingredient_stack);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, ingredient_label);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, ingredient_entry);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, buttons_stack);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, ebox);

        gtk_widget_class_bind_template_callback (widget_class, emit_delete);
        gtk_widget_class_bind_template_callback (widget_class, drag_handle_clicked);
        gtk_widget_class_bind_template_callback (widget_class, edit_ingredient);
        gtk_widget_class_bind_template_callback (widget_class, save_row);
        gtk_widget_class_bind_template_callback (widget_class, entry_key_press);
}

static GtkTargetEntry entries[] = {
        { "GTK_LIST_BOX_ROW", GTK_TARGET_SAME_APP, 0 }
};

static void
drag_begin (GtkWidget      *widget,
            GdkDragContext *context,
            gpointer        data)
{
        GtkAllocation alloc;
        cairo_surface_t *surface;
        cairo_t *cr;
        GtkWidget *row;
        int x, y;

        row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);

        gtk_widget_get_allocation (row, &alloc);
        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, alloc.width, alloc.height);
        cr = cairo_create (surface);

        gtk_style_context_add_class (gtk_widget_get_style_context (row), "during-dnd");
        gtk_widget_draw (row, cr);
        gtk_style_context_remove_class (gtk_widget_get_style_context (row), "during-dnd");

        gtk_widget_translate_coordinates (widget, row, 0, 0, &x, &y);
        cairo_surface_set_device_offset (surface, -x, -y);
        gtk_drag_set_icon_surface (context, surface);

        cairo_destroy (cr);
        cairo_surface_destroy (surface);
}

void
drag_data_get (GtkWidget        *widget,
               GdkDragContext   *context,
               GtkSelectionData *selection_data,
               guint             info,
               guint             time,
               gpointer          data)
{
        gtk_selection_data_set (selection_data,
                                gdk_atom_intern_static_string ("GTK_LIST_BOX_ROW"),
                                32,
                                (const guchar *)&widget,
                                sizeof (gpointer));
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
        GtkWidget *target;
        GtkWidget *row;
        GtkWidget *source;

        target = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);

        row = (gpointer)* (gpointer*)gtk_selection_data_get_data (selection_data);
        source = gtk_widget_get_ancestor (row, GTK_TYPE_LIST_BOX_ROW);

        if (target == source)
                return;

        g_signal_emit (source, signals[MOVE], 0, target);
}

static GtkTreeModel *
get_ingredients_model (void)
{
        static GtkListStore *store = NULL;

        if (store == NULL) {
                const char **names;
                int length;
                int i;

                store = gtk_list_store_new (1, G_TYPE_STRING);

                names = gr_ingredient_get_names (&length);
                for (i = 0; i < length; i++) {
                        gtk_list_store_insert_with_values (store, NULL, -1,
                                                           0, names[i],
                                                           -1);
                }
        }

        return GTK_TREE_MODEL (g_object_ref (store));
}

static void
prepend_amount (GtkTreeModel *model,
                GtkTreeIter  *iter,
                GValue       *value,
                gint          column,
                gpointer      data)
{
        GrIngredientsViewerRow *row = data;
        GtkTreeModelFilter *filter_model = GTK_TREE_MODEL_FILTER (model);
        GtkTreeModel *child_model;
        GtkTreeIter child_iter;
        g_autofree char *amount = NULL;
        g_autofree char *tmp = NULL;
        g_autofree char *unit = NULL;
        g_autofree char *new_unit = NULL;

        parse_unit (gtk_entry_get_text (GTK_ENTRY (row->unit_entry)), &amount, &unit);

        child_model = gtk_tree_model_filter_get_model (filter_model);
        gtk_tree_model_filter_convert_iter_to_child_iter (filter_model, &child_iter, iter);

        gtk_tree_model_get (child_model, &child_iter, column, &new_unit, -1);

        tmp = g_strdup_printf ("%s %s", amount, new_unit);
        g_value_set_string (value, tmp);
}

static GtkTreeModel *
get_units_model (GrIngredientsViewerRow *row)
{
        static GtkListStore *store = NULL;
        GtkTreeModel *model;
        GType types[1];

        if (store == NULL) {
                const char **names;
                int i;

                store = gtk_list_store_new (1, G_TYPE_STRING);

                names = gr_unit_get_names ();
                for (i = 0; names[i]; i++) {
                        gtk_list_store_insert_with_values (store, NULL, -1,
                                                           0, names[i],
                                                           -1);
                }
        }

        model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
        types[0] = G_TYPE_STRING;
        gtk_tree_model_filter_set_modify_func (GTK_TREE_MODEL_FILTER (model),
                                               G_N_ELEMENTS (types),
                                               types,
                                               prepend_amount,
                                               row,
                                               NULL);

        return model;
}

static void
gr_ingredients_viewer_row_init (GrIngredientsViewerRow *self)
{
        g_autoptr(GtkEntryCompletion) completion = NULL;
        g_autoptr(GtkTreeModel) ingredients_model = NULL;
        g_autoptr(GtkTreeModel) units_model = NULL;

        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));

        gtk_drag_source_set (self->ebox, GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);
        g_signal_connect (self->ebox, "drag-begin", G_CALLBACK (drag_begin), NULL);
        g_signal_connect (self->ebox, "drag-data-get", G_CALLBACK (drag_data_get), NULL);

        gtk_drag_dest_set (GTK_WIDGET (self), GTK_DEST_DEFAULT_ALL, entries, 1, GDK_ACTION_MOVE);
        g_signal_connect (self, "drag-data-received", G_CALLBACK (drag_data_received), NULL);

        ingredients_model = get_ingredients_model ();
        completion = gtk_entry_completion_new ();
        gtk_entry_completion_set_model (completion, ingredients_model);
        gtk_entry_completion_set_text_column (completion, 0);
        gtk_entry_set_completion (GTK_ENTRY (self->ingredient_entry), completion);

        units_model = get_units_model (self);
        completion = gtk_entry_completion_new ();
        gtk_entry_completion_set_model (completion, units_model);
        gtk_entry_completion_set_text_column (completion, 0);
        gtk_entry_set_completion (GTK_ENTRY (self->unit_entry), completion);
}
