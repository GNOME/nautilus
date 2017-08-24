/* nautilus-pathbar.h
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * 
 */

#ifndef NAUTILUS_PATHBAR_H
#define NAUTILUS_PATHBAR_H

#include <gtk/gtk.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_PATH_BAR (nautilus_path_bar_get_type ())
G_DECLARE_DERIVABLE_TYPE (NautilusPathBar, nautilus_path_bar, NAUTILUS, PATH_BAR, GtkContainer)

struct _NautilusPathBarClass
{
	GtkContainerClass parent_class;

	void     (* path_clicked)   (NautilusPathBar  *self,
				     GFile            *location);
        void     (* open_location)  (NautilusPathBar   *self,
                                     GFile             *location,
                                     GtkPlacesOpenFlags flags);
};
void     nautilus_path_bar_set_path    (NautilusPathBar *self, GFile *file);

#endif /* NAUTILUS_PATHBAR_H */
