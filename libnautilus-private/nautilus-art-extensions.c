/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-art-extensions.c - implementation of libart extension functions.

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

#include "nautilus-art-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include <math.h>

ArtIRect NAUTILUS_ART_IRECT_EMPTY = { 0, 0, 0, 0 };
NautilusArtIPoint NAUTILUS_ART_IPOINT_ZERO = { 0, 0 };
NautilusDimensions NAUTILUS_DIMENSIONS_EMPTY = { 0, 0 };

gboolean
nautilus_art_irect_contains_irect (const ArtIRect *outer_rect,
				   const ArtIRect *inner_rect)
{
	g_return_val_if_fail (outer_rect != NULL, FALSE);
	g_return_val_if_fail (inner_rect != NULL, FALSE);

	return outer_rect->x0 <= inner_rect->x0
		&& outer_rect->y0 <= inner_rect->y0
		&& outer_rect->x1 >= inner_rect->x1
		&& outer_rect->y1 >= inner_rect->y1; 
}

/**
 * nautilus_art_irect_contains_point:
 * 
 * @rectangle: An ArtIRect.
 * @x: X coordinate to test.
 * @y: Y coordinate to test.
 *
 * Returns: A boolean value indicating whether the rectangle 
 *          contains the x,y coordinate.
 * 
 */
gboolean
nautilus_art_irect_contains_point (const ArtIRect *rectangle,
				   int x,
				   int y)
{
	g_return_val_if_fail (rectangle != NULL, FALSE);

	return x >= rectangle->x0
		&& x <= rectangle->x1
		&& y >= rectangle->y0
		&& y <= rectangle->y1;
}

gboolean
nautilus_art_irect_hits_irect (const ArtIRect *rect_a,
			       const ArtIRect *rect_b)
{
	ArtIRect intersection;

	g_return_val_if_fail (rect_a != NULL, FALSE);
	g_return_val_if_fail (rect_b != NULL, FALSE);

	art_irect_intersect (&intersection, rect_a, rect_b);
	return !art_irect_empty (&intersection);
}

gboolean
nautilus_art_irect_equal (const ArtIRect *rect_a,
			  const ArtIRect *rect_b)
{
	g_return_val_if_fail (rect_a != NULL, FALSE);
	g_return_val_if_fail (rect_b != NULL, FALSE);

	return rect_a->x0 == rect_b->x0
		&& rect_a->y0 == rect_b->y0
		&& rect_a->x1 == rect_b->x1
		&& rect_a->y1 == rect_b->y1;
}

gboolean
nautilus_art_drect_equal (const ArtDRect *rect_a,
			  const ArtDRect *rect_b)
{
	g_return_val_if_fail (rect_a != NULL, FALSE);
	g_return_val_if_fail (rect_b != NULL, FALSE);

	return rect_a->x0 == rect_b->x0
		&& rect_a->y0 == rect_b->y0
		&& rect_a->x1 == rect_b->x1
		&& rect_a->y1 == rect_b->y1;
}

gboolean
nautilus_art_irect_is_valid (const ArtIRect *rect)
{
	return rect && !art_irect_empty (rect);
}

void
nautilus_art_irect_assign (ArtIRect *rect,
			   int x,
			   int y,
			   int width,
			   int height)
{
	g_return_if_fail (rect != NULL);

	rect->x0 = x;
	rect->y0 = y;
	rect->x1 = rect->x0 + width;
	rect->y1 = rect->y0 + height;
}

/**
 * nautilus_art_irect_get_width:
 * 
 * @rectangle: An ArtIRect.
 *
 * Returns: The width of the rectangle.
 * 
 */
int
nautilus_art_irect_get_width (const ArtIRect *rectangle)
{
	g_return_val_if_fail (rectangle != NULL, 0);
	
	return rectangle->x1 - rectangle->x0;
}

/**
 * nautilus_art_irect_get_height:
 * 
 * @rectangle: An ArtIRect.
 *
 * Returns: The height of the rectangle.
 * 
 */
int
nautilus_art_irect_get_height (const ArtIRect *rectangle)
{
	g_return_val_if_fail (rectangle != NULL, 0);
	
	return rectangle->y1 - rectangle->y0;
}

/**
 * nautilus_art_irect_align:
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
ArtIRect
nautilus_art_irect_align (const ArtIRect *container,
			  int aligned_width,
			  int aligned_height,
			  float x_alignment,
			  float y_alignment)
{
	ArtIRect aligned;
	int available_width;
	int available_height;

	g_return_val_if_fail (container != NULL, NAUTILUS_ART_IRECT_EMPTY);

	if (art_irect_empty (container)) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	if (aligned_width == 0 || aligned_height == 0) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	/* Make sure the aligment parameters are within range */
	x_alignment = MAX (0, x_alignment);
	x_alignment = MIN (1.0, x_alignment);
	y_alignment = MAX (0, y_alignment);
	y_alignment = MIN (1.0, y_alignment);

	available_width = nautilus_art_irect_get_width (container) - aligned_width;
	available_height = nautilus_art_irect_get_height (container) - aligned_height;

	aligned.x0 = floor (container->x0 + (available_width * x_alignment) + 0.5);
	aligned.y0 = floor (container->y0 + (available_height * y_alignment) + 0.5);
	aligned.x1 = aligned.x0 + aligned_width;
	aligned.y1 = aligned.y0 + aligned_height;

	return aligned;
}

/**
 * nautilus_dimensions_empty:
 * 
 * @dimensions: A NautilusDimensions structure.
 *
 * Returns: Whether the dimensions are empty.
 */
gboolean
nautilus_dimensions_empty (const NautilusDimensions *dimensions)
{
	g_return_val_if_fail (dimensions != NULL, TRUE);

	return dimensions->width <= 0 || dimensions->height <= 0;
}

/**
 * nautilus_art_irect_assign_dimensions:
 * 
 * @x: X coodinate for resulting rectangle.
 * @y: Y coodinate for resulting rectangle.
 * @dimensions: A NautilusDimensions structure for the rect's width and height.
 *
 * Returns: An ArtIRect with the given coordinates and dimensions.
 */
ArtIRect
nautilus_art_irect_assign_dimensions (int x,
				      int y,
				      const NautilusDimensions *dimensions)
{
	ArtIRect rectangle;

	g_return_val_if_fail (dimensions != NULL, NAUTILUS_ART_IRECT_EMPTY);

	rectangle.x0 = x;
	rectangle.y0 = y;
	rectangle.x1 = rectangle.x0 + dimensions->width;
	rectangle.y1 = rectangle.y0 + dimensions->height;

	return rectangle;
}

ArtIRect 
nautilus_art_irect_offset_by (ArtIRect rectangle, int x, int y)
{
	rectangle.x0 += x;
	rectangle.x1 += x;
	rectangle.y0 += y;
	rectangle.y1 += y;
	
	return rectangle;
}

ArtIRect 
nautilus_art_irect_offset_to (ArtIRect rectangle, int x, int y)
{
	rectangle.x1 = rectangle.x1 - rectangle.x0 + x;
	rectangle.x0 = x;
	rectangle.y1 = rectangle.y1 - rectangle.y0 + y;
	rectangle.y0 = y;
	
	return rectangle;
}

ArtIRect 
nautilus_art_irect_scale_by (ArtIRect rectangle, double scale)
{
	rectangle.x0 *= scale;
	rectangle.x1 *= scale;
	rectangle.y0 *= scale;
	rectangle.y1 *= scale;
	
	return rectangle;
}

ArtIRect 
nautilus_art_irect_inset (ArtIRect rectangle, int horizontal_inset, int vertical_inset)
{
	rectangle.x0 += horizontal_inset;
	rectangle.x1 -= horizontal_inset;
	rectangle.y0 += vertical_inset;
	rectangle.y1 -= vertical_inset;
	
	return rectangle;
}


ArtDRect 
nautilus_art_drect_offset_by (ArtDRect rectangle, double x, double y)
{
	rectangle.x0 += x;
	rectangle.x1 += x;
	rectangle.y0 += y;
	rectangle.y1 += y;
	
	return rectangle;
}

ArtDRect 
nautilus_art_drect_offset_to (ArtDRect rectangle, double x, double y)
{
	rectangle.x1 = rectangle.x1 - rectangle.x0 + x;
	rectangle.x0 = x;
	rectangle.y1 = rectangle.y1 - rectangle.y0 + y;
	rectangle.y0 = y;
	
	return rectangle;
}

ArtIRect 
nautilus_art_irect_offset_by_point (ArtIRect rectangle, NautilusArtIPoint point)
{
	rectangle.x0 += point.x;
	rectangle.x1 += point.x;
	rectangle.y0 += point.y;
	rectangle.y1 += point.y;
	
	return rectangle;
}

ArtIRect 
nautilus_art_irect_offset_to_point (ArtIRect rectangle, NautilusArtIPoint point)
{
	rectangle.x1 = rectangle.x1 - rectangle.x0 + point.x;
	rectangle.x0 = point.x;
	rectangle.y1 = rectangle.y1 - rectangle.y0 + point.y;
	rectangle.y0 = point.y;
	
	return rectangle;
}

ArtDRect 
nautilus_art_drect_scale_by (ArtDRect rectangle, double scale)
{
	rectangle.x0 *= scale;
	rectangle.x1 *= scale;
	rectangle.y0 *= scale;
	rectangle.y1 *= scale;
	
	return rectangle;
}

ArtDRect 
nautilus_art_drect_inset (ArtDRect rectangle, double horizontal_inset, double vertical_inset)
{
	rectangle.x0 += horizontal_inset;
	rectangle.x1 -= horizontal_inset;
	rectangle.y0 += vertical_inset;
	rectangle.y1 -= vertical_inset;
	
	return rectangle;
}


#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_art_extensions (void)
{
	ArtIRect one;
	ArtIRect two;
	ArtIRect empty_rect = NAUTILUS_ART_IRECT_EMPTY;
	ArtIRect inside;
	ArtIRect outside;
	ArtIRect container;
	NautilusDimensions empty_dimensions = NAUTILUS_DIMENSIONS_EMPTY;
	NautilusDimensions dim1;

	nautilus_art_irect_assign (&one, 10, 10, 20, 20);
	nautilus_art_irect_assign (&two, 10, 10, 20, 20);
	nautilus_art_irect_assign (&inside, 11, 11, 18, 18);
	nautilus_art_irect_assign (&outside, 31, 31, 10, 10);
	nautilus_art_irect_assign (&container, 0, 0, 100, 100);

	/* nautilus_art_irect_equal */
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_equal (&one, &two), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_equal (&one, &empty_rect), FALSE);

	/* nautilus_art_irect_is_valid */
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_is_valid (NULL), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_is_valid (&empty_rect), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_is_valid (&one), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_is_valid (&two), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_is_valid (&inside), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_is_valid (&outside), TRUE);

	/* nautilus_art_irect_hits_irect */
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_hits_irect (&one, &two), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_hits_irect (&one, &inside), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_hits_irect (&one, &outside), FALSE);

	/* nautilus_art_irect_contains_point */
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 9, 9), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 9, 10), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 10, 9), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 10, 10), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 11, 10), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 10, 11), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 11, 11), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 30, 30), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 29, 30), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 30, 29), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_art_irect_contains_point (&one, 31, 31), FALSE);

	/* nautilus_art_irect_get_width */
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_art_irect_get_width (&one), 20);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_art_irect_get_width (&empty_rect), 0);

	/* nautilus_art_irect_get_height */
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_art_irect_get_height (&one), 20);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_art_irect_get_height (&empty_rect), 0);

	/* nautilus_art_irect_align */
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&empty_rect, 1, 1, 0.0, 0.0), 0, 0, 0, 0);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 0, 0, 0.0, 0.0), 0, 0, 0, 0);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 0, 0.0, 0.0), 0, 0, 0, 0);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 0, 9, 0.0, 0.0), 0, 0, 0, 0);

	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 10, 10, 0.0, 0.0), 0, 0, 10, 10);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 10, 10, 1.0, 0.0), 90, 0, 100, 10);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 10, 10, 0.0, 1.0), 0, 90, 10, 100);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 10, 10, 1.0, 1.0), 90, 90, 100, 100);

	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 9, 0.0, 0.0), 0, 0, 9, 9);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 9, 1.0, 0.0), 91, 0, 100, 9);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 9, 0.0, 1.0), 0, 91, 9, 100);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 9, 1.0, 1.0), 91, 91, 100, 100);

	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 10, 10, 0.5, 0.0), 45, 0, 55, 10);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 10, 10, 0.5, 0.0), 45, 0, 55, 10);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 10, 10, 0.0, 0.5), 0, 45, 10, 55);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 10, 10, 0.5, 0.5), 45, 45, 55, 55);

	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 9, 0.5, 0.0), 46, 0, 55, 9);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 9, 0.5, 0.0), 46, 0, 55, 9);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 9, 0.0, 0.5), 0, 46, 9, 55);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 9, 9, 0.5, 0.5), 46, 46, 55, 55);

	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 120, 120, 0.0, 0.0), 0, 0, 120, 120);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_align (&container, 120, 120, 0.5, 0.5), -10, -10, 110, 110);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_dimensions_empty (&empty_dimensions), TRUE);

	dim1.width = 10; dim1.height = 10;
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_dimensions_empty (&dim1), FALSE);

	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (0, 0, &dim1), 0, 0, 10, 10);

	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (1, 1, &dim1), 1, 1, 11, 11);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (-1, 1, &dim1), -1, 1, 9, 11);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (1, -1, &dim1), 1, -1, 11, 9);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (-1, -1, &dim1), -1, -1, 9, 9);

	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (2, 2, &dim1), 2, 2, 12, 12);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (-2, 2, &dim1), -2, 2, 8, 12);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (2, -2, &dim1), 2, -2, 12, 8);
	NAUTILUS_CHECK_RECTANGLE_RESULT (nautilus_art_irect_assign_dimensions (-2, -2, &dim1), -2, -2, 8, 8);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
