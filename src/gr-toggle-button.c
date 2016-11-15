/* gr-toggle-button.c:
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

#include <string.h>
#include "gr-toggle-button.h"

struct _GrToggleButton
{
	GtkButton parent_instance;

        GtkWidget *label;
        GtkWidget *image;

	gboolean active;
};

G_DEFINE_TYPE (GrToggleButton, gr_toggle_button, GTK_TYPE_BUTTON)

enum {
	PROP_0,
	PROP_ACTIVE,
	PROP_LABEL,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

GrToggleButton *
gr_toggle_button_new (void)
{
	return g_object_new (GR_TYPE_TOGGLE_BUTTON, NULL);
}

static void
gr_toggle_button_finalize (GObject *object)
{
	GrToggleButton *self = (GrToggleButton *)object;

	G_OBJECT_CLASS (gr_toggle_button_parent_class)->finalize (object);
}

static void
set_active (GrToggleButton *button,
            gboolean        active)
{
        GtkStyleContext *context;

        if (button->active == active)
                return;

        button->active = active;

        context = gtk_widget_get_style_context (button->image);
        if (active) {
                gtk_style_context_add_class (context, "checked");
        }
        else {
                gtk_style_context_remove_class (context, "checked");
        }

        g_object_notify (G_OBJECT (button), "active");
}

static void
clicked (GtkButton *button)
{
        GrToggleButton *toggle_button = GR_TOGGLE_BUTTON (button);

        set_active (toggle_button, !toggle_button->active);
}


static void
gr_toggle_button_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
	GrToggleButton *self = GR_TOGGLE_BUTTON (object);

	switch (prop_id)
	  {
          case PROP_LABEL:
                  g_value_set_string (value, gtk_label_get_label (GTK_LABEL (self->label)));
                  break;

          case PROP_ACTIVE:
                  g_value_set_boolean (value, self->active);
                  break;

	  default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	  }
}

static void
gr_toggle_button_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
	GrToggleButton *self = GR_TOGGLE_BUTTON (object);

	switch (prop_id)
	  {
          case PROP_LABEL:
		  gtk_label_set_label (GTK_LABEL (self->label), g_value_get_string (value));
                  break;

          case PROP_ACTIVE:
                  set_active (self, g_value_get_boolean (value));
                  break;

	  default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	  }
}

static void
gr_toggle_button_class_init (GrToggleButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

	object_class->finalize = gr_toggle_button_finalize;
	object_class->get_property = gr_toggle_button_get_property;
	object_class->set_property = gr_toggle_button_set_property;

        button_class->clicked = clicked;

        g_object_class_install_property (object_class,
                                         PROP_ACTIVE,
                                         g_param_spec_boolean ("active", NULL, NULL,
                                                               FALSE, G_PARAM_READWRITE));

        g_object_class_override_property (object_class, PROP_LABEL, "label");
}

static void
gr_toggle_button_init (GrToggleButton *self)
{
        GtkWidget *box;

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_show (box);
        gtk_container_add (GTK_CONTAINER (self), box);

        self->label = gtk_label_new ("");
        gtk_widget_show (self->label);
        gtk_container_add (GTK_CONTAINER (box), self->label);

        self->image = gtk_image_new_from_icon_name ("object-select-symbolic", 1);
        gtk_image_set_pixel_size (GTK_IMAGE (self->image), 24);
        gtk_widget_show (self->image);
        gtk_style_context_add_class (gtk_widget_get_style_context (self->image), "checkmark");
        gtk_container_add (GTK_CONTAINER (box), self->image);
}
