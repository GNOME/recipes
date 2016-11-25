/* main.c:
 *
 * Copyright (C) 2016 Matthias Clasen
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

#include <gtk/gtk.h>
#include <libintl.h>

#include "gr-app.h"
#include "gr-cuisine-page.h"
#include "gr-cuisines-page.h"
#include "gr-details-page.h"
#include "gr-edit-page.h"
#include "gr-ingredients-page.h"
#include "gr-ingredients-search-page.h"
#include "gr-list-page.h"
#include "gr-recipes-page.h"
#include "gr-search-page.h"
#include "gr-timer-widget.h"
#include "gr-toggle-button.h"
#include "gr-images.h"


int
main (int argc, char *argv[])
{
        g_autoptr (GApplication) app = NULL;
        int status;

        g_type_ensure (GR_TYPE_CUISINE_PAGE);
        g_type_ensure (GR_TYPE_CUISINES_PAGE);
        g_type_ensure (GR_TYPE_DETAILS_PAGE);
        g_type_ensure (GR_TYPE_EDIT_PAGE);
        g_type_ensure (GR_TYPE_IMAGES);
        g_type_ensure (GR_TYPE_INGREDIENTS_PAGE);
        g_type_ensure (GR_TYPE_INGREDIENTS_SEARCH_PAGE);
        g_type_ensure (GR_TYPE_LIST_PAGE);
        g_type_ensure (GR_TYPE_SEARCH_PAGE);
        g_type_ensure (GR_TYPE_RECIPES_PAGE);
        g_type_ensure (GR_TYPE_TIMER_WIDGET);
        g_type_ensure (GR_TYPE_TOGGLE_BUTTON);

        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        app = G_APPLICATION (gr_app_new ());

        status = g_application_run (app, argc, argv);

        return status;
}
