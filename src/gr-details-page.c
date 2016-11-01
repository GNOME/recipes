/* gr-details-page.c:
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-details-page.h"
#include "gr-recipe-store.h"
#include "gr-app.h"
#include "gr-window.h"
#include "gr-utils.h"


struct _GrDetailsPage
{
        GtkBox        parent_instance;

        GrRecipe  *recipe;
        GrChef    *chef;
        GtkWidget *recipe_image;
        GtkWidget *prep_time_label;
        GtkWidget *cook_time_label;
        GtkWidget *serves_label;
        GtkWidget *description_label;
        GtkWidget *chef_label;
        GtkWidget *chef_link;
        GtkWidget *ingredients_label;
        GtkWidget *instructions_label;
        GtkWidget *notes_label;
};

G_DEFINE_TYPE (GrDetailsPage, gr_details_page, GTK_TYPE_BOX)

static void
delete_recipe (GrDetailsPage *page)
{
        GrRecipeStore *store;
        GtkWidget *window;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
        gr_recipe_store_remove (store, page->recipe);
        g_set_object (&page->recipe, NULL);
	g_set_object (&page->chef, NULL);

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_show_main (GR_WINDOW (window));
}

static void
edit_recipe (GrDetailsPage *page)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_edit_recipe (GR_WINDOW (window), page->recipe);
}

static gboolean
more_recipes (GrDetailsPage *page)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_show_chef (GR_WINDOW (window), page->chef);

	return TRUE;
}

static void
details_page_finalize (GObject *object)
{
        GrDetailsPage *self = GR_DETAILS_PAGE (object);

        g_clear_object (&self->recipe);
        g_clear_object (&self->chef);

        G_OBJECT_CLASS (gr_details_page_parent_class)->finalize (object);
}

static void
gr_details_page_init (GrDetailsPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));
}

static void
gr_details_page_class_init (GrDetailsPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = details_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-details-page.ui");

        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, recipe_image);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, prep_time_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, cook_time_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, serves_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, description_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, chef_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, chef_link);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, ingredients_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, instructions_label);
        gtk_widget_class_bind_template_child (widget_class, GrDetailsPage, notes_label);

        gtk_widget_class_bind_template_callback (widget_class, edit_recipe);
        gtk_widget_class_bind_template_callback (widget_class, delete_recipe);
        gtk_widget_class_bind_template_callback (widget_class, more_recipes);
}

GtkWidget *
gr_details_page_new (void)
{
        GrDetailsPage *page;

        page = g_object_new (GR_TYPE_DETAILS_PAGE, NULL);

        return GTK_WIDGET (page);
}


void
gr_details_page_set_recipe (GrDetailsPage *page,
                            GrRecipe      *recipe)
{
        g_autofree char *image_path = NULL;
        g_autofree char *prep_time = NULL;
        g_autofree char *cook_time = NULL;
        int serves;
        g_autofree char *description = NULL;
        g_autofree char *author = NULL;
        g_autofree char *ingredients = NULL;
        g_autofree char *instructions = NULL;
        g_autofree char *notes = NULL;
        char *tmp;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	GrRecipeStore *store;
	g_autoptr(GrChef) chef = NULL;
	g_autofree char *author_desc = NULL;

        g_set_object (&page->recipe, recipe); 

        g_object_get (recipe,
                      "image-path", &image_path,
                      "prep-time", &prep_time,
                      "cook-time", &cook_time,
                      "serves", &serves,
                      "description", &description,
                      "author", &author,
		      "ingredients", &ingredients,
		      "instructions", &instructions,
                      "notes", &notes,
                      NULL);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));
	chef = gr_recipe_store_get_chef (store, author);

	g_set_object (&page->chef, chef);

	pixbuf = load_pixbuf_at_size (image_path, 256, 256);
	gtk_image_set_from_pixbuf (GTK_IMAGE (page->recipe_image), pixbuf);
        gtk_label_set_label (GTK_LABEL (page->prep_time_label), prep_time);
        gtk_label_set_label (GTK_LABEL (page->cook_time_label), cook_time);
        gtk_label_set_label (GTK_LABEL (page->ingredients_label), ingredients);
        gtk_label_set_label (GTK_LABEL (page->instructions_label), instructions);
        gtk_label_set_label (GTK_LABEL (page->notes_label), notes);

        tmp = g_strdup_printf ("%d", serves);
        gtk_label_set_label (GTK_LABEL (page->serves_label), tmp);
        g_free (tmp);

        gtk_label_set_label (GTK_LABEL (page->description_label), description);

	if (page->chef)
		g_object_get (page->chef, "description", &author_desc, NULL);

	if (author_desc)
        	tmp = g_strdup_printf ("About GNOME chef %s:\n%s", author, author_desc);
	else
        	tmp = g_strdup_printf ("A recipe by GNOME chef %s", author);
        gtk_label_set_label (GTK_LABEL (page->chef_label), tmp);
	g_free (tmp);

        tmp = g_strdup_printf ("More recipes by %s", author);
        gtk_button_set_label (GTK_BUTTON (page->chef_link), tmp);
        g_free (tmp);
}
