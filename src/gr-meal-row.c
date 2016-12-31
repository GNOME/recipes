/* gr-meal-row.c:
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

#include <glib.h>
#include <glib/gi18n.h>

#include "gr-meal.h"
#include "gr-meal-row.h"


struct _GrMealRow
{
        GtkListBoxRow parent_instance;

        GtkWidget *label;
        GtkWidget *image;

        char *meal;

        gboolean include;

        GdTaggedEntry *entry;
        GdTaggedEntryTag *tag;
};

G_DEFINE_TYPE (GrMealRow, gr_meal_row, GTK_TYPE_LIST_BOX_ROW)

enum {
        PROP_0,
        PROP_MEAL,
        PROP_INCLUDE,
        N_PROPS
};

static void
gr_meal_row_finalize (GObject *object)
{
        GrMealRow *self = (GrMealRow *)object;

        g_free (self->meal);
        g_clear_object (&self->tag);

        G_OBJECT_CLASS (gr_meal_row_parent_class)->finalize (object);
}

static void
gr_meal_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
        GrMealRow *self = GR_MEAL_ROW (object);

        switch (prop_id)
          {
          case PROP_MEAL:
                  g_value_set_string (value, self->meal);
                  break;
          case PROP_INCLUDE:
                  g_value_set_boolean (value, self->include);
                  break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
update_image (GrMealRow *self)
{
        if (self->include)
                gtk_widget_set_opacity (self->image, 1);
        else
                gtk_widget_set_opacity (self->image, 0);
}

static void
update_label (GrMealRow *self)
{
        gtk_label_set_label (GTK_LABEL (self->label), gr_meal_get_title (self->meal));
}

static void
clear_other_tags (GrMealRow *self)
{
        GtkWidget *box, *row;
        GList *children, *l;

        box = gtk_widget_get_parent (GTK_WIDGET (self));

        children = gtk_container_get_children (GTK_CONTAINER (box));
        for (l = children; l; l = l->next) {
                row = l->data;

                if (row == GTK_WIDGET (self))
                        continue;

                if (!GR_IS_MEAL_ROW (row))
                        continue;

                g_object_set (row, "include", FALSE, NULL);
        }
        g_list_free (children);
}

static void
update_tag (GrMealRow *self)
{
        if (!self->entry)
                return;

        if (!self->include) {
                if (self->tag) {
                        gd_tagged_entry_remove_tag (self->entry, self->tag);
                        g_clear_object (&self->tag);
                        g_signal_emit_by_name (self->entry, "search-changed", 0);
                }
                return;
        }

        if (self->include && !self->tag) {
                clear_other_tags (self);
                self->tag = gd_tagged_entry_tag_new ("");
                gd_tagged_entry_tag_set_style (self->tag, "meal-tag");
                gd_tagged_entry_add_tag (self->entry, self->tag);
                g_object_set_data (G_OBJECT (self->tag), "row", self);
        }

        if (self->include)
                gd_tagged_entry_tag_set_label (self->tag, gtk_label_get_label (GTK_LABEL (self->label)));

        g_signal_emit_by_name (self->entry, "search-changed", 0);
}

static void
gr_meal_row_notify (GObject *object, GParamSpec *pspec)
{
        GrMealRow *self = GR_MEAL_ROW (object);

        if (pspec->param_id == PROP_MEAL)
                update_label (self);

        if (pspec->param_id == PROP_INCLUDE)
                update_image (self);

        update_tag (self);
}

static void
gr_meal_row_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
        GrMealRow *self = GR_MEAL_ROW (object);

        switch (prop_id)
          {
          case PROP_MEAL:
                if (g_strcmp0 (self->meal, g_value_get_string (value)) != 0) {
                        g_free (self->meal);
                        self->meal = g_value_dup_string (value);
                        g_object_notify_by_pspec (object, pspec);
                }
                break;
          case PROP_INCLUDE:
                if (self->include != g_value_get_boolean (value)) {
                        self->include = g_value_get_boolean (value);
                        g_object_notify_by_pspec (object, pspec);
                }
                break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_meal_row_class_init (GrMealRowClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_meal_row_finalize;
        object_class->get_property = gr_meal_row_get_property;
        object_class->set_property = gr_meal_row_set_property;
        object_class->notify = gr_meal_row_notify;

        pspec = g_param_spec_string ("meal", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
        g_object_class_install_property (object_class, PROP_MEAL, pspec);

        pspec = g_param_spec_boolean ("include", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
        g_object_class_install_property (object_class, PROP_INCLUDE, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-meal-row.ui");

        gtk_widget_class_bind_template_child (widget_class, GrMealRow, label);
        gtk_widget_class_bind_template_child (widget_class, GrMealRow, image);
}

static void
gr_meal_row_init (GrMealRow *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));
}

GrMealRow *
gr_meal_row_new (const char *meal)
{
        return GR_MEAL_ROW (g_object_new (GR_TYPE_MEAL_ROW,
                                          "meal", meal,
                                          NULL));
}

void
gr_meal_row_set_entry (GrMealRow     *row,
                       GdTaggedEntry *entry)
{
        row->entry = entry;
        g_object_add_weak_pointer (G_OBJECT (entry), (gpointer *)&row->entry);
        update_tag (row);
}

char *
gr_meal_row_get_search_term (GrMealRow *row)
{
        if (row->include)
                return g_strconcat ("me:", row->meal, NULL);
        else
                return NULL;
}

char *
gr_meal_row_get_label (GrMealRow *row)
{
        if (row->include)
                return g_strdup (gtk_label_get_label (GTK_LABEL (row->label)));
        else
                return NULL;
}

const char *
gr_meal_row_get_meal (GrMealRow *row)
{
        return row->meal;
}
