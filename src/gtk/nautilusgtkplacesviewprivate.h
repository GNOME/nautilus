/* nautilusgtkplacesview.h
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#ifndef NAUTILUS_GTK_PLACES_VIEW_H
#define NAUTILUS_GTK_PLACES_VIEW_H

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#endif

#include "nautilusgtkplacessidebar.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_GTK_PLACES_VIEW        (nautilus_gtk_places_view_get_type ())
#define NAUTILUS_GTK_PLACES_VIEW(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_GTK_PLACES_VIEW, NautilusGtkPlacesView))
#define NAUTILUS_GTK_PLACES_VIEW_CLASS(klass)(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_GTK_PLACES_VIEW, NautilusGtkPlacesViewClass))
#define NAUTILUS_IS_GTK_PLACES_VIEW(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_GTK_PLACES_VIEW))
#define NAUTILUS_IS_GTK_PLACES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_GTK_PLACES_VIEW))
#define NAUTILUS_GTK_PLACES_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_GTK_PLACES_VIEW, NautilusGtkPlacesViewClass))

typedef struct _NautilusGtkPlacesView NautilusGtkPlacesView;
typedef struct _NautilusGtkPlacesViewClass NautilusGtkPlacesViewClass;
typedef struct _NautilusGtkPlacesViewPrivate NautilusGtkPlacesViewPrivate;

struct _NautilusGtkPlacesViewClass
{
  GtkBoxClass parent_class;

  void     (* open_location)        (NautilusGtkPlacesView          *view,
                                     GFile                  *location,
                                     NautilusGtkPlacesOpenFlags  open_flags);

  void    (* show_error_message)     (NautilusGtkPlacesSidebar      *sidebar,
                                      const gchar           *primary,
                                      const gchar           *secondary);

  /*< private >*/

  /* Padding for future expansion */
  gpointer reserved[10];
};

struct _NautilusGtkPlacesView
{
  GtkBox parent_instance;
};

GType              nautilus_gtk_places_view_get_type                      (void) G_GNUC_CONST;

NautilusGtkPlacesOpenFlags nautilus_gtk_places_view_get_open_flags                (NautilusGtkPlacesView      *view);
void               nautilus_gtk_places_view_set_open_flags                (NautilusGtkPlacesView      *view,
                                                                  NautilusGtkPlacesOpenFlags  flags);

const gchar*       nautilus_gtk_places_view_get_search_query              (NautilusGtkPlacesView      *view);
void               nautilus_gtk_places_view_set_search_query              (NautilusGtkPlacesView      *view,
                                                                  const gchar        *query_text);

gboolean           nautilus_gtk_places_view_get_local_only                (NautilusGtkPlacesView         *view);

void               nautilus_gtk_places_view_set_local_only                (NautilusGtkPlacesView         *view,
                                                                  gboolean               local_only);

gboolean           nautilus_gtk_places_view_get_loading                   (NautilusGtkPlacesView         *view);

GtkWidget *        nautilus_gtk_places_view_new                           (void);

G_END_DECLS

#endif /* NAUTILUS_GTK_PLACES_VIEW_H */
