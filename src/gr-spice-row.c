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


/** Spiciness levels:
 *
 *  We use numbers between 0 and 100 for spiciness, currently, and we discriminate
 *  4 levels of spiciness:
 *  mild:     0 - 24
 *  spicy:   25 - 49
 *  hot:     50 - 74
 *  extreme: 75 - 99
 *
 *  When selecting values in the edit, we assign the following values:
 *  mild:    15
 *  spicy:   40
 *  hot:     60
 *  extreme: 90
 *
 *  For the search, we have the following queries in the list:
 *  mild:             < 25
 *  at most spicy:    < 50
 *  at least spicy:  >= 25
 *  at most hot:      < 75
 *  at least hot:    >= 50
 *  very spicy:      >= 75
 */

struct _GrSpiceRow
{
        GtkListBoxRow parent_instance;

        GtkWidget *label;
        GtkWidget *image;

        char *spice;

        gboolean less;
        gboolean include;

        GdTaggedEntry *entry;
        GdTaggedEntryTag *tag;
};

G_DEFINE_TYPE (GrSpiceRow, gr_spice_row, GTK_TYPE_LIST_BOX_ROW)

enum {
        PROP_0,
        PROP_SPICE,
        PROP_LESS,
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
          case PROP_LESS:
                  g_value_set_boolean (value, self->less);
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
spice_get_title (GrSpiceRow *self)
{
        if (strcmp (self->spice, "mild") == 0) {
                return _("Mild");
        }
        else if (strcmp (self->spice, "spicy") == 0) {
                if (self->less)
                        return _("Mild or somewhat spicy");
                else
                        return _("At least somewhat spicy");
        }
        else if (strcmp (self->spice, "hot") == 0) {
                if (self->less)
                        return _("At most hot");
                else
                        return _("Hot or very spicy");
        }
        else if (strcmp (self->spice, "extreme") == 0) {
                return _("Very spicy");
        }
        else
                return "ERROR";
}

static void
update_label (GrSpiceRow *self)
{
        gtk_label_set_label (GTK_LABEL (self->label), spice_get_title (self));
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

        if ((self->include) && !self->tag) {
                clear_other_tags (self);
                self->tag = gd_tagged_entry_tag_new ("");
                gd_tagged_entry_tag_set_style (self->tag, "spice-tag");
                gd_tagged_entry_add_tag (self->entry, self->tag);
                g_object_set_data (G_OBJECT (self->tag), "row", self);
        }

        if (self->include)
                gd_tagged_entry_tag_set_label (self->tag,
                                               gtk_label_get_label (GTK_LABEL (self->label)));

        g_signal_emit_by_name (self->entry, "search-changed", 0);
}

static void
gr_spice_row_notify (GObject *object, GParamSpec *pspec)
{
        GrSpiceRow *self = GR_SPICE_ROW (object);

        if (pspec->param_id == PROP_SPICE)
                update_label (self);

        if (pspec->param_id == PROP_INCLUDE) {
                update_label (self);
                update_image (self);
        }

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
          case PROP_LESS:
                if (self->less != g_value_get_boolean (value)) {
                        self->less = g_value_get_boolean (value);
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

        pspec = g_param_spec_boolean ("less", NULL, NULL,
                                      FALSE,
                                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
        g_object_class_install_property (object_class, PROP_LESS, pspec);

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
gr_spice_row_new (const char *spice,
                  gboolean    less)
{
        return GR_SPICE_ROW (g_object_new (GR_TYPE_SPICE_ROW,
                                           "spice", spice,
                                           "less", less,
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
        if (!row->include)
                return NULL;

        if (strcmp (row->spice, "mild") == 0)
                return g_strdup ("s-:24");
        else if (strcmp (row->spice, "spicy") == 0) {
                if (row->less)
                        return g_strdup ("s-:49");
                else
                        return g_strdup ("s+:25");
        }
        else if (strcmp (row->spice, "hot") == 0) {
                if (row->less)
                        return g_strdup ("s-:74");
                else
                        return g_strdup ("s+:50");
        }
        else if (strcmp (row->spice, "extreme") == 0)
                return g_strdup ("s+:75");
        else
                return g_strdup ("ERROR");
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
