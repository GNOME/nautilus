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

#ifndef GNOME_ICON_CONTAINER_H
#define GNOME_ICON_CONTAINER_H

#include <libgnomeui/gnome-canvas.h>
#include "nautilus-icon-factory.h"

typedef struct GnomeIconContainer GnomeIconContainer;
typedef struct GnomeIconContainerClass GnomeIconContainerClass;
typedef struct GnomeIconContainerDetails GnomeIconContainerDetails;

typedef struct GnomeIconContainerIconData GnomeIconContainerIconData;


#define GNOME_ICON_CONTAINER(obj) \
	GTK_CHECK_CAST (obj, gnome_icon_container_get_type (), GnomeIconContainer)
#define GNOME_ICON_CONTAINER_CLASS(k) \
	GTK_CHECK_CLASS_CAST (k, gnome_icon_container_get_type (), GnomeIconListView)
#define GNOME_IS_ICON_CONTAINER(obj) \
	GTK_CHECK_TYPE (obj, gnome_icon_container_get_type ())

#define GNOME_ICON_CONTAINER_ICON_DATA(pointer) \
	((GnomeIconContainerIconData *) (pointer))


struct GnomeIconContainer {
	GnomeCanvas canvas;
	GnomeIconContainerDetails *details;
};

struct GnomeIconContainerClass {
	GnomeCanvasClass parent_class;

	int                    (* button_press) 	    (GnomeIconContainer *container,
							     GdkEventButton *event);
	void                   (* activate)	  	    (GnomeIconContainer *container,
							     GnomeIconContainerIconData *data);
	
	void                   (* context_click_selection)  (GnomeIconContainer *container);
	void                   (* context_click_background) (GnomeIconContainer *container);

	void                   (* selection_changed) 	    (GnomeIconContainer *container);

	void                   (* icon_changed)             (GnomeIconContainer *container,
							     GnomeIconContainerIconData *data,
							     int x, int y,
							     double scale_x, double scale_y);
	
	/* Connect to these signals to supply information about icons.
	 * They are called as needed after the icons are inserted.
	 */
	NautilusScalableIcon * (* get_icon_images)          (GnomeIconContainer *container,
							     GnomeIconContainerIconData *data,
							     GList **emblem_images);
	char *                 (* get_icon_text)            (GnomeIconContainer *container,
							     GnomeIconContainerIconData *data);
	char *                 (* get_icon_uri)             (GnomeIconContainer *container,
							     GnomeIconContainerIconData *data);
	char *  	       (* get_icon_property)        (GnomeIconContainer *container,
							     GnomeIconContainerIconData *data,
							     const char *property_name);
};



/* GtkObject */
guint      gnome_icon_container_get_type                (void);
GtkWidget *gnome_icon_container_new                     (void);

/* adding, removing, and managing icons */
void       gnome_icon_container_clear                   (GnomeIconContainer         *view);
void       gnome_icon_container_add                     (GnomeIconContainer         *view,
							 GnomeIconContainerIconData *data,
							 int                         x,
							 int                         y,
							 double                      scale_x,
							 double                      scale_y);
void       gnome_icon_container_add_auto                (GnomeIconContainer         *view,
							 GnomeIconContainerIconData *data);
gboolean   gnome_icon_container_remove                  (GnomeIconContainer         *view,
							 GnomeIconContainerIconData *data);
void       gnome_icon_container_request_update          (GnomeIconContainer         *view,
							 GnomeIconContainerIconData *data);
void       gnome_icon_container_request_update_all      (GnomeIconContainer         *container);

/* operations on all icons */
void       gnome_icon_container_relayout                (GnomeIconContainer         *view);
void       gnome_icon_container_line_up                 (GnomeIconContainer         *view);
void       gnome_icon_container_unselect_all            (GnomeIconContainer         *view);
void       gnome_icon_container_select_all              (GnomeIconContainer         *view);

/* operations on the selection */
GList *    gnome_icon_container_get_selection           (GnomeIconContainer         *view);
gboolean   gnome_icon_container_has_stretch_handles     (GnomeIconContainer         *container);
gboolean   gnome_icon_container_is_stretched            (GnomeIconContainer         *container);
void       gnome_icon_container_show_stretch_handles    (GnomeIconContainer         *container);
void       gnome_icon_container_unstretch               (GnomeIconContainer         *container);

/* options */
int        gnome_icon_container_get_zoom_level          (GnomeIconContainer         *view);
void       gnome_icon_container_set_zoom_level          (GnomeIconContainer         *view,
							 int                         new_zoom_level);
void       gnome_icon_container_enable_linger_selection (GnomeIconContainer         *view,
							 gboolean                    enable);

#endif
