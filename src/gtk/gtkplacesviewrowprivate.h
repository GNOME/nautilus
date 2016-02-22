/* gtkplacesviewrow.h
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

#ifndef GTK_NAUTILUS_PLACES_VIEW_ROW_H
#define GTK_NAUTILUS_PLACES_VIEW_ROW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_NAUTILUS_PLACES_VIEW_ROW (gtk_nautilus_places_view_row_get_type())

G_DECLARE_FINAL_TYPE (GtkNautilusPlacesViewRow, gtk_nautilus_places_view_row, GTK, NAUTILUS_PLACES_VIEW_ROW, GtkListBoxRow)

GtkWidget*         gtk_nautilus_places_view_row_new                       (GVolume            *volume,
                                                                  GMount             *mount);

GtkWidget*         gtk_nautilus_places_view_row_get_eject_button          (GtkNautilusPlacesViewRow   *row);

GtkWidget*         gtk_nautilus_places_view_row_get_event_box             (GtkNautilusPlacesViewRow   *row);

GMount*            gtk_nautilus_places_view_row_get_mount                 (GtkNautilusPlacesViewRow   *row);

GVolume*           gtk_nautilus_places_view_row_get_volume                (GtkNautilusPlacesViewRow   *row);

GFile*             gtk_nautilus_places_view_row_get_file                  (GtkNautilusPlacesViewRow   *row);

void               gtk_nautilus_places_view_row_set_busy                  (GtkNautilusPlacesViewRow   *row,
                                                                  gboolean            is_busy);

gboolean           gtk_nautilus_places_view_row_get_is_network            (GtkNautilusPlacesViewRow   *row);

void               gtk_nautilus_places_view_row_set_is_network            (GtkNautilusPlacesViewRow   *row,
                                                                  gboolean            is_network);

void               gtk_nautilus_places_view_row_set_path_size_group       (GtkNautilusPlacesViewRow   *row,
                                                                  GtkSizeGroup       *group);

void               gtk_nautilus_places_view_row_set_space_size_group      (GtkNautilusPlacesViewRow   *row,
                                                                  GtkSizeGroup       *group);

G_END_DECLS

#endif /* GTK_NAUTILUS_PLACES_VIEW_ROW_H */
