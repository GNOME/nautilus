/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gdk-extensions.c - Possible additions for gdk.

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gdk-extensions.h"

#define GRADIENT_BAND_SIZE 4

/**
 * gdk_fill_rectangle:
 * @drawable: Target to draw into.
 * @gc: Graphics context (mainly for clip).
 * @rectangle: Rectangle to fill.
 *
 * Fill the rectangle with the foreground color.
 * Convenient when you have a GdkRectangle structure.
 */
void
gdk_fill_rectangle (GdkDrawable *drawable,
		    GdkGC *gc,
		    const GdkRectangle *rectangle)
{
	gdk_draw_rectangle (drawable, gc, TRUE,
			    rectangle->x, rectangle->y, rectangle->width, rectangle->height);
}

/**
 * gdk_fill_rectangle_with_color:
 * @drawable: Target to draw into.
 * @gc: Graphics context (mainly for clip).
 * @rectangle: Rectangle to fill.
 * @color: Color to fill with.
 *
 * Fill the rectangle with a color.
 * Convenient when you have a GdkRectangle structure.
 */
void
gdk_fill_rectangle_with_color (GdkDrawable *drawable,
			       GdkGC *gc,
			       const GdkRectangle *rectangle,
			       const GdkColor *color)
{
	GdkGCValues saved_values;
	
	gdk_gc_get_values(gc, &saved_values);
	gdk_gc_set_foreground (gc, (GdkColor *) color);
	gdk_fill_rectangle (drawable, gc, rectangle);
	gdk_gc_set_foreground (gc, &saved_values.foreground);
}

/**
 * gdk_fill_rectangle_with_gradient:
 * @drawable: Target to draw into.
 * @gc: Graphics context (mainly for clip).
 * @colormap: Map to use to allocate colors for gradient.
 * @rectangle: Rectangle to draw gradient in.
 * @start_color: Color for the left or top; pixel value does not matter.
 * @end_color: Color for the right or bottom; pixel value does not matter.
 * @horizontal: TRUE if the color changes from left to right. FALSE if from top to bottom.
 *
 * Fill the rectangle with a gradient.
 * The color changes from start_color to end_color.
 * A colormap is necessary because a gradient uses many different colors.
 * This effect works best on true color displays.
 */
void
gdk_fill_rectangle_with_gradient (GdkDrawable *drawable,
				  GdkGC *gc,
				  GdkColormap *colormap,
				  const GdkRectangle *rectangle,
				  const GdkColor *start_color,
				  const GdkColor *end_color,
				  gboolean horizontal)
{
	GdkRectangle band_box;
	gint16 *position;
	guint16 *size;
	gint num_bands;
	guint16 last_band_size;
	gdouble multiplier;
	gint band;

	g_return_if_fail (drawable);
	g_return_if_fail (gc);
	g_return_if_fail (rectangle);
	g_return_if_fail (start_color);
	g_return_if_fail (end_color);
	g_return_if_fail (horizontal == FALSE || horizontal == TRUE);

	/* Set up the band box so we can access it the same way for horizontal or vertical. */
	band_box = *rectangle;
	position = horizontal ? &band_box.x : &band_box.y;
	size = horizontal ? &band_box.width : &band_box.height;

	/* Figure out how many bands we will need. */
	num_bands = (*size + GRADIENT_BAND_SIZE - 1) / GRADIENT_BAND_SIZE;
	last_band_size = GRADIENT_BAND_SIZE - (GRADIENT_BAND_SIZE * num_bands - *size);

	/* Change the band box to be the size of a single band. */
	*size = GRADIENT_BAND_SIZE;

	/* Set up a multiplier to use to interpolate the colors as we go. */
	multiplier = num_bands <= 1 ? 0.0 : 1.0 / (num_bands - 1);
	
	/* Fill each band with a separate gdk_draw_rectangle call. */
	for (band = 0; band < num_bands; band++) {
		GdkColor band_color;

		/* Compute a new color value for each band. */
		gdk_interpolate_color(band * multiplier, start_color, end_color, &band_color);
		if (!gdk_colormap_alloc_color (colormap, &band_color, FALSE, TRUE))
			g_warning ("could not allocate color for gradient");
		else {
			/* Last band may need to be a bit smaller to avoid writing outside the box.
			 * This is more efficient than changing and restoring the clip.
			 */
			if (band == num_bands - 1)
				*size = last_band_size;

			gdk_fill_rectangle_with_color (drawable, gc, &band_box, &band_color);
		}
		*position += *size;
	}
}

/**
 * gdk_interpolate_color:
 * @ratio: Place on line between colors to interpolate.
 * @start_color: Color for one end.
 * @end_color: Color for the other end
 * @interpolated_color: Result.
 *
 * Compute a color between @start_color and @end_color in color space.
 * Currently, the color space used is RGB, but a future version could
 * instead do the interpolation in the best color space for expressing
 * human perception.
 */
void
gdk_interpolate_color (gdouble ratio,
		       const GdkColor *start_color,
		       const GdkColor *end_color,
		       GdkColor *interpolated_color)
{
	g_return_if_fail(ratio >= 0.0);
	g_return_if_fail(ratio <= 1.0);
	g_return_if_fail(start_color);
	g_return_if_fail(end_color);
	g_return_if_fail(interpolated_color);

	interpolated_color->red = start_color->red * (1.0 - ratio) + end_color->red * ratio;
	interpolated_color->green = start_color->green * (1.0 - ratio) + end_color->green * ratio;
	interpolated_color->blue = start_color->blue * (1.0 - ratio) + end_color->blue * ratio;
}
