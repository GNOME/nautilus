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
eel_gdk_rgba_is_dark (const GdkRGBA *color)
{
	int intensity;

	intensity = ((((int) (color->red) >> 8) * 77)
		     + (((int) (color->green) >> 8) * 150)
		     + (((int) (color->blue) >> 8) * 28)) >> 8;

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
eel_cairo_draw_layout_with_drop_shadow (cairo_t     *cr,
					GdkRGBA     *text_color,
					GdkRGBA     *shadow_color,
					int          x,
					int          y,
					PangoLayout *layout)
{
        cairo_save (cr);

	gdk_cairo_set_source_rgba (cr, shadow_color);
	cairo_move_to (cr, x+1, y+1);
	pango_cairo_show_layout (cr, layout);
	
	gdk_cairo_set_source_rgba (cr, text_color);
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, layout);
	
	cairo_restore (cr);
}

#define CLAMP_COLOR(v) (t = (v), CLAMP (t, 0, 1))
#define SATURATE(v) ((1.0 - saturation) * intensity + saturation * (v))

void
eel_make_color_inactive (GdkRGBA *color)
{
	double intensity, saturation;
	gdouble t;

	saturation = 0.7;
	intensity = color->red * 0.30 + color->green * 0.59 + color->blue * 0.11;
	color->red = SATURATE (color->red);
	color->green = SATURATE (color->green);
	color->blue = SATURATE (color->blue);

	if (intensity > 0.5) {
		color->red *= 0.9;
		color->green *= 0.9;
		color->blue *= 0.9;
	} else {
		color->red *= 1.25;
		color->green *= 1.25;
		color->blue *= 1.25;
	}

	color->red = CLAMP_COLOR (color->red);
	color->green = CLAMP_COLOR (color->green);
	color->blue = CLAMP_COLOR (color->blue);
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
