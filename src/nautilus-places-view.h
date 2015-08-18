/* nautilus-places-view.h
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#ifndef NAUTILUS_PLACES_VIEW_H
#define NAUTILUS_PLACES_VIEW_H

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-view.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PLACES_VIEW (nautilus_places_view_get_type())

G_DECLARE_FINAL_TYPE (NautilusPlacesView, nautilus_places_view, NAUTILUS, PLACES_VIEW, GtkBox)

NautilusPlacesView*  nautilus_places_view_new                    (void);

G_END_DECLS

#endif /* NAUTILUS_PLACES_VIEW_H */
