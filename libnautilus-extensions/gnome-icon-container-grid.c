/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-icon-container-grid.c - Grid used by icon container.

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
#include "gnome-icon-container-grid.h"

#include <string.h>
#include <math.h>
#include "nautilus-gnome-extensions.h"

#define INITIAL_GRID_WIDTH 64
#define INITIAL_GRID_HEIGHT 64

#define GRID_CELL_WIDTH 80
#define GRID_CELL_HEIGHT 80

GnomeIconContainerGrid *
gnome_icon_container_grid_new (void)
{
	GnomeIconContainerGrid *new;

	new = g_new (GnomeIconContainerGrid, 1);

	new->width = new->height = 0;
	new->visible_width = 0;
	new->alloc_width = new->alloc_height = 0;

	new->elems = NULL;

	new->first_free_x = -1;
	new->first_free_y = -1;

	return new;
}

void
gnome_icon_container_grid_clear (GnomeIconContainerGrid *grid)
{
	GList **p;
	int i, j;

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

void
gnome_icon_container_grid_destroy (GnomeIconContainerGrid *grid)
{
	gnome_icon_container_grid_clear (grid);
	g_free (grid->elems);
	g_free (grid);
}

GList **
gnome_icon_container_grid_get_element_ptr (GnomeIconContainerGrid *grid,
			   int x, int y)
{
	return &grid->elems[y * grid->alloc_width + x];
}

/* This is admittedly a bit lame.
 *
 * Instead of re-allocating the grid from scratch and copying the values, we
 * should just link grid chunks horizontally and vertically in lists;
 * i.e. use a hybrid list/array representation.
 */
static void
resize_allocation (GnomeIconContainerGrid *grid,
		   int new_alloc_width,
		   int new_alloc_height)
{
	GList **new_elems;
	int i, j;
	int new_alloc_size;

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
		int copy_width, copy_height;

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
			int elems_left;

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
update_first_free_forward (GnomeIconContainerGrid *grid)
{
	GList **p;
	int start_x, start_y;
	int x, y;

	if (grid->first_free_x == -1) {
		start_x = start_y = 0;
		p = grid->elems;
	} else {
		start_x = grid->first_free_x;
		start_y = grid->first_free_y;
		p = gnome_icon_container_grid_get_element_ptr (grid, start_x, start_y);
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

void
gnome_icon_container_grid_set_visible_width (GnomeIconContainerGrid *grid,
					     int visible_width)
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
			update_first_free_forward (grid);
		}
	}

	grid->visible_width = visible_width;
}

void
gnome_icon_container_grid_resize (GnomeIconContainerGrid *grid,
				  int width, int height)
{
	int new_alloc_width, new_alloc_height;

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

		resize_allocation (grid, new_alloc_width,
				   new_alloc_height);
	}

	grid->width = width;
	grid->height = height;

	if (grid->visible_width != grid->width) {
		gnome_icon_container_grid_set_visible_width (grid, grid->width);
	}
}

static void
maybe_resize (GnomeIconContainerGrid *grid,
	      int x, int y)
{
	int new_width, new_height;

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

	gnome_icon_container_grid_resize (grid, new_width, new_height);
}

static void
grid_add_one (GnomeIconContainerGrid *grid,
	      GnomeIconContainerIcon *icon,
	      int x, int y)
{
	GList **elem_ptr;

	if (x < 0 || y < 0) {
		/* FIXME: Can't handle negative coordinates. */
		return;
	}

	maybe_resize (grid, x, y);

	elem_ptr = gnome_icon_container_grid_get_element_ptr (grid, x, y);
	g_assert (g_list_find (*elem_ptr, icon) == NULL);
	*elem_ptr = g_list_prepend (*elem_ptr, icon);

	if (x == grid->first_free_x && y == grid->first_free_y) {
		update_first_free_forward (grid);
	}
}

static void
grid_remove_one (GnomeIconContainerGrid *grid,
		 GnomeIconContainerIcon *icon,
		 int x, int y)
{
	GList **elem_ptr;
	
	if (x < 0 || y < 0) {
		/* FIXME: Can't handle negative coordinates. */
		return;
	}

	elem_ptr = gnome_icon_container_grid_get_element_ptr (grid, x, y);
	g_assert (g_list_find (*elem_ptr, icon) != NULL);
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
add_or_remove (GnomeIconContainerGrid *grid,
	       GnomeIconContainerIcon *icon,
	       gboolean add)
{
	int x, y;

	/* Add/remove to all the overlapped grid squares. */
	for (x = icon->grid_rectangle.x0; x < icon->grid_rectangle.x1; x++) {
		for (y = icon->grid_rectangle.y0; y < icon->grid_rectangle.y1; y++) {
			if (add) {
				grid_add_one (grid, icon, x, y);
			} else {
				grid_remove_one (grid, icon, x, y);
			}
		}
	}
}

void
gnome_icon_container_grid_add (GnomeIconContainerGrid *grid,
			       GnomeIconContainerIcon *icon)
{
	ArtDRect world_bounds;

	/* Figure out how big the icon is. */
	nautilus_gnome_canvas_item_get_world_bounds
		(GNOME_CANVAS_ITEM (icon->item), &world_bounds);
	
	/* Compute grid bounds for the icon. */
	icon->grid_rectangle.x0 = floor (world_bounds.x0 / GRID_CELL_WIDTH);
	icon->grid_rectangle.y0 = floor (world_bounds.y0 / GRID_CELL_HEIGHT);
	icon->grid_rectangle.x1 = ceil (world_bounds.x1 / GRID_CELL_WIDTH);
	icon->grid_rectangle.y1 = ceil (world_bounds.y1 / GRID_CELL_HEIGHT);

	add_or_remove (grid, icon, TRUE);
}

void
gnome_icon_container_grid_remove (GnomeIconContainerGrid *grid,
				  GnomeIconContainerIcon *icon)
{
	add_or_remove (grid, icon, FALSE);
}

void
gnome_icon_container_grid_get_position (GnomeIconContainerGrid *grid,
					GnomeIconContainerIcon *icon,
					int *x_return, int *y_return)
{
	int grid_x, grid_y;

	g_return_if_fail (grid != NULL);
	g_return_if_fail (x_return != NULL);
	g_return_if_fail (y_return != NULL);

	if (grid->first_free_x < 0 || grid->first_free_y < 0
	    || grid->height == 0 || grid->width == 0) {
		grid_x = 0;
		grid_y = grid->height;
	} else {
		grid_x = grid->first_free_x;
		grid_y = grid->first_free_y;
	}

	gnome_icon_container_grid_to_world (grid,
					    grid_x, grid_y,
					    x_return, y_return);
}



void
gnome_icon_container_world_to_grid (GnomeIconContainerGrid *grid,
				    int world_x, int world_y,
				    int *grid_x_return, int *grid_y_return)
{
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

void
gnome_icon_container_grid_to_world (GnomeIconContainerGrid *grid,
				    int grid_x, int grid_y,
				    int *world_x_return, int *world_y_return)
{
	if (world_x_return != NULL)
		*world_x_return
			= grid_x * GNOME_ICON_CONTAINER_CELL_WIDTH (container);

	if (world_y_return != NULL)
		*world_y_return
			= grid_y * GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
}

/* Find the "first" icon (in left-to-right, top-to-bottom order) in
   `container'.  */
GnomeIconContainerIcon *
gnome_icon_container_grid_find_first (GnomeIconContainerGrid *grid,
				      gboolean selected_only)
{
	GnomeIconContainerIcon *first;
	GList **p;
	int i, j;

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
				if (selected_only && !icon->is_selected) {
					continue;
				}
				
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

GnomeIconContainerIcon *
gnome_icon_container_grid_find_last (GnomeIconContainerGrid *grid,
				     gboolean selected_only)
{
	GnomeIconContainerIcon *last;
	GList **p;
	int i, j;

	last = NULL;

	if (grid->height == 0 || grid->width == 0)
		return NULL;

	p = gnome_icon_container_grid_get_element_ptr (grid, 0, grid->height - 1);

	for (i = grid->height - 1; i >= 0; i--) {
		for (j = grid->width - 1; j >= 0; j--) {
			GList *q;

			for (q = p[j]; q != NULL; q = q->next) {
				GnomeIconContainerIcon *icon;

				icon = q->data;
				if (selected_only && !icon->is_selected) {
					continue;
				}
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
