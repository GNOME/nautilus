/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-container-grid.c - Grid used by icon container.

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
#include "nautilus-icon-grid.h"

#include <string.h>
#include <math.h>
#include "nautilus-gnome-extensions.h"

#define INITIAL_GRID_WIDTH 64
#define INITIAL_GRID_HEIGHT 64

#define GRID_CELL_WIDTH 80
#define GRID_CELL_HEIGHT 80

#define FIRST_FREE_NONE G_MININT

struct NautilusIconGrid {
	/* The grid. This is automatically sized to fit all the
	 * icons, so it doesn't need to be explicitly allocated.
	 */
	ArtIRect bounds;
	GList **elements;

	/* This is the number or grid positions that we actually use
	 * for finding positions for new icons.
	 */
	int visible_width;

	/* Position of the first free cell (used to speed up get_position).
	 * Set first_free_x to FIRST_FREE_NONE to indicate no free cell.
	 */
	int first_free_x, first_free_y;
};

NautilusIconGrid *
nautilus_icon_grid_new (void)
{
	return g_new0 (NautilusIconGrid, 1);
}

void
nautilus_icon_grid_clear (NautilusIconGrid *grid)
{
	int i, num_elements;

	num_elements = (grid->bounds.x1 - grid->bounds.x0)
		* (grid->bounds.y1 - grid->bounds.y0);
	for (i = 0; i < num_elements; i++) {
		g_list_free (grid->elements[i]);
	}
	g_free (grid->elements);
	grid->elements = 0;

	grid->bounds.x0 = 0;
	grid->bounds.y0 = 0;
	grid->bounds.x1 = 0;
	grid->bounds.y1 = 0;
	grid->first_free_x = 0;
	grid->first_free_y = 0;
}

void
nautilus_icon_grid_destroy (NautilusIconGrid *grid)
{
	nautilus_icon_grid_clear (grid);
	g_free (grid);
}

static GList **
get_element_ptr (GList **elements,
		 const ArtIRect *bounds,
		 int x, int y)
{
	g_assert (x >= bounds->x0);
	g_assert (y >= bounds->y0);
	g_assert (x < bounds->x1);
	g_assert (y < bounds->y1);

	return &elements[(y - bounds->y0)
			* (bounds->x1 - bounds->x0)
			+ (x - bounds->x0)];
}

static GList **
grid_get_element_ptr (NautilusIconGrid *grid,
		      int x, int y)
{
	return get_element_ptr (grid->elements, &grid->bounds, x, y);
}

static void
resize (NautilusIconGrid *grid,
	const ArtIRect *new_bounds)
{
	int new_size;
	GList **new_elements;
	int x, y;

	g_assert (nautilus_art_irect_contains_irect (new_bounds, &grid->bounds));
	g_assert (new_bounds->x1 >= grid->visible_width);

	new_size = (new_bounds->x1 - new_bounds->x0) * (new_bounds->y1 - new_bounds->y0);
	new_elements = g_new0 (GList *, new_size);

	for (x = grid->bounds.x0; x < grid->bounds.x1; x++) {
		for (y = grid->bounds.y0; y < grid->bounds.y1; y++) {
			*get_element_ptr (new_elements, new_bounds, x, y) =
				*grid_get_element_ptr (grid, x, y);
		}
	}

	g_free (grid->elements);
	grid->elements = new_elements;

	/* We might have a newly-free position if we are making the grid taller. */
	if (new_bounds->y1 > grid->bounds.y1
	    && grid->first_free_x == FIRST_FREE_NONE) {
		grid->first_free_x = 0;
		grid->first_free_y = grid->bounds.y1;
	}

	grid->bounds = *new_bounds;
}

static void
update_first_free_forward (NautilusIconGrid *grid)
{
	int x, y;

	if (grid->first_free_x == FIRST_FREE_NONE) {
		x = 0;
		y = 0;
	} else {
		x = grid->first_free_x;
		y = grid->first_free_y;
	}

	while (y < grid->bounds.y1) {
		if (*grid_get_element_ptr (grid, x, y) == NULL) {
			grid->first_free_x = x;
			grid->first_free_y = y;
			return;
		}

		x++;
		if (x >= grid->visible_width) {
			x = 0;
			y++;
		}
	}

	/* No free cell found. */
	grid->first_free_x = FIRST_FREE_NONE;
}

void
nautilus_icon_grid_set_visible_width (NautilusIconGrid *grid,
				      double world_visible_width)
{
	int visible_width;
	ArtIRect bounds;

	visible_width = MAX(1, floor (world_visible_width / GRID_CELL_WIDTH));

	if (visible_width > grid->bounds.x1) {
		bounds = grid->bounds;
		bounds.x1 = visible_width;
		resize (grid, &bounds);
	}

	/* Check and see if there are newly-free positions because
	 * the layout part of the grid is getting wider.
	 */
	if (visible_width > grid->visible_width
	    && grid->bounds.y1 > 0
	    && grid->first_free_x == FIRST_FREE_NONE) {
		grid->first_free_x = visible_width;
		grid->first_free_y = 0;
	}

	grid->visible_width = visible_width;

	/* Check and see if the old first-free position is illegal
	 * because the layout part of the grid is getting narrower.
	 */
	if (grid->first_free_x >= visible_width) {
		g_assert (grid->first_free_x != FIRST_FREE_NONE);
		if (grid->first_free_y == grid->bounds.y1 - 1) {
			grid->first_free_x = FIRST_FREE_NONE;
		} else {
			grid->first_free_x = 0;
			grid->first_free_y++;
			update_first_free_forward (grid);
		}
	}
}

static void
maybe_resize (NautilusIconGrid *grid,
	      int x, int y)
{
	ArtIRect new_bounds;

	new_bounds = grid->bounds;

	if (new_bounds.x0 == new_bounds.x1) {
		if (grid->visible_width != 0) {
			new_bounds.x1 = grid->visible_width;
		} else {
			new_bounds.x1 = INITIAL_GRID_WIDTH;
		}
	}

	if (new_bounds.y0 == new_bounds.y1) {
		new_bounds.y1 = INITIAL_GRID_HEIGHT;
	}

	while (x < new_bounds.x0) {
		new_bounds.x0 -= new_bounds.x1 - new_bounds.x0;
	}
	while (x >= new_bounds.x1) {
		new_bounds.x1 += new_bounds.x1 - new_bounds.x0;
	}
	while (y < new_bounds.y0) {
		new_bounds.y0 -= new_bounds.y1 - new_bounds.y0;
	}
	while (y >= new_bounds.y1) {
		new_bounds.y1 += new_bounds.y1 - new_bounds.y0;
	}

	if (!nautilus_art_irect_equal (&new_bounds, &grid->bounds)) {
		resize (grid, &new_bounds);
	}
}

static void
grid_add_one (NautilusIconGrid *grid,
	      NautilusIcon *icon,
	      int x, int y)
{
	GList **elem_ptr;

	maybe_resize (grid, x, y);

	elem_ptr = grid_get_element_ptr (grid, x, y);
	g_assert (g_list_find (*elem_ptr, icon) == NULL);
	*elem_ptr = g_list_prepend (*elem_ptr, icon);

	if (x == grid->first_free_x && y == grid->first_free_y) {
		update_first_free_forward (grid);
	}
}

static void
grid_remove_one (NautilusIconGrid *grid,
		 NautilusIcon *icon,
		 int x, int y)
{
	GList **elem_ptr;
	
	elem_ptr = grid_get_element_ptr (grid, x, y);
	g_assert (g_list_find (*elem_ptr, icon) != NULL);
	*elem_ptr = g_list_remove (*elem_ptr, icon);

	if (*elem_ptr == NULL) {
		if (grid->first_free_x == FIRST_FREE_NONE
		    || grid->first_free_y > y
		    || (grid->first_free_y == y && grid->first_free_x > x)) {
			grid->first_free_x = x;
			grid->first_free_y = y;
		}
	}
}

static void
add_or_remove (NautilusIconGrid *grid,
	       NautilusIcon *icon,
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
nautilus_icon_grid_add (NautilusIconGrid *grid,
			NautilusIcon *icon)
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
nautilus_icon_grid_remove (NautilusIconGrid *grid,
			   NautilusIcon *icon)
{
	add_or_remove (grid, icon, FALSE);
}

void
nautilus_icon_grid_get_position (NautilusIconGrid *grid,
				 NautilusIcon *icon,
				 ArtPoint *position)
{
	int grid_x, grid_y;

	g_return_if_fail (grid != NULL);
	g_return_if_fail (position != NULL);

	if (grid->first_free_x == FIRST_FREE_NONE) {
		grid_x = 0;
		grid_y = grid->bounds.y1;
	} else {
		grid_x = grid->first_free_x;
		grid_y = grid->first_free_y;
	}

	position->x = (double) grid_x * GRID_CELL_WIDTH;
	position->y = (double) grid_y * GRID_CELL_HEIGHT;
}

static int
nautilus_compare_pointers_as_integers (gconstpointer a, gconstpointer b)
{
	int ai, bi;

	ai = GPOINTER_TO_INT (a);
	bi = GPOINTER_TO_INT (b);
	if (ai < bi) {
		return -1;
	}
	if (ai > bi) {
		return 1;
	}
	return 0;
}

static GList *
nautilus_g_list_remove_duplicates (GList *list)
{
	GList *p, *next;
	gpointer previous_data;

	list = g_list_sort (list, nautilus_compare_pointers_as_integers);

	previous_data = NULL;
	for (p = list; p != NULL; p = next) {
		next = p->next;

		g_assert (p->data != NULL);
		if (previous_data != p->data) {
			previous_data = p->data;
		} else {
			list = g_list_remove_link (list, p);
			g_list_free_1 (p);
		}
	}
	return list;
}

GList *
nautilus_icon_grid_get_intersecting_icons (NautilusIconGrid *grid,
					   const ArtDRect *world_rect)
{
	ArtIRect test_rect;
	int x, y;
	GList *list;
	GList *cell_list;

	test_rect.x0 = floor (world_rect->x0 / GRID_CELL_WIDTH);
	test_rect.y0 = floor (world_rect->y0 / GRID_CELL_HEIGHT);
	test_rect.x1 = ceil (world_rect->x1 / GRID_CELL_WIDTH);
	test_rect.y1 = ceil (world_rect->y1 / GRID_CELL_HEIGHT);

	art_irect_intersect (&test_rect, &test_rect, &grid->bounds);

	list = NULL;
	for (x = test_rect.x0; x < test_rect.x1; x++) {
		for (y = test_rect.y0; y < test_rect.y1; y++) {
			cell_list = *grid_get_element_ptr (grid, x, y);
			list = g_list_concat (list, g_list_copy (cell_list));
		}
	}
	return nautilus_g_list_remove_duplicates (list);
}
