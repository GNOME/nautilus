/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-container.c - Icon container widget.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel, Inc.
   
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
#include "nautilus-icon-container.h"

#include "nautilus-font-factory.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-private.h"
#include "nautilus-icon-text-item.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-theme.h"
#include <ctype.h>
#include <eel/eel-background.h>
#include <eel/eel-canvas-rect.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtklayout.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libnautilus/nautilus-clipboard.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Interval for updating the rubberband selection, in milliseconds.  */
#define RUBBERBAND_TIMEOUT_INTERVAL 10

/* Internal double click time constant */
#define DOUBLE_CLICK_TIME 500000

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

/* Distance you have to move before it becomes a drag. */
#define SNAP_RESISTANCE 2 /* GMC has this set to 3, but it's too much for Ettore's taste. */

/* Button assignments. */
#define DRAG_BUTTON 1
#define RUBBERBAND_BUTTON 1
#define MIDDLE_BUTTON 2
#define CONTEXTUAL_MENU_BUTTON 3

/* Maximum size (pixels) allowed for icons at the standard zoom level. */
#define MINIMUM_IMAGE_SIZE 24
#define MAXIMUM_IMAGE_SIZE 96
#define MAXIMUM_EMBLEM_SIZE 48

#define ICON_PAD_LEFT 4
#define ICON_PAD_RIGHT 4
#define ICON_BASE_WIDTH 96

#define ICON_PAD_TOP 4
#define ICON_PAD_BOTTOM 4

#define CONTAINER_PAD_LEFT 4
#define CONTAINER_PAD_RIGHT 4
#define CONTAINER_PAD_TOP 4
#define CONTAINER_PAD_BOTTOM 4

#define STANDARD_ICON_GRID_WIDTH 145

/* Desktop layout mode defines */
#define DESKTOP_PAD_HORIZONTAL 	30
#define DESKTOP_PAD_VERTICAL 	10
#define CELL_SIZE 		20

/* Value used to protect against icons being dragged outside of the desktop bounds */
#define DESKTOP_ICON_SAFETY_PAD 10

enum {
	NAUTILUS_TYPESELECT_FLUSH_DELAY = 1000000
	/* After this time the current typeselect buffer will be
	 * thrown away and the new pressed character will be made
	 * the the start of a new pattern.
	 */
};

static void          activate_selected_items                  (NautilusIconContainer      *container);
static void          nautilus_icon_container_initialize_class (NautilusIconContainerClass *class);
static void          nautilus_icon_container_initialize       (NautilusIconContainer      *container);
static void	     nautilus_icon_container_theme_changed    (gpointer		 	  user_data);

static void          compute_stretch                          (StretchState               *start,
							       StretchState               *current);
static NautilusIcon *get_first_selected_icon                  (NautilusIconContainer      *container);
static NautilusIcon *get_nth_selected_icon                    (NautilusIconContainer      *container,
							       int                         index);
static gboolean      has_multiple_selection                   (NautilusIconContainer      *container);
static void          icon_destroy                             (NautilusIconContainer      *container,
							       NautilusIcon               *icon);
static void          end_renaming_mode                        (NautilusIconContainer      *container,
							       gboolean                    commit);
static NautilusIcon *get_icon_being_renamed 		      (NautilusIconContainer 	  *container);
static void          finish_adding_new_icons                  (NautilusIconContainer      *container);
static void          update_label_color                       (EelBackground         *background,
							       NautilusIconContainer      *icon_container);
static void	     icon_get_bounding_box 		      (NautilusIcon 		  *icon,
							       int 			  *x1_return, 
							       int 			  *y1_return,
		       					       int 			  *x2_return, 
		       					       int 			  *y2_return);
static gboolean	     is_renaming	      		      (NautilusIconContainer	  *container);
static gboolean	     is_renaming_pending		      (NautilusIconContainer	  *container);
static void	     process_pending_icon_to_rename	      (NautilusIconContainer	  *container);

static int click_policy_auto_value;

EEL_DEFINE_CLASS_BOILERPLATE (NautilusIconContainer,
				   nautilus_icon_container,
				   GNOME_TYPE_CANVAS)

/* The NautilusIconContainer signals.  */
enum {
	ACTIVATE,
	BAND_SELECT_STARTED,
	BAND_SELECT_ENDED,
	BUTTON_PRESS,
	CAN_ACCEPT_ITEM,
	COMPARE_ICONS,
	COMPARE_ICONS_BY_NAME,
	CONTEXT_CLICK_BACKGROUND,
	CONTEXT_CLICK_SELECTION,
	MIDDLE_CLICK,
	GET_CONTAINER_URI,
	GET_ICON_IMAGES,
	GET_ICON_TEXT,
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
	HANDLE_URI_LIST,
	PREVIEW,
	SELECTION_CHANGED,	
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

/* Bitmap for stippled selection rectangles.  */
static GdkBitmap *stipple;
static char stipple_bits[] = { 0x02, 0x01 };

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
	
	int left, top, right, bottom;
	int width, height;
	int x1, y1, x2, y2;
	
	if (icon->x == x && icon->y == y) {
		return;
	}

	container = NAUTILUS_ICON_CONTAINER (GNOME_CANVAS_ITEM (icon->item)->canvas);

	if (icon == get_icon_being_renamed (container)) {
		end_renaming_mode (container, TRUE);
	}

	if (nautilus_icon_container_get_is_fixed_size (container)) {
		/* Clip the position of the icon within our desktop bounds */
		left = GTK_WIDGET (container)->allocation.x;
		top = GTK_WIDGET (container)->allocation.y;
		right = left + GTK_WIDGET (container)->allocation.width;
		bottom = top + GTK_WIDGET (container)->allocation.height;

		icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);
		width = x2 - x1;
		height = y2 - y1;
				
		if (x > right - DESKTOP_ICON_SAFETY_PAD) {
			x = right - DESKTOP_ICON_SAFETY_PAD;
		}
		
		if (x < left) {
			x = left;
		}
		if (y > bottom - DESKTOP_ICON_SAFETY_PAD) {
			y = bottom - DESKTOP_ICON_SAFETY_PAD;
		}
		if (y < top) {
			y = top;
		}		
	}

	gnome_canvas_item_move (GNOME_CANVAS_ITEM (icon->item),
				x - icon->x,
				y - icon->y);

	icon->x = x;
	icon->y = y;
}

static void
icon_get_size (NautilusIconContainer *container,
	       NautilusIcon *icon,
	       guint *size_x, guint *size_y)
{
	if (size_x != NULL) {
		*size_x = MAX (nautilus_get_icon_size_for_zoom_level (container->details->zoom_level)
			       * icon->scale_x, NAUTILUS_ICON_SIZE_SMALLEST);
	}
	if (size_y != NULL) {
		*size_y = MAX (nautilus_get_icon_size_for_zoom_level (container->details->zoom_level)
			       * icon->scale_y, NAUTILUS_ICON_SIZE_SMALLEST);
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
	       gboolean update_position)
{
	guint old_size_x, old_size_y;
	double scale;

	icon_get_size (container, icon, &old_size_x, &old_size_y);
	if (icon_size == old_size_x && icon_size == old_size_y) {
		return;
	}

	scale = (double) icon_size /
		nautilus_get_icon_size_for_zoom_level
		(container->details->zoom_level);
	nautilus_icon_container_move_icon (container, icon,
					   icon->x, icon->y,
					   scale, scale,
					   FALSE, update_position);
}

static void
icon_raise (NautilusIcon *icon)
{
	gnome_canvas_item_raise_to_top (GNOME_CANVAS_ITEM (icon->item));
}

static void
emit_stretch_started (NautilusIconContainer *container, NautilusIcon *icon)
{
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[ICON_STRETCH_STARTED],
			 icon->data);
}

static void
emit_stretch_ended (NautilusIconContainer *container, NautilusIcon *icon)
{
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[ICON_STRETCH_ENDED],
			 icon->data);
}

static void
icon_toggle_selected (NautilusIconContainer *container,
		      NautilusIcon *icon)
{		
	end_renaming_mode (container, TRUE);

	icon->is_selected = !icon->is_selected;
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->item),
			       "highlighted_for_selection", (gboolean) icon->is_selected,
			       NULL);

	/* If the icon is deselected, then get rid of the stretch handles.
	 * No harm in doing the same if the item is newly selected.
	 */
	if (icon == container->details->stretch_icon) {
		container->details->stretch_icon = NULL;
		nautilus_icon_canvas_item_set_show_stretch_handles (icon->item, FALSE);
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

	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (icon->item),
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
	NautilusIcon *cur_pending;
	
	cur_pending = container->details->pending_icon_to_reveal;
	
	if (icon == cur_pending) {
		return;
	}
	
	if (cur_pending != NULL) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (cur_pending->item),
					       pending_icon_to_reveal_destroy_callback,
					       container);
	}
	
	if (icon != NULL) {
		gtk_signal_connect (GTK_OBJECT (icon->item), "destroy",
				    pending_icon_to_reveal_destroy_callback, container);
	}
	
	container->details->pending_icon_to_reveal = icon;
}

static void
reveal_icon (NautilusIconContainer *container,
	     NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;
	ArtIRect bounds;

	if (!icon_is_positioned (icon)) {
		set_pending_icon_to_reveal (container, icon);
		return;
	}
	
	set_pending_icon_to_reveal (container, NULL);

	details = container->details;
	allocation = &GTK_WIDGET (container)->allocation;

	hadj = gtk_layout_get_hadjustment (GTK_LAYOUT (container));
	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (container));

	bounds = eel_gnome_canvas_item_get_canvas_bounds
		(GNOME_CANVAS_ITEM (icon->item));

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
		gtk_timeout_remove (details->keyboard_icon_reveal_timer_id);
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
		= gtk_timeout_add (KEYBOARD_ICON_REVEAL_TIMEOUT,
				   keyboard_icon_reveal_timeout_callback,
				   container);
}

static void
clear_keyboard_focus (NautilusIconContainer *container)
{
        if (container->details->keyboard_focus != NULL) {
		gnome_canvas_item_set (GNOME_CANVAS_ITEM (container->details->keyboard_focus->item),
				       "highlighted_as_keyboard_focus", 0,
				       NULL);
	}
	
	container->details->keyboard_focus = NULL;
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

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (container->details->keyboard_focus->item),
			       "highlighted_as_keyboard_focus", 1,
			       NULL);
}

static void
get_all_icon_bounds (NautilusIconContainer *container,
		     double *x1, double *y1,
		     double *x2, double *y2)
{
	/* FIXME bugzilla.gnome.org 42477: Do we have to do something about the rubberband
	 * here? Any other non-icon items?
	 */
	gnome_canvas_item_get_bounds
		(GNOME_CANVAS (container)->root,
		 x1, y1, x2, y2);
}

/* Don't preserve visible white space the next time the scroll region
 * is recomputed when the container is not empty. */
void
nautilus_icon_container_reset_scroll_region (NautilusIconContainer *container)
{
	container->details->reset_scroll_region_trigger = TRUE;
}

void
nautilus_icon_container_update_scroll_region (NautilusIconContainer *container)
{
	double x1, y1, x2, y2;
	GtkAdjustment *hadj, *vadj;
	float step_increment;
	GtkAllocation *allocation;
	gboolean reset_scroll_region;

	if (nautilus_icon_container_get_is_fixed_size (container)) {
		/* Set the scroll region to the size of the container allocation */
		allocation = &GTK_WIDGET (container)->allocation;			
		eel_gnome_canvas_set_scroll_region_left_justify
			(GNOME_CANVAS (container),
			 (double) - container->details->left_margin,
			 (double) - container->details->top_margin,
			 (double) allocation->width - 1
			 - container->details->left_margin
			 - container->details->right_margin,
			 (double) allocation->height - 1
			 - container->details->top_margin
			 - container->details->bottom_margin);
		return;
	}

	get_all_icon_bounds (container, &x1, &y1, &x2, &y2);

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

	if (reset_scroll_region) {
		eel_gnome_canvas_set_scroll_region_left_justify
			(GNOME_CANVAS (container),
			 x1 - CONTAINER_PAD_LEFT,
			 y1 - CONTAINER_PAD_TOP,
			 x2 + CONTAINER_PAD_RIGHT,
			 y2 + CONTAINER_PAD_BOTTOM);
	} else {
		eel_gnome_canvas_set_scroll_region_include_visible_area
			(GNOME_CANVAS (container),
			 x1 - CONTAINER_PAD_LEFT,
			 y1 - CONTAINER_PAD_TOP,
			 x2 + CONTAINER_PAD_RIGHT,
			 y2 + CONTAINER_PAD_BOTTOM);
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
}

static NautilusIconContainer *sort_hack_container;

static int
compare_icons (gconstpointer a, gconstpointer b)
{
	const NautilusIcon *icon_a, *icon_b;
	int result;

	icon_a = a;
	icon_b = b;

	result = 0;
	gtk_signal_emit (GTK_OBJECT (sort_hack_container),
			 signals[COMPARE_ICONS],
			 icon_a->data,
			 icon_b->data,
			 &result);
	return result;
}

static void
sort_icons (NautilusIconContainer *container,
	    GList **icons)
{
	sort_hack_container = container;
	*icons = g_list_sort (*icons, compare_icons);
}

static int
compare_icons_by_name (gconstpointer a, gconstpointer b)
{
	const NautilusIcon *icon_a, *icon_b;
	int result;

	icon_a = a;
	icon_b = b;

	result = 0;
	gtk_signal_emit (GTK_OBJECT (sort_hack_container),
			 signals[COMPARE_ICONS_BY_NAME],
			 icon_a->data,
			 icon_b->data,
			 &result);
	return result;
}

static void
sort_icons_by_name (NautilusIconContainer *container,
		    GList **icons)
{
	sort_hack_container = container;
	*icons = g_list_sort (*icons, compare_icons_by_name);
}


static void
resort (NautilusIconContainer *container)
{
	sort_icons (container, &container->details->icons);
}

/* Given an icon's bounds, compute the width of the space it should be
 * placed in.
 */
static int
get_icon_space_width (NautilusIconContainer *container, const ArtDRect *bounds)
{
	double world_width;

	world_width = ICON_PAD_LEFT + (bounds->x1 - bounds->x0) + ICON_PAD_RIGHT;
	if (container->details->tighter_layout) {
		return world_width + 8; /* 8 pixels extra for fancy selection box */
	}
	
	if (world_width > STANDARD_ICON_GRID_WIDTH)
		return world_width;
	
	return STANDARD_ICON_GRID_WIDTH;
}

static void
lay_down_one_line (NautilusIconContainer *container,
		   GList *line_start,
		   GList *line_end,
		   double *y)
{
	GList *p;
	double max_height_above, max_height_below;
	NautilusIcon *icon;
	ArtDRect bounds, icon_bounds;
	double height_above, height_below, x, width;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	g_assert (line_end == NULL || g_list_first (line_start) == g_list_first (line_end));
	g_assert (y != NULL);

	/* Compute the total height above and below the baseline. */
	max_height_above = 0;
	max_height_below = 0;
	for (p = line_start; p != line_end; p = p->next) {
		icon = p->data;

		bounds = eel_gnome_canvas_item_get_world_bounds
			(GNOME_CANVAS_ITEM (icon->item));
		icon_bounds = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
		height_above = icon_bounds.y1 - bounds.y0;
		height_below = bounds.y1 - icon_bounds.y1;

		if (height_above > max_height_above) {
			max_height_above = height_above;
		}
		if (height_below > max_height_below) {
			max_height_below = height_below;
		}
	}

	/* Advance to the baseline. */
	*y += ICON_PAD_TOP + max_height_above;

	/* Lay out the icons along the baseline. */
	x = 0;
	for (p = line_start; p != line_end; p = p->next) {
		icon = p->data;

		bounds = eel_gnome_canvas_item_get_world_bounds
			(GNOME_CANVAS_ITEM (icon->item));
		icon_bounds = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
		width = get_icon_space_width (container, &bounds);

		icon_set_position
			(icon,
			 x + (width - (icon_bounds.x1 - icon_bounds.x0)) / 2,
			 *y - (icon_bounds.y1 - icon_bounds.y0));

		x += width;
	}

	/* Advance to next line. */
	*y += max_height_below + ICON_PAD_BOTTOM;
}


static void
lay_down_icons_horizontal (NautilusIconContainer *container,
			   GList *icons,
			   double start_y)
{
	GList *p, *line_start;
	NautilusIcon *icon;
	ArtDRect bounds;
	double canvas_width, line_width, space_width, y;

	/* Lay out icons a line at a time. */
	canvas_width = (GTK_WIDGET (container)->allocation.width
			- container->details->left_margin
			- container->details->right_margin)
		/ GNOME_CANVAS (container)->pixels_per_unit;
	line_width = 0;
	line_start = icons;
	y = start_y;
	for (p = icons; p != NULL; p = p->next) {
		icon = p->data;

		/* Get the width of the icon. */
		bounds = eel_gnome_canvas_item_get_world_bounds
			(GNOME_CANVAS_ITEM (icon->item));
		space_width = get_icon_space_width (container, &bounds);
		
		/* If this icon doesn't fit, it's time to lay out the line that's queued up. */
		
		/* FIXME: why don't we want to guarantee a small white space to the right of
		 * the last column just like we guarantee a small white space to the left of
		 * the first column?
		 */
		if (line_start != p && line_width + space_width - ICON_PAD_RIGHT > canvas_width) {
			lay_down_one_line (container, line_start, p, &y);
			line_width = 0;
			line_start = p;
		}

		/* Add this icon. */
		line_width += space_width;
	}

	/* Lay down that last line of icons. */
	if (line_start != NULL) {
		lay_down_one_line (container, line_start, NULL, &y);
	}
}

/* Search for available space at location */
static gboolean
find_open_grid_space (NautilusIcon *icon, int **icon_grid, int num_rows, 
		      int num_columns, int row, int column)
{		      
	int row_index, column_index;
	int x1, x2, y1, y2;
	double width, height;
	int qwidth, qheight;
	
	/* Get icon dimensions */
	icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);
	width = x2 - x1;
	height = y2 - y1;

	/* Convert to grid coordinates */
	qwidth = ceil (width / CELL_SIZE);
	qheight = ceil (height / CELL_SIZE);

	if (row + qwidth > num_rows) {
		qwidth = num_rows;
	} else {
		qwidth += row;	
	}
	if (column + qheight > num_columns) {
		qheight = num_columns;
	} else {
		qheight += column;
	}
	
	for (row_index = row; row_index < qwidth; row_index++) {
		for (column_index = column; column_index < qheight; column_index++) {
			if (icon_grid [row_index] [column_index] == 1) {
				return FALSE;
			}
		}
	}
	return TRUE;
}


static void
get_best_empty_grid_location (NautilusIcon *icon, int **icon_grid, int num_rows, 
			      int num_columns, int *x, int *y)
{
	gboolean found_space;
	int row, column;
	
	g_assert (icon_grid != NULL);
	g_assert (x != NULL);
	g_assert (y != NULL);

	found_space = FALSE;
	
	/* Set up default fallback position */
	*x = num_columns * CELL_SIZE;
	*y = num_rows * CELL_SIZE;

	/* Find best empty location */
	for (row = 0; row < num_rows; row++) {
		for (column = 0; column < num_columns; column++) {
			found_space = find_open_grid_space (icon, icon_grid, num_rows, 
							    num_columns, row, column);
			if (found_space) {				
				*x = row * CELL_SIZE;
				*y = column * CELL_SIZE;

				/* Correct for padding */
				if (*x < DESKTOP_PAD_HORIZONTAL) {
					*x = DESKTOP_PAD_HORIZONTAL;
				}
				if (*y < DESKTOP_PAD_VERTICAL) {
					*y = DESKTOP_PAD_VERTICAL;
				}
				return;
			}
		}		
	}
}

static void
mark_icon_location_in_grid (NautilusIcon *icon, int **icon_grid, int num_rows, int num_columns)
{
	int x1, x2, y1, y2;
	double width, height;
	int qx, qy, qwidth, qheight, qy_index;
	int grid_width, grid_height;

	icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);
	width = x2 - x1;
	height = y2 - y1;

	/* Convert x and y to our quantized grid value */
	qx = icon->x / CELL_SIZE;
	qy = icon->y / CELL_SIZE;
	qwidth = ceil (width / CELL_SIZE);
	qheight = ceil (height / CELL_SIZE);

	/* Check and correct for edge conditions */
	grid_width = num_rows;
	grid_height = num_columns;
	
	if ((qx + qwidth) > grid_width) {
		qwidth = grid_width;  
	} else {
		qwidth = qx + qwidth;
	}
	if ((qy + qheight) > grid_height) {
		qheight = grid_height;  
	} else {
		qheight = qy + qheight;  
	}
	
	/* Mark location */
	for (; qx < qwidth; qx++) {		
		for (qy_index = qy; qy_index < qheight; qy_index++) {
			icon_grid [qx] [qy_index] = 1;
		}
	}	
}

static void 
mark_icon_locations_in_grid (GList *icon_list, int **icon_grid, int num_rows, int num_columns)
{
	GList *p;
	NautilusIcon *icon;
	
	/* Mark filled grid locations */
	for (p = icon_list; p != NULL; p = p->next) {
		icon = p->data;
		mark_icon_location_in_grid (icon, icon_grid, num_rows, num_columns);
	}
}

static void
lay_down_icons_tblr (NautilusIconContainer *container, GList *icons)
{
	GList *p, *placed_icons, *unplaced_icons;
	int index, total, new_length, placed;
	NautilusIcon *icon;
	int width, height, max_width, icon_width, icon_height;
	int x, y, x1, x2, y1, y2;
	int *grid_memory;
	int **icon_grid;
	int num_rows, num_columns;
	int row, column;

	/* Get container dimensions */
	width  = GTK_WIDGET (container)->allocation.width
		- container->details->left_margin
		- container->details->right_margin;
	height = GTK_WIDGET (container)->allocation.height
		- container->details->top_margin
		- container->details->bottom_margin;

	/* Determine which icons have and have not been placed */
	placed_icons = NULL;
	unplaced_icons = NULL;
	
	total = g_list_length (container->details->icons);
	new_length = g_list_length (icons);
	placed = total - new_length;
	if (placed > 0) {
		/* Add only placed icons in list */
		for (p = container->details->icons; p != NULL; p = p->next) {
			icon = p->data;
			if (icon_is_positioned (icon)) {
				placed_icons = g_list_prepend (placed_icons, icon);
			} else {
				icon->x = 0;
				icon->y = 0;
				unplaced_icons = g_list_prepend (unplaced_icons, icon);
			}
		}
		placed_icons = g_list_reverse (placed_icons);
		unplaced_icons = g_list_reverse (unplaced_icons);
					
		/* Allocate grid array */
		num_rows = width / CELL_SIZE;
		num_columns = height / CELL_SIZE;

		/* Allocate array memory */
		grid_memory = malloc (num_rows * num_columns * sizeof (int *));
		g_assert (grid_memory);

		/* Allocate room for the pointers to the rows */
		icon_grid = malloc (num_rows * sizeof (int *));
		g_assert (icon_grid);

		/* Point to array pointers */
		for (index = 0; index < num_rows; index++) {
			icon_grid[index] = grid_memory + (index * num_columns);
		}

		/* Set all grid values to unfilled */
		for (row = 0; row < num_rows; row++) {
			for (column = 0; column < num_columns; column++) {
				icon_grid [row] [column] = 0;
			}
		}
		
		/* Mark filled grid locations */
		mark_icon_locations_in_grid (placed_icons, icon_grid, num_rows, num_columns);

		/* Place unplaced icons in the best locations */
		for (p = unplaced_icons; p != NULL; p = p->next) {
			icon = p->data;
			get_best_empty_grid_location (icon, icon_grid, num_rows, num_columns,
						      &x, &y);
			icon_set_position (icon, x, y);
			/* Add newly placed icon to grid */
			mark_icon_location_in_grid (icon, icon_grid, num_rows, num_columns);
		}

		/* Clean up */
		free (icon_grid);
		free (grid_memory);
		g_list_free (placed_icons);
		g_list_free (unplaced_icons);
	} else {
		/* There are no placed icons.  Just lay them down using our rules */		
		x = DESKTOP_PAD_HORIZONTAL;
		y = DESKTOP_PAD_VERTICAL;
		max_width = 0;

		for (p = icons; p != NULL; p = p->next) {
			icon = p->data;
			icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);
			icon_set_position (icon, x, y);
			
			icon_width = x2 - x1;
			icon_height = y2 - y1;
			y += icon_height + 10;

			/* Check for increase in column width */
			if (max_width < icon_width) {
				max_width = icon_width;
			}
			
			/* Check and see if we need to move to a new column */
			if (y > height - icon_height) {
				x += max_width + DESKTOP_PAD_VERTICAL;
				y = DESKTOP_PAD_VERTICAL;
			}
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
		lay_down_icons_horizontal (container, icons, start_y);
		break;
		
	case NAUTILUS_ICON_LAYOUT_T_B_L_R:
	case NAUTILUS_ICON_LAYOUT_T_B_R_L:
		/* FIXME bugzilla.gnome.org 42479: 
		 * Need to handle T_B_R_L differently. */
		lay_down_icons_tblr (container, icons);
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

	nautilus_icon_container_update_scroll_region (container);

	process_pending_icon_to_reveal (container);
	process_pending_icon_to_rename (container);
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
		gtk_idle_remove (container->details->idle_id);
		container->details->idle_id = 0;
	}
}

static void
schedule_redo_layout (NautilusIconContainer *container)
{
	if (container->details->idle_id == 0
	    && container->details->has_been_allocated) {
		container->details->idle_id = gtk_idle_add
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
	ArtDRect bounds;
	double bottom;

	g_assert (!container->details->auto_layout);

	resort (container);

	no_position_icons = NULL;

	/* Place all the icons with positions. */
	bottom = 0;
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		have_stored_position = FALSE;
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[GET_STORED_ICON_POSITION],
				 icon->data,
				 &position,
				 &have_stored_position);
		if (have_stored_position) {
			icon_set_position (icon, position.x, position.y);
			bounds = eel_gnome_canvas_item_get_world_bounds
				(GNOME_CANVAS_ITEM (icon->item));
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
				   double scale_x, double scale_y,
				   gboolean raise,
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

	if (!details->auto_layout) {
		if (x != icon->x || y != icon->y) {
			icon_set_position (icon, x, y);
			emit_signal = update_position;
		}
	}
	
	if (scale_x != icon->scale_x || scale_y != icon->scale_y) {
		icon->scale_x = scale_x;
		icon->scale_y = scale_y;
		nautilus_icon_container_update_icon (container, icon);
		if (update_position) {
			redo_layout (container); 
			emit_signal = TRUE;
		}

	}
	
	if (emit_signal) {
		position.x = icon->x;
		position.y = icon->y;
		position.scale_x = scale_x;
		position.scale_y = scale_y;
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[ICON_POSITION_CHANGED],
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
		   const ArtDRect *previous_rect,
		   const ArtDRect *current_rect)
{
	GList *p;
	gboolean selection_changed, is_in, canvas_rect_calculated;
	NautilusIcon *icon;
	ArtIRect canvas_rect;
			
	selection_changed = FALSE;
	canvas_rect_calculated = FALSE;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		
		if (!canvas_rect_calculated) {
			/* Only do this calculation once, since all the canvas items
			 * we are interating are in the same coordinate space
			 */
			canvas_rect = eel_gnome_canvas_world_to_canvas_rectangle
				(GNOME_CANVAS_ITEM (icon->item)->canvas, *current_rect);
			canvas_rect_calculated = TRUE;
		}
		
		is_in = nautilus_icon_canvas_item_hit_test_rectangle (icon->item, canvas_rect);
		
		g_assert (icon->was_selected_before_rubberband == FALSE
			  || icon->was_selected_before_rubberband == TRUE);
		selection_changed |= icon_set_selected
			(container, icon,
			 is_in ^ icon->was_selected_before_rubberband);
	}

	if (selection_changed) {
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
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
	ArtDRect selection_rect;

	widget = GTK_WIDGET (data);
	container = NAUTILUS_ICON_CONTAINER (data);
	band_info = &container->details->rubberband_info;

	g_assert (band_info->timer_id != 0);
	g_assert (GNOME_IS_CANVAS_RECT (band_info->selection_rectangle));

	gdk_window_get_pointer (widget->window, &x, &y, NULL);

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
	    && (int) band_info->prev_x == x && (int) band_info->prev_y == y) {
		return TRUE;
	}

	nautilus_icon_container_scroll (container, x_scroll, y_scroll);

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);

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

	gnome_canvas_item_set
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
	
	gnome_canvas_item_raise_to_top (band_info->selection_rectangle);
	

	band_info->prev_x = x;
	band_info->prev_y = y;

	band_info->prev_rect = selection_rect;

	return TRUE;
}

static void
start_rubberbanding (NautilusIconContainer *container,
		     GdkEventButton *event)
{
	NautilusIconContainerDetails *details;
	NautilusIconRubberbandInfo *band_info;
	uint fill_color, outline_color;
	char *fill_color_str;
	GList *p;

	details = container->details;
	band_info = &details->rubberband_info;

	gtk_signal_emit (GTK_OBJECT (container),
			 signals[BAND_SELECT_STARTED]);

	for (p = details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		icon->was_selected_before_rubberband = icon->is_selected;
	}

	gnome_canvas_window_to_world
		(GNOME_CANVAS (container),
		 event->x, event->y,
		 &band_info->start_x, &band_info->start_y);

	/* FIXME: The code to extract colors from the theme should be in FMDirectoryView, not here.
	 * The NautilusIconContainer class should simply provide calls to set the colors.
	 */
	if (GNOME_CANVAS(container)->aa) {
		/* FIXME: Should use some standard format, not just a 32-bit integer. */
		fill_color_str = nautilus_theme_get_theme_data ("directory", "selection_box_color_rgba");
		if (fill_color_str == NULL) {
			fill_color = 0x77BBDD40;
		} else {
			fill_color = strtoul (fill_color_str, NULL, 0);
			/* FIXME: Need error handling here. */
			g_free (fill_color_str);
		}
		
		outline_color = fill_color | 255;

		band_info->selection_rectangle = gnome_canvas_item_new
			(gnome_canvas_root
			 (GNOME_CANVAS (container)),
			 eel_canvas_rect_get_type (),
			 "x1", band_info->start_x,
			 "y1", band_info->start_y,
			 "x2", band_info->start_x,
			 "y2", band_info->start_y,
			 "fill_color_rgba", fill_color,
			 "outline_color_rgba", outline_color,
			 "width_pixels", 1,
			 NULL);
	
	} else {
		fill_color_str = nautilus_theme_get_theme_data ("directory", "selection_box_color");
		if (fill_color_str == NULL) {
			fill_color_str = g_strdup ("rgb:7777/BBBB/DDDD");
		}

		band_info->selection_rectangle = gnome_canvas_item_new
			(gnome_canvas_root
			 (GNOME_CANVAS (container)),
			 eel_canvas_rect_get_type (),
			 "x1", band_info->start_x,
			 "y1", band_info->start_y,
			 "x2", band_info->start_x,
			 "y2", band_info->start_y,
			 "fill_color", fill_color_str,
			 "fill_stipple", stipple,
			 "outline_color", fill_color_str,
			 "width_pixels", 1,
			 NULL);
		g_free (fill_color_str);
	}
	
	band_info->prev_x = event->x;
	band_info->prev_y = event->y;

	band_info->active = TRUE;

	if (band_info->timer_id == 0) {
		band_info->timer_id = gtk_timeout_add
			(RUBBERBAND_TIMEOUT_INTERVAL,
			 rubberband_timeout_callback,
			 container);
	}

	gnome_canvas_item_grab (band_info->selection_rectangle,
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK),
				NULL, event->time);
}

static void
stop_rubberbanding (NautilusIconContainer *container,
		    GdkEventButton *event)
{
	NautilusIconRubberbandInfo *band_info;

	band_info = &container->details->rubberband_info;

	g_assert (band_info->timer_id != 0);
	gtk_timeout_remove (band_info->timer_id);
	band_info->timer_id = 0;

	band_info->active = FALSE;

	/* Destroy this canvas item; the parent will unref it. */
	gnome_canvas_item_ungrab (band_info->selection_rectangle, event->time);
	gtk_object_destroy (GTK_OBJECT (band_info->selection_rectangle));
	band_info->selection_rectangle = NULL;


	gtk_signal_emit (GTK_OBJECT (container),
			 signals[BAND_SELECT_ENDED]);
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
compare_icons_horizontal_first (NautilusIconContainer *container,
				NautilusIcon *icon_a,
				NautilusIcon *icon_b)
{
	if (icon_a->x < icon_b->x) {
		return -1;
	}
	if (icon_a->x > icon_b->x) {
		return +1;
	}
	if (icon_a->y < icon_b->y) {
		return -1;
	}
	if (icon_a->y > icon_b->y) {
		return +1;
	}
	return compare_icons_by_uri (container, icon_a, icon_b);
}

static int
compare_icons_vertical_first (NautilusIconContainer *container,
			      NautilusIcon *icon_a,
			      NautilusIcon *icon_b)
{
	if (icon_a->y < icon_b->y) {
		return -1;
	}
	if (icon_a->y > icon_b->y) {
		return +1;
	}
	if (icon_a->x < icon_b->x) {
		return -1;
	}
	if (icon_a->x > icon_b->x) {
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
	ArtIRect bounds;

	bounds = eel_gnome_canvas_item_get_current_canvas_bounds (GNOME_CANVAS_ITEM (icon->item));
	if (container->details->arrow_key_start < bounds.y0) {
		return -1;
	}
	if (container->details->arrow_key_start > bounds.y1) {
		return +1;
	}
	return 0;
}

static int
compare_with_start_column (NautilusIconContainer *container,
			   NautilusIcon *icon)
{
	ArtIRect bounds;
	
	bounds = eel_gnome_canvas_item_get_current_canvas_bounds (GNOME_CANVAS_ITEM (icon->item));
	if (container->details->arrow_key_start < bounds.x0) {
		return -1;
	}
	if (container->details->arrow_key_start > bounds.x1) {
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
	/* Candidates not on the start column do not qualify. */
	if (compare_with_start_column (container, candidate) != 0) {
		return FALSE;
	}

	/* Candidates that are lower lose out. */
	if (best_so_far != NULL) {
		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) < 0) {
			return FALSE;
		}
	}

	/* Candidate above the start do not qualify. */
	if (compare_icons_vertical_first (container,
					  candidate,
					  start_icon) <= 0) {
		return FALSE;
	}

	return TRUE;
}

static void
keyboard_move_to (NautilusIconContainer *container,
		  NautilusIcon *icon,
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
	} else {
		/* Select icons and get rid of the special keyboard focus. */
		clear_keyboard_focus (container);
		if (select_one_unselect_others (container, icon)) {
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
		}
	}
	schedule_keyboard_icon_reveal (container, icon);
}

static void
keyboard_home (NautilusIconContainer *container,
	       GdkEventKey *event)
{
	/* Home selects the first icon.
	 * Control-Home sets the keyboard focus to the first icon.
	 */
	container->details->arrow_key_axis = AXIS_NONE;
	keyboard_move_to (container,
			  find_best_icon (container,
					  NULL,
					  leftmost_in_top_row,
					  NULL),
			  event);
}

static void
keyboard_end (NautilusIconContainer *container,
	      GdkEventKey *event)
{
	/* End selects the last icon.
	 * Control-End sets the keyboard focus to the last icon.
	 */
	container->details->arrow_key_axis = AXIS_NONE;
	keyboard_move_to (container,
			  find_best_icon (container,
					  NULL,
					  rightmost_in_bottom_row,
					  NULL),
			  event);
}

static void
record_arrow_key_start (NautilusIconContainer *container,
			NautilusIcon *icon,
			Axis arrow_key_axis)
{
	ArtDRect world_rect;

	if (container->details->arrow_key_axis == arrow_key_axis) {
		return;
	}

	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
	gnome_canvas_w2c
		(GNOME_CANVAS (container),
		 (world_rect.x0 + world_rect.x1) / 2,
		 (world_rect.y0 + world_rect.y1) / 2,
		 arrow_key_axis == AXIS_VERTICAL
		 ? &container->details->arrow_key_start : NULL,
		 arrow_key_axis == AXIS_HORIZONTAL
		 ? &container->details->arrow_key_start : NULL);
	container->details->arrow_key_axis = arrow_key_axis;
}

static void
keyboard_arrow_key (NautilusIconContainer *container,
		    GdkEventKey *event,
		    Axis axis,
		    IsBetterIconFunction better_start,
		    IsBetterIconFunction empty_start,
		    IsBetterIconFunction better_destination)
{
	NautilusIcon *icon;

	/* Chose the icon to start with.
	 * If we have a keyboard focus, start with it.
	 * Otherwise, use the single selected icon.
	 * If there's multiple selection, use the icon farthest toward the end.
	 */
	icon = container->details->keyboard_focus;
	if (icon == NULL) {
		if (has_multiple_selection (container)) {
			icon = find_best_selected_icon
				(container, NULL,
				 better_start, NULL);
		} else {
			icon = get_first_selected_icon (container);
		}
	}

	/* If there's no icon, select the icon farthest toward the end.
	 * If there is an icon, select the next icon based on the arrow direction.
	 */
	if (icon == NULL) {
		container->details->arrow_key_axis = AXIS_NONE;
		icon = find_best_icon
			(container, NULL,
			 empty_start, NULL);
	} else {
		record_arrow_key_start (container, icon, axis);
		icon = find_best_icon
			(container, icon,
			 better_destination, NULL);
	}

	keyboard_move_to (container, icon, event);
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
			    AXIS_HORIZONTAL,
			    rightmost_in_bottom_row,
			    leftmost_in_top_row,
			    same_row_right_side_leftmost);
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
			    AXIS_HORIZONTAL,
			    leftmost_in_top_row,
			    rightmost_in_bottom_row,
			    same_row_left_side_rightmost);
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
			    AXIS_VERTICAL,
			    rightmost_in_bottom_row,
			    leftmost_in_top_row,
			    same_column_below_highest);
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
			    AXIS_VERTICAL,
			    leftmost_in_top_row,
			    rightmost_in_bottom_row,
			    same_column_above_lowest);
}

static void
keyboard_space (NautilusIconContainer *container,
		GdkEventKey *event)
{
	/* Control-space toggles the selection state of the current icon. */
	if (container->details->keyboard_focus != NULL &&
	    (event->state & GDK_CONTROL_MASK) != 0) {
		icon_toggle_selected (container, container->details->keyboard_focus);
		gtk_signal_emit (GTK_OBJECT (container), signals[SELECTION_CHANGED]);
	}
}

/* look for the first icon that matches the longest part of a given
 * search pattern
 */
typedef struct {
	char *name;
	int last_match_length;
} BestNameMatch;

static gboolean
match_best_name (NautilusIconContainer *container,
		 NautilusIcon *start_icon,
		 NautilusIcon *best_so_far,
		 NautilusIcon *candidate,
		 void *data)
{
	BestNameMatch *match_state;
	const char *name;
	int match_length;

	match_state = (BestNameMatch *) data;

	name = nautilus_icon_canvas_item_get_editable_text (candidate->item);

	/* This can happen if a key event is handled really early while loading
	 * the icon container, before the items have all been updated once.
	 */
	if (name == NULL) {
		return FALSE;
	}

	for (match_length = 0; ; match_length++) {
		if (name[match_length] == '\0'
		    || match_state->name[match_length] == '\0') {
			break;
		}

		/* Require the match pattern to already be lowercase. */
		g_assert (((char) tolower ((guchar) match_state->name[match_length]))
			  == match_state->name[match_length]);
			
		if (((char) tolower ((guchar) name[match_length]))
		    != match_state->name[match_length]) {
			break;
		}
	}

	if (match_length > match_state->last_match_length) {
		/* This is the longest pattern match sofar, remember the
		 * length and return with a candidate.
		 */
		match_state->last_match_length = match_length;
		return TRUE;
	}

	return FALSE;
}

static void
select_matching_name (NautilusIconContainer *container,
		      const char *match_name)
{
	int index;
	NautilusIcon *icon;
	BestNameMatch match_state;

	match_state.name = g_strdup (match_name);
	match_state.last_match_length = 0;

	/* a little optimization for case-insensitive match - convert the
	 * pattern to lowercase ahead of time
	 */
	for (index = 0; ; index++) {
		if (match_state.name[index] == '\0')
			break;
		match_state.name[index] = tolower ((guchar) match_state.name[index]);
	}

	icon = find_best_icon (container,
			       NULL,
			       match_best_name,
			       &match_state);
	if (icon == NULL) {
		return;
	}

	/* Select icons and get rid of the special keyboard focus. */
	clear_keyboard_focus (container);
	if (select_one_unselect_others (container, icon)) {
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	}
	schedule_keyboard_icon_reveal (container, icon);

	g_free (match_state.name);
}

static GList *
build_icon_list_sorted_by_name (NautilusIconContainer *container)
{
	GList *result;
	
	if (container->details->icons == NULL) {
		return NULL;
	}

	result = eel_g_list_copy (container->details->icons);
	sort_icons_by_name (container, &result);

	return result;
}

static void
select_previous_or_next_name (NautilusIconContainer *container, 
			      gboolean next, 
			      GdkEventKey *event)
{
	NautilusIcon *icon;
	GList *list;
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

	list = build_icon_list_sorted_by_name (container);

	if (icon != NULL) {
		/* must have at least @icon in the list */
		g_assert (list != NULL);
		item = g_list_find (list, icon);
		g_assert (item != NULL);
		
		item = next ? item->next : item->prev;
	} else if (list != NULL) {
		/* no selection yet, pick the first or last item to select */
		item = next ? g_list_first (list) : g_list_last (list);
	}

	icon = (item != NULL) ? item->data : NULL;

	if (icon != NULL) {
		keyboard_move_to (container, icon, event);
	}

	g_list_free (list);
}

/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	NautilusIconContainer *container;
	guint i;

	container = NAUTILUS_ICON_CONTAINER (object);

	nautilus_icon_dnd_fini (container);
        nautilus_icon_container_clear (container);

	if (container->details->rubberband_info.timer_id != 0) {
		gtk_timeout_remove (container->details->rubberband_info.timer_id);
	}
	if (container->details->rubberband_info.selection_rectangle != NULL) {
		/* Destroy this canvas item; the parent will unref it. */
		gtk_object_destroy (GTK_OBJECT (container->details->rubberband_info.selection_rectangle));
	}

        if (container->details->idle_id != 0) {
		gtk_idle_remove (container->details->idle_id);
	}

	if (container->details->stretch_idle_id != 0) {
		gtk_idle_remove (container->details->stretch_idle_id);
	}
       
        for (i = 0; i < EEL_N_ELEMENTS (container->details->label_font); i++) {
       		if (container->details->label_font[i] != NULL)
        		gdk_font_unref (container->details->label_font[i]);
	}

	if (container->details->smooth_label_font != NULL) {
		gtk_object_unref (GTK_OBJECT (container->details->smooth_label_font));
	}

	if (container->details->highlight_frame != NULL) {
		gdk_pixbuf_unref (container->details->highlight_frame);
	}

	if (container->details->rename_widget != NULL) {
		gtk_object_destroy (GTK_OBJECT (container->details->rename_widget));
	}

	/* FIXME: The code to extract colors from the theme should be in FMDirectoryView, not here.
	 * The NautilusIconContainer class should simply provide calls to set the colors.
	 */
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_icon_container_theme_changed,
					      container);
	
	nautilus_icon_container_flush_typeselect_state (container);

	g_hash_table_destroy (container->details->icon_set);
	container->details->icon_set = NULL;
	
	g_free (container->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* GtkWidget methods.  */

static void
size_request (GtkWidget *widget,
	      GtkRequisition *requisition)
{
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

	/* FIXME bugzilla.gnome.org 47219: 
	 * We shouldn't have to redo the layout when x, y, or height
	 * changes, only when width changes. However, just removing these
	 * tests causes a problem when you're vertically stretching a window
	 * taller than the size needed to display all contents (the whole
	 * batch of contents start moving down, centered in the extra space).
	 */
	if (allocation->x != widget->allocation.x
	    || allocation->width != widget->allocation.width
	    || allocation->y != widget->allocation.y
	    || allocation->height != widget->allocation.height) {
		need_layout_redone = TRUE;
	}
	
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	container->details->has_been_allocated = TRUE;

	if (need_layout_redone) {
		redo_layout (NAUTILUS_ICON_CONTAINER (widget));
	}
}

static void
realize (GtkWidget *widget)
{
	GtkStyle *style;
	GtkWindow *window;

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, realize, (widget));

	style = gtk_style_copy (gtk_widget_get_style (widget));
	style->bg[GTK_STATE_NORMAL] = style->base[GTK_STATE_NORMAL];
	gtk_widget_set_style (widget, style);

	gdk_window_set_background
		(GTK_LAYOUT (widget)->bin_window,
		 &widget->style->bg[GTK_STATE_NORMAL]);

	/* reduce flicker when scrolling by setting the back pixmap to NULL */
	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window,
				    NULL, FALSE);
	
 	/* make us the focused widget */
 	g_assert (GTK_IS_WINDOW (gtk_widget_get_toplevel (widget)));
	window = GTK_WINDOW (gtk_widget_get_toplevel (widget));
	gtk_window_set_focus (window, widget);
}

static void
unrealize (GtkWidget *widget)
{
	GtkWindow *window;
        g_assert (GTK_IS_WINDOW (gtk_widget_get_toplevel (widget)));
        window = GTK_WINDOW (gtk_widget_get_toplevel (widget));
	gtk_window_set_focus (window, NULL);

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, unrealize, (widget));
}

static gboolean
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	NautilusIconContainer *container;
	gboolean selection_changed;
	gboolean return_value;
	gboolean clicked_on_icon;
	gint64 current_time;
	static gint64 last_click_time = 0;
	static gint click_count = 0;
	
	/* Determine click count */
	current_time = eel_get_system_time ();
	if (current_time - last_click_time < DOUBLE_CLICK_TIME) {
		click_count++;
	} else {
		click_count = 0;
	}

	/* Stash time for next compare */
	last_click_time = current_time;

	/* Ignore double click if we are in single click mode */
	if (click_policy_auto_value == NAUTILUS_CLICK_POLICY_SINGLE && click_count >= 2) {		
		return TRUE;
	}

	container = NAUTILUS_ICON_CONTAINER (widget);
        container->details->button_down_time = event->time;
	
        /* Forget about the old keyboard selection now that we've started mousing. */
        clear_keyboard_focus (container);

	/* Forget about where we began with the arrow keys now that we're mousing. */
	container->details->arrow_key_axis = AXIS_NONE;
	
	/* Forget the typeahead state. */
	nautilus_icon_container_flush_typeselect_state (container);
	
	/* Invoke the canvas event handler and see if an item picks up the event. */
	clicked_on_icon = EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, button_press_event, (widget, event));
	
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

	/* Button 1 does rubber banding. */
	if (event->button == RUBBERBAND_BUTTON) {
		if (! button_event_modifies_selection (event)) {
			selection_changed = unselect_all (container);
			if (selection_changed) {
				gtk_signal_emit (GTK_OBJECT (container),
						 signals[SELECTION_CHANGED]);
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
		gtk_signal_emit (GTK_OBJECT (widget), signals[MIDDLE_CLICK], event);
		return TRUE;
	}

	/* Button 3 does a contextual menu. */
	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		end_renaming_mode (container, TRUE);
		gtk_signal_emit (GTK_OBJECT (widget), signals[CONTEXT_CLICK_BACKGROUND], event);
		return TRUE;
	}
	
	/* Otherwise, we emit a button_press message. */
	gtk_signal_emit (GTK_OBJECT (widget),
			 signals[BUTTON_PRESS], event,
			 &return_value);
	return return_value;
}

static void
nautilus_icon_container_did_not_drag (NautilusIconContainer *container,
				      GdkEventButton *event)
{
	NautilusIconContainerDetails *details;
	gboolean selection_changed;
		
	details = container->details;

	if (!button_event_modifies_selection (event) && !details->drag_icon->is_selected) {
		selection_changed = select_one_unselect_others 
			(container, details->drag_icon);
		
		if (selection_changed) {
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
		}
	}
	
	if (details->drag_icon != NULL) {		
		/* If single-click mode, activate the selected icons, unless modifying
		 * the selection or pressing for a very long time.
		 */
		if (details->single_click_mode
		    && event->time - details->button_down_time < MAX_CLICK_TIME
		    && ! button_event_modifies_selection (event)) {

			/* It's a tricky UI issue whether this should activate
			 * just the clicked item (as if it were a link), or all
			 * the selected items (as if you were issuing an "activate
			 * selection" command). For now, we're trying the activate
			 * entire selection version to see how it feels. Note that
			 * NautilusList goes the other way because its "links" seem
			 * much more link-like.
			 */
			activate_selected_items (container);
		}
	}
}

static void
clear_drag_state (NautilusIconContainer *container)
{
	container->details->drag_icon = NULL;
	container->details->drag_state = DRAG_STATE_INITIAL;
	if (container->details->context_menu_timeout_id != 0) {
		gtk_timeout_remove (container->details->context_menu_timeout_id);
		container->details->context_menu_timeout_id = 0;
	}
}

static gboolean
start_stretching (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	ArtPoint world_point;
	GtkWidget *toplevel;

	details = container->details;
	icon = details->stretch_icon;
	
	/* Check if we hit the stretch handles. */
	world_point.x = details->drag_x;
	world_point.y = details->drag_y;
	if (!nautilus_icon_canvas_item_hit_test_stretch_handles
	    (icon->item, world_point)) {
		return FALSE;
	}

	/* Set up the dragging. */
	details->drag_state = DRAG_STATE_STRETCH;
	gnome_canvas_w2c (GNOME_CANVAS (container),
			  details->drag_x,
			  details->drag_y,
			  &details->stretch_start.pointer_x,
			  &details->stretch_start.pointer_y);
	gnome_canvas_w2c (GNOME_CANVAS (container),
			  icon->x, icon->y,
			  &details->stretch_start.icon_x,
			  &details->stretch_start.icon_y);
	icon_get_size (container, icon,
		       &details->stretch_start.icon_size, NULL);

	gnome_canvas_item_grab (GNOME_CANVAS_ITEM (icon->item),
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK),
				NULL,
				GDK_CURRENT_TIME);

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

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      details->window_x, details->window_y,
				      &world_x, &world_y);
	gnome_canvas_w2c (GNOME_CANVAS (container),
			  world_x, world_y,
			  &stretch_state.pointer_x, &stretch_state.pointer_y);

	compute_stretch (&details->stretch_start,
			 &stretch_state);

	gnome_canvas_c2w (GNOME_CANVAS (container),
			  stretch_state.icon_x, stretch_state.icon_y,
			  &world_x, &world_y);

	icon_set_position (icon, world_x, world_y);
	icon_set_size (container, icon, stretch_state.icon_size, FALSE);

	container->details->stretch_idle_id = 0;

	return FALSE;
}	

static void
continue_stretching (NautilusIconContainer *container,
		     int window_x, int window_y)
{

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->window_x = window_x;
	container->details->window_y = window_y;

	if (container->details->stretch_idle_id == 0) {		
		container->details->stretch_idle_id = gtk_idle_add ((GtkFunction) update_stretch_at_idle, container);
	}
}

static void
ungrab_stretch_icon (NautilusIconContainer *container)
{
	gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (container->details->stretch_icon->item),
				  GDK_CURRENT_TIME);
}

static void
end_stretching (NautilusIconContainer *container,
		int window_x, int window_y)
{
	NautilusIconPosition position;
	NautilusIcon *icon;
	
	continue_stretching (container, window_x, window_y);
	ungrab_stretch_icon (container);

	/* now that we're done stretching, update the icon's position */
	icon = container->details->drag_icon;	
	position.x = icon->x;
	position.y = icon->y;
	position.scale_x = icon->scale_x;
	position.scale_y = icon->scale_y;
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[ICON_POSITION_CHANGED],
			 icon->data, &position);
	
	clear_drag_state (container);
	redo_layout (container);
}

static void
undo_stretching (NautilusIconContainer *container)
{
	NautilusIcon *stretched_icon;

	stretched_icon = container->details->stretch_icon;

	if (stretched_icon == NULL) {
		return;
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
		       TRUE);
	
	container->details->stretch_icon = NULL;				
	emit_stretch_ended (container, stretched_icon);
	redo_layout (container);
}

static gboolean
button_release_event (GtkWidget *widget,
		      GdkEventButton *event)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;

	container = NAUTILUS_ICON_CONTAINER (widget);
	details = container->details;

	if (event->button == RUBBERBAND_BUTTON && details->rubberband_info.active) {
		stop_rubberbanding (container, event);
		return TRUE;
	}
	
	if (event->button == details->drag_button) {
		details->drag_button = 0;

		switch (details->drag_state) {
		case DRAG_STATE_MOVE_COPY_OR_MENU:
			if (!details->drag_started) {
				/* Right click, drag did not start,
				 * show context menu.
				 */
				clear_drag_state (container);
				gtk_signal_emit (GTK_OBJECT (container),
						 signals[CONTEXT_CLICK_SELECTION],
						 event);
				break;
			}
			/* fall through */

		case DRAG_STATE_MOVE_OR_COPY:
			if (!details->drag_started) {
				nautilus_icon_container_did_not_drag (container, event);
			} else {
				nautilus_icon_dnd_end_drag (container);
			}
			break;
		case DRAG_STATE_STRETCH:
			end_stretching (container, event->x, event->y);
			break;
		default:
			break;
		}

		clear_drag_state (container);
		return TRUE;
	}

	return EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, button_release_event, (widget, event));
}

static int
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *event)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;
	double world_x, world_y;

	container = NAUTILUS_ICON_CONTAINER (widget);
	details = container->details;

	if (details->drag_button != 0) {
		switch (details->drag_state) {
		case DRAG_STATE_MOVE_COPY_OR_MENU:
			if (details->drag_started) {
				break;
			}
			gtk_timeout_remove (details->context_menu_timeout_id);
			/* fall through */

		case DRAG_STATE_MOVE_OR_COPY:
			if (details->drag_started) {
				break;
			}

			gnome_canvas_window_to_world
				(GNOME_CANVAS (container),
				 event->x, event->y,
				 &world_x, &world_y);
			
			if (abs (details->drag_x - world_x) >= SNAP_RESISTANCE
			    || abs (details->drag_y - world_y) >= SNAP_RESISTANCE) {
				details->drag_started = TRUE;

				end_renaming_mode (container, TRUE);
				
				/* KLUDGE ALERT: Poke the starting values into the motion
				 * structure so that dragging behaves as expected.
				 */
				event->x = details->drag_x;
				event->y = details->drag_y;
			
				nautilus_icon_dnd_begin_drag (container,
							      details->drag_state == DRAG_STATE_MOVE_OR_COPY
							      ? (GDK_ACTION_MOVE 
								 | GDK_ACTION_COPY 
								 | GDK_ACTION_LINK
								 | GDK_ACTION_ASK)
							      : GDK_ACTION_ASK,
							      details->drag_button,
							      event);
				details->drag_state = DRAG_STATE_MOVE_OR_COPY;
			}
			break;
		case DRAG_STATE_STRETCH:
			continue_stretching (container, event->x, event->y);
			break;
		default:
			break;
		}
	}

	return EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, motion_notify_event, (widget, event));
}

void
nautilus_icon_container_flush_typeselect_state (NautilusIconContainer *container)
{
	if (container->details->type_select_state == NULL) {
		return;
	}
	
	g_free (container->details->type_select_state->type_select_pattern);
	g_free (container->details->type_select_state);
	container->details->type_select_state = NULL;
}

static gboolean
handle_typeahead (NautilusIconContainer *container, const char *key_string)
{
	char *new_pattern;
	gint64 now;
	gint64 time_delta;
	int key_string_length;
	int index;

	g_assert (key_string != NULL);
	g_assert (strlen (key_string) < 5);

	key_string_length = strlen (key_string);

	if (key_string_length == 0) {
		/* can be an empty string if the modifier was held down, etc. */
		return FALSE;
	}

	/* only handle if printable keys typed */
	for (index = 0; index < key_string_length; index++) {
		if (!isprint ((guchar) key_string[index])) {
			return FALSE;
		}
	}

	/* lazily allocate the typeahead state */
	if (container->details->type_select_state == NULL) {
		container->details->type_select_state = g_new0 (TypeSelectState, 1);
	}

	/* find out how long since last character was typed */
	now = eel_get_system_time ();
	time_delta = now - container->details->type_select_state->last_typeselect_time;
	if (time_delta < 0 || time_delta > NAUTILUS_TYPESELECT_FLUSH_DELAY) {
		/* the typeselect state is too old, start with a fresh one */
		g_free (container->details->type_select_state->type_select_pattern);
		container->details->type_select_state->type_select_pattern = NULL;
	}

	if (container->details->type_select_state->type_select_pattern != NULL) {
		new_pattern = g_strconcat
			(container->details->type_select_state->type_select_pattern,
			 key_string, NULL);
		g_free (container->details->type_select_state->type_select_pattern);
	} else {
		new_pattern = g_strdup (key_string);
	}

	container->details->type_select_state->type_select_pattern = new_pattern;
	container->details->type_select_state->last_typeselect_time = now;

	select_matching_name (container, new_pattern);

	return TRUE;
}

static int
key_press_event (GtkWidget *widget,
		 GdkEventKey *event)
{
	NautilusIconContainer *container;
	gboolean handled;
	gboolean flush_typeahead;

	container = NAUTILUS_ICON_CONTAINER (widget);
	handled = FALSE;
	flush_typeahead = TRUE;

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
			keyboard_home (container, event);
			handled = TRUE;
			break;
		case GDK_End:
			keyboard_end (container, event);
			handled = TRUE;
			break;
		case GDK_Left:
			keyboard_left (container, event);
			handled = TRUE;
			break;
		case GDK_Up:
			keyboard_up (container, event);
			handled = TRUE;
			break;
		case GDK_Right:
			keyboard_right (container, event);
			handled = TRUE;
			break;
		case GDK_Down:
			keyboard_down (container, event);
			handled = TRUE;
			break;
		case GDK_space:
			keyboard_space (container, event);
			handled = TRUE;
			break;
		case GDK_Tab:
		case GDK_ISO_Left_Tab:
			select_previous_or_next_name (container, 
						      (event->state & GDK_SHIFT_MASK) == 0, event);
			handled = TRUE;
			break;
		case GDK_Return:
		case GDK_KP_Enter:
			activate_selected_items (container);
			handled = TRUE;
			break;
		case GDK_Escape:
			undo_stretching (container);
			handled = TRUE;
			break;
		default:
			/* Don't use Control or Alt keys for type-selecting, because they
			 * might be used for menus.
			 */
			handled = (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0 &&
				handle_typeahead (container, event->string);
			flush_typeahead = !handled;
			break;
		}
	}

	if (flush_typeahead) {
		/* any non-ascii key will force the typeahead state to be forgotten */
		nautilus_icon_container_flush_typeselect_state (container);
	}

	if (!handled) {
		handled = EEL_CALL_PARENT_WITH_RETURN_VALUE
			(GTK_WIDGET_CLASS, key_press_event, (widget, event));
	}

	return handled;
}

/* Initialization.  */

static void
nautilus_icon_container_initialize_class (NautilusIconContainerClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	/* NautilusIconContainer class.  */

	class->button_press = NULL;

	/* GtkObject class.  */

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	/* Signals.  */

	signals[SELECTION_CHANGED]
		= gtk_signal_new ("selection_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     selection_changed),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	signals[BUTTON_PRESS]
		= gtk_signal_new ("button_press",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     button_press),
				  gtk_marshal_BOOL__POINTER,
				  GTK_TYPE_BOOL, 1,
				  GTK_TYPE_GDK_EVENT);
	signals[ACTIVATE]
		= gtk_signal_new ("activate",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     activate),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
	signals[CONTEXT_CLICK_SELECTION]
		= gtk_signal_new ("context_click_selection",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     context_click_selection),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
	signals[CONTEXT_CLICK_BACKGROUND]
		= gtk_signal_new ("context_click_background",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     context_click_background),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
	signals[MIDDLE_CLICK]
		= gtk_signal_new ("middle_click",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     middle_click),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
	signals[ICON_POSITION_CHANGED]
		= gtk_signal_new ("icon_position_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     icon_position_changed),
				  gtk_marshal_NONE__POINTER_POINTER,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER);
	signals[ICON_TEXT_CHANGED]
		= gtk_signal_new ("icon_text_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     icon_text_changed),
				  gtk_marshal_NONE__POINTER_POINTER,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_STRING);
	signals[ICON_STRETCH_STARTED]
		= gtk_signal_new ("icon_stretch_started",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     icon_stretch_started),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
	signals[ICON_STRETCH_ENDED]
		= gtk_signal_new ("icon_stretch_ended",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     icon_stretch_ended),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
	signals[RENAMING_ICON]
		= gtk_signal_new ("renaming_icon",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     renaming_icon),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
	signals[GET_ICON_IMAGES]
		= gtk_signal_new ("get_icon_images",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_images),
				  eel_gtk_marshal_POINTER__POINTER_STRING_POINTER,
				  GTK_TYPE_POINTER, 3,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_STRING,
				  GTK_TYPE_POINTER);
	signals[GET_ICON_TEXT]
		= gtk_signal_new ("get_icon_text",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_text),
				  eel_gtk_marshal_NONE__POINTER_STRING_STRING,
				  GTK_TYPE_NONE, 3,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_STRING,
				  GTK_TYPE_STRING);
	signals[GET_ICON_URI]
		= gtk_signal_new ("get_icon_uri",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_uri),
				  eel_gtk_marshal_STRING__POINTER,
				  GTK_TYPE_STRING, 1,
				  GTK_TYPE_POINTER);
	signals[GET_ICON_DROP_TARGET_URI]
		= gtk_signal_new ("get_icon_drop_target_uri",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_drop_target_uri),
				  eel_gtk_marshal_STRING__POINTER,
				  GTK_TYPE_STRING, 1,
				  GTK_TYPE_POINTER);
	signals[COMPARE_ICONS]
		= gtk_signal_new ("compare_icons",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     compare_icons),
				  eel_gtk_marshal_INT__POINTER_POINTER,
				  GTK_TYPE_INT, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER);
	signals[COMPARE_ICONS_BY_NAME]
		= gtk_signal_new ("compare_icons_by_name",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     compare_icons_by_name),
				  eel_gtk_marshal_INT__POINTER_POINTER,
				  GTK_TYPE_INT, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER);
	signals[MOVE_COPY_ITEMS] 
		= gtk_signal_new ("move_copy_items",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass, 
						     move_copy_items),
				  eel_gtk_marshal_NONE__POINTER_POINTER_POINTER_INT_INT_INT,
				  GTK_TYPE_NONE, 6,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT);
	signals[HANDLE_URI_LIST] 
		= gtk_signal_new ("handle_uri_list",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass, 
						     handle_uri_list),
				  eel_gtk_marshal_NONE__POINTER_INT_INT_INT,
				  GTK_TYPE_NONE, 4,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT);

	signals[GET_CONTAINER_URI] 
		= gtk_signal_new ("get_container_uri",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass, 
						     get_container_uri),
				  eel_gtk_marshal_STRING__NONE,
				  GTK_TYPE_STRING, 0);
	signals[CAN_ACCEPT_ITEM] 
		= gtk_signal_new ("can_accept_item",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass, 
						     can_accept_item),
				  eel_gtk_marshal_INT__POINTER_STRING,
				  GTK_TYPE_INT, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_STRING);
	signals[GET_STORED_ICON_POSITION]
		= gtk_signal_new ("get_stored_icon_position",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_stored_icon_position),
				  eel_gtk_marshal_BOOL__POINTER_POINTER,
				  GTK_TYPE_BOOL, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER);
	signals[LAYOUT_CHANGED]
		= gtk_signal_new ("layout_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     layout_changed),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	signals[PREVIEW]
		= gtk_signal_new ("preview",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     preview),
				  eel_gtk_marshal_INT__POINTER_INT,
				  GTK_TYPE_INT, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_BOOL);
	signals[BAND_SELECT_STARTED]
		= gtk_signal_new ("band_select_started",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     band_select_started),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	signals[BAND_SELECT_ENDED]
		= gtk_signal_new ("band_select_ended",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     band_select_ended),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

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

	/* Initialize the stipple bitmap.  */

	stipple = gdk_bitmap_create_from_data (NULL, stipple_bits, 2, 2);

	eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_CLICK_POLICY,
					       &click_policy_auto_value);
}

static void
handle_focus_out_event (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	/* End renaming and commit change. */
	end_renaming_mode (NAUTILUS_ICON_CONTAINER (widget), TRUE);
}

static void
nautilus_icon_container_initialize (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	EelBackground *background;
	
	details = g_new0 (NautilusIconContainerDetails, 1);

	details->icon_set = g_hash_table_new (g_direct_hash, g_direct_equal);

        details->zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;
 
 	/* font table - this isn't exactly proportional, but it looks better than computed */
	/* FIXME bugzilla.gnome.org 45093: Font name is hard-coded here. */
	/* FIXME bugzilla.gnome.org 45101: Font size is hard-coded here. */

        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALLEST] = nautilus_font_factory_get_font_by_family ("helvetica", 8);
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALLER] = nautilus_font_factory_get_font_by_family ("helvetica", 8);
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALL] = nautilus_font_factory_get_font_by_family ("helvetica", 10);
        details->label_font[NAUTILUS_ZOOM_LEVEL_STANDARD] = nautilus_font_factory_get_font_by_family ("helvetica", 12);
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGE] = nautilus_font_factory_get_font_by_family ("helvetica", 14);
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGER] = nautilus_font_factory_get_font_by_family ("helvetica", 18);
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGEST] = nautilus_font_factory_get_font_by_family ("helvetica", 18);

        details->smooth_label_font = eel_scalable_font_get_default_font ();
	
	/* These are the default font sizes.  The font sizes are configurable via
	 * nautilus_icon_container_set_font_size_table() 
	 */
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLEST] = 8;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_SMALLER] = 8;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_SMALL] = 10;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_STANDARD] = 12;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_LARGE] = 14;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_LARGER] = 18;
        details->font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST] = 18;

	container->details = details;

	/* Set up DnD.  */
	nautilus_icon_dnd_init (container, stipple);

	/* Make sure that we find out if the icons change. */
	gtk_signal_connect_object_while_alive
		(nautilus_icon_factory_get (),
		 "icons_changed",
		 nautilus_icon_container_request_update_all,
		 GTK_OBJECT (container));	

	/* when the background changes, we must set up the label text color */
	background = eel_get_widget_background (GTK_WIDGET (container));
	
	gtk_signal_connect
		(GTK_OBJECT(background),
		 "appearance_changed",
		 update_label_color,
		 GTK_OBJECT (container));	


	gtk_signal_connect (GTK_OBJECT (container), "focus-out-event", handle_focus_out_event, NULL);	

	/* FIXME: The code to extract colors from the theme should be in FMDirectoryView, not here.
	 * The NautilusIconContainer class should simply provide calls to set the colors.
	 */
	/* read in theme-dependent data */
	nautilus_icon_container_theme_changed (container);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_THEME, nautilus_icon_container_theme_changed, container);	
}

typedef struct {
	NautilusIconContainer *container;
	GdkEventButton	      *event;
} ContextMenuParameters;

static ContextMenuParameters *
context_menu_parameters_new (NautilusIconContainer *container,
			     GdkEventButton *event)
{
	ContextMenuParameters *parameters;

	parameters = g_new (ContextMenuParameters, 1);
	parameters->container = container;
	parameters->event = (GdkEventButton *)(gdk_event_copy ((GdkEvent *)event));

	return parameters;
}			     

static void
context_menu_parameters_free (ContextMenuParameters *parameters)
{
	gdk_event_free ((GdkEvent *)parameters->event);
	g_free (parameters);
}

static gboolean
show_context_menu_callback (void *cast_to_parameters)
{
	ContextMenuParameters *parameters;

	parameters = (ContextMenuParameters *)cast_to_parameters;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (parameters->container));

	if (parameters->container->details->drag_state == DRAG_STATE_MOVE_COPY_OR_MENU) {
		clear_drag_state (parameters->container);

		/* Context menu applies to all selected items. The only
		 * odd case is if this click deselected the icon under
		 * the mouse, but at least the behavior is consistent.
		 */
		gtk_signal_emit (GTK_OBJECT (parameters->container),
				 signals[CONTEXT_CLICK_SELECTION],
				 parameters->event);
	}

	context_menu_parameters_free (parameters);

	return TRUE;
}

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
	
	if (event->button != DRAG_BUTTON
	    && event->button != CONTEXTUAL_MENU_BUTTON) {
		return TRUE;
	}

	details = container->details;

	if (event->button == DRAG_BUTTON
	    || event->button == CONTEXTUAL_MENU_BUTTON) {
		details->drag_button = event->button;
		details->drag_icon = icon;
		details->drag_x = event->x;
		details->drag_y = event->y;
		details->drag_state = event->button == DRAG_BUTTON
			? DRAG_STATE_MOVE_OR_COPY : DRAG_STATE_MOVE_COPY_OR_MENU;
		details->drag_started = FALSE;

		/* Check to see if this is a click on the stretch handles.
		 * If so, it won't modify the selection.
		 */
		if (icon == container->details->stretch_icon) {
			if (start_stretching (container)) {
				return TRUE;
			}
		}

		if (event->button == CONTEXTUAL_MENU_BUTTON) {
			/* after a timeout we will decide if this is a
			 * context menu click or a drag start.
			 */
			if (details->context_menu_timeout_id == 0) {
				details->context_menu_timeout_id = gtk_timeout_add (
					CONTEXT_MENU_TIMEOUT_INTERVAL, 
					show_context_menu_callback, 
					context_menu_parameters_new (container, event));
			}
		}
	}

	/* Modify the selection as appropriate. Selection is modified
	 * the same way for contextual menu as it would be without. 
	 */
	if (button_event_modifies_selection (event)) {
		icon_toggle_selected (container, icon);
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	} else if (!icon->is_selected) {
		unselect_all (container);
		icon_set_selected (container, icon, TRUE);
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	}

	if (event->type == GDK_2BUTTON_PRESS && event->button == DRAG_BUTTON) {
		/* Double clicking does not trigger a D&D action. */
		details->drag_button = 0;
		details->drag_icon = NULL;

		activate_selected_items (container);
	}

	return TRUE;
}

static int
item_event_callback (GnomeCanvasItem *item,
		     GdkEvent *event,
		     gpointer data)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;

	container = NAUTILUS_ICON_CONTAINER (data);
	details = container->details;

	icon = NAUTILUS_ICON_CANVAS_ITEM (item)->user_data;
	g_return_val_if_fail (icon != NULL, FALSE);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
		if (handle_icon_button_press (container, icon, &event->button)) {
			/* Stop the event from being passed along further. Returning
			 * TRUE ain't enough. 
			 */
    			gtk_signal_emit_stop_by_name (GTK_OBJECT (item), "event");
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
	GtkWidget *container;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	container = gtk_widget_new (nautilus_icon_container_get_type (), NULL);
	
	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	return container;
}

/* Clear all of the icons in the container. */
void
nautilus_icon_container_clear (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	details = container->details;

	end_renaming_mode (container, TRUE);
	
	clear_keyboard_focus (container);
	unschedule_keyboard_icon_reveal (container);
	set_pending_icon_to_reveal (container, NULL);
	details->stretch_icon = NULL;
	details->drop_target = NULL;

	for (p = details->icons; p != NULL; p = p->next) {
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
	
	details = container->details;

	details->icons = g_list_remove (details->icons, icon);
	details->new_icons = g_list_remove (details->new_icons, icon);
	g_hash_table_remove (details->icon_set, icon->data);

	was_selected = icon->is_selected;

	if (details->keyboard_focus == icon) {
        	clear_keyboard_focus (container);
	}
	if (details->keyboard_icon_to_reveal == icon) {
		unschedule_keyboard_icon_reveal (container);
	}
	if (details->drop_target == icon) {
		details->drop_target = NULL;
	}
	if (details->pending_icon_to_reveal == icon) {
		set_pending_icon_to_reveal (container, NULL);
	}

	icon_free (icon);

	if (was_selected) {
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
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
	  	gtk_signal_emit (GTK_OBJECT (container),
				 signals[ACTIVATE],
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

void 
nautilus_icon_container_update_icon (NautilusIconContainer *container,
				     NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;
	guint icon_size_x, icon_size_y;
	guint min_image_size, max_image_size;
	guint width, height, scaled_width, scaled_height;
	double scale_factor;
	NautilusScalableIcon *scalable_icon;
	NautilusEmblemAttachPoints attach_points;
	GdkPixbuf *pixbuf, *emblem_pixbuf, *saved_pixbuf;
	GList *emblem_scalable_icons, *emblem_pixbufs, *p;
	char *editable_text, *additional_text;
	GdkFont *font;
	int smooth_font_size;

	if (icon == NULL) {
		return;
	}

	details = container->details;

	/* Get the icons. */
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_ICON_IMAGES],
			 icon->data,
			 (icon == details->drop_target) ? "accept" : "",
			 &emblem_scalable_icons,
			 &scalable_icon);

	/* compute the maximum size based on the scale factor */
	min_image_size = MINIMUM_IMAGE_SIZE * GNOME_CANVAS (container)->pixels_per_unit;
	max_image_size = MAXIMUM_IMAGE_SIZE * GNOME_CANVAS (container)->pixels_per_unit;
		
	/* Get the appropriate images for the file. */
	icon_get_size (container, icon, &icon_size_x, &icon_size_y);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(scalable_icon,
		 icon_size_x,
		 icon_size_y,
		 max_image_size * icon->scale_x,
		 max_image_size * icon->scale_y,
		 nautilus_icon_container_get_anti_aliased_mode (container),
		 &attach_points,
		 TRUE);
	
	nautilus_scalable_icon_unref (scalable_icon);

	/*  in the rare case an image is too small, scale it up */
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	if (width < min_image_size || height < min_image_size) {
		scale_factor = MAX (min_image_size  / (double) width, min_image_size / (double) height);
		/* don't let it exceed the maximum width in the other dimension */
		scale_factor = MIN (scale_factor, max_image_size / width);
		scale_factor = MIN (scale_factor, max_image_size / height);
		
		scaled_width  = floor (width * scale_factor + .5);
		scaled_height = floor (height * scale_factor + .5);
		saved_pixbuf = pixbuf;
		pixbuf = gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_BILINEAR);
		gdk_pixbuf_unref (saved_pixbuf);
	}
		
	emblem_pixbufs = NULL;
	
	icon_size_x = MAX (nautilus_get_icon_size_for_zoom_level (container->details->zoom_level)
			   * icon->scale_x, NAUTILUS_ICON_SIZE_SMALLEST);
	icon_size_y = MAX (nautilus_get_icon_size_for_zoom_level (container->details->zoom_level)
			   * icon->scale_y, NAUTILUS_ICON_SIZE_SMALLEST);
	for (p = emblem_scalable_icons; p != NULL; p = p->next) {
		emblem_pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data,
			 icon_size_x,
			 icon_size_y,
			 MAXIMUM_EMBLEM_SIZE,
			 MAXIMUM_EMBLEM_SIZE,
			 nautilus_icon_container_get_anti_aliased_mode (container),
			 NULL,
			 FALSE);
		if (emblem_pixbuf != NULL) {
			emblem_pixbufs = g_list_prepend
				(emblem_pixbufs, emblem_pixbuf);
		}
	}
	emblem_pixbufs = g_list_reverse (emblem_pixbufs);
	nautilus_scalable_icon_list_free (emblem_scalable_icons);

	/* Get both editable and non-editable icon text */
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_ICON_TEXT],
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
	
	font = details->label_font[details->zoom_level];

	smooth_font_size = details->font_size_table[details->zoom_level];
        
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->item),
			       "editable_text", editable_text,
			       "additional_text", additional_text,
			       "font", font,
			       "highlighted_for_drop", icon == details->drop_target,
			       "smooth_font_size", smooth_font_size,
			       "smooth_font", details->smooth_label_font,
			       NULL);
	
	nautilus_icon_canvas_item_set_image (icon->item, pixbuf);
	nautilus_icon_canvas_item_set_attach_points (icon->item, &attach_points);
	nautilus_icon_canvas_item_set_emblems (icon->item, emblem_pixbufs);

	/* Let the pixbufs go. */
	gdk_pixbuf_unref (pixbuf);
	eel_gdk_pixbuf_list_free (emblem_pixbufs);

	g_free (editable_text);
	g_free (additional_text);
}

static gboolean
assign_icon_position (NautilusIconContainer *container,
		      NautilusIcon *icon)
{
	gboolean have_stored_position;
	NautilusIconPosition position;

	/* Get the stored position. */
	have_stored_position = FALSE;
	position.scale_x = 1.0;
	position.scale_y = 1.0;
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_STORED_ICON_POSITION],
			 icon->data,
			 &position,
			 &have_stored_position);
	icon->scale_x = position.scale_x;
	icon->scale_y = position.scale_y;
	if (!container->details->auto_layout) {
		if (have_stored_position) {
			icon_set_position (icon, position.x, position.y);
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
	gnome_canvas_item_show (GNOME_CANVAS_ITEM (icon->item));

	gtk_signal_connect (GTK_OBJECT (icon->item), "event",
			    GTK_SIGNAL_FUNC (item_event_callback), container);
}

static void
finish_adding_new_icons (NautilusIconContainer *container)
{
	GList *p, *new_icons, *no_position_icons;
	NautilusIcon *icon;
	double bottom;

	new_icons = container->details->new_icons;
	container->details->new_icons = NULL;

	/* Position most icons (not unpositioned manual-layout icons). */
	new_icons = g_list_reverse (new_icons);
	no_position_icons = NULL;
	for (p = new_icons; p != NULL; p = p->next) {
		icon = p->data;
		if (!assign_icon_position (container, icon)) {
			no_position_icons = g_list_prepend (no_position_icons, icon);
		}
		finish_adding_icon (container, icon);
	}
	g_list_free (new_icons);

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
 * 
 * Add icon to represent @data to container.
 * Returns FALSE if there was already such an icon.
 **/
gboolean
nautilus_icon_container_add (NautilusIconContainer *container,
			     NautilusIconData *data)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	
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
	icon->scale_x = 1.0;
	icon->scale_y = 1.0;
 	icon->item = NAUTILUS_ICON_CANVAS_ITEM
		(gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (container)->root),
					nautilus_icon_canvas_item_get_type (),
					NULL));
	gnome_canvas_item_hide (GNOME_CANVAS_ITEM (icon->item));
	icon->item->user_data = icon;

	/* Put it on both lists. */
	details->icons = g_list_prepend (details->icons, icon);
	details->new_icons = g_list_prepend (details->new_icons, icon);

	g_hash_table_insert (details->icon_set, data, icon);

	/* Run an idle function to add the icons. */
	schedule_redo_layout (container);
	
	return TRUE;
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
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (container), pixels_per_unit);

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
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	for (p = container->details->icons; p != NULL; p = p->next) {
		nautilus_icon_container_update_icon (container, p->data);
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
 * nautilus_icon_container_get_anti_aliased_mode:
 * get the state of the anti_aliased boolean
 *
 **/

gboolean
nautilus_icon_container_get_anti_aliased_mode (NautilusIconContainer *container)
{
	GnomeCanvas *canvas;

	canvas = GNOME_CANVAS (container);
	return canvas->aa;
}

/**
 * nautilus_icon_container_set_anti_aliased_mode:
 * Change the anti-aliased mode and redraw everything
 *
 **/

void
nautilus_icon_container_set_anti_aliased_mode (NautilusIconContainer *container, gboolean anti_aliased_mode)
{
	GnomeCanvas *canvas;

	canvas = GNOME_CANVAS (container);
	if (canvas->aa != anti_aliased_mode) {
		canvas->aa = anti_aliased_mode;
		
		invalidate_label_sizes (container);	
		nautilus_icon_container_request_update_all (container);	
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

	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);

	list = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		if (icon->is_selected) {
			list = g_list_prepend (list, icon->data);
		}
	}

	return list;
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
	GList *icons, *node;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), NULL);

	icons = NULL;
	for (node = container->details->icons; node != NULL; node = node->next) {
		NautilusIcon *icon;

		icon = node->data;
		if (icon->is_selected) {
			icons = g_list_prepend (icons, icon);
		}
	}

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
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
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
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
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
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
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
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
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

	g_return_val_if_fail (index > 0, NULL);

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
	int initial_size_x, initial_size_y;
	
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
	
	icon_get_size (container, icon, &initial_size_x, &initial_size_y);

	/* only need to keep size in one dimension, since they are constrained to be the same */
	container->details->stretch_initial_x = icon->x;
	container->details->stretch_initial_y = icon->y;
	container->details->stretch_initial_size = initial_size_x;

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
		if (icon->is_selected && (icon->scale_x != 1.0 || icon->scale_y != 1.0)) {
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
							   1.0, 1.0,
							   FALSE, TRUE);
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
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_ICON_URI],
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
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_ICON_DROP_TARGET_URI],
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

	gtk_signal_emit (GTK_OBJECT (container), signals[LAYOUT_CHANGED]);
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

		gtk_signal_emit (GTK_OBJECT (container), signals[LAYOUT_CHANGED]);
	} else {
		/* in manual layout, label sizes still change, even though
		 * the icons don't move.
		 */
		invalidate_label_sizes (container);	
		nautilus_icon_container_request_update_all (container);	
	}
}


void
nautilus_icon_container_set_layout_mode (NautilusIconContainer *container,
					 NautilusIconLayoutMode mode)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->layout_mode = mode;

	redo_layout (container);

	gtk_signal_emit (GTK_OBJECT (container), signals[LAYOUT_CHANGED]);
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

		position.x = icon->x;
		position.y = icon->y;
		position.scale_x = icon->scale_x;
		position.scale_y = icon->scale_y;
		gtk_signal_emit (GTK_OBJECT (container), signals[ICON_POSITION_CHANGED],
				 icon->data, &position);
	}

	if (changed) {
		gtk_signal_emit (GTK_OBJECT (container), signals[LAYOUT_CHANGED]);
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
		gtk_signal_emit (GTK_OBJECT (container), signals[LAYOUT_CHANGED]);
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
	NautilusIcon *cur_pending;
	
	cur_pending = container->details->pending_icon_to_rename;
	
	if (icon == cur_pending) {
		return;
	}
	
	if (cur_pending != NULL) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (cur_pending->item),
					       &pending_icon_to_rename_destroy_callback,
					       container);
	}
	
	if (icon != NULL) {
		gtk_signal_connect (GTK_OBJECT (icon->item), "destroy", &pending_icon_to_rename_destroy_callback, container);
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
			nautilus_icon_container_start_renaming_selected_item (container);
		} else {
			set_pending_icon_to_rename (container, NULL);
		}
	}
}

gboolean
is_renaming_pending (NautilusIconContainer *container)
{
	return get_pending_icon_to_rename (container) != NULL;
}

gboolean
is_renaming (NautilusIconContainer *container)
{
	return container->details->renaming;
}

/**
 * nautilus_icon_container_start_renaming_selected_item
 * @container: An icon container widget.
 * 
 * Displays the edit name widget on the first selected icon
 **/
void
nautilus_icon_container_start_renaming_selected_item (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	ArtDRect icon_rect;
	const char *editable_text;

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

	/* Create text renaming widget, if it hasn't been created already.
	 * We deal with the broken icon text item widget by keeping it around
	 * so its contents can still be cut and pasted as part of the clipboard
	 */
	if (details->rename_widget == NULL) {
		details->rename_widget = NAUTILUS_ICON_TEXT_ITEM
			(gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (container)),
						nautilus_icon_text_item_get_type (),
						NULL));
	} else {
		gnome_canvas_item_show (GNOME_CANVAS_ITEM (details->rename_widget));
	}
	
	icon_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);
	gnome_canvas_item_w2i (GNOME_CANVAS_ITEM (details->rename_widget), &icon_rect.x0, &icon_rect.y0);
	gnome_canvas_item_w2i (GNOME_CANVAS_ITEM (details->rename_widget), &icon_rect.x1, &icon_rect.y1);
	
	nautilus_icon_text_item_configure
		(details->rename_widget,
		 (icon_rect.x0 + icon_rect.x1) / 2,				/* x_center */
		 icon_rect.y1,							/* y_top */		
		 nautilus_icon_canvas_item_get_max_text_width (icon->item),	/* max_text_width */
		 details->label_font[details->zoom_level],			/* font */
		 editable_text,							/* text */
		 FALSE);							/* allocate local copy */

	nautilus_icon_text_item_start_editing (details->rename_widget);
	
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[RENAMING_ICON],
			 nautilus_icon_text_item_get_renaming_editable (details->rename_widget));

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
		
	if (commit) {						
		/* Verify that text has been modified before signalling change. */			
		changed_text = nautilus_icon_text_item_get_text (container->details->rename_widget);
		if (strcmp (container->details->original_text, changed_text) != 0) {			
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[ICON_TEXT_CHANGED],
					 icon->data,
					 changed_text);
		}
	}
	
	nautilus_icon_text_item_stop_editing (container->details->rename_widget, TRUE);

	gnome_canvas_item_hide (GNOME_CANVAS_ITEM (container->details->rename_widget));

	g_free (container->details->original_text);

	/* We are not in renaming mode */
	container->details->renaming = FALSE;
	nautilus_icon_canvas_item_set_renaming (icon->item, FALSE);		
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
	gtk_signal_emit (GTK_OBJECT (icon_container),
			 signals[PREVIEW],
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
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[GET_STORED_ICON_POSITION],
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
nautilus_icon_container_set_label_font_for_zoom_level (NautilusIconContainer *container,
						       int                    zoom_level,
						       GdkFont               *font)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (font != NULL);
	g_return_if_fail (zoom_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST);
	g_return_if_fail (zoom_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	if (container->details->label_font[zoom_level] != NULL) {
		gdk_font_unref (container->details->label_font[zoom_level]);
	}

	gdk_font_ref (font);
	
	container->details->label_font[zoom_level] = font;
}

void
nautilus_icon_container_set_smooth_label_font (NautilusIconContainer *container,
					       EelScalableFont *font)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (EEL_IS_SCALABLE_FONT (font));

	gtk_object_ref (GTK_OBJECT (font));
	gtk_object_unref (GTK_OBJECT (container->details->smooth_label_font));

	container->details->smooth_label_font = font;
}

void
nautilus_icon_container_set_single_click_mode (NautilusIconContainer *container,
					       gboolean single_click_mode)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	container->details->single_click_mode = single_click_mode;
}


/* update the label color when the background changes */

guint32
nautilus_icon_container_get_label_color (NautilusIconContainer *container,
					 gboolean is_name)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), 0);

	if (is_name) {
		return container->details->label_color;
	} else {
		return container->details->label_info_color;
	}
	
}

static void
update_label_color (EelBackground *background,
		    NautilusIconContainer *container)
{
	char *light_info_color, *dark_info_color;
	uint light_info_value, dark_info_value;
	
	g_assert (EEL_IS_BACKGROUND (background));
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	
	/* FIXME: The code to extract colors from the theme should be in FMDirectoryView, not here.
	 * The NautilusIconContainer class should simply provide calls to set the colors.
	 */
	/* read the info colors from the current theme; use a reasonable default if undefined */
	light_info_color = nautilus_theme_get_theme_data ("directory", "light_info_color");
	if (light_info_color == NULL) {
		light_info_value = 0xAAAAFD;
	} else {
		light_info_value = strtoul (light_info_color, NULL, 0);
		g_free (light_info_color);
	}
	
	dark_info_color = nautilus_theme_get_theme_data ("directory", "dark_info_color");
	if (dark_info_color == NULL) {
		dark_info_value = 0x33337F;
	} else {
		dark_info_value = strtoul (dark_info_color, NULL, 0);
		g_free (dark_info_color);
	}
	
	if (eel_background_is_dark (background)) {
		container->details->label_color = 0xEFEFEF;
		container->details->label_info_color = light_info_value;
	} else {
		container->details->label_color = 0x000000;
		container->details->label_info_color = dark_info_value;
	}
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

/* handle theme changes */

/* FIXME: The code to extract colors from the theme should be in FMDirectoryView, not here.
 * The NautilusIconContainer class should simply provide calls to set the colors.
 */
static void
nautilus_icon_container_theme_changed (gpointer user_data)
{
	NautilusIconContainer *container;
	char *text_frame_path, *highlight_color_str;

	container = NAUTILUS_ICON_CONTAINER (user_data);
	
	/* load the highlight frame */		
	text_frame_path = nautilus_theme_get_image_path ("text-selection-frame-aa.png");
	if (container->details->highlight_frame) {
		gdk_pixbuf_unref (container->details->highlight_frame);
	}
	
	container->details->highlight_frame = gdk_pixbuf_new_from_file (text_frame_path);
	g_free (text_frame_path);

	/* load the highlight color */	
	highlight_color_str = nautilus_theme_get_theme_data ("directory", "highlight_color_rgba");
	
	if (highlight_color_str == NULL) {
		container->details->highlight_color = EEL_RGBA_COLOR_PACK (0, 0, 0, 102);
	} else {
		container->details->highlight_color = strtoul (highlight_color_str, NULL, 0);
		g_free (highlight_color_str);
	}
}

void
nautilus_icon_container_set_font_size_table (NautilusIconContainer *container,
					     const int font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1])
{
	int i;
	
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (font_size_table != NULL);
	
	for (i = 0; i <= NAUTILUS_ZOOM_LEVEL_LARGEST; i++) {
		container->details->font_size_table[i] = font_size_table[i];
	}
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
	EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 12, 0, 0, 0, 0), "0,0:12");
	EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 12, 12, 12, 13, 13), "0,0:13");
	EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 12, 12, 12, 13, 12), "0,0:12");
	EEL_CHECK_STRING_RESULT (check_compute_stretch (100, 100, 64, 105, 105, 40, 40), "35,35:129");
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
