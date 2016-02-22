/* gtkplacesview.h
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


#ifndef GTK_NAUTILUS_PLACES_VIEW_H
#define GTK_NAUTILUS_PLACES_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_NAUTILUS_PLACES_VIEW        (gtk_nautilus_places_view_get_type ())
#define GTK_NAUTILUS_PLACES_VIEW(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_NAUTILUS_PLACES_VIEW, GtkNautilusPlacesView))
#define GTK_NAUTILUS_PLACES_VIEW_CLASS(klass)(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_NAUTILUS_PLACES_VIEW, GtkNautilusPlacesViewClass))
#define GTK_IS_NAUTILUS_PLACES_VIEW(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_NAUTILUS_PLACES_VIEW))
#define GTK_IS_NAUTILUS_PLACES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_NAUTILUS_PLACES_VIEW))
#define GTK_NAUTILUS_PLACES_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_NAUTILUS_PLACES_VIEW, GtkNautilusPlacesViewClass))

typedef struct _GtkNautilusPlacesView GtkNautilusPlacesView;
typedef struct _GtkNautilusPlacesViewClass GtkNautilusPlacesViewClass;
typedef struct _GtkNautilusPlacesViewPrivate GtkNautilusPlacesViewPrivate;

struct _GtkNautilusPlacesViewClass
{
  GtkBoxClass parent_class;

  void     (* open_location)        (GtkNautilusPlacesView          *view,
                                     GFile                  *location,
                                     GtkPlacesOpenFlags  open_flags);

  void    (* show_error_message)     (GtkPlacesSidebar      *sidebar,
                                      const gchar           *primary,
                                      const gchar           *secondary);

  /*< private >*/

  /* Padding for future expansion */
  gpointer reserved[10];
};

struct _GtkNautilusPlacesView
{
  GtkBox parent_instance;
};

GType              gtk_nautilus_places_view_get_type                      (void) G_GNUC_CONST;

GtkPlacesOpenFlags gtk_nautilus_places_view_get_open_flags                (GtkNautilusPlacesView      *view);
void               gtk_nautilus_places_view_set_open_flags                (GtkNautilusPlacesView      *view,
                                                                  GtkPlacesOpenFlags  flags);

const gchar*       gtk_nautilus_places_view_get_search_query              (GtkNautilusPlacesView      *view);
void               gtk_nautilus_places_view_set_search_query              (GtkNautilusPlacesView      *view,
                                                                  const gchar        *query_text);

gboolean           gtk_nautilus_places_view_get_local_only                (GtkNautilusPlacesView         *view);

void               gtk_nautilus_places_view_set_local_only                (GtkNautilusPlacesView         *view,
                                                                  gboolean               local_only);

gboolean           gtk_nautilus_places_view_get_loading                   (GtkNautilusPlacesView         *view);

GtkWidget *        gtk_nautilus_places_view_new                           (void);

G_END_DECLS

#endif /* GTK_NAUTILUS_PLACES_VIEW_H */
