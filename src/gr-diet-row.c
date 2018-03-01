/* gr-diet-row.c:
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

#include "types.h"
#include "gr-diet-row.h"
#include "gr-diet.h"


struct _GrDietRow
{
        GtkListBoxRow parent_instance;

        GtkWidget *label;
        GtkWidget *image;

        char *diet_name;
        GrDiets diet;

        gboolean include;

        GdTaggedEntry *entry;
        GdTaggedEntryTag *tag;
};

G_DEFINE_TYPE (GrDietRow, gr_diet_row, GTK_TYPE_LIST_BOX_ROW)

enum {
        PROP_0,
        PROP_DIET,
        PROP_INCLUDE,
        N_PROPS
};

static void
gr_diet_row_finalize (GObject *object)
{
        GrDietRow *self = (GrDietRow *)object;

        g_clear_object (&self->tag);

        G_OBJECT_CLASS (gr_diet_row_parent_class)->finalize (object);
}

static void
gr_diet_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
        GrDietRow *self = GR_DIET_ROW (object);

        switch (prop_id)
          {
          case PROP_DIET:
                  g_value_set_flags (value, self->diet);
                  break;
          case PROP_INCLUDE:
                  g_value_set_boolean (value, self->include);
                  break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
update_image (GrDietRow *self)
{
        if (self->include)
                gtk_widget_set_opacity (self->image, 1);
        else
                gtk_widget_set_opacity (self->image, 0);
}

static void
update_label (GrDietRow *self)
{
        switch (self->diet)
          {
          case GR_DIET_GLUTEN_FREE:
                gtk_label_set_label (GTK_LABEL (self->label), _("Gluten-free"));
                break;
          case GR_DIET_NUT_FREE:
                gtk_label_set_label (GTK_LABEL (self->label), _("Nut-free"));
                break;
          case GR_DIET_VEGAN:
                gtk_label_set_label (GTK_LABEL (self->label), _("Vegan"));
                break;
          case GR_DIET_VEGETARIAN:
                gtk_label_set_label (GTK_LABEL (self->label), _("Vegetarian"));
                break;
          case GR_DIET_MILK_FREE:
                gtk_label_set_label (GTK_LABEL (self->label), _("Milk-free"));
                break;
          case GR_DIET_HALAL:
                gtk_label_set_label (GTK_LABEL (self->label), _("Halal"));
                break;
          default:
                gtk_label_set_label (GTK_LABEL (self->label), "Unknown");
                break;
          }
}

static void
update_tag (GrDietRow *self)
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
                self->tag = gd_tagged_entry_tag_new ("");
                gd_tagged_entry_tag_set_style (self->tag, "diet-tag");
                gd_tagged_entry_add_tag (self->entry, self->tag);
                g_object_set_data (G_OBJECT (self->tag), "row", self);
        }

        if (self->include)
                gd_tagged_entry_tag_set_label (self->tag, gtk_label_get_label (GTK_LABEL (self->label)));

        g_signal_emit_by_name (self->entry, "search-changed", 0);
}

static void
gr_diet_row_notify (GObject *object, GParamSpec *pspec)
{
        GrDietRow *self = GR_DIET_ROW (object);

        if (pspec->param_id == PROP_DIET)
                update_label (self);

        if (pspec->param_id == PROP_INCLUDE)
                update_image (self);

        update_tag (self);
}

static void
gr_diet_row_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
        GrDietRow *self = GR_DIET_ROW (object);

        switch (prop_id)
          {
          case PROP_DIET:
                  self->diet = g_value_get_flags (value);
                  break;
          case PROP_INCLUDE:
                  self->include = g_value_get_boolean (value);
                  break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_diet_row_class_init (GrDietRowClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_diet_row_finalize;
        object_class->get_property = gr_diet_row_get_property;
        object_class->set_property = gr_diet_row_set_property;
        object_class->notify = gr_diet_row_notify;

        pspec = g_param_spec_flags ("diet", NULL, NULL,
                                   GR_TYPE_DIETS, 0,
                                   G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_DIET, pspec);

        pspec = g_param_spec_boolean ("include", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INCLUDE, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-diet-row.ui");

        gtk_widget_class_bind_template_child (widget_class, GrDietRow, label);
        gtk_widget_class_bind_template_child (widget_class, GrDietRow, image);
}

static void
gr_diet_row_init (GrDietRow *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));
}

GrDietRow *
gr_diet_row_new (GrDiets diet)
{
        return GR_DIET_ROW (g_object_new (GR_TYPE_DIET_ROW,
                                          "diet", diet,
                                          NULL));
}

void
gr_diet_row_set_entry (GrDietRow     *row,
                       GdTaggedEntry *entry)
{
        row->entry = entry;
        g_object_add_weak_pointer (G_OBJECT (entry), (gpointer *)&row->entry);
        update_tag (row);
}

char *
gr_diet_row_get_search_term (GrDietRow *row)
{
        if (row->include)
                return g_strconcat ("di:", gr_diet_row_get_diet (row), NULL);
        else
                return NULL;
}

const char *
gr_diet_row_get_diet (GrDietRow *row)
{
        switch (row->diet)
          {
          case GR_DIET_GLUTEN_FREE:
                return "gluten-free";
          case GR_DIET_NUT_FREE:
                return "nut-free";
          case GR_DIET_VEGAN:
                return "vegan";
          case GR_DIET_VEGETARIAN:
                return "vegetarian";
          case GR_DIET_MILK_FREE:
                return "milk-free";
          case GR_DIET_HALAL:
                return "halal";
          default:
                return NULL;
          }
}

char *
gr_diet_row_get_label (GrDietRow *row)
{
        if (row->include)
                return g_strdup (gtk_label_get_label (GTK_LABEL (row->label)));
        else
                return NULL;
}
