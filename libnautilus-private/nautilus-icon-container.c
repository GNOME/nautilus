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

#include <math.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

#include "nautilus-glib-extensions.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gnome-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-lib-self-check-functions.h"

#include "nautilus-icon-grid.h"
#include "nautilus-icon-private.h"

/* Interval for updating the rubberband selection, in milliseconds.  */
#define RUBBERBAND_TIMEOUT_INTERVAL 10

/* Timeout for making the icon currently selected for keyboard operation
 * visible. FIXME: This *must* be higher than the double-click time in GDK,
 * but there is no way to access its value from outside.
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

/* Maximum size (multiplier) allowed for icons at the time
 * they are installed - the user can still stretch them further.
 */
#define MAXIMUM_INITIAL_ICON_SIZE 2

static void                    activate_selected_items               (NautilusIconContainer      *container);
static void                    nautilus_icon_container_initialize_class (NautilusIconContainerClass *class);
static void                    nautilus_icon_container_initialize       (NautilusIconContainer      *container);
static void                    update_icon                           (NautilusIconContainer      *container,
								      NautilusIcon  *icon);
static void                    compute_stretch                       (StretchState            *start,
								      StretchState            *current);
static NautilusIcon *get_first_selected_icon               (NautilusIconContainer      *container);
static NautilusIcon *get_nth_selected_icon                 (NautilusIconContainer      *container,
								      int                      index);
#if 0
static gboolean                has_selection                         (NautilusIconContainer      *container);
#endif
static gboolean                has_multiple_selection                (NautilusIconContainer      *container);
static void                    icon_destroy                          (NautilusIconContainer      *container,
								      NautilusIcon  *icon);
static guint                   icon_get_actual_size                  (NautilusIcon  *icon);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIconContainer, nautilus_icon_container, GNOME_TYPE_CANVAS)



/* The NautilusIconContainer signals.  */
enum {
	ACTIVATE,
	BUTTON_PRESS,
	CONTEXT_CLICK_BACKGROUND,
	CONTEXT_CLICK_SELECTION,
	GET_ICON_IMAGES,
	GET_ICON_PROPERTY,
	GET_ICON_TEXT,
	GET_ICON_URI,
	ICON_CHANGED,
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

static NautilusIcon *
icon_new (NautilusIconContainer *container,
	  NautilusIconData *data)
{
	NautilusIcon *icon;
        GnomeCanvas *canvas;
	guint max_size, actual_size;
        
	canvas = GNOME_CANVAS (container);
	
	icon = g_new0 (NautilusIcon, 1);
	
	icon->scale_x = 1.0;
	icon->scale_y = 1.0;
	
	icon->data = data;

        icon->item = NAUTILUS_ICON_CANVAS_ITEM
		(gnome_canvas_item_new (GNOME_CANVAS_GROUP (canvas->root),
					nautilus_icon_canvas_item_get_type (),
					NULL));
	icon->item->user_data = icon;

	update_icon (container, icon);
	
	/* Enforce a maximum size for new icons by reducing the scale factor as necessary.
	 * FIXME: This needs to be done again later when the image changes, so it's not
	 * sufficient to just have this check here. Also, this should not be done by
	 * changing the scale factor because we don't want a persistent change to that.
	 * I think that the best way to implement this is probably to put something in
	 * the icon factory that enforces this rule.
	 */
	max_size = nautilus_get_icon_size_for_zoom_level (container->details->zoom_level)
		* MAXIMUM_INITIAL_ICON_SIZE;
	actual_size = icon_get_actual_size (icon);
	if (actual_size > max_size) {
		icon->scale_x = max_size / (double) actual_size;
		icon->scale_y = icon->scale_x;
		update_icon (container, icon);
	}
	
	return icon;
}

static void
icon_set_position (NautilusIcon *icon,
		   double x, double y)
{
	if (icon->x == x && icon->y == y)
		return;

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

/* return the size in pixels of the largest dimension of the pixmap associated with the icon */ 
static guint
icon_get_actual_size (NautilusIcon *icon)
{
	GdkPixbuf *pixbuf;
	guint max_size;
	
 	pixbuf = nautilus_icon_canvas_item_get_image (icon->item, NULL);
	max_size = pixbuf->art_pixbuf->width;
	if (pixbuf->art_pixbuf->height > max_size) {
		max_size = pixbuf->art_pixbuf->height;
	}

	return max_size;
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
	/* Since is_selected is a bit field, we have to do the ! business
	 * to be sure we have either a 1 or a 0. Similarly, the caller
	 * might pass a value other than 1 or 0 so we have to pass do the
	 * same thing there.
	 */
	if (!select == !icon->is_selected) {
		return FALSE;
	}

	icon_toggle_selected (container, icon);
	g_assert (!select == !icon->is_selected);
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
	 * FIXME: Need to unschedule this if the user scrolls explicitly.
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
 * The update is in there with an #if 0 around it, with no explanation fo
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

	/* FIXME: Why are we subtracting one from each dimension? */
	nautilus_gnome_canvas_set_scroll_region (GNOME_CANVAS (container),
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

static gboolean
idle_handler (gpointer data)
{
	NautilusIconContainer *container;
	NautilusIconContainerDetails *details;

	GDK_THREADS_ENTER ();

	container = NAUTILUS_ICON_CONTAINER (data);
	details = container->details;

	set_scroll_region (container);

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
	return event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
}

static gboolean
select_one_unselect_others (NautilusIconContainer *container,
			    NautilusIcon *icon_to_select)
{
	NautilusIconContainerDetails *details;
	GList *p;
	gboolean selection_changed;

	details = container->details;
	selection_changed = FALSE;
	
	for (p = details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		selection_changed |= icon_set_selected (container, icon, icon == icon_to_select);
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

	details = container->details;

	emit_signal = FALSE;

	if (x != icon->x || y != icon->y) {
		icon_set_position (icon, x, y);
		emit_signal = TRUE;
	}
	
	if (scale_x != icon->scale_x || scale_y != icon->scale_y) {
		icon->scale_x = scale_x;
		icon->scale_y = scale_y;
		update_icon (container, icon);
		emit_signal = TRUE;
	}
	
	if (emit_signal) {
		gtk_signal_emit (GTK_OBJECT (container), signals[ICON_CHANGED],
				 icon->data, x, y, scale_x, scale_y);
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
		
	/* As an optimization, ask the grid which icons intersect the rectangles. */
	art_drect_union (&both_rects, previous_rect, current_rect);
	icons = nautilus_icon_grid_get_intersecting_icons (container->details->grid,
								  &both_rects);
	
	selection_changed = FALSE;

	for (p = icons; p != NULL; p = p->next) {
		icon = p->data;

		is_in = nautilus_icon_canvas_item_hit_test_rectangle
			(icon->item, current_rect);
		if (icon_set_selected (container, icon,
				       is_in ^ icon->was_selected_before_rubberband)) {
			selection_changed = TRUE;
		}
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

	gnome_canvas_item_set (band_info->selection_rectangle,
			       "x1", x1,
			       "y1", y1,
			       "x2", x2,
			       "y2", y2,
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

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      event->x, event->y,
				      &band_info->start_x, &band_info->start_y);

	band_info->selection_rectangle
		= gnome_canvas_item_new (gnome_canvas_root
					 (GNOME_CANVAS (container)),
					 gnome_canvas_rect_get_type (),
					 "x1", band_info->start_x,
					 "y1", band_info->start_y,
					 "x2", band_info->start_x,
					 "y2", band_info->start_y,
					 "outline_color", "black",
					 "outline_stipple", stipple,
					 "width_pixels", 2,
					 NULL);

	band_info->prev_x = event->x;
	band_info->prev_y = event->y;

	band_info->active = TRUE;

	if (band_info->timer_id == 0) {
		band_info->timer_id = gtk_timeout_add (RUBBERBAND_TIMEOUT_INTERVAL,
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
					   NautilusIcon *candidate);

static NautilusIcon *
find_best_icon (NautilusIconContainer *container,
		NautilusIcon *start_icon,
		IsBetterIconFunction function)
{
	GList *p;
	NautilusIcon *best, *candidate;

	best = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		candidate = p->data;

		if (candidate != start_icon) {
			if ((* function) (container, start_icon, best, candidate)) {
				best = candidate;
			}
		}
	}
	return best;
}

static NautilusIcon *
find_best_selected_icon (NautilusIconContainer *container,
			 NautilusIcon *start_icon,
			 IsBetterIconFunction function)
{
	GList *p;
	NautilusIcon *best, *candidate;

	best = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		candidate = p->data;

		if (candidate != start_icon && candidate->is_selected) {
			if ((* function) (container, start_icon, best, candidate)) {
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
		     NautilusIcon *candidate)
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
			 NautilusIcon *candidate)
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
			      NautilusIcon *candidate)
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
			      NautilusIcon *candidate)
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
			  NautilusIcon *candidate)
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
			  NautilusIcon *candidate)
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

	if ((event->state & GDK_CONTROL_MASK) == 0) {
		/* Select icons and get rid of the special keyboard focus. */
		select_one_unselect_others (container, icon);
		clear_keyboard_focus (container);
	} else {
		/* Move the keyboard focus. */
		set_keyboard_focus (container, icon);
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
					  leftmost_in_top_row),
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
					  rightmost_in_bottom_row),
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

	nautilus_icon_canvas_item_get_icon_rectangle (icon->item,
							  &world_rect);
	gnome_canvas_w2c (GNOME_CANVAS (container),
			  (world_rect.x0 + world_rect.x1) / 2,
			  (world_rect.y0 + world_rect.y1) / 2,
			  arrow_key_axis == AXIS_VERTICAL ? &container->details->arrow_key_start : NULL,
			  arrow_key_axis == AXIS_HORIZONTAL ? &container->details->arrow_key_start : NULL);
	container->details->arrow_key_axis = arrow_key_axis;
}

static void
keyboard_arrow_key (NautilusIconContainer *container,
		    GdkEventKey *event,
		    Axis axis,
		    IsBetterIconFunction better_start,
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
			icon = find_best_selected_icon (container,
							NULL,
							better_start);
		} else {
			icon = get_first_selected_icon (container);
		}
	}

	/* If there's no icon, select the icon farthest toward the end.
	 * If there is an icon, select the next icon based on the arrow direction.
	 */
	if (icon == NULL) {
		container->details->arrow_key_axis = AXIS_NONE;
		icon = find_best_icon (container,
				       NULL,
				       better_start);
	} else {
		record_arrow_key_start (container, icon, axis);
		icon = find_best_icon (container,
				       icon,
				       better_destination);
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
			    same_column_above_lowest);
}

static void
keyboard_space (NautilusIconContainer *container,
		GdkEventKey *event)
{
	/* Control-space toggles the selection state of the current icon. */
	if (container->details->keyboard_focus != NULL &&
	    (event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK) {
		icon_toggle_selected (container, container->details->keyboard_focus);
		gtk_signal_emit (GTK_OBJECT (container), signals[SELECTION_CHANGED]);
	}
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	NautilusIconContainer *container;
	int i;

	container = NAUTILUS_ICON_CONTAINER (object);

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
        	if (container->details->label_font[i] != NULL) {
                	gdk_font_unref (container->details->label_font[i]);
		}
	}
	
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
size_allocate (GtkWidget *widget,
	       GtkAllocation *allocation)
{
	NautilusIconContainer *container;
	NautilusIconGrid *grid;
	double world_width;

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	container = NAUTILUS_ICON_CONTAINER (widget);
	grid = container->details->grid;

	gnome_canvas_c2w (GNOME_CANVAS (container),
			  allocation->width, 0,
			  &world_width, NULL);

	nautilus_icon_grid_set_visible_width (grid, world_width);

	set_scroll_region (container);
}

static void
realize (GtkWidget *widget)
{
	GtkStyle *style;

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

	style = gtk_style_copy (gtk_widget_get_style (widget));
	style->bg[GTK_STATE_NORMAL] = style->base[GTK_STATE_NORMAL];
	gtk_widget_set_style (widget, style);

	gdk_window_set_background (GTK_LAYOUT (widget)->bin_window,
				   & widget->style->bg [GTK_STATE_NORMAL]);
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

static void
nautilus_icon_container_almost_drag (NautilusIconContainer *container,
				  GdkEventButton *event)
{
	NautilusIconContainerDetails *details;

	details = container->details;

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
		int elapsed_time;
		
		/* If single-click mode, activate the icon, unless modifying
		 * the selection or pressing for a very long time.
		 */
		elapsed_time = event->time - details->button_down_time;
		if (details->single_click_mode
		    && elapsed_time < MAX_CLICK_TIME
		    && ! button_event_modifies_selection (event)) {
			
			/* FIXME: This should activate all selected icons, not just one */
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[ACTIVATE],
					 details->drag_icon->data);
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
			if (!details->drag_started)
				nautilus_icon_container_almost_drag (container, event);
			else
				nautilus_icon_dnd_end_drag (container);
			break;
		case DRAG_ACTION_STRETCH:
			end_stretching (container, event->x, event->y);
			break;
		}

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

			gnome_canvas_window_to_world (GNOME_CANVAS (container),
						      motion->x, motion->y,
						      &world_x, &world_y);
			
			if (abs (details->drag_x - world_x) >= SNAP_RESISTANCE
			    && abs (details->drag_y - world_y) >= SNAP_RESISTANCE) {
				
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

static int
key_press_event (GtkWidget *widget,
		 GdkEventKey *event)
{
	NautilusIconContainer *container;

	if (NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, key_press_event, (widget, event))) {
		return TRUE;
	}

	container = NAUTILUS_ICON_CONTAINER (widget);

	switch (event->keyval) {
	case GDK_Home:
		keyboard_home (container, event);
		break;
	case GDK_End:
		keyboard_end (container, event);
		break;
	case GDK_Left:
		keyboard_left (container, event);
		break;
	case GDK_Up:
		keyboard_up (container, event);
		break;
	case GDK_Right:
		keyboard_right (container, event);
		break;
	case GDK_Down:
		keyboard_down (container, event);
		break;
	case GDK_space:
		keyboard_space (container, event);
		break;
	case GDK_Return:
		activate_selected_items (container);
		break;
	default:
		return FALSE;
	}

	return TRUE;
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
	signals[ICON_CHANGED]
		= gtk_signal_new ("icon_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     icon_changed),
				  nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE_DOUBLE,
				  GTK_TYPE_NONE, 5,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT,
				  GTK_TYPE_DOUBLE,
				  GTK_TYPE_DOUBLE);
	signals[GET_ICON_IMAGES]
		= gtk_signal_new ("get_icon_images",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_images),
				  nautilus_gtk_marshal_POINTER__POINTER_POINTER,
				  GTK_TYPE_POINTER, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_POINTER);
	signals[GET_ICON_TEXT]
		= gtk_signal_new ("get_icon_text",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_text),
				  nautilus_gtk_marshal_STRING__POINTER,
				  GTK_TYPE_STRING, 1,
				  GTK_TYPE_POINTER);
	signals[GET_ICON_URI]
		= gtk_signal_new ("get_icon_uri",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_uri),
				  nautilus_gtk_marshal_STRING__POINTER,
				  GTK_TYPE_STRING, 1,
				  GTK_TYPE_POINTER);
	signals[GET_ICON_PROPERTY]
		= gtk_signal_new ("get_icon_property",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconContainerClass,
						     get_icon_property),
				  nautilus_gtk_marshal_STRING__POINTER_STRING,
				  GTK_TYPE_STRING, 2,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	/* GtkWidget class.  */

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = size_request;
	widget_class->size_allocate = size_allocate;
	widget_class->realize = realize;
	widget_class->button_press_event = button_press_event;
	widget_class->button_release_event = button_release_event;
	widget_class->motion_notify_event = motion_notify_event;
	widget_class->key_press_event = key_press_event;

	/* Initialize the stipple bitmap.  */

	stipple = gdk_bitmap_create_from_data (NULL, stipple_bits, 2, 2);
}

static GdkFont *
load_font (const char *name)
{
	GdkFont *font;

	/* FIXME: Eventually we need a runtime check, but an assert is better than nothing. */
	font = gdk_font_load (name);
	g_assert (font != NULL);
	return font;
}

static void
nautilus_icon_container_initialize (NautilusIconContainer *container)
{
	NautilusIconContainerDetails *details;

	details = g_new0 (NautilusIconContainerDetails, 1);

	details->grid = nautilus_icon_grid_new ();

        details->zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;
 
 	/* font table - this isn't exactly proportional, but it looks better than computed */
        /* FIXME: read font from metadata and/or preferences */
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALLEST] = load_font ("-*-helvetica-medium-r-normal-*-8-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALLER] = load_font ("-*-helvetica-medium-r-normal-*-8-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALL] = load_font ("-*-helvetica-medium-r-normal-*-10-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_STANDARD] = load_font ("-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGE] = load_font ("-*-helvetica-medium-r-normal-*-14-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGER] = load_font ("-*-helvetica-medium-r-normal-*-18-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGEST] = load_font ("-*-helvetica-medium-r-normal-*-18-*-*-*-*-*-*-*");

	/* FIXME: Read this from preferences. */
	details->single_click_mode = TRUE;

	container->details = details;

	/* Set up DnD.  */
	nautilus_icon_dnd_init (container, stipple);

	/* Request update.  */
	request_idle (container);

	/* Make sure that we find out if the theme changes. */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       nautilus_icon_container_request_update_all,
					       GTK_OBJECT (container));
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

	if (event->button != DRAG_BUTTON && event->button != CONTEXTUAL_MENU_BUTTON) {
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
	
	/* Modify the selection as appropriate. Selection is modified
	 * the same way for contextual menu as it would be without. 
	 */
	if (button_event_modifies_selection (event)) {
		icon_toggle_selected (container, icon);
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	} else if (! icon->is_selected) {
		unselect_all (container);
		icon_set_selected (container, icon, TRUE);
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	}

	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		/* FIXME this means you cannot drag with right click.
		 * If we decide we want right drags, we will have to
		 * set up a timeout and emit this signal if the
                 *  timeout expires without movement.
		 */

		details->drag_button = 0;

		/* Context menu applies to all selected items. The only
		 * odd case is if this click deselected the icon under
		 * the mouse, but at least the behavior is consistent.
		 */
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[CONTEXT_CLICK_SELECTION]);

		return TRUE;
	} else if (event->type == GDK_2BUTTON_PRESS) {
		/* Double clicking does not trigger a D&D action. */
		details->drag_button = 0;

		/* FIXME: This should activate all selected icons, not just one */
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[ACTIVATE],
				 icon->data);
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
	GtkWidget *new;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	new = gtk_type_new (nautilus_icon_container_get_type ());

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

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
	NautilusIcon *icon;
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	for (p = container->details->icons; p != NULL; p = p->next) {  	
	  	icon = p->data;
		if (icon->is_selected) {
	  		gtk_signal_emit (GTK_OBJECT (container),
					 signals[ACTIVATE],
					 icon->data);
		}
	}
}

static void
bounds_changed_callback (NautilusIconCanvasItem *item,
			 const ArtDRect *old_bounds,
			 NautilusIconContainer *container)
{
	NautilusIcon *icon;

	g_assert (NAUTILUS_IS_ICON_CANVAS_ITEM (item));
	g_assert (old_bounds != NULL);
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));

	icon = item->user_data;
	g_assert (icon != NULL);

	nautilus_icon_grid_remove (container->details->grid, icon);
	nautilus_icon_grid_add (container->details->grid, icon);
}

static void
set_up_icon_in_container (NautilusIconContainer *container,
			  NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;

	details = container->details;

	details->icons = g_list_prepend (details->icons, icon);

	nautilus_icon_canvas_item_update_bounds (icon->item);
	nautilus_icon_grid_add (details->grid, icon);

	icon_show (icon);

	gtk_signal_connect (GTK_OBJECT (icon->item), "event",
			    GTK_SIGNAL_FUNC (item_event_callback), container);
	gtk_signal_connect (GTK_OBJECT (icon->item), "bounds_changed",
			    GTK_SIGNAL_FUNC (bounds_changed_callback), container);
}

static void 
update_icon (NautilusIconContainer *container, NautilusIcon *icon)
{
	NautilusIconContainerDetails *details;
	NautilusScalableIcon *scalable_icon;
	guint icon_size_x, icon_size_y;
	GdkPixbuf *pixbuf, *emblem_pixbuf;
	ArtIRect text_rect;
	GList *emblem_icons, *emblem_pixbufs, *p;
	char *label;
	char *contents_as_text;
	GdkFont *font;

	details = container->details;

	/* Get the icons. */
	scalable_icon = NULL;
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_ICON_IMAGES],
			 icon->data,
			 &emblem_icons,
			 &scalable_icon);
	g_assert (scalable_icon != NULL);

	/* Get the corresponding pixbufs for this size. */
	icon_get_size (container, icon, &icon_size_x, &icon_size_y);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(scalable_icon, icon_size_x, icon_size_y, &text_rect);
	emblem_pixbufs = NULL;
	for (p = emblem_icons; p != NULL; p = p->next) {
		emblem_pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data, icon_size_x, icon_size_y, NULL);
		if (emblem_pixbuf != NULL) {
			emblem_pixbufs = g_list_prepend
				(emblem_pixbufs, emblem_pixbuf);
		}
	}
	emblem_pixbufs = g_list_reverse (emblem_pixbufs);

	/* Let the icons go. */
	nautilus_scalable_icon_unref (scalable_icon);
	nautilus_scalable_icon_list_free (emblem_icons);

	label = NULL;
	gtk_signal_emit (GTK_OBJECT (container),
			 signals[GET_ICON_TEXT],
			 icon->data,
			 &label);

	font = details->label_font[details->zoom_level];
        
        /* Choose to show mini-text based on this icon's requested size,
	 * not zoom level, since icon may be stretched big or small.
         */
	contents_as_text = NULL;
	if (!art_irect_empty (&text_rect)
	    && icon_size_x >= NAUTILUS_ICON_SIZE_STANDARD
	    && icon_size_y >= NAUTILUS_ICON_SIZE_STANDARD) {
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[GET_ICON_PROPERTY],
				 icon->data,
				 "contents_as_text",
				 &contents_as_text);
	}
	
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->item),
			       "text", label,
			       "font", font,
			       "text_source", contents_as_text,
			       NULL);
	nautilus_icon_canvas_item_set_image (icon->item, pixbuf, &text_rect);
	nautilus_icon_canvas_item_set_emblems (icon->item, emblem_pixbufs);

	/* Let the pixbufs go. */
	gdk_pixbuf_unref (pixbuf);
	nautilus_gdk_pixbuf_list_free (emblem_pixbufs);

	g_free (label);
        g_free (contents_as_text);
}

void
nautilus_icon_container_add (NautilusIconContainer *container,
			  NautilusIconData *data,
			  int x, int y,
			  double scale_x, double scale_y)
{
	NautilusIconContainerDetails *details;
	NautilusIcon *new_icon;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	details = container->details;

	new_icon = icon_new (container, data);
	icon_set_position (new_icon, x, y);
	new_icon->scale_x = scale_x;
	new_icon->scale_y = scale_y;

	set_up_icon_in_container (container, new_icon);

	request_idle (container);

	update_icon (container, new_icon);
}

/**
 * nautilus_icon_container_add_auto:
 * @container: A NautilusIconContainer
 * @data: Icon data.
 * 
 * Add @image with caption @text and data @data to @container, in the first
 * empty spot available.
 **/
void
nautilus_icon_container_add_auto (NautilusIconContainer *container,
			       NautilusIconData *data)
{
	NautilusIcon *new_icon;
	ArtPoint position;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	new_icon = icon_new (container, data);

	nautilus_icon_grid_get_position (container->details->grid,
						new_icon,
						&position);

	icon_set_position (new_icon, position.x, position.y);

	set_up_icon_in_container (container, new_icon);

	request_idle (container);
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

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->data == data) {
			icon_destroy (container, icon);
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
			update_icon (container, icon);
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
		update_icon (container, p->data);
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

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	selection_changed = FALSE;
	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		selection_changed |= icon_set_selected (container, icon, TRUE);
	}

	if (selection_changed) {
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	}
}

/**
 * nautilus_icon_container_select_list_unselect_others:
 * @container: An icon container widget.
 * @list: A list of BonoboContainerIcons.
 * 
 * Select only the icons in the list, deselect all others.
 **/
void
nautilus_icon_container_select_list_unselect_others (NautilusIconContainer *container,
						  GList *icons)
{
	gboolean selection_changed;
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	/* To avoid an N^2 algorithm, we could put the icons into a hash
	   table, but this should be OK for now.
	*/

	selection_changed = FALSE;
	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		selection_changed |= icon_set_selected
			(container, icon, g_list_find (icons, icon) != NULL);
	}

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

#if 0

static gboolean
has_selection (NautilusIconContainer *container)
{
	return get_first_selected_icon (container) != NULL;
}

#endif

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
