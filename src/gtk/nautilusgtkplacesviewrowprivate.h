/* nautilusgtkplacesviewrow.h
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_GTK_PLACES_VIEW_ROW_H
#define NAUTILUS_GTK_PLACES_VIEW_ROW_H

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#endif


G_BEGIN_DECLS

#define NAUTILUS_TYPE_GTK_PLACES_VIEW_ROW (nautilus_gtk_places_view_row_get_type())

 G_DECLARE_FINAL_TYPE (NautilusGtkPlacesViewRow, nautilus_gtk_places_view_row, NAUTILUS, GTK_PLACES_VIEW_ROW, GtkListBoxRow)

GtkWidget*         nautilus_gtk_places_view_row_new                       (GVolume            *volume,
                                                                  GMount             *mount);

GtkWidget*         nautilus_gtk_places_view_row_get_eject_button          (NautilusGtkPlacesViewRow   *row);

GMount*            nautilus_gtk_places_view_row_get_mount                 (NautilusGtkPlacesViewRow   *row);

GVolume*           nautilus_gtk_places_view_row_get_volume                (NautilusGtkPlacesViewRow   *row);

GFile*             nautilus_gtk_places_view_row_get_file                  (NautilusGtkPlacesViewRow   *row);

void               nautilus_gtk_places_view_row_set_busy                  (NautilusGtkPlacesViewRow   *row,
                                                                  gboolean            is_busy);

gboolean           nautilus_gtk_places_view_row_get_is_network            (NautilusGtkPlacesViewRow   *row);

void               nautilus_gtk_places_view_row_set_is_network            (NautilusGtkPlacesViewRow   *row,
                                                                  gboolean            is_network);

void               nautilus_gtk_places_view_row_set_path_size_group       (NautilusGtkPlacesViewRow   *row,
                                                                  GtkSizeGroup       *group);

void               nautilus_gtk_places_view_row_set_space_size_group      (NautilusGtkPlacesViewRow   *row,
                                                                  GtkSizeGroup       *group);

G_END_DECLS

#endif /* NAUTILUS_GTK_PLACES_VIEW_ROW_H */
