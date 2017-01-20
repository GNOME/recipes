/* gr-ingredient-item.c:
 *
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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


#include "gr-ingredient-item.h"


struct _GrIngredientItem
{
        GtkBox parent_instance;

        GtkWidget *unit_label;
        GtkWidget *ingredient_label;

        gchar *ingredient;
        gchar *unit;
};


G_DEFINE_TYPE (GrIngredientItem, gr_ingredient_item, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_INGREDIENT,
        PROP_UNIT,
};

static void
gr_ingredient_item_finalize (GObject *object)
{
        G_OBJECT_CLASS (gr_ingredient_item_parent_class)->finalize (object);
}

static void
gr_ingredient_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        GrIngredientItem *self = GR_INGREDIENT_ITEM (object);

        switch (prop_id)
          {
          case PROP_INGREDIENT:
                  g_value_set_string (value, self->ingredient);
                  break;
          case PROP_UNIT:
                  g_value_set_string (value, self->unit);
                  break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_ingredient_item_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        GrIngredientItem *self = GR_INGREDIENT_ITEM (object);

        switch (prop_id)
          {
          case PROP_INGREDIENT:
                  {
                          g_free (self->ingredient);
                          self->ingredient = g_value_dup_string (value);
                          gtk_label_set_text (GTK_LABEL (self->ingredient_label), self->ingredient);
                  }
                  break;
          case PROP_UNIT:
                  {
                          g_free (self->unit);
                          self->unit = g_value_dup_string (value);
                          gtk_label_set_text (GTK_LABEL (self->unit_label), self->unit);
                  }
                  break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}


static void
gr_ingredient_item_class_init (GrIngredientItemClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_ingredient_item_finalize;
        object_class->get_property = gr_ingredient_item_get_property;
        object_class->set_property = gr_ingredient_item_set_property;

        pspec = g_param_spec_string ("ingredient", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INGREDIENT, pspec);

        pspec = g_param_spec_string ("unit", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_UNIT, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredient-item.ui");

        gtk_widget_class_bind_template_child (widget_class, GrIngredientItem, unit_label);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientItem, ingredient_label);
}

static void
gr_ingredient_item_init (GrIngredientItem *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));

}

GtkWidget *
gr_ingredient_item_new (gchar *ingredient, gchar *unit)
{
        return g_object_new (GR_TYPE_INGREDIENT_ITEM,
                             "ingredient", ingredient,
                             "unit", unit,
                             NULL);
}
