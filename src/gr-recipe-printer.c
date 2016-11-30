/* gr-recipe-printer.c
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
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

#include "gr-recipe-printer.h"
#include "gr-images.h"
#include "gr-utils.h"


struct _GrRecipePrinter
{
        GObject parent_instance;

        GtkWindow *window;

        PangoLayout *title_layout;
        PangoLayout *left_layout;
        PangoLayout *bottom_layout;

        GrRecipe *recipe;
};

G_DEFINE_TYPE (GrRecipePrinter, gr_recipe_printer, G_TYPE_OBJECT)

enum {
        PROP_0,
        N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gr_recipe_printer_finalize (GObject *object)
{
        GrRecipePrinter *self = (GrRecipePrinter *)object;

        G_OBJECT_CLASS (gr_recipe_printer_parent_class)->finalize (object);
}

static void
gr_recipe_printer_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GrRecipePrinter *self = GR_RECIPE_PRINTER (object);

        switch (prop_id)
          {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_recipe_printer_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GrRecipePrinter *self = GR_RECIPE_PRINTER (object);

        switch (prop_id)
          {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_recipe_printer_class_init (GrRecipePrinterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gr_recipe_printer_finalize;
        object_class->get_property = gr_recipe_printer_get_property;
        object_class->set_property = gr_recipe_printer_set_property;
}

static void
gr_recipe_printer_init (GrRecipePrinter *self)
{
}

GrRecipePrinter *
gr_recipe_printer_new (GtkWindow *parent)
{
        GrRecipePrinter *printer;

        printer = g_object_new (GR_TYPE_RECIPE_PRINTER, NULL);

        printer->window = parent;
}

static void
begin_print (GtkPrintOperation *operation,
             GtkPrintContext   *context,
             GrRecipePrinter   *printer)
{
        int width, height;
        PangoFontDescription *title_font;
        PangoFontDescription *body_font;
        PangoAttrList *attrs;
        PangoAttribute *attr;
        char **ingredients;
        int length;
        int i;
        g_autoptr(GString) s = NULL;

        width = gtk_print_context_get_width (context);
        height = gtk_print_context_get_height (context);

        title_font = pango_font_description_from_string ("Cantarell Bold 24");
        body_font = pango_font_description_from_string ("Cantarell 18");

        printer->title_layout = gtk_print_context_create_pango_layout (context);
        pango_layout_set_font_description (printer->title_layout, title_font);

        pango_layout_set_text (printer->title_layout, gr_recipe_get_name (printer->recipe), -1);

        printer->left_layout = gtk_print_context_create_pango_layout (context);
        pango_layout_set_width (printer->left_layout, (width / 2) * PANGO_SCALE);
        pango_layout_set_font_description (printer->left_layout, body_font);

        s = g_string_new ("");

        g_string_append_printf (s, "%s %s\n", _("Author:"), gr_recipe_get_author (printer->recipe));
        g_string_append_printf (s, "%s %s\n", _("Preparation:"), gr_recipe_get_prep_time (printer->recipe));
        g_string_append_printf (s, "%s %s\n", _("Cooking:"), gr_recipe_get_cook_time (printer->recipe));
        g_string_append_printf (s, "%s %d", _("Serves:"), gr_recipe_get_serves (printer->recipe));

        pango_layout_set_text (printer->left_layout, s->str, s->len);

        g_string_truncate (s, 0);

        printer->bottom_layout = gtk_print_context_create_pango_layout (context);
        pango_layout_set_width (printer->bottom_layout, width * PANGO_SCALE);
        pango_layout_set_font_description (printer->bottom_layout, body_font);

        g_string_append (s, gr_recipe_get_description (printer->recipe));
        g_string_append (s, "\n\n");

        attrs = pango_attr_list_new ();

        attr = pango_attr_font_desc_new (title_font);
        attr->start_index = s->len;

        g_string_append (s, _("Ingredients"));

        attr->end_index = s->len + 1;
        pango_attr_list_insert (attrs, attr);

        g_string_append (s, "\n");

        ingredients = g_strsplit (gr_recipe_get_ingredients (printer->recipe), "\n", -1);
        length = g_strv_length (ingredients);
        if (length > 3) {
                PangoTabArray *tabs;
                int mid;

                tabs = pango_tab_array_new (2, FALSE);
                pango_tab_array_set_tab (tabs, 0, PANGO_TAB_LEFT, 0);
                pango_tab_array_set_tab (tabs, 1, PANGO_TAB_LEFT, (width / 2) * PANGO_SCALE);
                pango_layout_set_tabs (printer->bottom_layout, tabs);
                pango_tab_array_free (tabs);

                mid = length / 2 + length % 2;
                for (i = 0; i < mid; i++) {
                        g_string_append (s, "\n");
                        g_string_append (s, ingredients[i]);
                        g_string_append (s, "\t");
                        if (mid + i < length)
                                g_string_append (s, ingredients[mid + i]);
                }
        }
        else {
                for (i = 0; i < length; i++) {
                        g_string_append (s, "\n");
                        g_string_append (s, ingredients[i]);
                }
        }

        g_string_append (s, "\n\n");

        attr = pango_attr_font_desc_new (title_font);
        attr->start_index = s->len;

        g_string_append (s, _("Instructions"));

        attr->end_index = s->len + 1;
        pango_attr_list_insert (attrs, attr);

        g_string_append (s, "\n\n");
        g_string_append (s, gr_recipe_get_instructions (printer->recipe));

        pango_layout_set_text (printer->bottom_layout, s->str, s->len);
        pango_layout_set_attributes (printer->bottom_layout, attrs);
        pango_attr_list_unref (attrs);

        gtk_print_operation_set_n_pages (operation, 1);

        pango_font_description_free (title_font);
        pango_font_description_free (body_font);
}

static void
end_print (GtkPrintOperation *operation,
             GtkPrintContext *context,
             GrRecipePrinter *printer)
{
        g_clear_object (&printer->title_layout);
        g_clear_object (&printer->left_layout);
        g_clear_object (&printer->bottom_layout);
        g_clear_object (&printer->recipe);
}

static void
draw_page (GtkPrintOperation *operation,
           GtkPrintContext   *context,
           int                page_nr,
           GrRecipePrinter   *printer)
{
        cairo_t *cr;
        PangoRectangle logical_rect;
        int baseline;
        int width, height;
        g_autoptr(GArray) images = NULL;
        GrRotatedImage *ri;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        int y;

        cr = gtk_print_context_get_cairo_context (context);

        width = gtk_print_context_get_width (context);
        height = gtk_print_context_get_height (context);

        cairo_set_source_rgb (cr, 0, 0, 0);

        pango_layout_get_extents (printer->title_layout, NULL, &logical_rect);
        baseline = pango_layout_get_baseline (printer->title_layout);

        cairo_move_to (cr, logical_rect.x / 1024.0 + (width - logical_rect.width / 1024.0) / 2, baseline / 1024.0 - logical_rect.y / 1024.0);
        pango_cairo_show_layout (cr, printer->title_layout);

        y = baseline / 1024.0 - logical_rect.y / 1024.0 + logical_rect.height / 1024.0 + 10;

        g_object_get (printer->recipe, "images", &images, NULL);
        ri = &g_array_index (images, GrRotatedImage, 0);
        pixbuf = load_pixbuf_fit_size (ri->path, ri->angle, width / 2, height / 4, FALSE);

        gdk_cairo_set_source_pixbuf (cr, pixbuf, width - gdk_pixbuf_get_width (pixbuf), y);
        cairo_paint (cr);

        cairo_set_source_rgb (cr, 0, 0, 0);

        pango_layout_get_extents (printer->left_layout, NULL, &logical_rect);
        baseline = pango_layout_get_baseline (printer->left_layout);

        cairo_move_to (cr, logical_rect.x / 1024.0, y + baseline / 1024.0 - logical_rect.y / 1024.0);
        pango_cairo_show_layout (cr, printer->left_layout);

        y += MAX (gdk_pixbuf_get_height (pixbuf), baseline / 1024.0 - logical_rect.y / 1024.0 + logical_rect.height / 1024.0) + 10;

        pango_layout_get_extents (printer->bottom_layout, NULL, &logical_rect);
        baseline = pango_layout_get_baseline (printer->bottom_layout);

        cairo_move_to (cr, logical_rect.x / 1024.0, y + baseline / 1024.0 - logical_rect.y / 1024.0);
        pango_cairo_show_layout (cr, printer->bottom_layout);
}

static void
print_done (GtkPrintOperation       *operation,
            GtkPrintOperationResult  res,
            GrRecipePrinter         *printer)
{
       GError *error = NULL;

  if (res == GTK_PRINT_OPERATION_RESULT_ERROR)
    {

      GtkWidget *error_dialog;

      gtk_print_operation_get_error (operation, &error);

      error_dialog = gtk_message_dialog_new (GTK_WINDOW (printer->window),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_CLOSE,
                                             "%s\n%s",
                                             _("Error printing file:"),
                                             error ? error->message : _("No details"));
      g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (error_dialog);
    }
  else if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
    {
            /* TODO: print settings */
    }

  if (!gtk_print_operation_is_finished (operation))
    {
            /* TODO monitoring */
    }

  g_object_unref (operation);
}

void
gr_recipe_printer_print (GrRecipePrinter *printer,
                         GrRecipe        *recipe)
{
        GtkPrintOperation *operation;

        printer->recipe = g_object_ref (recipe);

        operation = gtk_print_operation_new ();

        /* TODO: print settings */
        /* TODO: page setup */

        g_signal_connect (operation, "begin-print", G_CALLBACK (begin_print), printer);
        g_signal_connect (operation, "end-print", G_CALLBACK (end_print), printer);
        g_signal_connect (operation, "draw-page", G_CALLBACK (draw_page), printer);
        g_signal_connect (operation, "done", G_CALLBACK (print_done), printer);

        gtk_print_operation_set_allow_async (operation, TRUE);

        gtk_print_operation_run (operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, printer->window, NULL);
}
