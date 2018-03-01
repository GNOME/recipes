/* gr-query-editor.c:
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

#include "gr-query-editor.h"
#include "gr-ingredient.h"
#include "gr-meal.h"
#include "gr-meal-row.h"
#include "gr-spice-row.h"
#include "gr-diet-row.h"
#include "gr-ingredient-row.h"

struct _GrQueryEditor
{
        GtkSearchBar parent_instance;

        GtkWidget *entry;
        GtkWidget *popover;
        GtkWidget *meal_search_revealer;
        GtkWidget *meal_search_button;
        GtkWidget *meal_search_button_label;
        GtkWidget *meal_list;
        GtkWidget *spice_search_revealer;
        GtkWidget *spice_search_button;
        GtkWidget *spice_search_button_label;
        GtkWidget *spice_list;
        GtkWidget *diet_search_revealer;
        GtkWidget *diet_search_button;
        GtkWidget *diet_search_button_label;
        GtkWidget *diet_list;
        GtkWidget *ing_search_revealer;
        GtkWidget *ing_search_button;
        GtkWidget *ing_search_button_label;
        GtkWidget *ing_filter_entry;
        GtkWidget *ing_list;

        char *ing_term;
        char **terms;
};

enum
{
        ACTIVATED,
        CHANGED,
        CANCEL,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void entry_activate_cb       (GtkWidget     *entry,
                                     GrQueryEditor *editor);
static void entry_changed_cb        (GtkWidget      *entry,
                                     GrQueryEditor *editor);

G_DEFINE_TYPE (GrQueryEditor, gr_query_editor, GTK_TYPE_SEARCH_BAR)

enum {
        PROP_0,
        N_PROPS
};

GrQueryEditor *
gr_query_editor_new (void)
{
        return g_object_new (GR_TYPE_QUERY_EDITOR, NULL);
}

static void
show_meal_search_list (GrQueryEditor *self)
{
        gtk_widget_hide (self->meal_search_button);
        gtk_widget_show (self->meal_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->meal_search_revealer), TRUE);
}

static void
hide_meal_search_list (GrQueryEditor *self,
                       gboolean       animate)
{
        gtk_widget_show (self->meal_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->meal_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->meal_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->meal_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
show_spice_search_list (GrQueryEditor *self)
{
        gtk_widget_hide (self->spice_search_button);
        gtk_widget_show (self->spice_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->spice_search_revealer), TRUE);
}

static void
hide_spice_search_list (GrQueryEditor *self,
                        gboolean       animate)
{
        gtk_widget_show (self->spice_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->spice_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->spice_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->spice_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
show_diet_search_list (GrQueryEditor *self)
{
        gtk_widget_hide (self->diet_search_button);
        gtk_widget_show (self->diet_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->diet_search_revealer), TRUE);
}

static void
hide_diet_search_list (GrQueryEditor *self,
                       gboolean       animate)
{
        gtk_widget_show (self->diet_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->diet_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->diet_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->diet_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
show_ingredients_search_list (GrQueryEditor *self)
{
        gtk_widget_hide (self->ing_search_button);
        gtk_widget_show(self->ing_search_revealer);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->ing_search_revealer), TRUE);
}

static void
hide_ingredients_search_list (GrQueryEditor *self,
                              gboolean       animate)
{
        gtk_entry_set_text (GTK_ENTRY (self->ing_filter_entry), "");
        gtk_widget_show (self->ing_search_button);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->ing_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_NONE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->ing_search_revealer), FALSE);
        if (!animate)
                gtk_revealer_set_transition_type (GTK_REVEALER (self->ing_search_revealer),
                                                  GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
meal_search_button_clicked (GtkButton     *button,
                            GrQueryEditor *self)
{
        hide_diet_search_list (self, TRUE);
        hide_spice_search_list (self, TRUE);
        hide_ingredients_search_list (self, TRUE);
        show_meal_search_list (self);
}

static void
spice_search_button_clicked (GtkButton     *button,
                             GrQueryEditor *self)
{
        hide_meal_search_list (self, TRUE);
        hide_diet_search_list (self, TRUE);
        hide_ingredients_search_list (self, TRUE);
        show_spice_search_list (self);
}

static void
diet_search_button_clicked (GtkButton     *button,
                            GrQueryEditor *self)
{
        hide_meal_search_list (self, TRUE);
        hide_spice_search_list (self, TRUE);
        hide_ingredients_search_list (self, TRUE);
        show_diet_search_list (self);
}

static void
ing_search_button_clicked (GtkButton     *button,
                           GrQueryEditor *self)
{
        hide_meal_search_list (self, TRUE);
        hide_spice_search_list (self, TRUE);
        hide_diet_search_list (self, TRUE);
        show_ingredients_search_list (self);
}

static void
ing_row_activated (GtkListBox    *list,
                   GtkListBoxRow *row,
                   GrQueryEditor *self)
{
        gboolean include, exclude;

        if (!GR_IS_INGREDIENT_ROW (row)) {
                GList *children, *l;

                children = gtk_container_get_children (GTK_CONTAINER (list));
                for (l = children; l; l = l->next) {
                        row = l->data;
                        if (GR_IS_INGREDIENT_ROW (row)) {
                                g_object_set (row, "include", FALSE, "exclude", FALSE, NULL);
                        }
                }

                g_list_free (children);

                hide_ingredients_search_list (self, TRUE);

                return;
        }

        g_object_get (row, "include", &include, "exclude", &exclude, NULL);

        if (!include && !exclude)
                g_object_set (row, "include", TRUE, "exclude", FALSE, NULL);
        else if (include && !exclude)
                g_object_set (row, "include", FALSE, "exclude", TRUE, NULL);
        else
                g_object_set (row, "include", FALSE, "exclude", FALSE, NULL);
}

static void
ing_header_func (GtkListBoxRow *row,
                 GtkListBoxRow *before,
                 gpointer       data)
{
        if (before != NULL && !GR_IS_INGREDIENT_ROW (before))
                gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
        else
                gtk_list_box_row_set_header (row, NULL);
}

static gboolean
ing_filter_func (GtkListBoxRow *row,
                 gpointer       data)
{
        GrQueryEditor *self = data;
        const char *cf;

        if (!GR_IS_INGREDIENT_ROW (row))
                return TRUE;

        if (!self->ing_term)
                return TRUE;

        cf = gr_ingredient_row_get_filter_term (GR_INGREDIENT_ROW (row));

        return g_str_has_prefix (cf, self->ing_term);
}

static void
ing_filter_changed (GrQueryEditor *self)
{
        const char *term;

        term = gtk_entry_get_text (GTK_ENTRY (self->ing_filter_entry));
        g_free (self->ing_term);
        self->ing_term = g_utf8_casefold (term, -1);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->ing_list));
}

static void
ing_filter_stop (GrQueryEditor *self)
{
        gtk_entry_set_text (GTK_ENTRY (self->ing_filter_entry), "");
}

static void
ing_filter_activated (GrQueryEditor *self)
{
        GList *children, *l;
        GtkWidget *row;

        row = NULL;
        children = gtk_container_get_children (GTK_CONTAINER (self->ing_list));
        for (l = children; l; l = l->next) {
                GtkWidget *r = l->data;

                if (!GR_IS_INGREDIENT_ROW (r))
                        continue;

                if (!ing_filter_func (GTK_LIST_BOX_ROW (r), self))
                        continue;

                if (row != NULL) {
                        /* more than one match, don't activate */
                        g_list_free (children);
                        return;
                }

                row = r;
        }
        g_list_free (children);

        gtk_widget_activate (row);
}

static void
populate_ingredients_list (GrQueryEditor *self)
{
        int i;
        const char **ingredients;
        GtkWidget *row;

        /* FIXME: repopulate */
        row = gtk_label_new (_("Anything"));
        g_object_set (row, "margin", 6, NULL);
        gtk_label_set_xalign (GTK_LABEL (row), 0);
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->ing_list), row);

        ingredients = gr_ingredient_get_names (NULL);
        for (i = 0; ingredients[i]; i++) {
                row = GTK_WIDGET (gr_ingredient_row_new (ingredients[i]));
                gtk_widget_show (row);
                gtk_container_add (GTK_CONTAINER (self->ing_list), row);
                gr_ingredient_row_set_entry (GR_INGREDIENT_ROW (row),
                                             GD_TAGGED_ENTRY (self->entry));
        }

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->ing_list),
                                      ing_header_func, self, NULL);

        gtk_list_box_set_filter_func (GTK_LIST_BOX (self->ing_list),
                                      ing_filter_func, self, NULL);

        g_signal_connect (self->ing_list, "row-activated",
                          G_CALLBACK (ing_row_activated), self);
}

static void
diet_row_activated (GtkListBox    *list,
                    GtkListBoxRow *row,
                    GrQueryEditor *self)
{
        gboolean include;

        if (!GR_IS_DIET_ROW (row)) {
                GList *children, *l;

                children = gtk_container_get_children (GTK_CONTAINER (list));
                for (l = children; l; l = l->next) {
                        row = l->data;
                        if (GR_IS_DIET_ROW (row)) {
                                g_object_set (row, "include", FALSE, NULL);
                        }
                }
                g_list_free (children);

                hide_diet_search_list (self, TRUE);

                return;
        }

        g_object_get (row, "include", &include, NULL);
        g_object_set (row, "include", !include, NULL);
}

static void
diet_header_func (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       data)
{
        if (before != NULL && !GR_IS_DIET_ROW (before))
                gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
        else
                gtk_list_box_row_set_header (row, NULL);
}

static void
populate_diets_list (GrQueryEditor *self)
{
        GtkWidget *row;

        row = gtk_label_new (_("No restrictions"));
        g_object_set (row, "margin", 6, NULL);
        gtk_label_set_xalign (GTK_LABEL (row), 0);
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_GLUTEN_FREE));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_NUT_FREE));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_VEGAN));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_VEGETARIAN));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_MILK_FREE));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->entry));

        row = GTK_WIDGET (gr_diet_row_new (GR_DIET_HALAL));
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->diet_list), row);
        gr_diet_row_set_entry (GR_DIET_ROW (row), GD_TAGGED_ENTRY (self->entry));

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->diet_list),
                                      diet_header_func, self, NULL);

        g_signal_connect (self->diet_list, "row-activated", G_CALLBACK (diet_row_activated), self);
}

static void
meal_row_activated (GtkListBox    *list,
                    GtkListBoxRow *row,
                    GrQueryEditor *self)
{
        gboolean include;

        if (!GR_IS_MEAL_ROW (row)) {
                GList *children, *l;

                children = gtk_container_get_children (GTK_CONTAINER (list));
                for (l = children; l; l = l->next) {
                        row = l->data;
                        if (GR_IS_MEAL_ROW (row)) {
                                g_object_set (row, "include", FALSE, NULL);
                        }
                }
                g_list_free (children);

                hide_meal_search_list (self, TRUE);

                return;
        }

        g_object_get (row, "include", &include, NULL);
        g_object_set (row, "include", !include, NULL);
}

static void
meal_header_func (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       data)
{
        if (before != NULL && !GR_IS_MEAL_ROW (before))
                gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
        else
                gtk_list_box_row_set_header (row, NULL);
}

static void
populate_meals_list (GrQueryEditor *self)
{
        int i;
        const char **names;
        GtkWidget *row;
        int length;

        row = gtk_label_new (_("Any meal"));
        g_object_set (row, "margin", 6, NULL);
        gtk_label_set_xalign (GTK_LABEL (row), 0);
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->meal_list), row);

        names = gr_meal_get_names (&length);
        for (i = 0; i < length; i++) {
                row = GTK_WIDGET (gr_meal_row_new (names[i]));
                gtk_widget_show (row);
                gtk_container_add (GTK_CONTAINER (self->meal_list), row);
                gr_meal_row_set_entry (GR_MEAL_ROW (row), GD_TAGGED_ENTRY (self->entry));
        }

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->meal_list),
                                      meal_header_func, self, NULL);

        g_signal_connect (self->meal_list, "row-activated", G_CALLBACK (meal_row_activated), self);
}

static void
spice_header_func (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       data)
{
        if (before != NULL && !GR_IS_SPICE_ROW (before))
                gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
        else
                gtk_list_box_row_set_header (row, NULL);
}

static void
spice_row_activated (GtkListBox    *list,
                     GtkListBoxRow *row,
                     GrQueryEditor *self)
{
        gboolean include;

        if (!GR_IS_SPICE_ROW (row)) {
                GList *children, *l;

                children = gtk_container_get_children (GTK_CONTAINER (list));
                for (l = children; l; l = l->next) {
                        row = l->data;
                        if (GR_IS_SPICE_ROW (row)) {
                                g_object_set (row, "include", FALSE, NULL);
                        }
                }
                g_list_free (children);

                hide_spice_search_list (self, TRUE);

                return;
        }

        g_object_get (row, "include", &include, NULL);
        g_object_set (row, "include", !include, NULL);
}

static void
populate_spice_list (GrQueryEditor *self)
{
        int i;
        GtkWidget *row;
        const char *levels[] = { "mild", "spicy", "spicy", "hot", "hot", "extreme" };
        gboolean less[] = { TRUE, TRUE, FALSE, TRUE, FALSE, FALSE };

        row = gtk_label_new (_("Any spiciness"));
        g_object_set (row, "margin", 6, NULL);
        gtk_label_set_xalign (GTK_LABEL (row), 0);
        gtk_widget_show (row);
        gtk_container_add (GTK_CONTAINER (self->spice_list), row);

        for (i = 0; i < G_N_ELEMENTS (levels); i++) {
                row = GTK_WIDGET (gr_spice_row_new (levels[i], less[i]));
                gtk_widget_show (row);
                gtk_container_add (GTK_CONTAINER (self->spice_list), row);
                gr_spice_row_set_entry (GR_SPICE_ROW (row), GD_TAGGED_ENTRY (self->entry));
        }

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->spice_list),
                                      spice_header_func, self, NULL);

        g_signal_connect (self->spice_list, "row-activated", G_CALLBACK (spice_row_activated), self);
}

static void
tag_clicked (GdTaggedEntry    *entry,
             GdTaggedEntryTag *tag,
             GrQueryEditor    *self)
{
        GtkWidget *row;
        gboolean include, exclude;

        row = GTK_WIDGET (g_object_get_data (G_OBJECT (tag), "row"));
        if (GR_IS_INGREDIENT_ROW (row)) {
                g_object_get (row, "include", &include, "exclude", &exclude, NULL);
                g_object_set (row, "include", !include, "exclude", !exclude, NULL);
        }
}

static void
tag_button_clicked (GdTaggedEntry    *entry,
                    GdTaggedEntryTag *tag,
                    GrQueryEditor    *self)
{
        GtkWidget *row;

        row = GTK_WIDGET (g_object_get_data (G_OBJECT (tag), "row"));
        if (GR_IS_INGREDIENT_ROW (row)) {
                g_object_set (row, "include", FALSE, "exclude", FALSE, NULL);
        }
        else {
                g_object_set (row, "include", FALSE, NULL);
        }
}

static void
search_popover_notify (GObject       *object,
                       GParamSpec    *pspec,
                       GrQueryEditor *self)
{
        hide_meal_search_list (self, FALSE);
        hide_spice_search_list (self, FALSE);
        hide_diet_search_list (self, FALSE);
        hide_ingredients_search_list (self, FALSE);
}

static gboolean
entry_key_press_event_cb (GtkWidget     *widget,
                          GdkEventKey   *event,
                          GrQueryEditor *editor)
{
        if (event->keyval == GDK_KEY_Down)
                gtk_widget_grab_focus (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
        return FALSE;
}

static void
entry_activate_cb (GtkWidget     *entry,
                   GrQueryEditor *editor)
{
        g_signal_emit (editor, signals[ACTIVATED], 0);
}

static void
entry_changed_cb (GtkWidget     *entry,
                  GrQueryEditor *editor)
{
        const char *text;
        g_autoptr(GString) s2 = NULL;
        g_auto(GStrv) terms = NULL;
        GPtrArray *a = NULL;
        GList *children, *l;
        int i;

        a = g_ptr_array_new ();

        text = gtk_entry_get_text (GTK_ENTRY (editor->entry));
        terms = g_strsplit (text, " ", -1);

        for (i = 0; terms[i]; i++)
                g_ptr_array_add (a, g_utf8_casefold (terms[i], -1));

        s2 = g_string_new ("");

        children = gtk_container_get_children (GTK_CONTAINER (editor->meal_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                g_autofree char *term = NULL;
                g_autofree char *label = NULL;

                if (!GR_IS_MEAL_ROW (row))
                        continue;

                term = gr_meal_row_get_search_term (GR_MEAL_ROW (row));
                if (term)
                        g_ptr_array_add (a, g_strdup (term));

                label = gr_meal_row_get_label (GR_MEAL_ROW (row));
                if (label) {
                        if (s2->len > 0)
                                g_string_append (s2, ", ");
                        g_string_append (s2, label);
                }
        }
        g_list_free (children);

        if (s2->len == 0)
                g_string_append (s2, _("Any meal"));

        gtk_label_set_label (GTK_LABEL (editor->meal_search_button_label), s2->str);

        g_string_truncate (s2, 0);

        children = gtk_container_get_children (GTK_CONTAINER (editor->spice_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                g_autofree char *term = NULL;
                g_autofree char *label = NULL;

                if (!GR_IS_SPICE_ROW (row))
                        continue;

                term = gr_spice_row_get_search_term (GR_SPICE_ROW (row));
                if (term)
                        g_ptr_array_add (a, g_strdup (term));

                label = gr_spice_row_get_label (GR_SPICE_ROW (row));
                if (label) {
                        if (s2->len > 0)
                                g_string_append (s2, ", ");
                        g_string_append (s2, label);
                }
        }
        g_list_free (children);

        if (s2->len == 0)
                g_string_append (s2, _("Any spiciness"));

        gtk_label_set_label (GTK_LABEL (editor->spice_search_button_label), s2->str);

        g_string_truncate (s2, 0);

        children = gtk_container_get_children (GTK_CONTAINER (editor->diet_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                g_autofree char *term = NULL;
                g_autofree char *label = NULL;

                if (!GR_IS_DIET_ROW (row))
                        continue;

                term = gr_diet_row_get_search_term (GR_DIET_ROW (row));
                if (term)
                        g_ptr_array_add (a, g_strdup (term));

                label = gr_diet_row_get_label (GR_DIET_ROW (row));
                if (label) {
                        if (s2->len > 0)
                                g_string_append (s2, ", ");
                        g_string_append (s2, label);
                }
        }
        g_list_free (children);

        if (s2->len == 0)
                g_string_append (s2, _("No restrictions"));

        gtk_label_set_label (GTK_LABEL (editor->diet_search_button_label), s2->str);

        g_string_truncate (s2, 0);

        children = gtk_container_get_children (GTK_CONTAINER (editor->ing_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                g_autofree char *term = NULL;
                g_autofree char *label = NULL;

                if (!GR_IS_INGREDIENT_ROW (row))
                        continue;

                term = gr_ingredient_row_get_search_term (GR_INGREDIENT_ROW (row));
                if (term)
                        g_ptr_array_add (a, g_strdup (term));

                label = gr_ingredient_row_get_label (GR_INGREDIENT_ROW (row));
                if (label) {
                        if (s2->len > 0)
                                g_string_append (s2, ", ");
                        g_string_append (s2, label);
                }
        }
        g_list_free (children);

        if (s2->len == 0)
                g_string_append (s2, _("Anything"));

        gtk_label_set_label (GTK_LABEL (editor->ing_search_button_label), s2->str);

        g_ptr_array_add (a, NULL);

        g_strfreev (editor->terms);
        editor->terms = (char **)g_ptr_array_free (a, FALSE);

        g_signal_emit (editor, signals[CHANGED], 0);
}

static void
stop_search_cb (GtkWidget     *entry,
                GrQueryEditor *editor)
{
        g_signal_emit (editor, signals[CANCEL], 0);
}

static void
gr_query_editor_finalize (GObject *object)
{
        GrQueryEditor *self = (GrQueryEditor *)object;

        g_strfreev (self->terms);
        g_free (self->ing_term);

        G_OBJECT_CLASS (gr_query_editor_parent_class)->finalize (object);
}

static void
gr_query_editor_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        switch (prop_id)
          {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_query_editor_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        switch (prop_id)
          {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void clear_tags (GrQueryEditor *editor);

static void
gr_query_editor_notify (GObject *object,
                        GParamSpec *pspec)
{
        if (strcmp (pspec->name, "search-mode-enabled") == 0) {
                if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (object))) {
                        clear_tags (GR_QUERY_EDITOR (object));
                }
        }

        if (G_OBJECT_CLASS (gr_query_editor_parent_class)->notify)
                G_OBJECT_CLASS (gr_query_editor_parent_class)->notify (object, pspec);
}

static void
gr_query_editor_class_init (GrQueryEditorClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = gr_query_editor_finalize;
        object_class->get_property = gr_query_editor_get_property;
        object_class->set_property = gr_query_editor_set_property;
        object_class->notify = gr_query_editor_notify;

        signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_generic,
                              G_TYPE_NONE, 0);

        signals[CANCEL] =
                g_signal_new ("cancel",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals[ACTIVATED] =
                g_signal_new ("activated",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-query-editor.ui");

        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, entry);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, popover);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, meal_search_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, meal_search_button);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, meal_search_button_label);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, meal_list);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, spice_search_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, spice_search_button);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, spice_search_button_label);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, spice_list);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, diet_search_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, diet_search_button);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, diet_search_button_label);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, diet_list);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, ing_search_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, ing_search_button);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, ing_search_button_label);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, ing_filter_entry);
        gtk_widget_class_bind_template_child (widget_class, GrQueryEditor, ing_list);

        gtk_widget_class_bind_template_callback (widget_class, tag_clicked);
        gtk_widget_class_bind_template_callback (widget_class, tag_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, search_popover_notify);
        gtk_widget_class_bind_template_callback (widget_class, meal_search_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, spice_search_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, diet_search_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, ing_search_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, entry_key_press_event_cb);
        gtk_widget_class_bind_template_callback (widget_class, entry_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, entry_activate_cb);
        gtk_widget_class_bind_template_callback (widget_class, stop_search_cb);
        gtk_widget_class_bind_template_callback (widget_class, ing_filter_changed);
        gtk_widget_class_bind_template_callback (widget_class, ing_filter_stop);
        gtk_widget_class_bind_template_callback (widget_class, ing_filter_activated);
}

static void
gr_query_editor_init (GrQueryEditor *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));

        gtk_search_bar_connect_entry (GTK_SEARCH_BAR (self), GTK_ENTRY (self->entry));

        populate_meals_list (self);
        populate_spice_list (self);
        populate_diets_list (self);
        populate_ingredients_list (self);
}

const char **
gr_query_editor_get_terms (GrQueryEditor *editor)
{
        return (const char **)editor->terms;
}

static void
clear_tags (GrQueryEditor *editor)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (editor->meal_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                if (!GR_IS_MEAL_ROW (row))
                        continue;
                g_object_set (row, "include", FALSE, NULL);
        }
        g_list_free (children);

        children = gtk_container_get_children (GTK_CONTAINER (editor->spice_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                if (!GR_IS_SPICE_ROW (row))
                        continue;
                g_object_set (row, "include", FALSE, NULL);
        }
        g_list_free (children);

        children = gtk_container_get_children (GTK_CONTAINER (editor->diet_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                if (!GR_IS_DIET_ROW (row))
                        continue;
                g_object_set (row, "include", FALSE, NULL);
        }
        g_list_free (children);

        children = gtk_container_get_children (GTK_CONTAINER (editor->ing_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                if (!GR_IS_INGREDIENT_ROW (row))
                        continue;
                g_object_set (row, "include", FALSE, "exclude", FALSE, NULL);
        }
        g_list_free (children);
}

static void
set_ingredient_tag (GrQueryEditor *editor,
                    const char    *ingredient,
                    gboolean       include,
                    gboolean       exclude)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (editor->ing_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                if (!GR_IS_INGREDIENT_ROW (row))
                        continue;
                if (strcmp (ingredient, gr_ingredient_row_get_id (GR_INGREDIENT_ROW (row))) == 0) {
                        g_object_set (row, "include", include, "exclude", exclude, NULL);
                        break;
                }
        }
        g_list_free (children);
}

static void
set_diet_tag (GrQueryEditor *editor,
              const char    *diet,
              gboolean       include)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (editor->diet_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                if (!GR_IS_DIET_ROW (row))
                        continue;
                if (strcmp (diet, gr_diet_row_get_diet (GR_DIET_ROW (row))) == 0) {
                        g_object_set (row, "include", include, NULL);
                        break;
                }
        }
        g_list_free (children);
}

static void
set_meal_tag (GrQueryEditor *editor,
              const char    *meal,
              gboolean       include)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (editor->meal_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                if (!GR_IS_MEAL_ROW (row))
                        continue;
                if (strcmp (meal, gr_meal_row_get_meal (GR_MEAL_ROW (row))) == 0) {
                        g_object_set (row, "include", include, NULL);
                        break;
                }
        }
        g_list_free (children);
}

static void
append_search_text (GrQueryEditor *editor,
                    const char    *term)
{
        g_autofree char *tmp;

       tmp = g_strconcat (gtk_entry_get_text (GTK_ENTRY (editor->entry)), " ", term, NULL);
       gtk_entry_set_text (GTK_ENTRY (editor->entry), tmp);
}

void
gr_query_editor_set_query (GrQueryEditor *editor,
                           const char    *query)
{
        g_auto(GStrv) terms = NULL;

        terms = g_strsplit (query, " ", -1);

        gr_query_editor_set_terms (editor, (const char **)terms);
}

void
gr_query_editor_set_terms (GrQueryEditor  *editor,
                           const char    **terms)
{
        int i;
        clear_tags (editor);

        g_signal_handlers_block_by_func (editor->entry, entry_changed_cb, editor);

        for (i = 0; terms[i]; i++) {
                if (g_str_has_prefix (terms[i], "i+:")) {
                        set_ingredient_tag (editor, terms[i] + 3, TRUE, FALSE);
                }
                else if (g_str_has_prefix (terms[i], "i-:")) {
                        set_ingredient_tag (editor, terms[i] + 3, FALSE, TRUE);
                }
                else if (g_str_has_prefix (terms[i], "di:")) {
                        set_diet_tag (editor, terms[i] + 3, TRUE);
                }
                else if (g_str_has_prefix (terms[i], "me:")) {
                        set_meal_tag (editor, terms[i] + 3, TRUE);
                }
                else {
                        append_search_text (editor, terms[i]);
                }
        }

        gtk_editable_set_position (GTK_EDITABLE (editor->entry), -1);
        g_signal_handlers_unblock_by_func (editor->entry, entry_changed_cb, editor);

        g_signal_emit_by_name (editor->entry, "search-changed", 0);
}

gboolean
gr_query_editor_handle_event (GrQueryEditor *editor,
                              GdkEvent      *event)
{
        if (gtk_widget_is_visible (editor->popover)) {
                if (gtk_revealer_get_child_revealed (GTK_REVEALER (editor->ing_search_revealer))) {
                        gtk_widget_grab_focus (editor->ing_filter_entry);

                        return gtk_widget_event (editor->ing_filter_entry, event);
                }

                return GDK_EVENT_PROPAGATE;
        }

        if (event->type == GDK_KEY_PRESS) {
                GdkEventKey *e = (GdkEventKey *) event;
                if ((e->state & GDK_MOD1_MASK) > 0 && e->keyval == GDK_KEY_Down) {
                        gtk_popover_popup (GTK_POPOVER (editor->popover));
                        return GDK_EVENT_PROPAGATE;
                }
        }

   return gtk_search_bar_handle_event (GTK_SEARCH_BAR (editor), event);
}
