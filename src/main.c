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
#include <locale.h>
#include <libgd/gd.h>

#include "gr-app.h"

int
main (int argc, char *argv[])
{
        g_autoptr (GApplication) app = NULL;
        int status;

        gd_ensure_types ();

        setlocale (LC_ALL, "");
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        bindtextdomain (GETTEXT_PACKAGE "-data", LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE "-data", "UTF-8");

        app = G_APPLICATION (gr_app_new ());

        status = g_application_run (app, argc, argv);

        return status;
}
