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

#include "nautilus-entry.h"
#include <eel/eel-glib-extensions.h>
#include "nautilus-icon-container.h"
#include "nautilus-icon-dnd.h"
#include "nautilus-icon-factory.h"
#include "nautilus-icon-canvas-item.h"
#include "nautilus-icon-text-item.h"

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
	eel_boolean_bit is_selected : 1;

	/* Whether this item was selected before rubberbanding. */
	eel_boolean_bit was_selected_before_rubberband : 1;
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
	DRAG_STATE_INITIAL,
	DRAG_STATE_MOVE_OR_COPY,
	DRAG_STATE_MOVE_COPY_OR_MENU,
	DRAG_STATE_STRETCH
} DragState;

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

typedef struct {
	char *type_select_pattern;
	guint64 last_typeselect_time;
} TypeSelectState;

struct NautilusIconContainerDetails {
	/* List of icons. */
	GList *icons;
	GList *new_icons;
	GHashTable *icon_set;

	/* Current icon for keyboard navigation. */
	NautilusIcon *keyboard_focus;

	/* Current icon with stretch handles, so we have only one. */
	NautilusIcon *stretch_icon;
	double stretch_initial_x, stretch_initial_y;
	guint stretch_initial_size;
	
	/* Last highlighted drop target. */
	NautilusIcon *drop_target;

	/* Rubberbanding status. */
	NautilusIconRubberbandInfo rubberband_info;

	/* Timeout used to make a selected icon fully visible after a short
	 * period of time. (The timeout is needed to make sure
	 * double-clicking still works.)
	 */
	guint keyboard_icon_reveal_timer_id;
	NautilusIcon *keyboard_icon_to_reveal;

	/* If a request is made to reveal an unpositioned icon we remember
	 * it and reveal it once it gets positioned (in relayout).
	 */
	NautilusIcon *pending_icon_to_reveal;

	/* If a request is made to rename an unpositioned icon we remember
	 * it and start renaming it once it gets positioned (in relayout).
	 */
	NautilusIcon *pending_icon_to_rename;

	/* Remembered information about the start of the current event. */
	guint32 button_down_time;
	
	/* Drag state. Valid only if drag_button is non-zero. */
	guint drag_button;
	NautilusIcon *drag_icon;
	int drag_x, drag_y;
	DragState drag_state;
	gboolean drag_started;
	StretchState stretch_start;
	int context_menu_timeout_id;

	/* Renaming Details */
	gboolean renaming;
	NautilusIconTextItem *rename_widget;	/* Editable text item */
	char *original_text;			/* Copy of editable text for later compare */

	/* typeahead selection state */
	TypeSelectState *type_select_state;
	
	/* Idle ID. */
	guint idle_id;

	/* Idle handler for stretch code */
	guint stretch_idle_id;

	/* DnD info. */
	NautilusIconDndInfo *dnd_info;

	/* zoom level */
	int zoom_level;
	
	/* fonts used to draw labels in regular mode */
	GdkFont *label_font[NAUTILUS_ZOOM_LEVEL_LARGEST + 1];

	/* font used to draw labels in smooth mode */
	EelScalableFont *smooth_label_font;
	int font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1];

	/* pixbuf and color for label highlighting */
	GdkPixbuf *highlight_frame;
	guint32 highlight_color;
	
	/* color for text labels */
	guint32 label_color;
	guint32 label_info_color;
	
	/* State used so arrow keys don't wander if icons aren't lined up.
	 * Keeps track of last axis arrow key was used on.
	 */
	Axis arrow_key_axis;
	int arrow_key_start;

	/* Mode settings. */
	gboolean single_click_mode;
	gboolean auto_layout;
	gboolean tighter_layout;
	
	/* Layout mode */
	NautilusIconLayoutMode layout_mode;

	/* Set to TRUE after first allocation has been done */
	gboolean has_been_allocated;
	
	/* Is the container fixed or resizable */
	gboolean is_fixed_size;

	/* Ignore the visible area the next time the scroll region is recomputed */
	gboolean reset_scroll_region_trigger;
	
	/* The position we are scaling to on stretch */
	int window_x;
	int window_y;

	/* margins to follow, used for the desktop panel avoidance */
	int left_margin;
	int right_margin;
	int top_margin;
	int bottom_margin;
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
								   gboolean               raise,
								   gboolean		  update_position);
void          nautilus_icon_container_select_list_unselect_others (NautilusIconContainer *container,
								   GList                 *icons);
char *        nautilus_icon_container_get_icon_uri                (NautilusIconContainer *container,
								   NautilusIcon          *icon);
char *        nautilus_icon_container_get_icon_drop_target_uri    (NautilusIconContainer *container,
								   NautilusIcon          *icon);
void          nautilus_icon_container_update_icon                 (NautilusIconContainer *container,
								   NautilusIcon          *icon);
void          nautilus_icon_container_flush_typeselect_state      (NautilusIconContainer *container);
gboolean      nautilus_icon_container_has_stored_icon_positions   (NautilusIconContainer *container);
gboolean      nautilus_icon_container_emit_preview_signal         (NautilusIconContainer *view,
								   NautilusIcon          *icon,
								   gboolean               start_flag);
gboolean      nautilus_icon_container_scroll                      (NautilusIconContainer *container,
								   int                    delta_x,
								   int                    delta_y);
void          nautilus_icon_container_update_scroll_region        (NautilusIconContainer *container);

/* label color for items */
guint32       nautilus_icon_container_get_label_color             (NautilusIconContainer *container,
								   gboolean               first_line);

#endif /* NAUTILUS_ICON_CONTAINER_PRIVATE_H */
