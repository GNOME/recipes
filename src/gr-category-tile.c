/* gr-category-tile.c:
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-category-tile.h"
#include "gr-window.h"
#include "gr-utils.h"


struct _GrCategoryTile
{
        GtkButton parent_instance;

	GrDiets diet;
        char *category;

        GtkWidget *label;
        GtkWidget *image;
};

G_DEFINE_TYPE (GrCategoryTile, gr_category_tile, GTK_TYPE_BUTTON)

static const char *
get_category_title (GrDiets diet)
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
	default:
		label = _("Other dietary restrictions");
		break;
	}

        return label;
}

const char *
gr_diet_get_label (GrDiets diet)
{
        return get_category_title (diet);
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
	default:
		label = _("Other dietary restrictions");
		break;
	}

        return label;
}

static const char *colors[] = {
        "#215d9c",
        "#297bcc",
        "#29cc5d",
        "#c4a000",
        "#75505b",
        "#cc0000",
        "#4e9a06",
        "#9c29ca",
        "#729fcf",
        "#ac5500",
        "#2944cc",
        "#44cc29"
};

static const char *
get_category_color (GrDiets diets)
{
	switch (diets) {
        case GR_DIET_GLUTEN_FREE: return colors[0];
	case GR_DIET_NUT_FREE:    return colors[1];
	case GR_DIET_VEGAN:       return colors[2];
	case GR_DIET_VEGETARIAN:  return colors[3];
	case GR_DIET_MILK_FREE:   return colors[4];
	default:                  return colors[5];
	}
}

static const char *
get_category_color_for_label (const char *label)
{
        return colors[g_str_hash (label) % G_N_ELEMENTS (colors)];
}

static void
category_tile_set_category (GrCategoryTile *tile,
                            GrDiets         diet)
{
	tile->diet = diet;
        gtk_label_set_label (GTK_LABEL (tile->label), get_category_title (diet));
}

static void
category_tile_finalize (GObject *object)
{
        GrCategoryTile *tile = GR_CATEGORY_TILE (object);

        g_free (tile->category);

        G_OBJECT_CLASS (gr_category_tile_parent_class)->finalize (object);
}

static void
gr_category_tile_init (GrCategoryTile *tile)
{
        gtk_widget_set_has_window (GTK_WIDGET (tile), FALSE);
        gtk_widget_init_template (GTK_WIDGET (tile));
        tile->diet = 0;
}

static void
gr_category_tile_class_init (GrCategoryTileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = category_tile_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-category-tile.ui");

        gtk_widget_class_bind_template_child (widget_class, GrCategoryTile, label);
        gtk_widget_class_bind_template_child (widget_class, GrCategoryTile, image);
}

GtkWidget *
gr_category_tile_new (GrDiets diet)
{
        GrCategoryTile *tile;
        g_autofree char *css;

        tile = g_object_new (GR_TYPE_CATEGORY_TILE, NULL);
        category_tile_set_category (tile, diet);

        css = g_strdup_printf ("border-bottom: 3px solid %s", get_category_color (diet));
        gr_utils_widget_set_css_simple (GTK_WIDGET (tile), css);

        return GTK_WIDGET (tile);
}

GtkWidget *
gr_category_tile_new_with_label (const char *category,
                                 const char *label)
{
        GrCategoryTile *tile;
        g_autofree char *css;

        tile = g_object_new (GR_TYPE_CATEGORY_TILE, NULL);
        gtk_label_set_label (GTK_LABEL (tile->label), label);
        tile->category = g_strdup (category);

        css = g_strdup_printf ("border-bottom: 3px solid %s", get_category_color_for_label (label));
        gr_utils_widget_set_css_simple (GTK_WIDGET (tile), css);

        return GTK_WIDGET (tile);
}

GrDiets
gr_category_tile_get_diet (GrCategoryTile *tile)
{
        return tile->diet;
}

const char *
gr_category_tile_get_category (GrCategoryTile *tile)
{
        return tile->category;
}

const char *
gr_category_tile_get_label (GrCategoryTile *tile)
{
        return gtk_label_get_label (GTK_LABEL (tile->label));
}
