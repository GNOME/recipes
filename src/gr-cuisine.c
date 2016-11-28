/* gr-cuisine.c:
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

#include "gr-cuisine.h"


static const char *names[] = {
        "american",
        "chinese",
        "indian",
        "italian",
        "french",
        "greek",
        "mexican",
        "turkish",
        "mediterranean",
        NULL
};

static const char *titles[] = {
        N_("American"),
        N_("Chinese"),
        N_("Indian"),
        N_("Italian"),
        N_("French"),
        N_("Greek"),
        N_("Mexican"),
        N_("Turkish"),
        N_("Mediterranean")
};

static const char *full_titles[] = {
        N_("American Cuisine"),
        N_("Chinese Cuisine"),
        N_("Indian Cuisine"),
        N_("Italian Cuisine"),
        N_("French Cuisine"),
        N_("Greek Cuisine"),
        N_("Mexican Cuisine"),
        N_("Turkish Cuisine"),
        N_("Mediterranean Cuisine")
};

static const char *descriptions[] = {
        N_("American cuisine has burgers"),
        N_("Good stuff"),
        N_("Spice stuff"),
        N_("Pizza, pasta, pesto - we love it all. Top it off with an esspresso and a gelato. Perfect!"),
        N_("Yep"),
        N_("Yep"),
        N_("Tacos"),
        N_("Yummy"),
        N_("The mediterranean quisine has a lot to offer, and is legendary for being very healthy too. Expect to see olives, yoghurt and garlic."),
};

const char **
gr_cuisine_get_names (int *length)
{
        if (length)
                *length = G_N_ELEMENTS(names) - 1;

        return names;
}

void
gr_cuisine_get_data (const char  *name,
                     const char **title,
                     const char **full_title,
                     const char **description)
{
        int i;

        for (i = 0; i < G_N_ELEMENTS (names); i++) {
                if (g_strcmp0 (name, names[i]) == 0) {
                        if (title)
                                *title = _(titles[i]);
                        if (full_title)
                                *full_title = _(full_titles[i]);
                        if (description)
                                *description = _(descriptions[i]);
                        return;
                }
        }

        if (title)
                title = NULL;
        if (description)
                description = NULL;
}

char *
gr_cuisine_get_css (void)
{
        GString *s;
        int i;

        s = g_string_new ("button.tile.cuisine {\n"
                          "  padding: 0;\n"
                          "  border-radius: 6px;\n"
                          "}\n"
                          "label.cuisine.heading {\n"
                          "  font: 20px Cantarell;\n"
                          "  color: white;\n"
                          "  background: rgb(50,50,50);\n"
                          "  padding: 6px;\n"
                          "}\n");

        for (i = 0; names[i]; i++) {
                g_string_append_printf (s, "button.cuisine.%s {\n"
                                           "  background: url('%s');\n"
                                           "  background-repeat: no-repeat;\n"
                                           "  background-size: 100%%;\n"
                                           "}\n", names[i], "resource:/org/gnome/Recipes/italian.png");
                g_string_append_printf (s, "button.big.cuisine.%s {\n"
                                           "  background: url('%s');\n"
                                           "  background-repeat: no-repeat;\n"
                                           "  background-size: 100%%;\n"
                                           "}\n", names[i], "resource:/org/gnome/Recipes/big-italian.png");
        }

        return g_string_free (s, FALSE);
}
