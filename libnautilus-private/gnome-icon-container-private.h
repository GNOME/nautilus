/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-private.h

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

#ifndef GNOME_ICON_CONTAINER_PRIVATE_H
#define GNOME_ICON_CONTAINER_PRIVATE_H

#include "gnome-icon-container.h"
#include "gnome-icon-container-dnd.h"
#include <libgnomeui/gnome-icon-item.h>

/* An Icon.  */

struct _GnomeIconContainerIcon {
	/* Group containing the text and the image.  */
	GnomeCanvasItem *item;

	/* X/Y coordinates and size.  We could use the GnomeCanvasItem
           functions, but this is a lot faster.  */
	gdouble x, y;
	guint width, height;	
        
	/* Whether this item is selected (i.e. highlighted) for operation.  */
	gboolean is_selected : 1;

	/* Whether this item is selected for keyboard navigation.  */
	gboolean is_current : 1;

	/* Whether this item has been repositioned during layout already.  */
	gboolean layout_done : 1;

	/* Whether this item was selected before rubberbanding.  */
	gboolean was_selected_before_rubberband : 1;

	NautilusControllerIcon *data;
};
typedef struct _GnomeIconContainerIcon GnomeIconContainerIcon;


#define INITIAL_GRID_WIDTH 64
#define INITIAL_GRID_HEIGHT 64
struct _GnomeIconContainerIconGrid {
	/* Size of the grid.  */
	guint width, height;

	/* This is the width that we can actually use for finding an empty
           position.  */
	guint visible_width;

	/* Array of grid elements.  */
	GList **elems;

	/* Size of the allocated array.  */
	guint alloc_width, alloc_height;

	/* Position of the first free cell (used to speed up progressive
	   updates).  If negative, there is no free cell.  */
	gint first_free_x, first_free_y;
};
typedef struct _GnomeIconContainerIconGrid GnomeIconContainerIconGrid;


/* Private GnomeIconContainer members.  */

struct _GnomeIconContainerRubberbandInfo {
	gboolean active : 1;

	gdouble start_x, start_y;

	GnomeCanvasItem *selection_rectangle;
	guint timer_tag;

	guint prev_x, prev_y;
	guint prev_x1, prev_y1;
	guint prev_x2, prev_y2;
};
typedef struct _GnomeIconContainerRubberbandInfo GnomeIconContainerRubberbandInfo;

struct _GnomeIconContainerDetails {
	NautilusIconsController *controller;

	/* linger selection mode setting. */
	gboolean linger_selection_mode : 1;

	/* single-click mode setting */
	gboolean single_click_mode : 1;
	
	/* Size of the container.  */
	guint width, height;

	/* List of icons.  */
	GList *icons;
	guint num_icons;

	/* The grid.  */
	GnomeIconContainerIconGrid *grid;

	/* FIXME: This is *ugly*, but more efficient (both memory- and
           speed-wise) than using gtk_object_{set,get}_data() for all the
           icon items.  */
	GHashTable *canvas_item_to_icon;

	/* Rectangle that shows that a certain icon is selected.  */
	GnomeCanvasItem *kbd_navigation_rectangle;

	/* Current icon for keyboard navigation.  */
	GnomeIconContainerIcon *kbd_current;

	/* Rubberbanding status. */
	GnomeIconContainerRubberbandInfo rubberband_info;

	/* Timeout used to make a selected icon fully visible after a short
           period of time.  (The timeout is needed to make sure
           double-clicking still works.)  */
	int kbd_icon_visibility_timer_tag;

        /* the time the mouse button went down in milliseconds */
        guint32 button_down_time;
        
	/* Position of the pointer during the last click.  */
	int drag_x, drag_y;

	/* Button currently pressed, possibly for dragging.  */
	guint drag_button;

	/* Icon on which the click happened.  */
	GnomeIconContainerIcon *drag_icon;

	/* Whether we are actually performing a dragging action.  */
	gboolean doing_drag;

	/* Idle ID.  */
	guint idle_id;

	/* Timeout for selection in browser mode.  */
	int linger_selection_mode_timer_tag;

	/* Icon to be selected at timeout in browser mode.  */
	GnomeIconContainerIcon *linger_selection_mode_icon;

	/* DnD info.  */
	GnomeIconContainerDndInfo *dnd_info;

        /* default font used to draw labels */
        GdkFont *label_font;
};

/* Layout and icon size constants.
   These will change based on the zoom level eventually, so they
   should probably become function calls instead of macros.
*/

#define GNOME_ICON_CONTAINER_CELL_WIDTH(container)     80
#define GNOME_ICON_CONTAINER_CELL_HEIGHT(container)    80

#define GNOME_ICON_CONTAINER_CELL_SPACING(container)    4

#define GNOME_ICON_CONTAINER_ICON_WIDTH(container)     48
#define GNOME_ICON_CONTAINER_ICON_HEIGHT(container)    48

GnomeIconContainerIcon *gnome_icon_container_get_icon_by_uri (GnomeIconContainer *container,
							      const char *uri);
void gnome_icon_container_move_icon (GnomeIconContainer *container,
				     GnomeIconContainerIcon *icon,
				     int x, int y, gboolean raise);
void gnome_icon_container_select_list_unselect_others (GnomeIconContainer *container,
						       GList *icons);

#endif /* GNOME_ICON_CONTAINER_PRIVATE_H */
