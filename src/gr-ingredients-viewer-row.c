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

        GtkWidget *label;

        char *ingredient;
};

G_DEFINE_TYPE (GrIngredientsViewerRow, gr_ingredients_viewer_row, GTK_TYPE_LIST_BOX_ROW)

enum {
        PROP_0,
        PROP_INGREDIENT
};

static void
gr_ingredients_viewer_row_finalize (GObject *object)
{
        GrIngredientsViewerRow *self = GR_INGREDIENTS_VIEWER_ROW (object);

        g_free (self->ingredient);

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
          case PROP_INGREDIENT:
                g_value_set_string (value, self->ingredient);
                break;
          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
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
          case PROP_INGREDIENT:
                {
                        g_free (self->ingredient);
                        self->ingredient = g_value_dup_string (value);
                        gtk_label_set_label (GTK_LABEL (self->label), self->ingredient);
                }
                break;
          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
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

        pspec = g_param_spec_string ("ingredient", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INGREDIENT, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredients-viewer-row.ui");

        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewerRow, label);
}

static void
gr_ingredients_viewer_row_init (GrIngredientsViewerRow *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));
}

GrIngredientsViewerRow *
gr_ingredients_viewer_row_new (const char *ingredient)
{
        return GR_INGREDIENTS_VIEWER_ROW (g_object_new (GR_TYPE_INGREDIENTS_VIEWER_ROW,
                                         "ingredient", ingredient,
                                         NULL));
}
