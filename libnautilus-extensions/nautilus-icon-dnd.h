/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-dnd.h - Drag & drop handling for the icon container widget.

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

   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Darin Adler <darin@eazel.com>,
	    Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_ICON_DND_H
#define NAUTILUS_ICON_DND_H

#include "nautilus-icon-container.h"

#include <gtk/gtkdnd.h>

typedef struct NautilusIconDndInfo NautilusIconDndInfo;
typedef enum NautilusIconDndTargetType NautilusIconDndTargetType;

/* Standard DnD types. */
enum NautilusIconDndTargetType {
	NAUTILUS_ICON_DND_GNOME_ICON_LIST,
	NAUTILUS_ICON_DND_URI_LIST,
	NAUTILUS_ICON_DND_URL,
	NAUTILUS_ICON_DND_COLOR,
	NAUTILUS_ICON_DND_BGIMAGE
};

/* DnD target names. */
#define NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE "special/x-gnome-icon-list"
#define NAUTILUS_ICON_DND_URI_LIST_TYPE        "text/uri-list"
#define NAUTILUS_ICON_DND_URL_TYPE	       "_NETSCAPE_URL"
#define NAUTILUS_ICON_DND_COLOR_TYPE           "application/x-color"
#define NAUTILUS_ICON_DND_BGIMAGE_TYPE         "property/bgimage"

/* DnD-related information. */
struct NautilusIconDndInfo {
	GtkTargetList *target_list;

	/* Stuff saved at "receive data" time needed later in the drag. */
	gboolean got_data_type;
	NautilusIconDndTargetType data_type;
	GtkSelectionData *selection_data;

	/* Start of the drag, in world coordinates. */
	gdouble start_x, start_y;

	/* List of DndSelectionItems, representing items being dragged, or NULL
	 * if data about them has not been received from the source yet.
	 */
	GList *selection_list;

	/* Stipple for drawing icon shadows during DnD.  */
	GdkBitmap *stipple;

	/* Shadow for the icons being dragged.  */
	GnomeCanvasItem *shadow;
};

void nautilus_icon_dnd_init       (NautilusIconContainer *container,
				   GdkBitmap             *stipple);
void nautilus_icon_dnd_fini       (NautilusIconContainer *container);
void nautilus_icon_dnd_begin_drag (NautilusIconContainer *container,
				   GdkDragAction          actions,
				   gint                   button,
				   GdkEventMotion        *event);
void nautilus_icon_dnd_end_drag   (NautilusIconContainer *container);

void nautilus_icon_dnd_update_drop_action (GtkWidget 	  *widget);

#endif /* NAUTILUS_ICON_DND_H */
