/* gr-chef.h
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GR_TYPE_CHEF (gr_chef_get_type())

G_DECLARE_DERIVABLE_TYPE (GrChef, gr_chef, GR, CHEF, GObject)

struct _GrChefClass
{
	GObjectClass parent;
};

GrChef     *gr_chef_new             (void);
const char *gr_chef_get_name        (GrChef *chef);
const char *gr_chef_get_fullname    (GrChef *chef);
const char *gr_chef_get_description (GrChef *chef);
const char *gr_chef_get_image       (GrChef *chef);

G_END_DECLS
