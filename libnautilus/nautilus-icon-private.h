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

#ifndef NAUTILUS_ICON_CONTAINER_PRIVATE_H
#define NAUTILUS_ICON_CONTAINER_PRIVATE_H

#include "nautilus-icon-container.h"
#include "nautilus-icon-dnd.h"
#include "nautilus-icon-factory.h"
#include "nautilus-icon-canvas-item.h"

/* An Icon. */

typedef struct {
	/* Object represented by this icon. */
	NautilusIconData *data;

	/* Canvas item for the icon. */
	NautilusIconCanvasItem *item;

	/* X/Y coordinates. */
	double x, y;
	
	/* Scale factor (stretches icon). */
	double scale_x, scale_y;

	/* Whether this item is selected. */
	gboolean is_selected : 1;

	/* Whether this item was selected before rubberbanding. */
	gboolean was_selected_before_rubberband : 1;

	/* Grid space occupied by this icon. */
	ArtIRect grid_rectangle;
} NautilusIcon;



/* Private NautilusIconContainer members. */

typedef struct {
	gboolean active;

	double start_x, start_y;

	GnomeCanvasItem *selection_rectangle;

	guint timer_id;

	guint prev_x, prev_y;
	ArtDRect prev_rect;
} NautilusIconRubberbandInfo;

typedef enum {
	DRAG_ACTION_MOVE_OR_COPY,
	DRAG_ACTION_STRETCH
} DragAction;

typedef struct {
	/* Pointer position in canvas coordinates. */
	int pointer_x, pointer_y;

	/* Icon top, left, and size in canvas coordinates. */
	int icon_x, icon_y;
	guint icon_size;
} StretchState;

typedef enum {
	AXIS_NONE,
	AXIS_HORIZONTAL,
	AXIS_VERTICAL
} Axis;

typedef struct NautilusIconGrid NautilusIconGrid;

struct NautilusIconContainerDetails {
	/* single-click mode setting */
	gboolean single_click_mode;
	
	/* List of icons. */
	GList *icons;

	/* The grid. */
	NautilusIconGrid *grid;

	/* Current icon for keyboard navigation. */
	NautilusIcon *keyboard_focus;

	/* Current icon with stretch handles, so we have only one. */
	NautilusIcon *stretch_icon;

	/* Rubberbanding status. */
	NautilusIconRubberbandInfo rubberband_info;

	/* Timeout used to make a selected icon fully visible after a short
	 * period of time. (The timeout is needed to make sure
	 * double-clicking still works.)
	 */
	guint keyboard_icon_reveal_timer_id;
	NautilusIcon *keyboard_icon_to_reveal;

	/* Remembered information about the start of the current event. */
	guint32 button_down_time;

	/* Drag state. Valid only if drag_button is non-zero. */
	guint drag_button;
	NautilusIcon *drag_icon;
	int drag_x, drag_y;
	DragAction drag_action;
	gboolean drag_started;
	StretchState stretch_start;

	/* Idle ID. */
	guint idle_id;

	/* Timeout for selection in browser mode. */
	guint linger_selection_mode_timer_id;

	/* Icon to be selected at timeout in browser mode. */
	NautilusIcon *linger_selection_mode_icon;

	/* DnD info. */
	NautilusIconDndInfo *dnd_info;

	/* zoom level */
	int zoom_level;
	
	/* default fonts used to draw labels */
	GdkFont *label_font[NAUTILUS_ZOOM_LEVEL_LARGEST + 1];
	GdkFont *hilite_font[NAUTILUS_ZOOM_LEVEL_LARGEST + 1];
	
	/* State used so arrow keys don't wander if icons aren't lined up.
	 * Keeps track of last axis arrow key was used on.
	 */
	Axis arrow_key_axis;
	int arrow_key_start;
};

/* Private functions shared by mutiple files. */
NautilusIcon *nautilus_icon_container_get_icon_by_uri             (NautilusIconContainer *container,
								   const char            *uri);
void          nautilus_icon_container_move_icon                   (NautilusIconContainer *container,
								   NautilusIcon          *icon,
								   int                    x,
								   int                    y,
								   double                 scale_x,
								   double                 scale_y,
								   gboolean               raise);
void          nautilus_icon_container_select_list_unselect_others (NautilusIconContainer *container,
								   GList                 *icons);
char *        nautilus_icon_container_get_icon_uri                (NautilusIconContainer *container,
								   NautilusIcon          *icon);

#endif /* NAUTILUS_ICON_CONTAINER_PRIVATE_H */
