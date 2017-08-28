/* gr-recipe-printer.c:
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

#include <glib/gi18n.h>

#include "gr-recipe-printer.h"
#include "gr-recipe-formatter.h"
#include "gr-ingredients-list.h"
#include "gr-image.h"
#include "gr-chef.h"
#include "gr-recipe-store.h"
#include "gr-cuisine.h"
#include "gr-meal.h"
#include "gr-season.h"
#include "gr-utils.h"
#include "gr-convert-units.h"


struct _GrRecipePrinter
{
        GObject parent_instance;

        GtkWindow *window;

        PangoLayout *title_layout;
        PangoLayout *left_layout;
        PangoLayout *bottom_layout;
        GdkPixbuf *image;
        GList *page_breaks;

        GrRecipe *recipe;
};

G_DEFINE_TYPE (GrRecipePrinter, gr_recipe_printer, G_TYPE_OBJECT)

static void
gr_recipe_printer_class_init (GrRecipePrinterClass *klass)
{
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

        return printer;
}

static char *
process_instructions (const char *instructions)
{
        g_autoptr(GPtrArray) steps = NULL;
        GString *s;
        int i;

        steps = gr_recipe_parse_instructions (instructions, TRUE);

        s = g_string_new ("");

        for (i = 0; i < steps->len; i++) {
                GrRecipeStep *step = (GrRecipeStep *)g_ptr_array_index (steps, i);

                if (i > 0)
                        g_string_append (s, "\n\n");

                g_string_append (s, step->text);
        }

        return g_string_free (s, FALSE);
}

static void
begin_print (GtkPrintOperation *operation,
             GtkPrintContext   *context,
             GrRecipePrinter   *printer)
{
        double width, height;
        PangoFontDescription *title_font;
        PangoFontDescription *body_font;
        PangoAttrList *attrs;
        PangoAttribute *attr;
        int length;
        int i, j;
        double page_height;
        GList *page_breaks;
        g_autoptr(GString) s = NULL;
        GPtrArray *images;
        PangoRectangle title_rect;
        PangoRectangle left_rect;
        int num_lines;
        int line;
        g_autoptr(GrIngredientsList) ingredients = NULL;
        PangoTabArray *tabs;
        g_autofree char **segs = NULL;
        g_auto(GStrv) ings = NULL;
        g_autofree char *instructions = NULL;
        g_autoptr(GrChef) chef = NULL;
        GrRecipeStore *store;
        PangoLayout *layout;
        int amount_width;
        const char *value;
        g_autofree char *amount = NULL;

        store = gr_recipe_store_get ();
        chef = gr_recipe_store_get_chef (store, gr_recipe_get_author (printer->recipe));

        width = gtk_print_context_get_width (context);
        height = gtk_print_context_get_height (context);

        images = gr_recipe_get_images (printer->recipe);
        if (images && images->len > 0) {
                int def_index = gr_recipe_get_default_image (printer->recipe);
                GrImage *ri = g_ptr_array_index (images, def_index);
                printer->image = gr_image_load_sync (ri, width / 2, height / 4, TRUE);
        }

        title_font = pango_font_description_from_string ("Cantarell Bold 18");
        body_font = pango_font_description_from_string ("Cantarell 12");

        printer->title_layout = gtk_print_context_create_pango_layout (context);
        pango_layout_set_width (printer->title_layout, width * PANGO_SCALE);
        pango_layout_set_font_description (printer->title_layout, title_font);

        s = g_string_new ("");
        g_string_append (s, gr_recipe_get_translated_name (printer->recipe));
        g_string_append (s, "\n\n");

        pango_layout_set_text (printer->title_layout, s->str, s->len);

        printer->left_layout = gtk_print_context_create_pango_layout (context);
        pango_layout_set_width (printer->left_layout, (width / 2) * PANGO_SCALE);
        pango_layout_set_font_description (printer->left_layout, body_font);

        g_string_truncate (s, 0);

        g_string_append_printf (s, "%s %s\n", _("Author:"), gr_chef_get_fullname (chef));
        g_string_append_printf (s, "%s %s\n", _("Preparation:"), gr_recipe_get_prep_time (printer->recipe));
        g_string_append_printf (s, "%s %s\n", _("Cooking:"), gr_recipe_get_cook_time (printer->recipe));
        amount = gr_number_format (gr_recipe_get_yield (printer->recipe));
        g_string_append_printf (s, "%s %s %s\n", _("Yield:"), amount, gr_recipe_get_yield_unit (printer->recipe));
        value = gr_recipe_get_cuisine (printer->recipe);
        if (value && *value) {
                const char *title;
                gr_cuisine_get_data (value, &title, NULL, NULL);
                g_string_append_printf (s, "%s %s\n", _("Cuisine:"), title);
        }
        value = gr_recipe_get_category (printer->recipe);
        if (value && *value) {
                g_string_append_printf (s, "%s %s\n", _("Meal:"), gr_meal_get_title (value));
        }
        value = gr_recipe_get_season (printer->recipe);
        if (value && *value) {
                g_string_append_printf (s, "%s %s\n", _("Season:"), gr_season_get_title (value));
        }

        g_string_append (s, "\n");

        pango_layout_set_text (printer->left_layout, s->str, s->len);

        g_string_truncate (s, 0);

        ingredients = gr_ingredients_list_new (gr_recipe_get_ingredients (printer->recipe));
        segs = gr_ingredients_list_get_segments (ingredients);

        layout = gtk_print_context_create_pango_layout (context);
        pango_layout_set_width (layout, width * PANGO_SCALE);
        pango_layout_set_font_description (layout, body_font);

        for (j = 0; segs[j]; j++) {
                ings = gr_ingredients_list_get_ingredients (ingredients, segs[j]);
                for (i = 0; ings[i]; i++) {
                        double amount;
                        GrUnit unit;
                        double scale = 1.0;

                        unit = gr_ingredients_list_get_unit (ingredients, segs[j], ings[i]);
                        amount = gr_ingredients_list_get_amount (ingredients, segs[j], ings[i]) * scale;
                        gr_convert_format (s, amount, unit);
                }
        }

        pango_layout_set_text (layout, s->str, s->len);
        pango_layout_get_size (layout, &amount_width, NULL);
        g_clear_object (&layout);

        g_string_truncate (s, 0);

        printer->bottom_layout = gtk_print_context_create_pango_layout (context);
        pango_layout_set_width (printer->bottom_layout, width * PANGO_SCALE);
        pango_layout_set_font_description (printer->bottom_layout, body_font);

        g_string_append (s, gr_recipe_get_translated_description (printer->recipe));
        g_string_append (s, "\n\n");

        attrs = pango_attr_list_new ();

        value = gr_recipe_get_notes (printer->recipe);
        if (value && *value) {
                attr = pango_attr_font_desc_new (title_font);
                attr->start_index = s->len;
                g_string_append (s, _("Notes"));
                attr->end_index = s->len + 1;
                pango_attr_list_insert (attrs, attr);

                g_string_append (s, "\n");
                g_string_append (s, value);
                g_string_append (s, "\n\n");
        }

        tabs = pango_tab_array_new (2, FALSE);
        pango_tab_array_set_tab (tabs, 0, PANGO_TAB_LEFT, 0);
        pango_tab_array_set_tab (tabs, 1, PANGO_TAB_LEFT, amount_width);
        pango_tab_array_set_tab (tabs, 2, PANGO_TAB_LEFT, (width / 2) * PANGO_SCALE);
        pango_tab_array_set_tab (tabs, 3, PANGO_TAB_LEFT, (width / 2) * PANGO_SCALE + amount_width);
        pango_layout_set_tabs (printer->bottom_layout, tabs);
        pango_tab_array_free (tabs);

        for (j = 0; segs[j]; j++) {
                attr = pango_attr_font_desc_new (title_font);
                attr->start_index = s->len;

                if (segs[j][0] != 0)
                        g_string_append (s, g_dgettext (GETTEXT_PACKAGE "-data", segs[j]));
                else
                        g_string_append (s, _("Ingredients"));

                attr->end_index = s->len + 1;
                pango_attr_list_insert (attrs, attr);

                g_string_append (s, "\n");

                ings = gr_ingredients_list_get_ingredients (ingredients, segs[j]);
                length = g_strv_length (ings);
                if (length > 3) {
                        int mid;
                        mid = length / 2 + length % 2;
                        for (i = 0; i < mid; i++) {
                                g_autofree char *unit = NULL;

                                g_string_append (s, "\n");

                                unit = gr_ingredients_list_scale_unit (ingredients, segs[j], ings[i], 1.0);
                                g_string_append (s, unit);
                                g_clear_pointer (&unit, g_free);
                                g_string_append (s, "\t");
                                g_string_append (s, ings[i]);
                                g_string_append (s, "\t");
                                if (mid + i < length) {
                                        unit = gr_ingredients_list_scale_unit (ingredients, segs[j], ings[mid + i], 1.0);
                                        g_string_append (s, unit);
                                        g_string_append (s, "\t");
                                        g_string_append (s, ings[mid + i]);
                                }
                        }
                }
                else {
                        for (i = 0; i < length; i++) {
                                g_autofree char *unit = NULL;

                                g_string_append (s, "\n");
                                unit = gr_ingredients_list_scale_unit (ingredients, segs[j], ings[i], 1.0);
                                g_string_append (s, unit);
                                g_string_append (s, "\t");
                                g_string_append (s, ings[i]);
                        }
                }

                g_string_append (s, "\n\n");
        }

        attr = pango_attr_font_desc_new (title_font);
        attr->start_index = s->len;

        g_string_append (s, _("Directions"));

        attr->end_index = s->len + 1;
        pango_attr_list_insert (attrs, attr);

        instructions = process_instructions (gr_recipe_get_translated_instructions (printer->recipe));

        g_string_append (s, "\n\n");
        g_string_append (s, instructions);

        pango_layout_set_text (printer->bottom_layout, s->str, s->len);
        pango_layout_set_attributes (printer->bottom_layout, attrs);
        pango_attr_list_unref (attrs);

        /* TODO: we assume only the bottom layout will break across pages */

        num_lines = pango_layout_get_line_count (printer->bottom_layout);

        pango_layout_get_extents (printer->title_layout, NULL, &title_rect);
        pango_layout_get_extents (printer->left_layout, NULL, &left_rect);

        if (printer->image)
                page_height = title_rect.height/1024.0 + MAX (left_rect.height/1024.0, gdk_pixbuf_get_height (printer->image) + 10);
        else
                page_height = title_rect.height/1024.0 + left_rect.height/1024.0;

        page_breaks = NULL;

        for (line = 0; line < num_lines; line++) {
                PangoLayoutLine *layout_line;
                PangoRectangle logical_rect;
                double line_height;

                layout_line = pango_layout_get_line (printer->bottom_layout, line);
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
        g_clear_object (&printer->image);
        g_list_free (printer->page_breaks);
        printer->page_breaks = NULL;
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
        int width;
        int start, end, i;
        PangoLayoutIter *iter;
        double start_pos;
        GList *pagebreak;

        cr = gtk_print_context_get_cairo_context (context);

        width = gtk_print_context_get_width (context);

        if (page_nr == 0) {
                cairo_set_source_rgb (cr, 0, 0, 0);

                pango_layout_get_extents (printer->title_layout, NULL, &logical_rect);
                baseline = pango_layout_get_baseline (printer->title_layout);

                cairo_move_to (cr, logical_rect.x / 1024.0 + (width - logical_rect.width / 1024.0) / 2, baseline / 1024.0 - logical_rect.y / 1024.0);
                pango_cairo_show_layout (cr, printer->title_layout);

                start_pos = baseline / 1024.0 - logical_rect.y / 1024.0 + logical_rect.height / 1024.0;

                if (printer->image) {
                        gdk_cairo_set_source_pixbuf (cr, printer->image, width - gdk_pixbuf_get_width (printer->image), start_pos);
                        cairo_paint (cr);
                }

                cairo_set_source_rgb (cr, 0, 0, 0);

                pango_layout_get_extents (printer->left_layout, NULL, &logical_rect);
                baseline = pango_layout_get_baseline (printer->left_layout);

                cairo_move_to (cr, logical_rect.x / 1024.0, start_pos + baseline / 1024.0 - logical_rect.y / 1024.0);
                pango_cairo_show_layout (cr, printer->left_layout);

                if (printer->image)
                        start_pos += MAX (gdk_pixbuf_get_height (printer->image) + 10, baseline / 1024.0 - logical_rect.y / 1024.0 + logical_rect.height / 1024.0);
                else
                        start_pos += baseline / 1024.0 - logical_rect.y / 1024.0 + logical_rect.height / 1024.0;

                start = 0;
        }
        else {
                pagebreak = g_list_nth (printer->page_breaks, page_nr - 1);
                start = GPOINTER_TO_INT (pagebreak->data);
                start_pos = 0;
        }

        pagebreak = g_list_nth (printer->page_breaks, page_nr);
        if (pagebreak == NULL)
                end = pango_layout_get_line_count (printer->bottom_layout);
        else
                end = GPOINTER_TO_INT (pagebreak->data);

        i = 0;
        iter = pango_layout_get_iter (printer->bottom_layout);
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
            GrRecipePrinter         *printer)
{
       GError *error = NULL;

        if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
                GtkWidget *error_dialog;

                gtk_print_operation_get_error (operation, &error);

                error_dialog = gtk_message_dialog_new (GTK_WINDOW (printer->window),
                                                       GTK_DIALOG_MODAL |
                                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                                       GTK_MESSAGE_ERROR,
                                                       GTK_BUTTONS_CLOSE,
                                                       "%s\n%s",
                                                       _("Error printing file:"),
                                                       error ? error->message : _("No details"));
                g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
                gtk_widget_show (error_dialog);
        }
        else if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
                /* TODO: print settings */
        }

        if (!gtk_print_operation_is_finished (operation)) {
                /* TODO monitoring */
        }

        g_object_unref (operation);
}

GFile *
gr_recipe_printer_get_pdf (GrRecipePrinter *printer,
                            GrRecipe        *recipe)
{
        GtkPrintOperation *operation = NULL;
        g_autofree char *path = NULL;
        g_autofree char *name = NULL;

        name = g_strdup (gr_recipe_get_name (GR_RECIPE (recipe)));
        g_strdelimit (name, "./", ' ');
        path = g_strdup_printf ("%s/%s.pdf", get_user_data_dir (), name);

        g_set_object (&printer->recipe, recipe);
        operation = gtk_print_operation_new ();

        g_signal_connect (operation, "begin-print", G_CALLBACK (begin_print), printer);
        g_signal_connect (operation, "end-print", G_CALLBACK (end_print), printer);
        g_signal_connect (operation, "draw-page", G_CALLBACK (draw_page), printer);
        g_signal_connect (operation, "done", G_CALLBACK (print_done), printer);
        gtk_print_operation_set_allow_async (operation, TRUE);
        gtk_print_operation_set_export_filename (operation, path);

        gtk_print_operation_run (operation, GTK_PRINT_OPERATION_ACTION_EXPORT, NULL, NULL);
        return g_file_new_for_path (path);
}

void
gr_recipe_printer_print (GrRecipePrinter *printer,
                         GrRecipe        *recipe)
{
        GtkPrintOperation *operation;

        if (in_flatpak_sandbox () && !portal_available (printer->window, "org.freedesktop.portal.Print"))
                return;

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
