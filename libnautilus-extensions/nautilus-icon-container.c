/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-container.c - Icon container widget.

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

#include <config.h>
#include "nautilus-icon-container.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-gnome-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-icon-text-item.h"
#include "nautilus-font-factory.h"
#include "nautilus-lib-self-check-functions.h"


#include "nautilus-icon-grid.h"
#include "nautilus-icon-private.h"

/* Interval for updating the rubberband selection, in milliseconds.  */
#define RUBBERBAND_TIMEOUT_INTERVAL 10

/* Timeout for making the icon currently selected for keyboard operation visible. */
/* FIXME bugzilla.eazel.com 611: This *must* be higher than the double-click 
 * time in GDK, but there is no way to access its value from outside.
 */
#define KEYBOARD_ICON_REVEAL_TIMEOUT 300

/* Maximum amount of milliseconds the mouse button is allowed to stay down
 * and still be considered a click.
 */
#define MAX_CLICK_TIME 1500

/* Distance you have to move before it becomes a drag. */
#define SNAP_RESISTANCE 2 /* GMC has this set to 3, but it's too much for my (Ettore's?) taste. */

/* Button assignments. */
#define DRAG_BUTTON 1
#define RUBBERBAND_BUTTON 1
#define CONTEXTUAL_MENU_BUTTON 3

/* Maximum size (pixels) allowed for icons at the standard zoom level. */
#define MAXIMUM_IMAGE_SIZE 96
#define MAXIMUM_EMBLEM_SIZE 48

static void          activate_selected_items                  (NautilusIconContainer      *container);
static void          nautilus_icon_container_initialize_class (NautilusIconContainerClass *class);
static void          nautilus_icon_container_initialize       (NautilusIconContainer      *container);
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
static void          hide_rename_widget                       (NautilusIconContainer      *container,
							       NautilusIcon               *icon);
static void	     anti_aliased_preferences_changed	      (gpointer 		   user_data);
static void          click_policy_changed_callback            (gpointer                    user_data);

static void 	     remember_selected_files		      (NautilusIconContainer 	*container);
static void 	     forget_selected_files		      (NautilusIconContainer 	*container);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIconContainer, nautilus_icon_container, GNOME_TYPE_CANVAS)



/* The NautilusIconContainer signals.  */
enum {
	ACTIVATE,
	BUTTON_PRESS,
	CAN_ACCEPT_ITEM,
	COMPARE_ICONS,
	CONTEXT_CLICK_BACKGROUND,
	CONTEXT_CLICK_SELECTION,
	GET_CONTAINER_URI,
	GET_ICON_IMAGES,
	GET_ICON_TEXT,
	GET_ICON_URI,
	GET_STORED_ICON_POSITION,
	ICON_POSITION_CHANGED,
	ICON_TEXT_CHANGED,
	LAYOUT_CHANGED,
	MOVE_COPY_ITEMS,
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
	gtk_object_destroy (GTK_OBJECT (icon->item));
	g_free (icon);
}

static void
icon_set_position (NautilusIcon *icon,
		   double x, double y)
{
	if (icon->x == x && icon->y == y) {
		return;
	}

	gnome_canvas_item_move (GNOME_CANVAS_ITEM (icon->item),
				x - icon->x,
				y - icon->y);

	icon->x = x;
	icon->y = y;
}

/* icon_get_size and icon_set_size are used by the stretching user interface,
 * which currently stretches in a way that keeps the aspect ratio. Later we
 * might have a stretching interface that stretches Y separate from X and
 * we will change this around.
 */

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

static void
icon_set_size (NautilusIconContainer *container,
	       NautilusIcon *icon,
	       guint icon_size)
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
					FALSE);
}

static void
icon_raise (NautilusIcon *icon)
{
	gnome_canvas_item_raise_to_top (GNOME_CANVAS_ITEM (icon->item));
}

static void
icon_show (NautilusIcon *icon)
{
	gnome_canvas_item_show (GNOME_CANVAS_ITEM (icon->item));
}

static void
icon_toggle_selected (NautilusIconContainer *container,
		      NautilusIcon *icon)
{		
	end_renaming_mode(container, TRUE);

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

static void
scroll (NautilusIconContainer *container,
	int delta_x, int delta_y)
{
	GtkAdjustment *hadj, *vadj;

	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	gtk_adjustment_set_value (hadj, hadj->value + delta_x);
	gtk_adjustment_set_value (vadj, vadj->value + delta_y);
}

static void
reveal_icon (NautilusIconContainer *container,
	     NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;
	int x1, y1, x2, y2;

	details = container->details;
	allocation = &GTK_WIDGET (container)->allocation;

	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);

	if (y1 < vadj->value) {
		gtk_adjustment_set_value (vadj, y1);
	} else if (y2 > vadj->value + allocation->height) {
		gtk_adjustment_set_value (vadj, y2 - allocation->height);
	}

	if (x1 < hadj->value) {
		gtk_adjustment_set_value (hadj, x1);
	} else if (x2 > hadj->value + allocation->width) {
		gtk_adjustment_set_value (hadj, x2 - allocation->width);
	}
}

static gboolean
keyboard_icon_reveal_timeout_callback (gpointer data)
{
	NautilusIconContainer *container;
	NautilusIcon *icon;

	GDK_THREADS_ENTER ();

	container = NAUTILUS_ICON_CONTAINER (data);
	icon = container->details->keyboard_icon_to_reveal;

	g_assert (icon != NULL);

	/* Only reveal the icon if it's still the keyboard focus
	 * or if it's still selected.
	 */
	/* FIXME bugzilla.eazel.com 612: 
	 * Need to unschedule this if the user scrolls explicitly.
	 */
	if (icon == container->details->keyboard_focus
	    || icon->is_selected) {
		reveal_icon (container, icon);
	}
	container->details->keyboard_icon_reveal_timer_id = 0;

	GDK_THREADS_LEAVE ();

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

/* Set `icon' as the icon currently selected for keyboard operations. */
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



/* Idle operation handler.  */

static void
nautilus_gnome_canvas_item_request_update_deep (GnomeCanvasItem *item)
{
	GList *p;

	gnome_canvas_item_request_update (item);
	if (GNOME_IS_CANVAS_GROUP (item)) {
		for (p = GNOME_CANVAS_GROUP (item)->item_list; p != NULL; p = p->next) {
			nautilus_gnome_canvas_item_request_update_deep (p->data);
		}
	}
}

static void
nautilus_gnome_canvas_request_update_all (GnomeCanvas *canvas)
{
	nautilus_gnome_canvas_item_request_update_deep (canvas->root);
}

/* gnome_canvas_set_scroll_region doesn't do an update, even though it should.
 * The update is in there with an #if 0 around it, with no explanation of
 * why it's commented out. For now, work around this by requesting an update
 * explicitly.
 */
static void
nautilus_gnome_canvas_set_scroll_region (GnomeCanvas *canvas,
					 double x1, double y1,
					 double x2, double y2)
{
	double old_x1, old_y1, old_x2, old_y2;

	gnome_canvas_get_scroll_region (canvas, &old_x1, &old_y1, &old_x2, &old_y2);
	if (old_x1 == x1 && old_y1 == y1 && old_x2 == x2 && old_y2 == y2) {
		return;
	}

	gnome_canvas_set_scroll_region (canvas, x1, y1, x2, y2);
	nautilus_gnome_canvas_request_update_all (canvas);
	gnome_canvas_item_request_update (canvas->root);
}

static void
set_scroll_region (NautilusIconContainer *container)
{
	double x1, y1, x2, y2;
        double content_width, content_height;
	double scroll_width, scroll_height;
	int step_increment;
	GtkAllocation *allocation;
	GtkAdjustment *vadj, *hadj;

	gnome_canvas_item_get_bounds (GNOME_CANVAS (container)->root,
				      &x1, &y1, &x2, &y2);

	content_width = x2 - x1;
	content_height = y2 - y1;

	allocation = &GTK_WIDGET (container)->allocation;

	scroll_width = MAX (content_width, allocation->width);
	scroll_height = MAX (content_height, allocation->height);

	/* FIXME bugzilla.eazel.com 622: Why are we subtracting one from each dimension? */
	nautilus_gnome_canvas_set_scroll_region
		(GNOME_CANVAS (container),
		 x1, y1,
		 x1 + scroll_width - 1,
		 y1 + scroll_height - 1);

	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	if (content_width <= allocation->width) {
		gtk_adjustment_set_value (hadj, x1);
	}
	if (content_height <= allocation->height) {
		gtk_adjustment_set_value (vadj, y1);
	}

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
resort_and_clear (NautilusIconContainer *container)
{
	sort_hack_container = container;
	container->details->icons = g_list_sort
		(container->details->icons, compare_icons);

	nautilus_icon_grid_clear (container->details->grid);
}

static void
auto_position_icon (NautilusIconContainer *container,
		    NautilusIcon *icon)
{
	ArtPoint position;

	nautilus_icon_grid_get_position (container->details->grid,
					 icon, &position);
	icon_set_position (icon, position.x, position.y);
}

static void
relayout (NautilusIconContainer *container)
{
	GList *p;
	NautilusIcon *icon;

	if (!container->details->auto_layout) {
		return;
	}

	resort_and_clear (container);

	/* An icon currently being stretched must be left in place.
	 * That's "drag_icon". This doesn't come up for cases where
	 * we are doing other kinds of drags, but if it did, the
	 * same logic would probably apply.
	 */

	/* Place the icon that must stay put first. */
	if (container->details->drag_icon != NULL) {
		nautilus_icon_grid_add (container->details->grid,
					container->details->drag_icon);
	}

	/* Place all the other icons. */
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		if (icon != container->details->drag_icon) {
			auto_position_icon (container, icon);
			nautilus_icon_grid_add (container->details->grid, icon);
		}
	}
}

static void
reload_icon_positions (NautilusIconContainer *container)
{
	GList *p, *no_position_icons;
	NautilusIcon *icon;
	gboolean have_stored_position;
	NautilusIconPosition position;

	g_assert (!container->details->auto_layout);

	resort_and_clear (container);

	no_position_icons = NULL;

	/* Place all the icons with positions. */
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
			nautilus_icon_grid_add (container->details->grid, icon);
		} else {
			no_position_icons = g_list_prepend (no_position_icons, icon);
		}
	}
	no_position_icons = g_list_reverse (no_position_icons);

	/* Place all the other icons. */
	for (p = no_position_icons; p != NULL; p = p->next) {
		icon = p->data;

		auto_position_icon (container, icon);
		nautilus_icon_grid_add (container->details->grid, icon);
	}

	g_list_free (no_position_icons);
}

static gboolean
idle_handler (gpointer data)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;

	GDK_THREADS_ENTER ();

	container = NAUTILUS_ICON_CONTAINER (data);
	details = container->details;

	set_scroll_region (container);
	relayout (container);

	details->idle_id = 0;

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static void
request_idle (NautilusIconContainer *container)
{
	if (container->details->idle_id != 0) {
		return;
	}

	container->details->idle_id = gtk_idle_add (idle_handler, container);
}


/* Container-level icon handling functions.  */

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static gboolean
select_one_unselect_others (NautilusIconContainer *container,
			    NautilusIcon *icon_to_select)
{
	GList *p;
	gboolean selection_changed;

	selection_changed = FALSE;
	
	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

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
				   gboolean raise)
{
	NautilusIconContainerDetails *details;
	gboolean emit_signal;
	NautilusIconPosition position;
	
	details = container->details;
	
	emit_signal = FALSE;
	
	if (!details->auto_layout) {
		if (x != icon->x || y != icon->y) {
			icon_set_position (icon, x, y);
			emit_signal = TRUE;
		}
	}
	
	if (scale_x != icon->scale_x || scale_y != icon->scale_y) {
		icon->scale_x = scale_x;
		icon->scale_y = scale_y;
		nautilus_icon_container_update_icon (container, icon);
		relayout (container);
		emit_signal = TRUE;
	}
	
	if (emit_signal) {
		position.x = x;
		position.y = y;
		position.scale_x = scale_x;
		position.scale_y = scale_y;
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[ICON_POSITION_CHANGED],
				 icon->data, &position);
	}
	
	if (raise) {
		icon_raise (icon);
	}
}


/* Implementation of rubberband selection.  */

static void
rubberband_select (NautilusIconContainer *container,
		   const ArtDRect *previous_rect,
		   const ArtDRect *current_rect)
{
	ArtDRect both_rects;
	GList *icons, *p;
	gboolean selection_changed;
	NautilusIcon *icon;
	gboolean is_in;
		
	selection_changed = FALSE;

	/* As an optimization, ask the grid which icons intersect the rectangles. */
	art_drect_union (&both_rects, previous_rect, current_rect);
	icons = nautilus_icon_grid_get_intersecting_icons
		(container->details->grid, &both_rects);
	
	for (p = icons; p != NULL; p = p->next) {
		icon = p->data;
		
		is_in = nautilus_icon_canvas_item_hit_test_rectangle
			(icon->item, current_rect);

		g_assert (icon->was_selected_before_rubberband == FALSE
			  || icon->was_selected_before_rubberband == TRUE);
		selection_changed |= icon_set_selected
			(container, icon,
			 is_in ^ icon->was_selected_before_rubberband);
	}

	g_list_free (icons);

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

	GDK_THREADS_ENTER ();

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
	    && band_info->prev_x == x && band_info->prev_y == y) {
		GDK_THREADS_LEAVE ();
		return TRUE;
	}

	scroll (container, x_scroll, y_scroll);

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

	band_info->prev_x = x;
	band_info->prev_y = y;

	band_info->prev_rect = selection_rect;

	GDK_THREADS_LEAVE ();

	return TRUE;
}

static void
start_rubberbanding (NautilusIconContainer *container,
		     GdkEventButton *event)
{
	NautilusIconContainerDetails *details;
	NautilusIconRubberbandInfo *band_info;
	GList *p;

	details = container->details;
	band_info = &details->rubberband_info;

	for (p = details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		icon->was_selected_before_rubberband = icon->is_selected;
	}

	gnome_canvas_window_to_world
		(GNOME_CANVAS (container),
		 event->x, event->y,
		 &band_info->start_x, &band_info->start_y);

	if (GNOME_CANVAS(container)->aa) {
		band_info->selection_rectangle = gnome_canvas_item_new
			(gnome_canvas_root
		 	(GNOME_CANVAS (container)),
		 	gnome_canvas_rect_get_type (),
		 	"x1", band_info->start_x,
		 	"y1", band_info->start_y,
		 	"x2", band_info->start_x,
		 	"y2", band_info->start_y,
		 	"fill_color_rgba", 0x77bbdd40,
		 	"outline_color_rgba", 0x77bbddFF,
		 	"width_pixels", 1,
		 	NULL);
	
	} else {
		band_info->selection_rectangle = gnome_canvas_item_new
			(gnome_canvas_root
		 	(GNOME_CANVAS (container)),
		 	gnome_canvas_rect_get_type (),
		 	"x1", band_info->start_x,
		 	"y1", band_info->start_y,
		 	"x2", band_info->start_x,
		 	"y2", band_info->start_y,
		 	"fill_color", "lightblue",
		 	"fill_stipple", stipple,
		 	"outline_color", "lightblue",
		 	"width_pixels", 1,
		 	NULL);
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

	gnome_canvas_item_ungrab (band_info->selection_rectangle, event->time);
	gtk_object_destroy (GTK_OBJECT (band_info->selection_rectangle));
	band_info->selection_rectangle = NULL;
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
		return 1;
	}
	if (icon_a->y < icon_b->y) {
		return -1;
	}
	if (icon_a->y > icon_b->y) {
		return 1;
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
		return 1;
	}
	if (icon_a->x < icon_b->x) {
		return -1;
	}
	if (icon_a->x > icon_b->x) {
		return 1;
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

	nautilus_gnome_canvas_item_get_current_canvas_bounds (GNOME_CANVAS_ITEM (icon->item),
							      &bounds);
	if (container->details->arrow_key_start < bounds.y0) {
		return -1;
	}
	if (container->details->arrow_key_start > bounds.y1) {
		return 1;
	}
	return 0;
}

static int
compare_with_start_column (NautilusIconContainer *container,
			   NautilusIcon *icon)
{
	ArtIRect bounds;
	
	nautilus_gnome_canvas_item_get_current_canvas_bounds (GNOME_CANVAS_ITEM (icon->item),
							      &bounds);
	if (container->details->arrow_key_start < bounds.x0) {
		return -1;
	}
	if (container->details->arrow_key_start > bounds.x1) {
		return 1;
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
		/* Move the keyboard focus. */
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

	nautilus_icon_canvas_item_get_icon_rectangle
		(icon->item, &world_rect);
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

	match_state = (BestNameMatch *)data;

	name = nautilus_icon_canvas_item_get_editable_text (candidate->item);	

	for (match_length = 0; ; match_length++) {
		if (name[match_length] == '\0'
			|| match_state->name[match_length] == '\0') {
			break;
		}

		/* expect the matched pattern to already be lowercase */
		g_assert (tolower (match_state->name[match_length]) 
			== match_state->name[match_length]);
			
		if (tolower (name[match_length]) != match_state->name[match_length])
			break;
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
		match_state.name[index] = tolower (match_state.name[index]);
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


static int
compare_icons_by_name (gconstpointer a, gconstpointer b)
{
	const NautilusIcon *icon_a, *icon_b;
	icon_a = a;
	icon_b = b;

	return g_strcasecmp (nautilus_icon_canvas_item_get_editable_text (icon_a->item),
		nautilus_icon_canvas_item_get_editable_text (icon_b->item));
}

static GList *
build_sorted_icon_list (NautilusIconContainer *container)
{
	if (container->details->icons == NULL) {
		return NULL;
	}

	return g_list_sort (nautilus_g_list_copy (container->details->icons), 
		compare_icons_by_name);
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

	list = build_sorted_icon_list (container);

	if (icon != NULL) {
		/* must have at least @icon in the list */
		g_assert (list != NULL);
		item = g_list_find (list, icon);
		g_assert (item != NULL);
		
		item = next ? item->next : item->prev;
	} else if (list != NULL) {
		/* no selection yet, pick the first or last item to select */
		g_assert (g_list_first (list) != NULL);
		g_assert (g_list_last (list) != NULL);
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
	int i;

	container = NAUTILUS_ICON_CONTAINER (object);
	forget_selected_files(container);

	nautilus_icon_dnd_fini (container);
        nautilus_icon_container_clear (container);

	nautilus_icon_grid_destroy (container->details->grid);
	unschedule_keyboard_icon_reveal (container);
	
	if (container->details->rubberband_info.timer_id != 0) {
		gtk_timeout_remove (container->details->rubberband_info.timer_id);
	}
	if (container->details->rubberband_info.selection_rectangle != NULL) {
		gtk_object_destroy (GTK_OBJECT (container->details->rubberband_info.selection_rectangle));
	}

        if (container->details->idle_id != 0) {
		gtk_idle_remove (container->details->idle_id);
	}
        for (i = 0; i < NAUTILUS_N_ELEMENTS (container->details->label_font); i++) {
       		if (container->details->label_font[i] != NULL)
        		gdk_font_unref (container->details->label_font[i]);
	}

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					      click_policy_changed_callback,
					      container);
	
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_ANTI_ALIASED_CANVAS,
					      anti_aliased_preferences_changed,
					      container);
	
	nautilus_icon_container_flush_typeselect_state (container);
	
	g_free (container->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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
world_width_changed (NautilusIconContainer *container, int new_width)
{
	NautilusIconGrid *grid;
	double world_width;

	grid = container->details->grid;

	gnome_canvas_c2w (GNOME_CANVAS (container),
			  new_width, 0,
			  &world_width, NULL);

	nautilus_icon_grid_set_visible_width (grid, world_width);

	relayout (container);
	set_scroll_region (container);
}

static void
size_allocate (GtkWidget *widget,
	       GtkAllocation *allocation)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	world_width_changed (NAUTILUS_ICON_CONTAINER (widget), widget->allocation.width);
}

static void
realize (GtkWidget *widget)
{
	GtkStyle *style;
	GtkWindow *window;

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

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

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, unrealize, (widget));
}

static gboolean
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	NautilusIconContainer *container;
	gboolean selection_changed;
	gboolean return_value;

	container = NAUTILUS_ICON_CONTAINER (widget);
        container->details->button_down_time = event->time;
	
        /* Forget about the old keyboard selection now that we've started mousing. */
        clear_keyboard_focus (container);

	/* Forget about where we began with the arrow keys now that we're mousing. */
	container->details->arrow_key_axis = AXIS_NONE;
	
	/* Forget the typeahead state. */
	nautilus_icon_container_flush_typeselect_state (container);

	/* Invoke the canvas event handler and see if an item picks up the event. */
	if (NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_press_event, (widget, event))) {
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
	
	/* Button 3 does a contextual menu. */
	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		gtk_signal_emit (GTK_OBJECT (widget),
				 signals[CONTEXT_CLICK_BACKGROUND]);
		return TRUE;
	}
	
	/* Otherwise, we emit a button_press message. */
	gtk_signal_emit (GTK_OBJECT (widget),
			 signals[BUTTON_PRESS], event,
			 &return_value);
	return return_value;
}

/* deallocate the list of selected files */
static void
forget_selected_files (NautilusIconContainer *container)
{
	if (container->details->last_selected_files) {
		g_list_free(container->details->last_selected_files);
		container->details->last_selected_files = NULL;	
	}
}

/* remember the selected files in a list for later access */
static void
remember_selected_files (NautilusIconContainer *container)
{
	NautilusIcon *icon;
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	forget_selected_files(container);
	for (p = container->details->icons; p != NULL; p = p->next) {  	
	  	icon = p->data;
		if (icon->is_selected) {
			container->details->last_selected_files = g_list_prepend
				(container->details->last_selected_files, icon->data);
		}
	}	
}

static void
nautilus_icon_container_almost_drag (NautilusIconContainer *container,
				     GdkEventButton *event)
{
	NautilusIconContainerDetails *details;
	details = container->details;

	/* build list of selected icons before we blow away the selection */	
	
	if (event->type != GDK_2BUTTON_PRESS)
		remember_selected_files(container);
 	
	if (!button_event_modifies_selection (event)) {
		gboolean selection_changed;
		
		selection_changed
			= select_one_unselect_others (container,
						      details->drag_icon);
		
		if (selection_changed) {
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
		}
	}
	
	if (details->drag_icon != NULL) {		
		/* If single-click mode, activate the icon, unless modifying
		 * the selection or pressing for a very long time.
		 */
		if (details->single_click_mode
		    && event->time - details->button_down_time < MAX_CLICK_TIME
		    && ! button_event_modifies_selection (event)) {
			
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[ACTIVATE],
					 container->details->last_selected_files);
			forget_selected_files(container);
		}
	}
}

static gboolean
start_stretching (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	ArtPoint world_point;

	details = container->details;
	icon = details->stretch_icon;
	
	/* Check if we hit the stretch handles. */
	world_point.x = details->drag_x;
	world_point.y = details->drag_y;
	if (!nautilus_icon_canvas_item_hit_test_stretch_handles
	    (icon->item, &world_point)) {
		return FALSE;
	}

	/* Set up the dragging. */
	details->drag_action = DRAG_ACTION_STRETCH;
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

	return TRUE;
}

static void
continue_stretching (NautilusIconContainer *container,
		     int window_x, int window_y)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *icon;
	double world_x, world_y;
	StretchState stretch_state;

	details = container->details;
	icon = details->stretch_icon;

	if (icon == NULL) {
		return;
	}

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      window_x, window_y,
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
	icon_set_size (container, icon, stretch_state.icon_size);
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
	continue_stretching (container, window_x, window_y);
	ungrab_stretch_icon (container);

	/* We must do a relayout after indicating we are done stretching. */
	container->details->drag_icon = NULL;
	relayout (container);
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

		switch (details->drag_action) {
		case DRAG_ACTION_MOVE_OR_COPY:
			if (!details->drag_started) {
				nautilus_icon_container_almost_drag (container, event);
			} else {
				nautilus_icon_dnd_end_drag (container);
			}
			break;
		case DRAG_ACTION_STRETCH:
			end_stretching (container, event->x, event->y);
			break;
		}

		details->drag_icon = NULL;
		return TRUE;
	}

	return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_release_event, (widget, event));
}

static int
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *motion)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;
	double world_x, world_y;

	container = NAUTILUS_ICON_CONTAINER (widget);
	details = container->details;

	if (details->drag_button != 0) {
		switch (details->drag_action) {
		case DRAG_ACTION_MOVE_OR_COPY:
			if (details->drag_started) {
				break;
			}

			gnome_canvas_window_to_world
				(GNOME_CANVAS (container),
				 motion->x, motion->y,
				 &world_x, &world_y);
			
			if (abs (details->drag_x - world_x) >= SNAP_RESISTANCE
			    || abs (details->drag_y - world_y) >= SNAP_RESISTANCE) {
				details->drag_started = TRUE;
				
				/* KLUDGE ALERT: Poke the starting values into the motion
				 * structure so that dragging behaves as expected.
				 */
				motion->x = details->drag_x;
				motion->y = details->drag_y;
			
				nautilus_icon_dnd_begin_drag (container,
							      GDK_ACTION_MOVE,
							      details->drag_button,
							      motion);
			}
			break;
		case DRAG_ACTION_STRETCH:
			continue_stretching (container, motion->x, motion->y);
			break;
		}
	}

	return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, motion_notify_event, (widget, motion));
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

enum {
	NAUTILUS_TYPESELECT_FLUSH_DELAY = 1000000
	/* After this time the current typeselect buffer will be
	 * thrown away and the new pressed character will be made
	 * the the start of a new pattern.
	 */
};

static gboolean
nautilus_icon_container_handle_typeahead (NautilusIconContainer *container, const char *key_string)
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
		if (!isprint (key_string[index])) {
			return FALSE;
		}
	}

	/* lazily allocate the typeahead state */
	if (container->details->type_select_state == NULL) {
		container->details->type_select_state = g_new0 (TypeSelectState, 1);
	}

	/* find out how long since last character was typed */
	now = nautilus_get_system_time();
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

	/* allow the drag state update the drag action if modifiers changed */
	nautilus_icon_dnd_update_drop_action (widget);

	if (nautilus_icon_container_is_renaming (container)) {
		switch (event->keyval) {
		case GDK_Return:
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
			activate_selected_items (container);
			handled = TRUE;
			break;
		default:
			handled = nautilus_icon_container_handle_typeahead (container, 
									    event->string);
			flush_typeahead = !handled;
			break;
		}
	}

	if (flush_typeahead) {
		/* any non-ascii key will force the typeahead state to be forgotten */
		nautilus_icon_container_flush_typeselect_state (container);
	}
	
	if (!handled) {
		handled = NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, key_press_event, (widget, event));
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
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	signals[CONTEXT_CLICK_BACKGROUND]
		= gtk_signal_new ("context_click_background",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     context_click_background),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
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
	signals[GET_ICON_IMAGES]
		= gtk_signal_new ("get_icon_images",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_images),
				  nautilus_gtk_marshal_POINTER__POINTER_STRING_POINTER,
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
				  nautilus_gtk_marshal_NONE__POINTER_STRING_STRING,
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
				  nautilus_gtk_marshal_STRING__POINTER,
				  GTK_TYPE_STRING, 1,
				  GTK_TYPE_POINTER);
	signals[COMPARE_ICONS]
		= gtk_signal_new ("compare_icons",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     compare_icons),
				  nautilus_gtk_marshal_INT__POINTER_POINTER,
				  GTK_TYPE_INT, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER);
	signals[MOVE_COPY_ITEMS] 
		= gtk_signal_new ("move_copy_items",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass, 
						     move_copy_items),
				  nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER_INT_INT_INT,
				  GTK_TYPE_NONE, 6,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER,
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
				  nautilus_gtk_marshal_STRING__NONE,
				  GTK_TYPE_STRING, 0);
	signals[CAN_ACCEPT_ITEM] 
		= gtk_signal_new ("can_accept_item",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass, 
						     can_accept_item),
				  nautilus_gtk_marshal_INT__POINTER_STRING,
				  GTK_TYPE_INT, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_STRING);
	signals[GET_STORED_ICON_POSITION]
		= gtk_signal_new ("get_stored_icon_position",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_stored_icon_position),
				  nautilus_gtk_marshal_BOOL__POINTER_POINTER,
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
				  nautilus_gtk_marshal_INT__POINTER_INT,
				  GTK_TYPE_INT, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_BOOL);
	
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
}

/* Handler for the editing_started signal of an icon text item.  We block the
 * event handler so that it will not be called while the text is being edited.
 */
 
static void
editing_started (NautilusIconTextItem *text_item, gpointer data)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;

	container = (NautilusIconContainer *)data;
	details = container->details;
}

/* Handler for the editing_stopped signal of an icon text item.  We unblock the
 * event handler so that we can get events from it again.
 */
static void
editing_stopped (NautilusIconTextItem *text_item, gpointer data)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;

	container = (NautilusIconContainer *)data;
	details = container->details;
}


static void
nautilus_icon_container_initialize (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	int mode;

	details = g_new0 (NautilusIconContainerDetails, 1);

	details->grid = nautilus_icon_grid_new ();

        details->zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;
 
 	/* font table - this isn't exactly proportional, but it looks better than computed */
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALLEST] = nautilus_font_factory_get_font_from_preferences (8);
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALLER] = nautilus_font_factory_get_font_from_preferences (8);
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALL] = nautilus_font_factory_get_font_from_preferences (10);
        details->label_font[NAUTILUS_ZOOM_LEVEL_STANDARD] = nautilus_font_factory_get_font_from_preferences (12);
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGE] = nautilus_font_factory_get_font_from_preferences (14);
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGER] = nautilus_font_factory_get_font_from_preferences (18);
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGEST] = nautilus_font_factory_get_font_from_preferences (18);

	container->details = details;

	/* Set up DnD.  */
	nautilus_icon_dnd_init (container, stipple);

	/* Request update.  */
	request_idle (container);

	/* Make sure that we find out if the theme changes. */
	gtk_signal_connect_object_while_alive
		(nautilus_icon_factory_get (),
		 "icons_changed",
		 nautilus_icon_container_request_update_all,
		 GTK_OBJECT (container));	

	container->details->rename_widget = NULL;
	container->details->original_text = NULL;
	container->details->type_select_state = NULL;

	/* Initialize the single click mode from preferences */
	mode = nautilus_preferences_get_enum
		(NAUTILUS_PREFERENCES_CLICK_POLICY,
		 NAUTILUS_CLICK_POLICY_SINGLE);
	details->single_click_mode =
		mode == NAUTILUS_CLICK_POLICY_SINGLE;

	/* Keep track of changes in clicking policy */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					   click_policy_changed_callback,
					   container);
	
	/* Keep track of changes in graphics trade offs */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_ANTI_ALIASED_CANVAS, 
					   (NautilusPreferencesCallback) anti_aliased_preferences_changed, 
					   container);
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

	if (event->button == DRAG_BUTTON) {
		details->drag_button = event->button;
		details->drag_icon = icon;
		details->drag_x = event->x;
		details->drag_y = event->y;
		details->drag_action = DRAG_ACTION_MOVE_OR_COPY;
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

	/* build list of selected icons before we blow away the selection */
	if ((event->type != GDK_2BUTTON_PRESS) && button_event_modifies_selection (event))
		remember_selected_files(container);
 	
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

	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		/* Note: this means you cannot drag with right click.
		 * If we decide we want right drags, we will have to
		 * set up a timeout and emit this signal if the
                 *  timeout expires without movement.
		 */

		details->drag_button = 0;
		details->drag_icon = NULL;

		/* Context menu applies to all selected items. The only
		 * odd case is if this click deselected the icon under
		 * the mouse, but at least the behavior is consistent.
		 */
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[CONTEXT_CLICK_SELECTION]);

	} else if (event->type == GDK_2BUTTON_PRESS) {
		/* Double clicking does not trigger a D&D action. */
		details->drag_button = 0;
		details->drag_icon = NULL;

		gtk_signal_emit (GTK_OBJECT (container),
				 signals[ACTIVATE],
				 container->details->last_selected_files);
		forget_selected_files(container);
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
	GnomeCanvas *canvas;
	GtkWidget *new;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	new = gtk_type_new (nautilus_icon_container_get_type ());
	
	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	canvas = GNOME_CANVAS(new);
	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_ANTI_ALIASED_CANVAS, FALSE))
		canvas->aa = TRUE;
	
	return new;
}

/* clear all of the icons in the container */

void
nautilus_icon_container_clear (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	details = container->details;

	end_renaming_mode(container, TRUE);
	
	clear_keyboard_focus (container);
	details->stretch_icon = NULL;

	for (p = details->icons; p != NULL; p = p->next) {
		icon_free (p->data);
	}
	g_list_free (details->icons);
	details->icons = NULL;

	nautilus_icon_grid_clear (details->grid);
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

	nautilus_icon_grid_remove (details->grid, icon);
	
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

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	remember_selected_files(container);
	if (container->details->last_selected_files != NULL) {
	  	gtk_signal_emit (GTK_OBJECT (container),
				signals[ACTIVATE],
				container->details->last_selected_files);

		forget_selected_files(container);
	}
}

static void
bounds_changed_callback (NautilusIconCanvasItem *item,
			 NautilusIconContainer *container)
{
	NautilusIcon *icon;

	g_assert (NAUTILUS_IS_ICON_CANVAS_ITEM (item));
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));

	icon = item->user_data;
	g_assert (icon != NULL);

	nautilus_icon_grid_remove (container->details->grid, icon);
	nautilus_icon_grid_add (container->details->grid, icon);
}

void 
nautilus_icon_container_update_icon (NautilusIconContainer *container,
				     NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;
	guint icon_size_x, icon_size_y, max_image_size;
	NautilusScalableIcon *scalable_icon;
	GdkPixbuf *pixbuf, *emblem_pixbuf;
	GList *emblem_scalable_icons, *emblem_pixbufs, *p;
	char *editable_text, *additional_text;
	GdkFont *font;

	if (icon == NULL) {
		return;
	}

	/* Close any open edit. */
	/* FIXME bugzilla.eazel.com 913: Why must we do this? This
	 * function is called if there's any change. Probably we only
	 * want to do this for certain kinds of changes and even then
	 * it seems rude to discard the user's text instead of
	 * prompting.
	 */
	end_renaming_mode (container, TRUE);

	details = container->details;

	/* Get the icons. */
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_ICON_IMAGES],
			 icon->data,
			 (icon == details->drop_target) ? "accept" : "",
			 &emblem_scalable_icons,
			 &scalable_icon);

	/* compute the maximum size based on the scale factor */
	max_image_size = MAXIMUM_IMAGE_SIZE * GNOME_CANVAS(container)->pixels_per_unit;
	
	/* Get the appropriate images for the file. */
	icon_get_size (container, icon, &icon_size_x, &icon_size_y);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(scalable_icon,
		 icon_size_x, icon_size_y,
		 max_image_size, max_image_size);
	nautilus_scalable_icon_unref (scalable_icon);
	emblem_pixbufs = NULL;
	for (p = emblem_scalable_icons; p != NULL; p = p->next) {
		emblem_pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data,
			 icon_size_x, icon_size_y,
			 MAXIMUM_EMBLEM_SIZE, MAXIMUM_EMBLEM_SIZE);
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

	font = details->label_font[details->zoom_level];
        
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->item),
			       "editable_text", editable_text,
			       "additional_text", additional_text,
			       "font", font,
			       "highlighted_for_drop", icon == details->drop_target,
			       NULL);
	
	nautilus_icon_canvas_item_set_image (icon->item, pixbuf);
	nautilus_icon_canvas_item_set_emblems (icon->item, emblem_pixbufs);

	/* Let the pixbufs go. */
	gdk_pixbuf_unref (pixbuf);
	nautilus_gdk_pixbuf_list_free (emblem_pixbufs);

	g_free (editable_text);
	g_free (additional_text);
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
	GList *p;
	NautilusIcon *icon;
	gboolean have_stored_position;
	NautilusIconPosition position;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	details = container->details;

	/* FIXME bugzilla.eazel.com 1288: 
	 * I guess we need to use an indexed data structure to avoid this loop.
	 */
	for (p = details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->data == data) {
			return FALSE;
		}
	}

	have_stored_position = FALSE;
	position.scale_x = 1.0;
	position.scale_y = 1.0;
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_STORED_ICON_POSITION],
			 data,
			 &position,
			 &have_stored_position);

	icon = g_new0 (NautilusIcon, 1);
	
	icon->scale_x = position.scale_x;
	icon->scale_y = position.scale_y;

	icon->data = data;

 	icon->item = NAUTILUS_ICON_CANVAS_ITEM
		(gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (container)->root),
					nautilus_icon_canvas_item_get_type (),
					NULL));
	icon->item->user_data = icon;

	if (!details->auto_layout && have_stored_position) {
		icon_set_position (icon, position.x, position.y);
	} else {
		auto_position_icon (container, icon);
	}

	details->icons = g_list_prepend (details->icons, icon);

	nautilus_icon_container_update_icon (container, icon);
	icon_show (icon);
	
	nautilus_icon_canvas_item_update_bounds (icon->item);
	nautilus_icon_grid_add (details->grid, icon);

	/* Must connect the bounds_changed signal after adding the icon to the
	 * grid, because it will try to remove/add the icon too.
	 */
	gtk_signal_connect (GTK_OBJECT (icon->item), "event",
			    GTK_SIGNAL_FUNC (item_event_callback), container);
	gtk_signal_connect (GTK_OBJECT (icon->item), "bounds_changed",
			    GTK_SIGNAL_FUNC (bounds_changed_callback), container);

	request_idle (container);

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
	GList *p;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (container), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* FIXME bugzilla.eazel.com 1288: 
	 * I guess we need to use an indexed data structure to avoid this loop.
	 */
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->data == data) {
			icon_destroy (container, icon);
			relayout (container);
			return TRUE;
		}
	}

	return FALSE;
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
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->data == data) {
			nautilus_icon_container_update_icon (container, icon);
			return;
		}
	}
}

/* zooming */

int
nautilus_icon_container_get_zoom_level(NautilusIconContainer *container)
{
        return container->details->zoom_level;
}

void
nautilus_icon_container_set_zoom_level(NautilusIconContainer *container, int new_level)
{
	NautilusIconContainerDetails *details;
        int pinned_level;
	double pixels_per_unit;
	
	details = container->details;

	end_renaming_mode(container, TRUE);
		
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

	world_width_changed (container, GTK_WIDGET (container)->allocation.width);
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
	relayout (container);
}

/** * nautilus_icon_container_set_anti_aliased_mode:
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

	hash = g_hash_table_new (g_direct_hash, g_direct_equal);
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

	hash = g_hash_table_new (g_direct_hash, g_direct_equal);
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
	}
	nautilus_icon_canvas_item_set_show_stretch_handles (icon->item, TRUE);
	details->stretch_icon = icon;
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
							   FALSE);
		}
	}
}

static void
compute_stretch (StretchState *start,
		 StretchState *current)
{
	gboolean right, bottom;
	int x_stretch, y_stretch;

	/* Figure out which handle we are dragging. */
	right = start->pointer_x > start->icon_x + start->icon_size / 2;
	bottom = start->pointer_y > start->icon_y + start->icon_size / 2;

	/* Figure out how big we should stretch. */
	x_stretch = start->pointer_x - current->pointer_x;
	y_stretch = start->pointer_y - current->pointer_y;
	if (right) {
		x_stretch = - x_stretch;
	}
	if (bottom) {
		y_stretch = - y_stretch;
	}
	current->icon_size = MAX ((int)start->icon_size + MIN (x_stretch, y_stretch),
				  (int)NAUTILUS_ICON_SIZE_SMALLEST);

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

	container->details->auto_layout = auto_layout;

	if (auto_layout) {
		relayout (container);
	} else {
		reload_icon_positions (container);
		nautilus_icon_container_freeze_icon_positions (container);
	}
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

	relayout (container);

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

/**
 * nautilus_icon_container_is_renaming
 * @container: An icon container widget.
 * 
 * Returns true if container is in renaming mode
 **/
 
gboolean
nautilus_icon_container_is_renaming (NautilusIconContainer *container)
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
	ArtIRect text_rect;
	ArtDRect icon_rect;
	GdkFont *font;
	const char *editable_text;
	int max_text_width;
	int cx0, cy0, cx1, cy1;
	int marginX, marginY;
	double ppu;
	
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

	/* Get location of text item */
	nautilus_icon_canvas_item_get_text_bounds (icon->item, &text_rect);

	/* Make a copy of the original editable text for a later compare */
	editable_text = nautilus_icon_canvas_item_get_editable_text (icon->item);
	details->original_text = g_strdup (editable_text);

	/* Create text renaming widget */	
	details->rename_widget = NAUTILUS_ICON_TEXT_ITEM
		(gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (container)),
					nautilus_icon_text_item_get_type (),
					NULL));

	/* Determine widget position widget in container */
	font = details->label_font[details->zoom_level];
	ppu = GNOME_CANVAS_ITEM (icon->item)->canvas->pixels_per_unit;
	max_text_width = floor (nautilus_icon_canvas_item_get_max_text_width (icon->item) * ppu);
	nautilus_icon_canvas_item_get_icon_rectangle (icon->item, &icon_rect);
	gnome_canvas_w2c(GNOME_CANVAS(container), icon_rect.x0, icon_rect.y0, &cx0, &cy0);
	gnome_canvas_w2c(GNOME_CANVAS(container), icon_rect.x1, icon_rect.y1, &cx1, &cy1);

	cx0 = cx0 + ((cx1 - cx0) - max_text_width) / 2;
	cy0 = text_rect.y0;

	nautilus_icon_text_item_get_margins(&marginX, &marginY);
		
	nautilus_icon_text_item_configure (details->rename_widget, 
					   cx0 - marginX,	/* x */
					   cy0 - marginY,	/* y */		
					   max_text_width + 4, 	/* width */
					   font,		/* font */
					   editable_text,	/* text */
					   1);			/* allocate local copy */
	
	/* Set up the signals */
	gtk_signal_connect (GTK_OBJECT (details->rename_widget), "editing_started",
			    GTK_SIGNAL_FUNC (editing_started),
			    container);
	gtk_signal_connect (GTK_OBJECT (details->rename_widget), "editing_stopped",
			    GTK_SIGNAL_FUNC (editing_stopped),
			    container);
	
	nautilus_icon_text_item_start_editing (details->rename_widget);
	nautilus_icon_container_update_icon (container, icon);
	
	/* We are in renaming mode */
	details->renaming = TRUE;
	
	nautilus_icon_canvas_item_set_renaming (icon->item, details->renaming);
}

static void
end_renaming_mode (NautilusIconContainer *container, gboolean commit)
{
	NautilusIcon *icon;
	char *changed_text;
		
	if (!container->details->renaming) {
		return;
	}

	icon = get_first_selected_icon (container);
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
	
	hide_rename_widget(container, icon);
}

static void
hide_rename_widget (NautilusIconContainer *container, NautilusIcon *icon)
{
	nautilus_icon_text_item_stop_editing (container->details->rename_widget, TRUE);

	/* Destroy renaming widget */
	gtk_object_destroy (GTK_OBJECT (container->details->rename_widget));
	container->details->rename_widget = NULL;

	g_free (container->details->original_text);
	
	/* We are not in renaming mode */
	container->details->renaming = FALSE;
	nautilus_icon_canvas_item_set_renaming (icon->item, container->details->renaming);		
}

/* emit preview signal, called by the canvas item */

int nautilus_icon_container_emit_preview_signal(NautilusIconContainer *icon_container, GnomeCanvasItem *item, gboolean start_flag)
{
	int result;
	NautilusIcon *icon;
	
	icon = NAUTILUS_ICON_CANVAS_ITEM (item)->user_data;

	result = 0;
	gtk_signal_emit (GTK_OBJECT (icon_container),
		signals[PREVIEW],
		icon->data,
		start_flag,
		&result);
	
	return result;
}

/* preferences callbacks */
static void
click_policy_changed_callback (gpointer user_data)
{
	NautilusIconContainer *container;
	int mode;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (user_data));
	
	container = NAUTILUS_ICON_CONTAINER (user_data);

	mode = nautilus_preferences_get_enum
		(NAUTILUS_PREFERENCES_CLICK_POLICY,
		 NAUTILUS_CLICK_POLICY_SINGLE);
	container->details->single_click_mode =
		mode == NAUTILUS_CLICK_POLICY_SINGLE;
}

static void
anti_aliased_preferences_changed (gpointer user_data)
{
	NautilusIconContainer *container;
	gboolean aa_mode;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (user_data));
	
	container = NAUTILUS_ICON_CONTAINER (user_data);

	aa_mode = nautilus_preferences_get_boolean
		(NAUTILUS_PREFERENCES_ANTI_ALIASED_CANVAS,
		 FALSE);

	nautilus_icon_container_set_anti_aliased_mode(container, aa_mode);
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

#if ! defined (NAUTILUS_OMIT_SELF_CHECK)

static char *
nautilus_self_check_compute_stretch (int icon_x, int icon_y, int icon_size,
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
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_compute_stretch (0, 0, 12, 0, 0, 0, 0), "0,0:12");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_compute_stretch (0, 0, 12, 12, 12, 13, 13), "0,0:13");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_compute_stretch (0, 0, 12, 12, 12, 13, 12), "0,0:12");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_compute_stretch (100, 100, 64, 105, 105, 40, 40), "35,35:129");
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
