/* gr-logging.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gr-logging.h>
#include <gr-utils.h>

/* verbose_logging turns enables DEBUG and INFO messages just for our log domain.
 * We also respect the G_MESSAGES_DEBUG environment variable.
 */

static gboolean verbose_logging;

void
gr_set_verbose_logging (gboolean verbose)
{
        verbose_logging = verbose;

        g_debug ("org.gnome.Recipes %s", get_version ());
}


#define DEFAULT_LEVELS (G_LOG_LEVEL_ERROR |     \
                        G_LOG_LEVEL_CRITICAL |  \
                        G_LOG_LEVEL_WARNING |   \
                        G_LOG_LEVEL_MESSAGE)
#define INFO_LEVELS    (G_LOG_LEVEL_INFO |      \
                        G_LOG_LEVEL_DEBUG)

GLogWriterOutput
gr_log_writer (GLogLevelFlags   log_level,
               const GLogField *fields,
               gsize            n_fields,
               gpointer         user_data)
{
        if (!(log_level & DEFAULT_LEVELS)) {
                const gchar *domains, *log_domain = NULL;
                gsize i;

                domains = g_getenv ("G_MESSAGES_DEBUG");

                if (verbose_logging && domains == NULL)
                        domains = G_LOG_DOMAIN;

                if ((log_level & INFO_LEVELS) == 0 || domains == NULL)
                        return G_LOG_WRITER_HANDLED;

                for (i = 0; i < n_fields; i++) {
                        if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0) {
                                log_domain = fields[i].value;
                                break;
                        }
                }

                if (!verbose_logging || g_strcmp0 (log_domain, G_LOG_DOMAIN) != 0) {
                        if (domains != NULL &&
                            strcmp (domains, "all") != 0 &&
                            (log_domain == NULL || !strstr (domains, log_domain)))
                                return G_LOG_WRITER_HANDLED;
                }
        }

        if (g_log_writer_is_journald (fileno (stderr)) &&
            g_log_writer_journald (log_level, fields, n_fields, user_data) == G_LOG_WRITER_HANDLED)
                goto handled;

        if (g_log_writer_standard_streams (log_level, fields, n_fields, user_data) == G_LOG_WRITER_HANDLED)
                goto handled;

        return G_LOG_WRITER_UNHANDLED;

handled:
        if (log_level & G_LOG_LEVEL_ERROR)
                g_abort ();

        return G_LOG_WRITER_HANDLED;
}
