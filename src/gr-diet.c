/* gr-diet.c:
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

#include "gr-diet.h"

const char *
gr_diet_get_label (GrDiets diet)
{
        const char *label;

        switch (diet) {
        case GR_DIET_GLUTEN_FREE:
                label = _("Gluten-free recipes");
                break;
        case GR_DIET_NUT_FREE:
                label = _("Nut-free recipes");
                break;
        case GR_DIET_VEGAN:
                label = _("Vegan recipes");
                break;
        case GR_DIET_VEGETARIAN:
                label = _("Vegetarian recipes");
                break;
        case GR_DIET_MILK_FREE:
                label = _("Milk-free recipes");
                break;
        case GR_DIET_HALAL:
                label = _("Halal recipes");
                break;
        default:
                label = _("Other dietary restrictions");
                break;
        }

        return label;
}

const char *
gr_diet_get_description (GrDiets diet)
{
        const char *label;

        switch (diet) {
        case GR_DIET_GLUTEN_FREE:
                label = _("A gluten-free diet is a diet that excludes gluten, a protein composite found in wheat, barley, rye, and all their species and hybrids (such as spelt, kamut, and triticale). The inclusion of oats in a gluten-free diet remains controversial. Avenin present in oats may also be toxic for coeliac people; its toxicity depends on the cultivar consumed. Furthermore, oats are frequently cross-contaminated with cereals containing gluten.\n<a href=\"https://en.wikibooks.org/wiki/Cookbook:Gluten-Free\">Learn more…</a>");
                break;
        case GR_DIET_NUT_FREE:
                label = _("A tree nut allergy is a hypersensitivity to dietary substances from tree nuts and edible tree seeds causing an overreaction of the immune system which may lead to severe physical symptoms. Tree nuts include, but are not limited to, almonds, Brazil nuts, cashews, chestnuts, filberts/hazelnuts, macadamia nuts, pecans, pistachios, pine nuts, shea nuts and walnuts.\n<a href=\"https://en.wikipedia.org/wiki/Tree_nut_allergy\">Learn more…</a>");
                break;
        case GR_DIET_VEGAN:
                label = _("Veganism is both the practice of abstaining from the use of animal products, particularly in diet, and an associated philosophy that rejects the commodity status of animals.\n<a href=\"https://en.wikipedia.org/wiki/Veganism\">Learn more…</a>");
                break;
        case GR_DIET_VEGETARIAN:
                label = _("Vegetarian cuisine is based on food that meets vegetarian standards by not including meat and animal tissue products (such as gelatin or animal-derived rennet). For lacto-ovo vegetarianism (the most common type of vegetarianism in the Western world), eggs and dairy products such as milk and cheese are permitted. For lacto vegetarianism, the earliest known type of vegetarianism (recorded in India), dairy products such as milk and cheese are permitted.\nThe strictest forms of vegetarianism are veganism and fruitarianism, which exclude all animal products, including dairy products as well as honey, and even some refined sugars if filtered and whitened with bone char.\n<a href=\"https://en.wikipedia.org/wiki/Vegetarianism\">Learn more…</a>");
                break;
        case GR_DIET_MILK_FREE:
                label = _("Lactose intolerance is a condition in which people have symptoms due to the decreased ability to digest lactose, a sugar found in milk products. Those affected vary in the amount of lactose they can tolerate before symptoms develop. Symptoms may include abdominal pain, bloating, diarrhea, gas, and nausea. These typically start between half and two hours after drinking milk. Severity depends on the amount a person eats or drinks. It does not cause damage to the gastrointestinal tract.\n<a href=\"https://en.wikipedia.org/wiki/Lactose_intolerance\">Learn more…</a>");
                break;
        case GR_DIET_HALAL:
                label = _("Halal meat is prepared according to Islamic traditions");
                break;
        default:
                label = _("Other dietary restrictions");
                break;
        }

        return label;
}
