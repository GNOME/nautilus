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
#include <math.h>

ArtIRect NAUTILUS_ART_IRECT_EMPTY = { 0, 0, 0, 0 };
NautilusArtIPoint NAUTILUS_ART_IPOINT_ZERO = { 0, 0 };

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

void
nautilus_art_ipoint_assign (NautilusArtIPoint *point,
			   int x,
			   int y)
{
	g_return_if_fail (point != NULL);

	point->x = x;
	point->y = y;
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
