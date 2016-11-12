/* gr-ingredient.c:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "gr-ingredient.h"
#include "gr-utils.h"


static const char *names_[] = {
        N_("Almond"),
        N_("Amaretti"),
        N_("Apple"),
        N_("Apricot"),
        N_("Anchovis"),
        N_("Artichoke"),
        N_("Asparagus"),
        N_("Aubergine"),
        N_("Bacon"),
        N_("Banana"),
        N_("Baked Beans"),
        N_("Basil"),
        N_("Beans"),
        N_("Bagel"),
        N_("Basmati rice"),
        N_("Bay leaf"),
        N_("Beef mince"),
        N_("Berry"),
        N_("Beetroot"),
        N_("Biscotti"),
        N_("Beef sausage"),
        N_("Beef stock"),
        N_("Bilberry"),
        N_("Carrot"),
        N_("Couscous"),
        N_("Date"),
        N_("Egg"),
        N_("Fig"),
        N_("Garlic"),
        N_("Honey"),
        N_("Lemon"),
        N_("Mayonnaise"),
        N_("Mustard"),
        N_("Onion"),
        N_("Orange"),
        N_("Parsley"),
        N_("Pepper"),
        N_("Potato"),
        N_("Silantro"),
        N_("Squash"),
        N_("Tangerine"),
        N_("Tomato"),
        N_("Vinegar"),
        N_("Wine"),
        N_("Yoghurt"),
        N_("Zinfandel"),
        NULL
};

static const char *plurals[] = {
        N_("Almonds"),
        N_("Amaretti"),
        N_("Apples"),
        N_("Apricots"),
        N_("Anchovis"),
        N_("Artichokes"),
        N_("Asparagus"),
        N_("Aubergines"),
        N_("Bacon"),
        N_("Bananas"),
        N_("Baked Beans"),
        N_("Basil"),
        N_("Beans"),
        N_("Bagels"),
        N_("Basmati rice"),
        N_("Bay leaves"),
        N_("Beef mince"),
        N_("Berries"),
        N_("Beetroots"),
        N_("Biscottis"),
        N_("Beef sausages"),
        N_("Beef stock"),
        N_("Bilberries"),
        N_("Carrots"),
        N_("Couscous"),
        N_("Dates"),
        N_("Eggs"),
        N_("Figs"),
        N_("Garlic"),
        N_("Honey"),
        N_("Lemons"),
        N_("Mayonnaise"),
        N_("Mustard"),
        N_("Onions"),
        N_("Oranges"),
        N_("Parsley"),
        N_("Peppers"),
        N_("Potatoes"),
        N_("Silantro"),
        N_("Squashs"),
        N_("Tangerines"),
        N_("Tomatoes"),
        N_("Vinegar"),
        N_("Wines"),
        N_("Yoghurts"),
        N_("Zinfandels"),
        NULL
};

static const char *negations[] = {
        N_("no Almond"),
        N_("no Amaretti"),
        N_("no Apple"),
        N_("no Apricot"),
        N_("no Anchovis"),
        N_("no Artichoke"),
        N_("no Asparagus"),
        N_("no Aubergine"),
        N_("no Bacon"),
        N_("no Banana"),
        N_("no Baked Beans"),
        N_("no Basil"),
        N_("no Beans"),
        N_("no Bagel"),
        N_("no Basmati rice"),
        N_("no Bay leaf"),
        N_("no Beef mince"),
        N_("no Berry"),
        N_("no Beetroot"),
        N_("no Biscotti"),
        N_("no Beef sausage"),
        N_("no Beef stock"),
        N_("no Bilberry"),
        N_("no Carrot"),
        N_("no Couscous"),
        N_("no Date"),
        N_("no Egg"),
        N_("no Fig"),
        N_("no Garlic"),
        N_("no Honey"),
        N_("no Lemon"),
        N_("no Mayonnaise"),
        N_("no Mustard"),
        N_("no Onion"),
        N_("no Orange"),
        N_("no Parsley"),
        N_("no Pepper"),
        N_("no Potatoe"),
        N_("no Silantro"),
        N_("no Squash"),
        N_("no Tangerine"),
        N_("no Tomato"),
        N_("no Vinegar"),
        N_("no Wine"),
        N_("no Yoghurt"),
        N_("no Zinfandel"),
        NULL
};

static char **names;
static char **cf_names;
static char **cf_plurals;

static void
translate_names (void)
{
        int i;

        if (names)
                return;

        names = g_new0 (char *, G_N_ELEMENTS (names_));
        cf_names = g_new0 (char *, G_N_ELEMENTS (names_));
        cf_plurals = g_new0 (char *, G_N_ELEMENTS (names_));

        for (i = 0; names_[i]; i++) {
                names[i] = _(names_[i]);
                cf_names[i] = g_utf8_casefold (names[i], -1);
                cf_plurals[i] = g_utf8_casefold (_(plurals[i]), -1);
        }
}

const char **
gr_ingredient_get_names (int *length)
{
        translate_names ();

        if (length)
                *length = G_N_ELEMENTS (names) - 1;

        return (const char **)names;
}

const char *
gr_ingredient_find (const char *text)
{
        int i;
        g_autofree char *cf_text = NULL;

        translate_names ();

        cf_text = g_utf8_casefold (text, -1);

        for (i = 0; names[i]; i++) {
                if (strstr (cf_text, cf_names[i]) != NULL ||
                    strstr (cf_text, cf_plurals[i]) != NULL) {
                        return names[i];
                }
        }

        return NULL;
}

const char *
gr_ingredient_get_plural (const char *name)
{
        int i;

        for (i = 0; names[i]; i++) {
                if (g_strcmp0 (name, names[i]) == 0) {
                        return _(plurals[i]);
                }
        }

        return NULL;
}

const char *
gr_ingredient_get_negation (const char *name)
{
        int i;

        for (i = 0; names[i]; i++) {
                if (g_strcmp0 (name, names[i]) == 0) {
                        return _(negations[i]);
                }
        }

        return NULL;
}

char *
gr_ingredient_get_image (const char *name)
{
        int i;
        g_autofree char *filename;

        filename = g_strconcat (name, ".png", NULL);

        return g_build_filename (get_pkg_data_dir (), "ingredients", filename, NULL);
}
