/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gnome-extensions.c - implementation of new functions that operate on
                                 gnome classes. Perhaps some of these should be
  			         rolled into gnome someday.

   Copyright (C) 1999, 2000 Eazel, Inc.

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
*/

#include <config.h>
#include "nautilus-gnome-extensions.h"

void
nautilus_gnome_canvas_world_to_window_rectangle (GnomeCanvas *canvas,
						 const ArtDRect *world_rect,
						 ArtIRect *window_rect)
{
	double x0, y0, x1, y1;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (world_rect != NULL);
	g_return_if_fail (window_rect != NULL);

	gnome_canvas_world_to_window (canvas,
				      world_rect->x0,
				      world_rect->y0,
				      &x0, &y0);
	gnome_canvas_world_to_window (canvas,
				      world_rect->x1,
				      world_rect->y1,
				      &x1, &y1);

	window_rect->x0 = x0;
	window_rect->y0 = y0;
	window_rect->x1 = x1;
	window_rect->y1 = y1;
}

void
nautilus_gnome_canvas_world_to_canvas_rectangle (GnomeCanvas *canvas,
						 const ArtDRect *world_rect,
						 ArtIRect *canvas_rect)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (world_rect != NULL);
	g_return_if_fail (canvas_rect != NULL);

	gnome_canvas_w2c (canvas,
			  world_rect->x0,
			  world_rect->y0,
			  &canvas_rect->x0,
			  &canvas_rect->y0);
	gnome_canvas_w2c (canvas,
			  world_rect->x1,
			  world_rect->y1,
			  &canvas_rect->x1,
			  &canvas_rect->y1);
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
nautilus_gnome_canvas_item_get_current_canvas_bounds (GnomeCanvasItem *item,
						      ArtIRect *bounds)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (bounds != NULL);

	bounds->x0 = item->x1;
	bounds->y0 = item->y1;
	bounds->x1 = item->x2;
	bounds->y1 = item->y2;
}

void
nautilus_gnome_canvas_item_request_redraw (GnomeCanvasItem *item)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	gnome_canvas_request_redraw (item->canvas,
				     item->x1, item->y1,
				     item->x2, item->y2);
}

void
nautilus_gnome_canvas_request_redraw_rectangle (GnomeCanvas *canvas,
						const ArtIRect *canvas_rectangle)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	gnome_canvas_request_redraw (canvas,
				     canvas_rectangle->x0, canvas_rectangle->y0,
				     canvas_rectangle->x1, canvas_rectangle->y1);
}

void
nautilus_gnome_canvas_item_get_world_bounds (GnomeCanvasItem *item,
					     ArtDRect *world_bounds)
{
	gnome_canvas_item_get_bounds (item,
				      &world_bounds->x0,
				      &world_bounds->y0,
				      &world_bounds->x1,
				      &world_bounds->y1);
	if (item->parent != NULL) {
		gnome_canvas_item_i2w (item->parent,
				       &world_bounds->x0,
				       &world_bounds->y0);
		gnome_canvas_item_i2w (item->parent,
				       &world_bounds->x1,
				       &world_bounds->y1);
	}
}
