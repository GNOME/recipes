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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "gr-ingredient.h"
#include "gr-utils.h"


static const char *names_[] = {
#include "ingredients.inc"
        NULL
};

static const char *negations[] = {
#include "no-ingredients.inc"
        NULL
};

static char **names;
static char **cf_names;
static char **cf_en_names;

static void
translate_names (void)
{
        int i;

        if (names)
                return;

        names = g_new0 (char *, G_N_ELEMENTS (names_));
        cf_names = g_new0 (char *, G_N_ELEMENTS (names_));
        cf_en_names = g_new0 (char *, G_N_ELEMENTS (names_));

        for (i = 0; names_[i]; i++) {
                names[i] = _(names_[i]);
                cf_names[i] = g_utf8_casefold (names[i], -1);
                cf_en_names[i] = g_utf8_casefold (names_[i], -1);
        }
}

const char **
gr_ingredient_get_names (int *length)
{
        translate_names ();

        if (length)
                *length = G_N_ELEMENTS (names_) - 1;

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
                if (strcmp (cf_text, cf_names[i]) == 0 ||
                    strcmp (cf_text, cf_en_names[i]) == 0)
                        return names[i];
        }

        return NULL;
}

const char *
gr_ingredient_get_id (const char *name)
{
        int i;
        g_autofree char *cf_text = NULL;

        translate_names ();

        cf_text = g_utf8_casefold (name, -1);

        for (i = 0; names[i]; i++) {
                if (strcmp (cf_text, cf_names[i]) == 0 ||
                    strcmp (cf_text, cf_en_names[i]) == 0)
                        return names_[i];
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
