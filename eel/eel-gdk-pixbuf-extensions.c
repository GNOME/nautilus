/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gdk-pixbuf-extensions.c: Routines to augment what's in gdk-pixbuf.

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
#include "eel-gdk-pixbuf-extensions.h"

#include "eel-art-gtk-extensions.h"
#include "eel-debug-drawing.h"
#include "eel-debug.h"
#include "eel-gdk-extensions.h"
#include "eel-glib-extensions.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define LOAD_BUFFER_SIZE 65536

const EelIRect eel_gdk_pixbuf_whole_pixbuf = { G_MININT, G_MININT, G_MAXINT, G_MAXINT };

struct EelPixbufLoadHandle {
	GCancellable *cancellable;
	GInputStream *stream;
	EelPixbufLoadCallback callback;
	gpointer callback_data;
	GdkPixbufLoader *loader;
	char buffer[LOAD_BUFFER_SIZE];
};

/**
 * eel_gdk_pixbuf_list_ref
 * @pixbuf_list: A list of GdkPixbuf objects.
 *
 * Refs all the pixbufs.
 **/
void
eel_gdk_pixbuf_list_ref (GList *pixbuf_list)
{
	g_list_foreach (pixbuf_list, (GFunc) g_object_ref, NULL);
}

/**
 * eel_gdk_pixbuf_list_free
 * @pixbuf_list: A list of GdkPixbuf objects.
 *
 * Unrefs all the pixbufs, then frees the list.
 **/
void
eel_gdk_pixbuf_list_free (GList *pixbuf_list)
{
	eel_g_list_free_deep_custom (pixbuf_list, (GFunc) g_object_unref, NULL);
}

GdkPixbuf *
eel_gdk_pixbuf_load (const char *uri)
{
	GdkPixbuf *pixbuf;
	GFile *file;
	GFileInputStream *stream;

	g_return_val_if_fail (uri != NULL, NULL);

	file = g_file_new_for_uri (uri);

	stream = g_file_read (file, NULL, NULL);
	
	g_object_unref (file);
	
	if (stream == NULL) {
		return NULL;
	}

	pixbuf = eel_gdk_pixbuf_load_from_stream (G_INPUT_STREAM (stream));

	g_object_unref (stream);
		
	return pixbuf;
}

GdkPixbuf *
eel_gdk_pixbuf_load_from_stream (GInputStream  *stream)
{
	return eel_gdk_pixbuf_load_from_stream_at_size (stream, -1);
}

static void
pixbuf_loader_size_prepared (GdkPixbufLoader *loader,
			     int              width,
			     int              height,
			     gpointer         desired_size_ptr)
{
	int size, desired_size;
	float scale;

	size = MAX (width, height);
	desired_size = GPOINTER_TO_INT (desired_size_ptr);

	if (size != desired_size) {
		scale = (float) desired_size / size;
		gdk_pixbuf_loader_set_size (loader,
					    floor (scale * width + 0.5),
					    floor (scale * height + 0.5));
	}
}

GdkPixbuf *
eel_gdk_pixbuf_load_from_stream_at_size (GInputStream  *stream,
					 int            size)
{
	char buffer[LOAD_BUFFER_SIZE];
	gssize bytes_read;
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	gboolean got_eos;
	

	g_return_val_if_fail (stream != NULL, NULL);

	got_eos = FALSE;
	loader = gdk_pixbuf_loader_new ();

	if (size > 0) {
		g_signal_connect (loader, "size-prepared",
				  G_CALLBACK (pixbuf_loader_size_prepared),
				  GINT_TO_POINTER (size));
	}

	while (1) {
		bytes_read = g_input_stream_read (stream, buffer, sizeof (buffer),
						  NULL, NULL);
		
		if (bytes_read < 0) {
			break;
		}
		if (bytes_read == 0) {
			got_eos = TRUE;
			break;
		}
		if (!gdk_pixbuf_loader_write (loader,
					      buffer,
					      bytes_read,
					      NULL)) {
			break;
		}
	}

	g_input_stream_close (stream, NULL, NULL);
	gdk_pixbuf_loader_close (loader, NULL);

	pixbuf = NULL;
	if (got_eos) {
		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		if (pixbuf != NULL) {
			g_object_ref (pixbuf);
		}
	}

	g_object_unref (loader);

	return pixbuf;
}

static void
free_pixbuf_load_handle (EelPixbufLoadHandle *handle)
{
	g_object_unref (handle->cancellable);
	if (handle->loader != NULL) {
		g_object_unref (handle->loader);
	}
	if (handle->stream) {
		g_input_stream_close_async (handle->stream, 0, NULL, NULL, NULL);
		g_object_unref (handle->stream);
	}
	g_free (handle);
}

static void
load_done (EelPixbufLoadHandle *handle, GError *error, gboolean get_pixbuf)
{
	GdkPixbuf *pixbuf;

	if (handle->loader != NULL) {
		gdk_pixbuf_loader_close (handle->loader, NULL);
	}

	pixbuf = get_pixbuf ? gdk_pixbuf_loader_get_pixbuf (handle->loader) : NULL;
	
	handle->callback (error, pixbuf, handle->callback_data);

	free_pixbuf_load_handle (handle);
}

static void
file_read_callback (GObject *source_object,
		    GAsyncResult *res,
		    gpointer user_data)
{
	EelPixbufLoadHandle *handle;
	gssize bytes_read;
	GError *error;

	handle = user_data;

	if (g_cancellable_is_cancelled (handle->cancellable)) {
		free_pixbuf_load_handle (handle);
		return;
	}
	
	error = NULL;
	bytes_read = g_input_stream_read_finish  (G_INPUT_STREAM (source_object),
						  res, &error);
	
	if (bytes_read > 0) {
		if (!gdk_pixbuf_loader_write (handle->loader,
					      handle->buffer,
					      bytes_read,
					      &error)) {
			bytes_read = -1;
		} else {
			g_input_stream_read_async (handle->stream,
						   handle->buffer,
						   sizeof (handle->buffer),
						   0,
						   handle->cancellable,
						   file_read_callback, handle);
			return;
		}
	}

	load_done (handle, error, bytes_read == 0);
	
	if (error != NULL) {
		g_error_free (error);
	}
}

static void
file_opened_callback (GObject *source_object,
		      GAsyncResult *res,
		      gpointer user_data)
{
	EelPixbufLoadHandle *handle;
	GFileInputStream *stream;
	GError *error;

	handle = user_data;

	if (g_cancellable_is_cancelled (handle->cancellable)) {
		free_pixbuf_load_handle (handle);
		return;
	}

	error = NULL;
	stream = g_file_read_finish (G_FILE (source_object), res, &error);

	if (stream == NULL) {
		load_done (handle, error, FALSE);
		g_error_free (error);
		return;
	}

	handle->stream = G_INPUT_STREAM (stream);
	handle->loader = gdk_pixbuf_loader_new ();

	
	g_input_stream_read_async (handle->stream,
				   handle->buffer,
				   sizeof (handle->buffer),
				   0,
				   handle->cancellable,
				   file_read_callback, handle);
}

EelPixbufLoadHandle *
eel_gdk_pixbuf_load_async (const char *uri,
			   int priority,
			   EelPixbufLoadCallback callback,
			   gpointer callback_data)
{
	EelPixbufLoadHandle *handle;
	GFile *file;

	handle = g_new0 (EelPixbufLoadHandle, 1);
	handle->cancellable = g_cancellable_new ();
	handle->callback = callback;
	handle->callback_data = callback_data;

	file = g_file_new_for_uri (uri);
	
	g_file_read_async (file, priority, handle->cancellable,
			   file_opened_callback, handle);

	return handle;
}

void
eel_cancel_gdk_pixbuf_load (EelPixbufLoadHandle *handle)
{
	if (handle == NULL) {
		return;
	}
	
	g_cancellable_cancel (handle->cancellable);
}

/* return the average value of each component */
guint32
eel_gdk_pixbuf_average_value (GdkPixbuf *pixbuf)
{
	guint64 a_total, r_total, g_total, b_total;
	guint row, column;
	int row_stride;
	const guchar *pixels, *p;
	int r, g, b, a;
	guint64 dividend;
	guint width, height;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	/* iterate through the pixbuf, counting up each component */
	a_total = 0;
	r_total = 0;
	g_total = 0;
	b_total = 0;

	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		for (row = 0; row < height; row++) {
			p = pixels + (row * row_stride);
			for (column = 0; column < width; column++) {
				r = *p++;
				g = *p++;
				b = *p++;
				a = *p++;
				
				a_total += a;
				r_total += r * a;
				g_total += g * a;
				b_total += b * a;
			}
		}
		dividend = height * width * 0xFF;
		a_total *= 0xFF;
	} else {
		for (row = 0; row < height; row++) {
			p = pixels + (row * row_stride);
			for (column = 0; column < width; column++) {
				r = *p++;
				g = *p++;
				b = *p++;
				
				r_total += r;
				g_total += g;
				b_total += b;
			}
		}
		dividend = height * width;
		a_total = dividend * 0xFF;
	}

	return ((a_total + dividend / 2) / dividend) << 24
		| ((r_total + dividend / 2) / dividend) << 16
		| ((g_total + dividend / 2) / dividend) << 8
		| ((b_total + dividend / 2) / dividend);
}

double
eel_gdk_scale_to_fit_factor (int width, int height,
			     int max_width, int max_height,
			     int *scaled_width, int *scaled_height)
{
	double scale_factor;
	
	scale_factor = MIN (max_width  / (double) width, max_height / (double) height);

	*scaled_width  = floor (width * scale_factor + .5);
	*scaled_height = floor (height * scale_factor + .5);

	return scale_factor;
}

/* Returns a scaled copy of pixbuf, preserving aspect ratio. The copy will
 * be scaled as large as possible without exceeding the specified width and height.
 */
GdkPixbuf *
eel_gdk_pixbuf_scale_to_fit (GdkPixbuf *pixbuf, int max_width, int max_height)
{
	int scaled_width;
	int scaled_height;

	eel_gdk_scale_to_fit_factor (gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf),
				     max_width, max_height,
				     &scaled_width, &scaled_height);

	return gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_BILINEAR);	
}

/* Returns a copy of pixbuf scaled down, preserving aspect ratio, to fit
 * within the specified width and height. If it already fits, a copy of
 * the original, without scaling, is returned.
 */
GdkPixbuf *
eel_gdk_pixbuf_scale_down_to_fit (GdkPixbuf *pixbuf, int max_width, int max_height)
{
	int scaled_width;
	int scaled_height;
	
	double scale_factor;

	scale_factor = eel_gdk_scale_to_fit_factor (gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf),
						    max_width, max_height,
						    &scaled_width, &scaled_height);

	if (scale_factor >= 1.0) {
		return gdk_pixbuf_copy (pixbuf);
	} else {				
		return eel_gdk_pixbuf_scale_down (pixbuf, scaled_width, scaled_height);	
	}
}

double
eel_gdk_scale_to_min_factor (int width, int height,
			     int min_width, int min_height,
			     int *scaled_width, int *scaled_height)
{
	double scale_factor;

	scale_factor = MAX (min_width / (double) width, min_height / (double) height);

	*scaled_width  = floor (width * scale_factor + .5);
	*scaled_height = floor (height * scale_factor + .5);

	return scale_factor;
}

/* Returns a scaled copy of pixbuf, preserving aspect ratio. The copy will
 * be scaled as small as possible without going under the specified width and height.
 */
GdkPixbuf *
eel_gdk_pixbuf_scale_to_min (GdkPixbuf *pixbuf, int min_width, int min_height)
{
	int scaled_width;
	int scaled_height;

	eel_gdk_scale_to_min_factor (gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf),
				     min_width, min_height,
				     &scaled_width, &scaled_height);

	return gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_BILINEAR);
}

/**
 * eel_gdk_pixbuf_is_valid:
 * @pixbuf: A GdkPixbuf
 *
 * Return value: A boolean indicating whether the given pixbuf is valid.
 *
 * A pixbuf is valid if:
 * 
 *   1. It is non NULL
 *   2. It is has non NULL pixel data.
 *   3. It has width and height greater than 0.
 */
gboolean
eel_gdk_pixbuf_is_valid (const GdkPixbuf *pixbuf)
{
	return ((pixbuf != NULL)
		&& (gdk_pixbuf_get_pixels (pixbuf) != NULL)
		&& (gdk_pixbuf_get_width (pixbuf) > 0)
		&& (gdk_pixbuf_get_height (pixbuf) > 0));
}

/**
 * eel_gdk_pixbuf_get_dimensions:
 * @pixbuf: A GdkPixbuf
 *
 * Return value: The dimensions of the pixbuf as a EelDimensions.
 *
 * This function is useful in code that uses libart rect 
 * intersection routines.
 */
EelDimensions
eel_gdk_pixbuf_get_dimensions (const GdkPixbuf *pixbuf)
{
	EelDimensions dimensions;

	g_return_val_if_fail (eel_gdk_pixbuf_is_valid (pixbuf), eel_dimensions_empty);

	dimensions.width = gdk_pixbuf_get_width (pixbuf);
	dimensions.height = gdk_pixbuf_get_height (pixbuf);

	return dimensions;
}

/**
 * eel_gdk_pixbuf_fill_rectangle_with_color:
 * @pixbuf: Target pixbuf to fill into.
 * @area: Rectangle to fill.
 * @color: The color to use.
 *
 * Fill the rectangle with the the given color.
 */
void
eel_gdk_pixbuf_fill_rectangle_with_color (GdkPixbuf *pixbuf,
					  EelIRect area,
					  guint32 color)
{
	EelIRect target;
	guchar red;
	guchar green;
	guchar blue;
	guchar alpha;
	guchar *pixels;
	gboolean has_alpha;
	guint pixel_offset;
	guint rowstride;
	guchar *row_offset;
	int x;
	int y;

	g_return_if_fail (eel_gdk_pixbuf_is_valid (pixbuf));
	
	target = eel_gdk_pixbuf_intersect (pixbuf, 0, 0, area);
	if (eel_irect_is_empty (&target)) {
		return;
	}

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	pixel_offset = has_alpha ? 4 : 3;
	red = EEL_RGBA_COLOR_GET_R (color);
	green = EEL_RGBA_COLOR_GET_G (color);
	blue = EEL_RGBA_COLOR_GET_B (color);
	alpha = EEL_RGBA_COLOR_GET_A (color);

	row_offset = pixels + target.y0 * rowstride;

	for (y = target.y0; y < target.y1; y++) {
		guchar *offset = row_offset + (target.x0 * pixel_offset);
		
		for (x = target.x0; x < target.x1; x++) {
			*(offset++) = red;
			*(offset++) = green;
			*(offset++) = blue;
			
			if (has_alpha) {
				*(offset++) = alpha;
			}
			
		}

		row_offset += rowstride;
	}
}

gboolean
eel_gdk_pixbuf_save_to_file (const GdkPixbuf *pixbuf,
			     const char *file_name)
{
	return gdk_pixbuf_save ((GdkPixbuf *) pixbuf,
				file_name, "png", NULL, NULL);
}

void
eel_gdk_pixbuf_ref_if_not_null (GdkPixbuf *pixbuf_or_null)
{
	if (pixbuf_or_null != NULL) {
		g_object_ref (pixbuf_or_null);
	}
}

void
eel_gdk_pixbuf_unref_if_not_null (GdkPixbuf *pixbuf_or_null)
{
	if (pixbuf_or_null != NULL) {
		g_object_unref (pixbuf_or_null);
	}
}

void
eel_gdk_pixbuf_draw_to_drawable (const GdkPixbuf *pixbuf,
				 GdkDrawable *drawable,
				 GdkGC *gc,
				 int source_x,
				 int source_y,
				 EelIRect destination_area,
				 GdkRgbDither dither,
				 GdkPixbufAlphaMode alpha_compositing_mode,
				 int alpha_threshold)
{
	EelDimensions dimensions;
	EelIRect target;
	EelIRect source;
	int target_width;
	int target_height;
	int source_width;
	int source_height;

	g_return_if_fail (eel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (drawable != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (!eel_irect_is_empty (&destination_area));
 	g_return_if_fail (alpha_threshold > EEL_OPACITY_FULLY_TRANSPARENT);
 	g_return_if_fail (alpha_threshold <= EEL_OPACITY_FULLY_OPAQUE);
 	g_return_if_fail (alpha_compositing_mode >= GDK_PIXBUF_ALPHA_BILEVEL);
 	g_return_if_fail (alpha_compositing_mode <= GDK_PIXBUF_ALPHA_FULL);

	dimensions = eel_gdk_pixbuf_get_dimensions (pixbuf);
	
	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < dimensions.width);
	g_return_if_fail (source_y < dimensions.height);

	/* Clip the destination area to the pixbuf dimensions; bail if no work */
	target = eel_gdk_pixbuf_intersect (pixbuf,
					   destination_area.x0,
					   destination_area.y0,
					   destination_area);
	if (eel_irect_is_empty (&target)) {
		return;
	}

	/* Assign the source area */
	source = eel_irect_assign (source_x,
				       source_y,
				       dimensions.width - source_x,
				       dimensions.height - source_y);

	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf dimensions */
	target_width = target.x1 - target.x0;
	target_height = target.y1 - target.y0;
	source_width = source.x1 - source.x0;
	source_height = source.y1 - source.y0;

	target.x1 = target.x0 + MIN (target_width, source_width);
	target.y1 = target.y0 + MIN (target_height, source_height);

	gdk_draw_pixbuf (drawable, gc, (GdkPixbuf *) pixbuf,
			 source.x0,
			 source.y0,
			 target.x0,
			 target.y0,
			 target.x1 - target.x0,
			 target.y1 - target.y0,
			 dither,
			 0,
			 0);
}

/**
 * eel_gdk_pixbuf_draw_to_pixbuf:
 * @pixbuf: The source pixbuf to draw.
 * @destination_pixbuf: The destination pixbuf.
 * @source_x: The source pixbuf x coordiate to composite from.
 * @source_y: The source pixbuf y coordiate to composite from.
 * @destination_area: The destination area within the destination pixbuf.
 *                    This area will be clipped if invalid in any way.
 *
 * Copy one pixbuf onto another another..  This function has some advantages
 * over plain gdk_pixbuf_copy_area():
 *
 *   Composition paramters (source coordinate, destination area) are
 *   given in a way that is consistent with the rest of the extensions
 *   in this file.  That is, it matches the declaration of
 *   eel_gdk_pixbuf_draw_to_pixbuf_alpha() and 
 *   eel_gdk_pixbuf_draw_to_drawable() very closely.
 *
 *   All values are clipped to make sure they are valid.
 *
 */
void
eel_gdk_pixbuf_draw_to_pixbuf (const GdkPixbuf *pixbuf,
			       GdkPixbuf *destination_pixbuf,
			       int source_x,
			       int source_y,
			       EelIRect destination_area)
{
	EelDimensions dimensions;
	EelIRect target;
	EelIRect source;
	int target_width;
	int target_height;
	int source_width;
	int source_height;
	
	g_return_if_fail (eel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (eel_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (!eel_irect_is_empty (&destination_area));

	dimensions = eel_gdk_pixbuf_get_dimensions (pixbuf);

	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < dimensions.width);
	g_return_if_fail (source_y < dimensions.height);

	/* Clip the destination area to the pixbuf dimensions; bail if no work */
	target = eel_gdk_pixbuf_intersect (destination_pixbuf, 0, 0, destination_area);
	if (eel_irect_is_empty (&target)) {
 		return;
 	}

	/* Assign the source area */
	source = eel_irect_assign (source_x,
				       source_y,
				       dimensions.width - source_x,
				       dimensions.height - source_y);

	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf dimensions */
	target_width = target.x1 - target.x0;
	target_height = target.y1 - target.y0;
	source_width = source.x1 - source.x0;
	source_height = source.y1 - source.y0;

	target.x1 = target.x0 + MIN (target_width, source_width);
	target.y1 = target.y0 + MIN (target_height, source_height);

	gdk_pixbuf_copy_area (pixbuf,
			      source.x0,
			      source.y0,
			      target.x1 - target.x0,
			      target.y1 - target.y0,
			      destination_pixbuf,
			      target.x0,
			      target.y0);
}

/**
 * eel_gdk_pixbuf_draw_to_pixbuf_alpha:
 * @pixbuf: The source pixbuf to draw.
 * @destination_pixbuf: The destination pixbuf.
 * @source_x: The source pixbuf x coordiate to composite from.
 * @source_y: The source pixbuf y coordiate to composite from.
 * @destination_area: The destination area within the destination pixbuf.
 *                    This area will be clipped if invalid in any way.
 * @opacity: The opacity of the drawn tiles where 0 <= opacity <= 255.
 * @interpolation_mode: The interpolation mode.  See <gdk-pixbuf.h>
 *
 * Composite one pixbuf over another.  This function has some advantages
 * over plain gdk_pixbuf_composite():
 *
 *   Composition paramters (source coordinate, destination area) are
 *   given in a way that is consistent with the rest of the extensions
 *   in this file.  That is, it matches the declaration of
 *   eel_gdk_pixbuf_draw_to_pixbuf() and 
 *   eel_gdk_pixbuf_draw_to_drawable() very closely.
 *
 *   All values are clipped to make sure they are valid.
 *
 *   Workaround a limitation in gdk_pixbuf_composite() that does not allow
 *   the source (x,y) to be greater than (0,0)
 * 
 */
void
eel_gdk_pixbuf_draw_to_pixbuf_alpha (const GdkPixbuf *pixbuf,
				     GdkPixbuf *destination_pixbuf,
				     int source_x,
				     int source_y,
				     EelIRect destination_area,
				     int opacity,
				     GdkInterpType interpolation_mode)
{
	EelDimensions dimensions;
	EelIRect target;
	EelIRect source;
	int target_width;
	int target_height;
	int source_width;
	int source_height;

	g_return_if_fail (eel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (eel_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (!eel_irect_is_empty (&destination_area));
	g_return_if_fail (opacity >= EEL_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity <= EEL_OPACITY_FULLY_OPAQUE);
	g_return_if_fail (interpolation_mode >= GDK_INTERP_NEAREST);
	g_return_if_fail (interpolation_mode <= GDK_INTERP_HYPER);
	
	dimensions = eel_gdk_pixbuf_get_dimensions (pixbuf);

	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < dimensions.width);
	g_return_if_fail (source_y < dimensions.height);

	/* Clip the destination area to the pixbuf dimensions; bail if no work */
	target = eel_gdk_pixbuf_intersect (destination_pixbuf, 0, 0, destination_area);
	if (eel_irect_is_empty (&target)) {
 		return;
 	}

	/* Assign the source area */
	source = eel_irect_assign (source_x,
				       source_y,
				       dimensions.width - source_x,
				       dimensions.height - source_y);
	
	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf dimensions */
	target_width = target.x1 - target.x0;
	target_height = target.y1 - target.y0;
	source_width = source.x1 - source.x0;
	source_height = source.y1 - source.y0;

	target.x1 = target.x0 + MIN (target_width, source_width);
	target.y1 = target.y0 + MIN (target_height, source_height);
	
	/* If the source point is not (0,0), then we need to create a sub pixbuf
	 * with only the source area.  This is needed to work around a limitation
	 * in gdk_pixbuf_composite() that requires the source area to be (0,0). */
	if (source.x0 != 0 || source.y0 != 0) {
		EelIRect area;
		int width;
		int height;

		width = dimensions.width - source.x0;
		height = dimensions.height - source.y0;
		
		area.x0 = source.x0;
		area.y0 = source.y0;
		area.x1 = area.x0 + width;
		area.y1 = area.y0 + height;
		
		pixbuf = eel_gdk_pixbuf_new_from_pixbuf_sub_area ((GdkPixbuf *) pixbuf, area);
	} else {
		g_object_ref (G_OBJECT (pixbuf));
	}
	
	gdk_pixbuf_composite (pixbuf,
			      destination_pixbuf,
			      target.x0,
			      target.y0,
			      target.x1 - target.x0,
			      target.y1 - target.y0,
			      target.x0,
			      target.y0,
			      1.0,
			      1.0,
			      interpolation_mode,
			      opacity);

	g_object_unref (G_OBJECT (pixbuf));
}

static void
pixbuf_destroy_callback (guchar  *pixels,
			 gpointer callback_data)
{
	g_assert (pixels != NULL);
	g_assert (callback_data != NULL);

	g_object_unref (callback_data);
}

/**
 * eel_gdk_pixbuf_new_from_pixbuf_sub_area:
 * @pixbuf: The source pixbuf.
 * @area: The area within the source pixbuf to use for the sub pixbuf.
 *        This area needs to be contained within the bounds of the 
 *        source pixbuf, otherwise it will be clipped to that. 
 *
 * Return value: A newly allocated pixbuf that shares the pixel data
 *               of the source pixbuf in order to represent a sub area.
 *
 * Create a pixbuf from a sub area of another pixbuf.  The resulting pixbuf
 * will share the pixel data of the source pixbuf.  Memory bookeeping is
 * all taken care for the caller.  All you need to do is g_object_unref()
 * the resulting pixbuf to properly free resources.
 */
GdkPixbuf *
eel_gdk_pixbuf_new_from_pixbuf_sub_area (GdkPixbuf *pixbuf,
					 EelIRect area)
{
	GdkPixbuf *sub_pixbuf;
	EelIRect target;
	guchar *pixels;
	
	g_return_val_if_fail (eel_gdk_pixbuf_is_valid (pixbuf), NULL);
	g_return_val_if_fail (!eel_irect_is_empty (&area), NULL);

	/* Clip the pixbuf by the given area; bail if no work */
	target = eel_gdk_pixbuf_intersect (pixbuf, 0, 0, area);
	if (eel_irect_is_empty (&target)) {
 		return NULL;
 	}

	/* Since we are going to be sharing the given pixbuf's data, we need 
	 * to ref it.  It will be unreffed in the destroy function above */
	g_object_ref (pixbuf);

	/* Compute the offset into the pixel data */
	pixels = 
		gdk_pixbuf_get_pixels (pixbuf)
		+ (target.y0 * gdk_pixbuf_get_rowstride (pixbuf))
		+ (target.x0 * (gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3));
	
	/* Make a pixbuf pretending its real estate is the sub area */
	sub_pixbuf = gdk_pixbuf_new_from_data (pixels,
					       GDK_COLORSPACE_RGB,
					       gdk_pixbuf_get_has_alpha (pixbuf),
					       8,
					       eel_irect_get_width (target),
					       eel_irect_get_height (target),
					       gdk_pixbuf_get_rowstride (pixbuf),
					       pixbuf_destroy_callback,
					       pixbuf);

	return sub_pixbuf;
}

/**
 * eel_gdk_pixbuf_new_from_existing_buffer:
 * @buffer: The existing buffer.
 * @buffer_rowstride: The existing buffer's rowstride.
 * @buffer_has_alpha: A boolean value indicating whether the buffer has alpha.
 * @area: The area within the existing buffer to use for the pixbuf.
 *        This area needs to be contained within the bounds of the 
 *        buffer, otherwise memory will be trashed.
 *
 * Return value: A newly allocated pixbuf that uses the existing buffer
 *               for its pixel data.
 *
 * Create a pixbuf from an existing buffer.
 *
 * The resulting pixbuf is only valid for as long as &buffer is valid.  It is
 * up to the caller to make sure they both exist in the same scope.
 * Also, it is up to the caller to make sure that the given area is fully 
 * contained in the buffer, otherwise memory trashing will happen.
 */
GdkPixbuf *
eel_gdk_pixbuf_new_from_existing_buffer (guchar *buffer,
					 int buffer_rowstride,
					 gboolean buffer_has_alpha,
					 EelIRect area)
{
	GdkPixbuf *pixbuf;
	guchar *pixels;
	
	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (buffer_rowstride > 0, NULL);
	g_return_val_if_fail (!eel_irect_is_empty (&area), NULL);
	
	/* Compute the offset into the buffer */
	pixels = 
		buffer
		+ (area.y0 * buffer_rowstride)
		+ (area.x0 * (buffer_has_alpha ? 4 : 3));
	
	pixbuf = gdk_pixbuf_new_from_data (pixels,
					   GDK_COLORSPACE_RGB,
					   buffer_has_alpha,
					   8,
					   eel_irect_get_width (area),
					   eel_irect_get_height (area),
					   buffer_rowstride,
					   NULL,
					   NULL);

	return pixbuf;
}

/**
 * eel_gdk_pixbuf_intersect:
 * @pixbuf: A GdkPixbuf.
 * @pixbuf_x: X coordinate of pixbuf.
 * @pixbuf_y: Y coordinate of pixbuf.
 * @rectangle: An EelIRect.
 *
 * Return value: The intersection of the pixbuf and the given rectangle.
 *
 */
EelIRect
eel_gdk_pixbuf_intersect (const GdkPixbuf *pixbuf,
			  int pixbuf_x,
			  int pixbuf_y,
			  EelIRect rectangle)
{
	EelIRect intersection;
	EelIRect bounds;
	EelDimensions dimensions;

	g_return_val_if_fail (eel_gdk_pixbuf_is_valid (pixbuf), eel_irect_empty);

	dimensions = eel_gdk_pixbuf_get_dimensions (pixbuf);
	bounds = eel_irect_assign_dimensions (pixbuf_x, pixbuf_y, dimensions);

	eel_irect_intersect (&intersection, &rectangle, &bounds);

	/* In theory, this is not needed because a rectangle is empty
	 * regardless of how MUCH negative the dimensions are.  
	 * However, to make debugging and self checks simpler, we
	 * consistenly return a standard empty rectangle.
	 */
	if (eel_irect_is_empty (&intersection)) {
		return eel_irect_empty;
	}

	return intersection;
}

GdkPixbuf *
eel_gdk_pixbuf_scale_down (GdkPixbuf *pixbuf,
			   int dest_width,
			   int dest_height)
{
	int source_width, source_height;
	int s_x1, s_y1, s_x2, s_y2;
	int s_xfrac, s_yfrac;
	int dx, dx_frac, dy, dy_frac;
	div_t ddx, ddy;
	int x, y;
	int r, g, b, a;
	int n_pixels;
	gboolean has_alpha;
	guchar *dest, *src, *xsrc, *src_pixels;
	GdkPixbuf *dest_pixbuf;
	int pixel_stride;
	int source_rowstride, dest_rowstride;

	if (dest_width == 0 || dest_height == 0) {
		return NULL;
	}
	
	source_width = gdk_pixbuf_get_width (pixbuf);
	source_height = gdk_pixbuf_get_height (pixbuf);

	g_assert (source_width >= dest_width);
	g_assert (source_height >= dest_height);

	ddx = div (source_width, dest_width);
	dx = ddx.quot;
	dx_frac = ddx.rem;
	
	ddy = div (source_height, dest_height);
	dy = ddy.quot;
	dy_frac = ddy.rem;

	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	source_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	src_pixels = gdk_pixbuf_get_pixels (pixbuf);

	dest_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, has_alpha, 8,
				      dest_width, dest_height);
	dest = gdk_pixbuf_get_pixels (dest_pixbuf);
	dest_rowstride = gdk_pixbuf_get_rowstride (dest_pixbuf);

	pixel_stride = (has_alpha)?4:3;
	
	s_y1 = 0;
	s_yfrac = -dest_height/2;
	while (s_y1 < source_height) {
		s_y2 = s_y1 + dy;
		s_yfrac += dy_frac;
		if (s_yfrac > 0) {
			s_y2++;
			s_yfrac -= dest_height;
		}

		s_x1 = 0;
		s_xfrac = -dest_width/2;
		while (s_x1 < source_width) {
			s_x2 = s_x1 + dx;
			s_xfrac += dx_frac;
			if (s_xfrac > 0) {
				s_x2++;
				s_xfrac -= dest_width;
			}

			/* Average block of [x1,x2[ x [y1,y2[ and store in dest */
			r = g = b = a = 0;
			n_pixels = 0;

			src = src_pixels + s_y1 * source_rowstride + s_x1 * pixel_stride;
			for (y = s_y1; y < s_y2; y++) {
				xsrc = src;
				if (has_alpha) {
					for (x = 0; x < s_x2-s_x1; x++) {
						n_pixels++;
						
						r += xsrc[3] * xsrc[0];
						g += xsrc[3] * xsrc[1];
						b += xsrc[3] * xsrc[2];
						a += xsrc[3];
						xsrc += 4;
					}
				} else {
					for (x = 0; x < s_x2-s_x1; x++) {
						n_pixels++;
						r += *xsrc++;
						g += *xsrc++;
						b += *xsrc++;
					}
				}
				src += source_rowstride;
			}
			
			if (has_alpha) {
				if (a != 0) {
					*dest++ = r / a;
					*dest++ = g / a;
					*dest++ = b / a;
					*dest++ = a / n_pixels;
				} else {
					*dest++ = 0;
					*dest++ = 0;
					*dest++ = 0;
					*dest++ = 0;
				}
			} else {
				*dest++ = r / n_pixels;
				*dest++ = g / n_pixels;
				*dest++ = b / n_pixels;
			}
			
			s_x1 = s_x2;
		}
		s_y1 = s_y2;
		dest += dest_rowstride - dest_width * pixel_stride;
	}
	
	return dest_pixbuf;
}


#if !defined (EEL_OMIT_SELF_CHECK)

static char *
check_average_value (int width, int height, const char* fill)
{
	char c;
	guint r, g, b, a;
	gboolean alpha, gray;
	int gray_tweak;
	GdkPixbuf *pixbuf;
	int x, y, rowstride, n_channels;
	guchar *pixels;
	guint32 average;
	guchar v;

	r = g = b = a = 0;
	alpha = FALSE;
	gray = FALSE;
	gray_tweak = 0;
	if (sscanf (fill, " %x,%x,%x,%x %c", &r, &g, &b, &a, &c) == 4) {
		alpha = TRUE;
	} else if (sscanf (fill, " %x,%x,%x %c", &r, &g, &b, &c) == 3) {
	} else if (sscanf (fill, " gray%d %c", &gray_tweak, &c) == 1) {
		gray = TRUE;
	} else {
		return g_strdup ("bad fill string format");
	}

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, alpha, 8, width, height);

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	n_channels = gdk_pixbuf_get_n_channels (pixbuf);

	if (!gray) {
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				pixels [y * rowstride + x * n_channels + 0] = r;
				pixels [y * rowstride + x * n_channels + 1] = g;
				pixels [y * rowstride + x * n_channels + 2] = b;
				if (alpha) {
					pixels [y * rowstride + x * n_channels + 3] = a;
				}
			}
		}
	} else {
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				v = ((x + y) & 1) ? 0x80 : 0x7F;
				if (((x + y) & 0xFF) == 0)
					v += gray_tweak;
				pixels [y * rowstride + x * n_channels + 0] = v;
				pixels [y * rowstride + x * n_channels + 1] = v;
				pixels [y * rowstride + x * n_channels + 2] = v;
			}
		}
		pixels [0] += gray_tweak;
		pixels [1] += gray_tweak;
		pixels [2] += gray_tweak;
	}

	average = eel_gdk_pixbuf_average_value (pixbuf);
	g_object_unref (pixbuf);

	return g_strdup_printf ("%02X,%02X,%02X,%02X",
				(average >> 16) & 0xFF,
				(average >> 8) & 0xFF,
				average & 0xFF,
				average >> 24);
}

void
eel_self_check_gdk_pixbuf_extensions (void)
{
	GdkPixbuf *pixbuf;
	EelIRect clip_area;

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 100);

	EEL_CHECK_BOOLEAN_RESULT (eel_gdk_pixbuf_is_valid (pixbuf), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_gdk_pixbuf_is_valid (NULL), FALSE);

	EEL_CHECK_DIMENSIONS_RESULT (eel_gdk_pixbuf_get_dimensions (pixbuf), 100, 100);

	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, eel_gdk_pixbuf_whole_pixbuf), 0, 0, 100, 100);

	clip_area = eel_irect_assign (0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 0, 0);

	clip_area = eel_irect_assign (0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 0, 0);

	clip_area = eel_irect_assign (0, 0, 100, 100);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 100, 100);

	clip_area = eel_irect_assign (-10, -10, 100, 100);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 90, 90);

	clip_area = eel_irect_assign (-10, -10, 110, 110);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 100, 100);

	clip_area = eel_irect_assign (0, 0, 99, 99);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 99, 99);

	clip_area = eel_irect_assign (0, 0, 1, 1);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 1, 1);

	clip_area = eel_irect_assign (-1, -1, 1, 1);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 0, 0);

	clip_area = eel_irect_assign (-1, -1, 2, 2);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 1, 1);

	clip_area = eel_irect_assign (100, 100, 1, 1);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 0, 0);

	clip_area = eel_irect_assign (101, 101, 1, 1);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 0, 0, 0, 0);

	clip_area = eel_irect_assign (80, 0, 100, 100);
	EEL_CHECK_RECTANGLE_RESULT (eel_gdk_pixbuf_intersect (pixbuf, 0, 0, clip_area), 80, 0, 100, 100);

	g_object_unref (pixbuf);

	/* No checks for empty pixbufs because GdkPixbuf doesn't seem to allow them. */
	EEL_CHECK_STRING_RESULT (check_average_value (1, 1, "00,00,00"), "00,00,00,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1, 1, "00,00,00,00"), "00,00,00,00");
	EEL_CHECK_STRING_RESULT (check_average_value (1, 1, "00,00,00,FF"), "00,00,00,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1, 1, "01,01,01"), "01,01,01,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1, 1, "FE,FE,FE"), "FE,FE,FE,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1, 1, "FF,FF,FF"), "FF,FF,FF,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1, 1, "FF,FF,FF,00"), "00,00,00,00");
	EEL_CHECK_STRING_RESULT (check_average_value (1, 1, "11,22,33"), "11,22,33,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "00,00,00"), "00,00,00,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "00,00,00,00"), "00,00,00,00");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "00,00,00,FF"), "00,00,00,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "01,01,01"), "01,01,01,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "FE,FE,FE"), "FE,FE,FE,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "FF,FF,FF"), "FF,FF,FF,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "FF,FF,FF,00"), "00,00,00,00");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "11,22,33"), "11,22,33,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "gray -1"), "7F,7F,7F,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "gray 0"), "80,80,80,FF");
	EEL_CHECK_STRING_RESULT (check_average_value (1000, 1000, "gray 1"), "80,80,80,FF");
}

#endif /* !EEL_OMIT_SELF_CHECK */

