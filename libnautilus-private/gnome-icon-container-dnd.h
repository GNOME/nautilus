/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-dnd.h - Drag & drop handling for the icon container
   widget.

   Copyright (C) 1999 Free Software Foundation

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

#ifndef _GNOME_ICON_CONTAINER_DND_H
#define _GNOME_ICON_CONTAINER_DND_H

typedef struct _GnomeIconContainerDndInfo GnomeIconContainerDndInfo;
typedef enum _GnomeIconContainerDndTargetType GnomeIconContainerDndTargetType;

#include "gnome-icon-container.h"

/* Standard DnD types.  */
enum _GnomeIconContainerDndTargetType {
	GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST,
	GNOME_ICON_CONTAINER_DND_URI_LIST,
	GNOME_ICON_CONTAINER_DND_URL,
	GNOME_ICON_CONTAINER_DND_NTARGETS
};

/* DnD target names.  */
#define GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST_TYPE "special/x-gnome-icon-list"
#define GNOME_ICON_CONTAINER_DND_URI_LIST_TYPE 	      "text/uri-list"
#define GNOME_ICON_CONTAINER_DND_URL_TYPE	      "_NETSCAPE_URL"


/* DnD-related information.  */
struct _GnomeIconContainerDndInfo {
	GtkTargetList *target_list;

	/* Start of the drag, in world coordinates.  */
	gdouble start_x, start_y;

	/* List of DndSelectionItems, representing items being dragged, or NULL
           if data about them has not been received from the source yet.  */
	GList *selection_list;

	/* Stipple for drawing icon shadows during DnD.  */
	GdkBitmap *stipple;

	/* Shadow for the icons being dragged.  */
	GnomeCanvasItem *shadow;
};


void 	gnome_icon_container_dnd_init		(GnomeIconContainer *container,
						 GdkBitmap *stipple);
void	gnome_icon_container_dnd_fini		(GnomeIconContainer *container);
void	gnome_icon_container_dnd_begin_drag	(GnomeIconContainer *container,
						 GdkDragAction actions,
						 gint button,
						 GdkEventMotion *event);
void	gnome_icon_container_dnd_end_drag	(GnomeIconContainer *container);

#endif /* _GNOME_ICON_CONTAINER_DND_H */
