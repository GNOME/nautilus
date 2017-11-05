/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* GtkPlacesSidebar - sidebar widget for places in the filesystem
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * This code comes from Nautilus, GNOME’s file manager.
 *
 * Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *           Federico Mena Quintero <federico@gnome.org>
 *           Adam Hukalowicz (procing3r at gmail dot com)
 */

#ifndef NAUTILUS_GTK_PLACES_SIDEBAR_H
#define NAUTILUS_GTK_PLACES_SIDEBAR_H

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_GTK_TYPE_PLACES_SIDEBAR			(nautilus_gtk_places_sidebar_get_type ())
#define NAUTILUS_GTK_PLACES_SIDEBAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_GTK_TYPE_PLACES_SIDEBAR, NautilusGtkPlacesSidebar))
#define NAUTILUS_GTK_PLACES_SIDEBAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_GTK_TYPE_PLACES_SIDEBAR, NautilusGtkPlacesSidebarClass))
#define NAUTILUS_GTK_IS_PLACES_SIDEBAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_GTK_TYPE_PLACES_SIDEBAR))
#define NAUTILUS_GTK_IS_PLACES_SIDEBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_GTK_TYPE_PLACES_SIDEBAR))
#define NAUTILUS_GTK_PLACES_SIDEBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_GTK_TYPE_PLACES_SIDEBAR, NautilusGtkPlacesSidebarClass))

typedef struct _NautilusGtkPlacesSidebar NautilusGtkPlacesSidebar;
typedef struct _NautilusGtkPlacesSidebarClass NautilusGtkPlacesSidebarClass;

/**
 * GtkPlacesOpenFlags:
 * @GTK_PLACES_OPEN_NORMAL: This is the default mode that #GtkPlacesSidebar uses if no other flags
 *  are specified.  It indicates that the calling application should open the selected location
 *  in the normal way, for example, in the folder view beside the sidebar.
 * @GTK_PLACES_OPEN_NEW_TAB: When passed to gtk_places_sidebar_set_open_flags(), this indicates
 *  that the application can open folders selected from the sidebar in new tabs.  This value
 *  will be passed to the #GtkPlacesSidebar::open-location signal when the user selects
 *  that a location be opened in a new tab instead of in the standard fashion.
 * @GTK_PLACES_OPEN_NEW_WINDOW: Similar to @GTK_PLACES_OPEN_NEW_TAB, but indicates that the application
 *  can open folders in new windows.
 *
 * These flags serve two purposes.  First, the application can call gtk_places_sidebar_set_open_flags()
 * using these flags as a bitmask.  This tells the sidebar that the application is able to open
 * folders selected from the sidebar in various ways, for example, in new tabs or in new windows in
 * addition to the normal mode.
 *
 * Second, when one of these values gets passed back to the application in the
 * #GtkPlacesSidebar::open-location signal, it means that the application should
 * open the selected location in the normal way, in a new tab, or in a new
 * window.  The sidebar takes care of determining the desired way to open the location,
 * based on the modifier keys that the user is pressing at the time the selection is made.
 *
 * If the application never calls nautilus_gtk_places_sidebar_set_open_flags(), then the sidebar will only
 * use #GTK_PLACES_OPEN_NORMAL in the #GtkPlacesSidebar::open-location signal.  This is the
 * default mode of operation.
 */
typedef enum {
  NAUTLIS_GTK_PLACES_OPEN_NORMAL     = 1 << 0,
  NAUTILUS_GTK_PLACES_OPEN_NEW_TAB    = 1 << 1,
  NAUTILUS_GTK_PLACES_OPEN_NEW_WINDOW = 1 << 2
} NautilusGtkPlacesOpenFlags;

GDK_AVAILABLE_IN_3_10
GType              nautilus_gtk_places_sidebar_get_type                   (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_3_10
GtkWidget *        nautilus_gtk_places_sidebar_new                        (void);

GDK_AVAILABLE_IN_3_10
GtkPlacesOpenFlags nautilus_gtk_places_sidebar_get_open_flags             (NautilusGtkPlacesSidebar   *sidebar);
GDK_AVAILABLE_IN_3_10
void               nautilus_gtk_places_sidebar_set_open_flags             (NautilusGtkPlacesSidebar   *sidebar,
                                                                  GtkPlacesOpenFlags  flags);

GDK_AVAILABLE_IN_3_10
GFile *            nautilus_gtk_places_sidebar_get_location               (NautilusGtkPlacesSidebar   *sidebar);
GDK_AVAILABLE_IN_3_10
void               nautilus_gtk_places_sidebar_set_location               (NautilusGtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);

GDK_AVAILABLE_IN_3_18
gboolean           nautilus_gtk_places_sidebar_get_show_recent            (NautilusGtkPlacesSidebar   *sidebar);
GDK_AVAILABLE_IN_3_18
void               nautilus_gtk_places_sidebar_set_show_recent            (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_recent);

GDK_AVAILABLE_IN_3_10
gboolean           nautilus_gtk_places_sidebar_get_show_desktop           (NautilusGtkPlacesSidebar   *sidebar);
GDK_AVAILABLE_IN_3_10
void               nautilus_gtk_places_sidebar_set_show_desktop           (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_desktop);

GDK_AVAILABLE_IN_3_14
gboolean           nautilus_gtk_places_sidebar_get_show_enter_location    (NautilusGtkPlacesSidebar   *sidebar);
GDK_AVAILABLE_IN_3_14
void               nautilus_gtk_places_sidebar_set_show_enter_location    (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_enter_location);

GDK_AVAILABLE_IN_3_12
void                 nautilus_gtk_places_sidebar_set_local_only           (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            local_only);
GDK_AVAILABLE_IN_3_12
gboolean             nautilus_gtk_places_sidebar_get_local_only           (NautilusGtkPlacesSidebar   *sidebar);


GDK_AVAILABLE_IN_3_10
void               nautilus_gtk_places_sidebar_add_shortcut               (NautilusGtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);
GDK_AVAILABLE_IN_3_10
void               nautilus_gtk_places_sidebar_remove_shortcut            (NautilusGtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);
GDK_AVAILABLE_IN_3_10
GSList *           nautilus_gtk_places_sidebar_list_shortcuts             (NautilusGtkPlacesSidebar   *sidebar);

GDK_AVAILABLE_IN_3_10
GFile *            nautilus_gtk_places_sidebar_get_nth_bookmark           (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gint                n);
GDK_AVAILABLE_IN_3_18
void               nautilus_gtk_places_sidebar_set_drop_targets_visible   (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            visible,
                                                                  GdkDragContext     *context);
GDK_AVAILABLE_IN_3_18
gboolean           nautilus_gtk_places_sidebar_get_show_trash             (NautilusGtkPlacesSidebar   *sidebar);
GDK_AVAILABLE_IN_3_18
void               nautilus_gtk_places_sidebar_set_show_trash             (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_trash);

GDK_AVAILABLE_IN_3_18
void                 nautilus_gtk_places_sidebar_set_show_other_locations (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_other_locations);
GDK_AVAILABLE_IN_3_18
gboolean             nautilus_gtk_places_sidebar_get_show_other_locations (NautilusGtkPlacesSidebar   *sidebar);

G_END_DECLS

#endif /* NAUTILUS_GTK_PLACES_SIDEBAR_H */
