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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gnome-icon-container.h"

#include "gnome-icon-container-private.h"
#include "gnome-icon-container-dnd.h"
#include "nautilus-icons-view-icon-item.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

#include "nautilus-gtk-macros.h"

/* Interval for updating the rubberband selection, in milliseconds.  */
#define RUBBERBAND_TIMEOUT_INTERVAL 10

/* Timeout for making the icon currently selected for keyboard operation
   visible.  FIXME: This *must* be higher than the double-click time in GDK,
   but there is no way to access its value from outside.
*/
#define KBD_ICON_VISIBILITY_TIMEOUT 300

/* Timeout for selecting an icon in "linger-select" mode (i.e. by just placing the
   pointer over the icon, without pressing any button).
*/
#define LINGER_SELECTION_MODE_TIMEOUT 800

/* maximum amount of milliseconds the mouse button is allowed to stay down and still be considered a click */
#define MAX_CLICK_TIME 1500

static void gnome_icon_container_initialize_class (GnomeIconContainerClass *class);
static void gnome_icon_container_initialize (GnomeIconContainer *container);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (GnomeIconContainer, gnome_icon_container, GNOME_TYPE_CANVAS)



/* The GnomeIconContainer signals.  */
enum _GnomeIconContainerSignalNumber {
	SELECTION_CHANGED,
	BUTTON_PRESS,
	ACTIVATE,
	CONTEXT_CLICK_ICON,
	CONTEXT_CLICK_BACKGROUND,
	ICON_MOVED,
	LAST_SIGNAL
};
typedef enum _GnomeIconContainerSignalNumber GnomeIconContainerSignalNumber;
static guint signals[LAST_SIGNAL] = { 0 };

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
	GnomeCanvas *canvas;
	GnomeIconContainerDetails *details;
	GnomeIconContainerIcon *new;

	canvas = GNOME_CANVAS (container);
	details = container->details;

	new = g_new0 (GnomeIconContainerIcon, 1);
	
	new->layout_done = TRUE;

	new->data = data;

        new->item = NULL;

	new->width = GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	new->height = GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

	return new;
}

static GnomeIconContainerIcon *
icon_new_pixbuf (GnomeIconContainer *container,
		 NautilusControllerIcon *data)
{
	GnomeIconContainerDetails *details;
	char *name;
	GnomeIconContainerIcon *new;
	GdkPixbuf *image;
        GnomeCanvas* canvas = GNOME_CANVAS(container);
        
	details = container->details;

	new = icon_new (container, data);

	image = nautilus_icons_controller_get_icon_image (details->controller, data);
	name = nautilus_icons_controller_get_icon_name (details->controller, data);
	
        new->item = gnome_canvas_item_new
			(GNOME_CANVAS_GROUP (canvas->root),
			nautilus_icons_view_icon_item_get_type (),
	 		"pixbuf", image,    
			"label", name,
			"x", (gdouble) 0,
			"y", (gdouble) 0,	 
                        NULL);
	g_free (name);
        
	return new;
}

static void
icon_position (GnomeIconContainerIcon *icon,
	       GnomeIconContainer *container,
	       gdouble x, gdouble y)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	icon->x = x;
	icon->y = y;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->item),
			       "x", (gdouble) icon->x,
			       "y", (gdouble) icon->y,
			       NULL);
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
icon_toggle_selected (GnomeIconContainerIcon *icon)
{
	icon->is_selected = !icon->is_selected;
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->item),
			       "selected", (gboolean) icon->is_selected,
			       NULL);
}

/* Select an icon. Return TRUE if selection has changed. */
static gboolean
icon_select (GnomeIconContainerIcon *icon,
	     gboolean select)
{
	if (select == icon->is_selected)
		return FALSE;

	icon_toggle_selected (icon);
	return TRUE;
}

static gboolean
icon_is_in_region (GnomeIconContainerIcon *icon,
		   gint x1, gint y1,
		   gint x2, gint y2)
{
	gint icon_x2, icon_y2;

	icon_x2 = icon->x + icon->width;
	icon_y2 = icon->y + icon->height;

	if (x1 == x2 && y1 == y2)
		return FALSE;

	if (x1 < icon_x2 && x2 >= icon->x && y1 < icon_y2 && y2 >= icon->y)
		return TRUE;
	else
		return FALSE;
}

static void
icon_get_text_bounding_box (GnomeIconContainerIcon *icon,
			    guint *x1_return, guint *y1_return,
			    guint *x2_return, guint *y2_return)
{

        /* FIXME: need to ask item for the text bounds */
	
        /*  
	double x1, y1, x2, y2;
        gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (icon->text_item),        
	*x1_return = icon->x + x1;
	*y1_return = icon->y + y1;
	*x2_return = icon->x + x2;
	*y2_return = icon->y + y2;
        */
        
        *x1_return = 0;
        *y1_return = 0;
        *x2_return = 0;
        *y2_return = 0;
        
}

static void
icon_get_bounding_box (GnomeIconContainerIcon *icon,
		       guint *x1_return, guint *y1_return,
		       guint *x2_return, guint *y2_return)
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

   Instead of re-allocating the grid from scratch and copying the values, we
   should just link grid chunks horizontally and vertically in lists;
   i.e. use a hybrid list/array representation.  */
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

static gint
icon_grid_cell_compare_by_x (gconstpointer ap,
			     gconstpointer bp)
{
	GnomeIconContainerIcon *a, *b;

	a = (GnomeIconContainerIcon *) ap;
	b = (GnomeIconContainerIcon *) bp;

	return (gint) a->x - b->x;
}


static void
world_to_grid (GnomeIconContainer *container,
	       gint world_x, gint world_y,
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
	       gint *world_x_return, gint *world_y_return)
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
	gint delta_x, gint delta_y)
{
	GnomeIconContainerDetails *details;
	GtkAdjustment *hadj, *vadj;
	GtkAllocation *allocation;
	gfloat vnew, hnew;
	gfloat hmax, vmax;

	details = container->details;

	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	allocation = &GTK_WIDGET (container)->allocation;

	if (container->details->width > allocation->width)
		hmax = (gfloat) (container->details->width - allocation->width);
	else
		hmax = 0.0;

	if (container->details->height > allocation->height)
		vmax = (gfloat) (container->details->height - allocation->height);
	else
		vmax = 0.0;

	hnew = CLAMP (hadj->value + (gfloat) delta_x, 0.0, hmax);
	vnew = CLAMP (vadj->value + (gfloat) delta_y, 0.0, vmax);

	if (hnew != hadj->value) {
		hadj->value = hnew;
		gtk_signal_emit_by_name (GTK_OBJECT (hadj), "value_changed");
	}
	if (vnew != vadj->value) {
		vadj->value = vnew;
		gtk_signal_emit_by_name (GTK_OBJECT (vadj), "value_changed");
	}
}

static void
make_icon_visible (GnomeIconContainer *container,
		   GnomeIconContainerIcon *icon)
{
	GnomeIconContainerDetails *details;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;
	gint x1, y1, x2, y2;

	details = container->details;
	allocation = &GTK_WIDGET (container)->allocation;

	if (details->height < allocation->height
	    && details->width < allocation->width)
		return;

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

static gint
kbd_icon_visibility_timeout_cb (gpointer data)
{
	GnomeIconContainer *container;

	GDK_THREADS_ENTER ();

	container = GNOME_ICON_CONTAINER (data);

	if (container->details->kbd_current != NULL)
		make_icon_visible (container, container->details->kbd_current);
	container->details->kbd_icon_visibility_timer_tag = -1;

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static void
unschedule_kbd_icon_visibility (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	if (details->kbd_icon_visibility_timer_tag != -1)
		gtk_timeout_remove (details->kbd_icon_visibility_timer_tag);
}

static void
schedule_kbd_icon_visibility (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	unschedule_kbd_icon_visibility (container);

	details->kbd_icon_visibility_timer_tag
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
	gint i, j;

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
	gint x1, y1, x2, y2;

	details = container->details;

	details->kbd_current = icon;

	if (details->kbd_current == NULL)
		return;
	
	icon_get_text_bounding_box (icon, &x1, &y1, &x2, &y2);

	gnome_canvas_item_set (details->kbd_navigation_rectangle,
			       "x1", (gdouble) x1 - 1,
			       "y1", (gdouble) y1 - 1,
			       "x2", (gdouble) x2,
			       "y2", (gdouble) y2,
			       NULL);
	gnome_canvas_item_show (details->kbd_navigation_rectangle);

	icon_raise (icon);
	gnome_canvas_item_raise_to_top (details->kbd_navigation_rectangle);

	if (schedule_visibility)
		schedule_kbd_icon_visibility (container);
	else
		unschedule_kbd_icon_visibility (container);
}

static void
unset_kbd_current (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	details->kbd_current = NULL;
	gnome_canvas_item_hide (details->kbd_navigation_rectangle);

	unschedule_kbd_icon_visibility (container);
}


/* Idle operation handler.  */

static void
set_scroll_region (GnomeIconContainer *container)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerIconGrid *grid;
	GtkAllocation *allocation;
	GtkAdjustment *vadj, *hadj;
	gdouble x1, y1, x2, y2;
	guint scroll_width, scroll_height;

	details = container->details;
	grid = details->grid;
	allocation = &(GTK_WIDGET (container)->allocation);
	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	/* FIXME: We can do this more efficiently.  */
	gnome_canvas_item_get_bounds (GNOME_CANVAS (container)->root,
				      &x1, &y1, &x2, &y2);

	details->width = x2 + GNOME_ICON_CONTAINER_CELL_SPACING (container);
	details->height = y2 + GNOME_ICON_CONTAINER_CELL_SPACING (container);

	scroll_width = MAX (details->width, allocation->width);
	scroll_height = MAX (details->height, allocation->height);

	scroll_width--;
	scroll_height--;

	gnome_canvas_set_scroll_region (GNOME_CANVAS (container),
					0.0, 0.0,
					(gdouble) scroll_width,
					(gdouble) scroll_height);

	if (details->width <= allocation->width)
		gtk_adjustment_set_value (hadj, 0.0);
	if (details->height <= allocation->height)
		gtk_adjustment_set_value (vadj, 0.0);
}

static gint
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
		selection_changed |= icon_select (icon, icon == icon_to_select);
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
	gint old_x, old_y;
	guint old_grid_x, old_grid_y;
	gint old_x_offset, old_y_offset;
	guint new_grid_x, new_grid_y;
	gint new_x_offset, new_y_offset;

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

	icon_position (icon, container, x, y);
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
rubberband_select_in_cell (GList *cell,
			   gdouble curr_x1, gdouble curr_y1,
			   gdouble curr_x2, gdouble curr_y2,
			   gdouble prev_x1, gdouble prev_y1,
			   gdouble prev_x2, gdouble prev_y2)
{
	GList *p;
	gboolean selection_changed;

	selection_changed = FALSE;

	for (p = cell; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;
		gboolean in_curr_region;
		gboolean in_prev_region;

		icon = p->data;

		in_curr_region = icon_is_in_region (icon,
						    curr_x1, curr_y1,
						    curr_x2, curr_y2);

		in_prev_region = icon_is_in_region (icon,
						    prev_x1, prev_y1,
						    prev_x2, prev_y2);

		if (in_curr_region && ! in_prev_region)
			selection_changed |= icon_select (icon,
							  !icon->was_selected_before_rubberband);
		else if (in_prev_region && ! in_curr_region)
			selection_changed |= icon_select (icon,
							  icon->was_selected_before_rubberband);
	}

	return selection_changed;
}

static void
rubberband_select (GnomeIconContainer *container,
		   gdouble curr_x1, gdouble curr_y1,
		   gdouble curr_x2, gdouble curr_y2,
		   gdouble prev_x1, gdouble prev_y1,
		   gdouble prev_x2, gdouble prev_y2)
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
			if (rubberband_select_in_cell (p[j],
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

static gint
rubberband_timeout_cb (gpointer data)
{
	GnomeIconContainer *container;
	GtkWidget *widget;
	GnomeIconContainerRubberbandInfo *rinfo;
	gint x, y;
	gdouble x1, y1, x2, y2;
	gdouble world_x, world_y;
	gint x_scroll, y_scroll;

	GDK_THREADS_ENTER ();

	widget = GTK_WIDGET (data);
	container = GNOME_ICON_CONTAINER (data);
	rinfo = &container->details->rubberband_info;

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
	    && rinfo->prev_x == x && rinfo->prev_y == y) {
		GDK_THREADS_LEAVE ();
		return TRUE;
	}

	scroll (container, x_scroll, y_scroll);

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);

	if (world_x < rinfo->start_x) {
		x1 = world_x;
		x2 = rinfo->start_x;
	} else {
		x1 = rinfo->start_x;
		x2 = world_x;
	}

	if (world_y < rinfo->start_y) {
		y1 = world_y;
		y2 = rinfo->start_y;
	} else {
		y1 = rinfo->start_y;
		y2 = world_y;
	}

	gnome_canvas_item_set (rinfo->selection_rectangle,
			       "x1", (gdouble) x1,
			       "y1", (gdouble) y1,
			       "x2", (gdouble) x2,
			       "y2", (gdouble) y2,
			       NULL);

	rubberband_select (container,
			   x1, y1, x2, y2,
			   rinfo->prev_x1, rinfo->prev_y1,
			   rinfo->prev_x2, rinfo->prev_y2);

	rinfo->prev_x = x;
	rinfo->prev_y = y;
	rinfo->prev_x1 = x1;
	rinfo->prev_y1 = y1;
	rinfo->prev_x2 = x2;
	rinfo->prev_y2 = y2;	

	GDK_THREADS_LEAVE ();

	return TRUE;
}

static void
start_rubberbanding (GnomeIconContainer *container,
		     GdkEventButton *event)
{
	GnomeIconContainerDetails *details;
	GnomeIconContainerRubberbandInfo *rinfo;
	GList *p;

	details = container->details;
	rinfo = &details->rubberband_info;

	for (p = details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		icon->was_selected_before_rubberband = icon->is_selected;
	}

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      event->x, event->y,
				      &rinfo->start_x, &rinfo->start_y);

	rinfo->selection_rectangle
		= gnome_canvas_item_new (gnome_canvas_root
					 (GNOME_CANVAS (container)),
					 gnome_canvas_rect_get_type (),
					 "x1", rinfo->start_x,
					 "y1", rinfo->start_y,
					 "x2", rinfo->start_x,
					 "y2", rinfo->start_y,
					 "outline_color", "black",
					 "outline_stipple", stipple,
					 "width_pixels", 2,
					 NULL);

	rinfo->prev_x = rinfo->prev_x1 = rinfo->prev_x2 = event->x;
	rinfo->prev_y = rinfo->prev_y1 = rinfo->prev_y2 = event->y;

	rinfo->active = TRUE;

	rinfo->timer_tag = gtk_timeout_add (RUBBERBAND_TIMEOUT_INTERVAL,
					    rubberband_timeout_cb,
					    container);

	gnome_canvas_item_grab (rinfo->selection_rectangle,
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK),
				NULL, event->time);
}

static void
stop_rubberbanding (GnomeIconContainer *container,
		    GdkEventButton *event)
{
	GnomeIconContainerRubberbandInfo *rinfo;

	rinfo = &container->details->rubberband_info;

	gtk_timeout_remove (rinfo->timer_tag);
	rinfo->active = FALSE;

	gnome_canvas_item_ungrab (rinfo->selection_rectangle, event->time);
	gtk_object_destroy (GTK_OBJECT (rinfo->selection_rectangle));
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
		selection_changed |= icon_select (icon, TRUE);

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
	gint x, y;
	gint max_x;

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
	gint x, y;

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
	gint x, y;
	gint min_x;

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
	gint x, y;

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
		if (icon_select (container->details->kbd_current, TRUE))
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
	}
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	GnomeIconContainer *container;

	container = GNOME_ICON_CONTAINER (object);

	gnome_icon_container_dnd_fini (container);

	icon_grid_destroy (container->details->grid);
	g_hash_table_destroy (container->details->canvas_item_to_icon);
	unschedule_kbd_icon_visibility (container);
	if (container->details->idle_id != 0)
		gtk_idle_remove (container->details->idle_id);
	if (container->details->linger_selection_mode_timer_tag != -1)
		gtk_timeout_remove (container->details->linger_selection_mode_timer_tag);
        if (container->details->label_font)
                gdk_font_unref(container->details->label_font);
                
	g_free (container->details);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate)
			(widget, allocation);

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
#else
	/*
	if (visible_width > grid->width || visible_height > grid->height)
		icon_grid_resize (grid,
				  MAX (visible_width, grid->width),
				  MAX (visible_height, grid->height));
	*/
	/* icon_grid_resize(grid, visible_width, visible_height); */
	/*	icon_grid_set_visible_width (grid, visible_width); */
#endif

	set_scroll_region (container);
}

static void
realize (GtkWidget *widget)
{
	GtkStyle *style;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

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
	gboolean return_value;
	GnomeIconContainer *container = GNOME_ICON_CONTAINER (widget);
        
        container->details->button_down_time = event->time;

	/* Invoke the canvas event handler and see if an item picks up the
           event.  */
	if ((* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event))
		return TRUE;
         
	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
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

	if (event->button == 3) {
		gtk_signal_emit (GTK_OBJECT (widget),
				 signals[CONTEXT_CLICK_BACKGROUND]);
		return TRUE;
	}

	gtk_signal_emit (GTK_OBJECT (widget), signals[BUTTON_PRESS], event,
			 &return_value);

	return return_value;
}

static gboolean
button_release_event (GtkWidget *widget,
		      GdkEventButton *event)
{
	GnomeIconContainer *container;
	GnomeIconContainerDetails *details;

	container = GNOME_ICON_CONTAINER (widget);
	details = container->details;

	if (event->button == 1 && details->rubberband_info.active) {
		stop_rubberbanding (container, event);
		return TRUE;
	}

	if (event->button == details->drag_button) {
		details->drag_button = 0;
		
	        if (! details->doing_drag
		    && ! button_event_modifies_selection (event)) {
			gboolean selection_changed;

			selection_changed
				= select_one_unselect_others (container,
							      details->drag_icon);

			if (selection_changed)
				gtk_signal_emit (GTK_OBJECT (container),
						 signals[SELECTION_CHANGED]);
		}

		if ((details->drag_icon != NULL) && (!details->doing_drag)) {
			gint elapsed_time = event->time - details->button_down_time;
                        set_kbd_current (container, details->drag_icon, TRUE);

			/* If single-click mode, activate the icon, unless modifying
			 * the selection or pressing for a very long time. */
			if (details->single_click_mode && (elapsed_time < MAX_CLICK_TIME)
				&& ! button_event_modifies_selection (event)) {

			    /* FIXME: This should activate all selected icons, not just one */
			    gtk_signal_emit (GTK_OBJECT (container),
					     signals[ACTIVATE],
					     details->drag_icon->data);
			}
			
			details->drag_icon = NULL;
			return TRUE;		
		}

		if (details->doing_drag)
			gnome_icon_container_dnd_end_drag (container);

		details->doing_drag = FALSE;
		return TRUE;
	}

	if (GTK_WIDGET_CLASS (parent_class)->button_release_event != NULL)
		return GTK_WIDGET_CLASS (parent_class)->button_release_event
			(widget, event);

	return FALSE;
}

static gint
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *motion)
{
	GnomeIconContainer *container;
	GnomeIconContainerDetails *details;
	double world_x, world_y;

	container = GNOME_ICON_CONTAINER (widget);
	details = container->details;

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      motion->x, motion->y,
				      &world_x, &world_y);

#define SNAP_RESISTANCE 2	/* GMC has this set to 3, but it's too much for
                                   my taste.  */
	if (details->drag_button != 0
	    && abs (details->drag_x - world_x) >= SNAP_RESISTANCE
	    && abs (details->drag_y - world_y) >= SNAP_RESISTANCE) {
		details->doing_drag = TRUE;

		/* KLUDGE ALERT: Poke the starting values into the motion
                   structure so that dragging behaves as expected.  */
		motion->x = details->drag_x;
		motion->y = details->drag_y;

		gnome_icon_container_dnd_begin_drag (container,
						     GDK_ACTION_MOVE,
						     details->drag_button,
						     motion);
		return TRUE;
	}
#undef SNAP_RESISTANCE

	if (GTK_WIDGET_CLASS (parent_class)->motion_notify_event != NULL)
		return (* GTK_WIDGET_CLASS (parent_class)->motion_notify_event)
			(widget, motion);

	return FALSE;
}

static gint
key_press_event (GtkWidget *widget,
		 GdkEventKey *event)
{
	GnomeIconContainer *container;

	if ((* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event))
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

	/* Derive from GnomeCanvas.  */

	parent_class = gtk_type_class (gnome_canvas_get_type ());

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
	signals[CONTEXT_CLICK_ICON]
		= gtk_signal_new ("context_click_icon",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     context_click_icon),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);
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

	details->kbd_navigation_rectangle 
		= gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (container)),
					 gnome_canvas_rect_get_type (),
					 "outline_color", "black",
					 "outline_stipple", stipple,
					 "width_pixels", 1,
					 NULL);
	gnome_canvas_item_hide (details->kbd_navigation_rectangle);

	details->kbd_icon_visibility_timer_tag = -1;
	details->linger_selection_mode_timer_tag = -1;
        
        /* FIXME: soon we'll need fonts at multiple sizes */
        details->label_font = gdk_font_load("-bitstream-charter-medium-r-normal-*-12-*-*-*-*-*-*-*"); 	

	/* FIXME: Read these from preferences. */
	details->linger_selection_mode = FALSE;
	details->single_click_mode = TRUE;

	container->details = details;

	/* Set up DnD.  */
	gnome_icon_container_dnd_init (container, stipple);

	/* Request update.  */
	add_idle (container);
}


/* GnomeIconContainerIcon event handling.  */

/* Selection in linger selection mode.  */
static gint
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
	selection_changed |= icon_select (icon, TRUE);

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

/* Conceptually, pressing button 1 together with CTRL or SHIFT toggles selection of a
   single icon without affecting the other icons; without CTRL or SHIFT, it selects a
   single icon and un-selects all the other icons.  But in this latter case,
   the de-selection should only happen when the button is released if the
   icon is already selected, because the user might select multiple icons and
   drag all of them by doing a simple click-drag.  */
static gint
handle_icon_button_press (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventButton *event)
{
	GnomeIconContainerDetails *details;

	details = container->details;

	if (event->button == 3) {
		/* FIXME this means you cannot drag with right click.  Instead,
                   we should setup a timeout and emit this signal if the
                   timeout expires without movement.  */
		details->drag_button = 0;
		details->drag_icon = NULL;

		gtk_signal_emit (GTK_OBJECT (container),
				 signals[CONTEXT_CLICK_ICON],
				 icon->data);

		return TRUE;
	}

	if (event->button != 1)
		return FALSE;

	if (button_event_modifies_selection (event)) {
		icon_toggle_selected (icon);
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	} else if (! icon->is_selected) {
		unselect_all (container);
		icon_select (icon, TRUE);
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	}

	if (event->type == GDK_2BUTTON_PRESS) {
		/* Double clicking should *never* trigger a D&D action.
                 * We must clear this out before emitting the signal, because
		 * handling the activate signal might invalidate the drag_icon pointer.
                 */
		details->drag_button = 0;
		details->drag_icon = NULL;

		/* FIXME: This should activate all selected icons, not just one */
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[ACTIVATE],
				 icon->data);

		return TRUE;
	}

	details->drag_button = event->button;
	details->drag_icon = icon;
	details->drag_x = event->x;
	details->drag_y = event->y;

	return TRUE;
}

static gint
handle_icon_enter_notify (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventMotion *motion)
{
	GnomeIconContainerDetails *details;

	details = container->details;
	if (! details->linger_selection_mode)
		return FALSE;

	if (details->linger_selection_mode_timer_tag != -1)
		gtk_timeout_remove (details->linger_selection_mode_timer_tag);

	details->linger_selection_mode_timer_tag
		= gtk_timeout_add (LINGER_SELECTION_MODE_TIMEOUT,
				   linger_select_timeout_cb, container);

	details->linger_selection_mode_icon = icon;

	return TRUE;
}

static gint
handle_icon_leave_notify (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventMotion *motion)
{
	GnomeIconContainerDetails *details;

	details = container->details;
	if (! details->linger_selection_mode)
		return FALSE;

	if (details->linger_selection_mode_timer_tag != -1)
		gtk_timeout_remove (details->linger_selection_mode_timer_tag);

	return TRUE;
}

static gint
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

	for (p = details->icons; p != NULL; p = p->next)
		icon_destroy (p->data);
	g_list_free (details->icons);
	details->icons = NULL;
	details->num_icons = 0;

	icon_grid_clear (details->grid);

	unset_kbd_current (container);
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

	new_icon = icon_new_pixbuf (container, data);
	icon_position (new_icon, container, x, y);

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
	gint x, y;

	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	new_icon = icon_new_pixbuf (container, data);

	icon_grid_add_auto (container->details->grid, new_icon, &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	icon_position (new_icon, container, x, y);

	setup_icon_in_container (container, new_icon);

	add_idle (container);
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

				icon_position (icon, container, dx, dy);

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
	gint x, y, dx;

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

				icon_position (icon, container, dx, y);
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
		selection_changed |= icon_select (icon, TRUE);
	}

	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
}

/**
 * gnome_icon_container_select_list_unselect_others:
 * @container: An icon container widget.
 * @list: A list of GnomeContainerIcons.
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
		selection_changed |= icon_select
			(icon, g_list_find (icons, icon) != NULL);
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
