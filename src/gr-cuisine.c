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

#include "gr-utils.h"
#include "gr-cuisine.h"


static const char *names[] = {
        "american",
        "chinese",
        "indian",
        "italian",
        "french",
        "greek",
        "mediterranean",
        "mexican",
        "nordic",
        "turkish",
        NULL
};

static const char *titles[] = {
        N_("American"),
        N_("Chinese"),
        N_("Indian"),
        N_("Italian"),
        N_("French"),
        N_("Greek"),
        N_("Mediterranean"),
        N_("Mexican"),
        N_("Nordic"),
        N_("Turkish"),
};

static const char *full_titles[] = {
        N_("American Cuisine"),
        N_("Chinese Cuisine"),
        N_("Indian Cuisine"),
        N_("Italian Cuisine"),
        N_("French Cuisine"),
        N_("Greek Cuisine"),
        N_("Mediterranean Cuisine"),
        N_("Mexican Cuisine"),
        N_("Nordic Cuisine"),
        N_("Turkish Cuisine"),
};

static const char *descriptions[] = {
        N_("The cuisine of the United States reflects its history. The European colonization of the "
           "Americas yielded the introduction of a number of ingredients and cooking styles to the "
           "latter. The various styles continued expanding well into the 19th and 20th centuries, "
           "proportional to the influx of immigrants from many foreign nations; such influx developed "
           "a rich diversity in food preparation throughout the country."),
        N_("Chinese cuisine includes styles originating from the diverse regions of China, as well as "
           "from Chinese people in other parts of the world including most Asian nations. The history "
           "of Chinese cuisine in China stretches back for thousands of years and has changed from "
           "period to period and in each region according to climate, imperial fashions, and local "
           "preferences. Over time, techniques and ingredients from the cuisines of other cultures "
           "were integrated into the cuisine of the Chinese people due both to imperial expansion and "
           "from the trade with nearby regions in pre-modern times, and from Europe and the New World "
           "in the modern period."),
        N_("Indian cuisine encompasses a wide variety of regional and traditional cuisines native to "
           "India. Given the range of diversity in soil type, climate, culture, ethnic group and "
           "occupations, these cuisines vary significantly from each other and use locally available "
           "spices, herbs, vegetables and fruits. Indian food is also heavily influenced by religious "
           "and cultural choices and traditions. There has also been Middle Eastern and Central Asian "
           "influence on North Indian cuisine from the years of Mughal rule. Indian cuisine has been "
           "and is still evolving, as a result of the nation’s cultural interactions with other "
           "societies."),
        N_("Italian cuisine is characterized by its simplicity, with many dishes having only four to "
           "eight ingredients. Italian cooks rely chiefly on the quality of the ingredients rather "
           "than on elaborate preparation. Ingredients and dishes vary by region. Many dishes that "
           "were once regional, however, have proliferated with variations throughout the country."),
        N_("French cuisine was codified in the 20th century by Auguste Escoffier to become the modern "
           "haute cuisine; Escoffier, however, left out much of the local culinary character to be "
           "found in the regions of France and was considered difficult to execute by home cooks. "
           "Gastro-tourism and the Guide Michelin helped to acquaint people with the rich bourgeois "
           "and peasant cuisine of the French countryside starting in the 20th century. Gascon cuisine "
           "has also had great influence over the cuisine in the southwest of France. Many dishes that "
           "were once regional have proliferated in variations across the country."),
        N_("Contemporary Greek cookery makes wide use of vegetables, olive oil, grains, fish, wine, "
           "and meat (white and red, including lamb, poultry, rabbit and pork). Other important "
           "ingredients include olives, cheese, eggplant (aubergine), zucchini (courgette), lemon "
           "juice, vegetables, herbs, bread and yoghurt. The most commonly used grain is wheat; barley "
           "is also used. Common dessert ingredients include nuts, honey, fruits, and filo pastry."),
        N_("Mediterranean cuisine has a lot to offer, and is legendary for being very healthy too. Expect to see olives, yoghurt and garlic."),
        N_("Mexican cuisine is primarily a fusion of indigenous Mesoamerican cooking with European, "
           "especially Spanish, elements added after the Spanish conquest of the Aztec Empire in the "
           "16th century. The staples are native foods, such as corn, beans, avocados, tomatoes, and "
           "chili peppers, along with rice, which was brought by the Spanish. Europeans introduced a "
           "large number of other foods, the most important of which were meats from domesticated "
           "animals (beef, pork, chicken, goat, and sheep), dairy products (especially cheese), and "
           "various herbs and spices."),
        N_("Cooking in Denmark has always been inspired by foreign and continental practises and the use of imported tropical spices like cinnamon, cardamom, nutmeg and black pepper can be traced to the Danish cuisine of the Middle Ages and some even to the Vikings."),
        N_("You can find a great variety of mouth watering dishes in Turkish cuisine, which is mostly "
           "the heritage of Ottoman cuisine. It is the mixture and refinement of Central Asian, Middle "
           "Eastern and Balkan cuisines. Therefore it is impossible to fit Turkish cuisine into a short "
           "list."),
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
                *title = name;
        if (description)
                *description = NULL;
}

char *
gr_cuisine_get_css (const char *import_url)
{
        g_autoptr(GFile) file = NULL;
        const char *path;
        g_autofree char *css = NULL;
        GString *s = NULL;
        char *p, *q;

        path = "resource:///org/gnome/Recipes/cuisine.css";
        file = g_file_new_for_uri (path);

        g_file_load_contents (file, NULL, &css, NULL, NULL, NULL);

        s = g_string_new ("");

        g_string_append (s, "@import url(\"resource:///org/gnome/Recipes/category.css\");\n");

        g_string_append_printf (s, "@import url(\"%s\");\n", import_url);

        p = css;
        while (TRUE) {
                q = strstr (p, "@pkgdatadir@");
                if (!q) {
                        g_string_append (s, p);
                        break;
                }
                g_string_append_len (s, p, q - p);
                g_string_append (s, get_pkg_data_dir ());
                p = q + strlen ("@pkgdatadir@");
        }

        return g_string_free (s, FALSE);
}
