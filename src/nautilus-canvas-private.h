/* gnome-canvas-container-private.h

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
   see <http://www.gnu.org/licenses/>.

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#pragma once

#include <eel/eel-glib-extensions.h>
#include "nautilus-canvas-item.h"
#include "nautilus-canvas-container.h"
#include "nautilus-canvas-dnd.h"

/* An Icon. */

typedef struct {
	NautilusFile *file;

	/* Canvas item for the icon. */
	NautilusCanvasItem *item;

	/* X/Y coordinates. */
	double x, y;

	/*
	 * In RTL mode x is RTL x position, we use saved_ltr_x for
	 * keeping track of x value before it gets converted into
	 * RTL value, this is used for saving the icon position 
	 * to the nautilus metafile. 
	 */
	 double saved_ltr_x;

	/* Position in the view */
	int position;

	/* Whether this item is selected. */
	eel_boolean_bit is_selected : 1;

	/* Whether this item was selected before rubberbanding. */
	eel_boolean_bit was_selected_before_rubberband : 1;

	/* Whether this item is visible in the view. */
	eel_boolean_bit is_visible : 1;
} NautilusCanvasIcon;


/* Private NautilusCanvasContainer members. */

typedef struct {
	gboolean active;

	double start_x;
    double start_y;

    double prev_x;
    double prev_y;

    double x;
    double y;

	EelCanvasItem *selection_rectangle;

	guint timer_id;

	int last_adj_x;
	int last_adj_y;
} NautilusCanvasRubberbandInfo;

struct NautilusCanvasContainerDetails {
	/* List of icons. */
	GList *icons;
	GList *new_icons;
	GList *selection;
	GHashTable *icon_set;

	/* Currently focused icon for accessibility. */
	NautilusCanvasIcon *focus;
	gboolean keyboard_focus;

	/* Starting icon for keyboard rubberbanding. */
	NautilusCanvasIcon *keyboard_rubberband_start;

	/* Last highlighted drop target. */
	NautilusCanvasIcon *drop_target;

	/* Rubberbanding status. */
	NautilusCanvasRubberbandInfo rubberband_info;

	/* Timeout used to make a selected icon fully visible after a short
	 * period of time. (The timeout is needed to make sure
	 * double-clicking still works.)
	 */
	guint keyboard_icon_reveal_timer_id;
	NautilusCanvasIcon *keyboard_icon_to_reveal;

	/* Used to coalesce selection changed signals in some cases */
	guint selection_changed_id;
	
	/* If a request is made to reveal an unpositioned icon we remember
	 * it and reveal it once it gets positioned (in relayout).
	 */
	NautilusCanvasIcon *pending_icon_to_reveal;

	/* Remembered information about the start of the current event. */
	guint32 button_down_time;
	
	/* Drag state. Valid only if drag_button is non-zero. */
	guint drag_button;
	NautilusCanvasIcon *drag_icon;
	gdouble drag_x;
    gdouble drag_y;
	gboolean drag_started;

	gboolean icon_selected_on_button_down;
	gboolean double_clicked;
	NautilusCanvasIcon *double_click_icon[2]; /* Both clicks in a double click need to be on the same icon */
	guint double_click_button[2];

	NautilusCanvasIcon *range_selection_base_icon;
	
	/* Idle ID. */
	guint idle_id;

	/* Align idle id */
	guint align_idle_id;

	/* DnD info. */
	NautilusCanvasDndInfo *dnd_info;
	NautilusDragInfo *dnd_source_info;

	/* zoom level */
	int zoom_level;

	/* specific fonts used to draw labels */
	char *font;
	
	/* State used so arrow keys don't wander if icons aren't lined up.
	 */
	int arrow_key_start_x;
	int arrow_key_start_y;
	GtkDirectionType arrow_key_direction;

	/* Mode settings. */
	gboolean single_click_mode;

        /* Set to TRUE after first allocation has been done */
	gboolean has_been_allocated;

	/* a11y items used by canvas items */
	guint a11y_item_action_idle_handler;
	GQueue* a11y_item_action_queue;

	eel_boolean_bit in_layout_now : 1;
	eel_boolean_bit is_loading : 1;
	eel_boolean_bit is_populating_container : 1;
	eel_boolean_bit needs_resort : 1;
	eel_boolean_bit selection_needs_resort : 1;
};

/* Private functions shared by mutiple files. */
NautilusCanvasIcon *nautilus_canvas_container_get_icon_by_uri             (NautilusCanvasContainer *container,
									 const char            *uri);
void          nautilus_canvas_container_select_list_unselect_others (NautilusCanvasContainer *container,
								     GList                 *icons);
char *        nautilus_canvas_container_get_icon_uri                (NautilusCanvasContainer *container,
								       NautilusCanvasIcon          *canvas);
char *        nautilus_canvas_container_get_icon_activation_uri     (NautilusCanvasContainer *container,
								     NautilusCanvasIcon          *canvas);
char *        nautilus_canvas_container_get_icon_drop_target_uri    (NautilusCanvasContainer *container,
								       NautilusCanvasIcon          *canvas);
void          nautilus_canvas_container_update_icon                 (NautilusCanvasContainer *container,
								       NautilusCanvasIcon          *canvas);
gboolean      nautilus_canvas_container_scroll                      (NautilusCanvasContainer *container,
								     int                    delta_x,
								     int                    delta_y);
void          nautilus_canvas_container_update_scroll_region        (NautilusCanvasContainer *container);
