/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gdk-extensions.c: Graphics routines to augment what's in gdk.

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
#include "nautilus-gdk-extensions.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"

#define GRADIENT_BAND_SIZE 4

/**
 * nautilus_fill_rectangle:
 * @drawable: Target to draw into.
 * @gc: Graphics context (mainly for clip).
 * @rectangle: Rectangle to fill.
 *
 * Fill the rectangle with the foreground color.
 * Convenient when you have a GdkRectangle structure.
 */
void
nautilus_fill_rectangle (GdkDrawable *drawable,
			 GdkGC *gc,
			 const GdkRectangle *rectangle)
{
	gdk_draw_rectangle (drawable, gc, TRUE,
			    rectangle->x, rectangle->y, rectangle->width, rectangle->height);
}

/**
 * nautilus_fill_rectangle_with_color:
 * @drawable: Target to draw into.
 * @gc: Graphics context (mainly for clip).
 * @rectangle: Rectangle to fill.
 * @color: Color to fill with.
 *
 * Fill the rectangle with a color.
 * Convenient when you have a GdkRectangle structure.
 */
void
nautilus_fill_rectangle_with_color (GdkDrawable *drawable,
				    GdkGC *gc,
				    const GdkRectangle *rectangle,
				    const GdkColor *color)
{
	GdkGCValues saved_values;
	
	gdk_gc_get_values(gc, &saved_values);
	gdk_gc_set_foreground (gc, (GdkColor *) color);
	nautilus_fill_rectangle (drawable, gc, rectangle);
	gdk_gc_set_foreground (gc, &saved_values.foreground);
}

/**
 * nautilus_fill_rectangle_with_gradient:
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
nautilus_fill_rectangle_with_gradient (GdkDrawable *drawable,
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

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (rectangle != NULL);
	g_return_if_fail (start_color != NULL);
	g_return_if_fail (end_color != NULL);
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
	
	/* Fill each band with a separate nautilus_draw_rectangle call. */
	for (band = 0; band < num_bands; band++) {
		GdkColor band_color;

		/* Compute a new color value for each band. */
		nautilus_interpolate_color (band * multiplier, start_color, end_color, &band_color);
		if (!gdk_colormap_alloc_color (colormap, &band_color, FALSE, TRUE))
			g_warning ("could not allocate color for gradient");
		else {
			/* Last band may need to be a bit smaller to avoid writing outside the box.
			 * This is more efficient than changing and restoring the clip.
			 */
			if (band == num_bands - 1)
				*size = last_band_size;

			nautilus_fill_rectangle_with_color (drawable, gc, &band_box, &band_color);
		}
		*position += *size;
	}
}

/**
 * nautilus_interpolate_color:
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
nautilus_interpolate_color (gdouble ratio,
			    const GdkColor *start_color,
			    const GdkColor *end_color,
			    GdkColor *interpolated_color)
{
	g_return_if_fail (ratio >= 0.0);
	g_return_if_fail (ratio <= 1.0);
	g_return_if_fail (start_color);
	g_return_if_fail (end_color);
	g_return_if_fail (interpolated_color);

	interpolated_color->red = start_color->red * (1.0 - ratio) + end_color->red * ratio;
	interpolated_color->green = start_color->green * (1.0 - ratio) + end_color->green * ratio;
	interpolated_color->blue = start_color->blue * (1.0 - ratio) + end_color->blue * ratio;
}

/**
 * nautilus_gradient_new
 * @start_color: Color for the top or left.
 * @end_color: Color for the bottom or right.
 * @is_horizontal: Direction of the gradient.
 *
 * Create a string that combines the start and end colors along
 * with the direction of the gradient in a standard format.
 */
char *
nautilus_gradient_new (const char *start_color,
		       const char *end_color,
		       gboolean is_horizontal)
{
	g_return_val_if_fail (is_horizontal == FALSE || is_horizontal == TRUE, NULL);

	/* Handle the special case where the start and end colors are identical.
	   Handle the special case where the end color is an empty string.
	*/
	if (nautilus_strcmp(start_color, end_color) == 0 || end_color == NULL || end_color[0] == '\0')
		return g_strdup (start_color);

	/* Handle the special case where the start color is an empty string. */
	if (start_color == NULL || start_color[0] == '\0')
		return g_strdup (end_color);
	
	/* Handle the general case. */
	return g_strconcat (start_color, "-", end_color, is_horizontal ? ":h" : NULL, NULL);
}

/**
 * nautilus_gradient_is_gradient
 * @gradient_spec: A gradient spec. string.
 *
 * Return true if the spec. specifies a gradient instead of a solid color.
 */
gboolean
nautilus_gradient_is_gradient (const char *gradient_spec)
{
	return nautilus_strchr (gradient_spec, '-') != NULL;
}

/**
 * nautilus_gradient_is_horizontal
 * @gradient_spec: A gradient spec. string.
 *
 * Return true if the spec. specifies a horizontal gradient.
 */
gboolean
nautilus_gradient_is_horizontal (const char *gradient_spec)
{
	size_t length;

	length = nautilus_strlen (gradient_spec);
	return length >= 2 && gradient_spec[length - 2] == ':' && gradient_spec[length - 1] == 'h';
}

static char *
nautilus_gradient_strip_trailing_direction_if_any (const char *gradient_spec)
{
	size_t length;

	length = nautilus_strlen (gradient_spec);
	if (length >= 2 && gradient_spec[length - 2] == ':'
	    && (gradient_spec[length - 1] == 'v' || gradient_spec[length - 1] == 'h'))
		length -= 2;

	return g_strndup (gradient_spec, length);
}

/**
 * nautilus_gradient_get_start_color_spec
 * @gradient_spec: A gradient spec. string.
 *
 * Return the start color.
 * This may be the entire gradient_spec if it's a solid color.
 */
char *
nautilus_gradient_get_start_color_spec (const char *gradient_spec)
{
	const char *separator;
	
	separator = nautilus_strchr (gradient_spec, '-');
	if (separator == NULL)
		return nautilus_gradient_strip_trailing_direction_if_any (gradient_spec);

	return g_strndup (gradient_spec, separator - gradient_spec);
}

/**
 * nautilus_gradient_get_end_color_spec
 * @gradient_spec: A gradient spec. string.
 *
 * Return the end color.
 * This may be the entire gradient_spec if it's a solid color.
 */
char *
nautilus_gradient_get_end_color_spec (const char *gradient_spec)
{
	const char *separator;

	separator = nautilus_strchr (gradient_spec, '-');
	return nautilus_gradient_strip_trailing_direction_if_any
		(separator != NULL ? separator + 1 : gradient_spec);
}

/* Do the work shared by all the set_color_spec functions below. */
static char *
nautilus_gradient_set_edge_color (const char *gradient_spec,
				  const char *edge_color,
				  gboolean is_horizontal,
				  gboolean change_end)
{
	char *opposite_color;
	char *result;

	g_return_val_if_fail (edge_color != NULL, g_strdup (gradient_spec));

	/* Get the color from the existing gradient spec. for the opposite
	   edge. This will parse away all the stuff we don't want from the
	   old gradient spec.
	*/
	opposite_color = change_end
		? nautilus_gradient_get_start_color_spec (gradient_spec)
		: nautilus_gradient_get_end_color_spec (gradient_spec);

	/* Create a new gradient spec. The nautilus_gradient_new function handles
	   some special cases, so we don't have to bother with them here.
	*/
	result = nautilus_gradient_new (change_end ? opposite_color : edge_color,
					change_end ? edge_color : opposite_color,
					is_horizontal);

	g_free (opposite_color);

	return result;
}

/**
 * nautilus_gradient_set_left_color_spec
 * @gradient_spec: A gradient spec. string.
 * @left_color: Color spec. to replace left color with.
 *
 * Changes the left color to what's passed in.
 * This creates a horizontal gradient.
 */
char *
nautilus_gradient_set_left_color_spec (const char *gradient_spec,
				       const char *left_color)
{
	return nautilus_gradient_set_edge_color (gradient_spec, left_color, TRUE, FALSE);
}

/**
 * nautilus_gradient_set_top_color_spec
 * @gradient_spec: A gradient spec. string.
 * @top_color: Color spec. to replace top color with.
 *
 * Changes the top color to what's passed in.
 * This creates a vertical gradient.
 */
char *
nautilus_gradient_set_top_color_spec (const char *gradient_spec,
				      const char *top_color)
{
	return nautilus_gradient_set_edge_color (gradient_spec, top_color, FALSE, FALSE);
}

/**
 * nautilus_gradient_set_right_color_spec
 * @gradient_spec: A gradient spec. string.
 * @right_color: Color spec. to replace right color with.
 *
 * Changes the right color to what's passed in.
 * This creates a horizontal gradient.
 */
char *
nautilus_gradient_set_right_color_spec (const char *gradient_spec,
					const char *right_color)
{
	return nautilus_gradient_set_edge_color (gradient_spec, right_color, TRUE, TRUE);
}

/**
 * nautilus_gradient_set_bottom_color_spec
 * @gradient_spec: A gradient spec. string.
 * @bottom_color: Color spec. to replace bottom color with.
 *
 * Changes the bottom color to what's passed in.
 * This creates a vertical gradient.
 */
char *
nautilus_gradient_set_bottom_color_spec (const char *gradient_spec,
					 const char *bottom_color)
{
	return nautilus_gradient_set_edge_color (gradient_spec, bottom_color, FALSE, TRUE);
}

/**
 * nautilus_gdk_color_parse_with_white_default
 * @color_spec: A color spec.
 * @color: Pointer to place to put resulting color.
 *
 * The same as gdk_color_parse, except sets the color to white if
 * the spec. can't be parsed instead of returning a boolean flag.
 */
void
nautilus_gdk_color_parse_with_white_default (const char *color_spec,
					     GdkColor *color)
{
	if (color_spec == NULL || !gdk_color_parse (color_spec, color)) {
		color->red = 0xFFFF;
		color->green = 0xFFFF;
		color->blue = 0xFFFF;
	}
}

/**
 * nautilus_gdk_font_equal
 * @font_a_null_allowed: A font or NULL.
 * @font_b_null_allowed: A font or NULL.
 *
 * Calls gdk_font_equal, unless one of the fonts is NULL.
 */
gboolean
nautilus_gdk_font_equal (GdkFont *font_a_null_allowed,
			 GdkFont *font_b_null_allowed)
{
	if (font_a_null_allowed == NULL)
		return font_b_null_allowed == NULL;
	if (font_b_null_allowed == NULL)
		return FALSE;
	return gdk_font_equal (font_a_null_allowed, font_b_null_allowed);
}

/**
 * nautilus_gdk_pixbuf_list_ref
 * @pixbuf_list: A list of GdkPixbuf objects.
 *
 * Refs all the pixbufs.
 **/
void
nautilus_gdk_pixbuf_list_ref (GList *pixbuf_list)
{
	g_list_foreach (pixbuf_list, (GFunc) gdk_pixbuf_ref, NULL);
}

/**
 * nautilus_gdk_pixbuf_list_free
 * @pixbuf_list: A list of GdkPixbuf objects.
 *
 * Unrefs all the pixbufs, then frees the list.
 **/
void
nautilus_gdk_pixbuf_list_free (GList *pixbuf_list)
{
	g_list_foreach (pixbuf_list, (GFunc) gdk_pixbuf_unref, NULL);
	g_list_free (pixbuf_list);
}

#if ! defined (NAUTILUS_OMIT_SELF_CHECK)

static char *
nautilus_gdk_color_as_hex_string (GdkColor color)
{
	return g_strdup_printf("rgb:%04hX/%04hX/%04hX",
			       color.red, color.green, color.blue);
}

static char *
nautilus_self_check_interpolate (gdouble ratio,
				 gushort r1, gushort g1, gushort b1,
				 gushort r2, gushort g2, gushort b2)
{
	GdkColor start_color;
	GdkColor end_color;
	GdkColor interpolated_color;

	start_color.red = r1;
	start_color.green = g1;
	start_color.blue = b1;

	end_color.red = r2;
	end_color.green = g2;
	end_color.blue = b2;

	nautilus_interpolate_color (ratio, &start_color, &end_color, &interpolated_color);

	return nautilus_gdk_color_as_hex_string (interpolated_color);
}

static char *
nautilus_self_check_parse (const char *color_spec)
{
	GdkColor color;

	nautilus_gdk_color_parse_with_white_default (color_spec, &color);
	return nautilus_gdk_color_as_hex_string (color);
}

void
nautilus_self_check_gdk_extensions (void)
{
	/* nautilus_interpolate_color */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_interpolate (0.0, 0, 0, 0, 0, 0, 0),
				      "rgb:0000/0000/0000");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_interpolate (0.0, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF),
				      "rgb:0000/0000/0000");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_interpolate (0.5, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF),
				      "rgb:7FFF/7FFF/7FFF");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_interpolate (1.0, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF),
				      "rgb:FFFF/FFFF/FFFF");

	/* nautilus_fill_rectangle */
	/* Make a GdkImage and fill it, maybe? */

	/* nautilus_fill_rectangle_with_color */

	/* nautilus_fill_rectangle_with_gradient */

	/* nautilus_gradient_new */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_new ("", "", FALSE), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_new ("a", "b", FALSE), "a-b");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_new ("a", "b", TRUE), "a-b:h");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_new ("a", "a", FALSE), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_new ("a", "a", TRUE), "a");

	/* nautilus_gradient_is_gradient */
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_gradient_is_gradient (""), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_gradient_is_gradient ("-"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_gradient_is_gradient ("a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_gradient_is_gradient ("a-b"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_gradient_is_gradient ("a-b:h"), TRUE);

	/* nautilus_gradient_get_start_color_spec */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("-"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("a-b"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("a-"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("-b"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("a:h"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("a:v"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("a:c"), "a:c");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("a:-b"), "a:");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_start_color_spec ("a:-b:v"), "a:");

	/* nautilus_gradient_get_end_color_spec */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("-"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("a-b"), "b");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("a-"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("-b"), "b");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("a:h"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("a:v"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("a:c"), "a:c");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("a:-b"), "b");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_get_end_color_spec ("a:-b:v"), "b");

	/* nautilus_gradient_set_left_color_spec */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_left_color_spec ("", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_left_color_spec ("", "a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_left_color_spec ("a", ""), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_left_color_spec ("a", "a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_left_color_spec ("a", "b"), "b-a:h");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_left_color_spec ("a-c:v", "b"), "b-c:h");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_left_color_spec ("a-c:v", "c"), "c");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_left_color_spec ("a:-b:v", "d"), "d-b:h");

	/* nautilus_gradient_set_top_color_spec */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_top_color_spec ("", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_top_color_spec ("", "a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_top_color_spec ("a", ""), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_top_color_spec ("a", "a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_top_color_spec ("a", "b"), "b-a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_top_color_spec ("a-c:v", "b"), "b-c");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_top_color_spec ("a-c:v", "c"), "c");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_top_color_spec ("a:-b:h", "d"), "d-b");

	/* nautilus_gradient_set_right_color_spec */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_right_color_spec ("", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_right_color_spec ("", "a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_right_color_spec ("a", ""), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_right_color_spec ("a", "a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_right_color_spec ("a", "b"), "a-b:h");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_right_color_spec ("a-c:v", "b"), "a-b:h");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_right_color_spec ("a-c:v", "c"), "a-c:h");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_right_color_spec ("a:-b:v", "d"), "a:-d:h");

	/* nautilus_gradient_set_bottom_color_spec */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_bottom_color_spec ("", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_bottom_color_spec ("", "a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_bottom_color_spec ("a", ""), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_bottom_color_spec ("a", "a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_bottom_color_spec ("a", "b"), "a-b");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_bottom_color_spec ("a-c:v", "b"), "a-b");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_bottom_color_spec ("a-c:v", "c"), "a-c");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_gradient_set_bottom_color_spec ("a:-b:h", "d"), "a:-d");

	/* nautilus_gdk_color_parse_with_white_default */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_parse (""), "rgb:FFFF/FFFF/FFFF");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_parse ("a"), "rgb:FFFF/FFFF/FFFF");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_parse ("white"), "rgb:FFFF/FFFF/FFFF");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_parse ("black"), "rgb:0000/0000/0000");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_parse ("rgb:0123/4567/89AB"), "rgb:0123/4567/89AB");
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
