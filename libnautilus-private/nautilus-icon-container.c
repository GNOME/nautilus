/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-container.c - Icon container widget.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2002, 2003 Red Hat, Inc.
   
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
   Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include <math.h>
#include "nautilus-icon-container.h"

#include "nautilus-debug-log.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-private.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-marshal.h"
#include <atk/atkaction.h>
#include <eel/eel-accessibility.h>
#include <eel/eel-background.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-art-extensions.h>
#include <eel/eel-editable-label.h>
#include <eel/eel-marshal.h>
#include <eel/eel-string.h>
#include <eel/eel-canvas-rect-ellipse.h>
#include <libgnomeui/gnome-icon-item.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkaccessible.h>
#include <gtk/gtklayout.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-macros.h>
#include <stdio.h>
#include <string.h>

#define TAB_NAVIGATION_DISABLED

/* Interval for updating the rubberband selection, in milliseconds.  */
#define RUBBERBAND_TIMEOUT_INTERVAL 10

/* Initial unpositioned icon value */
#define ICON_UNPOSITIONED_VALUE -1

/* Timeout for making the icon currently selected for keyboard operation visible.
 * If this is 0, you can get into trouble with extra scrolling after holding
 * down the arrow key for awhile when there are many items.
 */
#define KEYBOARD_ICON_REVEAL_TIMEOUT 10

#define CONTEXT_MENU_TIMEOUT_INTERVAL 500

/* Maximum amount of milliseconds the mouse button is allowed to stay down
 * and still be considered a click.
 */
#define MAX_CLICK_TIME 1500

/* Button assignments. */
#define DRAG_BUTTON 1
#define RUBBERBAND_BUTTON 1
#define MIDDLE_BUTTON 2
#define CONTEXTUAL_MENU_BUTTON 3
#define DRAG_MENU_BUTTON 2

/* Maximum size (pixels) allowed for icons at the standard zoom level. */
#define MINIMUM_IMAGE_SIZE 24
#define MAXIMUM_IMAGE_SIZE 96

#define ICON_PAD_LEFT 4
#define ICON_PAD_RIGHT 4
#define ICON_BASE_WIDTH 96

#define ICON_PAD_TOP 4
#define ICON_PAD_BOTTOM 4

#define CONTAINER_PAD_LEFT 4
#define CONTAINER_PAD_RIGHT 4
#define CONTAINER_PAD_TOP 4
#define CONTAINER_PAD_BOTTOM 4

#define STANDARD_ICON_GRID_WIDTH 155

#define TEXT_BESIDE_ICON_GRID_WIDTH 205

/* Desktop layout mode defines */
#define DESKTOP_PAD_HORIZONTAL 	10
#define DESKTOP_PAD_VERTICAL 	10
#define SNAP_SIZE_X 		78
#define SNAP_SIZE_Y 		20

/* Value used to protect against icons being dragged outside of the desktop bounds */
#define DESKTOP_ICON_SAFETY_PAD 10

#define DEFAULT_SELECTION_BOX_ALPHA 0x40
#define DEFAULT_HIGHLIGHT_ALPHA 0xff
#define DEFAULT_NORMAL_ALPHA 0xff
#define DEFAULT_PRELIGHT_ALPHA 0xff
#define DEFAULT_LIGHT_INFO_COLOR 0xAAAAFD
#define DEFAULT_DARK_INFO_COLOR  0x33337F

#define DEFAULT_NORMAL_ICON_RENDER_MODE 0
#define DEFAULT_PRELIGHT_ICON_RENDER_MODE 1
#define DEFAULT_NORMAL_ICON_SATURATION 255
#define DEFAULT_PRELIGHT_ICON_SATURATION 255
#define DEFAULT_NORMAL_ICON_BRIGHTNESS 255
#define DEFAULT_PRELIGHT_ICON_BRIGHTNESS 255
#define DEFAULT_NORMAL_ICON_LIGHTEN 0
#define DEFAULT_PRELIGHT_ICON_LIGHTEN 0

#define MINIMUM_EMBEDDED_TEXT_RECT_WIDTH       20
#define MINIMUM_EMBEDDED_TEXT_RECT_HEIGHT      20

/* If icon size is bigger than this, request large embedded text.
 * Its selected so that the non-large text should fit in "normal" icon sizes
 */
#define ICON_SIZE_FOR_LARGE_EMBEDDED_TEXT 55

/* From nautilus-icon-canvas-item.c */
#define MAX_TEXT_WIDTH_BESIDE 90

#define SNAP_HORIZONTAL(func,x) ((func ((double)((x) - DESKTOP_PAD_HORIZONTAL) / SNAP_SIZE_X) * SNAP_SIZE_X) + DESKTOP_PAD_HORIZONTAL)
#define SNAP_VERTICAL(func, y) ((func ((double)((y) - DESKTOP_PAD_VERTICAL) / SNAP_SIZE_Y) * SNAP_SIZE_Y) + DESKTOP_PAD_VERTICAL)

#define SNAP_NEAREST_HORIZONTAL(x) SNAP_HORIZONTAL (eel_round, x)
#define SNAP_NEAREST_VERTICAL(y) SNAP_VERTICAL (eel_round, y)

#define SNAP_CEIL_HORIZONTAL(x) SNAP_HORIZONTAL (ceil, x)
#define SNAP_CEIL_VERTICAL(y) SNAP_VERTICAL (ceil, y)

/* Copied from NautilusIconContainer */
#define NAUTILUS_ICON_CONTAINER_SEARCH_DIALOG_TIMEOUT 5000

enum {
	ACTION_ACTIVATE,
	ACTION_MENU,
	LAST_ACTION
};

typedef struct {
	GList *selection;
	char *action_descriptions[LAST_ACTION];
} NautilusIconContainerAccessiblePrivate;

static GType         nautilus_icon_container_accessible_get_type (void);

static void          activate_selected_items                        (NautilusIconContainer *container);
static void          activate_selected_items_alternate              (NautilusIconContainer *container,
								     NautilusIcon          *icon);
static void          nautilus_icon_container_theme_changed          (gpointer               user_data);
static void          compute_stretch                                (StretchState          *start,
								     StretchState          *current);
static NautilusIcon *get_first_selected_icon                        (NautilusIconContainer *container);
static NautilusIcon *get_nth_selected_icon                          (NautilusIconContainer *container,
								     int                    index);
static gboolean      has_multiple_selection                         (NautilusIconContainer *container);
static gboolean      all_selected                                   (NautilusIconContainer *container);
static gboolean      has_selection                                  (NautilusIconContainer *container);
static void          icon_destroy                                   (NautilusIconContainer *container,
								     NautilusIcon          *icon);
static void          end_renaming_mode                              (NautilusIconContainer *container,
								     gboolean               commit);
static NautilusIcon *get_icon_being_renamed                         (NautilusIconContainer *container);
static void          finish_adding_new_icons                        (NautilusIconContainer *container);
static void          update_label_color                             (EelBackground         *background,
								     NautilusIconContainer *icon_container);
static void          icon_get_bounding_box                          (NautilusIcon          *icon,
								     int                   *x1_return,
								     int                   *y1_return,
								     int                   *x2_return,
								     int                   *y2_return);
static gboolean      is_renaming                                    (NautilusIconContainer *container);
static gboolean      is_renaming_pending                            (NautilusIconContainer *container);
static void          process_pending_icon_to_rename                 (NautilusIconContainer *container);
static void          setup_label_gcs                                (NautilusIconContainer *container);
static void          nautilus_icon_container_stop_monitor_top_left  (NautilusIconContainer *container,
								     NautilusIconData      *data,
								     gconstpointer          client);
static void          nautilus_icon_container_start_monitor_top_left (NautilusIconContainer *container,
								     NautilusIconData      *data,
								     gconstpointer          client,
								     gboolean               large_text);
static void          handle_vadjustment_changed                     (GtkAdjustment         *adjustment,
								     NautilusIconContainer *container);
static GList *       nautilus_icon_container_get_selected_icons (NautilusIconContainer *container);
static void          nautilus_icon_container_update_visible_icons   (NautilusIconContainer *container);
static void          reveal_icon                                    (NautilusIconContainer *container,
								     NautilusIcon *icon);

static void	     nautilus_icon_container_set_rtl_positions (NautilusIconContainer *container);
static double	     get_mirror_x_position                     (NautilusIconContainer *container,
								NautilusIcon *icon,
								double x);

static gpointer accessible_parent_class;

static GQuark accessible_private_data_quark = 0;

static const char *nautilus_icon_container_accessible_action_names[] = {
	"activate",
	"menu",
	NULL
};

static const char *nautilus_icon_container_accessible_action_descriptions[] = {
	"Activate selected items",
	"Popup context menu",
	NULL
};

GNOME_CLASS_BOILERPLATE (NautilusIconContainer, nautilus_icon_container,
			 EelCanvas, EEL_TYPE_CANVAS)

/* The NautilusIconContainer signals.  */
enum {
	ACTIVATE,
	ACTIVATE_ALTERNATE,
	BAND_SELECT_STARTED,
	BAND_SELECT_ENDED,
	BUTTON_PRESS,
	CAN_ACCEPT_ITEM,
	CONTEXT_CLICK_BACKGROUND,
	CONTEXT_CLICK_SELECTION,
	MIDDLE_CLICK,
	GET_CONTAINER_URI,
	GET_ICON_URI,
	GET_ICON_DROP_TARGET_URI,
	GET_STORED_ICON_POSITION,
	ICON_POSITION_CHANGED,
	ICON_TEXT_CHANGED,
	ICON_STRETCH_STARTED,
	ICON_STRETCH_ENDED,
	RENAMING_ICON,
	LAYOUT_CHANGED,
	MOVE_COPY_ITEMS,
	HANDLE_NETSCAPE_URL,
	HANDLE_URI_LIST,
	HANDLE_TEXT,
	PREVIEW,
	SELECTION_CHANGED,
	ICON_ADDED,
	ICON_REMOVED,
	CLEARED,
	START_INTERACTIVE_SEARCH,
	LAST_SIGNAL
};

typedef struct {
	int **icon_grid;
	int *grid_memory;
	int num_rows;
	int num_columns;
	gboolean tight;
} PlacementGrid;

static guint signals[LAST_SIGNAL];

/* Functions dealing with NautilusIcons.  */

static void
icon_free (NautilusIcon *icon)
{
	/* Destroy this canvas item; the parent will unref it. */
	gtk_object_destroy (GTK_OBJECT (icon->item));
	g_free (icon);
}

static gboolean
icon_is_positioned (const NautilusIcon *icon)
{
	return icon->x != ICON_UNPOSITIONED_VALUE && icon->y != ICON_UNPOSITIONED_VALUE;
}

static void
icon_set_position (NautilusIcon *icon,
		   double x, double y)
{	
	NautilusIconContainer *container;
	double pixels_per_unit;	
	int left, top, right, bottom;
	int width;
	int container_x, container_y, container_width, container_height;
	EelDRect icon_bounds;

	if (icon->x == x && icon->y == y) {
		return;
	}

	container = NAUTILUS_ICON_CONTAINER (EEL_CANVAS_ITEM (icon->item)->canvas);

	if (icon == get_icon_being_renamed (container)) {
		end_renaming_mode (container, TRUE);
	}

	if (nautilus_icon_container_get_is_fixed_size (container)) {
		/*  FIXME: This should be:
		    
		container_x = GTK_WIDGET (container)->allocation.x;
		container_y = GTK_WIDGET (container)->allocation.y;
		container_width = GTK_WIDGET (container)->allocation.width;
		container_height = GTK_WIDGET (container)->allocation.height;

		But for some reason the widget allocation is sometimes not done
		at startup, and the allocation is then only 45x60. which is
		really bad.

		For now, we have a cheesy workaround:
		*/
		container_x = 0;
		container_y = 0;
		container_width = gdk_screen_width ();
		container_height = gdk_screen_height ();
		pixels_per_unit = EEL_CANVAS (container)->pixels_per_unit;
		/* Clip the position of the icon within our desktop bounds */
		left = container_x / pixels_per_unit;
		top =  container_y / pixels_per_unit;
		right = left + container_width / pixels_per_unit;
		bottom = top + container_height / pixels_per_unit;

		icon_bounds = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
		width = icon_bounds.x1 - icon_bounds.x0;
				
		if (nautilus_icon_container_is_layout_rtl(container)) {
			if (x + width < left + DESKTOP_ICON_SAFETY_PAD) {
				x = left + DESKTOP_ICON_SAFETY_PAD - width;
			}
			if (x + width > right) {
				x = right - width;
			}
		} else {
			if (x > right - DESKTOP_ICON_SAFETY_PAD) {
				x = right - DESKTOP_ICON_SAFETY_PAD;
			}
			if (x < left) {
				x = left;
			}
		}
		if (y > bottom - DESKTOP_ICON_SAFETY_PAD) {
			y = bottom - DESKTOP_ICON_SAFETY_PAD;
		}
		if (y < top) {
			y = top;
		}		
	}

	if (icon->x == ICON_UNPOSITIONED_VALUE) {
		icon->x = 0;
	}
	if (icon->y == ICON_UNPOSITIONED_VALUE) {
		icon->y = 0;
	}
	
	eel_canvas_item_move (EEL_CANVAS_ITEM (icon->item),
				x - icon->x,
				y - icon->y);

	icon->x = x;
	icon->y = y;
}

static void
icon_get_size (NautilusIconContainer *container,
	       NautilusIcon *icon,
	       guint *size)
{
	if (size != NULL) {
		*size = MAX (nautilus_get_icon_size_for_zoom_level (container->details->zoom_level)
			       * icon->scale, NAUTILUS_ICON_SIZE_SMALLEST);
	}
}

/* The icon_set_size function is used by the stretching user
 * interface, which currently stretches in a way that keeps the aspect
 * ratio. Later we might have a stretching interface that stretches Y
 * separate from X and we will change this around.
 */
static void
icon_set_size (NautilusIconContainer *container,
	       NautilusIcon *icon,
	       guint icon_size,
	       gboolean snap,
	       gboolean update_position)
{
	guint old_size;
	double scale;

	icon_get_size (container, icon, &old_size);
	if (icon_size == old_size) {
		return;
	}

	scale = (double) icon_size /
		nautilus_get_icon_size_for_zoom_level
		(container->details->zoom_level);
	nautilus_icon_container_move_icon (container, icon,
					   icon->x, icon->y,
					   scale, FALSE,
					   snap, update_position);
}

static void
icon_raise (NautilusIcon *icon)
{
	EelCanvasItem *item, *band;
	
	item = EEL_CANVAS_ITEM (icon->item);
	band = NAUTILUS_ICON_CONTAINER (item->canvas)->details->rubberband_info.selection_rectangle;
	
	eel_canvas_item_send_behind (item, band);
}

static void
emit_stretch_started (NautilusIconContainer *container, NautilusIcon *icon)
{
	g_signal_emit (container,
			 signals[ICON_STRETCH_STARTED], 0,
			 icon->data);
}

static void
emit_stretch_ended (NautilusIconContainer *container, NautilusIcon *icon)
{
	g_signal_emit (container,
			 signals[ICON_STRETCH_ENDED], 0,
			 icon->data);
}

static void
icon_toggle_selected (NautilusIconContainer *container,
		      NautilusIcon *icon)
{		
	end_renaming_mode (container, TRUE);

	icon->is_selected = !icon->is_selected;
	eel_canvas_item_set (EEL_CANVAS_ITEM (icon->item),
			     "highlighted_for_selection", (gboolean) icon->is_selected,
			     NULL);

	/* If the icon is deselected, then get rid of the stretch handles.
	 * No harm in doing the same if the item is newly selected.
	 */
	if (icon == container->details->stretch_icon) {
		container->details->stretch_icon = NULL;
		nautilus_icon_canvas_item_set_show_stretch_handles (icon->item, FALSE);
		/* snap the icon if necessary */
		if (container->details->keep_aligned) {
			nautilus_icon_container_move_icon (container,
							   icon,
							   icon->x, icon->y,
							   icon->scale,
							   FALSE, TRUE, TRUE);
		}
		
		emit_stretch_ended (container, icon);
	}

	/* Raise each newly-selected icon to the front as it is selected. */
	if (icon->is_selected) {
		icon_raise (icon);
	}
}

/* Select an icon. Return TRUE if selection has changed. */
static gboolean
icon_set_selected (NautilusIconContainer *container,
		   NautilusIcon *icon,
		   gboolean select)
{
	g_assert (select == FALSE || select == TRUE);
	g_assert (icon->is_selected == FALSE || icon->is_selected == TRUE);

	if (select == icon->is_selected) {
		return FALSE;
	}

	icon_toggle_selected (container, icon);
	g_assert (select == icon->is_selected);
	return TRUE;
}

static void
icon_get_bounding_box (NautilusIcon *icon,
		       int *x1_return, int *y1_return,
		       int *x2_return, int *y2_return)
{
	double x1, y1, x2, y2;

	eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon->item),
				      &x1, &y1, &x2, &y2);

	*x1_return = x1;
	*y1_return = y1;
	*x2_return = x2;
	*y2_return = y2;
}

/* Utility functions for NautilusIconContainer.  */

gboolean
nautilus_icon_container_scroll (NautilusIconContainer *container,
				int delta_x, int delta_y)
{
	GtkAdjustment *hadj, *vadj;
	int old_h_value, old_v_value;

	hadj = gtk_layout_get_hadjustment (GTK_LAYOUT (container));
	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (container));

	/* Store the old ajustment values so we can tell if we
	 * ended up actually scrolling. We may not have in a case
	 * where the resulting value got pinned to the adjustment
	 * min or max.
	 */
	old_h_value = hadj->value;
	old_v_value = vadj->value;
	
	eel_gtk_adjustment_set_value (hadj, hadj->value + delta_x);
	eel_gtk_adjustment_set_value (vadj, vadj->value + delta_y);

	/* return TRUE if we did scroll */
	return hadj->value != old_h_value || vadj->value != old_v_value;
}

static void
pending_icon_to_reveal_destroy_callback (NautilusIconCanvasItem *item,
					 NautilusIconContainer *container)
{
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (container->details->pending_icon_to_reveal != NULL);
	g_assert (container->details->pending_icon_to_reveal->item == item);

	container->details->pending_icon_to_reveal = NULL;
}

static NautilusIcon*
get_pending_icon_to_reveal (NautilusIconContainer *container)
{
	return container->details->pending_icon_to_reveal;
}

static void
set_pending_icon_to_reveal (NautilusIconContainer *container, NautilusIcon *icon)
{
	NautilusIcon *old_icon;
	
	old_icon = container->details->pending_icon_to_reveal;
	
	if (icon == old_icon) {
		return;
	}
	
	if (old_icon != NULL) {
		g_signal_handlers_disconnect_by_func
			(old_icon->item,
			 G_CALLBACK (pending_icon_to_reveal_destroy_callback),
			 container);
	}
	
	if (icon != NULL) {
		g_signal_connect (icon->item, "destroy",
				  G_CALLBACK (pending_icon_to_reveal_destroy_callback),
				  container);
	}
	
	container->details->pending_icon_to_reveal = icon;
}

static void
item_get_canvas_bounds (EelCanvasItem *item, EelIRect *bounds)
{
	EelDRect world_rect;
	
	eel_canvas_item_get_bounds (item,
				    &world_rect.x0,
				    &world_rect.y0,
				    &world_rect.x1,
				    &world_rect.y1);
	eel_canvas_item_i2w (item->parent,
			     &world_rect.x0,
			     &world_rect.y0);
	eel_canvas_item_i2w (item->parent,
			     &world_rect.x1,
			     &world_rect.y1);
	eel_canvas_w2c (item->canvas,
			world_rect.x0,
			world_rect.y0,
			&bounds->x0,
			&bounds->y0);
	eel_canvas_w2c (item->canvas,
			world_rect.x1,
			world_rect.y1,
			&bounds->x1,
			&bounds->y1);
}

static void
reveal_icon (NautilusIconContainer *container,
	     NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;
	EelIRect bounds;

	if (!icon_is_positioned (icon)) {
		set_pending_icon_to_reveal (container, icon);
		return;
	}
	
	set_pending_icon_to_reveal (container, NULL);

	details = container->details;
	allocation = &GTK_WIDGET (container)->allocation;

	hadj = gtk_layout_get_hadjustment (GTK_LAYOUT (container));
	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (container));

	item_get_canvas_bounds (EEL_CANVAS_ITEM (icon->item), &bounds);
	if (bounds.y0 < vadj->value) {
		eel_gtk_adjustment_set_value (vadj, bounds.y0);
	} else if (bounds.y1 > vadj->value + allocation->height) {
		eel_gtk_adjustment_set_value
			(vadj, bounds.y1 - allocation->height);
	}

	if (bounds.x0 < hadj->value) {
		eel_gtk_adjustment_set_value (hadj, bounds.x0);
	} else if (bounds.x1 > hadj->value + allocation->width) {
		eel_gtk_adjustment_set_value
			(hadj, bounds.x1 - allocation->width);
	}
}

static void
process_pending_icon_to_reveal (NautilusIconContainer *container)
{
	NautilusIcon *pending_icon_to_reveal;
	
	pending_icon_to_reveal = get_pending_icon_to_reveal (container);
	
	if (pending_icon_to_reveal != NULL) {
		reveal_icon (container, pending_icon_to_reveal);
	}
}

static gboolean
keyboard_icon_reveal_timeout_callback (gpointer data)
{
	NautilusIconContainer *container;
	NautilusIcon *icon;

	container = NAUTILUS_ICON_CONTAINER (data);
	icon = container->details->keyboard_icon_to_reveal;

	g_assert (icon != NULL);

	/* Only reveal the icon if it's still the keyboard focus or if
	 * it's still selected. Someone originally thought we should
	 * cancel this reveal if the user manages to sneak a direct
	 * scroll in before the timeout fires, but we later realized
	 * this wouldn't actually be an improvement 
	 * (see bugzilla.gnome.org 40612).
	 */
	if (icon == container->details->keyboard_focus
	    || icon->is_selected) {
		reveal_icon (container, icon);
	}
	container->details->keyboard_icon_reveal_timer_id = 0;

	return FALSE;
}

static void
unschedule_keyboard_icon_reveal (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;

	details = container->details;

	if (details->keyboard_icon_reveal_timer_id != 0) {
		g_source_remove (details->keyboard_icon_reveal_timer_id);
	}
}

static void
schedule_keyboard_icon_reveal (NautilusIconContainer *container,
			       NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;

	details = container->details;

	unschedule_keyboard_icon_reveal (container);

	details->keyboard_icon_to_reveal = icon;
	details->keyboard_icon_reveal_timer_id
		= g_timeout_add (KEYBOARD_ICON_REVEAL_TIMEOUT,
				 keyboard_icon_reveal_timeout_callback,
				 container);
}

static void
clear_keyboard_focus (NautilusIconContainer *container)
{
        if (container->details->keyboard_focus != NULL) {
		eel_canvas_item_set (EEL_CANVAS_ITEM (container->details->keyboard_focus->item),
				       "highlighted_as_keyboard_focus", 0,
				       NULL);
	}
	
	container->details->keyboard_focus = NULL;
}

static void inline
emit_atk_focus_tracker_notify (NautilusIcon *icon)
{
	AtkObject *atk_object = eel_accessibility_for_object (icon->item);
	atk_focus_tracker_notify (atk_object);
}

/* Set @icon as the icon currently selected for keyboard operations. */
static void
set_keyboard_focus (NautilusIconContainer *container,
		    NautilusIcon *icon)
{
	g_assert (icon != NULL);

	if (icon == container->details->keyboard_focus) {
		return;
	}

	clear_keyboard_focus (container);

	container->details->keyboard_focus = icon;

	eel_canvas_item_set (EEL_CANVAS_ITEM (container->details->keyboard_focus->item),
			       "highlighted_as_keyboard_focus", 1,
			       NULL);

	emit_atk_focus_tracker_notify (icon);
}

static void
set_keyboard_rubberband_start (NautilusIconContainer *container,
			       NautilusIcon *icon)
{
	container->details->keyboard_rubberband_start = icon;
}

static void
clear_keyboard_rubberband_start (NautilusIconContainer *container)
{
	container->details->keyboard_rubberband_start = NULL;
}

static void
get_all_icon_bounds (NautilusIconContainer *container,
		     double *x1, double *y1,
		     double *x2, double *y2)
{
	/* FIXME bugzilla.gnome.org 42477: Do we have to do something about the rubberband
	 * here? Any other non-icon items?
	 */
	eel_canvas_item_get_bounds
		(EEL_CANVAS (container)->root,
		 x1, y1, x2, y2);
}

/* Don't preserve visible white space the next time the scroll region
 * is recomputed when the container is not empty. */
void
nautilus_icon_container_reset_scroll_region (NautilusIconContainer *container)
{
	container->details->reset_scroll_region_trigger = TRUE;
}

/* Set a new scroll region without eliminating any of the currently-visible area. */
static void
canvas_set_scroll_region_include_visible_area (EelCanvas *canvas,
					       double x1, double y1,
					       double x2, double y2)
{
	double old_x1, old_y1, old_x2, old_y2;
	double old_scroll_x, old_scroll_y;
	double height, width;
	
	eel_canvas_get_scroll_region (canvas, &old_x1, &old_y1, &old_x2, &old_y2);

	width = (GTK_WIDGET (canvas)->allocation.width) / canvas->pixels_per_unit;
	height = (GTK_WIDGET (canvas)->allocation.height) / canvas->pixels_per_unit;

	old_scroll_x = gtk_layout_get_hadjustment (GTK_LAYOUT (canvas))->value;
	old_scroll_y = gtk_layout_get_vadjustment (GTK_LAYOUT (canvas))->value;

	x1 = MIN (x1, old_x1 + old_scroll_x);
	y1 = MIN (y1, old_y1 + old_scroll_y);
	x2 = MAX (x2, old_x1 + old_scroll_x + width);
	y2 = MAX (y2, old_y1 + old_scroll_y + height);

	eel_canvas_set_scroll_region
		(canvas, x1, y1, x2, y2);
}

void
nautilus_icon_container_update_scroll_region (NautilusIconContainer *container)
{
	double x1, y1, x2, y2;
	double pixels_per_unit;
	GtkAdjustment *hadj, *vadj;
	float step_increment;
	GtkAllocation *allocation;
	gboolean reset_scroll_region;

	if (nautilus_icon_container_get_is_fixed_size (container)) {
		pixels_per_unit = EEL_CANVAS (container)->pixels_per_unit;
		
		/* Set the scroll region to the size of the container allocation */
		allocation = &GTK_WIDGET (container)->allocation;
		eel_canvas_set_scroll_region
			(EEL_CANVAS (container),
			 (double) - container->details->left_margin / pixels_per_unit,
			 (double) - container->details->top_margin / pixels_per_unit,
			 ((double) (allocation->width - 1)
			 - container->details->left_margin
			 - container->details->right_margin)
			 / pixels_per_unit,
			 ((double) (allocation->height - 1)
			 - container->details->top_margin
			 - container->details->bottom_margin)
			 / pixels_per_unit);
		return;
	}

	reset_scroll_region = container->details->reset_scroll_region_trigger
		|| nautilus_icon_container_is_empty (container)
		|| nautilus_icon_container_is_auto_layout (container);
		
	/* The trigger is only cleared when container is non-empty, so
	 * callers can reliably reset the scroll region when an item
	 * is added even if extraneous relayouts are called when the
	 * window is still empty.
	 */
	if (!nautilus_icon_container_is_empty (container)) {
		container->details->reset_scroll_region_trigger = FALSE;
	}

	get_all_icon_bounds (container, &x1, &y1, &x2, &y2);	

	/* Auto-layout assumes a 0, 0 scroll origin */
	if (nautilus_icon_container_is_auto_layout (container)) {
		x1 = 0;
		y1 = 0;
	} else {
		x1 -= CONTAINER_PAD_LEFT;
		y1 -= CONTAINER_PAD_TOP;
	}

	x2 -= 1;
	x2 = MAX(x1, x2);

	y2 -= 1;
	y2 = MAX(y1, y2);

	/* for horizontal layouts, we add a bottom border.
	 *
	 * vertical layout is used by the compact view,
	 * which does not need a bottom border.
	 */
	if (nautilus_icon_container_is_layout_vertical (container)) {
		x2 += CONTAINER_PAD_RIGHT;
	} else {
		y2 += CONTAINER_PAD_BOTTOM;
	}

	if (reset_scroll_region) {
		eel_canvas_set_scroll_region
			(EEL_CANVAS (container),
			 x1, y1, x2, y2);
	} else {
		canvas_set_scroll_region_include_visible_area
			(EEL_CANVAS (container),
			 x1, y1, x2, y2);
	}

	hadj = gtk_layout_get_hadjustment (GTK_LAYOUT (container));
	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (container));

	/* Scroll by 1/4 icon each time you click. */
	step_increment = nautilus_get_icon_size_for_zoom_level
		(container->details->zoom_level) / 4;
	if (hadj->step_increment != step_increment) {
		hadj->step_increment = step_increment;
		gtk_adjustment_changed (hadj);
	}
	if (vadj->step_increment != step_increment) {
		vadj->step_increment = step_increment;
		gtk_adjustment_changed (vadj);
	}

	/* Now that we have a new scroll region, clamp the
         * adjustments so we are within the valid scroll area.
	 */
	eel_gtk_adjustment_clamp_value (hadj);
	eel_gtk_adjustment_clamp_value (vadj);

	/*
	 * In RTL mode, when displayed force horizontal scrollbar to the 
	 * right side.
	 */

	if (nautilus_icon_container_is_layout_rtl(container))
		gtk_adjustment_set_value (hadj, hadj->upper - hadj->page_size);
}

static int
compare_icons (gconstpointer a, gconstpointer b, gpointer icon_container)
{
	NautilusIconContainerClass *klass;
	const NautilusIcon *icon_a, *icon_b;

	icon_a = a;
	icon_b = b;
	klass  = NAUTILUS_ICON_CONTAINER_GET_CLASS (icon_container);

	return klass->compare_icons (icon_container, icon_a->data, icon_b->data);
}

static void
sort_icons (NautilusIconContainer *container,
	    GList                **icons)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->compare_icons != NULL);

	*icons = g_list_sort_with_data (*icons, compare_icons, container);
}

static void
resort (NautilusIconContainer *container)
{
	sort_icons (container, &container->details->icons);
}

#if 0
static double
get_grid_width (NautilusIconContainer *container)
{
	if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
		return TEXT_BESIDE_ICON_GRID_WIDTH;
	} else {
		return STANDARD_ICON_GRID_WIDTH;
	}
}
#endif
typedef struct {
	double width;
	double height;
	double x_offset;
	double y_offset;
} IconPositions;

static void
lay_down_one_line (NautilusIconContainer *container,
		   GList *line_start,
		   GList *line_end,
		   double y,
		   double max_height,
		   GArray *positions)
{
	GList *p;
	NautilusIcon *icon;
	double x, y_offset;
	IconPositions *position;
	int i;
	gboolean is_rtl;

	is_rtl = nautilus_icon_container_is_layout_rtl (container);

	/* Lay out the icons along the baseline. */
	x = ICON_PAD_LEFT;
	i = 0;
	for (p = line_start; p != line_end; p = p->next) {
		icon = p->data;

		position = &g_array_index (positions, IconPositions, i++);
		
		if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
			y_offset = (max_height - position->height) / 2;
		} else {
			y_offset = position->y_offset;
		}

		icon_set_position
			(icon,
			 is_rtl ? get_mirror_x_position (container, icon, x + position->x_offset) : x + position->x_offset,
			 y + y_offset);

		icon->saved_ltr_x = is_rtl ? get_mirror_x_position (container, icon, icon->x) : icon->x;

		x += position->width;
	}
}

static void
lay_down_one_column (NautilusIconContainer *container,
		     GList *line_start,
		     GList *line_end,
		     double x,
		     double y_start,
		     double y_iter,
		     GArray *positions)
{
	GList *p;
	NautilusIcon *icon;
	double y;
	IconPositions *position;
	int i;

	/* FIXME: Should layout differently when in RTL base mode */

	/* Lay out the icons along the baseline. */
	y = y_start;
	i = 0;
	for (p = line_start; p != line_end; p = p->next) {
		icon = p->data;

		position = &g_array_index (positions, IconPositions, i++);

		icon_set_position
			(icon,
			 x + position->x_offset,
			 y + position->y_offset);

		y += y_iter;
	}
}

static void
lay_down_icons_horizontal (NautilusIconContainer *container,
			   GList *icons,
			   double start_y)
{
	GList *p, *line_start;
	NautilusIcon *icon;
	double canvas_width, y, canvas_height;
	GArray *positions;
	IconPositions *position;
	EelDRect bounds;
	EelDRect icon_bounds;
	EelDRect text_bounds;
	EelCanvasItem *item;
	double max_height_above, max_height_below;
	double height_above, height_below;
	double line_width;
	gboolean gridded_layout;
	double grid_width;
	double max_text_width, max_icon_width;
	int icon_width;
	int i;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));

	if (icons == NULL) {
		return;
	}

	positions = g_array_new (FALSE, FALSE, sizeof (IconPositions));
	
	/* Lay out icons a line at a time. */
	canvas_width = CANVAS_WIDTH(container);
	canvas_height = CANVAS_HEIGHT(container);

	max_icon_width = max_text_width = 0.0;

	if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
		/* Would it be worth caching these bounds for the next loop? */
		for (p = icons; p != NULL; p = p->next) {
			icon = p->data;

			icon_bounds = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
			max_icon_width = MAX (max_icon_width, ceil (icon_bounds.x1 - icon_bounds.x0));

			text_bounds = nautilus_icon_canvas_item_get_text_rectangle (icon->item);
			max_text_width = MAX (max_text_width, ceil (text_bounds.x1 - text_bounds.x0));
		}

		grid_width = max_icon_width + max_text_width + ICON_PAD_LEFT + ICON_PAD_RIGHT;
	} else {
		grid_width = STANDARD_ICON_GRID_WIDTH;
	}

	gridded_layout = !nautilus_icon_container_is_tighter_layout (container);
	
	line_width = container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE ? ICON_PAD_LEFT : 0;
	line_start = icons;
	y = start_y;
	i = 0;
	
	max_height_above = 0;
	max_height_below = 0;
	for (p = icons; p != NULL; p = p->next) {
		icon = p->data;

		/* Get the width of the icon. */
		item = EEL_CANVAS_ITEM (icon->item);
		
		/* Assume it's only one level hierarchy to avoid costly affine calculations */
		eel_canvas_item_get_bounds (item,
					    &bounds.x0, &bounds.y0,
					    &bounds.x1, &bounds.y1);

		icon_bounds = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
		text_bounds = nautilus_icon_canvas_item_get_text_rectangle (icon->item);

		if (gridded_layout) {
			icon_width = ceil ((bounds.x1 - bounds.x0)/grid_width) * grid_width;


		} else {
			icon_width = (bounds.x1 - bounds.x0) + ICON_PAD_RIGHT + 8; /* 8 pixels extra for fancy selection box */
		}		
		
		/* Calculate size above/below baseline */
		height_above = icon_bounds.y1 - bounds.y0;
		height_below = bounds.y1 - icon_bounds.y1;

		/* If this icon doesn't fit, it's time to lay out the line that's queued up. */
		if (line_start != p && line_width + icon_width >= canvas_width ) {
			if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
				y += ICON_PAD_TOP;
			} else {
				/* Advance to the baseline. */
				y += ICON_PAD_TOP + max_height_above;
			}

			lay_down_one_line (container, line_start, p, y, max_height_above, positions);
			
			if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
				y += max_height_above + max_height_below + ICON_PAD_BOTTOM;
			} else {
				/* Advance to next line. */
				y += max_height_below + ICON_PAD_BOTTOM;
			}
			
			line_width = container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE ? ICON_PAD_LEFT : 0;
			line_start = p;
			i = 0;
			
			max_height_above = height_above;
			max_height_below = height_below;
		} else {
			if (height_above > max_height_above) {
				max_height_above = height_above;
			}
			if (height_below > max_height_below) {
				max_height_below = height_below;
			}
		}
		
		g_array_set_size (positions, i + 1);
		position = &g_array_index (positions, IconPositions, i++);
		position->width = icon_width;
		position->height = icon_bounds.y1 - icon_bounds.y0;

		if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
			if (gridded_layout) {
				position->x_offset = max_icon_width + ICON_PAD_LEFT + ICON_PAD_RIGHT - (icon_bounds.x1 - icon_bounds.x0);
			} else {
				position->x_offset = icon_width - ((icon_bounds.x1 - icon_bounds.x0) + (text_bounds.x1 - text_bounds.x0));
			}
			position->y_offset = 0;
		} else {
			position->x_offset = (icon_width - (icon_bounds.x1 - icon_bounds.x0)) / 2;
			position->y_offset = icon_bounds.y0 - icon_bounds.y1;
		}

		/* Add this icon. */
		line_width += icon_width;
	}

	/* Lay down that last line of icons. */
	if (line_start != NULL) {
			if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
				y += ICON_PAD_TOP;
			} else {
				/* Advance to the baseline. */
				y += ICON_PAD_TOP + max_height_above;
			}
		
		lay_down_one_line (container, line_start, NULL, y, max_height_above, positions);
		
		/* Advance to next line. */
		y += max_height_below + ICON_PAD_BOTTOM;
	}

	g_array_free (positions, TRUE);
}

static void
get_max_icon_dimensions (GList *icon_start,
			 GList *icon_end,
			 double *max_icon_width,
			 double *max_icon_height,
			 double *max_text_width,
			 double *max_text_height,
			 double *max_bounds_height)
{
	NautilusIcon *icon;
	EelDRect icon_bounds;
	EelDRect text_bounds;
	GList *p;
	double y1, y2;

	*max_icon_width = *max_text_width = 0.0;
	*max_icon_height = *max_text_height = 0.0;
	*max_bounds_height = 0.0;

	/* Would it be worth caching these bounds for the next loop? */
	for (p = icon_start; p != icon_end; p = p->next) {
		icon = p->data;

		icon_bounds = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
		*max_icon_width = MAX (*max_icon_width, ceil (icon_bounds.x1 - icon_bounds.x0));
		*max_icon_height = MAX (*max_icon_height, ceil (icon_bounds.y1 - icon_bounds.y0));

		text_bounds = nautilus_icon_canvas_item_get_text_rectangle (icon->item);
		*max_text_width = MAX (*max_text_width, ceil (text_bounds.x1 - text_bounds.x0));
		*max_text_height = MAX (*max_text_height, ceil (text_bounds.y1 - text_bounds.y0));

		eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon->item), NULL, &y1, NULL, &y2);
		*max_bounds_height = MAX (*max_bounds_height, y2 - y1);
	}
}

/* column-wise layout. At the moment, this only works with label-beside-icon (used by "Compact View"). */
static void
lay_down_icons_vertical (NautilusIconContainer *container,
			 GList *icons,
			 double start_y)
{
	GList *p, *line_start;
	NautilusIcon *icon;
	double canvas_width, x, canvas_height;
	GArray *positions;
	IconPositions *position;
	EelDRect icon_bounds;
	EelDRect text_bounds;
	EelCanvasItem *item;

	double line_height;

	double max_height;
	double max_height_with_borders;
	double max_width;
	double max_width_in_column;

	double max_bounds_height;
	double max_bounds_height_with_borders;

	double max_text_width, max_icon_width;
	double max_text_height, max_icon_height;
	int height;
	int i;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE);

	if (icons == NULL) {
		return;
	}

	positions = g_array_new (FALSE, FALSE, sizeof (IconPositions));

	/* Lay out icons a column at a time. */
	canvas_width = CANVAS_WIDTH(container);
	canvas_height = CANVAS_HEIGHT(container);

	max_icon_width = max_text_width = 0.0;
	max_icon_height = max_text_height = 0.0;
	max_bounds_height = 0.0;

	get_max_icon_dimensions (icons, NULL,
				 &max_icon_width, &max_icon_height,
				 &max_text_width, &max_text_height,
				 &max_bounds_height);

	max_width = max_icon_width + max_text_width;
	max_height = MAX (max_icon_height, max_text_height);
	max_height_with_borders = ICON_PAD_TOP + max_height;

	max_bounds_height_with_borders = ICON_PAD_TOP + max_bounds_height;

	line_height = ICON_PAD_TOP;
	line_start = icons;
	x = 0;
	i = 0;

	max_width_in_column = 0.0;

	for (p = icons; p != NULL; p = p->next) {
		icon = p->data;
		item = EEL_CANVAS_ITEM (icon->item);

		/* If this icon doesn't fit, it's time to lay out the column that's queued up. */

		/* We use the bounds height here, since for wrapping we also want to consider
		 * overlapping emblems at the bottom. We may wrap a little bit too early since
		 * the icon with the max. bounds height may actually not be in the last row, but
		 * it is better than visual glitches
		 */
		if (line_start != p && line_height + (max_bounds_height_with_borders-1) >= canvas_height ) {
			x += ICON_PAD_LEFT;

			/* correctly set (per-column) width */
			if (!container->details->all_columns_same_width) {
				for (i = 0; i < (int) positions->len; i++) {
					position = &g_array_index (positions, IconPositions, i);
					position->width = max_width_in_column;
				}
			}

			lay_down_one_column (container, line_start, p, x, CONTAINER_PAD_TOP, max_height_with_borders, positions);

			/* Advance to next column. */
			if (container->details->all_columns_same_width) {
				x += max_width + ICON_PAD_RIGHT;
			} else {
				x += max_width_in_column + ICON_PAD_RIGHT;
			}

			line_height = ICON_PAD_TOP;
			line_start = p;
			i = 0;

			max_width_in_column = 0;
		}

		icon_bounds = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
		text_bounds = nautilus_icon_canvas_item_get_text_rectangle (icon->item);

		max_width_in_column = MAX (max_width_in_column,
					   ceil (icon_bounds.x1 - icon_bounds.x0) +
					   ceil (text_bounds.x1 - text_bounds.x0));

		g_array_set_size (positions, i + 1);
		position = &g_array_index (positions, IconPositions, i++);
		if (container->details->all_columns_same_width) {
			position->width = max_width;
		}
		position->height = max_height;
		position->y_offset = ICON_PAD_TOP;
		position->x_offset = ICON_PAD_LEFT;

		position->x_offset += max_icon_width - ceil (icon_bounds.x1 - icon_bounds.x0);

		height = MAX (ceil (icon_bounds.y1 - icon_bounds.y0), ceil(text_bounds.y1 - text_bounds.y0));
		position->y_offset += (max_height - height) / 2;

		/* Add this icon. */
		line_height += max_height_with_borders;
	}

	/* Lay down that last column of icons. */
	if (line_start != NULL) {
		x += ICON_PAD_LEFT;
		lay_down_one_column (container, line_start, NULL, x, CONTAINER_PAD_TOP, max_height_with_borders, positions);
	}

	g_array_free (positions, TRUE);
}

static void
snap_position (NautilusIconContainer *container,
	       NautilusIcon *icon,
	       int *x, int *y)
{
	int center_x;
	int baseline_y;
	int icon_width;
	int icon_height;
	EelDRect icon_position;
	
	icon_position = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
	icon_width = icon_position.x1 - icon_position.x0;
	icon_height = icon_position.y1 - icon_position.y0;

	if (nautilus_icon_container_is_layout_rtl (container))
	    *x = get_mirror_x_position (container, icon, *x);

	if (*x + icon_width / 2 < DESKTOP_PAD_HORIZONTAL + SNAP_SIZE_X) {
		*x = DESKTOP_PAD_HORIZONTAL + SNAP_SIZE_X - icon_width / 2;
	}

	if (*y + icon_height < DESKTOP_PAD_VERTICAL + SNAP_SIZE_Y) {
		*y = DESKTOP_PAD_VERTICAL + SNAP_SIZE_Y - icon_height;
	}

	center_x = *x + icon_width / 2;
	*x = SNAP_NEAREST_HORIZONTAL (center_x) - (icon_width / 2);
	if (nautilus_icon_container_is_layout_rtl (container)) {
	    *x = get_mirror_x_position (container, icon, *x);
	}


	/* Find the grid position vertically and place on the proper baseline */
	baseline_y = *y + icon_height;
	baseline_y = SNAP_NEAREST_VERTICAL (baseline_y);
	*y = baseline_y - icon_height;
}

static int
compare_icons_by_position (gconstpointer a, gconstpointer b)
{
	NautilusIcon *icon_a, *icon_b;
	int x1, y1, x2, y2;
	int center_a;
	int center_b;

	icon_a = (NautilusIcon*)a;
	icon_b = (NautilusIcon*)b;

	icon_get_bounding_box (icon_a, &x1, &y1, &x2, &y2);
	center_a = x1 + (x2 - x1) / 2;
	icon_get_bounding_box (icon_b, &x1, &y1, &x2, &y2);
	center_b = x1 + (x2 - x1) / 2;

	return center_a == center_b ?
		icon_a->y - icon_b->y :
		center_a - center_b;
}

static PlacementGrid *
placement_grid_new (NautilusIconContainer *container, gboolean tight)
{
	PlacementGrid *grid;
	int width, height;
	int num_columns;
	int num_rows;
	int i;

	/* Get container dimensions */
	width  = CANVAS_WIDTH(container);
	height = CANVAS_HEIGHT(container);

	num_columns = width / SNAP_SIZE_X;
	num_rows = height / SNAP_SIZE_Y;
	
	if (num_columns == 0 || num_rows == 0) {
		return NULL;
	}

	grid = g_new0 (PlacementGrid, 1);
	grid->tight = tight;
	grid->num_columns = num_columns;
	grid->num_rows = num_rows;

	grid->grid_memory = g_new0 (int, (num_rows * num_columns));
	grid->icon_grid = g_new0 (int *, num_columns);
	
	for (i = 0; i < num_columns; i++) {
		grid->icon_grid[i] = grid->grid_memory + (i * num_rows);
	}
	
	return grid;
}

static void
placement_grid_free (PlacementGrid *grid)
{
	g_free (grid->icon_grid);
	g_free (grid->grid_memory);
	g_free (grid);
}

static gboolean
placement_grid_position_is_free (PlacementGrid *grid, EelIRect pos)
{
	int x, y;
	
	g_assert (pos.x0 >= 0 && pos.x0 < grid->num_columns);
	g_assert (pos.y0 >= 0 && pos.y0 < grid->num_rows);
	g_assert (pos.x1 >= 0 && pos.x1 < grid->num_columns);
	g_assert (pos.y1 >= 0 && pos.y1 < grid->num_rows);

	for (x = pos.x0; x <= pos.x1; x++) {
		for (y = pos.y0; y <= pos.y1; y++) {
			if (grid->icon_grid[x][y] != 0) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

static void
placement_grid_mark (PlacementGrid *grid, EelIRect pos)
{
	int x, y;
	
	g_assert (pos.x0 >= 0 && pos.x0 < grid->num_columns);
	g_assert (pos.y0 >= 0 && pos.y0 < grid->num_rows);
	g_assert (pos.x1 >= 0 && pos.x1 < grid->num_columns);
	g_assert (pos.y1 >= 0 && pos.y1 < grid->num_rows);

	for (x = pos.x0; x <= pos.x1; x++) {
		for (y = pos.y0; y <= pos.y1; y++) {
			grid->icon_grid[x][y] = 1;
		}
	}
}

static void
canvas_position_to_grid_position (PlacementGrid *grid,
				  EelIRect canvas_position,
				  EelIRect *grid_position)
{
	/* The first causes minimal moving around during a snap, but
	 * can end up with partially overlapping icons.  The second one won't
	 * allow any overlapping, but can cause more movement to happen 
	 * during a snap. */
	if (grid->tight) {
		grid_position->x0 = ceil ((double)(canvas_position.x0 - DESKTOP_PAD_HORIZONTAL) / SNAP_SIZE_X);
		grid_position->y0 = ceil ((double)(canvas_position.y0 - DESKTOP_PAD_VERTICAL) / SNAP_SIZE_Y);
		grid_position->x1 = floor ((double)(canvas_position.x1 - DESKTOP_PAD_HORIZONTAL) / SNAP_SIZE_X);
		grid_position->y1 = floor ((double)(canvas_position.y1 - DESKTOP_PAD_VERTICAL) / SNAP_SIZE_Y);
	} else {
		grid_position->x0 = floor ((double)(canvas_position.x0 - DESKTOP_PAD_HORIZONTAL) / SNAP_SIZE_X);
		grid_position->y0 = floor ((double)(canvas_position.y0 - DESKTOP_PAD_VERTICAL) / SNAP_SIZE_Y);
		grid_position->x1 = floor ((double)(canvas_position.x1 - DESKTOP_PAD_HORIZONTAL) / SNAP_SIZE_X);
		grid_position->y1 = floor ((double)(canvas_position.y1 - DESKTOP_PAD_VERTICAL) / SNAP_SIZE_Y);
	}

	grid_position->x0 = CLAMP (grid_position->x0, 0, grid->num_columns - 1);
	grid_position->y0 = CLAMP (grid_position->y0, 0, grid->num_rows - 1);
	grid_position->x1 = CLAMP (grid_position->x1, grid_position->x0, grid->num_columns - 1);
	grid_position->y1 = CLAMP (grid_position->y1, grid_position->y0, grid->num_rows - 1);
}

static void
placement_grid_mark_icon (PlacementGrid *grid, NautilusIcon *icon)
{
	EelIRect icon_pos;
	EelIRect grid_pos;
	
	icon_get_bounding_box (icon, 
			       &icon_pos.x0, &icon_pos.y0,
			       &icon_pos.x1, &icon_pos.y1);
	canvas_position_to_grid_position (grid, 
					  icon_pos,
					  &grid_pos);
	placement_grid_mark (grid, grid_pos);
}

static void
find_empty_location (NautilusIconContainer *container,
		     PlacementGrid *grid,
		     NautilusIcon *icon,
		     int start_x,
		     int start_y,
		     int *x, 
		     int *y)
{
	double icon_width, icon_height;
	int canvas_width;
	int canvas_height;
	EelIRect icon_position;
	EelDRect pixbuf_rect;
	gboolean collision;

	/* Get container dimensions */
	canvas_width  = CANVAS_WIDTH(container);
	canvas_height = CANVAS_HEIGHT(container);

	icon_get_bounding_box (icon, 
			       &icon_position.x0, &icon_position.y0, 
			       &icon_position.x1, &icon_position.y1);
	icon_width = icon_position.x1 - icon_position.x0;
	icon_height = icon_position.y1 - icon_position.y0;
	
	pixbuf_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
	
	/* Start the icon on a grid location */
	snap_position (container, icon, &start_x, &start_y);

	icon_position.x0 = start_x;
	icon_position.y0 = start_y;
	icon_position.x1 = icon_position.x0 + icon_width;
	icon_position.y1 = icon_position.y0 + icon_height;

	do {
		EelIRect grid_position;

		collision = FALSE;
		
		canvas_position_to_grid_position (grid,
						  icon_position,
						  &grid_position);

		if (!placement_grid_position_is_free (grid, grid_position)) {
			icon_position.y0 += SNAP_SIZE_Y;
			icon_position.y1 = icon_position.y0 + icon_height;
			
			if (icon_position.y1 + DESKTOP_PAD_VERTICAL > canvas_height) {
				/* Move to the next column */
				icon_position.y0 = DESKTOP_PAD_VERTICAL + SNAP_SIZE_Y - (pixbuf_rect.y1 - pixbuf_rect.y0);
				while (icon_position.y0 < DESKTOP_PAD_VERTICAL) {
					icon_position.y0 += SNAP_SIZE_Y;
				}
				icon_position.y1 = icon_position.y0 + icon_height;
				
				icon_position.x0 += SNAP_SIZE_X;
				icon_position.x1 = icon_position.x0 + icon_width;
			}
				
			collision = TRUE;
		}
	} while (collision && (icon_position.x1 < canvas_width));

	*x = icon_position.x0;
	*y = icon_position.y0;
}

static void
align_icons (NautilusIconContainer *container)
{
	GList *unplaced_icons;
	GList *l;
	PlacementGrid *grid;

	unplaced_icons = g_list_copy (container->details->icons);
	
	unplaced_icons = g_list_sort (unplaced_icons, 
				      compare_icons_by_position);

	if (nautilus_icon_container_is_layout_rtl (container)) {
		unplaced_icons = g_list_reverse (unplaced_icons);
	}

	grid = placement_grid_new (container, TRUE);

	if (!grid) {
		return;
	}

	for (l = unplaced_icons; l != NULL; l = l->next) {
		NautilusIcon *icon;
		int x, y;

		icon = l->data;
		x = icon->saved_ltr_x;
		y = icon->y;
		find_empty_location (container, grid, 
				     icon, x, y, &x, &y);

		icon_set_position (icon, x, y);
		icon->saved_ltr_x = icon->x;
		placement_grid_mark_icon (grid, icon);
	}

	g_list_free (unplaced_icons);

	placement_grid_free (grid);

	if (nautilus_icon_container_is_layout_rtl (container)) {
		nautilus_icon_container_set_rtl_positions (container);
	}
}

static double
get_mirror_x_position (NautilusIconContainer *container, NautilusIcon *icon, double x)
{
	EelDRect icon_bounds;
	
	icon_bounds = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);

	return CANVAS_WIDTH(container) - x - (icon_bounds.x1 - icon_bounds.x0);
}

static void
nautilus_icon_container_set_rtl_positions (NautilusIconContainer *container)
{
	GList *l;
	NautilusIcon *icon;
	double x;

	if (!container->details->icons) {
		return;
	}

	for (l = container->details->icons; l != NULL; l = l->next) {
		icon = l->data;
		x = get_mirror_x_position (container, icon, icon->saved_ltr_x);
		icon_set_position (icon, x, icon->y);
	}
}

static void
lay_down_icons_vertical_desktop (NautilusIconContainer *container, GList *icons)
{
	GList *p, *placed_icons, *unplaced_icons;
	int total, new_length, placed;
	NautilusIcon *icon;
	int width, height, max_width, column_width, icon_width, icon_height;
	int x, y, x1, x2, y1, y2;
	EelDRect icon_rect;

	/* Get container dimensions */
	width  = CANVAS_WIDTH(container);
	height = CANVAS_HEIGHT(container);

	/* Determine which icons have and have not been placed */
	placed_icons = NULL;
	unplaced_icons = NULL;
	
	total = g_list_length (container->details->icons);
	new_length = g_list_length (icons);
	placed = total - new_length;
	if (placed > 0) {
		PlacementGrid *grid;
		/* Add only placed icons in list */
		for (p = container->details->icons; p != NULL; p = p->next) {
			icon = p->data;
			if (icon_is_positioned (icon)) {
				icon_set_position(icon, icon->saved_ltr_x, icon->y);
				placed_icons = g_list_prepend (placed_icons, icon);
			} else {
				icon->x = 0;
				icon->y = 0;
				unplaced_icons = g_list_prepend (unplaced_icons, icon);
			}
		}
		placed_icons = g_list_reverse (placed_icons);
		unplaced_icons = g_list_reverse (unplaced_icons);

		grid = placement_grid_new (container, FALSE);

		if (grid) {
			for (p = placed_icons; p != NULL; p = p->next) {
				placement_grid_mark_icon
					(grid, (NautilusIcon*)p->data);
			}
			
			/* Place unplaced icons in the best locations */
			for (p = unplaced_icons; p != NULL; p = p->next) {
				icon = p->data;
				
				icon_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
				
				/* Start the icon in the first column */
				x = DESKTOP_PAD_HORIZONTAL + (SNAP_SIZE_X / 2) - ((icon_rect.x1 - icon_rect.x0) / 2);
				y = DESKTOP_PAD_VERTICAL + SNAP_SIZE_Y - (icon_rect.y1 - icon_rect.y0);

				find_empty_location (container,
						     grid,
						     icon,
						     x, y,
						     &x, &y);
				
				icon_set_position (icon, x, y);
				icon->saved_ltr_x = x;
				placement_grid_mark_icon (grid, icon);
			}

			placement_grid_free (grid);
		}
		
		g_list_free (placed_icons);
		g_list_free (unplaced_icons);
	} else {
		/* There are no placed icons.  Just lay them down using our rules */		
		x = DESKTOP_PAD_HORIZONTAL;

		while (icons != NULL) {
			int center_x;
			int baseline;
			gboolean should_snap;
			
			should_snap = !(container->details->tighter_layout && !container->details->keep_aligned);
			
			y = DESKTOP_PAD_VERTICAL;

			max_width = 0;
			
			/* Calculate max width for column */
			for (p = icons; p != NULL; p = p->next) {
				icon = p->data;
				icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);
				
				icon_width = x2 - x1;
				icon_height = y2 - y1;

				if (should_snap) {
					/* Snap the baseline to a grid position */
					icon_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
					baseline = y + (icon_rect.y1 - icon_rect.y0);
					baseline = SNAP_CEIL_VERTICAL (baseline);
					y = baseline - (icon_rect.y1 - icon_rect.y0);
				}
				    
				/* Check and see if we need to move to a new column */
				if (y != DESKTOP_PAD_VERTICAL && y > height - icon_height) {
					break;
				}

				if (max_width < icon_width) {
					max_width = icon_width;
				}
				
				y += icon_height + DESKTOP_PAD_VERTICAL;
			}

			y = DESKTOP_PAD_VERTICAL;

			center_x = x + max_width / 2;
			column_width = max_width;
			if (should_snap) {
				/* Find the grid column to center on */
				center_x = SNAP_CEIL_HORIZONTAL (center_x);
				column_width = (center_x - x) + (max_width / 2);
			}
			
			/* Lay out column */
			for (p = icons; p != NULL; p = p->next) {
				icon = p->data;
				icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);
				
				icon_height = y2 - y1;
				
				icon_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);

				if (should_snap) {
					baseline = y + (icon_rect.y1 - icon_rect.y0);
					baseline = SNAP_CEIL_VERTICAL (baseline);
					y = baseline - (icon_rect.y1 - icon_rect.y0);
				}
				
				/* Check and see if we need to move to a new column */
				if (y != DESKTOP_PAD_VERTICAL && y > height - icon_height &&
				    /* Make sure we lay out at least one icon per column, to make progress */
				    p != icons) {
					x += column_width + DESKTOP_PAD_HORIZONTAL;
					break;
				}
				
				icon_set_position (icon,
						   center_x - (icon_rect.x1 - icon_rect.x0) / 2,
						   y);
				
				icon->saved_ltr_x = icon->x;
				y += icon_height + DESKTOP_PAD_VERTICAL;
			}
			icons = p;
		}
	}

	/* These modes are special. We freeze all of our positions
	 * after we do the layout.
	 */
	/* FIXME bugzilla.gnome.org 42478: 
	 * This should not be tied to the direction of layout.
	 * It should be a separate switch.
	 */
	nautilus_icon_container_freeze_icon_positions (container);
}


static void
lay_down_icons (NautilusIconContainer *container, GList *icons, double start_y)
{
	switch (container->details->layout_mode)
	{
	case NAUTILUS_ICON_LAYOUT_L_R_T_B:
	case NAUTILUS_ICON_LAYOUT_R_L_T_B:
		lay_down_icons_horizontal (container, icons, start_y);
		break;
		
	case NAUTILUS_ICON_LAYOUT_T_B_L_R:
	case NAUTILUS_ICON_LAYOUT_T_B_R_L:
		if (nautilus_icon_container_get_is_desktop (container)) {
			lay_down_icons_vertical_desktop (container, icons);
		} else {
			lay_down_icons_vertical (container, icons, start_y);
		}
		break;
		
	default:
		g_assert_not_reached ();
	}
}

static void
redo_layout_internal (NautilusIconContainer *container)
{
	finish_adding_new_icons (container);

	/* Don't do any re-laying-out during stretching. Later we
	 * might add smart logic that does this and leaves room for
	 * the stretched icon, but if we do it we want it to be fast
	 * and only re-lay-out when it's really needed.
	 */
	if (container->details->auto_layout
	    && container->details->drag_state != DRAG_STATE_STRETCH) {
		resort (container);
		lay_down_icons (container, container->details->icons, 0);
	}

	if (nautilus_icon_container_is_layout_rtl (container)) {
		nautilus_icon_container_set_rtl_positions (container);
	}

	nautilus_icon_container_update_scroll_region (container);

	process_pending_icon_to_reveal (container);
	process_pending_icon_to_rename (container);
	nautilus_icon_container_update_visible_icons (container);
}

static gboolean
redo_layout_callback (gpointer callback_data)
{
	NautilusIconContainer *container;

	container = NAUTILUS_ICON_CONTAINER (callback_data);
	redo_layout_internal (container);
	container->details->idle_id = 0;

	return FALSE;
}

static void
unschedule_redo_layout (NautilusIconContainer *container)
{
        if (container->details->idle_id != 0) {
		g_source_remove (container->details->idle_id);
		container->details->idle_id = 0;
	}
}

static void
schedule_redo_layout (NautilusIconContainer *container)
{
	if (container->details->idle_id == 0
	    && container->details->has_been_allocated) {
		container->details->idle_id = g_idle_add
			(redo_layout_callback, container);
	}
}

static void
redo_layout (NautilusIconContainer *container)
{
	unschedule_redo_layout (container);
	redo_layout_internal (container);
}

static void
reload_icon_positions (NautilusIconContainer *container)
{
	GList *p, *no_position_icons;
	NautilusIcon *icon;
	gboolean have_stored_position;
	NautilusIconPosition position;
	EelDRect bounds;
	double bottom;
	EelCanvasItem *item;

	g_assert (!container->details->auto_layout);

	resort (container);

	no_position_icons = NULL;

	/* Place all the icons with positions. */
	bottom = 0;
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		have_stored_position = FALSE;
		g_signal_emit (container,
				 signals[GET_STORED_ICON_POSITION], 0,
				 icon->data,
				 &position,
				 &have_stored_position);
		if (have_stored_position) {
			icon_set_position (icon, position.x, position.y);
			item = EEL_CANVAS_ITEM (icon->item);
			eel_canvas_item_get_bounds (item,
						    &bounds.x0,
						    &bounds.y0,
						    &bounds.x1,
						    &bounds.y1);
			eel_canvas_item_i2w (item->parent,
					     &bounds.x0,
					     &bounds.y0);
			eel_canvas_item_i2w (item->parent,
					     &bounds.x1,
					     &bounds.y1);
			if (bounds.y1 > bottom) {
				bottom = bounds.y1;
			}
		} else {
			no_position_icons = g_list_prepend (no_position_icons, icon);
		}
	}
	no_position_icons = g_list_reverse (no_position_icons);

	/* Place all the other icons. */
	lay_down_icons (container, no_position_icons, bottom + ICON_PAD_BOTTOM);
	g_list_free (no_position_icons);
}

/* Container-level icon handling functions.  */

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

/* invalidate the cached label sizes for all the icons */
static void
invalidate_label_sizes (NautilusIconContainer *container)
{
	GList *p;
	NautilusIcon *icon;
	
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		nautilus_icon_canvas_item_invalidate_label_size (icon->item);		
	}
}

static gboolean
select_range (NautilusIconContainer *container,
	      NautilusIcon *icon1,
	      NautilusIcon *icon2)
{
	gboolean selection_changed;
	GList *p;
	NautilusIcon *icon;
	NautilusIcon *unmatched_icon;
	gboolean select;

	selection_changed = FALSE;

	unmatched_icon = NULL;
	select = FALSE;
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		if (unmatched_icon == NULL) {
			if (icon == icon1) {
				unmatched_icon = icon2;
				select = TRUE;
			} else if (icon == icon2) {
				unmatched_icon = icon1;
				select = TRUE;
			}
		}
		
		selection_changed |= icon_set_selected
			(container, icon, select);

		if (unmatched_icon != NULL && icon == unmatched_icon) {
			select = FALSE;
		}
		
	}
	
	if (selection_changed && icon2 != NULL) {
		emit_atk_focus_tracker_notify (icon2);
	}
	return selection_changed;
}


static gboolean
select_one_unselect_others (NautilusIconContainer *container,
			    NautilusIcon *icon_to_select)
{
	gboolean selection_changed;
	GList *p;
	NautilusIcon *icon;

	selection_changed = FALSE;
	
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		selection_changed |= icon_set_selected
			(container, icon, icon == icon_to_select);
	}
	
	if (selection_changed && icon_to_select != NULL) {
		emit_atk_focus_tracker_notify (icon_to_select);
		reveal_icon (container, icon_to_select);
	}
	return selection_changed;
}

static gboolean
unselect_all (NautilusIconContainer *container)
{
	return select_one_unselect_others (container, NULL);
}

void
nautilus_icon_container_move_icon (NautilusIconContainer *container,
				   NautilusIcon *icon,
				   int x, int y,
				   double scale,
				   gboolean raise,
				   gboolean snap,
				   gboolean update_position)
{
	NautilusIconContainerDetails *details;
	gboolean emit_signal;
	NautilusIconPosition position;
	
	details = container->details;
	
	emit_signal = FALSE;
	
	if (icon == get_icon_being_renamed (container)) {
		end_renaming_mode (container, TRUE);
	}

	if (scale != icon->scale) {
		icon->scale = scale;
		nautilus_icon_container_update_icon (container, icon);
		if (update_position) {
			redo_layout (container); 
			emit_signal = TRUE;
		}
	}

	if (!details->auto_layout) {
		if (details->keep_aligned && snap) {
			snap_position (container, icon, &x, &y);
		}

		if (x != icon->x || y != icon->y) {
			icon_set_position (icon, x, y);
			emit_signal = update_position;
		}

		icon->saved_ltr_x = nautilus_icon_container_is_layout_rtl (container) ? get_mirror_x_position (container, icon, icon->x) : icon->x;
	}
	
	if (emit_signal) {
		position.x = icon->saved_ltr_x;
		position.y = icon->y;
		position.scale = scale;
		g_signal_emit (container,
				 signals[ICON_POSITION_CHANGED], 0,
				 icon->data, &position);
	}
	
	if (raise) {
		icon_raise (icon);
	}

	/* FIXME bugzilla.gnome.org 42474: 
	 * Handling of the scroll region is inconsistent here. In
	 * the scale-changing case, redo_layout is called, which updates the
	 * scroll region appropriately. In other cases, it's up to the
	 * caller to make sure the scroll region is updated. This could
	 * lead to hard-to-track-down bugs.
	 */
}

/* Implementation of rubberband selection.  */
static void
rubberband_select (NautilusIconContainer *container,
		   const EelDRect *previous_rect,
		   const EelDRect *current_rect)
{
	GList *p;
	gboolean selection_changed, is_in, canvas_rect_calculated;
	NautilusIcon *icon;
	EelIRect canvas_rect;
	EelCanvas *canvas;
			
	selection_changed = FALSE;
	canvas_rect_calculated = FALSE;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		
		if (!canvas_rect_calculated) {
			/* Only do this calculation once, since all the canvas items
			 * we are interating are in the same coordinate space
			 */
			canvas = EEL_CANVAS_ITEM (icon->item)->canvas;
			eel_canvas_w2c (canvas,
					current_rect->x0,
					current_rect->y0,
					&canvas_rect.x0,
					&canvas_rect.y0);
			eel_canvas_w2c (canvas,
					current_rect->x1,
					current_rect->y1,
					&canvas_rect.x1,
					&canvas_rect.y1);
			canvas_rect_calculated = TRUE;
		}
		
		is_in = nautilus_icon_canvas_item_hit_test_rectangle (icon->item, canvas_rect);

		selection_changed |= icon_set_selected
			(container, icon,
			 is_in ^ icon->was_selected_before_rubberband);
	}

	if (selection_changed) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

static int
rubberband_timeout_callback (gpointer data)
{
	NautilusIconContainer *container;
	GtkWidget *widget;
	NautilusIconRubberbandInfo *band_info;
	int x, y;
	double x1, y1, x2, y2;
	double world_x, world_y;
	int x_scroll, y_scroll;
	int adj_y;
	gboolean adj_changed;
	
	EelDRect selection_rect;

	widget = GTK_WIDGET (data);
	container = NAUTILUS_ICON_CONTAINER (data);
	band_info = &container->details->rubberband_info;

	g_assert (band_info->timer_id != 0);
	g_assert (EEL_IS_CANVAS_RECT (band_info->selection_rectangle) ||
		  EEL_IS_CANVAS_RECT (band_info->selection_rectangle));

	adj_changed = FALSE;
	adj_y = gtk_adjustment_get_value (gtk_layout_get_vadjustment (GTK_LAYOUT (container)));
	if (adj_y != band_info->last_adj_y) {
		band_info->last_adj_y = adj_y;
		adj_changed = TRUE;
	}

	gtk_widget_get_pointer (widget, &x, &y);

	if (x < 0) {
		x_scroll = x;
		x = 0;
	} else if (x >= widget->allocation.width) {
		x_scroll = x - widget->allocation.width + 1;
		x = widget->allocation.width - 1;
	} else {
		x_scroll = 0;
	}

	if (y < 0) {
		y_scroll = y;
		y = 0;
	} else if (y >= widget->allocation.height) {
		y_scroll = y - widget->allocation.height + 1;
		y = widget->allocation.height - 1;
	} else {
		y_scroll = 0;
	}

	if (y_scroll == 0 && x_scroll == 0
	    && (int) band_info->prev_x == x && (int) band_info->prev_y == y && !adj_changed) {
		return TRUE;
	}

	nautilus_icon_container_scroll (container, x_scroll, y_scroll);

	/* Remember to convert from widget to scrolled window coords */
	eel_canvas_window_to_world (EEL_CANVAS (container),
				    x + gtk_adjustment_get_value (gtk_layout_get_hadjustment (GTK_LAYOUT (container))),
				    y + gtk_adjustment_get_value (gtk_layout_get_vadjustment (GTK_LAYOUT (container))),
				    &world_x, &world_y);

	if (world_x < band_info->start_x) {
		x1 = world_x;
		x2 = band_info->start_x;
	} else {
		x1 = band_info->start_x;
		x2 = world_x;
	}

	if (world_y < band_info->start_y) {
		y1 = world_y;
		y2 = band_info->start_y;
	} else {
		y1 = band_info->start_y;
		y2 = world_y;
	}

	/* Don't let the area of the selection rectangle be empty.
	 * Aside from the fact that it would be funny when the rectangle disappears,
	 * this also works around a crash in libart that happens sometimes when a
	 * zero height rectangle is passed.
	 */
	x2 = MAX (x1 + 1, x2);
	y2 = MAX (y1 + 1, y2);

	eel_canvas_item_set
		(band_info->selection_rectangle,
		 "x1", x1, "y1", y1,
		 "x2", x2, "y2", y2,
		 NULL);

	selection_rect.x0 = x1;
	selection_rect.y0 = y1;
	selection_rect.x1 = x2;
	selection_rect.y1 = y2;

	rubberband_select (container,
			   &band_info->prev_rect,
			   &selection_rect);
	
	band_info->prev_x = x;
	band_info->prev_y = y;

	band_info->prev_rect = selection_rect;

	return TRUE;
}

static void
start_rubberbanding (NautilusIconContainer *container,
		     GdkEventButton *event)
{
	AtkObject *accessible;
	NautilusIconContainerDetails *details;
	NautilusIconRubberbandInfo *band_info;
	guint fill_color, outline_color;
	GdkColor *fill_color_gdk;
	guchar fill_color_alpha;
	GList *p;
	NautilusIcon *icon;

	details = container->details;
	band_info = &details->rubberband_info;

	g_signal_emit (container,
		       signals[BAND_SELECT_STARTED], 0);

	for (p = details->icons; p != NULL; p = p->next) {
		icon = p->data;
		icon->was_selected_before_rubberband = icon->is_selected;
	}

	eel_canvas_window_to_world
		(EEL_CANVAS (container), event->x, event->y,
		 &band_info->start_x, &band_info->start_y);

	gtk_widget_style_get (GTK_WIDGET (container),
			      "selection_box_color", &fill_color_gdk,
			      "selection_box_alpha", &fill_color_alpha,
			      NULL);

	if (!fill_color_gdk) {
		fill_color_gdk = gdk_color_copy (&GTK_WIDGET (container)->style->base[GTK_STATE_SELECTED]);
	}
	
	fill_color = eel_gdk_color_to_rgb (fill_color_gdk) << 8 | fill_color_alpha;

	gdk_color_free (fill_color_gdk);
	
	outline_color = fill_color | 255;
	
	band_info->selection_rectangle = eel_canvas_item_new
		(eel_canvas_root
		 (EEL_CANVAS (container)),
		 EEL_TYPE_CANVAS_RECT,
		 "x1", band_info->start_x,
		 "y1", band_info->start_y,
		 "x2", band_info->start_x,
		 "y2", band_info->start_y,
		 "fill_color_rgba", fill_color,
		 "outline_color_rgba", outline_color,
		 "width_pixels", 1,
		 NULL);

	accessible = atk_gobject_accessible_for_object
		(G_OBJECT (band_info->selection_rectangle));
	atk_object_set_name (accessible, "selection");
	atk_object_set_description (accessible, _("The selection rectangle"));

	band_info->prev_x = event->x - gtk_adjustment_get_value (gtk_layout_get_hadjustment (GTK_LAYOUT (container)));
	band_info->prev_y = event->y - gtk_adjustment_get_value (gtk_layout_get_vadjustment (GTK_LAYOUT (container)));

	band_info->active = TRUE;

	if (band_info->timer_id == 0) {
		band_info->timer_id = g_timeout_add
			(RUBBERBAND_TIMEOUT_INTERVAL,
			 rubberband_timeout_callback,
			 container);
	}

	eel_canvas_item_grab (band_info->selection_rectangle,
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK 
				 | GDK_SCROLL_MASK),
				NULL, event->time);
}

static void
stop_rubberbanding (NautilusIconContainer *container,
		    guint32 time)
{
	NautilusIconRubberbandInfo *band_info;
	GList *icons;

	band_info = &container->details->rubberband_info;

	g_assert (band_info->timer_id != 0);
	g_source_remove (band_info->timer_id);
	band_info->timer_id = 0;

	band_info->active = FALSE;

	/* Destroy this canvas item; the parent will unref it. */
	eel_canvas_item_ungrab (band_info->selection_rectangle, time);
	gtk_object_destroy (GTK_OBJECT (band_info->selection_rectangle));
	band_info->selection_rectangle = NULL;

	/* if only one item has been selected, use it as range
	 * selection base (cf. handle_icon_button_press) */
	icons = nautilus_icon_container_get_selected_icons (container);
	if (g_list_length (icons) == 1) {
		container->details->range_selection_base_icon = icons->data;
	}
	g_list_free (icons);

	g_signal_emit (container,
			 signals[BAND_SELECT_ENDED], 0);
}

/* Keyboard navigation.  */

typedef gboolean (* IsBetterIconFunction) (NautilusIconContainer *container,
					   NautilusIcon *start_icon,
					   NautilusIcon *best_so_far,
					   NautilusIcon *candidate,
					   void *data);

static NautilusIcon *
find_best_icon (NautilusIconContainer *container,
		NautilusIcon *start_icon,
		IsBetterIconFunction function,
		void *data)
{
	GList *p;
	NautilusIcon *best, *candidate;

	best = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		candidate = p->data;

		if (candidate != start_icon) {
			if ((* function) (container, start_icon, best, candidate, data)) {
				best = candidate;
			}
		}
	}
	return best;
}

static NautilusIcon *
find_best_selected_icon (NautilusIconContainer *container,
			 NautilusIcon *start_icon,
			 IsBetterIconFunction function,
			 void *data)
{
	GList *p;
	NautilusIcon *best, *candidate;

	best = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		candidate = p->data;

		if (candidate != start_icon && candidate->is_selected) {
			if ((* function) (container, start_icon, best, candidate, data)) {
				best = candidate;
			}
		}
	}
	return best;
}

static int
compare_icons_by_uri (NautilusIconContainer *container,
		      NautilusIcon *icon_a,
		      NautilusIcon *icon_b)
{
	char *uri_a, *uri_b;
	int result;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (icon_a != NULL);
	g_assert (icon_b != NULL);
	g_assert (icon_a != icon_b);

	uri_a = nautilus_icon_container_get_icon_uri (container, icon_a);
	uri_b = nautilus_icon_container_get_icon_uri (container, icon_b);
	result = strcmp (uri_a, uri_b);
	g_assert (result != 0);
	g_free (uri_a);
	g_free (uri_b);
	
	return result;
}

static int
get_cmp_point_x (NautilusIconContainer *container,
		 EelDRect icon_rect)
{
	if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
		if (gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL) {
			return icon_rect.x0;
		} else {
			return icon_rect.x1;
		}
	} else {
		return (icon_rect.x0 + icon_rect.x1) / 2;
	}
}

static int
get_cmp_point_y (NautilusIconContainer *container,
		 EelDRect icon_rect)
{
	if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
		return (icon_rect.y0 + icon_rect.y1)/2;
	} else {
		return icon_rect.y1;
	}
}

static int
compare_icons_horizontal_first (NautilusIconContainer *container,
				NautilusIcon *icon_a,
				NautilusIcon *icon_b)
{
	EelDRect world_rect;
	int ax, ay, bx, by;

	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon_a->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &ax,
		 &ay);
	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon_b->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &bx,
		 &by);
	
	if (ax < bx) {
		return -1;
	}
	if (ax > bx) {
		return +1;
	}
	if (ay < by) {
		return -1;
	}
	if (ay > by) {
		return +1;
	}
	return compare_icons_by_uri (container, icon_a, icon_b);
}

static int
compare_icons_vertical_first_reverse_horizontal (NautilusIconContainer *container,
						 NautilusIcon *icon_a,
						 NautilusIcon *icon_b)
{
	EelDRect world_rect;
	int ax, ay, bx, by;

	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon_a->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &ax,
		 &ay);
	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon_b->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &bx,
		 &by);
	
	if (ay < by) {
		return -1;
	}
	if (ay > by) {
		return +1;
	}
	if (ax < bx) {
		return (nautilus_icon_container_is_layout_rtl (container) ? -1 : +1);
	}
	if (ax > bx) {
		return (nautilus_icon_container_is_layout_rtl (container) ? +1 : -1);
	}
	return compare_icons_by_uri (container, icon_a, icon_b);
}


static int
compare_icons_vertical_first (NautilusIconContainer *container,
			      NautilusIcon *icon_a,
			      NautilusIcon *icon_b)
{
	EelDRect world_rect;
	int ax, ay, bx, by;

	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon_a->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &ax,
		 &ay);
	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon_b->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &bx,
		 &by);
	
	if (ay < by) {
		return -1;
	}
	if (ay > by) {
		return +1;
	}
	if (ax < bx) {
		return -1;
	}
	if (ax > bx) {
		return +1;
	}
	return compare_icons_by_uri (container, icon_a, icon_b);
}

static gboolean
leftmost_in_top_row (NautilusIconContainer *container,
		     NautilusIcon *start_icon,
		     NautilusIcon *best_so_far,
		     NautilusIcon *candidate,
		     void *data)
{
	if (best_so_far == NULL) {
		return TRUE;
	}
	return compare_icons_vertical_first (container, best_so_far, candidate) > 0;
}

static gboolean
rightmost_in_bottom_row (NautilusIconContainer *container,
			 NautilusIcon *start_icon,
			 NautilusIcon *best_so_far,
			 NautilusIcon *candidate,
			 void *data)
{
	if (best_so_far == NULL) {
		return TRUE;
	}
	return compare_icons_vertical_first (container, best_so_far, candidate) < 0;
}

static int
compare_with_start_row (NautilusIconContainer *container,
			NautilusIcon *icon)
{
	EelCanvasItem *item;

	item = EEL_CANVAS_ITEM (icon->item);
	
	if (container->details->arrow_key_start < item->y1) {
		return -1;
	}
	if (container->details->arrow_key_start > item->y2) {
		return +1;
	}
	return 0;
}

static int
compare_with_start_column (NautilusIconContainer *container,
			   NautilusIcon *icon)
{
	EelCanvasItem *item;

	item = EEL_CANVAS_ITEM (icon->item);
	
	if (container->details->arrow_key_start < item->x1) {
		return -1;
	}
	if (container->details->arrow_key_start > item->x2) {
		return +1;
	}
	return 0;
}

static gboolean
same_row_right_side_leftmost (NautilusIconContainer *container,
			      NautilusIcon *start_icon,
			      NautilusIcon *best_so_far,
			      NautilusIcon *candidate,
			      void *data)
{
	/* Candidates not on the start row do not qualify. */
	if (compare_with_start_row (container, candidate) != 0) {
		return FALSE;
	}

	/* Candidates that are farther right lose out. */
	if (best_so_far != NULL) {
		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) < 0) {
			return FALSE;
		}
	}

	/* Candidate to the left of the start do not qualify. */
	if (compare_icons_horizontal_first (container,
					    candidate,
					    start_icon) <= 0) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
same_row_left_side_rightmost (NautilusIconContainer *container,
			      NautilusIcon *start_icon,
			      NautilusIcon *best_so_far,
			      NautilusIcon *candidate,
			      void *data)
{
	/* Candidates not on the start row do not qualify. */
	if (compare_with_start_row (container, candidate) != 0) {
		return FALSE;
	}

	/* Candidates that are farther left lose out. */
	if (best_so_far != NULL) {
		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) > 0) {
			return FALSE;
		}
	}

	/* Candidate to the right of the start do not qualify. */
	if (compare_icons_horizontal_first (container,
					    candidate,
					    start_icon) >= 0) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
same_column_above_lowest (NautilusIconContainer *container,
			  NautilusIcon *start_icon,
			  NautilusIcon *best_so_far,
			  NautilusIcon *candidate,
			  void *data)
{
	/* Candidates not on the start column do not qualify. */
	if (compare_with_start_column (container, candidate) != 0) {
		return FALSE;
	}

	/* Candidates that are higher lose out. */
	if (best_so_far != NULL) {
		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) > 0) {
			return FALSE;
		}
	}

	/* Candidates below the start do not qualify. */
	if (compare_icons_vertical_first (container,
					  candidate,
					  start_icon) >= 0) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
same_column_below_highest (NautilusIconContainer *container,
			   NautilusIcon *start_icon,
			   NautilusIcon *best_so_far,
			   NautilusIcon *candidate,
			   void *data)
{
	EelCanvasItem *item;

	item = EEL_CANVAS_ITEM (candidate->item);

	/* Candidates above or on the start row do not qualify. */
	if (container->details->arrow_key_start_y >= item->y1) {
		return FALSE;
	}

	if (best_so_far != NULL) {
		/* Candidates on the start column are preferred. */
		if (compare_with_start_column (container, candidate) != 0 &&
		    compare_with_start_column (container, best_so_far) == 0) {
			return FALSE;
		}
		/* Candidates that are lower or to the left lose out. */
		if (compare_icons_vertical_first_reverse_horizontal (container, best_so_far, candidate) <= 0) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
closest_in_90_degrees (NautilusIconContainer *container,
		       NautilusIcon *start_icon,
		       NautilusIcon *best_so_far,
		       NautilusIcon *candidate,
		       void *data)
{
	EelDRect world_rect;
	int x, y;
	int dx, dy;
	int dist;
	int *best_dist;


	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (candidate->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &x,
		 &y);

	dx = x - container->details->arrow_key_start_x;
	dy = y - container->details->arrow_key_start_y;
	
	switch (container->details->arrow_key_direction) {
	case GTK_DIR_UP:
		if (dy > 0 ||
		    ABS(dx) > ABS(dy)) {
			return FALSE;
		}
		break;
	case GTK_DIR_DOWN:
		if (dy < 0 ||
		    ABS(dx) > ABS(dy)) {
			return FALSE;
		}
		break;
	case GTK_DIR_LEFT:
		if (dx > 0 ||
		    ABS(dy) > ABS(dx)) {
			return FALSE;
		}
		break;
	case GTK_DIR_RIGHT:
		if (dx < 0 ||
		    ABS(dy) > ABS(dx)) {
			return FALSE;
		}
		break;
	default:
		g_assert_not_reached();
	}

	dist = dx*dx + dy*dy;
	best_dist = data;
	
	if (best_so_far == NULL) {
		*best_dist = dist;
		return TRUE;
	}

	if (dist < *best_dist) {
		*best_dist = dist;
		return TRUE;
	}

	return FALSE;
}

static EelDRect 
get_rubberband (NautilusIcon *icon1,
		NautilusIcon *icon2)
{
	EelDRect rect1;
	EelDRect rect2;
	EelDRect ret;

	eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon1->item),
				    &rect1.x0, &rect1.y0, 
				    &rect1.x1, &rect1.y1);
	eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon2->item),
				    &rect2.x0, &rect2.y0, 
				    &rect2.x1, &rect2.y1);

	eel_drect_union (&ret, &rect1, &rect2);

	return ret;
}

static void
keyboard_move_to (NautilusIconContainer *container,
		  NautilusIcon *icon,
		  NautilusIcon *from,
		  GdkEventKey *event)
{
	if (icon == NULL) {
		return;
	}

	if ((event->state & GDK_CONTROL_MASK) != 0) {
		/* Move the keyboard focus. Use Control modifier
		 * rather than Alt to avoid Sawfish conflict.
		 */
		set_keyboard_focus (container, icon);
		container->details->keyboard_rubberband_start = NULL;
	} else if ((event->state & GDK_SHIFT_MASK) != 0) {
		/* Do rubberband selection */		
		EelDRect rect;

		if (from && !container->details->keyboard_rubberband_start) {
			set_keyboard_rubberband_start (container, from);
		} 

		set_keyboard_focus (container, icon);

		if (icon && container->details->keyboard_rubberband_start) {
			rect = get_rubberband (container->details->keyboard_rubberband_start,
					       icon);
			rubberband_select (container, NULL, &rect);
		}
	} else {
		/* Select icons and get rid of the special keyboard focus. */
		clear_keyboard_focus (container);
		clear_keyboard_rubberband_start (container);
		
		container->details->range_selection_base_icon = icon;
		if (select_one_unselect_others (container, icon)) {
			g_signal_emit (container,
					 signals[SELECTION_CHANGED], 0);
		}
	}
	schedule_keyboard_icon_reveal (container, icon);
}

static void
keyboard_home (NautilusIconContainer *container,
	       GdkEventKey *event)
{
	NautilusIcon *from;
	NautilusIcon *to;
	
	/* Home selects the first icon.
	 * Control-Home sets the keyboard focus to the first icon.
	 */

	from = find_best_selected_icon (container, NULL,
					rightmost_in_bottom_row, 
					NULL);
	to = find_best_icon (container, NULL, leftmost_in_top_row, NULL);	

	container->details->arrow_key_axis = AXIS_NONE;
	keyboard_move_to (container, to, from, event);
}

static void
keyboard_end (NautilusIconContainer *container,
	      GdkEventKey *event)
{
	NautilusIcon *to;
	NautilusIcon *from;

	/* End selects the last icon.
	 * Control-End sets the keyboard focus to the last icon.
	 */
	from = find_best_selected_icon (container, NULL,
					leftmost_in_top_row, 
					NULL);
	to = find_best_icon (container, NULL, rightmost_in_bottom_row, NULL);

	container->details->arrow_key_axis = AXIS_NONE;
	keyboard_move_to (container, to, from, event);
}

static void
record_arrow_key_start (NautilusIconContainer *container,
			NautilusIcon *icon,
			GtkDirectionType direction)
{
	EelDRect world_rect;

	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &container->details->arrow_key_start_x,
		 &container->details->arrow_key_start_y);
	
	container->details->arrow_key_direction = direction;
	
	switch (container->details->arrow_key_direction) {
	case GTK_DIR_UP:
	case GTK_DIR_DOWN:
		container->details->arrow_key_axis = AXIS_VERTICAL;
		container->details->arrow_key_start = container->details->arrow_key_start_x;
		break;
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
		container->details->arrow_key_axis = AXIS_HORIZONTAL;
		container->details->arrow_key_start = container->details->arrow_key_start_y;
		break;
	default:
		g_assert_not_reached();
	}
}

static void
keyboard_arrow_key (NautilusIconContainer *container,
		    GdkEventKey *event,
		    GtkDirectionType direction,
		    IsBetterIconFunction better_start,
		    IsBetterIconFunction empty_start,
		    IsBetterIconFunction better_destination,
		    IsBetterIconFunction better_destination_manual)
{
	NautilusIcon *from;
	NautilusIcon *to;
	int data;

	/* Chose the icon to start with.
	 * If we have a keyboard focus, start with it.
	 * Otherwise, use the single selected icon.
	 * If there's multiple selection, use the icon farthest toward the end.
	 */
	
	from = container->details->keyboard_focus;

	if (from == NULL) {
		if (has_multiple_selection (container)) {
			if (all_selected (container)) {
				from = find_best_selected_icon
					(container, NULL,
					 empty_start, NULL);
			} else {
				from = find_best_selected_icon
					(container, NULL,
					 better_start, NULL);
			}
		} else {
			from = get_first_selected_icon (container);
		}
	}

	/* If there's no icon, select the icon farthest toward the end.
	 * If there is an icon, select the next icon based on the arrow direction.
	 */
	if (from == NULL) {
		container->details->arrow_key_axis = AXIS_NONE;
		to = from = find_best_icon
			(container, NULL,
			 empty_start, NULL);
		
	} else {
		record_arrow_key_start (container, from, direction);
		
		to = find_best_icon
			(container, from,
			 container->details->auto_layout ? better_destination : better_destination_manual,
			 &data);
	}

	keyboard_move_to (container, to, from, event);
}

static void
keyboard_right (NautilusIconContainer *container,
		GdkEventKey *event)
{
	/* Right selects the next icon in the same row.
	 * Control-Right sets the keyboard focus to the next icon in the same row.
	 */
	keyboard_arrow_key (container,
			    event,
			    GTK_DIR_RIGHT,
			    rightmost_in_bottom_row,
			    leftmost_in_top_row,
			    same_row_right_side_leftmost,
			    closest_in_90_degrees);
}

static void
keyboard_left (NautilusIconContainer *container,
	       GdkEventKey *event)
{
	/* Left selects the next icon in the same row.
	 * Control-Left sets the keyboard focus to the next icon in the same row.
	 */
	keyboard_arrow_key (container,
			    event,
			    GTK_DIR_LEFT,
			    leftmost_in_top_row,
			    rightmost_in_bottom_row,
			    same_row_left_side_rightmost,
			    closest_in_90_degrees);
}

static void
keyboard_down (NautilusIconContainer *container,
	       GdkEventKey *event)
{
	/* Down selects the next icon in the same column.
	 * Control-Down sets the keyboard focus to the next icon in the same column.
	 */
	keyboard_arrow_key (container,
			    event,
			    GTK_DIR_DOWN,
			    rightmost_in_bottom_row,
			    leftmost_in_top_row,
			    same_column_below_highest,
			    closest_in_90_degrees);
}

static void
keyboard_up (NautilusIconContainer *container,
	     GdkEventKey *event)
{
	/* Up selects the next icon in the same column.
	 * Control-Up sets the keyboard focus to the next icon in the same column.
	 */
	keyboard_arrow_key (container,
			    event,
			    GTK_DIR_UP,
			    leftmost_in_top_row,
			    rightmost_in_bottom_row,
			    same_column_above_lowest,
			    closest_in_90_degrees);
}

static void
keyboard_space (NautilusIconContainer *container,
		GdkEventKey *event)
{
	NautilusIcon *icon;
	
	/* Control-space toggles the selection state of the current icon. */
	if ((event->state & GDK_CONTROL_MASK) != 0) {
		if (container->details->keyboard_focus != NULL) {
			icon_toggle_selected (container, container->details->keyboard_focus);
			g_signal_emit (container, signals[SELECTION_CHANGED], 0);
			if  (container->details->keyboard_focus->is_selected) {
				container->details->range_selection_base_icon = container->details->keyboard_focus;
			} 
		} else {
			icon = find_best_selected_icon (container,
							NULL,
							leftmost_in_top_row,
							NULL);
			if (icon == NULL) {
				icon = find_best_icon (container,
						       NULL,
						       leftmost_in_top_row,
						       NULL);
			}
			if (icon != NULL) {
				set_keyboard_focus (container, icon);
			}
		}
	} else if ((event->state & GDK_SHIFT_MASK) != 0) {
		activate_selected_items_alternate (container, NULL);
	} else {
		activate_selected_items (container);
	}
}

/* look for the first icon that matches the longest part of a given
 * search pattern
 */
typedef struct {
	gunichar *name;
	int last_match_length;
} BestNameMatch;

#ifndef TAB_NAVIGATION_DISABLED
static void
select_previous_or_next_icon (NautilusIconContainer *container, 
			      gboolean next, 
			      GdkEventKey *event)
{
	NautilusIcon *icon;
	const GList *item;

	item = NULL;
	/* Chose the icon to start with.
	 * If we have a keyboard focus, start with it.
	 * Otherwise, use the single selected icon.
	 */
	icon = container->details->keyboard_focus;
	if (icon == NULL) {
		icon = get_first_selected_icon (container);
	}

	if (icon != NULL) {
		/* must have at least @icon in the list */
		g_assert (container->details->icons != NULL);
		item = g_list_find (container->details->icons, icon);
		g_assert (item != NULL);
		
		item = next ? item->next : item->prev;
		if (item == NULL) {
			item = next ? g_list_first (container->details->icons) : g_list_last (container->details->icons);
		}

	} else if (container->details->icons != NULL) {
		/* no selection yet, pick the first or last item to select */
		item = next ? g_list_first (container->details->icons) : g_list_last (container->details->icons);
	}

	icon = (item != NULL) ? item->data : NULL;

	if (icon != NULL) {
		keyboard_move_to (container, icon, NULL, event);
	}
}
#endif

/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	NautilusIconContainer *container;

	container = NAUTILUS_ICON_CONTAINER (object);

        nautilus_icon_container_clear (container);

	if (container->details->rubberband_info.timer_id != 0) {
		g_source_remove (container->details->rubberband_info.timer_id);
		container->details->rubberband_info.timer_id = 0;
	}

        if (container->details->idle_id != 0) {
		g_source_remove (container->details->idle_id);
		container->details->idle_id = 0;
	}

	if (container->details->stretch_idle_id != 0) {
		g_source_remove (container->details->stretch_idle_id);
		container->details->stretch_idle_id = 0;
	}

        if (container->details->align_idle_id != 0) {
		g_source_remove (container->details->align_idle_id);
		container->details->align_idle_id = 0;
	}


	/* destroy interactive search dialog */
	if (container->details->search_window) {
		gtk_widget_destroy (container->details->search_window);
		container->details->search_window = NULL;
		container->details->search_entry = NULL;
		if (container->details->typeselect_flush_timeout) {
			g_source_remove (container->details->typeselect_flush_timeout);
			container->details->typeselect_flush_timeout = 0;
		}
	}


	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
finalize (GObject *object)
{
	NautilusIconContainerDetails *details;

	details = NAUTILUS_ICON_CONTAINER (object)->details;

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					 nautilus_icon_container_theme_changed,
					 object);
	
	g_hash_table_destroy (details->icon_set);
	details->icon_set = NULL;

	g_free (details->font);

	if (details->a11y_item_action_queue != NULL) {
		while (!g_queue_is_empty (details->a11y_item_action_queue)) {
			g_free (g_queue_pop_head (details->a11y_item_action_queue));
		}
		g_queue_free (details->a11y_item_action_queue);
	}
	if (details->a11y_item_action_idle_handler != 0) {
		g_source_remove (details->a11y_item_action_idle_handler);
	}

	g_free (details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GtkWidget methods.  */

static void
size_request (GtkWidget *widget,
	      GtkRequisition *requisition)
{
	GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);
	requisition->width = 1;
	requisition->height = 1;
}

static void
size_allocate (GtkWidget *widget,
	       GtkAllocation *allocation)
{
	NautilusIconContainer *container;
	gboolean need_layout_redone;

	container = NAUTILUS_ICON_CONTAINER (widget);

	need_layout_redone = !container->details->has_been_allocated;

	if (allocation->width != widget->allocation.width) {
		need_layout_redone = TRUE;
	}

	if (allocation->height != widget->allocation.height) {
		need_layout_redone = TRUE;
	}

	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

	container->details->has_been_allocated = TRUE;

	if (need_layout_redone) {
		redo_layout (container);
	}
}

static void
realize (GtkWidget *widget)
{
	GtkWindow *window;
	GdkBitmap *stipple;
	GtkAdjustment *vadj;

	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	/* Set up DnD.  */
	nautilus_icon_dnd_init (NAUTILUS_ICON_CONTAINER (widget), NULL);

	setup_label_gcs (NAUTILUS_ICON_CONTAINER (widget));

 	/* make us the focused widget */
 	g_assert (GTK_IS_WINDOW (gtk_widget_get_toplevel (widget)));
	window = GTK_WINDOW (gtk_widget_get_toplevel (widget));
	gtk_window_set_focus (window, widget);

	stipple = eel_stipple_bitmap_for_screen (
			gdk_drawable_get_screen (GDK_DRAWABLE (widget->window)));

	nautilus_icon_dnd_set_stipple (NAUTILUS_ICON_CONTAINER (widget), stipple);

	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (widget));
	g_signal_connect (vadj, "value_changed",
			  G_CALLBACK (handle_vadjustment_changed), widget);

}

static void
unrealize (GtkWidget *widget)
{
	int i;
	GtkWindow *window;
	NautilusIconContainer *container;

	container = NAUTILUS_ICON_CONTAINER (widget);

        g_assert (GTK_IS_WINDOW (gtk_widget_get_toplevel (widget)));
        window = GTK_WINDOW (gtk_widget_get_toplevel (widget));
	gtk_window_set_focus (window, NULL);

	for (i = 0; i < LAST_LABEL_COLOR; i++) {
		if (container->details->label_gcs [i]) {
			g_object_unref (container->details->label_gcs [i]);
			container->details->label_gcs [i] = NULL;
		}
	}

	nautilus_icon_dnd_fini (container);

	if (container->details->typeselect_flush_timeout) {
		g_source_remove (container->details->typeselect_flush_timeout);
		container->details->typeselect_flush_timeout = 0;
	}

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
style_set (GtkWidget *widget,
	   GtkStyle  *previous_style)
{
	NautilusIconContainer *container;
	gboolean frame_text;
	
	container = NAUTILUS_ICON_CONTAINER (widget);

	gtk_widget_style_get (GTK_WIDGET (container),
			      "frame_text", &frame_text,
			      NULL);

	container->details->use_drop_shadows = container->details->drop_shadows_requested && !frame_text;

	nautilus_icon_container_theme_changed (NAUTILUS_ICON_CONTAINER (widget));	

	if (GTK_WIDGET_REALIZED (widget)) {
		invalidate_label_sizes (container);
		nautilus_icon_container_request_update_all (container);
	}
	
	GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);
}

static gboolean
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	NautilusIconContainer *container;
	gboolean selection_changed;
	gboolean return_value;
	gboolean clicked_on_icon;

	container = NAUTILUS_ICON_CONTAINER (widget);
        container->details->button_down_time = event->time;
	
        /* Forget about the old keyboard selection now that we've started mousing. */
        clear_keyboard_focus (container);
	clear_keyboard_rubberband_start (container);

	/* Forget about where we began with the arrow keys now that we're mousing. */
	container->details->arrow_key_axis = AXIS_NONE;
	
	/* Invoke the canvas event handler and see if an item picks up the event. */
	clicked_on_icon = GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
	
	/* Move focus to icon container, unless we're still renaming (to avoid exiting
	 * renaming mode)
	 */
  	if (!GTK_WIDGET_HAS_FOCUS (widget) && !(is_renaming (container) || is_renaming_pending (container))) {
    		gtk_widget_grab_focus (widget);
    	}

	if (clicked_on_icon) {
		return TRUE;
	}

	/* An item didn't take the press, so it's a background press.
         * We ignore double clicks on the desktop for now.
	 */
	if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
		return TRUE;
	}

	if ((event->button == DRAG_BUTTON || event->button == MIDDLE_BUTTON) &&
	    event->type == GDK_BUTTON_PRESS) {
		/* Clear the last click icon for double click */
		container->details->double_click_icon[1] = container->details->double_click_icon[0];
		container->details->double_click_icon[0] = NULL;
	}
	
	/* Button 1 does rubber banding. */
	if (event->button == RUBBERBAND_BUTTON) {
		if (! button_event_modifies_selection (event)) {
			selection_changed = unselect_all (container);
			if (selection_changed) {
				g_signal_emit (container,
						 signals[SELECTION_CHANGED], 0);
			}
		}

		start_rubberbanding (container, event);
		return TRUE;
	}

	/* Prevent multi-button weirdness such as bug 6181 */
	if (container->details->rubberband_info.active) {
		return TRUE;
	}
	
	/* Button 2 may be passed to the window manager. */
	if (event->button == MIDDLE_BUTTON) {
		selection_changed = unselect_all (container);
		if (selection_changed) {
			g_signal_emit (container, signals[SELECTION_CHANGED], 0);
		}
		g_signal_emit (widget, signals[MIDDLE_CLICK], 0, event);
		return TRUE;
	}

	/* Button 3 does a contextual menu. */
	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		end_renaming_mode (container, TRUE);
		selection_changed = unselect_all (container);
		if (selection_changed) {
			g_signal_emit (container, signals[SELECTION_CHANGED], 0);
		}
		g_signal_emit (widget, signals[CONTEXT_CLICK_BACKGROUND], 0, event);
		return TRUE;
	}
	
	/* Otherwise, we emit a button_press message. */
	g_signal_emit (widget,
		       signals[BUTTON_PRESS], 0, event,
		       &return_value);
	return return_value;
}

static void
nautilus_icon_container_did_not_drag (NautilusIconContainer *container,
				      GdkEventButton *event)
{
	NautilusIconContainerDetails *details;
	gboolean selection_changed;
	static gint64 last_click_time = 0;
	static gint click_count = 0;
	gint double_click_time;
	gint64 current_time;
		
	details = container->details;

	if (details->icon_selected_on_button_down &&
	    ((event->state & GDK_CONTROL_MASK) != 0 ||
	     (event->state & GDK_SHIFT_MASK) == 0)) {
		if (button_event_modifies_selection (event)) {
			details->range_selection_base_icon = NULL;
			icon_toggle_selected (container, details->drag_icon);
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		} else {
			details->range_selection_base_icon = details->drag_icon;
			selection_changed = select_one_unselect_others 
				(container, details->drag_icon);
			
			if (selection_changed) {
				g_signal_emit (container,
					       signals[SELECTION_CHANGED], 0);
			}
		}
	} 
	
	if (details->drag_icon != NULL &&
	    details->single_click_mode) {		
		/* Determine click count */
		g_object_get (G_OBJECT (gtk_widget_get_settings (GTK_WIDGET (container))), 
			      "gtk-double-click-time", &double_click_time,
			      NULL);
		current_time = eel_get_system_time ();
		if (current_time - last_click_time < double_click_time * 1000) {
			click_count++;
		} else {
			click_count = 0;
		}
		
		/* Stash time for next compare */
		last_click_time = current_time;

		/* If single-click mode, activate the selected icons, unless modifying
		 * the selection or pressing for a very long time, or double clicking.
		 */

		
		if (click_count == 0 &&
		    event->time - details->button_down_time < MAX_CLICK_TIME &&
		    ! button_event_modifies_selection (event)) {
			
			/* It's a tricky UI issue whether this should activate
			 * just the clicked item (as if it were a link), or all
			 * the selected items (as if you were issuing an "activate
			 * selection" command). For now, we're trying the activate
			 * entire selection version to see how it feels. Note that
			 * NautilusList goes the other way because its "links" seem
			 * much more link-like.
			 */
			if (event->button == MIDDLE_BUTTON) {
				activate_selected_items_alternate (container, NULL);
			} else {
				activate_selected_items (container);
			}
		}
	}
}

static void
clear_drag_state (NautilusIconContainer *container)
{
	container->details->drag_icon = NULL;
	container->details->drag_state = DRAG_STATE_INITIAL;
}

static gboolean
start_stretching (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	EelDPoint world_point;
	GtkWidget *toplevel;
	GtkCornerType corner;
	GdkCursor *cursor;

	details = container->details;
	icon = details->stretch_icon;
	
	/* Check if we hit the stretch handles. */
	world_point.x = details->drag_x;
	world_point.y = details->drag_y;
	if (!nautilus_icon_canvas_item_hit_test_stretch_handles (icon->item, world_point, &corner)) {
		return FALSE;
	}

	switch (corner) {
	case GTK_CORNER_TOP_LEFT:
		cursor = gdk_cursor_new (GDK_TOP_LEFT_CORNER);
		break;
	case GTK_CORNER_BOTTOM_LEFT:
		cursor = gdk_cursor_new (GDK_BOTTOM_LEFT_CORNER);
		break;
	case GTK_CORNER_TOP_RIGHT:
		cursor = gdk_cursor_new (GDK_TOP_RIGHT_CORNER);
		break;
	case GTK_CORNER_BOTTOM_RIGHT:
		cursor = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);
		break;
	default: 
		cursor = NULL;
		break;
	}
	/* Set up the dragging. */
	details->drag_state = DRAG_STATE_STRETCH;
	eel_canvas_w2c (EEL_CANVAS (container),
			  details->drag_x,
			  details->drag_y,
			  &details->stretch_start.pointer_x,
			  &details->stretch_start.pointer_y);
	eel_canvas_w2c (EEL_CANVAS (container),
			  icon->x, icon->y,
			  &details->stretch_start.icon_x,
			  &details->stretch_start.icon_y);
	icon_get_size (container, icon,
		       &details->stretch_start.icon_size);

	eel_canvas_item_grab (EEL_CANVAS_ITEM (icon->item),
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK),
				cursor,
				GDK_CURRENT_TIME);
	if (cursor)
		gdk_cursor_unref (cursor);

	/* Ensure the window itself is focused.. */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));
	if (toplevel != NULL && GTK_WIDGET_REALIZED (toplevel)) {
		eel_gdk_window_focus (toplevel->window, GDK_CURRENT_TIME);
	}

	return TRUE;
}

static gboolean
update_stretch_at_idle (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	double world_x, world_y;
	StretchState stretch_state;

	details = container->details;
	icon = details->stretch_icon;

	if (icon == NULL) {
		container->details->stretch_idle_id = 0;
		return FALSE;
	}

	eel_canvas_w2c (EEL_CANVAS (container),
			  details->world_x, details->world_y,
			  &stretch_state.pointer_x, &stretch_state.pointer_y);

	compute_stretch (&details->stretch_start,
			 &stretch_state);

	eel_canvas_c2w (EEL_CANVAS (container),
			  stretch_state.icon_x, stretch_state.icon_y,
			  &world_x, &world_y);

	icon_set_position (icon, world_x, world_y);
	icon_set_size (container, icon, stretch_state.icon_size, FALSE, FALSE);

	container->details->stretch_idle_id = 0;

	return FALSE;
}	

static void
continue_stretching (NautilusIconContainer *container,
		     double world_x, double world_y)
{

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->world_x = world_x;
	container->details->world_y = world_y;

	if (container->details->stretch_idle_id == 0) {		
		container->details->stretch_idle_id = g_idle_add ((GtkFunction) update_stretch_at_idle, container);
	}
}

static gboolean
keyboard_stretching (NautilusIconContainer *container,
		     GdkEventKey           *event)
{
	NautilusIcon *icon;
	guint size;

	icon = container->details->stretch_icon;

	if (icon == NULL || !icon->is_selected) {
		return FALSE;
	}

	icon_get_size (container, icon, &size);

	switch (event->keyval) {
	case GDK_equal:
	case GDK_plus:
	case GDK_KP_Add:
		icon_set_size (container, icon, size + 5, FALSE, FALSE);
		break;
	case GDK_minus:
	case GDK_KP_Subtract:
		icon_set_size (container, icon, size - 5, FALSE, FALSE);
		break;
	case GDK_0:
	case GDK_KP_0:
		nautilus_icon_container_move_icon (container, icon,
						   icon->x, icon->y,
						   1.0,
						   FALSE, TRUE, TRUE);
		break;
	}
	
	return TRUE;
}

static void
ungrab_stretch_icon (NautilusIconContainer *container)
{
	eel_canvas_item_ungrab (EEL_CANVAS_ITEM (container->details->stretch_icon->item),
				  GDK_CURRENT_TIME);
}

static void
end_stretching (NautilusIconContainer *container,
		double world_x, double world_y)
{
	NautilusIconPosition position;
	NautilusIcon *icon;
	
	continue_stretching (container, world_x, world_y);
	ungrab_stretch_icon (container);

	/* now that we're done stretching, update the icon's position */
	
	icon = container->details->drag_icon;	
	if (nautilus_icon_container_is_layout_rtl (container)) {
		position.x = icon->saved_ltr_x = get_mirror_x_position (container, icon, icon->x);
	} else {
		position.x = icon->x;
	}
	position.y = icon->y;
	position.scale = icon->scale;
	g_signal_emit (container,
			 signals[ICON_POSITION_CHANGED], 0,
			 icon->data, &position);
	
	clear_drag_state (container);
	redo_layout (container);
}

static gboolean
undo_stretching (NautilusIconContainer *container)
{
	NautilusIcon *stretched_icon;

	stretched_icon = container->details->stretch_icon;

	if (stretched_icon == NULL) {
		return FALSE;
	}

	if (container->details->drag_state == DRAG_STATE_STRETCH) {
		ungrab_stretch_icon (container);
		clear_drag_state (container);
	}
	nautilus_icon_canvas_item_set_show_stretch_handles
		(stretched_icon->item, FALSE);
	
	icon_set_position (stretched_icon,
			   container->details->stretch_initial_x,
			   container->details->stretch_initial_y);
	icon_set_size (container,
		       stretched_icon, 
		       container->details->stretch_initial_size,
		       TRUE,
		       TRUE);
	
	container->details->stretch_icon = NULL;				
	emit_stretch_ended (container, stretched_icon);
	redo_layout (container);

	return TRUE;
}

static gboolean
button_release_event (GtkWidget *widget,
		      GdkEventButton *event)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;
	double world_x, world_y;
	
	container = NAUTILUS_ICON_CONTAINER (widget);
	details = container->details;

	if (event->button == RUBBERBAND_BUTTON && details->rubberband_info.active) {
		stop_rubberbanding (container, event->time);
		return TRUE;
	}
	
	if (event->button == details->drag_button) {
		details->drag_button = 0;

		switch (details->drag_state) {
		case DRAG_STATE_MOVE_OR_COPY:
			if (!details->drag_started) {
				nautilus_icon_container_did_not_drag (container, event);
			} else {
				nautilus_icon_dnd_end_drag (container);
				nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
						    "end drag from icon container");
			}
			break;
		case DRAG_STATE_STRETCH:
			eel_canvas_window_to_world
				(EEL_CANVAS (container), event->x, event->y, &world_x, &world_y);
			end_stretching (container, world_x, world_y);
			break;
		default:
			break;
		}

		clear_drag_state (container);
		return TRUE;
	}

	return GTK_WIDGET_CLASS (parent_class)->button_release_event (widget, event);
}

static int
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *event)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;
	double world_x, world_y;
	int canvas_x, canvas_y;
	GdkDragAction actions;

	container = NAUTILUS_ICON_CONTAINER (widget);
	details = container->details;

	if (details->drag_button != 0) {
		switch (details->drag_state) {
		case DRAG_STATE_MOVE_OR_COPY:
			if (details->drag_started) {
				break;
			}

			eel_canvas_window_to_world
				(EEL_CANVAS (container), event->x, event->y, &world_x, &world_y);
			
			if (gtk_drag_check_threshold (widget, 
						      details->drag_x,
						      details->drag_y,
						      world_x,
						      world_y)) {
				details->drag_started = TRUE;
				details->drag_state = DRAG_STATE_MOVE_OR_COPY;

				end_renaming_mode (container, TRUE);
			
				eel_canvas_w2c (EEL_CANVAS (container),
						  details->drag_x,
						  details->drag_y,
						  &canvas_x,
						  &canvas_y);

				actions = GDK_ACTION_COPY
					| GDK_ACTION_LINK
					| GDK_ACTION_ASK;

				if (container->details->drag_allow_moves) {
					actions |= GDK_ACTION_MOVE;
				}

				nautilus_icon_dnd_begin_drag (container,
							      actions,
							      details->drag_button,
							      event, 
							      canvas_x,
							      canvas_y);
				nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
						    "begin drag from icon container");
			}
			break;
		case DRAG_STATE_STRETCH:
			eel_canvas_window_to_world
				(EEL_CANVAS (container), event->x, event->y, &world_x, &world_y);
			continue_stretching (container, world_x, world_y);
			break;
		default:
			break;
		}
	}

	return GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);
}

static void
nautilus_icon_container_search_position_func (NautilusIconContainer *container,
					      GtkWidget *search_dialog)
{
	gint x, y;
	gint cont_x, cont_y;
	gint cont_width, cont_height;
	GdkWindow *cont_window;
	GdkScreen *screen;
	GtkRequisition requisition;
	gint monitor_num;
	GdkRectangle monitor;


	cont_window = GTK_WIDGET (container)->window;
	screen = gdk_drawable_get_screen (cont_window);

	monitor_num = gdk_screen_get_monitor_at_window (screen, cont_window);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	gtk_widget_realize (search_dialog);

	gdk_window_get_origin (cont_window, &cont_x, &cont_y);
	gdk_drawable_get_size (cont_window, &cont_width, &cont_height);
	gtk_widget_size_request (search_dialog, &requisition);

	if (cont_x + cont_width - requisition.width > gdk_screen_get_width (screen)) {
		x = gdk_screen_get_width (screen) - requisition.width;
	} else if (cont_x + cont_width - requisition.width < 0) {
		x = 0;
	} else {
		x = cont_x + cont_width - requisition.width;
	}

	if (cont_y + cont_height > gdk_screen_get_height (screen)) {
		y = gdk_screen_get_height (screen) - requisition.height;
	} else if (cont_y + cont_height < 0) { /* isn't really possible ... */
		y = 0;
	} else {
		y = cont_y + cont_height;
	}

	gtk_window_move (GTK_WINDOW (search_dialog), x, y);
}

static gboolean
nautilus_icon_container_real_search_enable_popdown (gpointer data)
{
	NautilusIconContainer *container = (NautilusIconContainer *)data;

	container->details->disable_popdown = FALSE;
	
	g_object_unref (container);

	return FALSE;
}

static void
nautilus_icon_container_search_enable_popdown (GtkWidget *widget,
					       gpointer   data)
{
	NautilusIconContainer *container = (NautilusIconContainer *) data;
	
	g_object_ref (container);
	g_timeout_add (200, nautilus_icon_container_real_search_enable_popdown, data);
}

static void
nautilus_icon_container_search_disable_popdown (GtkEntry *entry,
						GtkMenu  *menu,
						gpointer  data)
{
	NautilusIconContainer *container = (NautilusIconContainer *) data;

	container->details->disable_popdown = TRUE;
	g_signal_connect (menu, "hide",
			  G_CALLBACK (nautilus_icon_container_search_enable_popdown),
			  data);
}

/* Cut and paste from gtkwindow.c */
static void
send_focus_change (GtkWidget *widget, gboolean in)
{
	GdkEvent *fevent;
	
	fevent = gdk_event_new (GDK_FOCUS_CHANGE);

	g_object_ref (widget);

	if (in) {
		GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
	} else {
		GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
	}

	fevent->focus_change.type = GDK_FOCUS_CHANGE;
	fevent->focus_change.window = g_object_ref (widget->window);
	fevent->focus_change.in = in;

	gtk_widget_event (widget, fevent);

	g_object_notify (G_OBJECT (widget), "has-focus");

	g_object_unref (widget);
	gdk_event_free (fevent);
}

static void
nautilus_icon_container_search_dialog_hide (GtkWidget *search_dialog,
					    NautilusIconContainer *container)
{
	if (container->details->disable_popdown) {
		return;
	}

	if (container->details->search_entry_changed_id) {
		g_signal_handler_disconnect (container->details->search_entry,
					     container->details->search_entry_changed_id);
		container->details->search_entry_changed_id = 0;
	}
	if (container->details->typeselect_flush_timeout) {
		g_source_remove (container->details->typeselect_flush_timeout);
		container->details->typeselect_flush_timeout = 0;
	}

	/* send focus-in event */
	send_focus_change (GTK_WIDGET (container->details->search_entry), FALSE);
	gtk_widget_hide (search_dialog);
	gtk_entry_set_text (GTK_ENTRY (container->details->search_entry), "");
}

static gboolean
nautilus_icon_container_search_entry_flush_timeout (NautilusIconContainer *container)
{
	nautilus_icon_container_search_dialog_hide (container->details->search_window, container);

	return TRUE;
}

/* Because we're visible but offscreen, we just set a flag in the preedit
 * callback.
 */
static void
nautilus_icon_container_search_preedit_changed (GtkIMContext *im_context,
						NautilusIconContainer *container)
{
	container->details->imcontext_changed = 1;
	if (container->details->typeselect_flush_timeout) {
		g_source_remove (container->details->typeselect_flush_timeout);
		container->details->typeselect_flush_timeout =
			g_timeout_add (NAUTILUS_ICON_CONTAINER_SEARCH_DIALOG_TIMEOUT,
				(GSourceFunc) nautilus_icon_container_search_entry_flush_timeout,
				container);
	}
}

static void
nautilus_icon_container_search_activate (GtkEntry *entry,
					 NautilusIconContainer *container)
{
	nautilus_icon_container_search_dialog_hide (container->details->search_window,
						    container);

	activate_selected_items (container);
}

static gboolean
nautilus_icon_container_search_delete_event (GtkWidget *widget,
					     GdkEventAny *event,
					     NautilusIconContainer *container)
{
	g_assert (GTK_IS_WIDGET (widget));

	nautilus_icon_container_search_dialog_hide (widget, container);

	return TRUE;
}

static gboolean
nautilus_icon_container_search_button_press_event (GtkWidget *widget,
						   GdkEventButton *event,
						   NautilusIconContainer *container)
{
	g_assert (GTK_IS_WIDGET (widget));

	nautilus_icon_container_search_dialog_hide (widget, container);

	if (event->window == GTK_LAYOUT (container)->bin_window) {
		button_press_event (GTK_WIDGET (container), event);
	}

	return TRUE;
}

static gboolean
nautilus_icon_container_search_iter (NautilusIconContainer *container,
				     const char *key, gint n)
{
	GList *p;
	NautilusIcon *icon;
	const char *name;
	int count;
	char *normalized_key, *case_normalized_key;
	char *normalized_name, *case_normalized_name;
	
	g_assert (key != NULL);
	g_assert (n >= 1);
	
	normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);
	if (!normalized_key) {
		return FALSE;
	}
	case_normalized_key = g_utf8_casefold (normalized_key, -1);
	g_free (normalized_key);
	if (!case_normalized_key) {
		return FALSE;
	}
	
	icon = NULL;
	count = 0;
	for (p = container->details->icons; p != NULL && count != n; p = p->next) {
		icon = p->data;
		name = nautilus_icon_canvas_item_get_editable_text (icon->item);
		
		/* This can happen if a key event is handled really early while
		 * loading the icon container, before the items have all been
		 * updated once.
		 */
		if (!name) {
			continue;
		}
			
		normalized_name = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
		if (!normalized_name) {
			continue;
		}
		case_normalized_name = g_utf8_casefold (normalized_name, -1);
		g_free (normalized_name);
		if (!case_normalized_name) {
			continue;
		}
		
		if (strncmp (case_normalized_key, case_normalized_name,
			     strlen (case_normalized_key)) == 0) {
			count++;
		}

		g_free (case_normalized_name);
	}

	g_free (case_normalized_key);

	if (count == n) {
		if (select_one_unselect_others (container, icon)) {
			g_signal_emit (container, signals[SELECTION_CHANGED], 0);
		}
		schedule_keyboard_icon_reveal (container, icon);
		
		return TRUE;
	}
	
	return FALSE;
}

static void
nautilus_icon_container_search_move (GtkWidget *window,
				     NautilusIconContainer *container,
				     gboolean up)
{
	gboolean ret;
	gint len;
	gint count = 0;
	const gchar *text;

	text = gtk_entry_get_text (GTK_ENTRY (container->details->search_entry));

	g_assert (text != NULL);

	if (container->details->selected_iter == 0) {
		return;
	}
	
	if (up && container->details->selected_iter == 1) {
		return;
	}

	len = strlen (text);

	if (len < 1) {
		return;
	}

	/* search */
	unselect_all (container);

	ret = nautilus_icon_container_search_iter (container, text,
		up?((container->details->selected_iter) - 1):((container->details->selected_iter + 1)));

	if (ret) {
		/* found */
		container->details->selected_iter += up?(-1):(1);
	} else {
		/* return to old iter */
		count = 0;
		nautilus_icon_container_search_iter (container, text,
					container->details->selected_iter);
	}
}

static gboolean
nautilus_icon_container_search_scroll_event (GtkWidget *widget,
					     GdkEventScroll *event,
					     NautilusIconContainer *container)
{
	gboolean retval = FALSE;

	if (event->direction == GDK_SCROLL_UP) {
		nautilus_icon_container_search_move (widget, container, TRUE);
		retval = TRUE;
	} else if (event->direction == GDK_SCROLL_DOWN) {
		nautilus_icon_container_search_move (widget, container, FALSE);
		retval = TRUE;
	}

	/* renew the flush timeout */
	if (retval && container->details->typeselect_flush_timeout) {
		g_source_remove (container->details->typeselect_flush_timeout);
		container->details->typeselect_flush_timeout =
			g_timeout_add (NAUTILUS_ICON_CONTAINER_SEARCH_DIALOG_TIMEOUT,
				(GSourceFunc) nautilus_icon_container_search_entry_flush_timeout,
				container);
	}

	return retval;
}

static gboolean
nautilus_icon_container_search_key_press_event (GtkWidget *widget,
						GdkEventKey *event,
						NautilusIconContainer *container)
{
	gboolean retval = FALSE;

	g_assert (GTK_IS_WIDGET (widget));
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));

	/* close window and cancel the search */
	if (event->keyval == GDK_Escape || event->keyval == GDK_Tab) {
		nautilus_icon_container_search_dialog_hide (widget, container);
		return TRUE;
	}

	/* select previous matching iter */
	if (event->keyval == GDK_Up || event->keyval == GDK_KP_Up) {
		nautilus_icon_container_search_move (widget, container, TRUE);
		retval = TRUE;
	}

	if (((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
	    && (event->keyval == GDK_g || event->keyval == GDK_G)) {
		nautilus_icon_container_search_move (widget, container, TRUE);
		retval = TRUE;
	}

	/* select next matching iter */
	if (event->keyval == GDK_Down || event->keyval == GDK_KP_Down) {
		nautilus_icon_container_search_move (widget, container, FALSE);
		retval = TRUE;
	}

	if (((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK)
	    && (event->keyval == GDK_g || event->keyval == GDK_G)) {
		nautilus_icon_container_search_move (widget, container, FALSE);
		retval = TRUE;
	}

	/* renew the flush timeout */
	if (retval && container->details->typeselect_flush_timeout) {
		g_source_remove (container->details->typeselect_flush_timeout);
		container->details->typeselect_flush_timeout =
			g_timeout_add (NAUTILUS_ICON_CONTAINER_SEARCH_DIALOG_TIMEOUT,
				(GSourceFunc) nautilus_icon_container_search_entry_flush_timeout,
				container);
	}

	return retval;
}

static void
nautilus_icon_container_search_init (GtkWidget   *entry,
				     NautilusIconContainer *container)
{
	gint ret;
	gint len;
	const gchar *text;

	g_assert (GTK_IS_ENTRY (entry));
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	len = strlen (text);

	/* search */
	unselect_all (container);
	if (container->details->typeselect_flush_timeout)
	{
		g_source_remove (container->details->typeselect_flush_timeout);
		container->details->typeselect_flush_timeout =
			g_timeout_add (NAUTILUS_ICON_CONTAINER_SEARCH_DIALOG_TIMEOUT,
				(GSourceFunc) nautilus_icon_container_search_entry_flush_timeout,
				container);
	}

	if (len < 1) {
		return;
	}

	ret = nautilus_icon_container_search_iter (container, text, 1);

	if (ret) {
		container->details->selected_iter = 1;
	}
}

static void
nautilus_icon_container_ensure_interactive_directory (NautilusIconContainer *container)
{
	GtkWidget *frame, *vbox, *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));

	if (container->details->search_window != NULL) {
		return;
	}

	container->details->search_window = gtk_window_new (GTK_WINDOW_POPUP);

	gtk_window_set_modal (GTK_WINDOW (container->details->search_window), TRUE);
	g_signal_connect (container->details->search_window, "delete_event",
			  G_CALLBACK (nautilus_icon_container_search_delete_event),
			  container);
	g_signal_connect (container->details->search_window, "key_press_event",
			  G_CALLBACK (nautilus_icon_container_search_key_press_event),
			  container);
	g_signal_connect (container->details->search_window, "button_press_event",
			  G_CALLBACK (nautilus_icon_container_search_button_press_event),
			  container);
	g_signal_connect (container->details->search_window, "scroll_event",
			  G_CALLBACK (nautilus_icon_container_search_scroll_event),
			  container);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (container->details->search_window), frame);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

	/* add entry */
	container->details->search_entry = gtk_entry_new ();
	gtk_widget_show (container->details->search_entry);
	g_signal_connect (container->details->search_entry, "populate_popup",
			  G_CALLBACK (nautilus_icon_container_search_disable_popdown),
			  container);
	g_signal_connect (container->details->search_entry, "activate",
			  G_CALLBACK (nautilus_icon_container_search_activate),
			  container);
	g_signal_connect (GTK_ENTRY (container->details->search_entry)->im_context,
			  "preedit-changed",
			  G_CALLBACK (nautilus_icon_container_search_preedit_changed),
			  container);
	gtk_container_add (GTK_CONTAINER (vbox), container->details->search_entry);

	gtk_widget_realize (container->details->search_entry);
}

/* Pops up the interactive search entry.  If keybinding is TRUE then the user
 * started this by typing the start_interactive_search keybinding.  Otherwise, it came from 
 */
static gboolean
nautilus_icon_container_real_start_interactive_search (NautilusIconContainer *container,
						       gboolean keybinding)
{
	/* We only start interactive search if we have focus.  If one of our
	 * children have focus, we don't want to start the search.
	 */
	GtkWidgetClass *entry_parent_class;

	if (container->details->search_window != NULL &&
	    GTK_WIDGET_VISIBLE (container->details->search_window)) {
		return TRUE;
	}

	if (!GTK_WIDGET_HAS_FOCUS (container)) {
		return FALSE;
	}

	nautilus_icon_container_ensure_interactive_directory (container);

	if (keybinding) {
		gtk_entry_set_text (GTK_ENTRY (container->details->search_entry), "");
	}

	/* done, show it */
	nautilus_icon_container_search_position_func (container, container->details->search_window);
	gtk_widget_show (container->details->search_window);
	if (container->details->search_entry_changed_id == 0) {
		container->details->search_entry_changed_id =
			g_signal_connect (container->details->search_entry, "changed",
				G_CALLBACK (nautilus_icon_container_search_init),
				container);
	}

	container->details->typeselect_flush_timeout =
		g_timeout_add (NAUTILUS_ICON_CONTAINER_SEARCH_DIALOG_TIMEOUT,
			(GSourceFunc) nautilus_icon_container_search_entry_flush_timeout,
			container);

	/* Grab focus will select all the text.  We don't want that to happen, so we
	* call the parent instance and bypass the selection change.  This is probably
	* really non-kosher. */
	entry_parent_class = g_type_class_peek_parent (GTK_ENTRY_GET_CLASS (container->details->search_entry));
	(entry_parent_class->grab_focus) (container->details->search_entry);

	/* send focus-in event */
	send_focus_change (container->details->search_entry, TRUE);

	/* search first matching iter */
	nautilus_icon_container_search_init (container->details->search_entry, container);

	return TRUE;
}

static gboolean
nautilus_icon_container_start_interactive_search (NautilusIconContainer *container)
{
	return nautilus_icon_container_real_start_interactive_search (container, TRUE);
}

static gboolean
handle_popups (NautilusIconContainer *container,
	       GdkEventKey           *event,
	       const char            *signal)
{
	GdkEventButton button_event = { 0 };

	g_signal_emit_by_name (container, signal, &button_event);

	return TRUE;
}

static int
key_press_event (GtkWidget *widget,
		 GdkEventKey *event)
{
	NautilusIconContainer *container;
	gboolean handled;

	container = NAUTILUS_ICON_CONTAINER (widget);
	handled = FALSE;

	if (is_renaming (container) || is_renaming_pending (container)) {
		switch (event->keyval) {
		case GDK_Return:
		case GDK_KP_Enter:
			end_renaming_mode (container, TRUE);	
			handled = TRUE;
			break;			
		case GDK_Escape:
			end_renaming_mode (container, FALSE);
			handled = TRUE;
			break;
		default:
			break;
		}
	} else {
		switch (event->keyval) {
		case GDK_Home:
		case GDK_KP_Home:
			keyboard_home (container, event);
			handled = TRUE;
			break;
		case GDK_End:
		case GDK_KP_End:
			keyboard_end (container, event);
			handled = TRUE;
			break;
		case GDK_Left:
		case GDK_KP_Left:
			keyboard_left (container, event);
			handled = TRUE;
			break;
		case GDK_Up:
		case GDK_KP_Up:
			/* Don't eat Alt-Up, as that is used for alt-shift-Up */
			if ((event->state & GDK_MOD1_MASK) == 0) {
				keyboard_up (container, event);
				handled = TRUE;
			}
			break;
		case GDK_Right:
		case GDK_KP_Right:
			keyboard_right (container, event);
			handled = TRUE;
			break;
		case GDK_Down:
		case GDK_KP_Down:
			/* Don't eat Alt-Down, as that is used for Open */
			if ((event->state & GDK_MOD1_MASK) == 0) {
				keyboard_down (container, event);
				handled = TRUE;
			}
			break;
		case GDK_space:
			keyboard_space (container, event);
			handled = TRUE;
			break;
#ifndef TAB_NAVIGATION_DISABLED
		case GDK_Tab:
		case GDK_ISO_Left_Tab:
			select_previous_or_next_icon (container, 
						      (event->state & GDK_SHIFT_MASK) == 0, event);
			handled = TRUE;
			break;
#endif
		case GDK_Return:
		case GDK_KP_Enter:
			if ((event->state & GDK_SHIFT_MASK) != 0) {
				activate_selected_items_alternate (container, NULL);
			} else {
				activate_selected_items (container);
			}
			
			handled = TRUE;
			break;
 		case GDK_Escape:
			handled = undo_stretching (container);
			break;
 		case GDK_plus:
 		case GDK_minus:
 		case GDK_equal:
 		case GDK_KP_Add:
 		case GDK_KP_Subtract:
 		case GDK_0:
 		case GDK_KP_0:
			if (event->state & GDK_CONTROL_MASK) {
				handled = keyboard_stretching (container, event);
			}
			break;
		case GDK_F10:
			/* handle Ctrl+F10 because we want to display the
			 * background popup even if something is selected.
			 * The other cases are handled by popup_menu().
			 */
			if (event->state & GDK_CONTROL_MASK) {
				handled = handle_popups (container, event,
							 "context_click_background");
			}
			break;
		default:
			break;
		}
	}

	if (!handled) {
		handled = GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
	}
	
	/* We pass the event to the search_entry.  If its text changes, then we
	 * start the typeahead find capabilities.
	 * Copied from NautilusIconContainer */
	if (!handled &&
	    event->keyval != GDK_slash /* don't steal slash key event, used for "go to" */ &&
	    event->keyval != GDK_BackSpace &&
	    event->keyval != GDK_Delete) {
		GdkEvent *new_event;
		char *old_text;
		const char *new_text;
		gboolean retval;
		GdkScreen *screen;
		gboolean text_modified;
		gulong popup_menu_id;

		nautilus_icon_container_ensure_interactive_directory (container);

		/* Make a copy of the current text */
		old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (container->details->search_entry)));
		new_event = gdk_event_copy ((GdkEvent *) event);
		((GdkEventKey *) new_event)->window = container->details->search_entry->window;
		gtk_widget_realize (container->details->search_window);

		popup_menu_id = g_signal_connect (container->details->search_entry, 
						  "popup_menu", G_CALLBACK (gtk_true), NULL);

		/* Move the entry off screen */
		screen = gtk_widget_get_screen (GTK_WIDGET (container));
		gtk_window_move (GTK_WINDOW (container->details->search_window),
		gdk_screen_get_width (screen) + 1,
		gdk_screen_get_height (screen) + 1);
		gtk_widget_show (container->details->search_window);

		/* Send the event to the window.  If the preedit_changed signal is emitted
		 * during this event, we will set priv->imcontext_changed  */
		container->details->imcontext_changed = FALSE;
		retval = gtk_widget_event (container->details->search_entry, new_event);
		gtk_widget_hide (container->details->search_window);

		g_signal_handler_disconnect (container->details->search_entry, 
					     popup_menu_id);

		/* We check to make sure that the entry tried to handle the text, and that
		 * the text has changed. */
		new_text = gtk_entry_get_text (GTK_ENTRY (container->details->search_entry));
		text_modified = strcmp (old_text, new_text) != 0;
		g_free (old_text);
		if (container->details->imcontext_changed ||    /* we're in a preedit */
		    (retval && text_modified)) {                /* ...or the text was modified */
			if (nautilus_icon_container_real_start_interactive_search (container, FALSE)) {
				gtk_widget_grab_focus (GTK_WIDGET (container));
				return TRUE;
			} else {
				gtk_entry_set_text (GTK_ENTRY (container->details->search_entry), "");
				return FALSE;
			}
		}
	}

	return handled;
}

static gboolean
popup_menu (GtkWidget *widget)
{
	NautilusIconContainer *container;

	container = NAUTILUS_ICON_CONTAINER (widget);

	if (has_selection (container)) {
		handle_popups (container, NULL,
			       "context_click_selection");
	} else {
		handle_popups (container, NULL,
			       "context_click_background");
	}

	return TRUE;
}

static void
draw_canvas_background (EelCanvas *canvas,
			int x, int y, int width, int height)
{
	/* Don't chain up to the parent to avoid clearing and redrawing */
}


static gboolean
expose_event (GtkWidget      *widget,
	      GdkEventExpose *event)
{
/*	g_warning ("Expose Icon Container %p '%d,%d: %d,%d'",
		   widget,
		   event->area.x, event->area.y,
		   event->area.width, event->area.height); */
	
	return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
}

static AtkObject *
get_accessible (GtkWidget *widget)
{
	AtkObject *accessible;
	
	if ((accessible = eel_accessibility_get_atk_object (widget))) {
		return accessible;
	}
	
	accessible = g_object_new 
		(nautilus_icon_container_accessible_get_type (), NULL);
	
	return eel_accessibility_set_atk_object_return (widget, accessible);
}

static void
grab_notify_cb  (GtkWidget        *widget,
		 gboolean          was_grabbed)
{
	NautilusIconContainer *container;

	container = NAUTILUS_ICON_CONTAINER (widget);
	
	if (container->details->rubberband_info.active &&
	    !was_grabbed) {
		/* we got a (un)grab-notify during rubberband.
		 * This happens when a new modal dialog shows
		 * up (e.g. authentication or an error). Stop
		 * the rubberbanding so that we can handle the
		 * dialog. */
		stop_rubberbanding (container,
				    GDK_CURRENT_TIME);
	}
}


/* Initialization.  */

static void
nautilus_icon_container_class_init (NautilusIconContainerClass *class)
{
	GtkWidgetClass *widget_class;
	EelCanvasClass *canvas_class;
	GtkBindingSet *binding_set;

	G_OBJECT_CLASS (class)->finalize = finalize;
	GTK_OBJECT_CLASS (class)->destroy = destroy;

	/* Signals.  */

	signals[SELECTION_CHANGED]
		= g_signal_new ("selection_changed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 selection_changed),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);
	signals[BUTTON_PRESS]
		= g_signal_new ("button_press",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 button_press),
		                NULL, NULL,
		                nautilus_marshal_BOOLEAN__POINTER,
		                G_TYPE_BOOLEAN, 1,
				GDK_TYPE_EVENT);
	signals[ACTIVATE]
		= g_signal_new ("activate",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 activate),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ACTIVATE_ALTERNATE]
		= g_signal_new ("activate_alternate",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 activate_alternate),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[CONTEXT_CLICK_SELECTION]
		= g_signal_new ("context_click_selection",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 context_click_selection),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[CONTEXT_CLICK_BACKGROUND]
		= g_signal_new ("context_click_background",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 context_click_background),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[MIDDLE_CLICK]
		= g_signal_new ("middle_click",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 middle_click),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ICON_POSITION_CHANGED]
		= g_signal_new ("icon_position_changed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 icon_position_changed),
		                NULL, NULL,
		                nautilus_marshal_VOID__POINTER_POINTER,
		                G_TYPE_NONE, 2,
				G_TYPE_POINTER,
				G_TYPE_POINTER);
	signals[ICON_TEXT_CHANGED]
		= g_signal_new ("icon_text_changed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 icon_text_changed),
		                NULL, NULL,
		                nautilus_marshal_VOID__POINTER_STRING,
		                G_TYPE_NONE, 2,
				G_TYPE_POINTER,
				G_TYPE_STRING);
	signals[ICON_STRETCH_STARTED]
		= g_signal_new ("icon_stretch_started",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 icon_stretch_started),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ICON_STRETCH_ENDED]
		= g_signal_new ("icon_stretch_ended",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						     icon_stretch_ended),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[RENAMING_ICON]
		= g_signal_new ("renaming_icon",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 renaming_icon),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[GET_ICON_URI]
		= g_signal_new ("get_icon_uri",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 get_icon_uri),
		                NULL, NULL,
		                eel_marshal_STRING__POINTER,
		                G_TYPE_STRING, 1,
				G_TYPE_POINTER);
	signals[GET_ICON_DROP_TARGET_URI]
		= g_signal_new ("get_icon_drop_target_uri",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 get_icon_drop_target_uri),
		                NULL, NULL,
		                eel_marshal_STRING__POINTER,
		                G_TYPE_STRING, 1,
				G_TYPE_POINTER);
	signals[MOVE_COPY_ITEMS] 
		= g_signal_new ("move_copy_items",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass, 
						 move_copy_items),
		                NULL, NULL,
		                nautilus_marshal_VOID__POINTER_POINTER_POINTER_ENUM_INT_INT,
		                G_TYPE_NONE, 6,
				G_TYPE_POINTER,
				G_TYPE_POINTER,
				G_TYPE_POINTER,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[HANDLE_NETSCAPE_URL]
		= g_signal_new ("handle_netscape_url",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass, 
						 handle_netscape_url),
		                NULL, NULL,
		                nautilus_marshal_VOID__STRING_STRING_ENUM_INT_INT,
		                G_TYPE_NONE, 5,
				G_TYPE_STRING,
				G_TYPE_STRING,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[HANDLE_URI_LIST] 
		= g_signal_new ("handle_uri_list",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass, 
						     handle_uri_list),
		                NULL, NULL,
		                nautilus_marshal_VOID__STRING_STRING_ENUM_INT_INT,
		                G_TYPE_NONE, 5,
				G_TYPE_STRING,
				G_TYPE_STRING,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[HANDLE_TEXT]
		= g_signal_new ("handle_text",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass, 
						 handle_text),
		                NULL, NULL,
		                nautilus_marshal_VOID__STRING_STRING_ENUM_INT_INT,
		                G_TYPE_NONE, 5,
				G_TYPE_STRING,
				G_TYPE_STRING,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[GET_CONTAINER_URI] 
		= g_signal_new ("get_container_uri",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass, 
						 get_container_uri),
		                NULL, NULL,
		                eel_marshal_STRING__VOID,
		                G_TYPE_STRING, 0);
	signals[CAN_ACCEPT_ITEM] 
		= g_signal_new ("can_accept_item",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass, 
						 can_accept_item),
		                NULL, NULL,
		                eel_marshal_INT__POINTER_STRING,
		                G_TYPE_INT, 2,
				G_TYPE_POINTER,
				G_TYPE_STRING);
	signals[GET_STORED_ICON_POSITION]
		= g_signal_new ("get_stored_icon_position",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 get_stored_icon_position),
		                NULL, NULL,
		                eel_marshal_BOOLEAN__POINTER_POINTER,
		                G_TYPE_BOOLEAN, 2,
				G_TYPE_POINTER,
				G_TYPE_POINTER);
	signals[LAYOUT_CHANGED]
		= g_signal_new ("layout_changed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 layout_changed),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);
	signals[PREVIEW]
		= g_signal_new ("preview",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 preview),
		                NULL, NULL,
		                nautilus_marshal_INT__POINTER_BOOLEAN,
		                G_TYPE_INT, 2,
				G_TYPE_POINTER,
				G_TYPE_BOOLEAN);
	signals[BAND_SELECT_STARTED]
		= g_signal_new ("band_select_started",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 band_select_started),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);
	signals[BAND_SELECT_ENDED]
		= g_signal_new ("band_select_ended",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						     band_select_ended),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);
	signals[ICON_ADDED]
		= g_signal_new ("icon_added",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 icon_added),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[ICON_REMOVED]
		= g_signal_new ("icon_removed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 icon_removed),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[CLEARED]
		= g_signal_new ("cleared",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusIconContainerClass,
						 cleared),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);

	signals[START_INTERACTIVE_SEARCH]
		= g_signal_new ("start_interactive_search",
				G_TYPE_FROM_CLASS (class),
				G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				G_STRUCT_OFFSET (NautilusIconContainerClass,
						 start_interactive_search),
				NULL, NULL,
				nautilus_marshal_BOOLEAN__VOID,
				G_TYPE_BOOLEAN, 0);

	/* GtkWidget class.  */

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = size_request;
	widget_class->size_allocate = size_allocate;
	widget_class->realize = realize;
	widget_class->unrealize = unrealize;
	widget_class->button_press_event = button_press_event;
	widget_class->button_release_event = button_release_event;
	widget_class->motion_notify_event = motion_notify_event;
	widget_class->key_press_event = key_press_event;
	widget_class->popup_menu = popup_menu;
	widget_class->get_accessible = get_accessible;
	widget_class->style_set = style_set;
	widget_class->expose_event = expose_event;
	widget_class->grab_notify = grab_notify_cb;

	canvas_class = EEL_CANVAS_CLASS (class);
	canvas_class->draw_background = draw_canvas_background;

	class->start_interactive_search = nautilus_icon_container_start_interactive_search;
	
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boolean ("frame_text",
								       "Frame Text",
								       "Draw a frame around unselected text",
								       FALSE,
								       G_PARAM_READABLE));

	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("selection_box_color",
								     "Selection Box Color",
								     "Color of the selection box",
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uchar ("selection_box_alpha",
								     "Selection Box Alpha",
								     "Opacity of the selection box",
								     0, 0xff,
								     DEFAULT_SELECTION_BOX_ALPHA,
								     G_PARAM_READABLE));

	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uchar ("highlight_alpha",
								     "Highlight Alpha",
								     "Opacity of the highlight for selected icons",
								     0, 0xff,
								     DEFAULT_HIGHLIGHT_ALPHA,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uchar ("normal_alpha",
								     "Normal Alpha",
								     "Opacity of the normal icons if frame_text is set",
								     0, 0xff,
								     DEFAULT_NORMAL_ALPHA,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uchar ("prelight_alpha",
								     "Prelight Alpha",
								     "Opacity of the prelight icons if frame_text is set",
								     0, 0xff,
								     DEFAULT_PRELIGHT_ALPHA,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("light_info_color",
								     "Light Info Color",
								     "Color used for information text against a dark background",
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("dark_info_color",
								     "Dark Info Color",
								     "Color used for information text against a light background",
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));

	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uint ("normal_icon_render_mode",
								     "Normal Icon Render Mode",
								     "Mode of normal icons being rendered (0=normal, 1=spotlight, 2=colorize, 3=colorize-monochromely)",
								     0, 3,
								     DEFAULT_NORMAL_ICON_RENDER_MODE,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uint ("prelight_icon_render_mode",
								     "Prelight Icon Render Mode",
								     "Mode of prelight icons being rendered (0=normal, 1=spotlight, 2=colorize, 3=colorize-monochromely)",
								     0, 3,
								     DEFAULT_PRELIGHT_ICON_RENDER_MODE,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("normal_icon_color",
								     "Icon Normal Color",
								     "Color used for colorizing icons in normal state (default base[NORMAL])",
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boxed ("prelight_icon_color",
								     "Icon Prelight Color",
								     "Color used for colorizing prelighted icons (default base[PRELIGHT])",
								     GDK_TYPE_COLOR,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uint ("normal_icon_saturation",
								     "Normal Icon Saturation",
								     "Saturation of icons in normal state",
								     0, 255,
								     DEFAULT_NORMAL_ICON_SATURATION,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uint ("prelight_icon_saturation",
								     "Prelight Icon Saturation",
								     "Saturation of icons in prelight state",
								     0, 255,
								     DEFAULT_PRELIGHT_ICON_SATURATION,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uint ("normal_icon_brightness",
								     "Normal Icon Brightness",
								     "Brightness of icons in normal state",
								     0, 255,
								     DEFAULT_NORMAL_ICON_BRIGHTNESS,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uint ("prelight_icon_brightness",
								     "Prelight Icon Brightness",
								     "Brightness of icons in prelight state",
								     0, 255,
								     DEFAULT_PRELIGHT_ICON_BRIGHTNESS,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uint ("normal_icon_lighten",
								     "Normal Icon Lighten",
								     "Lighten icons in normal state",
								     0, 255,
								     DEFAULT_NORMAL_ICON_LIGHTEN,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_uint ("prelight_icon_lighten",
								     "Prelight Icon Lighten",
								     "Lighten icons in prelight state",
								     0, 255,
								     DEFAULT_PRELIGHT_ICON_LIGHTEN,
								     G_PARAM_READABLE));
	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boolean ("activate_prelight_icon_label",
								     "Activate Prelight Icon Label",
								     "Whether icon labels should make use of its prelight color in prelight state",
								     FALSE,
								     G_PARAM_READABLE));
								     								     

	binding_set = gtk_binding_set_by_class (class);

	gtk_binding_entry_add_signal (binding_set, GDK_f, GDK_CONTROL_MASK, "start_interactive_search", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_F, GDK_CONTROL_MASK, "start_interactive_search", 0);
}

static void
update_selected (NautilusIconContainer *container)
{
	GList *node;
	NautilusIcon *icon;
	
	for (node = container->details->icons; node != NULL; node = node->next) {
		icon = node->data;
		if (icon->is_selected) {
			eel_canvas_item_request_update (EEL_CANVAS_ITEM (icon->item));
		}
	}
}

static gboolean
handle_focus_in_event (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	update_selected (NAUTILUS_ICON_CONTAINER (widget));

	return FALSE;
}

static gboolean
handle_focus_out_event (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	/* End renaming and commit change. */
	end_renaming_mode (NAUTILUS_ICON_CONTAINER (widget), TRUE);
	update_selected (NAUTILUS_ICON_CONTAINER (widget));

	return FALSE;
}

static void
nautilus_icon_container_instance_init (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	EelBackground *background;
	
	details = g_new0 (NautilusIconContainerDetails, 1);

	details->icon_set = g_hash_table_new (g_direct_hash, g_direct_equal);

        details->zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;

	details->font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = -2 * PANGO_SCALE;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER] = -2 * PANGO_SCALE;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL] = -0 * PANGO_SCALE;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] = 0 * PANGO_SCALE;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE] = 0 * PANGO_SCALE;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER] = 0 * PANGO_SCALE;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST] = 0 * PANGO_SCALE;

	container->details = details;

	/* when the background changes, we must set up the label text color */
	background = eel_get_widget_background (GTK_WIDGET (container));
	
	g_signal_connect_object (background, "appearance_changed",
				 G_CALLBACK (update_label_color), container, 0);

	g_signal_connect (container, "focus-in-event",
			  G_CALLBACK (handle_focus_in_event), NULL);
	g_signal_connect (container, "focus-out-event",
			  G_CALLBACK (handle_focus_out_event), NULL);

	eel_background_set_use_base (background, TRUE);
	
	/* read in theme-dependent data */
	nautilus_icon_container_theme_changed (container);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
				      nautilus_icon_container_theme_changed,
				      container);	
}

typedef struct {
	NautilusIconContainer *container;
	GdkEventButton	      *event;
} ContextMenuParameters;

/* NautilusIcon event handling.  */

/* Conceptually, pressing button 1 together with CTRL or SHIFT toggles
 * selection of a single icon without affecting the other icons;
 * without CTRL or SHIFT, it selects a single icon and un-selects all
 * the other icons.  But in this latter case, the de-selection should
 * only happen when the button is released if the icon is already
 * selected, because the user might select multiple icons and drag all
 * of them by doing a simple click-drag.
*/

static gboolean
handle_icon_button_press (NautilusIconContainer *container,
			  NautilusIcon *icon,
			  GdkEventButton *event)
{
	NautilusIconContainerDetails *details;

	details = container->details;

	if (event->type == GDK_3BUTTON_PRESS) {
		return TRUE;
	}

	if (details->single_click_mode &&
	    event->type == GDK_2BUTTON_PRESS) {
		/* Don't care about double clicks in single click mode */
		return TRUE;
	}
	
	if (event->button != DRAG_BUTTON
	    && event->button != CONTEXTUAL_MENU_BUTTON
	    && event->button != DRAG_MENU_BUTTON) {
		return TRUE;
	}

	if ((event->button == DRAG_BUTTON || event->button == MIDDLE_BUTTON) &&
	    event->type == GDK_BUTTON_PRESS) {
		/* The next double click has to be on this icon */
		details->double_click_icon[1] = details->double_click_icon[0];
		details->double_click_icon[0] = icon;
	}
	if (event->type == GDK_2BUTTON_PRESS &&
	    (event->button == DRAG_BUTTON || event->button == MIDDLE_BUTTON)) {
		/* Double clicking does not trigger a D&D action. */
		details->drag_button = 0;
		details->drag_icon = NULL;
		
		if (icon == details->double_click_icon[1]) {
			if (!button_event_modifies_selection (event)) {
				if (event->button == MIDDLE_BUTTON) {
					activate_selected_items_alternate (container, NULL);
				} else {
					activate_selected_items (container);
				}
			} else if (event->button == DRAG_BUTTON &&
				   (event->state & GDK_SHIFT_MASK) != 0) {
				activate_selected_items_alternate (container, icon);
			}
		}
		return TRUE;
	}
	if (event->button == DRAG_BUTTON
	    || event->button == DRAG_MENU_BUTTON) {
		details->drag_button = event->button;
		details->drag_icon = icon;
		details->drag_x = event->x;
		details->drag_y = event->y;
		details->drag_state = DRAG_STATE_MOVE_OR_COPY;
		details->drag_started = FALSE;

		/* Check to see if this is a click on the stretch handles.
		 * If so, it won't modify the selection.
		 */
		if (icon == container->details->stretch_icon) {
			if (start_stretching (container)) {
				return TRUE;
			}
		}
	}

	/* Modify the selection as appropriate. Selection is modified
	 * the same way for contextual menu as it would be without. 
	 */
	details->icon_selected_on_button_down = icon->is_selected;
	
	if ((event->button == DRAG_BUTTON || event->button == MIDDLE_BUTTON) &&
	    (event->state & GDK_CONTROL_MASK) == 0 &&
	    (event->state & GDK_SHIFT_MASK) != 0) {
		NautilusIcon *start_icon;

		start_icon = details->range_selection_base_icon;
		if (start_icon == NULL || !start_icon->is_selected) {
			start_icon = icon;
			details->range_selection_base_icon = icon;
		} 
		if (select_range (container, start_icon, icon)) {
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		}
	} else if (!details->icon_selected_on_button_down) {
		details->range_selection_base_icon = icon;
		if (button_event_modifies_selection (event)) {
			icon_toggle_selected (container, icon);
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		} else {
			select_one_unselect_others (container, icon);
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		}
	}

	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		g_signal_emit (container,
			       signals[CONTEXT_CLICK_SELECTION], 0,
			       event);
	}


	return TRUE;
}

static int
item_event_callback (EelCanvasItem *item,
		     GdkEvent *event,
		     gpointer data)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;

	container = NAUTILUS_ICON_CONTAINER (data);
	details = container->details;

	icon = NAUTILUS_ICON_CANVAS_ITEM (item)->user_data;
	g_assert (icon != NULL);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
		if (handle_icon_button_press (container, icon, &event->button)) {
			/* Stop the event from being passed along further. Returning
			 * TRUE ain't enough. 
			 */
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

GtkWidget *
nautilus_icon_container_new (void)
{
	return gtk_widget_new (nautilus_icon_container_get_type (), NULL);
}

/* Clear all of the icons in the container. */
void
nautilus_icon_container_clear (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	details = container->details;

	if (details->icons == NULL) {
		return;
	}

	end_renaming_mode (container, TRUE);
	
	clear_keyboard_focus (container);
	clear_keyboard_rubberband_start (container);
	unschedule_keyboard_icon_reveal (container);
	set_pending_icon_to_reveal (container, NULL);
	details->stretch_icon = NULL;
	details->drop_target = NULL;

	for (p = details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->is_monitored) {
			nautilus_icon_container_stop_monitor_top_left (container,
								       icon->data,
								       icon);
		}
		icon_free (p->data);
	}
	g_list_free (details->icons);
	details->icons = NULL;
	g_list_free (details->new_icons);
	details->new_icons = NULL;
	
 	g_hash_table_destroy (details->icon_set);
 	details->icon_set = g_hash_table_new (g_direct_hash, g_direct_equal);
 
	nautilus_icon_container_update_scroll_region (container);
}

gboolean
nautilus_icon_container_is_empty (NautilusIconContainer *container)
{
	return container->details->icons == NULL;
}

NautilusIconData *
nautilus_icon_container_get_first_visible_icon (NautilusIconContainer *container)
{
	GList *l;
	NautilusIcon *icon, *best_icon;
	GtkAdjustment *vadj;
	double x, y;
	double x1, y1, x2, y2;
	double best_y1;

	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (container));

	eel_canvas_c2w (EEL_CANVAS (container),
			0, vadj->value,
			&x, &y);

	l = container->details->icons;
	best_icon = NULL;
	best_y1 = 0;
	while (l != NULL) {
		icon = l->data;

		if (icon_is_positioned (icon)) {
			eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon->item),
						    &x1, &y1, &x2, &y2);
			if (y2 > y) {
				if (best_icon != NULL) {
					if (best_y1 > y1) {
						best_icon = icon;
						best_y1 = y1;
					}
				} else {
					best_icon = icon;
					best_y1 = y1;
				}
			}
		}
		
		l = l->next;
	}

	return best_icon ? best_icon->data : NULL;
}

/* puts the icon at the top of the screen */
void
nautilus_icon_container_scroll_to_icon (NautilusIconContainer  *container,
					NautilusIconData       *data)
{
	GList *l;
	NautilusIcon *icon;
	GtkAdjustment *vadj;
	int x, y;
	double x1, y1, x2, y2;
	EelCanvasItem *item;

	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (container));

	/* We need to force a relayout now if there are updates queued
	 * since we need the final positions */
	nautilus_icon_container_layout_now (container);
	
	l = container->details->icons;
	while (l != NULL) {
		icon = l->data;
		
		if (icon->data == data &&
		    icon_is_positioned (icon)) {
			item = EEL_CANVAS_ITEM (icon->item);
			eel_canvas_item_get_bounds (item,
						    &x1, &y1, &x2, &y2);
			eel_canvas_item_i2w (item->parent,
					     &x1,
					     &y1);
			eel_canvas_w2c (item->canvas,
					x1, y1,
					&x, &y);

			y -= ICON_PAD_TOP;

			y = MAX (0, y);
			
			eel_gtk_adjustment_set_value (vadj, y);
		}
		
		l = l->next;
	}
}

/* Call a function for all the icons. */
typedef struct {
	NautilusIconCallback callback;
	gpointer callback_data;
} CallbackAndData;

static void
call_icon_callback (gpointer data, gpointer callback_data)
{
	NautilusIcon *icon;
	CallbackAndData *callback_and_data;

	icon = data;
	callback_and_data = callback_data;
	(* callback_and_data->callback) (icon->data, callback_and_data->callback_data);
}

void
nautilus_icon_container_for_each (NautilusIconContainer *container,
				  NautilusIconCallback callback,
				  gpointer callback_data)
{
	CallbackAndData callback_and_data;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	callback_and_data.callback = callback;
	callback_and_data.callback_data = callback_data;

	g_list_foreach (container->details->icons,
			call_icon_callback, &callback_and_data);
}

/* utility routine to remove a single icon from the container */

static void
icon_destroy (NautilusIconContainer *container,
	      NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;
	gboolean was_selected;
	NautilusIcon *icon_to_focus;
	GList *item;
 
	details = container->details;

	item = g_list_find (details->icons, icon);
	item = item->next ? item->next : item->prev;
	icon_to_focus = (item != NULL) ? item->data : NULL;
 
	details->icons = g_list_remove (details->icons, icon);
	details->new_icons = g_list_remove (details->new_icons, icon);
	g_hash_table_remove (details->icon_set, icon->data);

	was_selected = icon->is_selected;

	if (details->keyboard_focus == icon ||
	    details->keyboard_focus == NULL) {
		if (icon_to_focus != NULL) {
			set_keyboard_focus (container, icon_to_focus);
		} else {
			clear_keyboard_focus (container);
		}
	}
	
	if (details->keyboard_rubberband_start == icon) {
		clear_keyboard_rubberband_start (container);
	}

	if (details->keyboard_icon_to_reveal == icon) {
		unschedule_keyboard_icon_reveal (container);
	}
	if (details->drag_icon == icon) {
		clear_drag_state (container);
	}
	if (details->drop_target == icon) {
		details->drop_target = NULL;
	}
	if (details->range_selection_base_icon == icon) {
		details->range_selection_base_icon = NULL;
	}
	if (details->pending_icon_to_reveal == icon) {
		set_pending_icon_to_reveal (container, NULL);
	}
	if (details->stretch_icon == icon) {
		details->stretch_icon = NULL;
	}

	if (icon->is_monitored) {
		nautilus_icon_container_stop_monitor_top_left (container,
							       icon->data,
							       icon);
	}
	icon_free (icon);

	if (was_selected) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/* activate any selected items in the container */
static void
activate_selected_items (NautilusIconContainer *container)
{
	GList *selection;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	selection = nautilus_icon_container_get_selection (container);
	if (selection != NULL) {
	  	g_signal_emit (container,
				 signals[ACTIVATE], 0,
				 selection);
	}
	g_list_free (selection);
}

static void
activate_selected_items_alternate (NautilusIconContainer *container,
				   NautilusIcon *icon)
{
	GList *selection;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));

	if (icon != NULL) {
		selection = g_list_prepend (NULL, icon->data);
	} else {
		selection = nautilus_icon_container_get_selection (container);
	}
	if (selection != NULL) {
	  	g_signal_emit (container,
				 signals[ACTIVATE_ALTERNATE], 0,
				 selection);
	}
	g_list_free (selection);
}

static NautilusIcon *
get_icon_being_renamed (NautilusIconContainer *container)
{
	NautilusIcon *rename_icon;

	if (!is_renaming (container)) {
		return NULL;
	}

	g_assert (!has_multiple_selection (container));

	rename_icon = get_first_selected_icon (container);
	g_assert (rename_icon != NULL);

	return rename_icon;
}			 

static NautilusIconInfo *
nautilus_icon_container_get_icon_images (NautilusIconContainer *container,
					 NautilusIconData      *data,
					 int                    size,
					 GList                **emblem_pixbufs,
					 char                 **embedded_text,
					 gboolean               for_drag_accept,
					 gboolean               need_large_embeddded_text,
					 gboolean              *embedded_text_needs_loading,
					 gboolean              *has_open_window)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->get_icon_images != NULL);

	return klass->get_icon_images (container, data, size, emblem_pixbufs, embedded_text, for_drag_accept, need_large_embeddded_text, embedded_text_needs_loading, has_open_window);
}


static void
nautilus_icon_container_get_icon_text (NautilusIconContainer *container,
				       NautilusIconData      *data,
				       char                 **editable_text,
				       char                 **additional_text)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->get_icon_text != NULL);

	klass->get_icon_text (container, data, editable_text, additional_text);
}

static void
nautilus_icon_container_freeze_updates (NautilusIconContainer *container)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->freeze_updates != NULL);

	klass->freeze_updates (container);
}

static void
nautilus_icon_container_unfreeze_updates (NautilusIconContainer *container)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->unfreeze_updates != NULL);

	klass->unfreeze_updates (container);
}

static void
nautilus_icon_container_start_monitor_top_left (NautilusIconContainer *container,
						NautilusIconData *data,
						gconstpointer client,
						gboolean large_text)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->start_monitor_top_left != NULL);

	klass->start_monitor_top_left (container, data, client, large_text);
}

static void
nautilus_icon_container_stop_monitor_top_left (NautilusIconContainer *container,
					       NautilusIconData *data,
					       gconstpointer client)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);
	g_return_if_fail (klass->stop_monitor_top_left != NULL);

	klass->stop_monitor_top_left (container, data, client);
}


static void
nautilus_icon_container_prioritize_thumbnailing (NautilusIconContainer *container,
						 NautilusIcon *icon)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->prioritize_thumbnailing != NULL);

	klass->prioritize_thumbnailing (container, icon->data);
}

static void
nautilus_icon_container_update_visible_icons (NautilusIconContainer *container)
{
	GtkAdjustment *vadj;
	double min_y, max_y;
	double x0, y0, x1, y1;
	GList *node;
	NautilusIcon *icon;


	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (container));
	
	min_y = vadj->value;
	max_y = min_y + GTK_WIDGET (container)->allocation.height;

	eel_canvas_c2w (EEL_CANVAS (container),
			0, min_y, NULL, &min_y);
	eel_canvas_c2w (EEL_CANVAS (container),
			0, max_y, NULL, &max_y);
	
	/* Do the iteration in reverse to get the render-order from top to
	 * bottom for the prioritized thumbnails.
	 */
	for (node = g_list_last (container->details->icons); node != NULL; node = node->prev) {
		icon = node->data;

		if (icon_is_positioned (icon)) {
			eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon->item),
						    &x0,
						    &y0,
						    &x1,
						    &y1);
			eel_canvas_item_i2w (EEL_CANVAS_ITEM (icon->item)->parent,
					     &x0,
					     &y0);
			eel_canvas_item_i2w (EEL_CANVAS_ITEM (icon->item)->parent,
					     &x1,
					     &y1);
			if (y1 >= min_y && y0 <= max_y) {
				nautilus_icon_canvas_item_set_is_visible (icon->item, TRUE);
				nautilus_icon_container_prioritize_thumbnailing (container,
										 icon);
			} else {
				nautilus_icon_canvas_item_set_is_visible (icon->item, FALSE);
			}
		}
	}
}

static void
handle_vadjustment_changed (GtkAdjustment *adjustment,
			    NautilusIconContainer *container)
{
	nautilus_icon_container_update_visible_icons (container);
}

void 
nautilus_icon_container_update_icon (NautilusIconContainer *container,
				     NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;
	guint icon_size;
	guint min_image_size, max_image_size;
	NautilusIconInfo *icon_info;
	GdkPoint *attach_points;
	int n_attach_points;
	gboolean has_embedded_text_rect;
	GdkPixbuf *pixbuf;
	GList *emblem_pixbufs;
	char *editable_text, *additional_text;
	char *embedded_text;
	GdkRectangle embedded_text_rect;
	gboolean large_embedded_text;
	gboolean embedded_text_needs_loading;
	gboolean has_open_window;
	
	if (icon == NULL) {
		return;
	}

	details = container->details;

	/* compute the maximum size based on the scale factor */
	min_image_size = MINIMUM_IMAGE_SIZE * EEL_CANVAS (container)->pixels_per_unit;
	max_image_size = MAX (MAXIMUM_IMAGE_SIZE * EEL_CANVAS (container)->pixels_per_unit, NAUTILUS_ICON_MAXIMUM_SIZE);

	/* Get the appropriate images for the file. */
	if (container->details->forced_icon_size > 0) {
		icon_size = container->details->forced_icon_size;
	} else {
		icon_get_size (container, icon, &icon_size);
	}


	icon_size = MAX (icon_size, min_image_size);
	icon_size = MIN (icon_size, max_image_size);

	/* Get the icons. */
	emblem_pixbufs = NULL;
	embedded_text = NULL;
	large_embedded_text = icon_size > ICON_SIZE_FOR_LARGE_EMBEDDED_TEXT;
	icon_info = nautilus_icon_container_get_icon_images (container, icon->data, icon_size,
							     &emblem_pixbufs,
							     &embedded_text,
							     icon == details->drop_target,							     
							     large_embedded_text, &embedded_text_needs_loading,
							     &has_open_window);


	if (container->details->forced_icon_size > 0)
		pixbuf = nautilus_icon_info_get_pixbuf_at_size (icon_info, icon_size);
	else
		pixbuf = nautilus_icon_info_get_pixbuf (icon_info);
	nautilus_icon_info_get_attach_points (icon_info, &attach_points, &n_attach_points);
	has_embedded_text_rect = nautilus_icon_info_get_embedded_rect (icon_info,
								       &embedded_text_rect);
 
	if (has_embedded_text_rect && embedded_text_needs_loading) {
		icon->is_monitored = TRUE;
		nautilus_icon_container_start_monitor_top_left (container, icon->data, icon, large_embedded_text);
	}
	
	nautilus_icon_container_get_icon_text (container,
					       icon->data,
					       &editable_text,
					       &additional_text);

	/* If name of icon being renamed was changed from elsewhere, end renaming mode. 
	 * Alternatively, we could replace the characters in the editable text widget
	 * with the new name, but that could cause timing problems if the user just
	 * happened to be typing at that moment.
	 */
	if (icon == get_icon_being_renamed (container) &&
	    eel_strcmp (editable_text, 
			nautilus_icon_canvas_item_get_editable_text (icon->item)) != 0) {
		end_renaming_mode (container, FALSE);
	}

	eel_canvas_item_set (EEL_CANVAS_ITEM (icon->item),
			     "editable_text", editable_text,
			     "additional_text", additional_text,
			     "highlighted_for_drop", icon == details->drop_target,
			     NULL);

	nautilus_icon_canvas_item_set_image (icon->item, pixbuf);
	nautilus_icon_canvas_item_set_attach_points (icon->item, attach_points, n_attach_points);
	nautilus_icon_canvas_item_set_emblems (icon->item, emblem_pixbufs);
	nautilus_icon_canvas_item_set_embedded_text_rect (icon->item, &embedded_text_rect);
	nautilus_icon_canvas_item_set_embedded_text (icon->item, embedded_text);

	/* Let the pixbufs go. */
	g_object_unref (pixbuf);
	eel_gdk_pixbuf_list_free (emblem_pixbufs);

	g_free (editable_text);
	g_free (additional_text);

	g_object_unref (icon_info);
}

static gboolean
assign_icon_position (NautilusIconContainer *container,
		      NautilusIcon *icon)
{
	gboolean have_stored_position;
	NautilusIconPosition position;

	/* Get the stored position. */
	have_stored_position = FALSE;
	position.scale = 1.0;
	g_signal_emit (container,
			 signals[GET_STORED_ICON_POSITION], 0,
			 icon->data,
			 &position,
			 &have_stored_position);
	icon->scale = position.scale;
	if (!container->details->auto_layout) {
		if (have_stored_position) {
			icon_set_position (icon, position.x, position.y);
			icon->saved_ltr_x = icon->x;
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

static void
finish_adding_icon (NautilusIconContainer *container,
		    NautilusIcon *icon)
{
	nautilus_icon_container_update_icon (container, icon);
	eel_canvas_item_show (EEL_CANVAS_ITEM (icon->item));

	g_signal_connect_object (icon->item, "event",
				 G_CALLBACK (item_event_callback), container, 0);

	g_signal_emit (container, signals[ICON_ADDED], 0, icon->data);
}

static void
finish_adding_new_icons (NautilusIconContainer *container)
{
	GList *p, *new_icons, *no_position_icons, *semi_position_icons;
	NautilusIcon *icon;
	double bottom;

	new_icons = container->details->new_icons;
	container->details->new_icons = NULL;

	/* Position most icons (not unpositioned manual-layout icons). */
	new_icons = g_list_reverse (new_icons);
	no_position_icons = semi_position_icons = NULL;
	for (p = new_icons; p != NULL; p = p->next) {
		icon = p->data;
                if (assign_icon_position (container, icon)) {
                        if (!container->details->auto_layout && icon->has_lazy_position) {
                                semi_position_icons = g_list_prepend (semi_position_icons, icon);
                        }
                } else {
                        no_position_icons = g_list_prepend (no_position_icons, icon);
                }

		finish_adding_icon (container, icon);
	}
	g_list_free (new_icons);

	if (semi_position_icons != NULL) {
		PlacementGrid *grid;

		g_assert (!container->details->auto_layout);

		semi_position_icons = g_list_reverse (semi_position_icons);

		/* This is currently only used on the desktop.
		 * Thus, we pass FALSE for tight, like lay_down_icons_tblr */
		grid = placement_grid_new (container, FALSE);

		for (p = container->details->icons; p != NULL; p = p->next) {
			icon = p->data;

			if (icon_is_positioned (icon) && !icon->has_lazy_position) {
				placement_grid_mark_icon (grid, icon);
			}
		}

		for (p = semi_position_icons; p != NULL; p = p->next) {
			NautilusIcon *icon;
			int x, y;

			icon = p->data;
			x = icon->x;
			y = icon->y;

			find_empty_location (container, grid, 
					     icon, x, y, &x, &y);

			icon_set_position (icon, x, y);

			placement_grid_mark_icon (grid, icon);

			/* ensure that next time we run this code, the formerly semi-positioned
			 * icons are treated as being positioned. */
			icon->has_lazy_position = FALSE;
		}

		placement_grid_free (grid);

		g_list_free (semi_position_icons);
	}

	/* Position the unpositioned manual layout icons. */
	if (no_position_icons != NULL) {
		g_assert (!container->details->auto_layout);
		
		sort_icons (container, &no_position_icons);
		get_all_icon_bounds (container, NULL, NULL, NULL, &bottom);		
		lay_down_icons (container, no_position_icons, bottom + ICON_PAD_BOTTOM);
		g_list_free (no_position_icons);
	}
}

/**
 * nautilus_icon_container_add:
 * @container: A NautilusIconContainer
 * @data: Icon data.
 * @has_lazy_position: Whether the saved icon position should only be used
 * 		       if the previous icon position is free. If the position
 * 		       is occupied, another position near the last one will
 * 		       be used.
 * 
 * Add icon to represent @data to container.
 * Returns FALSE if there was already such an icon.
 **/
gboolean
nautilus_icon_container_add (NautilusIconContainer *container,
			     NautilusIconData *data,
			     gboolean has_lazy_position)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	EelCanvasItem *band, *item;
	
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	details = container->details;

	if (g_hash_table_lookup (details->icon_set, data) != NULL) {
		return FALSE;
	}

	/* Create the new icon, including the canvas item. */
	icon = g_new0 (NautilusIcon, 1);
	icon->data = data;
	icon->x = ICON_UNPOSITIONED_VALUE;
	icon->y = ICON_UNPOSITIONED_VALUE;
	icon->has_lazy_position = has_lazy_position;
	icon->scale = 1.0;
 	icon->item = NAUTILUS_ICON_CANVAS_ITEM
		(eel_canvas_item_new (EEL_CANVAS_GROUP (EEL_CANVAS (container)->root),
				      nautilus_icon_canvas_item_get_type (),
				      "visible", FALSE,
				      NULL));
	icon->item->user_data = icon;

	/* Make sure the icon is under the selection_rectangle */
	item = EEL_CANVAS_ITEM (icon->item);
	band = NAUTILUS_ICON_CONTAINER (item->canvas)->details->rubberband_info.selection_rectangle;
	if (band) {
		eel_canvas_item_send_behind (item, band);
	}
	
	/* Put it on both lists. */
	details->icons = g_list_prepend (details->icons, icon);
	details->new_icons = g_list_prepend (details->new_icons, icon);

	g_hash_table_insert (details->icon_set, data, icon);

	/* Run an idle function to add the icons. */
	schedule_redo_layout (container);
	
	return TRUE;
}

void
nautilus_icon_container_layout_now (NautilusIconContainer *container)
{
	if (container->details->idle_id != 0) {
		unschedule_redo_layout (container);
		redo_layout_internal (container);
	}
}

/**
 * nautilus_icon_container_remove:
 * @container: A NautilusIconContainer.
 * @data: Icon data.
 * 
 * Remove the icon with this data.
 **/
gboolean
nautilus_icon_container_remove (NautilusIconContainer *container,
				NautilusIconData *data)
{
	NautilusIcon *icon;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	end_renaming_mode (container, FALSE);
		
	icon = g_hash_table_lookup (container->details->icon_set, data);

	if (icon == NULL) {
		return FALSE;
	}

	icon_destroy (container, icon);
	schedule_redo_layout (container);

	g_signal_emit (container, signals[ICON_REMOVED], 0, icon);

	return TRUE;
}

/**
 * nautilus_icon_container_request_update:
 * @container: A NautilusIconContainer.
 * @data: Icon data.
 * 
 * Update the icon with this data.
 **/
void
nautilus_icon_container_request_update (NautilusIconContainer *container,
					NautilusIconData *data)
{
	NautilusIcon *icon;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	icon = g_hash_table_lookup (container->details->icon_set, data);

	if (icon != NULL) {
		nautilus_icon_container_update_icon (container, icon);
		schedule_redo_layout (container);
	}
}

/* zooming */

NautilusZoomLevel
nautilus_icon_container_get_zoom_level (NautilusIconContainer *container)
{
        return container->details->zoom_level;
}

void
nautilus_icon_container_set_zoom_level (NautilusIconContainer *container, int new_level)
{
	NautilusIconContainerDetails *details;
        int pinned_level;
	double pixels_per_unit;
	
	details = container->details;

	end_renaming_mode (container, TRUE);
		
	pinned_level = new_level;
        if (pinned_level < NAUTILUS_ZOOM_LEVEL_SMALLEST) {
		pinned_level = NAUTILUS_ZOOM_LEVEL_SMALLEST;
        } else if (pinned_level > NAUTILUS_ZOOM_LEVEL_LARGEST) {
        	pinned_level = NAUTILUS_ZOOM_LEVEL_LARGEST;
	}

        if (pinned_level == details->zoom_level) {
		return;
	}
	
	details->zoom_level = pinned_level;
	
	pixels_per_unit = (double) nautilus_get_icon_size_for_zoom_level (pinned_level)
		/ NAUTILUS_ICON_SIZE_STANDARD;
	eel_canvas_set_pixels_per_unit (EEL_CANVAS (container), pixels_per_unit);

	invalidate_label_sizes (container);
	nautilus_icon_container_request_update_all (container);
}

/**
 * nautilus_icon_container_request_update_all:
 * For each icon, synchronizes the displayed information (image, text) with the
 * information from the model.
 * 
 * @container: An icon container.
 **/
void
nautilus_icon_container_request_update_all (NautilusIconContainer *container)
{
	GList *node;
	NautilusIcon *icon;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	for (node = container->details->icons; node != NULL; node = node->next) {
		icon = node->data;
		nautilus_icon_container_update_icon (container, icon);
	}

	redo_layout (container);
}

/**
 * nautilus_icon_container_reveal:
 * Change scroll position as necessary to reveal the specified item.
 */
void
nautilus_icon_container_reveal (NautilusIconContainer *container, NautilusIconData *data)
{
	NautilusIcon *icon;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	icon = g_hash_table_lookup (container->details->icon_set, data);

	if (icon != NULL) {
		reveal_icon (container, icon);
	}
}

/**
 * nautilus_icon_container_get_selection:
 * @container: An icon container.
 * 
 * Get a list of the icons currently selected in @container.
 * 
 * Return value: A GList of the programmer-specified data associated to each
 * selected icon, or NULL if no icon is selected.  The caller is expected to
 * free the list when it is not needed anymore.
 **/
GList *
nautilus_icon_container_get_selection (NautilusIconContainer *container)
{
	GList *list, *p;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), NULL);

	list = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		if (icon->is_selected) {
			list = g_list_prepend (list, icon->data);
		}
	}

	return g_list_reverse (list);
}

static GList *
nautilus_icon_container_get_selected_icons (NautilusIconContainer *container)
{
	GList *list, *p;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), NULL);

	list = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		if (icon->is_selected) {
			list = g_list_prepend (list, icon);
		}
	}

	return g_list_reverse (list);
}

/**
 * nautilus_icon_container_invert_selection:
 * @container: An icon container.
 * 
 * Inverts the selection in @container.
 * 
 **/
void
nautilus_icon_container_invert_selection (NautilusIconContainer *container)
{
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		icon_toggle_selected (container, icon);
	}
}


/* Returns an array of GdkPoints of locations of the icons. */
static GArray *
nautilus_icon_container_get_icon_locations (NautilusIconContainer *container,
					    GList *icons)
{
	GArray *result;
	GList *node;
	int index;
		
	result = g_array_new (FALSE, TRUE, sizeof (GdkPoint));
	result = g_array_set_size (result, g_list_length (icons));
		
	for (index = 0, node = icons; node != NULL; index++, node = node->next) {
	     	g_array_index (result, GdkPoint, index).x =
	     		((NautilusIcon *)node->data)->x;
	     	g_array_index (result, GdkPoint, index).y =
			((NautilusIcon *)node->data)->y;
	}

	return result;
}

/**
 * nautilus_icon_container_get_selected_icon_locations:
 * @container: An icon container widget.
 * 
 * Returns an array of GdkPoints of locations of the selected icons.
 **/
GArray *
nautilus_icon_container_get_selected_icon_locations (NautilusIconContainer *container)
{
	GArray *result;
	GList *icons;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), NULL);

	icons = nautilus_icon_container_get_selected_icons (container);
	result = nautilus_icon_container_get_icon_locations (container, icons);
	g_list_free (icons);
	
	return result;
}

/**
 * nautilus_icon_container_select_all:
 * @container: An icon container widget.
 * 
 * Select all the icons in @container at once.
 **/
void
nautilus_icon_container_select_all (NautilusIconContainer *container)
{
	gboolean selection_changed;
	GList *p;
	NautilusIcon *icon;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	selection_changed = FALSE;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		
		selection_changed |= icon_set_selected (container, icon, TRUE);
	}

	if (selection_changed) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/**
 * nautilus_icon_container_set_selection:
 * @container: An icon container widget.
 * @selection: A list of NautilusIconData *.
 * 
 * Set the selection to exactly the icons in @container which have
 * programmer data matching one of the items in @selection.
 **/
void
nautilus_icon_container_set_selection (NautilusIconContainer *container,
				       GList *selection)
{
	gboolean selection_changed;
	GHashTable *hash;
	GList *p;
	NautilusIcon *icon;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	selection_changed = FALSE;

	hash = g_hash_table_new (NULL, NULL);
	for (p = selection; p != NULL; p = p->next) {
		g_hash_table_insert (hash, p->data, p->data);
	}
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		
		selection_changed |= icon_set_selected
			(container, icon,
			 g_hash_table_lookup (hash, icon->data) != NULL);
	}
	g_hash_table_destroy (hash);

	if (selection_changed) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/**
 * nautilus_icon_container_select_list_unselect_others.
 * @container: An icon container widget.
 * @selection: A list of NautilusIcon *.
 * 
 * Set the selection to exactly the icons in @selection.
 **/
void
nautilus_icon_container_select_list_unselect_others (NautilusIconContainer *container,
						     GList *selection)
{
	gboolean selection_changed;
	GHashTable *hash;
	GList *p;
	NautilusIcon *icon;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	selection_changed = FALSE;

	hash = g_hash_table_new (NULL, NULL);
	for (p = selection; p != NULL; p = p->next) {
		g_hash_table_insert (hash, p->data, p->data);
	}
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		
		selection_changed |= icon_set_selected
			(container, icon,
			 g_hash_table_lookup (hash, icon) != NULL);
	}
	g_hash_table_destroy (hash);

	if (selection_changed) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/**
 * nautilus_icon_container_unselect_all:
 * @container: An icon container widget.
 * 
 * Deselect all the icons in @container.
 **/
void
nautilus_icon_container_unselect_all (NautilusIconContainer *container)
{
	if (unselect_all (container)) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/**
 * nautilus_icon_container_get_icon_by_uri:
 * @container: An icon container widget.
 * @uri: The uri of an icon to find.
 * 
 * Locate an icon, given the URI. The URI must match exactly.
 * Later we may have to have some way of figuring out if the
 * URI specifies the same object that does not require an exact match.
 **/
NautilusIcon *
nautilus_icon_container_get_icon_by_uri (NautilusIconContainer *container,
					 const char *uri)
{
	NautilusIconContainerDetails *details;
	GList *p;

	/* Eventually, we must avoid searching the entire icon list,
	   but it's OK for now.
	   A hash table mapping uri to icon is one possibility.
	*/

	details = container->details;

	for (p = details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;
		char *icon_uri;
		gboolean is_match;

		icon = p->data;

		icon_uri = nautilus_icon_container_get_icon_uri
			(container, icon);
		is_match = strcmp (uri, icon_uri) == 0;
		g_free (icon_uri);

		if (is_match) {
			return icon;
		}
	}

	return NULL;
}

static NautilusIcon *
get_nth_selected_icon (NautilusIconContainer *container, int index)
{
	GList *p;
	NautilusIcon *icon;
	int selection_count;

	g_assert (index > 0);

	/* Find the nth selected icon. */
	selection_count = 0;
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->is_selected) {
			if (++selection_count == index) {
				return icon;
			}
		}
	}
	return NULL;
}

static NautilusIcon *
get_first_selected_icon (NautilusIconContainer *container)
{
        return get_nth_selected_icon (container, 1);
}

static gboolean
has_multiple_selection (NautilusIconContainer *container)
{
        return get_nth_selected_icon (container, 2) != NULL;
}

static gboolean
all_selected (NautilusIconContainer *container)
{
	GList *p;
	NautilusIcon *icon;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (!icon->is_selected) {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
has_selection (NautilusIconContainer *container)
{
        return get_nth_selected_icon (container, 1) != NULL;
}

/**
 * nautilus_icon_container_show_stretch_handles:
 * @container: An icon container widget.
 * 
 * Makes stretch handles visible on the first selected icon.
 **/
void
nautilus_icon_container_show_stretch_handles (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	guint initial_size;
	
	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return;
	}

	/* Check if it already has stretch handles. */
	details = container->details;
	if (details->stretch_icon == icon) {
		return;
	}

	/* Get rid of the existing stretch handles and put them on the new icon. */
	if (details->stretch_icon != NULL) {
		nautilus_icon_canvas_item_set_show_stretch_handles
			(details->stretch_icon->item, FALSE);
		ungrab_stretch_icon (container);
		emit_stretch_ended (container, details->stretch_icon);
	}
	nautilus_icon_canvas_item_set_show_stretch_handles (icon->item, TRUE);
	details->stretch_icon = icon;
	
	icon_get_size (container, icon, &initial_size);

	/* only need to keep size in one dimension, since they are constrained to be the same */
	container->details->stretch_initial_x = icon->x;
	container->details->stretch_initial_y = icon->y;
	container->details->stretch_initial_size = initial_size;

	emit_stretch_started (container, icon);
}

/**
 * nautilus_icon_container_has_stretch_handles
 * @container: An icon container widget.
 * 
 * Returns true if the first selected item has stretch handles.
 **/
gboolean
nautilus_icon_container_has_stretch_handles (NautilusIconContainer *container)
{
	NautilusIcon *icon;

	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return FALSE;
	}

	return icon == container->details->stretch_icon;
}

/**
 * nautilus_icon_container_is_stretched
 * @container: An icon container widget.
 * 
 * Returns true if the any selected item is stretched to a size other than 1.0.
 **/
gboolean
nautilus_icon_container_is_stretched (NautilusIconContainer *container)
{
	GList *p;
	NautilusIcon *icon;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->is_selected && icon->scale != 1.0) {
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * nautilus_icon_container_unstretch
 * @container: An icon container widget.
 * 
 * Gets rid of any icon stretching.
 **/
void
nautilus_icon_container_unstretch (NautilusIconContainer *container)
{
	GList *p;
	NautilusIcon *icon;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->is_selected) {
			nautilus_icon_container_move_icon (container, icon,
							   icon->x, icon->y,
							   1.0,
							   FALSE, TRUE, TRUE);
		}
	}
}

static void
compute_stretch (StretchState *start,
		 StretchState *current)
{
	gboolean right, bottom;
	int x_stretch, y_stretch;

	/* FIXME bugzilla.gnome.org 45390: This doesn't correspond to
         * the way the handles are drawn.
	 */
	/* Figure out which handle we are dragging. */
	right = start->pointer_x > start->icon_x + (int) start->icon_size / 2;
	bottom = start->pointer_y > start->icon_y + (int) start->icon_size / 2;

	/* Figure out how big we should stretch. */
	x_stretch = start->pointer_x - current->pointer_x;
	y_stretch = start->pointer_y - current->pointer_y;
	if (right) {
		x_stretch = - x_stretch;
	}
	if (bottom) {
		y_stretch = - y_stretch;
	}
	current->icon_size = MAX ((int) start->icon_size + MIN (x_stretch, y_stretch),
				  (int) NAUTILUS_ICON_SIZE_SMALLEST);

	/* Figure out where the corner of the icon should be. */
	current->icon_x = start->icon_x;
	if (!right) {
		current->icon_x += start->icon_size - current->icon_size;
	}
	current->icon_y = start->icon_y;
	if (!bottom) {
		current->icon_y += start->icon_size - current->icon_size;
	}
}

char *
nautilus_icon_container_get_icon_uri (NautilusIconContainer *container,
				      NautilusIcon *icon)
{
	char *uri;

	uri = NULL;
	g_signal_emit (container,
			 signals[GET_ICON_URI], 0,
			 icon->data,
			 &uri);
	return uri;
}

char *
nautilus_icon_container_get_icon_drop_target_uri (NautilusIconContainer *container,
				   	     	  NautilusIcon *icon)
{
	char *uri;

	uri = NULL;
	g_signal_emit (container,
			 signals[GET_ICON_DROP_TARGET_URI], 0,
			 icon->data,
			 &uri);
	return uri;
}

/* Call to reset the scroll region only if the container is not empty,
 * to avoid having the flag linger until the next file is added.
 */
static void
reset_scroll_region_if_not_empty (NautilusIconContainer *container)
{
	if (!nautilus_icon_container_is_empty (container)) {
		nautilus_icon_container_reset_scroll_region (container);
	}
}

/* Switch from automatic layout to manual or vice versa.
 * If we switch to manual layout, we restore the icon positions from the
 * last manual layout.
 */
void
nautilus_icon_container_set_auto_layout (NautilusIconContainer *container,
					 gboolean auto_layout)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (auto_layout == FALSE || auto_layout == TRUE);

	if (container->details->auto_layout == auto_layout) {
		return;
	}

	reset_scroll_region_if_not_empty (container);
	container->details->auto_layout = auto_layout;

	if (!auto_layout) {
		reload_icon_positions (container);
		nautilus_icon_container_freeze_icon_positions (container);
	}

	redo_layout (container);

	g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
}


/* Toggle the tighter layout boolean. */
void
nautilus_icon_container_set_tighter_layout (NautilusIconContainer *container,
					    gboolean tighter_layout)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (tighter_layout == FALSE || tighter_layout == TRUE);

	if (container->details->tighter_layout == tighter_layout) {
		return;
	}

	container->details->tighter_layout = tighter_layout;

	if (container->details->auto_layout) {
		invalidate_label_sizes (container);
		redo_layout (container);

		g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
	} else {
		/* in manual layout, label sizes still change, even though
		 * the icons don't move.
		 */
		invalidate_label_sizes (container);	
		nautilus_icon_container_request_update_all (container);	
	}
}

gboolean
nautilus_icon_container_is_keep_aligned (NautilusIconContainer *container)
{
	return container->details->keep_aligned;
}

static gboolean
align_icons_callback (gpointer callback_data)
{
	NautilusIconContainer *container;

	container = NAUTILUS_ICON_CONTAINER (callback_data);
	align_icons (container);
	container->details->align_idle_id = 0;

	return FALSE;
}

static void
unschedule_align_icons (NautilusIconContainer *container)
{
        if (container->details->align_idle_id != 0) {
		g_source_remove (container->details->align_idle_id);
		container->details->align_idle_id = 0;
	}
}

static void
schedule_align_icons (NautilusIconContainer *container)
{
	if (container->details->align_idle_id == 0
	    && container->details->has_been_allocated) {
		container->details->align_idle_id = g_idle_add
			(align_icons_callback, container);
	}
}

void
nautilus_icon_container_set_keep_aligned (NautilusIconContainer *container,
					  gboolean keep_aligned)
{
	if (container->details->keep_aligned != keep_aligned) {
		container->details->keep_aligned = keep_aligned;
		
		if (keep_aligned && !container->details->auto_layout) {
			schedule_align_icons (container);
		} else {
			unschedule_align_icons (container);
		}
	}
}

void
nautilus_icon_container_set_layout_mode (NautilusIconContainer *container,
					 NautilusIconLayoutMode mode)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->layout_mode = mode;
	invalidate_label_sizes (container);

	redo_layout (container);

	g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
}

void
nautilus_icon_container_set_label_position (NautilusIconContainer *container,
					    NautilusIconLabelPosition position)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	if (container->details->label_position != position) {
		container->details->label_position = position;

		invalidate_label_sizes (container);
		nautilus_icon_container_request_update_all (container);

		schedule_redo_layout (container);
	}
}

/* Switch from automatic to manual layout, freezing all the icons in their
 * current positions instead of restoring icon positions from the last manual
 * layout as set_auto_layout does.
 */
void
nautilus_icon_container_freeze_icon_positions (NautilusIconContainer *container)
{
	gboolean changed;
	GList *p;
	NautilusIcon *icon;
	NautilusIconPosition position;

	changed = container->details->auto_layout;
	container->details->auto_layout = FALSE;
	
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		position.x = icon->saved_ltr_x;
		position.y = icon->y;
		position.scale = icon->scale;
		g_signal_emit (container, signals[ICON_POSITION_CHANGED], 0,
				 icon->data, &position);
	}

	if (changed) {
		g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
	}
}

/* Re-sort, switching to automatic layout if it was in manual layout. */
void
nautilus_icon_container_sort (NautilusIconContainer *container)
{
	gboolean changed;

	changed = !container->details->auto_layout;
	container->details->auto_layout = TRUE;

	reset_scroll_region_if_not_empty (container);
	redo_layout (container);

	if (changed) {
		g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
	}
}

gboolean
nautilus_icon_container_is_auto_layout (NautilusIconContainer *container)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);

	return container->details->auto_layout;
}

gboolean
nautilus_icon_container_is_tighter_layout (NautilusIconContainer *container)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);

	return container->details->tighter_layout;
}

static void
pending_icon_to_rename_destroy_callback (NautilusIconCanvasItem *item, NautilusIconContainer *container)
{
	g_assert (container->details->pending_icon_to_rename != NULL);
	g_assert (container->details->pending_icon_to_rename->item == item);
	container->details->pending_icon_to_rename = NULL;
}

static NautilusIcon*
get_pending_icon_to_rename (NautilusIconContainer *container)
{
	return container->details->pending_icon_to_rename;
}

static void
set_pending_icon_to_rename (NautilusIconContainer *container, NautilusIcon *icon)
{
	NautilusIcon *old_icon;
	
	old_icon = container->details->pending_icon_to_rename;
	
	if (icon == old_icon) {
		return;
	}
	
	if (old_icon != NULL) {
		g_signal_handlers_disconnect_by_func
			(old_icon->item,
			 G_CALLBACK (pending_icon_to_rename_destroy_callback),
			 container);
	}
	
	if (icon != NULL) {
		g_signal_connect (icon->item, "destroy",
				  G_CALLBACK (pending_icon_to_rename_destroy_callback), container);
	}
	
	container->details->pending_icon_to_rename = icon;
}

static void
process_pending_icon_to_rename (NautilusIconContainer *container)
{
	NautilusIcon *pending_icon_to_rename;
	
	pending_icon_to_rename = get_pending_icon_to_rename (container);
	
	if (pending_icon_to_rename != NULL) {
		if (pending_icon_to_rename->is_selected && !has_multiple_selection (container)) {
			nautilus_icon_container_start_renaming_selected_item (container, FALSE);
		} else {
			set_pending_icon_to_rename (container, NULL);
		}
	}
}

static gboolean
is_renaming_pending (NautilusIconContainer *container)
{
	return get_pending_icon_to_rename (container) != NULL;
}

static gboolean
is_renaming (NautilusIconContainer *container)
{
	return container->details->renaming;
}

/**
 * nautilus_icon_container_start_renaming_selected_item
 * @container: An icon container widget.
 * @select_all: Whether the whole file should initially be selected, or
 *              only its basename (i.e. everything except its extension).
 * 
 * Displays the edit name widget on the first selected icon
 **/
void
nautilus_icon_container_start_renaming_selected_item (NautilusIconContainer *container,
						      gboolean select_all)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	EelDRect icon_rect;
	EelDRect text_rect;
	PangoContext *context;
	PangoFontDescription *desc;
	const char *editable_text;
	int x, y, width;
	int start_offset, end_offset;

	/* Check if it already in renaming mode. */
	details = container->details;
	if (details->renaming) {
		return;
	}

	/* Find selected icon */
	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return;
	}

	g_assert (!has_multiple_selection (container));


	if (!icon_is_positioned (icon)) {
		set_pending_icon_to_rename (container, icon);
		return;
	}
	
	set_pending_icon_to_rename (container, NULL);

	/* Make a copy of the original editable text for a later compare */
	editable_text = nautilus_icon_canvas_item_get_editable_text (icon->item);

	/* This could conceivably be NULL if a rename was triggered really early. */
	if (editable_text == NULL) {
		return;
	}

	details->original_text = g_strdup (editable_text);
	
	/* Freeze updates so files added while renaming don't cause rename to loose focus, bug #318373 */
	nautilus_icon_container_freeze_updates (container);

	/* Create text renaming widget, if it hasn't been created already.
	 * We deal with the broken icon text item widget by keeping it around
	 * so its contents can still be cut and pasted as part of the clipboard
	 */
	if (details->rename_widget == NULL) {
		details->rename_widget = eel_editable_label_new ("Test text");
		eel_editable_label_set_line_wrap (EEL_EDITABLE_LABEL (details->rename_widget), TRUE);
		eel_editable_label_set_line_wrap_mode (EEL_EDITABLE_LABEL (details->rename_widget), PANGO_WRAP_WORD_CHAR);
		eel_editable_label_set_draw_outline (EEL_EDITABLE_LABEL (details->rename_widget), TRUE);

		if (details->label_position != NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
			eel_editable_label_set_justify (EEL_EDITABLE_LABEL (details->rename_widget), GTK_JUSTIFY_CENTER);
		}

		gtk_misc_set_padding (GTK_MISC (details->rename_widget), 1, 1);
		gtk_layout_put (GTK_LAYOUT (container),
				details->rename_widget, 0, 0);
	} 

	/* Set the right font */
	if (details->font) {
		desc = pango_font_description_from_string (details->font);
	} else {
		context = gtk_widget_get_pango_context (GTK_WIDGET (container));
		desc = pango_font_description_copy (pango_context_get_font_description (context));
		pango_font_description_set_size (desc,
						 pango_font_description_get_size (desc) +
						 container->details->font_size_table [container->details->zoom_level]);
	}
	eel_editable_label_set_font_description (EEL_EDITABLE_LABEL (details->rename_widget),
						 desc);
	pango_font_description_free (desc);
	
	icon_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
	text_rect = nautilus_icon_canvas_item_get_text_rectangle (icon->item);

	if (nautilus_icon_container_is_layout_vertical (container) &&
	    container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
		/* for one-line editables, the width changes dynamically */
		width = -1;
	} else {
		width = nautilus_icon_canvas_item_get_max_text_width (icon->item);
	}

	if (details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
		eel_canvas_w2c (EEL_CANVAS_ITEM (icon->item)->canvas,
				text_rect.x0,
				text_rect.y0,
				&x, &y);
	} else {
		eel_canvas_w2c (EEL_CANVAS_ITEM (icon->item)->canvas,
				(icon_rect.x0 + icon_rect.x1) / 2,
				icon_rect.y1,
				&x, &y);
		x = x - width / 2 - 1;
	}

	gtk_layout_move (GTK_LAYOUT (container),
			 details->rename_widget,
			 x, y);
	
	gtk_widget_set_size_request (details->rename_widget,
				     width, -1);
	eel_editable_label_set_text (EEL_EDITABLE_LABEL (details->rename_widget),
				     editable_text);
	if (select_all) {
		start_offset = 0;
		end_offset = -1;
	} else {
		eel_filename_get_rename_region (editable_text, &start_offset, &end_offset);
	}
	eel_editable_label_select_region (EEL_EDITABLE_LABEL (details->rename_widget),
					  start_offset,
					  end_offset);
	gtk_widget_show (details->rename_widget);
	
	gtk_widget_grab_focus (details->rename_widget);
	
	g_signal_emit (container,
		       signals[RENAMING_ICON], 0,
		       GTK_EDITABLE (details->rename_widget));
	
	nautilus_icon_container_update_icon (container, icon);
	
	/* We are in renaming mode */
	details->renaming = TRUE;
	nautilus_icon_canvas_item_set_renaming (icon->item, TRUE);
}

static void
end_renaming_mode (NautilusIconContainer *container, gboolean commit)
{
	NautilusIcon *icon;
	const char *changed_text;

	set_pending_icon_to_rename (container, NULL);

	icon = get_icon_being_renamed (container);
	if (icon == NULL) {
		return;
	}

	/* We are not in renaming mode */
	container->details->renaming = FALSE;
	nautilus_icon_canvas_item_set_renaming (icon->item, FALSE);
	
	nautilus_icon_container_unfreeze_updates (container);

	if (commit) {
		set_pending_icon_to_reveal (container, icon);
	}

	gtk_widget_grab_focus (GTK_WIDGET (container));
	
	if (commit) {
		/* Verify that text has been modified before signalling change. */			
		changed_text = eel_editable_label_get_text (EEL_EDITABLE_LABEL (container->details->rename_widget));
		if (strcmp (container->details->original_text, changed_text) != 0) {			
			g_signal_emit (container,
				       signals[ICON_TEXT_CHANGED], 0,
				       icon->data,
				       changed_text);
		}
	}

	gtk_widget_hide (container->details->rename_widget);

	g_free (container->details->original_text);

}

/* emit preview signal, called by the canvas item */
gboolean
nautilus_icon_container_emit_preview_signal (NautilusIconContainer *icon_container,
					     NautilusIcon *icon,
					     gboolean start_flag)
{
	gboolean result;
	
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (icon_container), FALSE);
	g_return_val_if_fail (icon != NULL, FALSE);
	g_return_val_if_fail (start_flag == FALSE || start_flag == TRUE, FALSE);
	
	result = FALSE;
	g_signal_emit (icon_container,
			 signals[PREVIEW], 0,
			 icon->data,
			 start_flag,
			 &result);
	
	return result;
}

gboolean
nautilus_icon_container_has_stored_icon_positions (NautilusIconContainer *container)
{
	GList *p;
	NautilusIcon *icon;
	gboolean have_stored_position;
	NautilusIconPosition position;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		have_stored_position = FALSE;
		g_signal_emit (container,
				 signals[GET_STORED_ICON_POSITION], 0,
				 icon->data,
				 &position,
				 &have_stored_position);
		if (have_stored_position) {
			return TRUE;
		}
	}
	return FALSE;
}

void
nautilus_icon_container_set_single_click_mode (NautilusIconContainer *container,
					       gboolean single_click_mode)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->single_click_mode = single_click_mode;
}


/* update the label color when the background changes */

GdkGC *
nautilus_icon_container_get_label_color_and_gc (NautilusIconContainer *container,
						GdkColor             **color,
						gboolean               is_name,
						gboolean               is_highlight,
						gboolean		  is_prelit)
{
	int idx;
	
	if (is_name) {
		if (is_highlight) {
			if (GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (container))) {
				idx = LABEL_COLOR_HIGHLIGHT;
			} else {
				idx = LABEL_COLOR_ACTIVE;
			}
		} else {
			if (is_prelit) {
				idx = LABEL_COLOR_PRELIGHT;
			} else {
				idx = LABEL_COLOR;
			}
		}
	} else {
		if (is_highlight) {
			if (GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (container))) {
				idx = LABEL_INFO_COLOR_HIGHLIGHT;
			} else {
				idx = LABEL_INFO_COLOR_ACTIVE;
			}
		} else {
			idx = LABEL_INFO_COLOR;
		}
	}

	if (color) {
		*color = &container->details->label_colors [idx];
	}

	return container->details->label_gcs [idx];
}

static void
setup_gc_with_fg (NautilusIconContainer *container, int idx, guint32 color)
{
	GdkGC *gc;
	GdkColor gcolor;

	gcolor = eel_gdk_rgb_to_color (color);
	container->details->label_colors [idx] = gcolor;

	gc = gdk_gc_new (GTK_LAYOUT (container)->bin_window);
	gdk_gc_set_rgb_fg_color (gc, &gcolor);

	if (container->details->label_gcs [idx]) {
		g_object_unref (container->details->label_gcs [idx]);
	}

	container->details->label_gcs [idx] = gc;
}

static void
setup_label_gcs (NautilusIconContainer *container)
{
	EelBackground *background;
	GtkWidget *widget;
	GdkColor *light_info_color, *dark_info_color;
	guint light_info_value, dark_info_value;
	gboolean frame_text;
	
	if (!GTK_WIDGET_REALIZED (container))
		return;

	widget = GTK_WIDGET (container);

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));

	background = eel_get_widget_background (GTK_WIDGET (container));

	/* read the info colors from the current theme; use a reasonable default if undefined */
	gtk_widget_style_get (GTK_WIDGET (container),
			      "light_info_color", &light_info_color,
			      "dark_info_color", &dark_info_color,
			      NULL);

	if (light_info_color) {
		light_info_value = eel_gdk_color_to_rgb (light_info_color);
		gdk_color_free (light_info_color);
	} else {
		light_info_value = DEFAULT_LIGHT_INFO_COLOR;
	}

	if (dark_info_color) {
		dark_info_value = eel_gdk_color_to_rgb (dark_info_color);
		gdk_color_free (dark_info_color);
	} else {
		dark_info_value = DEFAULT_DARK_INFO_COLOR;
	}

	setup_gc_with_fg (container, LABEL_COLOR_HIGHLIGHT, eel_gdk_color_to_rgb (&widget->style->text[GTK_STATE_SELECTED]));
	setup_gc_with_fg (container, LABEL_COLOR_ACTIVE, eel_gdk_color_to_rgb (&widget->style->text[GTK_STATE_ACTIVE]));
	setup_gc_with_fg (container, LABEL_COLOR_PRELIGHT, eel_gdk_color_to_rgb (&widget->style->text[GTK_STATE_PRELIGHT]));
	setup_gc_with_fg (container, 
			  LABEL_INFO_COLOR_HIGHLIGHT, 
			  eel_gdk_color_is_dark (&GTK_WIDGET (container)->style->base[GTK_STATE_SELECTED]) ? light_info_value : dark_info_value);
	setup_gc_with_fg (container, 
			  LABEL_INFO_COLOR_ACTIVE, 
			  eel_gdk_color_is_dark (&GTK_WIDGET (container)->style->base[GTK_STATE_ACTIVE]) ? light_info_value : dark_info_value);
		
	/* If NautilusIconContainer::frame_text is set, we can safely
	 * use the foreground color from the theme, because it will
	 * always be displayed against the gtk background */
	gtk_widget_style_get (widget,
			      "frame_text", &frame_text,
			      NULL);

	if (frame_text || !eel_background_is_set(background)) {
		setup_gc_with_fg (container, LABEL_COLOR, 
				  eel_gdk_color_to_rgb (&widget->style->text[GTK_STATE_NORMAL]));
		setup_gc_with_fg (container, 
				  LABEL_INFO_COLOR, 
				  eel_gdk_color_is_dark (&widget->style->base[GTK_STATE_NORMAL]) ? light_info_value : dark_info_value);
	} else {
		if (container->details->use_drop_shadows || eel_background_is_dark (background)) {
			setup_gc_with_fg (container, LABEL_COLOR, 0xEFEFEF);
			setup_gc_with_fg (container, 
					  LABEL_INFO_COLOR, 
					  light_info_value);
		} else { /* converse */
			setup_gc_with_fg (container, LABEL_COLOR, 0x000000);
			setup_gc_with_fg (container, 
					  LABEL_INFO_COLOR, 
					  dark_info_value);
		}
	}
}

static void
update_label_color (EelBackground         *background,
		    NautilusIconContainer *container)
{
	g_assert (EEL_IS_BACKGROUND (background));

	setup_label_gcs (container);
}


/* Return if the icon container is a fixed size */
gboolean
nautilus_icon_container_get_is_fixed_size (NautilusIconContainer *container)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);

	return container->details->is_fixed_size;
}

/* Set the icon container to be a fixed size */
void
nautilus_icon_container_set_is_fixed_size (NautilusIconContainer *container,
					   gboolean is_fixed_size)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->is_fixed_size = is_fixed_size;
}

gboolean
nautilus_icon_container_get_is_desktop (NautilusIconContainer *container)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);

	return container->details->is_desktop;
}

void
nautilus_icon_container_set_is_desktop (NautilusIconContainer *container,
					   gboolean is_desktop)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->is_desktop = is_desktop;
}

void
nautilus_icon_container_set_margins (NautilusIconContainer *container,
				     int left_margin,
				     int right_margin,
				     int top_margin,
				     int bottom_margin)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->left_margin = left_margin;
	container->details->right_margin = right_margin;
	container->details->top_margin = top_margin;
	container->details->bottom_margin = bottom_margin;

	/* redo layout of icons as the margins have changed */
	schedule_redo_layout (container);
}

void
nautilus_icon_container_set_use_drop_shadows (NautilusIconContainer  *container,
					      gboolean                use_drop_shadows)
{
	gboolean frame_text;
	
	gtk_widget_style_get (GTK_WIDGET (container),
			      "frame_text", &frame_text,
			      NULL);

	if (container->details->drop_shadows_requested == use_drop_shadows) {
		return;
	}

	container->details->drop_shadows_requested = use_drop_shadows;
	container->details->use_drop_shadows = use_drop_shadows && !frame_text;
	gtk_widget_queue_draw (GTK_WIDGET (container));
}

/* handle theme changes */

static void
nautilus_icon_container_theme_changed (gpointer user_data)
{
	NautilusIconContainer *container;
	GtkStyle *style;
	GdkColor *prelight_icon_color, *normal_icon_color;
	guchar highlight_alpha, normal_alpha, prelight_alpha;

	container = NAUTILUS_ICON_CONTAINER (user_data);
	
	/* load the highlight color */
	gtk_widget_style_get (GTK_WIDGET (container),
			      "highlight_alpha", &highlight_alpha,
			      NULL);
	
	style = GTK_WIDGET (container)->style;

	container->details->highlight_color_rgba = 
		EEL_RGBA_COLOR_PACK (style->base[GTK_STATE_SELECTED].red >> 8, 
				     style->base[GTK_STATE_SELECTED].green >> 8, 
				     style->base[GTK_STATE_SELECTED].blue >> 8,
				     highlight_alpha);
	container->details->active_color_rgba = 
		EEL_RGBA_COLOR_PACK (style->base[GTK_STATE_ACTIVE].red >> 8, 
				     style->base[GTK_STATE_ACTIVE].green >> 8, 
				     style->base[GTK_STATE_ACTIVE].blue >> 8,
				     highlight_alpha);

	/* load the prelight icon color */
	gtk_widget_style_get (GTK_WIDGET (container),
			      "prelight_icon_color", &prelight_icon_color,
			      NULL);

	if (prelight_icon_color) {
		container->details->prelight_icon_color_rgba = 
			EEL_RGBA_COLOR_PACK (prelight_icon_color->red >> 8, 
					     prelight_icon_color->green >> 8, 
					     prelight_icon_color->blue >> 8,
					     255);
	} else { /* if not defined by rc, set to default value */
		container->details->prelight_icon_color_rgba = 
			EEL_RGBA_COLOR_PACK (style->base[GTK_STATE_PRELIGHT].red >> 8,
					     style->base[GTK_STATE_PRELIGHT].green >> 8,
					     style->base[GTK_STATE_PRELIGHT].blue >> 8,
					     255);
	}
  
  
	/* load the normal icon color */
	gtk_widget_style_get (GTK_WIDGET (container),
			      "normal_icon_color", &normal_icon_color,
			      NULL);

	if (normal_icon_color) {
		container->details->normal_icon_color_rgba = 
			EEL_RGBA_COLOR_PACK (normal_icon_color->red >> 8, 
					     normal_icon_color->green >> 8, 
					     normal_icon_color->blue >> 8,
					     255);
	} else { /* if not defined by rc, set to default value */
		container->details->normal_icon_color_rgba = 
			EEL_RGBA_COLOR_PACK (style->base[GTK_STATE_NORMAL].red >> 8, 
					     style->base[GTK_STATE_NORMAL].green >> 8, 
					     style->base[GTK_STATE_NORMAL].blue >> 8,
					     255);
	}


	/* load the normal color */
	gtk_widget_style_get (GTK_WIDGET (container),
			      "normal_alpha", &normal_alpha,
			      NULL);

	container->details->normal_color_rgba = 
		EEL_RGBA_COLOR_PACK (style->base[GTK_STATE_NORMAL].red >> 8, 
				     style->base[GTK_STATE_NORMAL].green >> 8, 
				     style->base[GTK_STATE_NORMAL].blue >> 8,
				     normal_alpha);


	/* load the prelight color */
	gtk_widget_style_get (GTK_WIDGET (container),
			      "prelight_alpha", &prelight_alpha,
			      NULL);

	container->details->prelight_color_rgba = 
		EEL_RGBA_COLOR_PACK (style->base[GTK_STATE_PRELIGHT].red >> 8, 
				     style->base[GTK_STATE_PRELIGHT].green >> 8, 
				     style->base[GTK_STATE_PRELIGHT].blue >> 8,
				     prelight_alpha);


	setup_label_gcs (container);
}

void
nautilus_icon_container_set_font (NautilusIconContainer *container,
				  const char *font)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	if (eel_strcmp (container->details->font, font) == 0) {
		return;
	}

	g_free (container->details->font);
	container->details->font = g_strdup (font);

	invalidate_label_sizes (container);
	nautilus_icon_container_request_update_all (container);
	gtk_widget_queue_draw (GTK_WIDGET (container));
}

void
nautilus_icon_container_set_font_size_table (NautilusIconContainer *container,
					     const int font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1])
{
	int old_font_size;
	int i;
	
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (font_size_table != NULL);
	
	old_font_size = container->details->font_size_table[container->details->zoom_level];

	for (i = 0; i <= NAUTILUS_ZOOM_LEVEL_LARGEST; i++) {
		if (container->details->font_size_table[i] != font_size_table[i]) {
			container->details->font_size_table[i] = font_size_table[i];
		}
	}

	if (old_font_size != container->details->font_size_table[container->details->zoom_level]) {
		invalidate_label_sizes (container);
		nautilus_icon_container_request_update_all (container);
	}
}

/**
 * nautilus_icon_container_get_icon_description
 * @container: An icon container widget.
 * @data: Icon data 
 * 
 * Gets the description for the icon. This function may return NULL.
 **/
char*
nautilus_icon_container_get_icon_description (NautilusIconContainer *container,
				              NautilusIconData      *data)
{
	NautilusIconContainerClass *klass;

	klass = NAUTILUS_ICON_CONTAINER_GET_CLASS (container);

	if (klass->get_icon_description) {
		return klass->get_icon_description (container, data);
	} else {
		return NULL;
	}
}

gboolean
nautilus_icon_container_get_allow_moves (NautilusIconContainer *container)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);

	return container->details->drag_allow_moves;
}

void
nautilus_icon_container_set_allow_moves	(NautilusIconContainer *container,
					 gboolean               allow_moves)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->drag_allow_moves = allow_moves;
}

void
nautilus_icon_container_set_forced_icon_size (NautilusIconContainer *container,
					      int                    forced_icon_size)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	if (forced_icon_size != container->details->forced_icon_size) {
		container->details->forced_icon_size = forced_icon_size;

		invalidate_label_sizes (container);
		nautilus_icon_container_request_update_all (container);
	}
}

void
nautilus_icon_container_set_all_columns_same_width (NautilusIconContainer *container,
						    gboolean               all_columns_same_width)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	if (all_columns_same_width != container->details->all_columns_same_width) {
		container->details->all_columns_same_width = all_columns_same_width;

		invalidate_label_sizes (container);
		nautilus_icon_container_request_update_all (container);
	}
}

/* NautilusIconContainerAccessible */

static NautilusIconContainerAccessiblePrivate *
accessible_get_priv (AtkObject *accessible)
{
	NautilusIconContainerAccessiblePrivate *priv;
	
	priv = g_object_get_qdata (G_OBJECT (accessible), 
				   accessible_private_data_quark);

	return priv;
}

/* AtkAction interface */

static gboolean
nautilus_icon_container_accessible_do_action (AtkAction *accessible, int i)
{
	GtkWidget *widget;
	NautilusIconContainer *container;
	GList *selection;

	g_return_val_if_fail (i < LAST_ACTION, FALSE);

	widget = GTK_ACCESSIBLE (accessible)->widget;
	if (!widget) {
		return FALSE;
	}
	
	container = NAUTILUS_ICON_CONTAINER (widget);
	switch (i) {
	case ACTION_ACTIVATE :
		selection = nautilus_icon_container_get_selection (container);

		if (selection) {
			g_signal_emit_by_name (container, "activate", selection);
			g_list_free (selection);
		}
		break;
	case ACTION_MENU :
		handle_popups (container, NULL,"context_click_background");
		break;
	default :
		g_warning ("Invalid action passed to NautilusIconContainerAccessible::do_action");
		return FALSE;
	}
	return TRUE;
}

static int
nautilus_icon_container_accessible_get_n_actions (AtkAction *accessible)
{
	return LAST_ACTION;
}

static const char *
nautilus_icon_container_accessible_action_get_description (AtkAction *accessible, 
							   int i)
{
	NautilusIconContainerAccessiblePrivate *priv;
	
	g_assert (i < LAST_ACTION);

	priv = accessible_get_priv (ATK_OBJECT (accessible));
	
	if (priv->action_descriptions[i]) {
		return priv->action_descriptions[i];
	} else {
		return nautilus_icon_container_accessible_action_descriptions[i];
	}
}

static const char *
nautilus_icon_container_accessible_action_get_name (AtkAction *accessible, int i)
{
	g_assert (i < LAST_ACTION);

	return nautilus_icon_container_accessible_action_names[i];
}

static const char *
nautilus_icon_container_accessible_action_get_keybinding (AtkAction *accessible, 
							  int i)
{
	g_assert (i < LAST_ACTION);

	return NULL;
}

static gboolean
nautilus_icon_container_accessible_action_set_description (AtkAction *accessible, 
							   int i, 
							   const char *description)
{
	NautilusIconContainerAccessiblePrivate *priv;

	g_assert (i < LAST_ACTION);

	priv = accessible_get_priv (ATK_OBJECT (accessible));

	if (priv->action_descriptions[i]) {
		g_free (priv->action_descriptions[i]);
	}
	priv->action_descriptions[i] = g_strdup (description);

	return FALSE;
}

static void
nautilus_icon_container_accessible_action_interface_init (AtkActionIface *iface)
{
	iface->do_action = nautilus_icon_container_accessible_do_action;
	iface->get_n_actions = nautilus_icon_container_accessible_get_n_actions;
	iface->get_description = nautilus_icon_container_accessible_action_get_description;
	iface->get_name = nautilus_icon_container_accessible_action_get_name;
	iface->get_keybinding = nautilus_icon_container_accessible_action_get_keybinding;
	iface->set_description = nautilus_icon_container_accessible_action_set_description;
}

/* AtkSelection interface */

static void
nautilus_icon_container_accessible_update_selection (AtkObject *accessible)
{
	NautilusIconContainer *container;
	NautilusIconContainerAccessiblePrivate *priv;
	GList *l;
	NautilusIcon *icon;

	container = NAUTILUS_ICON_CONTAINER (GTK_ACCESSIBLE (accessible)->widget);

	priv = accessible_get_priv (accessible);

	if (priv->selection) {
		g_list_free (priv->selection);
		priv->selection = NULL;
	}
	
	for (l = container->details->icons; l != NULL; l = l->next) {
		icon = l->data;
		if (icon->is_selected) {
			priv->selection = g_list_prepend (priv->selection, 
							  icon);
		}
	}

	priv->selection = g_list_reverse (priv->selection);
}

static void
nautilus_icon_container_accessible_selection_changed_cb (NautilusIconContainer *container,
							 gpointer data)
{
	g_signal_emit_by_name (data, "selection_changed");
}

static void
nautilus_icon_container_accessible_icon_added_cb (NautilusIconContainer *container,
						  NautilusIconData *icon_data,
						  gpointer data)
{
	NautilusIcon *icon;
	AtkObject *atk_parent;
	AtkObject *atk_child;
	int index;

	icon = g_hash_table_lookup (container->details->icon_set, icon_data);
	if (icon) {
		atk_parent = ATK_OBJECT (data);
		atk_child = atk_gobject_accessible_for_object 
			(G_OBJECT (icon->item));
		index = g_list_index (container->details->icons, icon);
		
		g_signal_emit_by_name (atk_parent, "children_changed::add",
				       index, atk_child, NULL);
	}
}

static void
nautilus_icon_container_accessible_icon_removed_cb (NautilusIconContainer *container,
						    NautilusIconData *icon_data,
						    gpointer data)
{
	NautilusIcon *icon;
	AtkObject *atk_parent;
	AtkObject *atk_child;
	int index;
	
	icon = g_hash_table_lookup (container->details->icon_set, icon_data);
	if (icon) {
		atk_parent = ATK_OBJECT (data);
		atk_child = atk_gobject_accessible_for_object 
			(G_OBJECT (icon->item));
		index = g_list_index (container->details->icons, icon);
		
		g_signal_emit_by_name (atk_parent, "children_changed::remove",
				       index, atk_child, NULL);
	}
}

static void
nautilus_icon_container_accessible_cleared_cb (NautilusIconContainer *container, 
					       gpointer data)
{
	g_signal_emit_by_name (data, "children_changed", 0, NULL, NULL);
}


static gboolean 
nautilus_icon_container_accessible_add_selection (AtkSelection *accessible, 
						  int i)
{
	GtkWidget *widget;
	NautilusIconContainer *container;
	GList *l;
	GList *selection;
	NautilusIcon *icon;

	widget = GTK_ACCESSIBLE (accessible)->widget;
	if (!widget) {
		return FALSE;
	}

        container = NAUTILUS_ICON_CONTAINER (widget);
	
	l = g_list_nth (container->details->icons, i);
	if (l) {
		icon = l->data;
		
		selection = nautilus_icon_container_get_selection (container);
		selection = g_list_prepend (selection, 
					    icon->data);
		nautilus_icon_container_set_selection (container, selection);
		
		g_list_free (selection);
		return TRUE;
	}

	return FALSE;
}

static gboolean
nautilus_icon_container_accessible_clear_selection (AtkSelection *accessible)
{
	GtkWidget *widget;
	NautilusIconContainer *container;

	widget = GTK_ACCESSIBLE (accessible)->widget;
	if (!widget) {
		return FALSE;
	}

        container = NAUTILUS_ICON_CONTAINER (widget);

	nautilus_icon_container_unselect_all (container);

	return TRUE;
}

static AtkObject *
nautilus_icon_container_accessible_ref_selection (AtkSelection *accessible, 
						  int i)
{
	AtkObject *atk_object;
	NautilusIconContainerAccessiblePrivate *priv;
	GList *item;
	NautilusIcon *icon;

	nautilus_icon_container_accessible_update_selection (ATK_OBJECT (accessible));
	priv = accessible_get_priv (ATK_OBJECT (accessible));

	item = (g_list_nth (priv->selection, i));

	if (item) {
		icon = item->data;
		atk_object = atk_gobject_accessible_for_object (G_OBJECT (icon->item));
		if (atk_object) {
			g_object_ref (atk_object);
		}

		return atk_object;
	} else {
		return NULL;
	}
}

static int
nautilus_icon_container_accessible_get_selection_count (AtkSelection *accessible)
{
	int count;
	NautilusIconContainerAccessiblePrivate *priv;

	nautilus_icon_container_accessible_update_selection (ATK_OBJECT (accessible));
	priv = accessible_get_priv (ATK_OBJECT (accessible));

	count = g_list_length (priv->selection);
	
	return count;
}

static gboolean
nautilus_icon_container_accessible_is_child_selected (AtkSelection *accessible,
						      int i)
{
	NautilusIconContainer *container;
	GList *l;
	NautilusIcon *icon;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (accessible)->widget;
	if (!widget) {
		return FALSE;
	}

        container = NAUTILUS_ICON_CONTAINER (widget);

	l = g_list_nth (container->details->icons, i);
	if (l) {
		icon = l->data;
		return icon->is_selected;
	}
	return FALSE;
}

static gboolean
nautilus_icon_container_accessible_remove_selection (AtkSelection *accessible,
						     int i)
{
	NautilusIconContainer *container;
	NautilusIconContainerAccessiblePrivate *priv;
	GList *l;
	GList *selection;
	NautilusIcon *icon;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (accessible)->widget;
	if (!widget) {
		return FALSE;
	}

	nautilus_icon_container_accessible_update_selection (ATK_OBJECT (accessible));
	priv = accessible_get_priv (ATK_OBJECT (accessible));

        container = NAUTILUS_ICON_CONTAINER (widget);
	
	l = g_list_nth (priv->selection, i);
	if (l) {
		icon = l->data;
		
		selection = nautilus_icon_container_get_selection (container);
		selection = g_list_remove (selection, icon->data);
		nautilus_icon_container_set_selection (container, selection);
		
		g_list_free (selection);
		return TRUE;
	}

	return FALSE;	
}

static gboolean
nautilus_icon_container_accessible_select_all_selection (AtkSelection *accessible)
{
	NautilusIconContainer *container;
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (accessible)->widget;
	if (!widget) {
		return FALSE;
	}

        container = NAUTILUS_ICON_CONTAINER (widget);

	nautilus_icon_container_select_all (container);

	return TRUE;
}

void
nautilus_icon_container_widget_to_file_operation_position (NautilusIconContainer *container,
							   GdkPoint              *position)
{
	double x, y;

	g_return_if_fail (position != NULL);

	x = position->x;
	y = position->y;

	eel_canvas_window_to_world (EEL_CANVAS (container), x, y, &x, &y);

	position->x = (int) x;
	position->y = (int) y;

	/* ensure that we end up in the middle of the icon */
	position->x -= nautilus_get_icon_size_for_zoom_level (container->details->zoom_level) / 2;
	position->y -= nautilus_get_icon_size_for_zoom_level (container->details->zoom_level) / 2;
}

static void 
nautilus_icon_container_accessible_selection_interface_init (AtkSelectionIface *iface)
{
	iface->add_selection = nautilus_icon_container_accessible_add_selection;
	iface->clear_selection = nautilus_icon_container_accessible_clear_selection;
	iface->ref_selection = nautilus_icon_container_accessible_ref_selection;
	iface->get_selection_count = nautilus_icon_container_accessible_get_selection_count;
	iface->is_child_selected = nautilus_icon_container_accessible_is_child_selected;
	iface->remove_selection = nautilus_icon_container_accessible_remove_selection;
	iface->select_all_selection = nautilus_icon_container_accessible_select_all_selection;
}


static gint 
nautilus_icon_container_accessible_get_n_children (AtkObject *accessible)
{
	NautilusIconContainer *container;
	GtkWidget *widget;
	gint i;
	
	widget = GTK_ACCESSIBLE (accessible)->widget;
	if (!widget) {
		return FALSE;
	}

	container = NAUTILUS_ICON_CONTAINER (widget);

	i = g_hash_table_size (container->details->icon_set);
	if (container->details->rename_widget) {
		i++;
	}
	return i;
}

static AtkObject* 
nautilus_icon_container_accessible_ref_child (AtkObject *accessible, int i)
{
        AtkObject *atk_object;
        NautilusIconContainer *container;
        GList *item;
        NautilusIcon *icon;
	GtkWidget *widget;
        
	widget = GTK_ACCESSIBLE (accessible)->widget;
	if (!widget) {
		return NULL;
	}

        container = NAUTILUS_ICON_CONTAINER (widget);
        
        item = (g_list_nth (container->details->icons, i));
        
        if (item) {
                icon = item->data;
                
                atk_object = atk_gobject_accessible_for_object (G_OBJECT (icon->item));
                g_object_ref (atk_object);
                
                return atk_object;
        } else {
		if (i == g_list_length (container->details->icons)) {
			if (container->details->rename_widget) {
				atk_object = gtk_widget_get_accessible (container->details->rename_widget);
				g_object_ref (atk_object);

                		return atk_object;
			}
		}
                return NULL;
        }
}

static void
nautilus_icon_container_accessible_initialize (AtkObject *accessible, 
					       gpointer data)
{
	NautilusIconContainer *container;
	NautilusIconContainerAccessiblePrivate *priv;

	if (ATK_OBJECT_CLASS (accessible_parent_class)->initialize) {
		ATK_OBJECT_CLASS (accessible_parent_class)->initialize (accessible, data);
	}

	priv = g_new0 (NautilusIconContainerAccessiblePrivate, 1);
	g_object_set_qdata (G_OBJECT (accessible), 
			    accessible_private_data_quark, 
			    priv);

	if (GTK_IS_ACCESSIBLE (accessible)) {
		nautilus_icon_container_accessible_update_selection 
			(ATK_OBJECT (accessible));
		
		container = NAUTILUS_ICON_CONTAINER (GTK_ACCESSIBLE (accessible)->widget);
		g_signal_connect (G_OBJECT (container), "selection_changed",
				  G_CALLBACK (nautilus_icon_container_accessible_selection_changed_cb), 
				  accessible);
		g_signal_connect (G_OBJECT (container), "icon_added",
				  G_CALLBACK (nautilus_icon_container_accessible_icon_added_cb), 
				  accessible);
		g_signal_connect (G_OBJECT (container), "icon_removed",
				  G_CALLBACK (nautilus_icon_container_accessible_icon_removed_cb), 
				  accessible);
		g_signal_connect (G_OBJECT (container), "cleared",
				  G_CALLBACK (nautilus_icon_container_accessible_cleared_cb), 
				  accessible);
	}
}

static void
nautilus_icon_container_accessible_finalize (GObject *object)
{
	NautilusIconContainerAccessiblePrivate *priv;
	int i;

	priv = accessible_get_priv (ATK_OBJECT (object));
	if (priv->selection) {
		g_list_free (priv->selection);
	}

	for (i = 0; i < LAST_ACTION; i++) {
		if (priv->action_descriptions[i]) {
			g_free (priv->action_descriptions[i]);
		}
	}
	
	g_free (priv);

	G_OBJECT_CLASS (accessible_parent_class)->finalize (object);
}

static void
nautilus_icon_container_accessible_class_init (AtkObjectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	accessible_parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = nautilus_icon_container_accessible_finalize;

	klass->get_n_children = nautilus_icon_container_accessible_get_n_children;
	klass->ref_child = nautilus_icon_container_accessible_ref_child;
	klass->initialize = nautilus_icon_container_accessible_initialize;

	accessible_private_data_quark = g_quark_from_static_string ("icon-container-accessible-private-data");
}

static GType
nautilus_icon_container_accessible_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static GInterfaceInfo atk_action_info = {
                        (GInterfaceInitFunc) nautilus_icon_container_accessible_action_interface_init,
                        (GInterfaceFinalizeFunc) NULL,
                        NULL
                };              
		
                static GInterfaceInfo atk_selection_info = {
                        (GInterfaceInitFunc) nautilus_icon_container_accessible_selection_interface_init,
                        (GInterfaceFinalizeFunc) NULL,
                        NULL
                };              

		type = eel_accessibility_create_derived_type 
			("NautilusIconContainerAccessible",
			 EEL_TYPE_CANVAS,
			 nautilus_icon_container_accessible_class_init);
		
                g_type_add_interface_static (type, ATK_TYPE_ACTION,
                                             &atk_action_info);
                g_type_add_interface_static (type, ATK_TYPE_SELECTION,
                                             &atk_selection_info);
        }

        return type;
}

#if ! defined (NAUTILUS_OMIT_SELF_CHECK)

static char *
check_compute_stretch (int icon_x, int icon_y, int icon_size,
		       int start_pointer_x, int start_pointer_y,
		       int end_pointer_x, int end_pointer_y)
{
	StretchState start, current;

	start.icon_x = icon_x;
	start.icon_y = icon_y;
	start.icon_size = icon_size;
	start.pointer_x = start_pointer_x;
	start.pointer_y = start_pointer_y;
	current.pointer_x = end_pointer_x;
	current.pointer_y = end_pointer_y;

	compute_stretch (&start, &current);

	return g_strdup_printf ("%d,%d:%d",
				current.icon_x,
				current.icon_y,
				current.icon_size);
}

void
nautilus_self_check_icon_container (void)
{
	EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 16, 0, 0, 0, 0), "0,0:16");
	EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 16, 16, 16, 17, 17), "0,0:17");
	EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 16, 16, 16, 17, 16), "0,0:16");
	EEL_CHECK_STRING_RESULT (check_compute_stretch (100, 100, 64, 105, 105, 40, 40), "35,35:129");
}

gboolean
nautilus_icon_container_is_layout_rtl (NautilusIconContainer *container)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), 0);

	return container->details->layout_mode == NAUTILUS_ICON_LAYOUT_T_B_R_L ||
		container->details->layout_mode == NAUTILUS_ICON_LAYOUT_R_L_T_B;
}

gboolean
nautilus_icon_container_is_layout_vertical (NautilusIconContainer *container)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);

	return (container->details->layout_mode == NAUTILUS_ICON_LAYOUT_T_B_L_R ||
		container->details->layout_mode == NAUTILUS_ICON_LAYOUT_T_B_R_L);
}


#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
