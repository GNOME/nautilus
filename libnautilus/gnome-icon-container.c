/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container.c - Icon container widget.

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

#include <config.h>
#include "gnome-icon-container.h"

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

#include "gdk-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-lib-self-check-functions.h"

#include "gnome-icon-container-private.h"

/* Interval for updating the rubberband selection, in milliseconds.  */
#define RUBBERBAND_TIMEOUT_INTERVAL 10

/* Timeout for making the icon currently selected for keyboard operation
 * visible.  FIXME: This *must* be higher than the double-click time in GDK,
 * but there is no way to access its value from outside.
 */
#define KBD_ICON_VISIBILITY_TIMEOUT 300

/* Timeout for selecting an icon in "linger-select" mode (i.e. by just placing the
 * pointer over the icon, without pressing any button).
 */
#define LINGER_SELECTION_MODE_TIMEOUT 800

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

static void gnome_icon_container_initialize_class (GnomeIconContainerClass *class);
static void gnome_icon_container_initialize (GnomeIconContainer *container);
static void update_icon (GnomeIconContainer *container, 
			 GnomeIconContainerIcon *icon);
static void compute_stretch (StretchState *start,
			     StretchState *current);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (GnomeIconContainer, gnome_icon_container, GNOME_TYPE_CANVAS)



/* The GnomeIconContainer signals.  */
enum {
	SELECTION_CHANGED,
	BUTTON_PRESS,
	ACTIVATE,
	CONTEXT_CLICK_SELECTION,
	CONTEXT_CLICK_BACKGROUND,
	ICON_MOVED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

/* Bitmap for stippled selection rectangles.  */
static GdkBitmap *stipple;
static char stipple_bits[] = { 0x02, 0x01 };


/* Functions dealing with GnomeIconContainerIcons.  */

static void
icon_destroy (GnomeIconContainerIcon *icon)
{
	gtk_object_destroy (GTK_OBJECT (icon->item));
}

static GnomeIconContainerIcon *
icon_new (GnomeIconContainer *container,
	  NautilusControllerIcon *data)
{
	GnomeIconContainerIcon *new;
        GnomeCanvas *canvas;
        
	canvas = GNOME_CANVAS (container);
	
	new = g_new0 (GnomeIconContainerIcon, 1);
	
	new->scale = 1.0;
	new->layout_done = TRUE;
	
	new->data = data;

        new->item = NAUTILUS_ICONS_VIEW_ICON_ITEM
		(gnome_canvas_item_new (GNOME_CANVAS_GROUP (canvas->root),
					nautilus_icons_view_icon_item_get_type (),
					NULL));

	update_icon (container, new);
	
	return new;
}

static void
icon_set_position (GnomeIconContainerIcon *icon,
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

static guint
icon_get_size (GnomeIconContainer *container,
	       GnomeIconContainerIcon *icon)
{
	return MAX (nautilus_get_icon_size_for_zoom_level (container->details->zoom_level)
		    * icon->scale,
		    NAUTILUS_ICON_SIZE_SMALLEST);
}

static void
icon_set_size (GnomeIconContainer *container,
	       GnomeIconContainerIcon *icon,
	       guint icon_size)
{
	if (icon_size == icon_get_size (container, icon))
		return;

	icon->scale = (double) icon_size /
		nautilus_get_icon_size_for_zoom_level
		(container->details->zoom_level);

	update_icon (container, icon);
}

static void
icon_raise (GnomeIconContainerIcon *icon)
{
	gnome_canvas_item_raise_to_top (GNOME_CANVAS_ITEM (icon->item));
}

static void
icon_show (GnomeIconContainerIcon *icon)
{
	gnome_canvas_item_show (GNOME_CANVAS_ITEM (icon->item));
}

static void
icon_toggle_selected (GnomeIconContainer *container,
		      GnomeIconContainerIcon *icon)
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
		nautilus_icons_view_icon_item_set_show_stretch_handles (icon->item, FALSE);
	}
}

/* Select an icon. Return TRUE if selection has changed. */
static gboolean
icon_set_selected (GnomeIconContainer *container,
		   GnomeIconContainerIcon *icon,
		   gboolean select)
{
	/* Since is_selected is a bit field, we have to do the ! business
	 * to be sure we have either a 1 or a 0. Similarly, the caller
	 * might pass a value other than 1 or 0 so we have to pass do the
	 * same thing there.
	 */
	if (!select == !icon->is_selected)
		return FALSE;

	icon_toggle_selected (container, icon);
	g_assert (!select == !icon->is_selected);
	return TRUE;
}

static gboolean
icon_is_in_region (GnomeIconContainer *container,
		   GnomeIconContainerIcon *icon,
		   int x0, int y0,
		   int x1, int y1)
{
	ArtDRect rect;

	if (x0 >= x1 || y0 >= y1)
		return FALSE;

	nautilus_icons_view_icon_item_get_icon_world_rectangle
		(icon->item, &rect);
	return x0 < rect.x1 && x1 >= rect.x0 && y0 < rect.y1 && y1 >= rect.y0;
}

static void
icon_get_bounding_box (GnomeIconContainerIcon *icon,
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



/* Functions for dealing with IconGrids.  */

static GnomeIconContainerIconGrid *
icon_grid_new (void)
{
	GnomeIconContainerIconGrid *new;

	new = g_new (GnomeIconContainerIconGrid, 1);

	new->width = new->height = 0;
	new->visible_width = 0;
	new->alloc_width = new->alloc_height = 0;

	new->elems = NULL;

	new->first_free_x = -1;
	new->first_free_y = -1;

	return new;
}

static void
icon_grid_clear (GnomeIconContainerIconGrid *grid)
{
	GList **p;
	guint i, j;

	p = grid->elems;
	for (j = 0; j < grid->height; j++) {
		for (i = 0; i < grid->width; i++) {
			if (p[i] != NULL) {
				g_list_free (p[i]);
				p[i] = NULL;
			}
		}

		p += grid->alloc_width;
	}

	grid->first_free_x = 0;
	grid->first_free_y = 0;
}

static void
icon_grid_destroy (GnomeIconContainerIconGrid *grid)
{
	icon_grid_clear (grid);
	g_free (grid->elems);
	g_free (grid);
}

inline static GList **
icon_grid_get_element_ptr (GnomeIconContainerIconGrid *grid,
			   guint x, guint y)
{
	return &grid->elems[y * grid->alloc_width + x];
}

inline static GList *
icon_grid_get_element (GnomeIconContainerIconGrid *grid,
		       guint x, guint y)
{
	return *icon_grid_get_element_ptr (grid, x, y);
}

/* This is admittedly a bit lame.
 *
 * Instead of re-allocating the grid from scratch and copying the values, we
 * should just link grid chunks horizontally and vertically in lists;
 * i.e. use a hybrid list/array representation.
 */
static void
icon_grid_resize_allocation (GnomeIconContainerIconGrid *grid,
			     guint new_alloc_width,
			     guint new_alloc_height)
{
	GList **new_elems;
	guint i, j;
	guint new_alloc_size;

	if (new_alloc_width == 0 || new_alloc_height == 0) {
		g_free (grid->elems);
		grid->elems = NULL;
		grid->width = grid->height = 0;
		grid->alloc_width = new_alloc_width;
		grid->alloc_height = new_alloc_height;
		return;
	}

	new_alloc_size = new_alloc_width * new_alloc_height;
	new_elems = g_new (GList *, new_alloc_size);

	if (grid->elems == NULL || grid->width == 0 || grid->height == 0) {
		memset (new_elems, 0, sizeof (*new_elems) * new_alloc_size);
	} else {
		GList **sp, **dp;
		guint copy_width, copy_height;

		/* Copy existing elements into the new array.  */

		sp = grid->elems;
		dp = new_elems;
		copy_width = MIN (grid->width, new_alloc_width);
		copy_height = MIN (grid->height, new_alloc_height);
		
		for (i = 0; i < copy_height; i++) {
			for (j = 0; j < copy_width; j++)
				dp[j] = sp[j];

			for (j = copy_width; j < new_alloc_width; j++)
				dp[j] = NULL;

			for (j = copy_width; j < grid->width; j++)
				g_list_free (sp[j]);

			sp += grid->alloc_width;
			dp += new_alloc_width;
		}

		/* If there are other lines left, zero them as well.  */

		if (i < new_alloc_height) {
			guint elems_left;

			elems_left = new_alloc_size - (dp - new_elems);
			memset (dp, 0, sizeof (*new_elems) * elems_left);
		}
	}

	g_free (grid->elems);
	grid->elems = new_elems;

	grid->alloc_width = new_alloc_width;
	grid->alloc_height = new_alloc_height;
}

static void
icon_grid_update_first_free_forward (GnomeIconContainerIconGrid *grid)
{
	GList **p;
	guint start_x, start_y;
	guint x, y;

	if (grid->first_free_x == -1) {
		start_x = start_y = 0;
		p = grid->elems;
	} else {
		start_x = grid->first_free_x;
		start_y = grid->first_free_y;
		p = icon_grid_get_element_ptr (grid, start_x, start_y);
	}

	x = start_x;
	y = start_y;
	while (y < grid->height) {
		if (*p == NULL) {
			grid->first_free_x = x;
			grid->first_free_y = y;
			return;
		}

		x++, p++;

		if (x >= grid->visible_width) {
			x = 0;
			y++;
			p += grid->alloc_width - grid->visible_width;
		}
	}

	/* No free cell found.  */

	grid->first_free_x = -1;
	grid->first_free_y = -1;
}

static void
icon_grid_set_visible_width (GnomeIconContainerIconGrid *grid,
			     guint visible_width)
{
	if (visible_width > grid->visible_width
	    && grid->height > 0
	    && grid->first_free_x == -1) {
		grid->first_free_x = visible_width;
		grid->first_free_y = 0;
	} else if (grid->first_free_x >= visible_width) {
		if (grid->first_free_y == grid->height - 1) {
			grid->first_free_x = -1;
			grid->first_free_y = -1;
		} else {
			grid->first_free_x = 0;
			grid->first_free_y++;
			icon_grid_update_first_free_forward (grid);
		}
	}

	grid->visible_width = visible_width;
}

static void
icon_grid_resize (GnomeIconContainerIconGrid *grid,
		  guint width, guint height)
{
	guint new_alloc_width, new_alloc_height;

	if (width > grid->alloc_width || height > grid->alloc_height) {
		if (grid->alloc_width > 0)
			new_alloc_width = grid->alloc_width;
		else
			new_alloc_width = INITIAL_GRID_WIDTH;
		while (new_alloc_width < width)
			new_alloc_width *= 2;

		if (grid->alloc_height > 0)
			new_alloc_height = grid->alloc_height;
		else
			new_alloc_height = INITIAL_GRID_HEIGHT;
		while (new_alloc_height < height)
			new_alloc_height *= 2;

		icon_grid_resize_allocation (grid, new_alloc_width,
					     new_alloc_height);
	}

	grid->width = width;
	grid->height = height;

	if (grid->visible_width != grid->width)
		icon_grid_set_visible_width (grid, grid->width);
}

static void
icon_grid_maybe_resize (GnomeIconContainerIconGrid *grid,
			guint x, guint y)
{
	guint new_width, new_height;

	if (x < grid->width && y < grid->height)
		return;

	if (x >= grid->width)
		new_width = x + 1;
	else
		new_width = grid->width;

	if (y >= grid->height)
		new_height = y + 1;
	else
		new_height = grid->height;

	icon_grid_resize (grid, new_width, new_height);
}

static void
icon_grid_add (GnomeIconContainerIconGrid *grid,
	       GnomeIconContainerIcon *icon,
	       guint x, guint y)
{
	GList **elem_ptr;

	icon_grid_maybe_resize (grid, x, y);

	elem_ptr = icon_grid_get_element_ptr (grid, x, y);
	*elem_ptr = g_list_prepend (*elem_ptr, icon);

	if (x == grid->first_free_x && y == grid->first_free_y)
		icon_grid_update_first_free_forward (grid);
}

static void
icon_grid_remove (GnomeIconContainerIconGrid *grid,
		  GnomeIconContainerIcon *icon,
		  guint x, guint y)
{
	GList **elem_ptr;

	elem_ptr = icon_grid_get_element_ptr (grid, x, y);

	g_return_if_fail (*elem_ptr != NULL);

	*elem_ptr = g_list_remove (*elem_ptr, icon);

	if (*elem_ptr == NULL) {
		if ((grid->first_free_x == -1 && grid->first_free_y == -1)
		    || grid->first_free_y > y
		    || (grid->first_free_y == y && grid->first_free_x > x)) {
			grid->first_free_x = x;
			grid->first_free_y = y;
		}
	}
}

static void
icon_grid_add_auto (GnomeIconContainerIconGrid *grid,
		    GnomeIconContainerIcon *icon,
		    guint *x_return, guint *y_return)
{
	GList **empty_elem_ptr;

	if (grid->first_free_x < 0 || grid->first_free_y < 0
	    || grid->height == 0 || grid->width == 0) {
		/* No empty element: add a row.  */
		icon_grid_resize (grid, MAX (grid->width, 1), grid->height + 1);
		grid->first_free_x = 0;
		grid->first_free_y = grid->height - 1;
	}

	empty_elem_ptr = icon_grid_get_element_ptr (grid,
						    grid->first_free_x,
						    grid->first_free_y);

	*empty_elem_ptr = g_list_prepend (*empty_elem_ptr, icon);

	if (x_return != NULL)
		*x_return = grid->first_free_x;
	if (y_return != NULL)
		*y_return = grid->first_free_y;

	icon_grid_update_first_free_forward (grid);
}

static int
icon_grid_cell_compare_by_x (gconstpointer ap,
			     gconstpointer bp)
{
	GnomeIconContainerIcon *a, *b;

	a = (GnomeIconContainerIcon *) ap;
	b = (GnomeIconContainerIcon *) bp;

	return (int) a->x - b->x;
}



static void
world_to_grid (GnomeIconContainer *container,
	       int world_x, int world_y,
	       guint *grid_x_return, guint *grid_y_return)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	if (grid_x_return != NULL) {
		if (world_x < 0)
			*grid_x_return = 0;
		else
			*grid_x_return = world_x / GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	}

	if (grid_y_return != NULL) {
		if (world_y < 0)
			*grid_y_return = 0;
		else
			*grid_y_return = world_y / GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}
}

static void
grid_to_world (GnomeIconContainer *container,
	       guint grid_x, guint grid_y,
	       int *world_x_return, int *world_y_return)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	if (world_x_return != NULL)
		*world_x_return
			= grid_x * GNOME_ICON_CONTAINER_CELL_WIDTH (container);

	if (world_y_return != NULL)
		*world_y_return
			= grid_y * GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
}



/* Utility functions for GnomeIconContainer.  */

static void
scroll (GnomeIconContainer *container,
	int delta_x, int delta_y)
{
	GtkAdjustment *hadj, *vadj;

	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	gtk_adjustment_set_value (hadj, hadj->value + delta_x);
	gtk_adjustment_set_value (vadj, vadj->value + delta_y);
}

static void
make_icon_visible (GnomeIconContainer *container,
		   GnomeIconContainerIcon *icon)
{
	GnomeIconContainerDetails *details;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;
	int x1, y1, x2, y2;

	details = container->details;
	allocation = &GTK_WIDGET (container)->allocation;

	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);

	if (y1 < vadj->value)
		gtk_adjustment_set_value (vadj, y1);
	else if (y2 > vadj->value + allocation->height)
		gtk_adjustment_set_value (vadj, y2 - allocation->height);

	if (x1 < hadj->value)
		gtk_adjustment_set_value (hadj, x1);
	else if (x2 > hadj->value + allocation->width)
		gtk_adjustment_set_value (hadj, x2 - allocation->width);
}

static gboolean
kbd_icon_visibility_timeout_cb (gpointer data)
{
	GnomeIconContainer *container;

	GDK_THREADS_ENTER ();

	container = GNOME_ICON_CONTAINER (data);

	if (container->details->kbd_current != NULL)
		make_icon_visible (container, container->details->kbd_current);
	container->details->kbd_icon_visibility_timer_id = 0;

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static void
unschedule_kbd_icon_visibility (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	if (details->kbd_icon_visibility_timer_id != 0)
		gtk_timeout_remove (details->kbd_icon_visibility_timer_id);
}

static void
schedule_kbd_icon_visibility (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	unschedule_kbd_icon_visibility (container);

	details->kbd_icon_visibility_timer_id
		= gtk_timeout_add (KBD_ICON_VISIBILITY_TIMEOUT,
				   kbd_icon_visibility_timeout_cb,
				   container);
}

static void
prepare_for_layout (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GList *p;

	details = container->details;

	for (p = details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		icon->layout_done = FALSE;
	}
}

/* Find the "first" icon (in left-to-right, top-to-bottom order) in
   `container'.  */
static GnomeIconContainerIcon *
find_first (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *first;
	GList **p;
	guint i, j;

	details = container->details;
	grid = details->grid;

	if (grid->width == 0 || grid->height == 0)
		return NULL;

	first = NULL;
	p = grid->elems;
	for (i = 0; i < grid->height; i++) {
		for (j = 0; j < grid->width; j++) {
			GList *q;

			for (q = p[j]; q != NULL; q = q->next) {
				GnomeIconContainerIcon *icon;

				icon = q->data;
				if (first == NULL
				    || icon->y < first->y
				    || (icon->y == first->y
					&& icon->x < first->x))
					first = icon;
			}
		}

		p += grid->alloc_width;
	}

	return first;
}

static GnomeIconContainerIcon *
find_last (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *last;
	GList **p;
	int i, j;

	details = container->details;
	grid = details->grid;

	last = NULL;

	if (grid->height == 0 || grid->width == 0)
		return NULL;

	p = icon_grid_get_element_ptr (grid, 0, grid->height - 1);

	for (i = grid->height - 1; i >= 0; i--) {
		for (j = grid->width - 1; j >= 0; j--) {
			GList *q;

			for (q = p[j]; q != NULL; q = q->next) {
				GnomeIconContainerIcon *icon;

				icon = q->data;
				if (last == NULL
				    || icon->y > last->y
				    || (icon->y == last->y
					&& icon->x > last->x))
					last = icon;
			}
		}

		p -= grid->alloc_width;
	}

	return last;
}

/* Set `icon' as the icon currently selected for keyboard operations.  */
static void
set_kbd_current (GnomeIconContainer *container,
		 GnomeIconContainerIcon *icon,
		 gboolean schedule_visibility)
{
	GnomeIconContainerDetails *details;

	details = container->details;

        if (details->kbd_current != NULL)
		gnome_canvas_item_set (GNOME_CANVAS_ITEM (details->kbd_current->item),
				       "highlighted_for_keyboard_selection", 0,
				       NULL);
	
	details->kbd_current = icon;
	
	if (icon != NULL) {
		gnome_canvas_item_set (GNOME_CANVAS_ITEM (details->kbd_current->item),
				       "highlighted_for_keyboard_selection", 1,
				       NULL);
		icon_raise (icon);
	}
	
	if (icon != NULL && schedule_visibility)
		schedule_kbd_icon_visibility (container);
	else
		unschedule_kbd_icon_visibility (container);
}



/* Idle operation handler.  */

static void
set_scroll_region (GnomeIconContainer *container)
{
	double x1, y1, x2, y2;
        double content_width, content_height;
	double scroll_width, scroll_height;
	int step_increment;
	GtkAllocation *allocation;
	GtkAdjustment *vadj, *hadj;

	gnome_canvas_item_get_bounds (GNOME_CANVAS (container)->root,
				      &x1, &y1, &x2, &y2);

	content_width = x2 - x1 + GNOME_ICON_CONTAINER_CELL_SPACING (container);
	content_height = y2 - y1 + GNOME_ICON_CONTAINER_CELL_SPACING (container);

	allocation = &GTK_WIDGET (container)->allocation;

	scroll_width = MAX (content_width, allocation->width);
	scroll_height = MAX (content_height, allocation->height);

	/* FIXME: Why are we subtracting one from each dimension? */
	gnome_canvas_set_scroll_region (GNOME_CANVAS (container),
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
	GnomeIconContainer *container;
	GnomeIconContainerDetails *details;

	GDK_THREADS_ENTER ();

	container = GNOME_ICON_CONTAINER (data);
	details = container->details;

	set_scroll_region (container);

	if (details->icons != NULL && details->kbd_current == NULL)
		set_kbd_current (container, find_first (container), FALSE);

	details->idle_id = 0;

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static void
add_idle (GnomeIconContainer *container)
{
	if (container->details->idle_id != 0)
		return;

	container->details->idle_id = gtk_idle_add (idle_handler, container);
}


/* Container-level icon handling functions.  */

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
}

static gboolean
select_one_unselect_others (GnomeIconContainer *container,
			    GnomeIconContainerIcon *icon_to_select)
{
	GnomeIconContainerDetails *details;
	GList *p;
	gboolean selection_changed;

	details = container->details;
	selection_changed = FALSE;
	
	for (p = details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		selection_changed |= icon_set_selected (container, icon, icon == icon_to_select);
	}
	
	return selection_changed;
}

static gboolean
unselect_all (GnomeIconContainer *container)
{
	return select_one_unselect_others (container, NULL);
}

/* FIXME: This could be optimized a bit.  */
void
gnome_icon_container_move_icon (GnomeIconContainer *container,
	   GnomeIconContainerIcon *icon,
	   int x, int y, gboolean raise)
{
	GnomeIconContainerDetails *details;
	int old_x, old_y;
	guint old_grid_x, old_grid_y;
	int old_x_offset, old_y_offset;
	guint new_grid_x, new_grid_y;
	int new_x_offset, new_y_offset;

	details = container->details;

	old_x = icon->x;
	old_y = icon->y;

	world_to_grid (container, old_x, old_y, &old_grid_x, &old_grid_y);
	old_x_offset = old_x % GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	old_y_offset = old_y % GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

	world_to_grid (container, x, y, &new_grid_x, &new_grid_y);
	new_x_offset = x % GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	new_y_offset = y % GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

	icon_grid_remove (details->grid, icon, old_grid_x, old_grid_y);
	if (old_x_offset > 0)
		icon_grid_remove (details->grid, icon,
				  old_grid_x + 1, old_grid_y);
	if (old_y_offset > 0)
		icon_grid_remove (details->grid, icon,
				  old_grid_x, old_grid_y + 1);
	if (old_x_offset > 0 && old_y_offset > 0)
		icon_grid_remove (details->grid, icon,
				  old_grid_x + 1, old_grid_y + 1);

	icon_grid_add (details->grid, icon, new_grid_x, new_grid_y);
	if (new_x_offset > 0)
		icon_grid_add (details->grid, icon, new_grid_x + 1, new_grid_y);
	if (new_y_offset > 0)
		icon_grid_add (details->grid, icon, new_grid_x, new_grid_y + 1);
	if (new_x_offset > 0 && new_y_offset > 0)
		icon_grid_add (details->grid, icon, new_grid_x + 1, new_grid_y + 1);

	icon_set_position (icon, x, y);
	if (raise)
		icon_raise (icon);

	/* Update the keyboard selection indicator.  */
	if (details->kbd_current == icon)
		set_kbd_current (container, icon, FALSE);

	gtk_signal_emit (GTK_OBJECT (container), signals[ICON_MOVED],
			 icon->data, x, y);
}


/* Implementation of rubberband selection.  */

static gboolean
rubberband_select_in_cell (GnomeIconContainer *container,
			   GList *cell,
			   double curr_x1, double curr_y1,
			   double curr_x2, double curr_y2,
			   double prev_x1, double prev_y1,
			   double prev_x2, double prev_y2)
{
	GList *p;
	gboolean selection_changed;

	selection_changed = FALSE;

	for (p = cell; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;
		gboolean in_curr_region;
		gboolean in_prev_region;

		icon = p->data;

		in_curr_region = icon_is_in_region (container, icon,
						    curr_x1, curr_y1,
						    curr_x2, curr_y2);

		in_prev_region = icon_is_in_region (container, icon,
						    prev_x1, prev_y1,
						    prev_x2, prev_y2);

		if (in_curr_region && ! in_prev_region)
			selection_changed |= icon_set_selected (container, icon,
								!icon->was_selected_before_rubberband);
		else if (in_prev_region && ! in_curr_region)
			selection_changed |= icon_set_selected (container, icon,
								icon->was_selected_before_rubberband);
	}

	return selection_changed;
}

static void
rubberband_select (GnomeIconContainer *container,
		   double curr_x1, double curr_y1,
		   double curr_x2, double curr_y2,
		   double prev_x1, double prev_y1,
		   double prev_x2, double prev_y2)
{
	GList **p;
	GnomeIconContainerIconGrid *grid;
	guint curr_grid_x1, curr_grid_y1;
	guint curr_grid_x2, curr_grid_y2;
	guint prev_grid_x1, prev_grid_y1;
	guint prev_grid_x2, prev_grid_y2;
	guint grid_x1, grid_y1;
	guint grid_x2, grid_y2;
	guint i, j;
	gboolean selection_changed;

	grid = container->details->grid;

	world_to_grid (container, curr_x1, curr_y1, &curr_grid_x1, &curr_grid_y1);
	world_to_grid (container, curr_x2, curr_y2, &curr_grid_x2, &curr_grid_y2);
	world_to_grid (container, prev_x1, prev_y1, &prev_grid_x1, &prev_grid_y1);
	world_to_grid (container, prev_x2, prev_y2, &prev_grid_x2, &prev_grid_y2);

	grid_x1 = MIN (curr_grid_x1, prev_grid_x1);
	grid_x2 = MAX (curr_grid_x2, prev_grid_x2);
	grid_y1 = MIN (curr_grid_y1, prev_grid_y1);
	grid_y2 = MAX (curr_grid_y2, prev_grid_y2);

	selection_changed = FALSE;

	p = icon_grid_get_element_ptr (grid, grid_x1, grid_y1);
	for (i = 0; i <= grid_y2 - grid_y1; i++) {
		for (j = 0; j <= grid_x2 - grid_x1; j++) {
			if (rubberband_select_in_cell (container, p[j],
						       curr_x1, curr_y1,
						       curr_x2, curr_y2,
						       prev_x1, prev_y1,
						       prev_x2, prev_y2))
				selection_changed = TRUE;
		}

		p += grid->alloc_width;
	}

	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
}

static int
rubberband_timeout_cb (gpointer data)
{
	GnomeIconContainer *container;
	GtkWidget *widget;
	GnomeIconContainerRubberbandInfo *band_info;
	int x, y;
	double x1, y1, x2, y2;
	double world_x, world_y;
	int x_scroll, y_scroll;

	GDK_THREADS_ENTER ();

	widget = GTK_WIDGET (data);
	container = GNOME_ICON_CONTAINER (data);
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

	rubberband_select (container,
			   x1, y1, x2, y2,
			   band_info->prev_x1, band_info->prev_y1,
			   band_info->prev_x2, band_info->prev_y2);

	band_info->prev_x = x;
	band_info->prev_y = y;
	band_info->prev_x1 = x1;
	band_info->prev_y1 = y1;
	band_info->prev_x2 = x2;
	band_info->prev_y2 = y2;

	GDK_THREADS_LEAVE ();

	return TRUE;
}

static void
start_rubberbanding (GnomeIconContainer *container,
		     GdkEventButton *event)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerRubberbandInfo *band_info;
	GList *p;

	details = container->details;
	band_info = &details->rubberband_info;

	for (p = details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

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

	band_info->prev_x = band_info->prev_x1 = band_info->prev_x2 = event->x;
	band_info->prev_y = band_info->prev_y1 = band_info->prev_y2 = event->y;

	band_info->active = TRUE;

	if (band_info->timer_id == 0)
		band_info->timer_id = gtk_timeout_add (RUBBERBAND_TIMEOUT_INTERVAL,
						       rubberband_timeout_cb,
						       container);

	gnome_canvas_item_grab (band_info->selection_rectangle,
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK),
				NULL, event->time);
}

static void
stop_rubberbanding (GnomeIconContainer *container,
		    GdkEventButton *event)
{
	GnomeIconContainerRubberbandInfo *band_info;

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

static void
kbd_move_to (GnomeIconContainer *container,
	     GnomeIconContainerIcon *icon,
	     GdkEventKey *event)
{
	/* Control key causes keyboard selection and "selected icon" to move separately.
	 * This seems like a confusing and bad idea. 
	 */
	if (! (event->state & GDK_CONTROL_MASK)) {
		gboolean selection_changed;

		selection_changed = unselect_all (container);
		selection_changed |= icon_set_selected (container, icon, TRUE);

		if (selection_changed)
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
	}

	set_kbd_current (container, icon, FALSE);
	make_icon_visible (container, icon);
}

static void
kbd_home (GnomeIconContainer *container,
	  GdkEventKey *event)
{
	GnomeIconContainerIcon *first;

	first = find_first (container);
	if (first != NULL)
		kbd_move_to (container, first, event);
}

static void
kbd_end (GnomeIconContainer *container,
	 GdkEventKey *event)
{
	GnomeIconContainerIcon *last;

	last = find_last (container);
	if (last != NULL)
		kbd_move_to (container, last, event);
}

static void
kbd_left (GnomeIconContainer *container,
	  GdkEventKey *event)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *nearmost;
	GList **e;
	guint grid_x, grid_y;
	int x, y;
	int max_x;

	details = container->details;
	grid = details->grid;

	if (details->kbd_current == NULL)
		return;

	world_to_grid (container, details->kbd_current->x, details->kbd_current->y,
		       &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	e = icon_grid_get_element_ptr (grid, 0, grid_y);
	nearmost = NULL;

	max_x = details->kbd_current->x;

	while (1) {
		while (1) {
			GList *p;

			for (p = e[grid_x]; p != NULL; p = p->next) {
				GnomeIconContainerIcon *icon;

				icon = p->data;
				if (icon == details->kbd_current
				    || icon->x < x
				    || icon->y < y)
					continue;

				if (icon->x <= max_x
				    && (nearmost == NULL
					|| icon->x > nearmost->x))
					nearmost = icon;
			}
 
			if (nearmost != NULL) {
				kbd_move_to (container, nearmost, event);
				return;
			}

			if (grid_x == 0)
				break;

			grid_x--;
			x -= GNOME_ICON_CONTAINER_CELL_WIDTH (container);
		}

		if (grid_y == 0)
			break;

		grid_x = grid->width - 1;
		max_x = G_MAXINT;
		grid_to_world (container, grid_x, 0, &x, NULL);

		e -= grid->alloc_width;
		grid_y--;
		y -= GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}
}

static void
kbd_up (GnomeIconContainer *container,
	GdkEventKey *event)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *nearmost;
	GList **e;
	guint grid_x, grid_y;
	int x, y;

	details = container->details;
	grid = details->grid;

	if (details->kbd_current == NULL)
		return;

	world_to_grid (container, details->kbd_current->x, details->kbd_current->y,
		       &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	e = icon_grid_get_element_ptr (grid, grid_x, grid_y);
	nearmost = NULL;

	while (1) {
		GList *p;

		p = *e;

		for (; p != NULL; p = p->next) {
			GnomeIconContainerIcon *icon;

			icon = p->data;
			if (icon == details->kbd_current
			    || icon->x < x
			    || icon->y < y)
				continue;

			if (icon->y <= details->kbd_current->y
			    && (nearmost == NULL || icon->y > nearmost->y))
				nearmost = icon;
		}

		if (nearmost != NULL)
			break;

		if (grid_y == 0)
			break;

		e -= grid->alloc_width;
		grid_y--;
		y -= GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}

	if (nearmost != NULL)
		kbd_move_to (container, nearmost, event);
}

static void
kbd_right (GnomeIconContainer *container,
	   GdkEventKey *event)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *nearmost;
	GList **e;
	guint grid_x, grid_y;
	int x, y;
	int min_x;

	details = container->details;
	grid = details->grid;

	if (details->kbd_current == NULL)
		return;

	world_to_grid (container, details->kbd_current->x, details->kbd_current->y,
		       &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	e = icon_grid_get_element_ptr (grid, 0, grid_y);
	nearmost = NULL;

	min_x = details->kbd_current->x;

	while (grid_y < grid->height) {
		while (grid_x < grid->width) {
			GList *p;

			for (p = e[grid_x]; p != NULL; p = p->next) {
				GnomeIconContainerIcon *icon;

				icon = p->data;
				if (icon == details->kbd_current
				    || icon->x < x
				    || icon->y < y)
					continue;

				if (icon->x >= min_x
				    && (nearmost == NULL
					|| icon->x < nearmost->x))
					nearmost = icon;
			}
 
			if (nearmost != NULL) {
				kbd_move_to (container, nearmost, event);
				return;
			}

			grid_x++;
			x += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
		}

		grid_x = 0;
		min_x = 0;
		x = 0;

		e += grid->alloc_width;
		grid_y++;
		y += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}
}

static void
kbd_down (GnomeIconContainer *container,
	  GdkEventKey *event)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *nearmost;
	GList **e;
	guint grid_x, grid_y;
	int x, y;

	details = container->details;
	grid = details->grid;

	if (details->kbd_current == NULL)
		return;

	world_to_grid (container, details->kbd_current->x, details->kbd_current->y,
		       &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	e = icon_grid_get_element_ptr (grid, grid_x, grid_y);
	nearmost = NULL;

	while (grid_y < grid->height) {
		GList *p;

		p = *e;

		for (; p != NULL; p = p->next) {
			GnomeIconContainerIcon *icon;

			icon = p->data;
			if (icon == details->kbd_current
			    || icon->x < x
			    || icon->y < y)
				continue;

			if (icon->y >= details->kbd_current->y
			    && (nearmost == NULL || icon->y < nearmost->y))
				nearmost = icon;
		}

		if (nearmost != NULL)
			break;

		e += grid->alloc_width;
		grid_y++;
		y += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}

	if (nearmost != NULL)
		kbd_move_to (container, nearmost, event);
}

static void
kbd_space (GnomeIconContainer *container,
	   GdkEventKey *event)
{
	if (container->details->kbd_current != NULL) {
		if (icon_set_selected (container, container->details->kbd_current, TRUE))
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
	}
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	GnomeIconContainer *container;
	int index;

	container = GNOME_ICON_CONTAINER (object);

	gnome_icon_container_dnd_fini (container);

	icon_grid_destroy (container->details->grid);
	g_hash_table_destroy (container->details->canvas_item_to_icon);
	unschedule_kbd_icon_visibility (container);
	
	if (container->details->rubberband_info.timer_id != 0)
		gtk_timeout_remove (container->details->rubberband_info.timer_id);
	if (container->details->rubberband_info.selection_rectangle != NULL)
		gtk_object_destroy (GTK_OBJECT (container->details->rubberband_info.selection_rectangle));

        if (container->details->idle_id != 0)
		gtk_idle_remove (container->details->idle_id);
	if (container->details->linger_selection_mode_timer_id != 0)
		gtk_timeout_remove (container->details->linger_selection_mode_timer_id);
        for (index = 0; index < NAUTILUS_N_ELEMENTS (container->details->label_font); index++)
        	if (container->details->label_font[index] != NULL)
                	gdk_font_unref(container->details->label_font[index]);
	
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
	GnomeIconContainer *container;
	GnomeIconContainerIconGrid *grid;
	guint visible_width, visible_height;

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	container = GNOME_ICON_CONTAINER (widget);
	grid = container->details->grid;

	world_to_grid (container,
		       allocation->width, 0,
		       &visible_width, &visible_height);

	if (visible_width == 0)
		visible_width = 1;

#if 0
	grid->visible_width = visible_width;
	grid->height = MAX(visible_height, grid->height);
	gnome_icon_container_relayout(container);
#elif 0
	if (visible_width > grid->width || visible_height > grid->height)
		icon_grid_resize (grid,
				  MAX (visible_width, grid->width),
				  MAX (visible_height, grid->height));
	icon_grid_resize(grid, visible_width, visible_height);
	icon_grid_set_visible_width (grid, visible_width);
#endif

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
	GnomeIconContainer *container;
	gboolean return_value;

	container = GNOME_ICON_CONTAINER (widget);
        container->details->button_down_time = event->time;
	
	/* Invoke the canvas event handler and see if an item picks up the event. */
	if (NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_press_event, (widget, event)))
		return TRUE;
	
	/* An item didn't take the press, so it's a background press. */
        
	/* Button 1 does rubber banding. */
	if (event->button == RUBBERBAND_BUTTON) {
		if (! button_event_modifies_selection (event)) {
			gboolean selection_changed;

			selection_changed = unselect_all (container);
			if (selection_changed)
				gtk_signal_emit (GTK_OBJECT (container),
						 signals[SELECTION_CHANGED]);
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
gnome_icon_container_almost_drag (GnomeIconContainer *container,
				  GdkEventButton *event)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	if (!button_event_modifies_selection (event)) {
		gboolean selection_changed;
		
		selection_changed
			= select_one_unselect_others (container,
						      details->drag_icon);
		
		if (selection_changed)
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
	}
	
	if (details->drag_icon != NULL) {
		int elapsed_time;
		
		set_kbd_current (container, details->drag_icon, TRUE);
		
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
start_stretching (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIcon *icon;
	int canvas_x, canvas_y;

	details = container->details;
	icon = details->stretch_icon;
	
	gnome_canvas_w2c (GNOME_CANVAS (container),
			  details->drag_x, details->drag_y,
			  &canvas_x, &canvas_y);
	
	/* Check if we hit the stretch handles. */
	if (!nautilus_icons_view_icon_item_get_hit_stretch_handle
	    (icon->item, canvas_x, canvas_y))
		return FALSE;

	/* Set up the dragging. */
	details->drag_action = DRAG_ACTION_STRETCH;
	details->stretch_start.pointer_x = canvas_x;
	details->stretch_start.pointer_y = canvas_y;
	gnome_canvas_w2c (GNOME_CANVAS (container),
			  icon->x, icon->y,
			  &details->stretch_start.icon_x,
			  &details->stretch_start.icon_y);
	details->stretch_start.icon_size = icon_get_size (container, icon);

	return TRUE;
}

static void
continue_stretching (GnomeIconContainer *container,
		     int window_x, int window_y)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIcon *icon;
	double world_x, world_y;
	StretchState stretch_state;

	details = container->details;
	icon = details->stretch_icon;

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
end_stretching (GnomeIconContainer *container,
		int window_x, int window_y)
{
	continue_stretching (container, window_x, window_y);
}

static gboolean
button_release_event (GtkWidget *widget,
		      GdkEventButton *event)
{
	GnomeIconContainer *container;
	GnomeIconContainerDetails *details;

	container = GNOME_ICON_CONTAINER (widget);
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
				gnome_icon_container_almost_drag (container, event);
			else
				gnome_icon_container_dnd_end_drag (container);
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
	GnomeIconContainer *container;
	GnomeIconContainerDetails *details;
	double world_x, world_y;

	container = GNOME_ICON_CONTAINER (widget);
	details = container->details;

	if (details->drag_button != 0) {
		switch (details->drag_action) {
		case DRAG_ACTION_MOVE_OR_COPY:
			if (details->drag_started)
				break;

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
				
				gnome_icon_container_dnd_begin_drag (container,
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
	GnomeIconContainer *container;

	if (NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, key_press_event, (widget, event)))
		return TRUE;

	container = GNOME_ICON_CONTAINER (widget);

	switch (event->keyval) {
	case GDK_Home:
		kbd_home (container, event);
		break;
	case GDK_End:
		kbd_end (container, event);
		break;
	case GDK_Left:
		kbd_left (container, event);
		break;
	case GDK_Up:
		kbd_up (container, event);
		break;
	case GDK_Right:
		kbd_right (container, event);
		break;
	case GDK_Down:
		kbd_down (container, event);
		break;
	case GDK_space:
		kbd_space (container, event);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}


/* Initialization.  */

static void
gnome_icon_container_initialize_class (GnomeIconContainerClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	/* GnomeIconContainer class.  */

	class->button_press = NULL;

	/* GtkObject class.  */

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	/* Signals.  */

	signals[SELECTION_CHANGED]
		= gtk_signal_new ("selection_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     selection_changed),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	signals[BUTTON_PRESS]
		= gtk_signal_new ("button_press",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     button_press),
				  gtk_marshal_BOOL__POINTER,
				  GTK_TYPE_BOOL, 1,
				  GTK_TYPE_GDK_EVENT);
	signals[ACTIVATE]
		= gtk_signal_new ("activate",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     activate),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
	signals[CONTEXT_CLICK_SELECTION]
		= gtk_signal_new ("context_click_selection",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     context_click_selection),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	signals[CONTEXT_CLICK_BACKGROUND]
		= gtk_signal_new ("context_click_background",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     context_click_background),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	signals[ICON_MOVED]
		= gtk_signal_new ("icon_moved",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     icon_moved),
				  gtk_marshal_NONE__POINTER_INT_INT,
				  GTK_TYPE_NONE, 3,
				  GTK_TYPE_POINTER,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT);

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

static void
gnome_icon_container_initialize (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;

	details = g_new0 (GnomeIconContainerDetails, 1);

	details->grid = icon_grid_new ();

	details->canvas_item_to_icon = g_hash_table_new (g_direct_hash,
							 g_direct_equal);

        details->zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;
 
 	/* font table - this isnt exactly proportional, but it looks better than computed */
        /* FIXME: read font from metadata */
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALLEST] = gdk_font_load("-bitstream-charter-medium-r-normal-*-8-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALLER] = gdk_font_load("-bitstream-charter-medium-r-normal-*-8-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_SMALL] = gdk_font_load("-bitstream-charter-medium-r-normal-*-10-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_STANDARD] = gdk_font_load("-bitstream-charter-medium-r-normal-*-12-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGE] = gdk_font_load("-bitstream-charter-medium-r-normal-*-14-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGER] = gdk_font_load("-bitstream-charter-medium-r-normal-*-16-*-*-*-*-*-*-*");
        details->label_font[NAUTILUS_ZOOM_LEVEL_LARGEST] = gdk_font_load("-bitstream-charter-medium-r-normal-*-18-*-*-*-*-*-*-*");

	/* FIXME: Read these from preferences. */
	details->linger_selection_mode = FALSE;
	details->single_click_mode = TRUE;

	container->details = details;

	/* Set up DnD.  */
	gnome_icon_container_dnd_init (container, stipple);

	/* Request update.  */
	add_idle (container);

	/* Make sure that we find out if the theme changes. */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "theme_changed",
					       gnome_icon_container_request_update_all,
					       GTK_OBJECT (container));
}


/* GnomeIconContainerIcon event handling.  */

/* Selection in linger selection mode.  */
static gboolean
linger_select_timeout_cb (gpointer data)
{
	GnomeIconContainer *container;
	GnomeIconContainerDetails *details;
	GnomeIconContainerIcon *icon;
	gboolean selection_changed;

	GDK_THREADS_ENTER ();

	container = GNOME_ICON_CONTAINER (data);
	details = container->details;
	icon = details->linger_selection_mode_icon;

	selection_changed = unselect_all (container);
	selection_changed |= icon_set_selected (container, icon, TRUE);

	set_kbd_current (container, icon, FALSE);
	make_icon_visible (container, icon);

	/* FIXME: Am I allowed to do this between `GDK_THREADS_ENTER()' and
           `GDK_THREADS_LEAVE()'?  */
	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);

	GDK_THREADS_LEAVE ();

	return FALSE;
}

/* Conceptually, pressing button 1 together with CTRL or SHIFT toggles
 * selection of a single icon without affecting the other icons;
 * without CTRL or SHIFT, it selects a single icon and un-selects all
 * the other icons.  But in this latter case, the de-selection should
 * only happen when the button is released if the icon is already
 * selected, because the user might select multiple icons and drag all
 * of them by doing a simple click-drag.
*/
static gboolean
handle_icon_button_press (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventButton *event)
{
	GnomeIconContainerDetails *details;

	if (event->button != DRAG_BUTTON && event->button != CONTEXTUAL_MENU_BUTTON)
	{
		return TRUE;
	}

	details = container->details;

	if (event->button == DRAG_BUTTON)
	{
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

static gboolean
handle_icon_enter_notify (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventMotion *motion)
{
	GnomeIconContainerDetails *details;

	details = container->details;
	if (! details->linger_selection_mode)
		return FALSE;

	if (details->linger_selection_mode_timer_id != 0)
		gtk_timeout_remove (details->linger_selection_mode_timer_id);

	details->linger_selection_mode_timer_id
		= gtk_timeout_add (LINGER_SELECTION_MODE_TIMEOUT,
				   linger_select_timeout_cb, container);

	details->linger_selection_mode_icon = icon;

	return TRUE;
}

static gboolean
handle_icon_leave_notify (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventMotion *motion)
{
	GnomeIconContainerDetails *details;

	details = container->details;
	if (! details->linger_selection_mode)
		return FALSE;

	if (details->linger_selection_mode_timer_id != 0)
		gtk_timeout_remove (details->linger_selection_mode_timer_id);

	return TRUE;
}

static int
item_event_cb (GnomeCanvasItem *item,
	       GdkEvent *event,
	       gpointer data)
{
	GnomeIconContainer *container;
	GnomeIconContainerDetails *details;
	GnomeIconContainerIcon *icon;

	container = GNOME_ICON_CONTAINER (data);
	details = container->details;

	icon = g_hash_table_lookup (details->canvas_item_to_icon, item);
	g_return_val_if_fail (icon != NULL, FALSE);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
		return handle_icon_button_press (container, icon, &event->button);
	case GDK_ENTER_NOTIFY:
		return handle_icon_enter_notify (container, icon, &event->motion);
	case GDK_LEAVE_NOTIFY:
		return handle_icon_leave_notify (container, icon, &event->motion);
	default:
		return FALSE;
	}
}

GtkWidget *
gnome_icon_container_new (NautilusIconsController *controller)
{
	GtkWidget *new;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	new = gtk_type_new (gnome_icon_container_get_type ());

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	GNOME_ICON_CONTAINER (new)->details->controller = controller;

	return new;
}

void
gnome_icon_container_clear (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GList *p;

	g_return_if_fail (container != NULL);

	details = container->details;

	set_kbd_current (container, NULL, FALSE);
	details->stretch_icon = NULL;

	for (p = details->icons; p != NULL; p = p->next)
		icon_destroy (p->data);
	g_list_free (details->icons);
	details->icons = NULL;
	details->num_icons = 0;

	icon_grid_clear (details->grid);
}

static void
setup_icon_in_container (GnomeIconContainer *container,
			 GnomeIconContainerIcon *icon)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	details->icons = g_list_prepend (details->icons, icon);
	details->num_icons++;

	g_hash_table_insert (details->canvas_item_to_icon, icon->item, icon);
	icon_show (icon);

	gtk_signal_connect (GTK_OBJECT (icon->item), "event",
			    GTK_SIGNAL_FUNC (item_event_cb), container);
}

static void 
update_icon (GnomeIconContainer *container, GnomeIconContainerIcon *icon)
{
	GnomeIconContainerDetails *details;
	NautilusScalableIcon *scalable_icon;
	guint icon_size;
	GdkPixbuf *pixbuf;
	GList *emblem_icons, *emblem_pixbufs, *p;
	char *label;
	char *contents_as_text;
	GdkFont *font;

	details = container->details;

	/* Get the icons. */
	scalable_icon = nautilus_icons_controller_get_icon_image
		(details->controller, icon->data, &emblem_icons);

	/* Get the corresponding pixbufs for this size. */
	icon_size = icon_get_size (container, icon);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(scalable_icon, icon_size);
	emblem_pixbufs = NULL;
	for (p = emblem_icons; p != NULL; p = p->next) {
		emblem_pixbufs = g_list_prepend
			(emblem_pixbufs, nautilus_icon_factory_get_pixbuf_for_icon (p->data, icon_size));
	}

	/* Let the icons go. */
	nautilus_scalable_icon_unref (scalable_icon);
	nautilus_scalable_icon_list_free (emblem_icons);

	label = nautilus_icons_controller_get_icon_text
		(details->controller, icon->data);

	font = details->label_font[details->zoom_level];
        
	contents_as_text = nautilus_icons_controller_get_icon_property  
		(details->controller, icon->data, "contents_as_text");
	
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->item),
			       "pixbuf", pixbuf,
			       "text", label,
			       "font", font,
			       "text_source", contents_as_text,
			       NULL);
	nautilus_icons_view_icon_item_set_emblems (icon->item, emblem_pixbufs);

	/* Let the pixbufs go. */
	gdk_pixbuf_unref (pixbuf);
	nautilus_gdk_pixbuf_list_free (emblem_pixbufs);

	g_free (label);
        g_free (contents_as_text);
}

void
gnome_icon_container_add (GnomeIconContainer *container,
			  NautilusControllerIcon *data,
			  int x, int y)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIcon *new_icon;
	guint grid_x, grid_y;

	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	details = container->details;

	new_icon = icon_new (container, data);
	icon_set_position (new_icon, x, y);

	world_to_grid (container, x, y, &grid_x, &grid_y);
	icon_grid_add (details->grid, new_icon, grid_x, grid_y);

	if (x % GNOME_ICON_CONTAINER_CELL_WIDTH (container) > 0)
		icon_grid_add (details->grid, new_icon, grid_x + 1, grid_y);
	if (y % GNOME_ICON_CONTAINER_CELL_HEIGHT (container) > 0)
		icon_grid_add (details->grid, new_icon, grid_x, grid_y + 1);
	if (x % GNOME_ICON_CONTAINER_CELL_WIDTH (container) > 0
	    && y % GNOME_ICON_CONTAINER_CELL_HEIGHT (container) > 0)
		icon_grid_add (details->grid, new_icon, grid_x + 1, grid_y + 1);

	setup_icon_in_container (container, new_icon);

	add_idle (container);
}

/**
 * gnome_icon_container_add_auto:
 * @container: A GnomeIconContainer
 * @data: Icon from the controller.
 * 
 * Add @image with caption @text and data @data to @container, in the first
 * empty spot available.
 **/
void
gnome_icon_container_add_auto (GnomeIconContainer *container,
			       NautilusControllerIcon *data)
{
	GnomeIconContainerIcon *new_icon;
	guint grid_x, grid_y;
	int x, y;

	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	new_icon = icon_new (container, data);

	icon_grid_add_auto (container->details->grid, new_icon, &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	icon_set_position (new_icon, x, y);

	setup_icon_in_container (container, new_icon);

	add_idle (container);
}

/* zooming */

int
gnome_icon_container_get_zoom_level(GnomeIconContainer *container)
{
        return container->details->zoom_level;
}

void
gnome_icon_container_set_zoom_level(GnomeIconContainer *container, int new_level)
{
	GnomeIconContainerDetails *details;
        int pinned_level;
	double pixels_per_unit;

	details = container->details;

	pinned_level = new_level;
        if (pinned_level < NAUTILUS_ZOOM_LEVEL_SMALLEST)
		pinned_level = NAUTILUS_ZOOM_LEVEL_SMALLEST;
        else if (pinned_level > NAUTILUS_ZOOM_LEVEL_LARGEST)
        	pinned_level = NAUTILUS_ZOOM_LEVEL_LARGEST;
	
        if (pinned_level == details->zoom_level)
		return;
	
	details->zoom_level = pinned_level;
	
	pixels_per_unit = (double) nautilus_get_icon_size_for_zoom_level (pinned_level)
		/ NAUTILUS_ICON_SIZE_STANDARD;
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (container), pixels_per_unit);

	gnome_icon_container_request_update_all (container);
}

/**
 * gnome_icon_container_request_update_all:
 * For each icon, synchronizes the displayed information (image, text) with the
 * information from the model.
 * 
 * @container: An icon container.
 **/
void
gnome_icon_container_request_update_all (GnomeIconContainer *container)
{
	GList *p;

	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	for (p = container->details->icons; p != NULL; p = p->next)
		update_icon (container, p->data);
}



/**
 * gnome_icon_container_relayout:
 * @container: An icon container.
 * 
 * Relayout the icons in @container according to the allocation we are given.
 * This is done by just collecting icons from top to bottom, from left to
 * right, and tiling them in the same direction.  The tiling is done in such a
 * way that no horizontal scrolling is needed to see all the icons.
 **/
void
gnome_icon_container_relayout (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *old_grid, *new_grid;
	GList **sp, **dp;
	guint i, j;
	guint dx, dy;
	guint sx, sy;
	guint cols;
	guint lines;

	g_return_if_fail (container != NULL);

	details = container->details;
	old_grid = details->grid;

	g_return_if_fail (old_grid->visible_width > 0);

	prepare_for_layout (container);

	new_grid = icon_grid_new ();

	if (details->num_icons % old_grid->visible_width != 0)
		icon_grid_resize (new_grid,
				  old_grid->visible_width,
				  (details->num_icons
				   / old_grid->visible_width) + 1);
	else
		icon_grid_resize (new_grid,
				  old_grid->visible_width,
				  details->num_icons / old_grid->visible_width);

	icon_grid_set_visible_width (new_grid, old_grid->visible_width);

	sp = old_grid->elems;
	dp = new_grid->elems;
	sx = sy = 0;
	dx = dy = 0;
	cols = lines = 0;
	for (i = 0; i < old_grid->height; i++) {
		for (j = 0; j < old_grid->width; j++) {
			GList *p;

			/* Make sure the icons are sorted by increasing X
			   position.  */
			sp[j] = g_list_sort (sp[j],
					     icon_grid_cell_compare_by_x);

			for (p = sp[j]; p != NULL; p = p->next) {
				GnomeIconContainerIcon *icon;

				icon = p->data;

				/* Make sure icons are not moved twice, and
				   ignore icons whose upper left corner is not
				   in this cell, unless the icon is partly
				   outside the container.  */
				if (icon->layout_done
				    || (icon->x >= 0 && icon->x < sx)
				    || (icon->y >= 0 && icon->y < sy))
					continue;

				dp[cols] = g_list_alloc ();
				dp[cols]->data = icon;

				icon_set_position (icon, dx, dy);

				icon->layout_done = TRUE;

				if (++cols == new_grid->visible_width) {
					cols = 0, lines++;
					dx = 0, dy += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
					dp += new_grid->alloc_width;
				} else {
					dx += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
				}
			}

			sx += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
		}

		sx = 0, sy += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

		sp += old_grid->alloc_width;
	}

	if (cols < new_grid->visible_width && lines < new_grid->height) {
		new_grid->first_free_x = cols;
		new_grid->first_free_y = lines;
	} else {
		new_grid->first_free_x = -1;
		new_grid->first_free_y = -1;
	}

	icon_grid_destroy (details->grid);
	details->grid = new_grid;

	if (details->kbd_current != NULL)
		set_kbd_current (container, details->kbd_current, FALSE);

	add_idle (container);
}


/**
 * gnome_icon_container_line_up:
 * @container: An icon container.
 * 
 * Line up icons in @container.
 **/
void
gnome_icon_container_line_up (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIconGrid *new_grid;
	GList **p, **q;
	guint new_grid_width;
	guint i, j, k, m;
	int x, y, dx;

	g_return_if_fail (container != NULL);

	details = container->details;
	grid = details->grid;

	/* Mark all icons as "not moved yet".  */

	prepare_for_layout (container);

	/* Calculate the width for the resulting new grid.  This is the maximum
           width across all the lines.  */

	new_grid_width = 0;
	p = grid->elems;
	x = y = 0;
	for (i = 0; i < grid->height; i++) {
		guint line_width;

		line_width = grid->width;
		for (j = 0; j < grid->width; j++) {
			GList *e;
			guint count;

			count = 0;
			for (e = p[j]; e != NULL; e = e->next) {
				GnomeIconContainerIcon *icon;

				icon = e->data;
				if (icon->x >= x && icon->y >= y)
					count++;
			}

			if (count > 1)
				new_grid_width += count - 1;

			x += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
		}

		new_grid_width = MAX (new_grid_width, line_width);
		p += grid->alloc_width;

		y += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
		x = 0;
	}

	/* Create the new grid.  */

	new_grid = icon_grid_new ();
	icon_grid_resize (new_grid, new_grid_width, grid->height);
        icon_grid_set_visible_width (new_grid, grid->visible_width);

	/* Allocate the icons in the new grid, one per cell.  */

	p = grid->elems;
	q = new_grid->elems;
	k = 0;
	x = y = dx = 0;
	for (i = 0; i < grid->height; i++) {
		m = 0;
		for (j = 0; j < grid->width; j++) {
			GList *e;
			guint count;

			/* Make sure the icons are sorted by increasing X
                           position.  */
			p[j] = g_list_sort
				(p[j], icon_grid_cell_compare_by_x);

			count = 0;
			for (e = p[j]; e != NULL; e = e->next) {
				GnomeIconContainerIcon *icon;

				icon = e->data;
					
				/* Make sure icons are not moved twice, and
				   ignore icons whose upper left corner is not
				   in this cell, unless the icon is partly
				   outside the container.  */
				if (icon->layout_done
				    || (icon->x >= 0 && icon->x < x)
				    || (icon->y >= 0 && icon->y < y))
					continue;

				icon_set_position (icon, dx, y);
				icon->layout_done = TRUE;

				q[k] = g_list_alloc ();
				q[k]->data = icon;
				dx += GNOME_ICON_CONTAINER_CELL_WIDTH (container);

				k++;

				if (count > 0)
					m++;

				count++;
			}

			if (count == 0) {
				if (m > 0) {
					m--;
				} else {
					k++;
					dx += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
				}
			}
		}

		x += GNOME_ICON_CONTAINER_CELL_WIDTH (container);

		p += grid->alloc_width;

		q += new_grid->alloc_width;
		k = 0;

		y += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
		x = 0;

		dx = 0;
	}

	/* Done: use the new grid.  */

	icon_grid_destroy (details->grid);
	details->grid = new_grid;

	/* Update the keyboard selection indicator.  */
	if (details->kbd_current != NULL)
		set_kbd_current (container, details->kbd_current, FALSE);

	add_idle (container);
}



/**
 * gnome_icon_container_get_selection:
 * @container: An icon container.
 * 
 * Get a list of the icons currently selected in @container.
 * 
 * Return value: A GList of the programmer-specified data associated to each
 * selected icon, or NULL if no icon is selected.  The caller is expected to
 * free the list when it is not needed anymore.
 **/
GList *
gnome_icon_container_get_selection (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GList *list, *p;

	g_return_val_if_fail (container != NULL, FALSE);

	details = container->details;

	list = NULL;
	for (p = details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		if (icon->is_selected)
			list = g_list_prepend (list, icon->data);
	}

	return list;
}

/**
 * gnome_icon_container_select_all:
 * @container: An icon container widget.
 * 
 * Select all the icons in @container at once.
 **/
void
gnome_icon_container_select_all (GnomeIconContainer *container)
{
	gboolean selection_changed;
	GList *p;

	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	selection_changed = FALSE;
	for (p = container->details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		selection_changed |= icon_set_selected (container, icon, TRUE);
	}

	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
}

/**
 * gnome_icon_container_select_list_unselect_others:
 * @container: An icon container widget.
 * @list: A list of BonoboContainerIcons.
 * 
 * Select only the icons in the list, deselect all others.
 **/
void
gnome_icon_container_select_list_unselect_others (GnomeIconContainer *container,
						  GList *icons)
{
	gboolean selection_changed;
	GList *p;

	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	/* To avoid an N^2 algorithm, we could put the icons into a hash
	   table, but this should be OK for now.
	*/

	selection_changed = FALSE;
	for (p = container->details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		selection_changed |= icon_set_selected
			(container, icon, g_list_find (icons, icon) != NULL);
	}

	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
}

/**
 * gnome_icon_container_unselect_all:
 * @container: An icon container widget.
 * 
 * Deselect all the icons in @container.
 **/
void
gnome_icon_container_unselect_all (GnomeIconContainer *container)
{
	if (unselect_all (container))
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
}

/**
 * gnome_icon_container_get_icon_by_uri:
 * @container: An icon container widget.
 * @uri: The uri of an icon to find.
 * 
 * Locate an icon, given the URI. The URI must match exactly.
 * Later we may have to have some way of figuring out if the
 * URI specifies the same object that does not require an exact match.
 **/
GnomeIconContainerIcon *
gnome_icon_container_get_icon_by_uri (GnomeIconContainer *container,
				      const char *uri)
{
	GnomeIconContainerDetails *details;
	GList *p;

	/* Eventually, we must avoid searching the entire icon list,
	   but it's OK for now.
	   A hash table mapping uri to icon is one possibility.
	*/

	details = container->details;

	for (p = details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;
		char *icon_uri;
		gboolean is_match;

		icon = p->data;

		icon_uri = nautilus_icons_controller_get_icon_uri
			(details->controller, icon->data);
		is_match = strcmp (uri, icon_uri) == 0;
		g_free (icon_uri);

		if (is_match)
			return icon;
	}

	return NULL;
}

static GnomeIconContainerIcon *
get_first_selected_icon (GnomeIconContainer *container)
{
	GList *p;
	GnomeIconContainerIcon *icon;

	/* Find the first selected icon. */
	icon = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->is_selected) {
			return icon;
		}
	}
	return NULL;
}

/**
 * gnome_icon_container_show_stretch_handles:
 * @container: An icon container widget.
 * 
 * Makes stretch handles visible on the first selected icon.
 **/
void
gnome_icon_container_show_stretch_handles (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIcon *icon;

	icon = get_first_selected_icon (container);
	if (icon == NULL)
		return;

	/* Check if it already has stretch handles. */
	details = container->details;
	if (details->stretch_icon == icon)
		return;

	/* Get rid of the existing stretch handles and put them on the new icon. */
	if (details->stretch_icon != NULL)
		nautilus_icons_view_icon_item_set_show_stretch_handles
			(details->stretch_icon->item, FALSE);
	nautilus_icons_view_icon_item_set_show_stretch_handles (icon->item, TRUE);
	details->stretch_icon = icon;
}

/**
 * gnome_icon_container_has_stretch_handles
 * @container: An icon container widget.
 * 
 * Returns true if the first selected item has stretch handles.
 **/
gboolean
gnome_icon_container_has_stretch_handles (GnomeIconContainer *container)
{
	GnomeIconContainerIcon *icon;

	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return FALSE;
	}

	return icon == container->details->stretch_icon;
}

/**
 * gnome_icon_container_is_stretched
 * @container: An icon container widget.
 * 
 * Returns true if the first selected item is stretched to a size other than 1.0.
 **/
gboolean
gnome_icon_container_is_stretched (GnomeIconContainer *container)
{
	GnomeIconContainerIcon *icon;

	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return FALSE;
	}

	return icon->scale != 1.0;
}

/**
 * gnome_icon_container_unstretch
 * @container: An icon container widget.
 * 
 * Gets rid of any icon stretching.
 **/
void
gnome_icon_container_unstretch (GnomeIconContainer *container)
{
	GnomeIconContainerIcon *icon;

	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return;
	}

	icon->scale = 1.0;
	update_icon (container, icon);
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
		x_stretch = -x_stretch;
	}
	if (bottom) {
		y_stretch = -y_stretch;
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
nautilus_self_check_gnome_icon_container (void)
{
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_compute_stretch (0, 0, 12, 0, 0, 0, 0), "0,0:12");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_compute_stretch (0, 0, 12, 12, 12, 13, 13), "0,0:13");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_compute_stretch (0, 0, 12, 12, 12, 13, 12), "0,0:12");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_compute_stretch (100, 100, 64, 105, 105, 40, 40), "35,35:129");
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
