/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container.h - Icon container widget.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_ICON_CONTAINER_H
#define NAUTILUS_ICON_CONTAINER_H

#include <libgnomeui/gnome-canvas.h>
#include "nautilus-icon-factory.h"

typedef struct NautilusIconContainer NautilusIconContainer;
typedef struct NautilusIconContainerClass NautilusIconContainerClass;
typedef struct NautilusIconContainerDetails NautilusIconContainerDetails;

typedef struct NautilusIconData NautilusIconData;

#define NAUTILUS_ICON_CONTAINER(obj) \
	GTK_CHECK_CAST (obj, nautilus_icon_container_get_type (), NautilusIconContainer)
#define NAUTILUS_ICON_CONTAINER_CLASS(k) \
	GTK_CHECK_CLASS_CAST (k, nautilus_icon_container_get_type (), NautilusIconListView)
#define NAUTILUS_IS_ICON_CONTAINER(obj) \
	GTK_CHECK_TYPE (obj, nautilus_icon_container_get_type ())

#define NAUTILUS_ICON_CONTAINER_ICON_DATA(pointer) \
	((NautilusIconData *) (pointer))

struct NautilusIconContainer {
	GnomeCanvas canvas;
	NautilusIconContainerDetails *details;
};

struct NautilusIconContainerClass {
	GnomeCanvasClass parent_class;

	int                    (* button_press) 	    (NautilusIconContainer *container,
							     GdkEventButton *event);
	void                   (* activate)	  	    (NautilusIconContainer *container,
							     NautilusIconData *data);
	
	void                   (* context_click_selection)  (NautilusIconContainer *container);
	void                   (* context_click_background) (NautilusIconContainer *container);

	void                   (* selection_changed) 	    (NautilusIconContainer *container);

	void                   (* icon_changed)             (NautilusIconContainer *container,
							     NautilusIconData *data,
							     int x, int y,
							     double scale_x, double scale_y);
	
	/* Connect to these signals to supply information about icons.
	 * They are called as needed after the icons are inserted.
	 */
	NautilusScalableIcon * (* get_icon_images)          (NautilusIconContainer *container,
							     NautilusIconData *data,
							     GList **emblem_images);
	char *                 (* get_icon_text)            (NautilusIconContainer *container,
							     NautilusIconData *data);
	char *                 (* get_icon_uri)             (NautilusIconContainer *container,
							     NautilusIconData *data);
	char *  	       (* get_icon_property)        (NautilusIconContainer *container,
							     NautilusIconData *data,
							     const char *property_name);
};

/* GtkObject */
guint      nautilus_icon_container_get_type                (void);
GtkWidget *nautilus_icon_container_new                     (void);

/* adding, removing, and managing icons */
void       nautilus_icon_container_clear                   (NautilusIconContainer *view);
void       nautilus_icon_container_add                     (NautilusIconContainer *view,
							    NautilusIconData      *data,
							    int                    x,
							    int                    y,
							    double                 scale_x,
							    double                 scale_y);
void       nautilus_icon_container_add_auto                (NautilusIconContainer *view,
							    NautilusIconData      *data);
gboolean   nautilus_icon_container_remove                  (NautilusIconContainer *view,
							    NautilusIconData      *data);
void       nautilus_icon_container_request_update          (NautilusIconContainer *view,
							    NautilusIconData      *data);
void       nautilus_icon_container_request_update_all      (NautilusIconContainer *container);

/* operations on all icons */
void       nautilus_icon_container_relayout                (NautilusIconContainer *view);
void       nautilus_icon_container_line_up                 (NautilusIconContainer *view);
void       nautilus_icon_container_unselect_all            (NautilusIconContainer *view);
void       nautilus_icon_container_select_all              (NautilusIconContainer *view);

/* operations on the selection */
GList *    nautilus_icon_container_get_selection           (NautilusIconContainer *view);
gboolean   nautilus_icon_container_has_stretch_handles     (NautilusIconContainer *container);
gboolean   nautilus_icon_container_is_stretched            (NautilusIconContainer *container);
void       nautilus_icon_container_show_stretch_handles    (NautilusIconContainer *container);
void       nautilus_icon_container_unstretch               (NautilusIconContainer *container);

/* options */
int        nautilus_icon_container_get_zoom_level          (NautilusIconContainer *view);
void       nautilus_icon_container_set_zoom_level          (NautilusIconContainer *view,
							    int                    new_zoom_level);
void       nautilus_icon_container_enable_linger_selection (NautilusIconContainer *view,
							    gboolean               enable);

#endif
