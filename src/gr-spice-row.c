/* gr-spice-row.c:
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

#include "gr-spice-row.h"


struct _GrSpiceRow
{
        GtkListBoxRow parent_instance;

        GtkWidget *label;
        GtkWidget *image;

        char *spice;

        gboolean include;

        GdTaggedEntry *entry;
        GdTaggedEntryTag *tag;
};

G_DEFINE_TYPE (GrSpiceRow, gr_spice_row, GTK_TYPE_LIST_BOX_ROW)

enum {
        PROP_0,
        PROP_SPICE,
        PROP_INCLUDE,
        N_PROPS
};

static void
gr_spice_row_finalize (GObject *object)
{
        GrSpiceRow *self = (GrSpiceRow *)object;

        g_free (self->spice);
        g_clear_object (&self->tag);

        G_OBJECT_CLASS (gr_spice_row_parent_class)->finalize (object);
}

static void
gr_spice_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
        GrSpiceRow *self = GR_SPICE_ROW (object);

        switch (prop_id)
          {
          case PROP_SPICE:
                  g_value_set_string (value, self->spice);
                  break;
          case PROP_INCLUDE:
                  g_value_set_boolean (value, self->include);
                  break;
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
update_image (GrSpiceRow *self)
{
        if (self->include)
                gtk_widget_set_opacity (self->image, 1);
        else
                gtk_widget_set_opacity (self->image, 0);
}

static const char *
spice_get_title (const char *spice)
{
        if (strcmp (spice, "mild") == 0)
                return _("Mild");
        else if (strcmp (spice, "spicy") == 0)
                return _("Somewhat Spicy");
        else if (strcmp (spice, "hot") == 0)
                return _("Hot");
        else if (strcmp (spice, "extreme") == 0)
                return _("Very Spicy");
        else
                return "ERROR";
}

static void
update_label (GrSpiceRow *self)
{
        gtk_label_set_label (GTK_LABEL (self->label), spice_get_title (self->spice));
}

static void
clear_other_tags (GrSpiceRow *self)
{
        GtkWidget *box, *row;
        GList *children, *l;

        box = gtk_widget_get_parent (GTK_WIDGET (self));

        children = gtk_container_get_children (GTK_CONTAINER (box));
        for (l = children; l; l = l->next) {
                row = l->data;

                if (row == GTK_WIDGET (self))
                        continue;

                if (!GR_IS_SPICE_ROW (row))
                        continue;

                g_object_set (row, "include", FALSE, NULL);
        }
        g_list_free (children);
}

static void
update_tag (GrSpiceRow *self)
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
                gd_tagged_entry_tag_set_style (self->tag, "spice-tag");
                gd_tagged_entry_add_tag (self->entry, self->tag);
                g_object_set_data (G_OBJECT (self->tag), "row", self);
        }

        if (self->include)
                gd_tagged_entry_tag_set_label (self->tag, gtk_label_get_label (GTK_LABEL (self->label)));

        g_signal_emit_by_name (self->entry, "search-changed", 0);
}

static void
gr_spice_row_notify (GObject *object, GParamSpec *pspec)
{
        GrSpiceRow *self = GR_SPICE_ROW (object);

        if (pspec->param_id == PROP_SPICE)
                update_label (self);

        if (pspec->param_id == PROP_INCLUDE)
                update_image (self);

        update_tag (self);
}

static void
gr_spice_row_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
        GrSpiceRow *self = GR_SPICE_ROW (object);

        switch (prop_id)
          {
          case PROP_SPICE:
                if (g_strcmp0 (self->spice, g_value_get_string (value)) != 0) {
                        g_free (self->spice);
                        self->spice = g_value_dup_string (value);
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
gr_spice_row_class_init (GrSpiceRowClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_spice_row_finalize;
        object_class->get_property = gr_spice_row_get_property;
        object_class->set_property = gr_spice_row_set_property;
        object_class->notify = gr_spice_row_notify;

        pspec = g_param_spec_string ("spice", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
        g_object_class_install_property (object_class, PROP_SPICE, pspec);

        pspec = g_param_spec_boolean ("include", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
        g_object_class_install_property (object_class, PROP_INCLUDE, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-spice-row.ui");

        gtk_widget_class_bind_template_child (widget_class, GrSpiceRow, label);
        gtk_widget_class_bind_template_child (widget_class, GrSpiceRow, image);
}

static void
gr_spice_row_init (GrSpiceRow *self)
{
        gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
        gtk_widget_init_template (GTK_WIDGET (self));
}

GrSpiceRow *
gr_spice_row_new (const char *spice)
{
        return GR_SPICE_ROW (g_object_new (GR_TYPE_SPICE_ROW,
                                          "spice", spice,
                                          NULL));
}

void
gr_spice_row_set_entry (GrSpiceRow     *row,
                       GdTaggedEntry *entry)
{
        row->entry = entry;
        g_object_add_weak_pointer (G_OBJECT (entry), (gpointer *)&row->entry);
        update_tag (row);
}

char *
gr_spice_row_get_search_term (GrSpiceRow *row)
{
        if (row->include) {
                if (strcmp (row->spice, "mild") == 0)
                        return g_strconcat ("s-:25", NULL);
                else if (strcmp (row->spice, "spicy") == 0)
                        return g_strconcat ("s-:50", NULL);
                else if (strcmp (row->spice, "hot") == 0)
                        return g_strconcat ("s+:50", NULL);
                else if (strcmp (row->spice, "extreme") == 0)
                        return g_strconcat ("s+:70", NULL);
                else
                        return g_strdup ("ERROR");
        }
        else
                return NULL;
}

char *
gr_spice_row_get_label (GrSpiceRow *row)
{
        if (row->include)
                return g_strdup (gtk_label_get_label (GTK_LABEL (row->label)));
        else
                return NULL;
}

const char *
gr_spice_row_get_spice (GrSpiceRow *row)
{
        return row->spice;
}
