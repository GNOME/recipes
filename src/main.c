/* main.c
 *
 * Copyright (C) 2016 Matthias Clasen
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

#include <gtk/gtk.h>
#include "gr-app.h"
#include "gs-hiding-box.h"
#include "gr-recipes-page.h"
#include "gr-details-page.h"
#include "gr-edit-page.h"
#include "gr-list-page.h"

int main (int argc, char *argv[])
{
  g_autoptr(GApplication) app = NULL;
  int status;

  g_type_ensure (GS_TYPE_HIDING_BOX);
  g_type_ensure (GR_TYPE_RECIPES_PAGE);
  g_type_ensure (GR_TYPE_DETAILS_PAGE);
  g_type_ensure (GR_TYPE_EDIT_PAGE);
  g_type_ensure (GR_TYPE_LIST_PAGE);

  app = G_APPLICATION (gr_app_new ());

  status = g_application_run (app, argc, argv);

  return status;
}
