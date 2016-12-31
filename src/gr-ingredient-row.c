/* gr-ingredient-row.c:
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

#include "gr-ingredient-row.h"
#include "gr-ingredient.h"


struct _GrIngredientRow
{
        GtkListBoxRow parent_instance;

        GtkWidget *label;
        GtkWidget *image;

        char *id;
        char *ingredient;
        char *cf_ingredient;

        gboolean include;
        gboolean exclude;

        GdTaggedEntry *entry;
        GdTaggedEntryTag *tag;
};

G_DEFINE_TYPE (GrIngredientRow, gr_ingredient_row, GTK_TYPE_LIST_BOX_ROW)

enum {
        PROP_0,
        PROP_INGREDIENT,
        PROP_INCLUDE,
        PROP_EXCLUDE,
        N_PROPS
};

static void
gr_ingredient_row_finalize (GObject *object)
{
        GrIngredientRow *self = (GrIngredientRow *)object;

        g_clear_object (&self->tag);
        g_free (self->id);
        g_free (self->ingredient);
        g_free (self->cf_ingredient);

        G_OBJECT_CLASS (gr_ingredient_row_parent_class)->finalize (object);
}

static void
gr_ingredient_row_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GrIngredientRow *self = GR_INGREDIENT_ROW (object);

        switch (prop_id)
          {
          case PROP_INGREDIENT:
                  g_value_set_string (value, self->ingredient);
                  break;
          case PROP_INCLUDE:
                  g_value_set_boolean (value, self->include);
                  break;
          case PROP_EXCLUDE:
                  g_value_set_boolean (value, self->exclude);
                  break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
update_image (GrIngredientRow *self)
{
        if (self->include || self->exclude) {
                gtk_widget_set_opacity (self->image, 1);
                gtk_image_set_from_icon_name (GTK_IMAGE (self->image), "object-select-symbolic", 1);
        }
        else
                gtk_widget_set_opacity (self->image, 0);
}

static void
update_label (GrIngredientRow *self)
{
        if (self->include)
                gtk_label_set_label (GTK_LABEL (self->label), self->ingredient);
        else if (self->exclude)
                gtk_label_set_label (GTK_LABEL (self->label), gr_ingredient_get_negation (self->ingredient));
        else
                gtk_label_set_label (GTK_LABEL (self->label), self->ingredient);
}

static void
update_tag (GrIngredientRow *self)
{
        if (!self->entry)
                return;

        if (!self->include && !self->exclude) {
                if (self->tag) {
                        gd_tagged_entry_remove_tag (self->entry, self->tag);
                        g_clear_object (&self->tag);
                        g_signal_emit_by_name (self->entry, "search-changed", 0);
                }
                return;
        }

        if ((self->include || self->exclude) && !self->tag) {
                self->tag = gd_tagged_entry_tag_new ("");
                gd_tagged_entry_tag_set_style (self->tag, "ingredient-tag");
                gd_tagged_entry_add_tag (self->entry, self->tag);
                g_object_set_data (G_OBJECT (self->tag), "row", self);
        }

        if (self->include)
                gd_tagged_entry_tag_set_label (self->tag, self->ingredient);
        else if (self->exclude)
                gd_tagged_entry_tag_set_label (self->tag,
                                               gr_ingredient_get_negation (self->ingredient));

        g_signal_emit_by_name (self->entry, "search-changed", 0);
}

static void
gr_ingredient_row_notify (GObject *object, GParamSpec *pspec)
{
        GrIngredientRow *self = GR_INGREDIENT_ROW (object);

        if (pspec->param_id == PROP_INGREDIENT)
                update_label (self);

        if (pspec->param_id == PROP_INCLUDE ||
            pspec->param_id == PROP_EXCLUDE) {
                update_label (self);
                update_image (self);
        }

        update_tag (self);
}

static void
gr_ingredient_row_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GrIngredientRow *self = GR_INGREDIENT_ROW (object);

        switch (prop_id)
          {
          case PROP_INGREDIENT:
                  {
                        const char *term;

                        g_free (self->ingredient);
                        self->ingredient = g_value_dup_string (value);

                        term = gr_ingredient_get_id (self->ingredient);
                        if (!term)
                                term = self->ingredient;
                        g_free (self->id);
                        self->id = g_strdup (term);

                        g_free (self->cf_ingredient);
                        self->cf_ingredient = g_utf8_casefold (self->ingredient, -1);
                  }
                  break;
          case PROP_INCLUDE:
                  self->include = g_value_get_boolean (value);
                  break;
          case PROP_EXCLUDE:
                  self->exclude = g_value_get_boolean (value);
                  break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_ingredient_row_class_init (GrIngredientRowClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_ingredient_row_finalize;
        object_class->get_property = gr_ingredient_row_get_property;
        object_class->set_property = gr_ingredient_row_set_property;
        object_class->notify = gr_ingredient_row_notify;

        pspec = g_param_spec_string ("ingredient", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INGREDIENT, pspec);

        pspec = g_param_spec_boolean ("include", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INCLUDE, pspec);

        pspec = g_param_spec_boolean ("exclude", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_EXCLUDE, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredient-row.ui");

        gtk_widget_class_bind_template_child (widget_class, GrIngredientRow, label);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientRow, image);
}

static void
gr_ingredient_row_init (GrIngredientRow *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));
}

GrIngredientRow *
gr_ingredient_row_new (const char *ingredient)
{
        return GR_INGREDIENT_ROW (g_object_new (GR_TYPE_INGREDIENT_ROW,
                                                "ingredient", ingredient,
                                                NULL));
}

void
gr_ingredient_row_set_entry (GrIngredientRow *row,
                             GdTaggedEntry   *entry)
{
        row->entry = entry;
        g_object_add_weak_pointer (G_OBJECT (entry), (gpointer *)&row->entry);
        update_tag (row);
}

char *
gr_ingredient_row_get_search_term (GrIngredientRow *row)
{
        if (row->include) {
                return g_strconcat ("i+:", row->id, NULL);
        }
        else if (row->exclude) {
                return g_strconcat ("i-:", row->id, NULL);
        }
        else
                return NULL;
}


const char *
gr_ingredient_row_get_filter_term (GrIngredientRow *row)
{
        return row->cf_ingredient;
}

char *
gr_ingredient_row_get_label (GrIngredientRow *row)
{
        if (row->include)
                return g_strdup (row->ingredient);
        else if (row->exclude)
                return g_strdup (gr_ingredient_get_negation (row->ingredient));
        else
                return NULL;
}

const char *
gr_ingredient_row_get_ingredient (GrIngredientRow *row)
{
        return row->ingredient;
}

const char *
gr_ingredient_row_get_id (GrIngredientRow *row)
{
        return row->id;
}
