/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-art-extensions.c - implementation of libart extension functions.

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

   Authors: Darin Adler <darin@eazel.com>
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "eel-art-extensions.h"
#include "eel-lib-self-check-functions.h"
#include <math.h>

const EelDRect eel_drect_empty = { 0.0, 0.0, 0.0, 0.0 };
const EelIRect eel_irect_empty = { 0, 0, 0, 0 };
const EelIPoint eel_ipoint_max = { G_MAXINT, G_MAXINT };
const EelIPoint eel_ipoint_min = { G_MININT, G_MININT };
const EelIPoint eel_ipoint_zero = { 0, 0 };
const EelDimensions eel_dimensions_empty = { 0, 0 };

void
eel_irect_copy (EelIRect *dest, const EelIRect *src)
{
	dest->x0 = src->x0;
	dest->y0 = src->y0;
	dest->x1 = src->x1;
	dest->y1 = src->y1;
}

void
eel_irect_union (EelIRect *dest,
		  const EelIRect *src1,
		  const EelIRect *src2) {
	if (eel_irect_is_empty (src1)) {
		eel_irect_copy (dest, src2);
	} else if (eel_irect_is_empty (src2)) {
		eel_irect_copy (dest, src1);
	} else {
		dest->x0 = MIN (src1->x0, src2->x0);
		dest->y0 = MIN (src1->y0, src2->y0);
		dest->x1 = MAX (src1->x1, src2->x1);
		dest->y1 = MAX (src1->y1, src2->y1);
	}
}

void
eel_irect_intersect (EelIRect *dest,
		     const EelIRect *src1,
		     const EelIRect *src2)
{
	dest->x0 = MAX (src1->x0, src2->x0);
	dest->y0 = MAX (src1->y0, src2->y0);
	dest->x1 = MIN (src1->x1, src2->x1);
	dest->y1 = MIN (src1->y1, src2->y1);
}

gboolean
eel_irect_is_empty (const EelIRect *src)
{
	return (src->x1 <= src->x0 ||
		src->y1 <= src->y0);
}

EelIRect
eel_irect_assign (int x,
		      int y,
		      int width,
		      int height)
{
	EelIRect rectangle;

	rectangle.x0 = x;
	rectangle.y0 = y;
	rectangle.x1 = rectangle.x0 + width;
	rectangle.y1 = rectangle.y0 + height;

	return rectangle;
}

/**
 * eel_irect_assign_dimensions:
 * 
 * @x: X coodinate for resulting rectangle.
 * @y: Y coodinate for resulting rectangle.
 * @dimensions: A EelDimensions structure for the rect's width and height.
 *
 * Returns: An EelIRect with the given coordinates and dimensions.
 */
EelIRect
eel_irect_assign_dimensions (int x,
				 int y,
				 EelDimensions dimensions)
{
	EelIRect rectangle;

	rectangle.x0 = x;
	rectangle.y0 = y;
	rectangle.x1 = rectangle.x0 + dimensions.width;
	rectangle.y1 = rectangle.y0 + dimensions.height;

	return rectangle;
}

/**
 * eel_irect_get_width:
 * 
 * @rectangle: An EelIRect.
 *
 * Returns: The width of the rectangle.
 * 
 */
int
eel_irect_get_width (EelIRect rectangle)
{
	return rectangle.x1 - rectangle.x0;
}

/**
 * eel_irect_get_height:
 * 
 * @rectangle: An EelIRect.
 *
 * Returns: The height of the rectangle.
 * 
 */
int
eel_irect_get_height (EelIRect rectangle)
{
	return rectangle.y1 - rectangle.y0;
}


static void
eel_drect_copy (EelDRect *dest,
		const EelDRect *src)
{
	dest->x0 = src->x0;
	dest->y0 = src->y0;
	dest->x1 = src->x1;
	dest->y1 = src->y1;
}

static gboolean
eel_drect_is_empty (const EelDRect *src)
{
	return (src->x1 <= src->x0 || src->y1 <= src->y0);
}

void
eel_drect_union (EelDRect *dest,
		 const EelDRect *src1,
		 const EelDRect *src2)
{
	if (eel_drect_is_empty (src1)) {
		eel_drect_copy (dest, src2);
	} else if (eel_drect_is_empty (src2)) {
		eel_drect_copy (dest, src1);
	} else {
		dest->x0 = MIN (src1->x0, src2->x0);
		dest->y0 = MIN (src1->y0, src2->y0);
		dest->x1 = MAX (src1->x1, src2->x1);
		dest->y1 = MAX (src1->y1, src2->y1);
	}
}


/**
 * eel_irect_contains_point:
 * 
 * @rectangle: An EelIRect.
 * @x: X coordinate to test.
 * @y: Y coordinate to test.
 *
 * Returns: A boolean value indicating whether the rectangle 
 *          contains the x,y coordinate.
 * 
 */
gboolean
eel_irect_contains_point (EelIRect rectangle,
			  int x,
			  int y)
{
	return x >= rectangle.x0
		&& x <= rectangle.x1
		&& y >= rectangle.y0
		&& y <= rectangle.y1;
}

gboolean
eel_irect_hits_irect (EelIRect rectangle_a,
			  EelIRect rectangle_b)
{
	EelIRect intersection;
	eel_irect_intersect (&intersection, &rectangle_a, &rectangle_b);
	return !eel_irect_is_empty (&intersection);
}

gboolean
eel_irect_equal (EelIRect rectangle_a,
		     EelIRect rectangle_b)
{
	return rectangle_a.x0 == rectangle_b.x0
		&& rectangle_a.y0 == rectangle_b.y0
		&& rectangle_a.x1 == rectangle_b.x1
		&& rectangle_a.y1 == rectangle_b.y1;
}

/**
 * eel_irect_align:
 * 
 * @container: The rectangle that is to contain the aligned rectangle.
 * @aligned_width: Width of rectangle being algined.
 * @aligned_height: Height of rectangle being algined.
 * @x_alignment: X alignment.
 * @y_alignment: Y alignment.
 *
 * Returns: A rectangle aligned within a container rectangle
 *          using the given alignment parameters.
 */
EelIRect
eel_irect_align (EelIRect container,
		     int aligned_width,
		     int aligned_height,
		     float x_alignment,
		     float y_alignment)
{
	EelIRect aligned;
	int available_width;
	int available_height;

	if (eel_irect_is_empty (&container)) {
		return eel_irect_empty;
	}

	if (aligned_width == 0 || aligned_height == 0) {
		return eel_irect_empty;
	}

	/* Make sure the aligment parameters are within range */
	x_alignment = MAX (0, x_alignment);
	x_alignment = MIN (1.0, x_alignment);
	y_alignment = MAX (0, y_alignment);
	y_alignment = MIN (1.0, y_alignment);

	available_width = eel_irect_get_width (container) - aligned_width;
	available_height = eel_irect_get_height (container) - aligned_height;

	aligned.x0 = floor (container.x0 + (available_width * x_alignment) + 0.5);
	aligned.y0 = floor (container.y0 + (available_height * y_alignment) + 0.5);
	aligned.x1 = aligned.x0 + aligned_width;
	aligned.y1 = aligned.y0 + aligned_height;

	return aligned;
}


/**
 * eel_dimensions_are_empty:
 * 
 * @dimensions: A EelDimensions structure.
 *
 * Returns: Whether the dimensions are empty.
 */
gboolean
eel_dimensions_are_empty (EelDimensions dimensions)
{
	return dimensions.width <= 0 || dimensions.height <= 0;
}

EelIRect 
eel_irect_offset_by (EelIRect rectangle, int x, int y)
{
	rectangle.x0 += x;
	rectangle.x1 += x;
	rectangle.y0 += y;
	rectangle.y1 += y;
	
	return rectangle;
}

EelIRect 
eel_irect_scale_by (EelIRect rectangle, double scale)
{
	rectangle.x0 *= scale;
	rectangle.x1 *= scale;
	rectangle.y0 *= scale;
	rectangle.y1 *= scale;
	
	return rectangle;
}
