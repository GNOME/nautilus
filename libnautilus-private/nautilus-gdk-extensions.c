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

   Authors: Darin Adler <darin@eazel.com>, 
            Pavel Cisler <pavel@eazel.com>,
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <stdlib.h>

#include <config.h>
#include "nautilus-gdk-extensions.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>
#include "nautilus-glib-extensions.h"
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
			    rectangle->x, rectangle->y,
			    rectangle->width, rectangle->height);
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
				    guint32 rgb)
{
	GdkGCValues saved_values;
	
	/* FIXME bugzilla.eazel.com 1287: Workaround for a bug in gdk_rgb. */
	gdk_rgb_init ();
	
	gdk_gc_get_values (gc, &saved_values);
	gdk_rgb_gc_set_foreground (gc, rgb);
	nautilus_fill_rectangle (drawable, gc, rectangle);
	gdk_gc_set_foreground (gc, &saved_values.foreground);
}

/**
 * nautilus_rectangle_contains:
 * @rectangle: Rectangle possibly containing a point.
 * @x: X coordinate of a point.
 * @y: Y coordinate of a point.
 *
 * Retun TRUE if point is contained inside a rectangle
 */
gboolean
nautilus_rectangle_contains (const GdkRectangle *rectangle, 
			     int x, 
			     int y)
{
	g_return_val_if_fail (rectangle != NULL, FALSE);
	return rectangle->x <= x && rectangle->x + rectangle->width >= x
		&& rectangle->y <= y && rectangle->y + rectangle->height >= y;
}

/**
 * nautilus_rectangle_inset:
 * @rectangle: Rectangle we are insetting.
 * @x: Horizontal ammount to inset by.
 * @y: Vertical ammount to inset by.
 *
 * Inset a rectangle by a given horizontal and vertical ammount.
 * Pass in negative inset values to grow the rectangle, positive to
 * shrink it.
 */
void
nautilus_rectangle_inset (GdkRectangle *rectangle, 
			  int x, 
			  int y)
{
	g_return_if_fail (rectangle != NULL);

	rectangle->x += x;
	rectangle->width -= 2 * x;
	rectangle->y += y;
	rectangle->height -= 2 * y;
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
guint32
nautilus_interpolate_color (gdouble ratio,
			    guint32 start_rgb,
			    guint32 end_rgb)
{
	guchar red, green, blue;

	g_return_val_if_fail (ratio >= 0.0, 0);
	g_return_val_if_fail (ratio <= 1.0, 0);

	red = ((start_rgb >> 16) & 0xFF) * (1.0 - ratio) + ((end_rgb >> 16) & 0xFF) * ratio;
	green = ((start_rgb >> 8) & 0xFF) * (1.0 - ratio) + ((end_rgb >> 8) & 0xFF) * ratio;
	blue = (start_rgb & 0xFF) * (1.0 - ratio) + (end_rgb & 0xFF) * ratio;
	return (((red << 8) | green) << 8) | blue;
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
	if (nautilus_strcmp(start_color, end_color) == 0 || end_color == NULL || end_color[0] == '\0') {
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
nautilus_gradient_parse_one_color_spec (const char *spec, int *percent, const char **next_spec)
{
	char *result;
	const char *rgb_end_ptr;
	const char *percent_ptr;
	const char *separator_ptr;
	
	percent_ptr   = nautilus_strchr (spec, '%');
	separator_ptr = nautilus_strchr (spec, '-');

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
		result = nautilus_gradient_strip_trailing_direction_if_any (spec);
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
 * anyone using nautilus_gradient_get_start_color_spec or
 * nautilus_gradient_get_end_color_spec is assuming the gradient
 * is 2 colors which is questionable.
 * 
 * Callers should be rewritten and these fns eliminated.
 */
 
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
	return nautilus_gradient_parse_one_color_spec (gradient_spec, NULL, NULL);
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
	char* color = NULL;

	do {
		g_free (color);
		color = nautilus_gradient_parse_one_color_spec (gradient_spec, NULL, &gradient_spec);
	} while (gradient_spec != NULL);

	return color;
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
 * nautilus_parse_rgb_with_white_default
 * @color_spec: A color spec.
 * Returns: An rgb value.
 *
 * The same as gdk_color_parse, except sets the color to white if
 * the spec. can't be parsed instead of returning a boolean flag
 * and returns a guint32 rgb value instead of a GdkColor.
 */
guint32
nautilus_parse_rgb_with_white_default (const char *color_spec)
{
	GdkColor color;

	if (color_spec == NULL || !gdk_color_parse (color_spec, &color)) {
		return NAUTILUS_RGB_COLOR_WHITE;
	}
	return ((color.red << 8) & NAUTILUS_RGB_COLOR_RED)
		| (color.green & NAUTILUS_RGB_COLOR_GREEN)
		| ((color.blue >> 8) & NAUTILUS_RGB_COLOR_BLUE);
}

guint32
nautilus_rgb16_to_rgb (gushort r, gushort g, gushort b)
{
	guint32 result;

	result = (0xff0000 | (r & 0xff00));
	result <<= 8;
	result |= ((g & 0xff00) | (b >> 8));

	return result;
}

guint32
nautilus_rgb8_to_rgb (guchar r, guchar g, guchar b)
{
	return nautilus_rgb16_to_rgb (r << 8, g << 8, b << 8);
}

/**
 * nautilus_gdk_color_to_rgb
 * @color: A GdkColor style color.
 * Returns: An rgb value.
 *
 * Converts from a GdkColor stlye color to a gdk_rgb one.
 * Alpha gets set to fully opaque
 */
guint32
nautilus_gdk_color_to_rgb (const GdkColor *color)
{
	return nautilus_rgb16_to_rgb (color->red, color->green, color->blue);
}

/**
 * nautilus_gdk_rgb_to_color
 * @color: a gdk_rgb style value.
 *
 * Converts from a gdk_rgb value style to a GdkColor one.
 * The gdk_rgb color alpha channel is ignored.
 * 
 * Return value: a newly allocated GdkColor.
 */
GdkColor *
nautilus_gdk_rgb_to_color (const guint32 color)
{
	GdkColor *result;
	
	result = g_new0 (GdkColor, 1);
	 
	result->red = (color & 0xff0000) >> 16 ;
	result->green = (color & 0xff00);
	result->blue = color << 8;

	return result;
}

/**
 * nautilus_gdk_rgb_to_color_spec
 * @color: a gdk_rgb style value.
 *
 * Converts from a gdk_rgb value style to a string color spec.
 * The gdk_rgb color alpha channel is ignored.
 * 
 * Return value: a newly allocated color spec.
 */
char *
nautilus_gdk_rgb_to_color_spec (const guint32 color)
{
	return g_strdup_printf("rgb:%04hX/%04hX/%04hX",
			       NAUTILUS_RGBA_COLOR_GET_R (color) * 65535,
			       NAUTILUS_RGBA_COLOR_GET_G (color) * 65535,
			       NAUTILUS_RGBA_COLOR_GET_B (color) * 65535);
}

static guint32
nautilus_shift_color_component (guchar component, float shift_by)
{
	guint32 result;
	if (shift_by > 1.0) {
		result = component * (2 - shift_by);
	} else {
		result = 0xff - shift_by * (0xff - component);
	}

	return result & 0xff;
}

/**
 * nautilus_rgb_shift_color
 * @color: A color.
 * @shift_by: darken or lighten factor.
 * Returns: An darkened or lightened rgb value.
 *
 * Darkens (@shift_by > 1) or lightens (@shift_by < 1)
 * @color.
 */
guint32
nautilus_rgb_shift_color (guint32 color, float shift_by)
{
	guint32 result;

	/* shift red by shift_by */
	result = nautilus_shift_color_component((color & 0x00ff0000) >> 16, shift_by);
	result <<= 8;
	/* shift green by shift_by */
	result |=  nautilus_shift_color_component((color & 0x0000ff00) >> 8, shift_by);
	result <<= 8;
	/* shift blue by shift_by */
	result |=  nautilus_shift_color_component((color & 0x000000ff), shift_by);

	/* alpha doesn't change */
	result |= (0xff000000 & color);

	return result;
}

/**
 * nautilus_gdk_color_is_dark:
 * 
 * Return true if the given color is `dark'
 */
gboolean
nautilus_gdk_color_is_dark (GdkColor *color)
{
	int intensity;

	intensity = (((color->red >> 8) * 77)
		     + ((color->green >> 8) * 150)
		     + ((color->blue >> 8) * 28)) >> 8;

	return intensity < 128;
}

/**
 * nautilus_gdk_choose_foreground_color:
 *
 * Select a foreground color given that BACKGROUND is the background
 * color. If the PREFERRED color has a high enough contrast with
 * BACKGROUND, use it, else use one of black or white, depending on
 * the darkness of BACKGROUND.
 *
 * The selected color is stored in PREFERRED.
 */
void
nautilus_gdk_choose_foreground_color (GdkColor *preferred,
				      GdkColor *background)
{
	gboolean preferred_is_dark, background_is_dark;

	preferred_is_dark = nautilus_gdk_color_is_dark (preferred);
	background_is_dark = nautilus_gdk_color_is_dark (background);

	if (preferred_is_dark == background_is_dark) {
		/* Colors are too similar, so choose a different fg.
		 * Currently hardcoded to use either white or black.
		 */
		if (preferred_is_dark) {
			preferred->red = 65535;
			preferred->green = 65535;
			preferred->blue = 65535;
		} else {
			preferred->red = 0;
			preferred->green = 0;
			preferred->blue = 0;
		}
	}
}

/**
 * nautilus_gdk_gc_choose_foreground_color:
 *
 * Use nautilus_gdk_color_choose_foreground_color () to set the
 * foreground color of GC to something suitable, given that BACKGROUND
 * will be the background color and PREFERRED is the preferred color.
 *
 * Uses GdkRGB to install the color value.
 */
void
nautilus_gdk_gc_choose_foreground_color (GdkGC *gc,
					 GdkColor *preferred,
					 GdkColor *background)
{
	GdkColor temp;
	guint32 rgb;

	temp = *preferred;
	nautilus_gdk_choose_foreground_color (&temp, background);
	rgb = nautilus_gdk_color_to_rgb (&temp);

	gdk_rgb_init ();
	gdk_rgb_gc_set_foreground (gc, rgb);
}

/**
 * nautilus_stipple_bitmap:
 * 
 * Get pointer to singleton 50% stippled bitmap.
 * This is a global object; do not free.
 */
GdkBitmap *
nautilus_stipple_bitmap ()
{
	static GdkBitmap *stipple = NULL;

	if (stipple == NULL) {
		char stipple_bits[] = { 0x02, 0x01 };
		stipple = gdk_bitmap_create_from_data (NULL, stipple_bits, 2, 2);	
	}

	return stipple;
}

/**
 * nautilus_gdk_window_bring_to_front:
 * 
 * Raise window and give it focus.
 */
void 
nautilus_gdk_window_bring_to_front (GdkWindow *window)
{
	/* This takes care of un-iconifying the window and
	 * raising it if needed.
	 */
	gdk_window_show (window);

	/* If the window was already showing, it would not have
	 * the focus at this point. Do a little X trickery to
	 * ensure it is focused.
	 */
	nautilus_gdk_window_focus (window, GDK_CURRENT_TIME);
}

void
nautilus_gdk_window_focus (GdkWindow *window, guint32 timestamp)
{
	gdk_error_trap_push ();
	XSetInputFocus (GDK_DISPLAY (),
			GDK_WINDOW_XWINDOW (window),
			RevertToNone,
			timestamp);
	gdk_flush();
	gdk_error_trap_pop ();
}

void
nautilus_gdk_window_set_wm_protocols (GdkWindow *window,
				      GdkAtom *protocols,
				      int nprotocols)
{
	/* This relies on the identity mapping from GdkAtoms to X Atoms */
	XSetWMProtocols (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XWINDOW (window),
			 protocols, nprotocols);
}

void
nautilus_set_mini_icon (GdkWindow *window,
			GdkPixmap *pixmap,
			GdkBitmap *mask)
{
        GdkAtom icon_atom;
        long data[2];
 
        g_return_if_fail (window != NULL);
        g_return_if_fail (pixmap != NULL);

        data[0] = GDK_WINDOW_XWINDOW (pixmap);
        if (mask) {
                data[1] = GDK_WINDOW_XWINDOW (mask);
        } else {
                data[1] = 0;
	}

        icon_atom = gdk_atom_intern ("KWM_WIN_ICON", FALSE);
        gdk_property_change (window, icon_atom, icon_atom, 
                             32, GDK_PROP_MODE_REPLACE,
                             (guchar *) data, 2);
}

/**
 * nautilus_gdk_window_set_wm_hints_input:
 * 
 * Set the WM_HINTS.input flag to the passed in value
 */
void
nautilus_gdk_window_set_wm_hints_input (GdkWindow *window, gboolean status)
{
	Display *dpy;
	Window id;
	XWMHints *wm_hints;

	g_return_if_fail (window != NULL);

	dpy = GDK_WINDOW_XDISPLAY (window);
	id = GDK_WINDOW_XWINDOW (window);

	wm_hints = XGetWMHints (dpy, id);
	if (wm_hints == 0) {
		wm_hints = XAllocWMHints ();
	}

	wm_hints->flags |= InputHint;
	wm_hints->input = status;

	XSetWMHints (dpy, id, wm_hints);
	XFree (wm_hints);
}

void
nautilus_gdk_window_set_invisible_cursor (GdkWindow *window)
{
	XColor foreColor, backColor;
	GdkWindowPrivate *window_private;
	Pixmap sourcePixmap, maskPixmap;
	Cursor xcursor;

	char invisible_cursor_bits[]      = {0x0};
	char invisible_cursor_mask_bits[] = {0x0};

	foreColor.pixel = 0L;
	foreColor.red = foreColor.green = foreColor.blue = 0;
	foreColor.flags = DoRed | DoGreen | DoBlue;

	backColor.pixel = 255L;
	backColor.red = backColor.green = backColor.blue = 65535;
	backColor.flags = DoRed | DoGreen | DoBlue;

	window_private = (GdkWindowPrivate *) window;
  
	sourcePixmap = XCreateBitmapFromData (window_private->xdisplay, window_private->xwindow,
					            		  invisible_cursor_bits, 1, 1);
	g_assert (sourcePixmap != 0);

	maskPixmap = XCreateBitmapFromData (window_private->xdisplay, window_private->xwindow,
					          			invisible_cursor_mask_bits, 1, 1);
	g_assert (maskPixmap != 0);

	xcursor = XCreatePixmapCursor (window_private->xdisplay, sourcePixmap, maskPixmap,
					      		   &foreColor, &backColor, 0, 0);

	XFreePixmap (window_private->xdisplay, sourcePixmap);
	XFreePixmap (window_private->xdisplay, maskPixmap);

	XDefineCursor (window_private->xdisplay, window_private->xwindow, xcursor);
}

NautilusGdkGeometryFlags
nautilus_gdk_parse_geometry (const char *string, int *x_return, int *y_return,
			     guint *width_return, guint *height_return)
{
	int x11_flags;
	NautilusGdkGeometryFlags gdk_flags;

	g_return_val_if_fail (string != NULL, NAUTILUS_GDK_NO_VALUE);
	g_return_val_if_fail (x_return != NULL, NAUTILUS_GDK_NO_VALUE);
	g_return_val_if_fail (y_return != NULL, NAUTILUS_GDK_NO_VALUE);
	g_return_val_if_fail (width_return != NULL, NAUTILUS_GDK_NO_VALUE);
	g_return_val_if_fail (height_return != NULL, NAUTILUS_GDK_NO_VALUE);

	x11_flags = XParseGeometry (string, x_return, y_return,
				    width_return, height_return);

	gdk_flags = NAUTILUS_GDK_NO_VALUE;
	if (x11_flags & XValue) {
		gdk_flags |= NAUTILUS_GDK_X_VALUE;
	}
	if (x11_flags & YValue) {
		gdk_flags |= NAUTILUS_GDK_Y_VALUE;
	}
	if (x11_flags & WidthValue) {
		gdk_flags |= NAUTILUS_GDK_WIDTH_VALUE;
	}
	if (x11_flags & HeightValue) {
		gdk_flags |= NAUTILUS_GDK_HEIGHT_VALUE;
	}
	if (x11_flags & XNegative) {
		gdk_flags |= NAUTILUS_GDK_X_NEGATIVE;
	}
	if (x11_flags & YNegative) {
		gdk_flags |= NAUTILUS_GDK_Y_NEGATIVE;
	}

	return gdk_flags;
}

#if ! defined (NAUTILUS_OMIT_SELF_CHECK)

static char *
nautilus_gdk_color_as_hex_string (GdkColor color)
{
	return g_strdup_printf("rgb:%04hX/%04hX/%04hX",
			       color.red, color.green, color.blue);
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
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_interpolate_color (0.0, 0, 0), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_interpolate_color (0.0, 0, 0xFFFFFF), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_interpolate_color (0.5, 0, 0xFFFFFF), 0x7F7F7F);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_interpolate_color (1.0, 0, 0xFFFFFF), 0xFFFFFF);

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
