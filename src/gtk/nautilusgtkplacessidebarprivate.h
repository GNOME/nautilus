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

G_BEGIN_DECLS

#define NAUTILUS_TYPE_GTK_PLACES_SIDEBAR			(nautilus_gtk_places_sidebar_get_type ())
#define NAUTILUS_GTK_PLACES_SIDEBAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR, NautilusGtkPlacesSidebar))
#define NAUTILUS_GTK_PLACES_SIDEBAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR, NautilusGtkPlacesSidebarClass))
#define NAUTILUS_IS_GTK_PLACES_SIDEBAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR))
#define NAUTILUS_IS_GTK_PLACES_SIDEBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR))
#define NAUTILUS_GTK_PLACES_SIDEBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_GTK_PLACES_SIDEBAR, NautilusGtkPlacesSidebarClass))

typedef struct _NautilusGtkPlacesSidebar NautilusGtkPlacesSidebar;
typedef struct _NautilusGtkPlacesSidebarClass NautilusGtkPlacesSidebarClass;

/*
 * NautilusGtkPlacesOpenFlags:
 * @NAUTILUS_GTK_PLACES_OPEN_NORMAL: This is the default mode that NautilusGtkPlacesSidebar uses if no other flags
 *  are specified.  It indicates that the calling application should open the selected location
 *  in the normal way, for example, in the folder view beside the sidebar.
 * @NAUTILUS_GTK_PLACES_OPEN_NEW_TAB: When passed to nautilus_gtk_places_sidebar_set_open_flags(), this indicates
 *  that the application can open folders selected from the sidebar in new tabs.  This value
 *  will be passed to the NautilusGtkPlacesSidebar::open-location signal when the user selects
 *  that a location be opened in a new tab instead of in the standard fashion.
 * @NAUTILUS_GTK_PLACES_OPEN_NEW_WINDOW: Similar to @NAUTILUS_GTK_PLACES_OPEN_NEW_TAB, but indicates that the application
 *  can open folders in new windows.
 *
 * These flags serve two purposes.  First, the application can call nautilus_gtk_places_sidebar_set_open_flags()
 * using these flags as a bitmask.  This tells the sidebar that the application is able to open
 * folders selected from the sidebar in various ways, for example, in new tabs or in new windows in
 * addition to the normal mode.
 *
 * Second, when one of these values gets passed back to the application in the
 * NautilusGtkPlacesSidebar::open-location signal, it means that the application should
 * open the selected location in the normal way, in a new tab, or in a new
 * window.  The sidebar takes care of determining the desired way to open the location,
 * based on the modifier keys that the user is pressing at the time the selection is made.
 *
 * If the application never calls nautilus_gtk_places_sidebar_set_open_flags(), then the sidebar will only
 * use NAUTILUS_GTK_PLACES_OPEN_NORMAL in the NautilusGtkPlacesSidebar::open-location signal.  This is the
 * default mode of operation.
 */
typedef enum {
  NAUTILUS_GTK_PLACES_OPEN_NORMAL     = 1 << 0,
  NAUTILUS_GTK_PLACES_OPEN_NEW_TAB    = 1 << 1,
  NAUTILUS_GTK_PLACES_OPEN_NEW_WINDOW = 1 << 2
} NautilusGtkPlacesOpenFlags;

GType              nautilus_gtk_places_sidebar_get_type                   (void) G_GNUC_CONST;
GtkWidget *        nautilus_gtk_places_sidebar_new                        (void);

NautilusGtkPlacesOpenFlags nautilus_gtk_places_sidebar_get_open_flags             (NautilusGtkPlacesSidebar   *sidebar);
void               nautilus_gtk_places_sidebar_set_open_flags             (NautilusGtkPlacesSidebar   *sidebar,
                                                                  NautilusGtkPlacesOpenFlags  flags);

GFile *            nautilus_gtk_places_sidebar_get_location               (NautilusGtkPlacesSidebar   *sidebar);
void               nautilus_gtk_places_sidebar_set_location               (NautilusGtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);

GFile *            nautilus_gtk_places_sidebar_get_nth_bookmark           (NautilusGtkPlacesSidebar   *sidebar,
                                                                  int                 n);
void               nautilus_gtk_places_sidebar_set_drop_targets_visible   (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            visible);

/* Keep order, since it's used for the sort functions */
typedef enum {
  NAUTILUS_GTK_PLACES_SECTION_INVALID,
  NAUTILUS_GTK_PLACES_SECTION_COMPUTER,
  NAUTILUS_GTK_PLACES_SECTION_BOOKMARKS,
  NAUTILUS_GTK_PLACES_SECTION_CLOUD,
  NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
  NAUTILUS_GTK_PLACES_N_SECTIONS
} NautilusGtkPlacesSectionType;

typedef enum {
  NAUTILUS_GTK_PLACES_INVALID,
  NAUTILUS_GTK_PLACES_BUILT_IN,
  NAUTILUS_GTK_PLACES_XDG_DIR,
  NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT,
  NAUTILUS_GTK_PLACES_INTERNAL_MOUNT,
  NAUTILUS_GTK_PLACES_BOOKMARK,
  NAUTILUS_GTK_PLACES_HEADING,
  NAUTILUS_GTK_PLACES_DROP_FEEDBACK,
  NAUTILUS_GTK_PLACES_BOOKMARK_PLACEHOLDER,
  NAUTILUS_GTK_PLACES_STARRED_LOCATION,
  NAUTILUS_GTK_PLACES_N_PLACES
} NautilusGtkPlacesPlaceType;

char *nautilus_gtk_places_sidebar_get_location_title (NautilusGtkPlacesSidebar *sidebar);

G_END_DECLS

#endif /* __NAUTILUS_GTK_PLACES_SIDEBAR_PRIVATE_H__ */
