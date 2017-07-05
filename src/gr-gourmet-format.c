/* gr-gourmet-format.c
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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
#include "gr-gourmet-format.h"
#include "gr-recipe-store.h"
#include "gr-recipe.h"
#include "gr-utils.h"
#include "gr-image.h"
#include "gr-app.h"

static gboolean
in_element (GMarkupParseContext *context,
            ...)
{
        va_list args;
        gboolean result;
        char *next;
        const GSList *stack;

        va_start (args, context);

        result = TRUE;

        stack = g_markup_parse_context_get_element_stack (context);

        while ((next = va_arg (args, char*)) != NULL) {
                const char *elt;

                if (stack == NULL) {
                        result = FALSE;
                        break;
                }

                elt = stack->data;
                stack = stack->next;

                if (strcmp (next, elt) != 0) {
                        result = FALSE;
                        break;
                }
        }

        va_end (args);

        return result;
}

typedef struct {
        GrRecipeStore *store;
        GList *recipes;
        gboolean gourmet_doc;
        gboolean collecting_text;
        GString *text;
        char *title;
        char *source;
        char *category;
        char *instructions;
        char *amount;
        char *unit;
        char *item;
        char *ingredients;
        char *cuisine;
        char *preptime;
        char *cooktime;
        char *yields;
        char *modifications;
        char *image_data;
        GString *ingredients_list;
} ParserData;

static void
parser_data_clear (ParserData *pd)
{
        g_list_free (pd->recipes);
        g_string_free (pd->text, TRUE);
        g_free (pd->title);
        g_free (pd->source);
        g_free (pd->category);
        g_free (pd->instructions);
        g_free (pd->amount);
        g_free (pd->unit);
        g_free (pd->item);
        g_free (pd->ingredients);
        g_free (pd->cuisine);
        g_free (pd->preptime);
        g_free (pd->cooktime);
        g_free (pd->yields);
        g_free (pd->modifications);
        g_free (pd->image_data);
        g_string_free (pd->ingredients_list, TRUE);
}

static GrChef *
ensure_chef (GrRecipeStore *store,
             const char *name)
{
        g_autoptr(GrChef) chef = NULL;

        chef = gr_recipe_store_get_chef (store, name);
        if (chef == NULL) {
                g_autoptr(GError) error = NULL;
                g_autofree char *id = NULL;
                g_auto(GStrv) strv = NULL;

                strv = g_strsplit (name, " ", -1);
                if (strv[1])
                        id = generate_id (strv[0], "_", strv[1], NULL);
                else
                        id = generate_id (strv[0], NULL);

                chef = g_object_new (GR_TYPE_CHEF,
                                     "id", id,
                                     "name", name,
                                     "fullname", name,
                                     "description", "No comment",
                                     NULL);

                if (!gr_recipe_store_add_chef (store, chef, &error))
                        g_warning ("Failed to add chef %s: %s", name, error->message);
        }

        return g_object_ref (chef);
}

static void
collect_text (ParserData *pd)
{
        g_string_set_size (pd->text, 0);
        pd->collecting_text = TRUE;
}

static char *
collected_text (ParserData *pd)
{
        pd->collecting_text = FALSE;
        return g_strdup (pd->text->str);
}

static void
start_element (GMarkupParseContext  *context,
               const char           *element_name,
               const char          **attribute_names,
               const char          **attribute_values,
               gpointer              user_data,
               GError              **error)
{
        ParserData *pd = user_data;

        if (strcmp (element_name, "gourmetDoc") == 0) {
                pd->gourmet_doc = TRUE;
                return;
        }

        if (!pd->gourmet_doc) {
                g_set_error (error,
                             G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             _("Not a Gourmet XML document"));
                return;
        }

        if (in_element (context, "title", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "category", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "source", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "cuisine", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "preptime", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "cooktime", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "yields", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "modifications", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "image", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "instructions", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "amount", "ingredient", "ingredient-list", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "unit", "ingredient", "ingredient-list", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (in_element (context, "item", "ingredient", "ingredient-list", "recipe", NULL)) {
                collect_text (pd);
        }
        else if (strcmp ("ingredient-list", element_name) == 0) {
                g_string_set_size (pd->ingredients_list, 0);
        }
}

static gboolean
parse_yield (const char  *text,
             double      *amount,
             char       **unit)
{
        char *tmp;
        const char *str;
        g_autofree char *num = NULL;

        g_clear_pointer (unit, g_free);

        tmp = (char *)text;
        skip_whitespace (&tmp);
        str = tmp;
        if (!gr_number_parse (amount, &tmp, NULL)) {
                *unit = g_strdup (str);
                return FALSE;
        }

        skip_whitespace (&tmp);
        if (tmp)
                *unit = g_strdup (tmp);

        return TRUE;
}

static GPtrArray *
load_image (const char *data)
{
        GPtrArray *images;
        GrImage *ri;
        g_autofree char *dir = NULL;
        g_autofree char *path = NULL;
        g_autoptr(GError) error = NULL;
        g_autofree char *decoded = NULL;
        gsize length;

        decoded = (char *)g_base64_decode (data, &length);

        dir = g_build_filename (get_user_data_dir (), "images", NULL);
        g_mkdir_with_parents (dir, 0755);

        path = g_build_filename (dir, "importXXXXXX.jpg", NULL);
        if (!g_file_set_contents (path, decoded, length, &error)) {
                g_warning ("Failed to save image: %s", error->message);
                return NULL;
        }

        images = gr_image_array_new ();

        ri = gr_image_new (gr_app_get_soup_session (GR_APP (g_application_get_default ())), "local", path);
        g_ptr_array_add (images, ri);

        return images;
}

static void
end_element (GMarkupParseContext *context,
             const char          *element_name,
             gpointer             user_data,
             GError             **error)
{
        ParserData *pd = user_data;

        if (in_element (context, "title", "recipe", NULL)) {
                pd->title = collected_text (pd);
        }
        else if (in_element (context, "category", "recipe", NULL)) {
                pd->category = collected_text (pd);
        }
        else if (in_element (context, "source", "recipe", NULL)) {
                pd->source = collected_text (pd);
        }
        else if (in_element (context, "cuisine", "recipe", NULL)) {
                pd->cuisine = collected_text (pd);
        }
        else if (in_element (context, "preptime", "recipe", NULL)) {
                pd->preptime = collected_text (pd);
        }
        else if (in_element (context, "cooktime", "recipe", NULL)) {
                pd->cooktime = collected_text (pd);
        }
        else if (in_element (context, "yields", "recipe", NULL)) {
                pd->yields = collected_text (pd);
        }
        else if (in_element (context, "modifications", "recipe", NULL)) {
                pd->modifications = collected_text (pd);
        }
        else if (in_element (context, "image", "recipe", NULL)) {
                pd->image_data = collected_text (pd);
        }
        else if (in_element (context, "instructions", "recipe", NULL)) {
                pd->instructions = collected_text (pd);
        }
        else if (in_element (context, "amount", "ingredient", "ingredient-list", "recipe", NULL)) {
                pd->amount = collected_text (pd);
        }
        else if (in_element (context, "unit", "ingredient", "ingredient-list", "recipe", NULL)) {
                pd->unit = collected_text (pd);
        }
        else if (in_element (context, "item", "ingredient", "ingredient-list", "recipe", NULL)) {
                pd->item = collected_text (pd);
        }
        else if (strcmp ("ingredient", element_name) == 0) {
                if (pd->ingredients_list->len > 0)
                        g_string_append (pd->ingredients_list, "\n");
                g_string_append_printf (pd->ingredients_list, "%s\t%s\t%s\t",
                                        pd->amount ? pd->amount : "",
                                        pd->unit ? pd->unit : "",
                                        pd->item ? pd->item : "");
                g_clear_pointer (&pd->amount, g_free);
                g_clear_pointer (&pd->unit, g_free);
                g_clear_pointer (&pd->item, g_free);
        }
        else if (strcmp (element_name, "ingredient-list") == 0) {
                pd->ingredients = g_strdup (pd->ingredients_list->str);
                g_string_set_size (pd->ingredients_list, 0);
        }
        else if (strcmp (element_name, "recipe") == 0) {
                g_autoptr(GrRecipe) recipe = NULL;
                g_autoptr(GrChef) chef = NULL;
                g_autofree char *id = NULL;
                double yield;
                g_autofree char *yield_unit = NULL;
                g_autoptr(GPtrArray) images = NULL;

                if (!parse_yield (pd->yields, &yield, &yield_unit)) {
                        yield = 1.0;
                        yield_unit = g_strdup ("serving");
                }

                if (pd->source == NULL)
                        pd->source = g_strdup ("anonymous");

                id = generate_id ("R_", pd->title, "_by_", pd->source, NULL);
                chef = ensure_chef (pd->store, pd->source);
                images = load_image (pd->image_data);
                recipe = g_object_new (GR_TYPE_RECIPE,
                                       "id", id,
                                       "author", gr_chef_get_id (chef),
                                       "name", pd->title,
                                       "instructions", pd->instructions,
                                       "ingredients", pd->ingredients,
                                       "cuisine", pd->cuisine,
                                       "prep-time", pd->preptime,
                                       "cook-time", pd->cooktime,
                                       "notes", pd->modifications,
                                       "season", "",
                                       "category", "",
                                       "yield", yield,
                                       "yield-unit", yield_unit,
                                       "images", images,
                                       NULL);
                g_message ("Importing recipe %s", id);
                gr_recipe_store_add_recipe (pd->store, recipe, error);
                pd->recipes = g_list_prepend (pd->recipes, recipe);

                g_clear_pointer (&pd->title, g_free);
                g_clear_pointer (&pd->source, g_free);
                g_clear_pointer (&pd->category, g_free);
                g_clear_pointer (&pd->cuisine, g_free);
                g_clear_pointer (&pd->preptime, g_free);
                g_clear_pointer (&pd->cooktime, g_free);
                g_clear_pointer (&pd->modifications, g_free);
                g_clear_pointer (&pd->image_data, g_free);
                g_clear_pointer (&pd->instructions, g_free);
                g_clear_pointer (&pd->ingredients, g_free);
        }
}

static void
text (GMarkupParseContext  *context,
      const char           *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
        ParserData *pd = user_data;

        if (pd->collecting_text)
                g_string_append_len (pd->text, text, text_len);
}

GList *
gr_gourmet_format_import (GFile   *file,
                          GError **error)
{
        GMarkupParser parser = {
                start_element,
                end_element,
                text,
                NULL,
                NULL
        };
        ParserData data;
        g_autoptr(GMarkupParseContext) context = NULL;
        g_autofree char *contents = NULL;
        gsize length;
        GList *recipes;

        if (!g_file_load_contents (file, NULL, &contents, &length, NULL, error))
                return NULL;

        memset (&data, 0, sizeof(ParserData));
        data.store = gr_recipe_store_get ();
        data.gourmet_doc = FALSE;
        data.text = g_string_new ("");
        data.collecting_text = FALSE;
        data.ingredients_list = g_string_new ("");

        context = g_markup_parse_context_new (&parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &data, NULL);

        if (!g_markup_parse_context_parse (context, contents, length, error)) {
                parser_data_clear (&data);
                return NULL;
        }

        recipes = data.recipes;
        data.recipes = NULL;
        parser_data_clear (&data);

        return recipes;
}
