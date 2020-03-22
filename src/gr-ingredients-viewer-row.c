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
#include <stdlib.h>

#include "gr-ingredients-viewer-row.h"
#include "gr-ingredients-viewer.h"
#include "gr-ingredient.h"
#include "gr-unit.h"
#include "gr-utils.h"
#include "gr-number.h"
#include "gr-recipe-store.h"
#include "gr-convert-units.h"
#include "types.h"

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
        GtkWidget *drag_handle;
        GtkWidget *unit_event_box;
        GtkWidget *ingredient_event_box;
        GtkWidget *unit_help_popover;
        GtkEntryCompletion *unit_completion;
        GtkCellRenderer *unit_cell;

        double value;
        GrUnit unit;
        char *amount;
        char *ingredient;

        gboolean editable;
        gboolean active;

        gboolean unit_error;

        GtkSizeGroup *group;
};

G_DEFINE_TYPE (GrIngredientsViewerRow, gr_ingredients_viewer_row, GTK_TYPE_LIST_BOX_ROW)

enum {
        PROP_0,
        PROP_AMOUNT,
        PROP_VALUE,
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

          case PROP_VALUE:
                g_value_set_double (value, self->value);
                break;

          case PROP_UNIT:
                g_value_set_enum (value, self->unit);
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
        GString *s;

        s = g_string_new ("");
        if (row->amount)
                g_string_append (s, row->amount);
        else
                gr_convert_format (s, row->value, row->unit);

        if (s->len == 0 && row->editable) {
                gtk_style_context_add_class (gtk_widget_get_style_context (row->unit_label), "dim-label");
                gtk_label_set_label (GTK_LABEL (row->unit_label), _("Amount…"));
        }
        else {
                gtk_style_context_remove_class (gtk_widget_get_style_context (row->unit_label), "dim-label");
                gtk_label_set_label (GTK_LABEL (row->unit_label), s->str);
        }

        g_string_free (s, TRUE);
}

static void
update_ingredient (GrIngredientsViewerRow *row)
{
      if (row->ingredient[0] == '\0') {
                gtk_style_context_add_class (gtk_widget_get_style_context (row->ingredient_label), "dim-label");
                gtk_label_set_label (GTK_LABEL (row->ingredient_label), _("Ingredient…"));
      }
      else {
                gtk_style_context_remove_class (gtk_widget_get_style_context (row->ingredient_label), "dim-label");
                gtk_label_set_label (GTK_LABEL (row->ingredient_label), row->ingredient);
      }
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
gr_ingredients_viewer_row_set_value (GrIngredientsViewerRow *row,
                                     double                  value)
{
        row->value = value;
        update_unit (row);
}

static void
gr_ingredients_viewer_row_set_unit (GrIngredientsViewerRow *row,
                                    GrUnit                  unit)
{
        row->unit = unit;
        update_unit (row);
}

static void
gr_ingredients_viewer_row_set_ingredient (GrIngredientsViewerRow *row,
                                          const char             *ingredient)
{
        g_free (row->ingredient);
        row->ingredient = g_strdup (ingredient);
        update_ingredient (row);
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

static void setup_editable_row (GrIngredientsViewerRow *self);

static void
gr_ingredients_viewer_row_set_editable (GrIngredientsViewerRow *row,
                                        gboolean                editable)
{
        row->editable = editable;
        setup_editable_row (row);
}

static gboolean save_unit       (GrIngredientsViewerRow *row);
static gboolean save_ingredient (GrIngredientsViewerRow *row);
static void     save_row        (GrIngredientsViewerRow *row);

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

          case PROP_VALUE:
                gr_ingredients_viewer_row_set_value (self, g_value_get_double (value));
                break;

          case PROP_UNIT:
                gr_ingredients_viewer_row_set_unit (self, g_value_get_enum (value));
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
edit_ingredient (GrIngredientsViewerRow *row)
{
        if (row->editable) {
                save_unit (row);
                gtk_entry_set_text (GTK_ENTRY (row->ingredient_entry), row->ingredient);
                gtk_stack_set_visible_child_name (GTK_STACK (row->ingredient_stack), "ingredient_entry");
                gtk_widget_grab_focus (row->ingredient_entry);
                g_signal_emit (row, signals[EDIT], 0);
        }
}

static gboolean
show_help (gpointer data)
{
        GrIngredientsViewerRow *row = data;

        if (row->unit_error)
                gtk_popover_popup (GTK_POPOVER (row->unit_help_popover));

        return G_SOURCE_REMOVE;
}

static void
edit_unit (GrIngredientsViewerRow *row)
{
        GString *s;

        s = g_string_new ("");
        if (row->amount)
                g_string_append (s, row->amount);
        else
                gr_convert_format (s, row->value, row->unit);

        save_ingredient (row);

        if (row->editable) {
                gtk_entry_set_text (GTK_ENTRY (row->unit_entry), s->str);
                gtk_stack_set_visible_child_name (GTK_STACK (row->unit_stack), "unit_entry");
                gtk_widget_grab_focus (row->unit_entry);
                show_help (row);
                g_signal_emit (row, signals[EDIT], 0);
        }

        g_string_free (s, TRUE);
}

static void
edit_unit_or_focus_out (GrIngredientsViewerRow *row)
{
        if (!row->active)
                edit_unit (row);
        else
                save_row (row);
}

static gboolean
parse_unit (const char  *text,
            char       **amount,
            char       **unit)
{
        char *tmp;
        const char *str;
        double number;
        g_autofree char *num = NULL;

        g_clear_pointer (amount, g_free);
        g_clear_pointer (unit, g_free);

        tmp = (char *)text;
        skip_whitespace (&tmp);
        str = tmp;
        if (!gr_number_parse (&number, &tmp, NULL)) {
                *unit = g_strdup (str);
                return FALSE;
        }

        *amount = gr_number_format (number);
        skip_whitespace (&tmp);
        if (tmp)
                *unit = g_strdup (tmp);

        return TRUE;
}

static void
set_unit_error (GrIngredientsViewerRow *row,
                const char             *text)
{
        g_free (row->amount);
        row->amount = g_strdup (text);

        row->unit_error = text != NULL;

        if (text != NULL) {
                gtk_style_context_add_class (gtk_widget_get_style_context (row->unit_entry), "error");
                gtk_style_context_add_class (gtk_widget_get_style_context (row->unit_label), "error");
        }
        else {
                gtk_style_context_remove_class (gtk_widget_get_style_context (row->unit_entry), "error");
                gtk_style_context_remove_class (gtk_widget_get_style_context (row->unit_label), "error");
                gtk_popover_popdown (GTK_POPOVER (row->unit_help_popover));
        }
}

static void
unit_text_changed (GrIngredientsViewerRow *row)
{
        set_unit_error (row, NULL);
}

static gboolean
save_unit (GrIngredientsViewerRow *row)
{
        GtkWidget *visible;

        visible = gtk_stack_get_visible_child (GTK_STACK (row->unit_stack));
        if (visible == row->unit_entry) {
                const char *text;

                text = gtk_entry_get_text (GTK_ENTRY (row->unit_entry));
                if (!gr_parse_units (text, &row->value, &row->unit)) {
                        set_unit_error (row, text);
                }

                update_unit (row);
                gtk_stack_set_visible_child (GTK_STACK (row->unit_stack), row->unit_event_box);
        }

        return GDK_EVENT_PROPAGATE;
}

static gboolean
save_ingredient (GrIngredientsViewerRow *row)
{
        GtkWidget *visible;

        visible = gtk_stack_get_visible_child (GTK_STACK (row->ingredient_stack));
        if (visible == row->ingredient_entry) {
                row->ingredient = g_strdup (gtk_entry_get_text (GTK_ENTRY (row->ingredient_entry)));
                update_ingredient (row);
                gtk_stack_set_visible_child (GTK_STACK (row->ingredient_stack), row->ingredient_event_box);
        }

        return GDK_EVENT_PROPAGATE;
}

static void
save_row (GrIngredientsViewerRow *row)
{
        save_unit (row);
        save_ingredient (row);
}

static gboolean
entry_key_press (GrIngredientsViewerRow *row,
                 GdkEventKey            *event)
{
        if (event->keyval == GDK_KEY_Escape) {
                gtk_stack_set_visible_child (GTK_STACK(row->unit_stack), row->unit_event_box);
                gtk_stack_set_visible_child (GTK_STACK (row->ingredient_stack), row->ingredient_event_box);
                return GDK_EVENT_STOP;
        }

        return GDK_EVENT_PROPAGATE;
}

static gboolean
drag_key_press (GrIngredientsViewerRow *source,
                GdkEventKey            *event)
{
        GtkWidget *list;
        GtkWidget *target;
        int index;

        list = gtk_widget_get_parent (GTK_WIDGET (source));
        index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (source));

        if ((event->state & GDK_MOD1_MASK) != 0 && event->keyval == GDK_KEY_Up)
                target = GTK_WIDGET (gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), index - 1));
        else if ((event->state & GDK_MOD1_MASK) != 0 && event->keyval == GDK_KEY_Down)
                target = GTK_WIDGET (gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), index + 1));
        else
                return GDK_EVENT_PROPAGATE;

        if (target) {
                g_signal_emit (source, signals[MOVE], 0, target);
                gtk_widget_grab_focus (GTK_WIDGET (source));
        }

        return GDK_EVENT_STOP;
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

        pspec = g_param_spec_double ("value", NULL, NULL,
                                     -G_MINDOUBLE, G_MAXDOUBLE, 0.0,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_VALUE, pspec);

        pspec = g_param_spec_enum ("unit", NULL, NULL,
                                   GR_TYPE_UNIT, GR_UNIT_UNKNOWN,
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
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, drag_handle);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, unit_event_box);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, ingredient_event_box);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, unit_help_popover);

        gtk_widget_class_bind_template_callback (widget_class, emit_delete);
        gtk_widget_class_bind_template_callback (widget_class, edit_unit);
        gtk_widget_class_bind_template_callback (widget_class, save_unit);
        gtk_widget_class_bind_template_callback (widget_class, edit_unit_or_focus_out);
        gtk_widget_class_bind_template_callback (widget_class, edit_ingredient);
        gtk_widget_class_bind_template_callback (widget_class, save_ingredient);
        gtk_widget_class_bind_template_callback (widget_class, entry_key_press);
        gtk_widget_class_bind_template_callback (widget_class, drag_key_press);
        gtk_widget_class_bind_template_callback (widget_class, unit_text_changed);
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
        GtkWidget *viewer;

        row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);

        gtk_widget_get_allocation (row, &alloc);
        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, alloc.width, alloc.height);
        cr = cairo_create (surface);

        gtk_style_context_add_class (gtk_widget_get_style_context (row), "drag-icon");
        gtk_widget_draw (row, cr);
        gtk_style_context_remove_class (gtk_widget_get_style_context (row), "drag-icon");

        gtk_widget_translate_coordinates (widget, row, 0, 0, &x, &y);
        cairo_surface_set_device_offset (surface, -x, -y);
        gtk_drag_set_icon_surface (context, surface);

        cairo_destroy (cr);
        cairo_surface_destroy (surface);

        viewer = gtk_widget_get_ancestor (row, GR_TYPE_INGREDIENTS_VIEWER);
        gr_ingredients_viewer_set_drag_row (GR_INGREDIENTS_VIEWER (viewer), row);
}

static void
drag_end (GtkWidget      *widget,
          GdkDragContext *context,
          gpointer        data)
{
        GtkWidget *viewer;

        viewer = gtk_widget_get_ancestor (widget, GR_TYPE_INGREDIENTS_VIEWER);
        gr_ingredients_viewer_set_drag_row (GR_INGREDIENTS_VIEWER (viewer), NULL);
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

static int
sort_func (GtkTreeModel *model,
           GtkTreeIter  *a,
           GtkTreeIter  *b,
           gpointer      user_data)
{
        g_autofree char *as = NULL;
        g_autofree char *bs = NULL;

        gtk_tree_model_get (model, a, 0, &as, -1);
        gtk_tree_model_get (model, b, 0, &bs, -1);

        return g_strcmp0 (as, bs);
}

static GtkListStore *ingredients_model = NULL;

static void
clear_ingredients_model (GrRecipeStore *store)
{
        g_clear_object (&ingredients_model);
}

static GtkTreeModel *
get_ingredients_model (void)
{
        static gboolean signal_connected;
        GrRecipeStore *store;

        store = gr_recipe_store_get ();

        if (!signal_connected) {
                g_signal_connect (store, "recipe-added", G_CALLBACK (clear_ingredients_model), NULL);
                g_signal_connect (store, "recipe-changed", G_CALLBACK (clear_ingredients_model), NULL);

                signal_connected = TRUE;
        }

        if (ingredients_model == NULL) {
                g_autofree char **names = NULL;
                guint length;
                int i;

                ingredients_model = gtk_list_store_new (1, G_TYPE_STRING);
                gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (ingredients_model),
                                                         sort_func, NULL, NULL);
                gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (ingredients_model),
                                                      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                                      GTK_SORT_ASCENDING);

                names = gr_recipe_store_get_all_ingredients (store, &length);
                for (i = 0; i < length; i++) {
                        gtk_list_store_insert_with_values (ingredients_model, NULL, -1,
                                                           0, names[i],
                                                           -1);
                }
        }

        return GTK_TREE_MODEL (g_object_ref (ingredients_model));
}

static GtkTreeModel *
get_units_model (GrIngredientsViewerRow *row)
{
        static GtkListStore *store = NULL;

        if (store == NULL) {
                int i;

                store = gtk_list_store_new (5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
                gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                                         sort_func, NULL, NULL);
                gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                                      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                                      GTK_SORT_ASCENDING);

                gtk_list_store_insert_with_values (store, NULL, -1,
                                                   0, "",
                                                   1, "",
                                                   2, "",
                                                   3, "",
                                                   4, "",
                                                   -1);

                for (i = GR_UNIT_NUMBER + 1; i <= GR_LAST_UNIT; i++) {
                        const char *abbrev;
                        const char *name;
                        const char *plural;

                        g_autofree char *tmp = NULL;
                        g_autofree char *tmp2 = NULL;

                        abbrev = gr_unit_get_abbreviation (i);
                        name = gr_unit_get_display_name (i);
                        plural = gr_unit_get_plural (i);

                        if (g_strcmp0 (abbrev, name) == 0)
                                tmp = g_strdup (name);
                        else
                                tmp = g_strdup_printf ("%s (%s)", name, abbrev);

                        if (g_strcmp0 (abbrev, plural) == 0)
                                tmp2 = g_strdup (name);
                        else
                                tmp2 = g_strdup_printf ("%s (%s)", plural, abbrev);

                        gtk_list_store_insert_with_values (store, NULL, -1,
                                                           0, abbrev,
                                                           1, name,
                                                           2, tmp,
                                                           3, plural,
                                                           4, tmp2,
                                                           -1);
                }
        }

        return GTK_TREE_MODEL (g_object_ref (store));
}

static void
gr_ingredients_viewer_row_init (GrIngredientsViewerRow *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
get_amount (GtkCellLayout   *layout,
            GtkCellRenderer *renderer,
            GtkTreeModel    *model,
            GtkTreeIter     *iter,
            gpointer         data)
{
        GrIngredientsViewerRow *row = data;
        g_autofree char *amount = NULL;
        g_autofree char *unit = NULL;
        parse_unit (gtk_entry_get_text (GTK_ENTRY (row->unit_entry)), &amount, &unit);
        g_object_set (renderer, "text", amount, NULL);
}

static gboolean
match_func (GtkEntryCompletion *completion,
            const char         *key,
            GtkTreeIter        *iter,
            gpointer            data)
{
        GrIngredientsViewerRow *row = data;
        GtkTreeModel *model;
        g_autofree char *abbrev = NULL;
        g_autofree char *name = NULL;
        g_autofree char *amount = NULL;
        g_autofree char *unit = NULL;
        g_autofree char *plural = NULL;

        model = gtk_entry_completion_get_model (completion);
        gtk_tree_model_get (model, iter, 0, &abbrev, 1, &name, 3, &plural, -1);

        parse_unit (gtk_entry_get_text (GTK_ENTRY (row->unit_entry)), &amount, &unit);

        if (!amount || !unit)
                return FALSE;

        if (g_str_has_prefix (abbrev, unit) ||
            g_str_has_prefix (name, unit) ||
            g_str_has_prefix (plural, unit))
                return TRUE;

        return FALSE;
}

static gboolean
match_selected (GtkEntryCompletion *completion,
                GtkTreeModel       *model,
                GtkTreeIter        *iter,
                gpointer            data)
{
        GrIngredientsViewerRow *row = data;
        g_autofree char *abbrev = NULL;
        g_autofree char *amount = NULL;
        g_autofree char *unit = NULL;
        g_autofree char *tmp = NULL;
        g_autofree char *plural = NULL;

        gtk_tree_model_get (model, iter, 0, &abbrev, -1);

        parse_unit (gtk_entry_get_text (GTK_ENTRY (row->unit_entry)), &amount, &unit);

        tmp = g_strdup_printf ("%s %s", amount, abbrev);

        gtk_entry_set_text (GTK_ENTRY (row->unit_entry), tmp);

        return TRUE;
}

static void
text_changed (GObject    *object,
              GParamSpec *pspec,
              gpointer    data)
{
        GtkEntry *entry = GTK_ENTRY (object);
        GrIngredientsViewerRow *row = data;
        double number;
        char *text;
        int col;

        text = (char *) gtk_entry_get_text (entry);
        gr_number_parse (&number, &text, NULL);
        col = number > 1 ? 4 : 2;
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (row->unit_completion),
                                        row->unit_cell,
                                        "text", col,
                                        NULL);
}

static void
setup_editable_row (GrIngredientsViewerRow *self)
{
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), self->editable);

        if (self->editable) {
                g_autoptr(GtkEntryCompletion) ingredient_completion = NULL;
                g_autoptr(GtkTreeModel) ingredients_model = NULL;
                g_autoptr(GtkEntryCompletion) unit_completion = NULL;
                g_autoptr(GtkTreeModel) units_model = NULL;
                GtkCellRenderer *cell = NULL;

                gtk_drag_source_set (self->drag_handle, GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);
                g_signal_connect (self->drag_handle, "drag-begin", G_CALLBACK (drag_begin), NULL);
                g_signal_connect (self->drag_handle, "drag-end", G_CALLBACK (drag_end), NULL);
                g_signal_connect (self->drag_handle, "drag-data-get", G_CALLBACK (drag_data_get), NULL);

                ingredients_model = get_ingredients_model ();
                ingredient_completion = gtk_entry_completion_new ();
                gtk_entry_completion_set_model (ingredient_completion, ingredients_model);
                gtk_entry_completion_set_text_column (ingredient_completion, 0);
                gtk_entry_set_completion (GTK_ENTRY (self->ingredient_entry), ingredient_completion);

                units_model = get_units_model (self);
                unit_completion = gtk_entry_completion_new ();
                gtk_entry_completion_set_model (unit_completion, units_model);
                g_object_set (unit_completion, "text-column", 2, NULL);

                cell = gtk_cell_renderer_text_new ();
                gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (unit_completion), cell, get_amount, self, NULL);
                gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (unit_completion), cell, FALSE);

                cell = gtk_cell_renderer_text_new ();
                gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (unit_completion), cell, TRUE);
                self->unit_cell = cell;
                self->unit_completion = unit_completion;
                g_signal_connect (self->unit_entry, "notify::text", G_CALLBACK (text_changed), self);

                gtk_entry_completion_set_match_func (unit_completion, match_func, self, NULL);
                g_signal_connect (unit_completion, "match-selected", G_CALLBACK (match_selected), self);
                gtk_entry_set_completion (GTK_ENTRY (self->unit_entry), unit_completion);
        }
        else {
                gtk_drag_source_unset (self->drag_handle);
                g_signal_handlers_disconnect_by_func (self->drag_handle, drag_begin, NULL);
                g_signal_handlers_disconnect_by_func (self->drag_handle, drag_data_get, NULL);

                gtk_entry_set_completion (GTK_ENTRY (self->ingredient_entry), NULL);
                gtk_entry_set_completion (GTK_ENTRY (self->unit_entry), NULL);
        }
}

GtkWidget *
gr_ingredients_viewer_row_has_error (GrIngredientsViewerRow *row)
{
        if (gtk_style_context_has_class (gtk_widget_get_style_context (row->unit_label), "error"))
                return gtk_widget_get_parent (row->unit_label);

        if (row->ingredient == NULL || row->ingredient[0] == '\0')
                return gtk_widget_get_parent (row->ingredient_label);

        return NULL;
}
