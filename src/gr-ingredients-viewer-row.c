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
#include "gr-ingredient.h"


struct _GrIngredientsViewerRow
{
        GtkListBoxRow parent_instance;

        GtkWidget *unit_label;
        GtkWidget *ingredient_entry;
        GtkWidget *buttons_stack;

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

        tmp = g_strconcat (row->amount ? row->amount : "", " ", row->unit, NULL);
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
        gtk_entry_set_text (GTK_ENTRY(row->ingredient_entry), ingredient);
}

static void
gr_ingredients_viewer_row_set_size_group (GrIngredientsViewerRow *row,
                                          GtkSizeGroup           *group)
{
        if (row->group)
                gtk_size_group_remove_widget (row->group, row->unit_label);
        g_set_object (&row->group, group);
        if (row->group)
                gtk_size_group_add_widget (row->group, row->unit_label);
}

static void
gr_ingredients_viewer_row_set_editable (GrIngredientsViewerRow *row,
                                        gboolean                editable)
{
        row->editable = editable;
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), editable);
}

static void
gr_ingredients_viewer_row_set_active (GrIngredientsViewerRow *row,
                                      gboolean                active)
{
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
emit_move_up (GrIngredientsViewerRow *row)
{
        g_signal_emit (row, signals[MOVE], 0, -1);
}

static void
emit_move_down (GrIngredientsViewerRow *row)
{
        g_signal_emit (row, signals[MOVE], 0, 1);
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
                                      G_TYPE_NONE, 1, G_TYPE_INT);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredients-viewer-row.ui");

        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, unit_label);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, ingredient_entry);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, buttons_stack);

        gtk_widget_class_bind_template_callback (widget_class, emit_delete);
        gtk_widget_class_bind_template_callback (widget_class, emit_move_up);
        gtk_widget_class_bind_template_callback (widget_class, emit_move_down);
}

static GtkTreeModel *
get_ingredients_model (void)
{
        static GtkListStore *store = NULL;

g_print ("getmodel\n");
        if (store == NULL) {
                const char **names;
                int length;
                int i;

                store = gtk_list_store_new (1, G_TYPE_STRING);

                names = gr_ingredient_get_names (&length);
g_print ("Found %d\n", length);
                for (i = 0; i < length; i++) {
g_print ("Add %s\n", names[i]);
                        gtk_list_store_insert_with_values (store, NULL, -1,
                                                           0, names[i],
                                                           -1);
                }
        }

        return GTK_TREE_MODEL (store);
}

static void
gr_ingredients_viewer_row_init (GrIngredientsViewerRow *self)
{
        g_autoptr(GtkEntryCompletion) completion = NULL;

        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));

        completion = gtk_entry_completion_new ();
        gtk_entry_completion_set_model (completion, get_ingredients_model ());
        gtk_entry_completion_set_text_column (completion, 0);
        gtk_entry_set_completion (GTK_ENTRY (self->ingredient_entry), completion);
}
