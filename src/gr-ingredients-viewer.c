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
#include "gr-utils.h"


struct _GrIngredientsViewer
{
        GtkEventBox parent_instance;

        GtkWidget *title_stack;
        GtkWidget *title_entry;
        GtkWidget *list;

        char **ingredients;
};


G_DEFINE_TYPE (GrIngredientsViewer, gr_ingredients_viewer, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_TITLE,
        PROP_EDITABLE_TITLE,
        PROP_INGREDIENTS
};

static void
gr_ingredients_viewer_finalize (GObject *object)
{
        GrIngredientsViewer *viewer = GR_INGREDIENTS_VIEWER (object);

        g_strfreev (viewer->ingredients);

        G_OBJECT_CLASS (gr_ingredients_viewer_parent_class)->finalize (object);
}

static void
gr_ingredients_viewer_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        GrIngredientsViewer *self = GR_INGREDIENTS_VIEWER (object);

        switch (prop_id)
          {
          case PROP_TITLE:
                g_value_set_string (value, gtk_entry_get_text (GTK_ENTRY (self->title_entry)));
                break;

          case PROP_EDITABLE_TITLE: {
                        const char *visible;

                        visible = gtk_stack_get_visible_child_name (GTK_STACK (self->title_stack));
                        g_value_set_boolean (value, strcmp (visible, "entry") == 0);
                }
                break;

          case PROP_INGREDIENTS:
                g_value_set_boxed (value, self->ingredients);
                break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_ingredients_viewer_set_ingredients (GrIngredientsViewer  *viewer,
                                       const char          **ings)
{
        int i;

        g_strfreev (viewer->ingredients);
        viewer->ingredients = g_strdupv ((char **)ings);

        container_remove_all (GTK_CONTAINER (viewer->list));

        for (i = 0; ings && ings[i]; i++) {
                GtkWidget *row;

                row = g_object_new (GR_TYPE_INGREDIENTS_VIEWER_ROW, "ingredient", ings[i], NULL);

                gtk_container_add (GTK_CONTAINER (viewer->list), row);
        }
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
                gtk_entry_set_text (GTK_ENTRY (self->title_entry), g_value_get_string (value));
                break;

          case PROP_EDITABLE_TITLE: {
                        gboolean editable;

                        editable = g_value_get_boolean (value);
                        gtk_stack_set_visible_child_name (GTK_STACK (self->title_stack),
                                                                     editable ? "entry" : "label");
                }
                break;

          case PROP_INGREDIENTS:
                gr_ingredients_viewer_set_ingredients (self, (const char **)g_value_get_boxed (value));
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

        pspec = g_param_spec_string ("title", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_TITLE, pspec);

        pspec = g_param_spec_boxed ("ingredients", NULL, NULL,
                                    G_TYPE_STRV,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_INGREDIENTS, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-ingredients-viewer.ui");
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, title_stack);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, title_entry);
        gtk_widget_class_bind_template_child (widget_class, GrIngredientsViewer, list);
}
