/* nautilusgtkplacessidebarprivate.h
 *
 * Copyright (C) 2015 Red Hat
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */

#ifndef __NAUTILUS_GTK_PLACES_SIDEBAR_PRIVATE_H__
#define __NAUTILUS_GTK_PLACES_SIDEBAR_PRIVATE_H__

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-enums.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_GTK_PLACES_SIDEBAR			(nautilus_gtk_places_sidebar_get_type ())
#define NAUTILUS_GTK_PLACES_SIDEBAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR, NautilusGtkPlacesSidebar))
#define NAUTILUS_GTK_PLACES_SIDEBAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR, NautilusGtkPlacesSidebarClass))
#define NAUTILUS_IS_GTK_PLACES_SIDEBAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR))
#define NAUTILUS_IS_GTK_PLACES_SIDEBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR))
#define NAUTILUS_GTK_PLACES_SIDEBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR, NautilusGtkPlacesSidebarClass))

typedef struct _NautilusGtkPlacesSidebar NautilusGtkPlacesSidebar;
typedef struct _NautilusGtkPlacesSidebarClass NautilusGtkPlacesSidebarClass;

GType              nautilus_gtk_places_sidebar_get_type                   (void) G_GNUC_CONST;
GtkWidget *        nautilus_gtk_places_sidebar_new                        (void);

NautilusOpenFlags  nautilus_gtk_places_sidebar_get_open_flags             (NautilusGtkPlacesSidebar   *sidebar);
void               nautilus_gtk_places_sidebar_set_open_flags             (NautilusGtkPlacesSidebar   *sidebar,
                                                                           NautilusOpenFlags           flags);

GFile *            nautilus_gtk_places_sidebar_get_location               (NautilusGtkPlacesSidebar   *sidebar);
void               nautilus_gtk_places_sidebar_set_location               (NautilusGtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);

GFile *            nautilus_gtk_places_sidebar_get_nth_bookmark           (NautilusGtkPlacesSidebar   *sidebar,
                                                                  int                 n);
void               nautilus_gtk_places_sidebar_set_drop_targets_visible   (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            visible);

void               nautilus_gtk_places_sidebar_set_show_trash             (NautilusGtkPlacesSidebar   *sidebar,
                                                                           gboolean                    show_trash);

/* Keep order, since it's used for the sort functions */
typedef enum {
  NAUTILUS_GTK_PLACES_SECTION_INVALID,
  NAUTILUS_GTK_PLACES_SECTION_DEFAULT_LOCATIONS,
  NAUTILUS_GTK_PLACES_SECTION_BOOKMARKS,
  NAUTILUS_GTK_PLACES_SECTION_CLOUD,
  NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
  NAUTILUS_GTK_PLACES_N_SECTIONS
} NautilusGtkPlacesSectionType;

typedef enum {
  NAUTILUS_GTK_PLACES_INVALID,
  NAUTILUS_GTK_PLACES_BUILT_IN,
  NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT,
  NAUTILUS_GTK_PLACES_INTERNAL_MOUNT,
  NAUTILUS_GTK_PLACES_BOOKMARK,
  NAUTILUS_GTK_PLACES_HEADING,
  NAUTILUS_GTK_PLACES_DROP_FEEDBACK,
  NAUTILUS_GTK_PLACES_BOOKMARK_PLACEHOLDER,
  NAUTILUS_GTK_PLACES_N_PLACES
} NautilusGtkPlacesPlaceType;

char *nautilus_gtk_places_sidebar_get_location_title (NautilusGtkPlacesSidebar *sidebar);

G_END_DECLS

#endif /* __NAUTILUS_GTK_PLACES_SIDEBAR_PRIVATE_H__ */
