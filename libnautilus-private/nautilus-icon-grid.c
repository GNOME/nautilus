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

   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-icon-grid.h"

#include <string.h>
#include <math.h>
#include "nautilus-gnome-extensions.h"

#define INITIAL_GRID_WIDTH 64
#define INITIAL_GRID_HEIGHT 64

#define BASE_CELL_WIDTH 12
#define BASE_CELL_HEIGHT 12

#define START_GRID_POWER 2 /* typical icon size */

#define FIRST_FREE_NONE G_MININT

/* This is a single grid at one grid resolution.
 * The icon grid as a whole is a set of these for each size.
 */
typedef struct {
	/* Grid resolution 2^<power>. */
	int power;

	/* This is the number or grid positions that we actually use
	 * for finding positions for new icons.
	 */
	int visible_width;

	/* The grid. This is automatically sized to fit all the
	 * icons, so it doesn't need to be explicitly allocated.
	 */
	ArtIRect bounds;
	GList **elements;

	/* Position of the first free cell (used to speed up get_position).
	 * Set first_free_x to FIRST_FREE_NONE to indicate no free cell.
	 */
	int first_free_x, first_free_y;
} Subgrid;

struct NautilusIconGrid {
	GPtrArray *subgrids;

	double world_visible_width;
};

static Subgrid *
subgrid_new (int power)
{
	Subgrid *subgrid;

	subgrid = g_new0 (Subgrid, 1);
	subgrid->power = power;
	return subgrid;
}

static void
subgrid_free (Subgrid *subgrid)
{
	int i, num_elements;

	if (subgrid == NULL) {
		return;
	}

	num_elements = (subgrid->bounds.x1 - subgrid->bounds.x0)
		* (subgrid->bounds.y1 - subgrid->bounds.y0);
	for (i = 0; i < num_elements; i++) {
		g_list_free (subgrid->elements[i]);
	}
	g_free (subgrid->elements);
	g_free (subgrid);
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
subgrid_get_element_ptr (Subgrid *subgrid,
		      int x, int y)
{
	return get_element_ptr (subgrid->elements, &subgrid->bounds, x, y);
}

static void
resize (Subgrid *subgrid,
	const ArtIRect *new_bounds)
{
	int new_size;
	GList **new_elements;
	int x, y;

	g_assert (nautilus_art_irect_contains_irect (new_bounds, &subgrid->bounds));
	g_assert (new_bounds->x1 >= subgrid->visible_width);

	new_size = (new_bounds->x1 - new_bounds->x0) * (new_bounds->y1 - new_bounds->y0);
	new_elements = g_new0 (GList *, new_size);

	for (x = subgrid->bounds.x0; x < subgrid->bounds.x1; x++) {
		for (y = subgrid->bounds.y0; y < subgrid->bounds.y1; y++) {
			*get_element_ptr (new_elements, new_bounds, x, y) =
				*subgrid_get_element_ptr (subgrid, x, y);
		}
	}

	g_free (subgrid->elements);
	subgrid->elements = new_elements;

	/* We might have a newly-free position if we are making the grid taller. */
	if (new_bounds->y1 > subgrid->bounds.y1
	    && subgrid->first_free_x == FIRST_FREE_NONE) {
		subgrid->first_free_x = 0;
		subgrid->first_free_y = subgrid->bounds.y1;
	}

	subgrid->bounds = *new_bounds;
}

static void
update_first_free_forward (Subgrid *subgrid)
{
	int x, y;

	if (subgrid->first_free_x == FIRST_FREE_NONE) {
		x = 0;
		y = 0;
	} else {
		x = subgrid->first_free_x;
		y = subgrid->first_free_y;
	}

	while (y < subgrid->bounds.y1) {
		if (*subgrid_get_element_ptr (subgrid, x, y) == NULL) {
			subgrid->first_free_x = x;
			subgrid->first_free_y = y;
			return;
		}

		x++;
		if (x >= subgrid->visible_width) {
			x = 0;
			y++;
		}
	}

	/* No free cell found. */
	subgrid->first_free_x = FIRST_FREE_NONE;
}

static void
subgrid_set_visible_width (Subgrid *subgrid,
			   double world_visible_width)
{
	int visible_width;
	ArtIRect bounds;

	if (subgrid == NULL) {
		return;
	}

	visible_width = MAX (floor (world_visible_width
				    / (BASE_CELL_WIDTH * (1 << subgrid->power))),
			     1);

	if (visible_width > subgrid->bounds.x1) {
		bounds = subgrid->bounds;
		bounds.x1 = visible_width;
		resize (subgrid, &bounds);
	}

	/* Check and see if there are newly-free positions because
	 * the layout part of the grid is getting wider.
	 */
	if (visible_width > subgrid->visible_width
	    && subgrid->bounds.y1 > 0
	    && subgrid->first_free_x == FIRST_FREE_NONE) {
		subgrid->first_free_x = visible_width;
		subgrid->first_free_y = 0;
	}

	subgrid->visible_width = visible_width;

	/* Check and see if the old first-free position is illegal
	 * because the layout part of the grid is getting narrower.
	 */
	if (subgrid->first_free_x >= visible_width) {
		g_assert (subgrid->first_free_x != FIRST_FREE_NONE);
		if (subgrid->first_free_y == subgrid->bounds.y1 - 1) {
			subgrid->first_free_x = FIRST_FREE_NONE;
		} else {
			subgrid->first_free_x = 0;
			subgrid->first_free_y++;
			update_first_free_forward (subgrid);
		}
	}
}

static void
maybe_resize (Subgrid *subgrid,
	      int x, int y)
{
	ArtIRect new_bounds;

	new_bounds = subgrid->bounds;

	if (new_bounds.x0 == new_bounds.x1) {
		if (subgrid->visible_width != 0) {
			new_bounds.x1 = subgrid->visible_width;
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

	if (!nautilus_art_irect_equal (&new_bounds, &subgrid->bounds)) {
		resize (subgrid, &new_bounds);
	}
}

static void
subgrid_add_one (Subgrid *subgrid,
		 NautilusIcon *icon,
		 int x, int y)
{
	GList **elem_ptr;

	maybe_resize (subgrid, x, y);

	elem_ptr = subgrid_get_element_ptr (subgrid, x, y);
	g_assert (g_list_find (*elem_ptr, icon) == NULL);
	*elem_ptr = g_list_prepend (*elem_ptr, icon);

	if (x == subgrid->first_free_x && y == subgrid->first_free_y) {
		update_first_free_forward (subgrid);
	}
}

static void
subgrid_remove_one (Subgrid *subgrid,
		    NautilusIcon *icon,
		    int x, int y)
{
	GList **elem_ptr;
	
	elem_ptr = subgrid_get_element_ptr (subgrid, x, y);
	g_assert (g_list_find (*elem_ptr, icon) != NULL);
	*elem_ptr = g_list_remove (*elem_ptr, icon);

	if (*elem_ptr == NULL) {
		if (subgrid->first_free_x == FIRST_FREE_NONE
		    || subgrid->first_free_y > y
		    || (subgrid->first_free_y == y && subgrid->first_free_x > x)) {
			subgrid->first_free_x = x;
			subgrid->first_free_y = y;
		}
	}
}

static void
subgrid_add_or_remove (Subgrid *subgrid,
		       NautilusIcon *icon,
		       gboolean add)
{
	int x, y;

	if (subgrid == NULL) {
		return;
	}

	/* Add/remove to all the overlapped grid squares. */
	for (x = (icon->grid_rectangle.x0 >> subgrid->power);
	     x < ((icon->grid_rectangle.x1 + ((1 << subgrid->power) - 1)) >> subgrid->power);
	     x++) {
		for (y = (icon->grid_rectangle.y0 >> subgrid->power);
		     y < ((icon->grid_rectangle.y1 + ((1 << subgrid->power) - 1)) >> subgrid->power);
		     y++) {
			if (add) {
				subgrid_add_one (subgrid, icon, x, y);
			} else {
				subgrid_remove_one (subgrid, icon, x, y);
			}
		}
	}
}

static void
subgrid_get_position (Subgrid *subgrid,
		      NautilusIcon *icon,
		      ArtPoint *position)
{
	int subgrid_x, subgrid_y;

	if (subgrid->first_free_x == FIRST_FREE_NONE) {
		subgrid_x = 0;
		subgrid_y = subgrid->bounds.y1;
	} else {
		subgrid_x = subgrid->first_free_x;
		subgrid_y = subgrid->first_free_y;
	}

	position->x = (double) subgrid_x * (BASE_CELL_WIDTH << subgrid->power);
	position->y = (double) subgrid_y * (BASE_CELL_HEIGHT << subgrid->power);
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

static GList *
subgrid_get_intersecting_icons (Subgrid *subgrid,
				const ArtDRect *world_rect)
{
	ArtIRect test_rect;
	int x, y;
	GList *list;
	GList *cell_list;

	if (subgrid == NULL) {
		return NULL;
	}

	if (world_rect == NULL) {
		test_rect = subgrid->bounds;
	} else {
		test_rect.x0 = floor (world_rect->x0 / (BASE_CELL_WIDTH << subgrid->power));
		test_rect.y0 = floor (world_rect->y0 / (BASE_CELL_HEIGHT << subgrid->power));
		test_rect.x1 = ceil (world_rect->x1 / (BASE_CELL_WIDTH << subgrid->power));
		test_rect.y1 = ceil (world_rect->y1 / (BASE_CELL_HEIGHT << subgrid->power));
		
		art_irect_intersect (&test_rect, &test_rect, &subgrid->bounds);
	}

	list = NULL;
	for (x = test_rect.x0; x < test_rect.x1; x++) {
		for (y = test_rect.y0; y < test_rect.y1; y++) {
			cell_list = *subgrid_get_element_ptr (subgrid, x, y);
			list = g_list_concat (list, g_list_copy (cell_list));
		}
	}
	return nautilus_g_list_remove_duplicates (list);
}

/* Get the smallest subgrid. */
static Subgrid *
get_smallest_subgrid (NautilusIconGrid *grid)
{
	int i;
	Subgrid *subgrid;

	for (i = 0; i < grid->subgrids->len; i++) {
		subgrid = g_ptr_array_index (grid->subgrids, i);
		if (subgrid != NULL) {
			return subgrid;
		}
	}

	return NULL;
}

/* Get the smallest subgrid. */
static Subgrid *
get_largest_subgrid (NautilusIconGrid *grid)
{
	int i;
	Subgrid *subgrid;

	for (i = grid->subgrids->len; i != 0; i--) {
		subgrid = g_ptr_array_index (grid->subgrids, i-1);
		if (subgrid != NULL) {
			return subgrid;
		}
	}

	return NULL;
}

/* Create a subgrid if we have to. */
static Subgrid *
create_subgrid (NautilusIconGrid *grid, int power)
{
	Subgrid *subgrid;
	GList *icons, *p;

	/* Make space for the new subgrid in the array. */
	if (grid->subgrids->len <= power) {
		g_ptr_array_set_size (grid->subgrids, power + 1);
	}
	
	/* If it was already there, return it. */
	subgrid = g_ptr_array_index (grid->subgrids, power);
	if (subgrid != NULL) {
		return subgrid;
	}

	/* Create the new subgrid. */
	subgrid = subgrid_new (power);
	subgrid_set_visible_width (subgrid, grid->world_visible_width);

	/* Add the icons to it. */
	icons = subgrid_get_intersecting_icons
		(get_largest_subgrid (grid), NULL);
	for (p = icons; p != NULL; p = p->next) {
		subgrid_add_or_remove (subgrid, p->data, TRUE);
	}
	g_list_free (icons);

	/* Put it in the array and return. */
	g_ptr_array_index (grid->subgrids, power) = subgrid;
	return subgrid;
}

NautilusIconGrid *
nautilus_icon_grid_new (void)
{
	NautilusIconGrid *grid;

	grid = g_new0 (NautilusIconGrid, 1);
	grid->subgrids = g_ptr_array_new ();
	return grid;
}

void
nautilus_icon_grid_clear (NautilusIconGrid *grid)
{
	int i;

	for (i = 0; i < grid->subgrids->len; i++) {
		subgrid_free (g_ptr_array_index (grid->subgrids, i));
	}

	/* Would just set size to 0 here, but that leaves around
	 * non-NULL entries in the array.
	 */
	g_ptr_array_free (grid->subgrids, TRUE);
	grid->subgrids = g_ptr_array_new ();
}

void
nautilus_icon_grid_destroy (NautilusIconGrid *grid)
{
	nautilus_icon_grid_clear (grid);
	g_ptr_array_free (grid->subgrids, TRUE);
	g_free (grid);
}

void
nautilus_icon_grid_set_visible_width (NautilusIconGrid *grid,
				      double world_visible_width)
{
	int i;

	if (grid->world_visible_width == world_visible_width) {
		return;
	}

	grid->world_visible_width = world_visible_width;

	for (i = 0; i < grid->subgrids->len; i++) {
		subgrid_set_visible_width
			(g_ptr_array_index (grid->subgrids, i),
			 world_visible_width);
	}
}

/* Get the size of the icon as a power of two.
 * Size 0 means it fits in 1x1 grid cells.
 * Size 1 means it fits in 2x2 grid cells.
 */
static int
get_icon_size_power (NautilusIcon *icon)
{
	ArtDRect world_bounds;
	int cell_count, power;

	nautilus_gnome_canvas_item_get_world_bounds
		(GNOME_CANVAS_ITEM (icon->item), &world_bounds);

	cell_count = MAX (ceil ((world_bounds.x1 - world_bounds.x0) / BASE_CELL_WIDTH),
			  ceil ((world_bounds.y1 - world_bounds.y0) / BASE_CELL_HEIGHT));

	for (power = 0; cell_count > 1; power++) {
		cell_count /= 2;
	}
	return power;
}

void
nautilus_icon_grid_add (NautilusIconGrid *grid,
			NautilusIcon *icon)
{
	ArtDRect world_bounds;
	int i;

	/* Figure out how big the icon is. */
	nautilus_gnome_canvas_item_get_world_bounds
		(GNOME_CANVAS_ITEM (icon->item), &world_bounds);
	
	/* Compute grid bounds for the icon. */
	icon->grid_rectangle.x0 = floor (world_bounds.x0 / BASE_CELL_WIDTH);
	icon->grid_rectangle.y0 = floor (world_bounds.y0 / BASE_CELL_HEIGHT);
	icon->grid_rectangle.x1 = ceil (world_bounds.x1 / BASE_CELL_WIDTH);
	icon->grid_rectangle.y1 = ceil (world_bounds.y1 / BASE_CELL_HEIGHT);

	if (grid->subgrids->len == 0) {
		create_subgrid (grid, get_icon_size_power (icon));
	}

	for (i = 0; i < grid->subgrids->len; i++) {
		subgrid_add_or_remove
			(g_ptr_array_index (grid->subgrids, i),
			 icon, TRUE);
	}
}

void
nautilus_icon_grid_remove (NautilusIconGrid *grid,
			   NautilusIcon *icon)
{
	int i;

	for (i = 0; i < grid->subgrids->len; i++) {
		subgrid_add_or_remove
			(g_ptr_array_index (grid->subgrids, i),
			 icon, FALSE);
	}
}

void
nautilus_icon_grid_get_position (NautilusIconGrid *grid,
				 NautilusIcon *icon,
				 ArtPoint *position)
{
	g_return_if_fail (grid != NULL);
	g_return_if_fail (position != NULL);

	subgrid_get_position
		(create_subgrid (grid, get_icon_size_power (icon)),
		 icon, position);
}

GList *
nautilus_icon_grid_get_intersecting_icons (NautilusIconGrid *grid,
					   const ArtDRect *world_rect)
{
	return subgrid_get_intersecting_icons
		(get_smallest_subgrid (grid), world_rect);
}
