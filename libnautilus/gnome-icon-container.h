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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef _GNOME_ICON_CONTAINER_H
#define _GNOME_ICON_CONTAINER_H

#include <libgnomeui/gnome-canvas.h>
#include "nautilus-icons-controller.h"

typedef struct _GnomeIconContainer GnomeIconContainer;
typedef struct _GnomeIconContainerClass GnomeIconContainerClass;
typedef struct _GnomeIconContainerDetails GnomeIconContainerDetails;


#define GNOME_ICON_CONTAINER(obj) \
	GTK_CHECK_CAST (obj, gnome_icon_container_get_type (), GnomeIconContainer)
#define GNOME_ICON_CONTAINER_CLASS(k) \
	GTK_CHECK_CLASS_CAST (k, gnome_icon_container_get_type (), GnomeIconListView)
#define GNOME_IS_ICON_CONTAINER(obj) \
	GTK_CHECK_TYPE (obj, gnome_icon_container_get_type ())


struct _GnomeIconContainer {
	GnomeCanvas canvas;
	GnomeIconContainerDetails *details;
};

struct _GnomeIconContainerClass {
	GnomeCanvasClass parent_class;

	void (* selection_changed) 	  (GnomeIconContainer *container);
	int  (* button_press) 		  (GnomeIconContainer *container,
					   GdkEventButton *event);
	void (* activate)	  	  (GnomeIconContainer *container,
					   NautilusControllerIcon *icon);

	void (* context_click_selection)  (GnomeIconContainer *container);

	void (* context_click_background) (GnomeIconContainer *container);

	void (* icon_changed)		  (GnomeIconContainer *container,
					   NautilusControllerIcon *icon,
					   int x, int y, double scale_x, double scale_y);
};



guint      gnome_icon_container_get_type                (void);
GtkWidget *gnome_icon_container_new                     (NautilusIconsController *controller);

void       gnome_icon_container_enable_linger_selection (GnomeIconContainer      *view,
							 gboolean                 enable);

void       gnome_icon_container_clear                   (GnomeIconContainer      *view);
void       gnome_icon_container_add                     (GnomeIconContainer      *view,
							 NautilusControllerIcon  *icon,
							 int                      x,
							 int                      y,
							 double                   scale_x,
							 double                   scale_y);
void       gnome_icon_container_add_auto                (GnomeIconContainer      *view,
							 NautilusControllerIcon  *icon);
gboolean   gnome_icon_container_remove                  (GnomeIconContainer      *view,
							 NautilusControllerIcon  *icon);
void       gnome_icon_container_update                  (GnomeIconContainer      *view,
							 NautilusControllerIcon  *icon);

void       gnome_icon_container_relayout                (GnomeIconContainer      *view);
void       gnome_icon_container_line_up                 (GnomeIconContainer      *view);

void	   gnome_icon_container_request_update_all 	(GnomeIconContainer 	 *container);

GList *    gnome_icon_container_get_selection           (GnomeIconContainer      *view);

int        gnome_icon_container_get_zoom_level          (GnomeIconContainer      *view);
void       gnome_icon_container_set_zoom_level          (GnomeIconContainer      *view,
							 int                      new_zoom_level);

void       gnome_icon_container_unselect_all            (GnomeIconContainer      *view);
void       gnome_icon_container_select_all              (GnomeIconContainer      *view);
void	   gnome_icon_container_update_icon		(GnomeIconContainer      *container,
							 char                    *icon_uri);

/* The following all work on the selected icon. */
gboolean   gnome_icon_container_has_stretch_handles     (GnomeIconContainer      *container);
gboolean   gnome_icon_container_is_stretched            (GnomeIconContainer      *container);
void       gnome_icon_container_show_stretch_handles    (GnomeIconContainer      *container);
void       gnome_icon_container_unstretch               (GnomeIconContainer      *container);

#endif
