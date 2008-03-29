/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-view.h: Interface for nautilus views
 
   Copyright (C) 2004 Red Hat Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NAUTILUS_VIEW_H
#define NAUTILUS_VIEW_H

#include <glib-object.h>
#include <gtk/gtkwidget.h>

/* For NautilusZoomLevel */
#include <libnautilus-private/nautilus-icon-info.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW           (nautilus_view_get_type ())
#define NAUTILUS_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_VIEW, NautilusView))
#define NAUTILUS_IS_VIEW(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_VIEW))
#define NAUTILUS_VIEW_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_VIEW, NautilusViewIface))


typedef struct _NautilusView NautilusView; /* dummy typedef */
typedef struct _NautilusViewIface NautilusViewIface;

struct _NautilusViewIface 
{
	GTypeInterface g_iface;

	/* Signals: */

	/* emitted when the view-specific title as returned by get_title changes */
        void           (* title_changed)          (NautilusView          *view);

	/* BONOBOTODO: remove this? */
        void           (* zoom_parameters_changed)(NautilusView          *view);
        void           (* zoom_level_changed)     (NautilusView          *view);
	
	/* VTable: */

	/* Get the id string for this view. Its a constant string, not memory managed */
	const char *   (* get_view_id)            (NautilusView          *view);

	/* Get the widget for this view, can be the same object or a different
	   object owned by the view. Doesn't ref the widget. */
	GtkWidget *    (* get_widget)             (NautilusView          *view);

	/* Called to tell the view to start loading a location, or to reload it.
	   The view responds with a load_underway as soon as it starts loading,
	   and a load_complete when the location is completely read. */
  	void           (* load_location)          (NautilusView          *view,
						   const char            *location_uri);
	
	/* Called to tell the view to stop loading the location its currently loading */
	void           (* stop_loading)           (NautilusView          *view);

	/* Returns the number of selected items in the view */	
	int            (* get_selection_count)    (NautilusView          *view);
	
	/* Returns a list of uris for th selected items in the view, caller frees it */	
	GList *        (* get_selection)          (NautilusView          *view);
	
	/* This is called when the window wants to change the selection in the view */
	void           (* set_selection)          (NautilusView          *view,
						   GList                 *list);
	
	/* Inverts the selection in the view */	
	void           (* invert_selection)       (NautilusView          *view);
	
	/* Return the uri of the first visible file */	
	char *         (* get_first_visible_file) (NautilusView          *view);
	/* Scroll the view so that the file specified by the uri is at the top
	   of the view */
	void           (* scroll_to_file)	  (NautilusView          *view,
						   const char            *uri);
	
	/* This function can supply a special window title, if you don't want one
	   have this function return NULL, or just don't supply a function  */
	char *         (* get_title)              (NautilusView          *view); 


	/* Zoom support */
	gboolean       (* supports_zooming)       (NautilusView          *view);
        void           (* bump_zoom_level)     	  (NautilusView          *view,
						   int                    zoom_increment);
        void           (* zoom_to_level) 	  (NautilusView          *view, 
						   NautilusZoomLevel     level);
        NautilusZoomLevel (* get_zoom_level) 	  (NautilusView          *view);
        void           (* restore_default_zoom_level) (NautilusView          *view);
        gboolean       (* can_zoom_in)	 	  (NautilusView          *view);
        gboolean       (* can_zoom_out)	 	  (NautilusView          *view);

        void           (* grab_focus)             (NautilusView          *view);

	/* Request popup of context menu referring to the open location.
	 * This is triggered in spatial windows by right-clicking the location button,
	 * in navigational windows by right-clicking the "Location:" label in the
	 * navigation bar.
	 * The view may display the popup synchronously, asynchronously
	 * or not react to the popup request at all. */
	void           (* pop_up_location_context_menu) (NautilusView   *view,
							 GdkEventButton *event);

	/* Padding for future expansion */
	void (*_reserved1) (void);
	void (*_reserved2) (void);
	void (*_reserved3) (void);
	void (*_reserved4) (void);
	void (*_reserved5) (void);
	void (*_reserved6) (void);
	void (*_reserved7) (void);
};

GType             nautilus_view_get_type             (void);

const char *      nautilus_view_get_view_id                (NautilusView      *view);
GtkWidget *       nautilus_view_get_widget                 (NautilusView      *view);
void              nautilus_view_load_location              (NautilusView      *view,
							    const char        *location_uri);
void              nautilus_view_stop_loading               (NautilusView      *view);
int               nautilus_view_get_selection_count        (NautilusView      *view);
GList *           nautilus_view_get_selection              (NautilusView      *view);
void              nautilus_view_set_selection              (NautilusView      *view,
							    GList             *list);
void              nautilus_view_invert_selection           (NautilusView      *view);
char *            nautilus_view_get_first_visible_file     (NautilusView      *view);
void              nautilus_view_scroll_to_file             (NautilusView      *view,
							    const char        *uri);
char *            nautilus_view_get_title                  (NautilusView      *view);
gboolean          nautilus_view_supports_zooming           (NautilusView      *view);
void              nautilus_view_bump_zoom_level            (NautilusView      *view,
							    int                zoom_increment);
void              nautilus_view_zoom_to_level              (NautilusView      *view,
							    NautilusZoomLevel  level);
void              nautilus_view_restore_default_zoom_level (NautilusView      *view);
gboolean          nautilus_view_can_zoom_in                (NautilusView      *view);
gboolean          nautilus_view_can_zoom_out               (NautilusView      *view);
NautilusZoomLevel nautilus_view_get_zoom_level             (NautilusView      *view);
void              nautilus_view_pop_up_location_context_menu (NautilusView    *view,
							      GdkEventButton  *event);
void              nautilus_view_grab_focus                 (NautilusView      *view);

G_END_DECLS

#endif /* NAUTILUS_VIEW_H */
