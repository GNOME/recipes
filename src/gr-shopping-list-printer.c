/* gr-shopping-list-printer.c:
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

#include <glib/gi18n.h>

#include "gr-shopping-list-printer.h"
#include "gr-utils.h"


static void
shopping_item_free (gpointer data)
{
        ShoppingListItem *item = data;

        g_free (item->amount);
        g_free (item->name);
        g_free (item);
}

static ShoppingListItem *
shopping_item_copy (ShoppingListItem *item)
{
        ShoppingListItem *item2;

        item2 = g_new (ShoppingListItem, 1);
        item2->amount = g_strdup (item->amount);
        item2->name = g_strdup (item->name);

        return item2;
}

struct _GrShoppingListPrinter
{
        GObject parent_instance;

        GtkWindow *window;

        PangoLayout *layout;
        GList *page_breaks;

        GList *recipes;
        GList *items;
};

G_DEFINE_TYPE (GrShoppingListPrinter, gr_shopping_list_printer, G_TYPE_OBJECT)

static void
finalize (GObject *object)
{
        GrShoppingListPrinter *printer = GR_SHOPPING_LIST_PRINTER (object);

        g_clear_object (&printer->layout);
        g_list_free (printer->page_breaks);
        g_list_free_full (printer->recipes, g_object_unref);
        g_list_free_full (printer->items, shopping_item_free);

        G_OBJECT_CLASS (gr_shopping_list_printer_parent_class)->finalize (object);
}

static void
gr_shopping_list_printer_class_init (GrShoppingListPrinterClass *klass)
{
        G_OBJECT_CLASS (klass)->finalize = finalize;
}

static void
gr_shopping_list_printer_init (GrShoppingListPrinter *self)
{
}

GrShoppingListPrinter *
gr_shopping_list_printer_new (GtkWindow *parent)
{
        GrShoppingListPrinter *printer;

        printer = g_object_new (GR_TYPE_SHOPPING_LIST_PRINTER, NULL);

        printer->window = parent;

        return printer;
}

static void
begin_print (GtkPrintOperation *operation,
             GtkPrintContext   *context,
             GrShoppingListPrinter   *printer)
{
        double height;
        PangoFontDescription *title_font;
        PangoFontDescription *subtitle_font;
        PangoFontDescription *body_font;
        PangoAttrList *attrs;
        PangoAttribute *attr;
        double page_height;
        GList *page_breaks;
        g_autoptr(GString) s = NULL;
        int num_lines;
        int line;
        GList *l;

        height = gtk_print_context_get_height (context);

        title_font = pango_font_description_from_string ("Cantarell Bold 18");
        subtitle_font = pango_font_description_from_string ("Cantarell Bold 12");
        body_font = pango_font_description_from_string ("Cantarell 12");

        printer->layout = gtk_print_context_create_pango_layout (context);
        pango_layout_set_font_description (printer->layout, body_font);

        s = g_string_new ("");

        attrs = pango_attr_list_new ();
        attr = pango_attr_font_desc_new (title_font);
        attr->start_index = s->len;

        g_string_append (s, _("Shopping List"));
        g_string_append (s, "\n\n");

        attr->end_index = s->len;
        pango_attr_list_insert (attrs, attr);

        attr = pango_attr_font_desc_new (subtitle_font);
        attr->start_index = s->len;

        g_string_append (s, _("For the following recipes"));
        g_string_append (s, "\n\n");

        attr->end_index = s->len;
        pango_attr_list_insert (attrs, attr);

        for (l = printer->recipes; l; l = l->next) {
                GrRecipe *recipe = l->data;
                g_string_append (s, gr_recipe_get_translated_name (recipe));
                g_string_append (s, "\n");
        }

        g_string_append (s, "\n");

        attr = pango_attr_font_desc_new (title_font);
        attr->start_index = s->len;

        g_string_append (s, _("Items"));
        g_string_append (s, "\n");

        attr->end_index = s->len;
        pango_attr_list_insert (attrs, attr);

        for (l = printer->items; l; l = l->next) {
                ShoppingListItem *item = l->data;

                g_string_append (s, "\n");
                g_string_append (s, item->amount);
                g_string_append (s, " ");
                g_string_append (s, item->name);
        }

        pango_layout_set_text (printer->layout, s->str, s->len);
        pango_layout_set_attributes (printer->layout, attrs);
        pango_attr_list_unref (attrs);

        num_lines = pango_layout_get_line_count (printer->layout);

        page_height = 0;
        page_breaks = NULL;

        for (line = 0; line < num_lines; line++) {
                PangoLayoutLine *layout_line;
                PangoRectangle logical_rect;
                double line_height;

                layout_line = pango_layout_get_line (printer->layout, line);
                pango_layout_line_get_extents (layout_line, NULL, &logical_rect);
                line_height = logical_rect.height / 1024.0;
                if (page_height + line_height > height) {
                        page_breaks = g_list_prepend (page_breaks, GINT_TO_POINTER (line));
                        page_height = 0;
                }

                page_height += line_height;
        }

        page_breaks = g_list_reverse (page_breaks);
        gtk_print_operation_set_n_pages (operation, g_list_length (page_breaks) + 1);
        printer->page_breaks = page_breaks;

        pango_font_description_free (title_font);
        pango_font_description_free (subtitle_font);
        pango_font_description_free (body_font);
}

static void
end_print (GtkPrintOperation *operation,
           GtkPrintContext *context,
           GrShoppingListPrinter *printer)
{
        g_clear_object (&printer->layout);
        g_list_free (printer->page_breaks);
        printer->page_breaks = NULL;
        g_list_free_full (printer->recipes, g_object_unref);
        printer->recipes = NULL;
        g_list_free_full (printer->items, shopping_item_free);
        printer->items = NULL;
}

static void
draw_page (GtkPrintOperation *operation,
           GtkPrintContext   *context,
           int                page_nr,
           GrShoppingListPrinter   *printer)
{
        cairo_t *cr;
        int start, end, i;
        PangoLayoutIter *iter;
        double start_pos;
        GList *pagebreak;

        cr = gtk_print_context_get_cairo_context (context);

        if (page_nr == 0) {
                start = 0;
                start_pos = 0;
        }
        else {
                pagebreak = g_list_nth (printer->page_breaks, page_nr - 1);
                start = GPOINTER_TO_INT (pagebreak->data);
                start_pos = 0;
        }

        pagebreak = g_list_nth (printer->page_breaks, page_nr);
        if (pagebreak == NULL)
                end = pango_layout_get_line_count (printer->layout);
        else
                end = GPOINTER_TO_INT (pagebreak->data);

        i = 0;
        iter = pango_layout_get_iter (printer->layout);
        do {
                PangoRectangle rect;
                PangoLayoutLine *line;
                int base;

                if (i >= start) {
                        line = pango_layout_iter_get_line (iter);
                        pango_layout_iter_get_line_extents (iter, NULL, &rect);
                        base = pango_layout_iter_get_baseline (iter);
                        if (i == start)
                                start_pos -= rect.y / 1024.0;

                        cairo_move_to (cr, rect.x / 1024.0, base / 1024.0 + start_pos);
                        pango_cairo_show_layout_line (cr, line);
                }
                i++;

        } while (i < end && pango_layout_iter_next_line (iter));

        pango_layout_iter_free (iter);
}

static void
print_done (GtkPrintOperation       *operation,
            GtkPrintOperationResult  res,
            GrShoppingListPrinter   *printer)
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
gr_shopping_list_printer_print (GrShoppingListPrinter *printer,
                                GList           *recipes,
                                GList           *items)
{
        GtkPrintOperation *operation;

        if (in_flatpak_sandbox () &&
            !portal_available (GTK_WINDOW (printer->window), "org.freedesktop.portal.Print"))
                return;

        printer->recipes = g_list_copy_deep (recipes, (GCopyFunc)g_object_ref, NULL);
        printer->items = g_list_copy_deep (items, (GCopyFunc)shopping_item_copy, NULL);

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
