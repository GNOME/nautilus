/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gdk-extensions.c: Graphics routines to augment what's in gdk.

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

   Authors: Darin Adler <darin@eazel.com>, 
            Pavel Cisler <pavel@eazel.com>,
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "eel-gdk-extensions.h"

#include "eel-glib-extensions.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <pango/pango.h>

#define GRADIENT_BAND_SIZE 4

/**
 * eel_gradient_new
 * @start_color: Color for the top or left.
 * @end_color: Color for the bottom or right.
 * @is_horizontal: Direction of the gradient.
 *
 * Create a string that combines the start and end colors along
 * with the direction of the gradient in a standard format.
 */
char *
eel_gradient_new (const char *start_color,
		       const char *end_color,
		       gboolean is_horizontal)
{
	/* Handle the special case where the start and end colors are identical.
	   Handle the special case where the end color is an empty string.
	*/
	if (eel_strcmp(start_color, end_color) == 0 || end_color == NULL || end_color[0] == '\0') {
		return g_strdup (start_color);
	}

	/* Handle the special case where the start color is an empty string. */
	if (start_color == NULL || start_color[0] == '\0') {
		return g_strdup (end_color);
	}
	
	/* Handle the general case. */
	return g_strconcat (start_color, "-", end_color, is_horizontal ? ":h" : NULL, NULL);
}

/**
 * eel_gradient_is_gradient
 * @gradient_spec: A gradient spec. string.
 *
 * Return true if the spec. specifies a gradient instead of a solid color.
 */
gboolean
eel_gradient_is_gradient (const char *gradient_spec)
{
	return eel_strchr (gradient_spec, '-') != NULL;
}

/**
 * eel_gradient_is_horizontal
 * @gradient_spec: A gradient spec. string.
 *
 * Return true if the spec. specifies a horizontal gradient.
 */
gboolean
eel_gradient_is_horizontal (const char *gradient_spec)
{
	size_t length;

	length = eel_strlen (gradient_spec);
	return length >= 2 && gradient_spec[length - 2] == ':' && gradient_spec[length - 1] == 'h';
}

static char *
eel_gradient_strip_trailing_direction_if_any (const char *gradient_spec)
{
	size_t length;

	length = eel_strlen (gradient_spec);
	if (length >= 2 && gradient_spec[length - 2] == ':'
	    && (gradient_spec[length - 1] == 'v' || gradient_spec[length - 1] == 'h')) {
		length -= 2;
	}

	return g_strndup (gradient_spec, length);
}

/* For parsing n-point gradients. Successive calls should pass the next_spec value
 * set by the previous call as their first argument - to continue parsing where the
 * previous call left off.
 */
char *
eel_gradient_parse_one_color_spec (const char *spec, int *percent, const char **next_spec)
{
	char *result;
	const char *rgb_end_ptr;
	const char *percent_ptr;
	const char *separator_ptr;
	
	percent_ptr   = eel_strchr (spec, '%');
	separator_ptr = eel_strchr (spec, '-');

	if (percent_ptr != NULL && (separator_ptr == NULL || percent_ptr < separator_ptr)) {
		if (percent != NULL) {
			*percent = (int) strtol (percent_ptr + 1, NULL, 10);
		}
		rgb_end_ptr = percent_ptr;
	} else {
		if (percent != NULL) {
			*percent = 100;
		}
		rgb_end_ptr = separator_ptr;
	}
		
	if (rgb_end_ptr != NULL) {
		result = g_strndup (spec, rgb_end_ptr - spec);
	} else {
		result = eel_gradient_strip_trailing_direction_if_any (spec);
	}

	/* It's important not to use spec after setting *next_spec because it's
	 * likely that *next_spec == spec. 
	 */
	if (next_spec != NULL) {
		*next_spec = (separator_ptr != NULL) ? separator_ptr + 1 : NULL;
	}

	return result;
}

/* FIXME bugzilla.eazel.com 5076:
 * anyone using eel_gradient_get_start_color_spec or
 * eel_gradient_get_end_color_spec is assuming the gradient
 * is 2 colors which is questionable.
 * 
 * Callers should be rewritten and these fns eliminated.
 */
 
/**
 * eel_gradient_get_start_color_spec
 * @gradient_spec: A gradient spec. string.
 *
 * Return the start color.
 * This may be the entire gradient_spec if it's a solid color.
 */
char *
eel_gradient_get_start_color_spec (const char *gradient_spec)
{
	return eel_gradient_parse_one_color_spec (gradient_spec, NULL, NULL);
}

/**
 * eel_gradient_get_end_color_spec
 * @gradient_spec: A gradient spec. string.
 *
 * Return the end color.
 * This may be the entire gradient_spec if it's a solid color.
 */
char *
eel_gradient_get_end_color_spec (const char *gradient_spec)
{
	char* color = NULL;

	do {
		g_free (color);
		color = eel_gradient_parse_one_color_spec (gradient_spec, NULL, &gradient_spec);
	} while (gradient_spec != NULL);

	return color;
}

/* Do the work shared by all the set_color_spec functions below. */
static char *
eel_gradient_set_edge_color (const char *gradient_spec,
				  const char *edge_color,
				  gboolean is_horizontal,
				  gboolean change_end)
{
	char *opposite_color;
	char *result;

	g_assert (edge_color != NULL);

	/* Get the color from the existing gradient spec. for the opposite
	   edge. This will parse away all the stuff we don't want from the
	   old gradient spec.
	*/
	opposite_color = change_end
		? eel_gradient_get_start_color_spec (gradient_spec)
		: eel_gradient_get_end_color_spec (gradient_spec);

	/* Create a new gradient spec. The eel_gradient_new function handles
	   some special cases, so we don't have to bother with them here.
	*/
	result = eel_gradient_new (change_end ? opposite_color : edge_color,
					change_end ? edge_color : opposite_color,
					is_horizontal);

	g_free (opposite_color);

	return result;
}

/**
 * eel_gradient_set_left_color_spec
 * @gradient_spec: A gradient spec. string.
 * @left_color: Color spec. to replace left color with.
 *
 * Changes the left color to what's passed in.
 * This creates a horizontal gradient.
 */
char *
eel_gradient_set_left_color_spec (const char *gradient_spec,
				       const char *left_color)
{
	g_return_val_if_fail (gradient_spec != NULL, NULL);
	g_return_val_if_fail (left_color != NULL, NULL);

	return eel_gradient_set_edge_color (gradient_spec, left_color, TRUE, FALSE);
}

/**
 * eel_gradient_set_top_color_spec
 * @gradient_spec: A gradient spec. string.
 * @top_color: Color spec. to replace top color with.
 *
 * Changes the top color to what's passed in.
 * This creates a vertical gradient.
 */
char *
eel_gradient_set_top_color_spec (const char *gradient_spec,
				      const char *top_color)
{
	g_return_val_if_fail (gradient_spec != NULL, NULL);
	g_return_val_if_fail (top_color != NULL, NULL);

	return eel_gradient_set_edge_color (gradient_spec, top_color, FALSE, FALSE);
}

/**
 * eel_gradient_set_right_color_spec
 * @gradient_spec: A gradient spec. string.
 * @right_color: Color spec. to replace right color with.
 *
 * Changes the right color to what's passed in.
 * This creates a horizontal gradient.
 */
char *
eel_gradient_set_right_color_spec (const char *gradient_spec,
					const char *right_color)
{
	g_return_val_if_fail (gradient_spec != NULL, NULL);
	g_return_val_if_fail (right_color != NULL, NULL);

	return eel_gradient_set_edge_color (gradient_spec, right_color, TRUE, TRUE);
}

/**
 * eel_gradient_set_bottom_color_spec
 * @gradient_spec: A gradient spec. string.
 * @bottom_color: Color spec. to replace bottom color with.
 *
 * Changes the bottom color to what's passed in.
 * This creates a vertical gradient.
 */
char *
eel_gradient_set_bottom_color_spec (const char *gradient_spec,
					 const char *bottom_color)
{
	g_return_val_if_fail (gradient_spec != NULL, NULL);
	g_return_val_if_fail (bottom_color != NULL, NULL);

	return eel_gradient_set_edge_color (gradient_spec, bottom_color, FALSE, TRUE);
}

/**
 * eel_gdk_color_parse_with_white_default
 * @color_spec: A color spec, or NULL.
 * @color: Pointer to place to put resulting color.
 *
 * The same as gdk_color_parse, except sets the color to white if
 * the spec. can't be parsed, instead of returning a boolean flag.
 */
void
eel_gdk_color_parse_with_white_default (const char *color_spec,
					GdkColor *color)
{
	gboolean got_color;

	g_return_if_fail (color != NULL);

	got_color = FALSE;
	if (color_spec != NULL) {
		if (gdk_color_parse (color_spec, color)) {
			got_color = TRUE;
		}
	}

	if (!got_color) {
		color->red = 0xFFFF;
		color->green = 0xFFFF;
		color->blue = 0xFFFF;
	}
}

guint32
eel_rgb16_to_rgb (gushort r, gushort g, gushort b)
{
	guint32 result;

	result = (0xff0000 | (r & 0xff00));
	result <<= 8;
	result |= ((g & 0xff00) | (b >> 8));

	return result;
}

guint32
eel_rgb8_to_rgb (guchar r, guchar g, guchar b)
{
	return eel_rgb16_to_rgb (r << 8, g << 8, b << 8);
}

/**
 * eel_gdk_color_to_rgb
 * @color: A GdkColor style color.
 * Returns: An rgb value.
 *
 * Converts from a GdkColor stlye color to a gdk_rgb one.
 * Alpha gets set to fully opaque
 */
guint32
eel_gdk_color_to_rgb (const GdkColor *color)
{
	return eel_rgb16_to_rgb (color->red, color->green, color->blue);
}

/**
 * eel_gdk_rgb_to_color
 * @color: a gdk_rgb style value.
 *
 * Converts from a gdk_rgb value style to a GdkColor one.
 * The gdk_rgb color alpha channel is ignored.
 * 
 * Return value: A GdkColor structure version of the given RGB color.
 */
GdkColor
eel_gdk_rgb_to_color (guint32 color)
{
	GdkColor result;

	result.red = ((color >> 16) & 0xFF) * 0x101;
	result.green = ((color >> 8) & 0xFF) * 0x101;
	result.blue = (color & 0xff) * 0x101;
	result.pixel = 0;

	return result;
}

/**
 * eel_gdk_rgb_to_color_spec
 * @color: a gdk_rgb style value.
 *
 * Converts from a gdk_rgb value style to a string color spec.
 * The gdk_rgb color alpha channel is ignored.
 * 
 * Return value: a newly allocated color spec.
 */
char *
eel_gdk_rgb_to_color_spec (const guint32 color)
{
	return g_strdup_printf ("#%06X", (guint) (color & 0xFFFFFF));
}

/**
 * eel_gdk_color_is_dark:
 * 
 * Return true if the given color is `dark'
 */
gboolean
eel_gdk_color_is_dark (GdkColor *color)
{
	int intensity;

	intensity = (((color->red >> 8) * 77)
		     + ((color->green >> 8) * 150)
		     + ((color->blue >> 8) * 28)) >> 8;

	return intensity < 128;
}

EelGdkGeometryFlags
eel_gdk_parse_geometry (const char *string, int *x_return, int *y_return,
			     guint *width_return, guint *height_return)
{
	int x11_flags;
	EelGdkGeometryFlags gdk_flags;

	g_return_val_if_fail (string != NULL, EEL_GDK_NO_VALUE);
	g_return_val_if_fail (x_return != NULL, EEL_GDK_NO_VALUE);
	g_return_val_if_fail (y_return != NULL, EEL_GDK_NO_VALUE);
	g_return_val_if_fail (width_return != NULL, EEL_GDK_NO_VALUE);
	g_return_val_if_fail (height_return != NULL, EEL_GDK_NO_VALUE);

	x11_flags = XParseGeometry (string, x_return, y_return,
				    width_return, height_return);

	gdk_flags = EEL_GDK_NO_VALUE;
	if (x11_flags & XValue) {
		gdk_flags |= EEL_GDK_X_VALUE;
	}
	if (x11_flags & YValue) {
		gdk_flags |= EEL_GDK_Y_VALUE;
	}
	if (x11_flags & WidthValue) {
		gdk_flags |= EEL_GDK_WIDTH_VALUE;
	}
	if (x11_flags & HeightValue) {
		gdk_flags |= EEL_GDK_HEIGHT_VALUE;
	}
	if (x11_flags & XNegative) {
		gdk_flags |= EEL_GDK_X_NEGATIVE;
	}
	if (x11_flags & YNegative) {
		gdk_flags |= EEL_GDK_Y_NEGATIVE;
	}

	return gdk_flags;
}

void
eel_gdk_draw_layout_with_drop_shadow (GdkDrawable         *drawable,
				      GdkColor            *text_color,
				      GdkColor            *shadow_color,
				      int                  x,
				      int                  y,
				      PangoLayout         *layout)
{
	cairo_t *cr;

	cr = gdk_cairo_create (drawable);

	gdk_cairo_set_source_color (cr, shadow_color);
	cairo_move_to (cr, x+1, y+1);
	pango_cairo_show_layout (cr, layout);
	
	gdk_cairo_set_source_color (cr, text_color);
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, layout);
	
	cairo_destroy (cr);
}

#if ! defined (EEL_OMIT_SELF_CHECK)

static char *
eel_gdk_color_as_hex_string (GdkColor color)
{
	return g_strdup_printf ("%04X%04X%04X",
				color.red, color.green, color.blue);
}

static char *
eel_self_check_parse (const char *color_spec)
{
	GdkColor color;

	eel_gdk_color_parse_with_white_default (color_spec, &color);
	return eel_gdk_color_as_hex_string (color);
}

static char *
eel_self_check_gdk_rgb_to_color (guint32 color)
{
	GdkColor result;

	result = eel_gdk_rgb_to_color (color);

	return eel_gdk_color_as_hex_string (result);
}

void
eel_self_check_gdk_extensions (void)
{
	/* eel_gradient_new */
	EEL_CHECK_STRING_RESULT (eel_gradient_new ("", "", FALSE), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_new ("a", "b", FALSE), "a-b");
	EEL_CHECK_STRING_RESULT (eel_gradient_new ("a", "b", TRUE), "a-b:h");
	EEL_CHECK_STRING_RESULT (eel_gradient_new ("a", "a", FALSE), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_new ("a", "a", TRUE), "a");

	/* eel_gradient_is_gradient */
	EEL_CHECK_BOOLEAN_RESULT (eel_gradient_is_gradient (""), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_gradient_is_gradient ("-"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_gradient_is_gradient ("a"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_gradient_is_gradient ("a-b"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_gradient_is_gradient ("a-b:h"), TRUE);

	/* eel_gradient_get_start_color_spec */
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec (""), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("-"), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("a-b"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("a-"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("-b"), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("a:h"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("a:v"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("a:c"), "a:c");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("a:-b"), "a:");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_start_color_spec ("a:-b:v"), "a:");

	/* eel_gradient_get_end_color_spec */
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec (""), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("-"), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("a-b"), "b");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("a-"), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("-b"), "b");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("a:h"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("a:v"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("a:c"), "a:c");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("a:-b"), "b");
	EEL_CHECK_STRING_RESULT (eel_gradient_get_end_color_spec ("a:-b:v"), "b");

	/* eel_gradient_set_left_color_spec */
	EEL_CHECK_STRING_RESULT (eel_gradient_set_left_color_spec ("", ""), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_left_color_spec ("", "a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_left_color_spec ("a", ""), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_left_color_spec ("a", "a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_left_color_spec ("a", "b"), "b-a:h");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_left_color_spec ("a-c:v", "b"), "b-c:h");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_left_color_spec ("a-c:v", "c"), "c");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_left_color_spec ("a:-b:v", "d"), "d-b:h");

	/* eel_gradient_set_top_color_spec */
	EEL_CHECK_STRING_RESULT (eel_gradient_set_top_color_spec ("", ""), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_top_color_spec ("", "a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_top_color_spec ("a", ""), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_top_color_spec ("a", "a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_top_color_spec ("a", "b"), "b-a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_top_color_spec ("a-c:v", "b"), "b-c");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_top_color_spec ("a-c:v", "c"), "c");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_top_color_spec ("a:-b:h", "d"), "d-b");

	/* eel_gradient_set_right_color_spec */
	EEL_CHECK_STRING_RESULT (eel_gradient_set_right_color_spec ("", ""), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_right_color_spec ("", "a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_right_color_spec ("a", ""), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_right_color_spec ("a", "a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_right_color_spec ("a", "b"), "a-b:h");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_right_color_spec ("a-c:v", "b"), "a-b:h");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_right_color_spec ("a-c:v", "c"), "a-c:h");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_right_color_spec ("a:-b:v", "d"), "a:-d:h");

	/* eel_gradient_set_bottom_color_spec */
	EEL_CHECK_STRING_RESULT (eel_gradient_set_bottom_color_spec ("", ""), "");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_bottom_color_spec ("", "a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_bottom_color_spec ("a", ""), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_bottom_color_spec ("a", "a"), "a");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_bottom_color_spec ("a", "b"), "a-b");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_bottom_color_spec ("a-c:v", "b"), "a-b");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_bottom_color_spec ("a-c:v", "c"), "a-c");
	EEL_CHECK_STRING_RESULT (eel_gradient_set_bottom_color_spec ("a:-b:h", "d"), "a:-d");

	/* eel_gdk_color_parse_with_white_default */
	EEL_CHECK_STRING_RESULT (eel_self_check_parse (""), "FFFFFFFFFFFF");
	EEL_CHECK_STRING_RESULT (eel_self_check_parse ("a"), "FFFFFFFFFFFF");
	EEL_CHECK_STRING_RESULT (eel_self_check_parse ("white"), "FFFFFFFFFFFF");
	EEL_CHECK_STRING_RESULT (eel_self_check_parse ("black"), "000000000000");
	EEL_CHECK_STRING_RESULT (eel_self_check_parse ("red"), "FFFF00000000");
	EEL_CHECK_STRING_RESULT (eel_self_check_parse ("#012345"), "010123234545");
	/* EEL_CHECK_STRING_RESULT (eel_self_check_parse ("rgb:0123/4567/89AB"), "#014589"); */

	/* eel_gdk_rgb_to_color */
	EEL_CHECK_STRING_RESULT (eel_self_check_gdk_rgb_to_color (EEL_RGB_COLOR_RED), "FFFF00000000");
	EEL_CHECK_STRING_RESULT (eel_self_check_gdk_rgb_to_color (EEL_RGB_COLOR_BLACK), "000000000000");
	EEL_CHECK_STRING_RESULT (eel_self_check_gdk_rgb_to_color (EEL_RGB_COLOR_WHITE), "FFFFFFFFFFFF");
	EEL_CHECK_STRING_RESULT (eel_self_check_gdk_rgb_to_color (EEL_RGB_COLOR_PACK (0x01, 0x23, 0x45)), "010123234545");
	
	/* EEL_RGBA_COLOR_PACK */
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0xFF, 0x00, 0x00, 00), EEL_RGB_COLOR_RED);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0x00, 0xFF, 0x00, 00), EEL_RGB_COLOR_GREEN);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0x00, 0x00, 0xFF, 00), EEL_RGB_COLOR_BLUE);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0xFF, 0xFF, 0xFF, 00), EEL_RGB_COLOR_WHITE);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0x00, 0x00, 0x00, 00), EEL_RGB_COLOR_BLACK);

	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0xFF, 0x00, 0x00, 0xFF), EEL_RGBA_COLOR_OPAQUE_RED);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0x00, 0xFF, 0x00, 0xFF), EEL_RGBA_COLOR_OPAQUE_GREEN);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0x00, 0x00, 0xFF, 0xFF), EEL_RGBA_COLOR_OPAQUE_BLUE);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0xFF, 0xFF, 0xFF, 0xFF), EEL_RGBA_COLOR_OPAQUE_WHITE);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_PACK (0x00, 0x00, 0x00, 0xFF), EEL_RGBA_COLOR_OPAQUE_BLACK);

	/* EEL_RGB_COLOR_PACK */
	EEL_CHECK_INTEGER_RESULT (EEL_RGB_COLOR_PACK (0xFF, 0x00, 0x00), EEL_RGBA_COLOR_OPAQUE_RED);
	EEL_CHECK_INTEGER_RESULT (EEL_RGB_COLOR_PACK (0x00, 0xFF, 0x00), EEL_RGBA_COLOR_OPAQUE_GREEN);
	EEL_CHECK_INTEGER_RESULT (EEL_RGB_COLOR_PACK (0x00, 0x00, 0xFF), EEL_RGBA_COLOR_OPAQUE_BLUE);
	EEL_CHECK_INTEGER_RESULT (EEL_RGB_COLOR_PACK (0xFF, 0xFF, 0xFF), EEL_RGBA_COLOR_OPAQUE_WHITE);
	EEL_CHECK_INTEGER_RESULT (EEL_RGB_COLOR_PACK (0x00, 0x00, 0x00), EEL_RGBA_COLOR_OPAQUE_BLACK);

	/* EEL_RGBA_COLOR_GET_R */
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGBA_COLOR_OPAQUE_RED), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGBA_COLOR_OPAQUE_GREEN), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGBA_COLOR_OPAQUE_BLUE), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGBA_COLOR_OPAQUE_WHITE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGBA_COLOR_OPAQUE_BLACK), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGB_COLOR_RED), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGB_COLOR_GREEN), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGB_COLOR_BLUE), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGB_COLOR_WHITE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_R (EEL_RGB_COLOR_BLACK), 0x00);

	/* EEL_RGBA_COLOR_GET_G */
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGBA_COLOR_OPAQUE_RED), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGBA_COLOR_OPAQUE_GREEN), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGBA_COLOR_OPAQUE_BLUE), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGBA_COLOR_OPAQUE_WHITE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGBA_COLOR_OPAQUE_BLACK), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGB_COLOR_RED), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGB_COLOR_GREEN), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGB_COLOR_BLUE), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGB_COLOR_WHITE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_G (EEL_RGB_COLOR_BLACK), 0x00);

	/* EEL_RGBA_COLOR_GET_B */
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGBA_COLOR_OPAQUE_RED), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGBA_COLOR_OPAQUE_GREEN), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGBA_COLOR_OPAQUE_BLUE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGBA_COLOR_OPAQUE_WHITE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGBA_COLOR_OPAQUE_BLACK), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGB_COLOR_RED), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGB_COLOR_GREEN), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGB_COLOR_BLUE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGB_COLOR_WHITE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_B (EEL_RGB_COLOR_BLACK), 0x00);

	/* EEL_RGBA_COLOR_GET_A */
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGBA_COLOR_OPAQUE_RED), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGBA_COLOR_OPAQUE_GREEN), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGBA_COLOR_OPAQUE_BLUE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGBA_COLOR_OPAQUE_WHITE), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGBA_COLOR_OPAQUE_BLACK), 0xFF);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGB_COLOR_RED), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGB_COLOR_GREEN), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGB_COLOR_BLUE), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGB_COLOR_WHITE), 0x00);
	EEL_CHECK_INTEGER_RESULT (EEL_RGBA_COLOR_GET_A (EEL_RGB_COLOR_BLACK), 0x00);

}

#endif /* ! EEL_OMIT_SELF_CHECK */
