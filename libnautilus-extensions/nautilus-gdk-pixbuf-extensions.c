/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gdk-pixbuf-extensions.c: Routines to augment what's in gdk-pixbuf.

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
#include "nautilus-gdk-pixbuf-extensions.h"

#include "nautilus-gdk-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-string.h"
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <math.h>
#include <png.h>
#include <stdlib.h>

#define LOAD_BUFFER_SIZE 4096

struct NautilusPixbufLoadHandle {
	GnomeVFSAsyncHandle *vfs_handle;
	NautilusPixbufLoadCallback callback;
	gpointer callback_data;
	GdkPixbufLoader *loader;
	char buffer[LOAD_BUFFER_SIZE];
};

static void file_opened_callback (GnomeVFSAsyncHandle      *vfs_handle,
				  GnomeVFSResult            result,
				  gpointer                  callback_data);
static void file_read_callback   (GnomeVFSAsyncHandle      *vfs_handle,
				  GnomeVFSResult            result,
				  gpointer                  buffer,
				  GnomeVFSFileSize          bytes_requested,
				  GnomeVFSFileSize          bytes_read,
				  gpointer                  callback_data);
static void file_closed_callback (GnomeVFSAsyncHandle      *handle,
				  GnomeVFSResult            result,
				  gpointer                  callback_data);
static void load_done            (NautilusPixbufLoadHandle *handle,
				  GnomeVFSResult            result,
				  gboolean		    get_pixbuf);

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
	nautilus_g_list_free_deep_custom (pixbuf_list, (GFunc) gdk_pixbuf_unref, NULL);
}

GdkPixbuf *
nautilus_gdk_pixbuf_load (const char *uri)
{
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	char buffer[LOAD_BUFFER_SIZE];
	GnomeVFSFileSize bytes_read;
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;	

	g_return_val_if_fail (uri != NULL, NULL);

	result = gnome_vfs_open (&handle,
				 uri,
				 GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();
	while (1) {
		result = gnome_vfs_read (handle,
					 buffer,
					 sizeof (buffer),
					 &bytes_read);
		if (result != GNOME_VFS_OK) {
			break;
		}
		if (bytes_read == 0) {
			break;
		}
		if (!gdk_pixbuf_loader_write (loader,
					      buffer,
					      bytes_read)) {
			result = GNOME_VFS_ERROR_WRONG_FORMAT;
			break;
		}
	}

	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		gtk_object_unref (GTK_OBJECT (loader));
		gnome_vfs_close (handle);
		return NULL;
	}

	gnome_vfs_close (handle);
	gdk_pixbuf_loader_close (loader);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf != NULL) {
		gdk_pixbuf_ref (pixbuf);
	}
	gtk_object_unref (GTK_OBJECT (loader));

	return pixbuf;
}

NautilusPixbufLoadHandle *
nautilus_gdk_pixbuf_load_async (const char *uri,
				NautilusPixbufLoadCallback callback,
				gpointer callback_data)
{
	NautilusPixbufLoadHandle *handle;

	handle = g_new0 (NautilusPixbufLoadHandle, 1);
	handle->callback = callback;
	handle->callback_data = callback_data;

	gnome_vfs_async_open (&handle->vfs_handle,
			      uri,
			      GNOME_VFS_OPEN_READ,
			      file_opened_callback,
			      handle);

	return handle;
}

static void
file_opened_callback (GnomeVFSAsyncHandle *vfs_handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	NautilusPixbufLoadHandle *handle;

	handle = callback_data;
	g_assert (handle->vfs_handle == vfs_handle);

	if (result != GNOME_VFS_OK) {
		handle->vfs_handle = NULL;
		load_done (handle, result, FALSE);
		return;
	}

	handle->loader = gdk_pixbuf_loader_new ();

	gnome_vfs_async_read (handle->vfs_handle,
			      handle->buffer,
			      sizeof (handle->buffer),
			      file_read_callback,
			      handle);
}

static void
file_read_callback (GnomeVFSAsyncHandle *vfs_handle,
		    GnomeVFSResult result,
		    gpointer buffer,
		    GnomeVFSFileSize bytes_requested,
		    GnomeVFSFileSize bytes_read,
		    gpointer callback_data)
{
	NautilusPixbufLoadHandle *handle;

	handle = callback_data;
	g_assert (handle->vfs_handle == vfs_handle);
	g_assert (handle->buffer == buffer);

	if (result == GNOME_VFS_OK && bytes_read != 0) {
		if (!gdk_pixbuf_loader_write (handle->loader,
					      buffer,
					      bytes_read)) {
			result = GNOME_VFS_ERROR_WRONG_FORMAT;
		}
		gnome_vfs_async_read (handle->vfs_handle,
				      handle->buffer,
				      sizeof (handle->buffer),
				      file_read_callback,
				      handle);
		return;
	}

	load_done (handle, result, result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF);
}

static void
file_closed_callback (GnomeVFSAsyncHandle *handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	g_assert (callback_data == NULL);
}

static void
free_pixbuf_load_handle (NautilusPixbufLoadHandle *handle)
{
	if (handle->loader != NULL) {
		gtk_object_unref (GTK_OBJECT (handle->loader));
	}
	g_free (handle);
}

static void
load_done (NautilusPixbufLoadHandle *handle, GnomeVFSResult result, gboolean get_pixbuf)
{
	GdkPixbuf *pixbuf;

	if (handle->loader != NULL) {
		gdk_pixbuf_loader_close (handle->loader);
	}

	pixbuf = get_pixbuf ? gdk_pixbuf_loader_get_pixbuf (handle->loader) : NULL;
	
	if (handle->vfs_handle != NULL) {
		gnome_vfs_async_close (handle->vfs_handle, file_closed_callback, NULL);
	}

	handle->callback (result, pixbuf, handle->callback_data);

	free_pixbuf_load_handle (handle);
}

void
nautilus_cancel_gdk_pixbuf_load (NautilusPixbufLoadHandle *handle)
{
	if (handle == NULL) {
		return;
	}
	if (handle->vfs_handle != NULL) {
		gnome_vfs_async_cancel (handle->vfs_handle);
	}
	free_pixbuf_load_handle (handle);
}

/* return the average value of each component */
void 
nautilus_gdk_pixbuf_average_value (GdkPixbuf *pixbuf, GdkColor *color)
{
	uint red_total, green_total, blue_total, count;
	int row, column;
	int width, height;
	int row_stride;
	guchar *pixsrc, *original_pixels;
	
	gboolean has_alpha;
	
	red_total = 0;
	green_total = 0;
	blue_total = 0;
	count = 0;

	/* iterate through the pixbuf, counting up each component */
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);
	original_pixels = gdk_pixbuf_get_pixels (pixbuf);

	for (row = 0; row < height; row++) {
		pixsrc = original_pixels + (row * row_stride);
		for (column = 0; column < width; column++) {
			red_total += *pixsrc++;
			green_total += *pixsrc++;
			blue_total += *pixsrc++;
			count += 1;
			if (has_alpha) {
				pixsrc++;
			}
		}
	}

	color->red =   (red_total   * 256) / count;
	color->green = (green_total * 256) / count;
	color->blue =  (blue_total  * 256) / count;
}

double
nautilus_gdk_scale_to_fit_factor (int width, int height,
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
nautilus_gdk_pixbuf_scale_to_fit (GdkPixbuf *pixbuf, int max_width, int max_height)
{
	int scaled_width;
	int scaled_height;

	nautilus_gdk_scale_to_fit_factor (gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf),
					  max_width, max_height,
					  &scaled_width, &scaled_height);

	return gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_BILINEAR);	
}

/* Returns a copy of pixbuf scaled down, preserving aspect ratio, to fit
 * within the specified width and height. If it already fits, a copy of
 * the original, without scaling, is returned.
 */
GdkPixbuf *
nautilus_gdk_pixbuf_scale_down_to_fit (GdkPixbuf *pixbuf, int max_width, int max_height)
{
	int scaled_width;
	int scaled_height;
	
	double scale_factor;

	scale_factor = nautilus_gdk_scale_to_fit_factor (gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf),
							 max_width, max_height,
							 &scaled_width, &scaled_height);

	if (scale_factor >= 1.0) {
		return gdk_pixbuf_copy (pixbuf);
	} else {				
		return gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_BILINEAR);	
	}
}

static gboolean
nautilus_gdk_pixbuf_is_valid (const GdkPixbuf *pixbuf)
{
	return ((pixbuf != NULL)
		&& (gdk_pixbuf_get_pixels (pixbuf) != NULL)
		&& (gdk_pixbuf_get_width (pixbuf) > 0)
		&& (gdk_pixbuf_get_height (pixbuf) > 0));
}

static ArtIRect
nautilus_gdk_pixbuf_get_frame (const GdkPixbuf *pixbuf)
{
	ArtIRect frame;

	g_return_val_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf), NAUTILUS_ART_IRECT_EMPTY);

	frame.x0 = 0;
	frame.y0 = 0;
	frame.x1 = gdk_pixbuf_get_width (pixbuf);
	frame.y1 = gdk_pixbuf_get_height (pixbuf);

	return frame;
}

/**
 * nautilus_gdk_pixbuf_fill_rectangle:
 * @pixbuf: Target pixbuf to fill into.
 * @area: Rectangle to fill.
 * @color: The color to use.
 *
 * Fill the rectangle with the the given color.
 * Use the given rectangle if not NULL.
 * If rectangle is NULL, fill the whole pixbuf.
 */
void
nautilus_gdk_pixbuf_fill_rectangle_with_color (GdkPixbuf *pixbuf,
					       const ArtIRect *area,
					       guint32 color)
{
	ArtIRect frame;
	ArtIRect target;
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

	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));

	frame = nautilus_gdk_pixbuf_get_frame (pixbuf);

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	pixel_offset = has_alpha ? 4 : 3;

	/* If an area is given, make sure its clipped to the pixbuf frame */
	if (area != NULL) {
		
		art_irect_intersect (&target, area, &frame);

		if (art_irect_empty (&target)) {
			return;
		}
	/* If no area is given, then use the whole pixbuf frame */
	} else {
		target = frame;
	}
	
	red = NAUTILUS_RGBA_COLOR_GET_R (color);
	green = NAUTILUS_RGBA_COLOR_GET_G (color);
	blue = NAUTILUS_RGBA_COLOR_GET_B (color);
	alpha = NAUTILUS_RGBA_COLOR_GET_A (color);

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

/* utility routine for saving a pixbuf to a png file.
 * This was adapted from Iain Holmes' code in gnome-iconedit, and probably
 * should be in a utility library, possibly in gdk-pixbuf itself.
 */
gboolean
nautilus_gdk_pixbuf_save_to_file (const GdkPixbuf *pixbuf,
				  const char *file_name)
{
	FILE *handle;
  	char *buffer;
	gboolean has_alpha;
	int width, height, depth, rowstride;
  	guchar *pixels;
  	png_structp png_ptr;
  	png_infop info_ptr;
  	png_text text[2];
  	int i;

	g_return_val_if_fail (pixbuf != NULL, FALSE);
	g_return_val_if_fail (file_name != NULL, FALSE);
	g_return_val_if_fail (file_name[0] != '\0', FALSE);

        handle = fopen (file_name, "wb");
        if (handle == NULL) {
        	return FALSE;
	}

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fclose (handle);
		return FALSE;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct (&png_ptr, (png_infopp)NULL);
		fclose (handle);
	    	return FALSE;
	}

	if (setjmp (png_ptr->jmpbuf)) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		fclose (handle);
		return FALSE;
	}

	png_init_io (png_ptr, handle);

        has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	depth = gdk_pixbuf_get_bits_per_sample (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	png_set_IHDR (png_ptr, info_ptr, width, height,
			depth, PNG_COLOR_TYPE_RGB_ALPHA,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);

	/* Some text to go with the png image */
	text[0].key = "Title";
	text[0].text = (char *) file_name;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = "Nautilus Thumbnail";
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;
	png_set_text (png_ptr, info_ptr, text, 2);

	/* Write header data */
	png_write_info (png_ptr, info_ptr);

	/* if there is no alpha in the data, allocate buffer to expand into */
	if (has_alpha) {
		buffer = NULL;
	} else {
		buffer = g_malloc(4 * width);
	}
	
	/* pump the raster data into libpng, one scan line at a time */	
	for (i = 0; i < height; i++) {
		if (has_alpha) {
			png_bytep row_pointer = pixels;
			png_write_row (png_ptr, row_pointer);
		} else {
			/* expand RGB to RGBA using an opaque alpha value */
			int x;
			char *buffer_ptr = buffer;
			char *source_ptr = pixels;
			for (x = 0; x < width; x++) {
				*buffer_ptr++ = *source_ptr++;
				*buffer_ptr++ = *source_ptr++;
				*buffer_ptr++ = *source_ptr++;
				*buffer_ptr++ = 255;
			}
			png_write_row (png_ptr, (png_bytep) buffer);		
		}
		pixels += rowstride;
	}
	
	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);
	
	g_free (buffer);
		
	fclose (handle);
	return TRUE;
}

void
nautilus_gdk_pixbuf_ref_if_not_null (GdkPixbuf *pixbuf_or_null)
{
	if (pixbuf_or_null != NULL) {
		gdk_pixbuf_ref (pixbuf_or_null);
	}
}

void
nautilus_gdk_pixbuf_unref_if_not_null (GdkPixbuf *pixbuf_or_null)
{
	if (pixbuf_or_null != NULL) {
		gdk_pixbuf_unref (pixbuf_or_null);
	}
}

static ArtIRect
nautilus_gdk_window_get_frame (const GdkWindow *window)
{
	ArtIRect frame;
	
	g_return_val_if_fail (window != NULL, NAUTILUS_ART_IRECT_EMPTY);

	frame.x0 = 0;
	frame.y0 = 0;
	gdk_window_get_size ((GdkWindow *) window, &frame.x1, &frame.y1);

	return frame;
}

void
nautilus_gdk_pixbuf_draw_to_drawable (const GdkPixbuf *pixbuf,
				      GdkDrawable *drawable,
				      GdkGC *gc,
				      int source_x,
				      int source_y,
				      const ArtIRect *destination_area,
				      GdkRgbDither dither,
				      GdkPixbufAlphaMode alpha_compositing_mode,
				      int alpha_threshold)
{
	ArtIRect frame;
	ArtIRect target;
	ArtIRect source;
	ArtIRect destination_frame;
	int target_width;
	int target_height;
	int source_width;
	int source_height;

	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (drawable != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (destination_area != NULL);
 	g_return_if_fail (destination_area->x1 > destination_area->x0);
 	g_return_if_fail (destination_area->y1 > destination_area->y0);
 	g_return_if_fail (alpha_threshold > NAUTILUS_OPACITY_FULL);
 	g_return_if_fail (alpha_threshold <= NAUTILUS_OPACITY_NONE);
 	g_return_if_fail (alpha_compositing_mode >= GDK_PIXBUF_ALPHA_BILEVEL);
 	g_return_if_fail (alpha_compositing_mode <= GDK_PIXBUF_ALPHA_FULL);

	frame = nautilus_gdk_pixbuf_get_frame (pixbuf);
	destination_frame = nautilus_gdk_window_get_frame (drawable);
	
	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < (frame.x1 - frame.x0));
	g_return_if_fail (source_y < (frame.y1 - frame.y0));

	/* Clip the destination area to the destination pixbuf frame */
	art_irect_intersect (&target, destination_area, &destination_frame);
	if (art_irect_empty (&target)) {
 		return;
 	}

	/* Assign the source area */
	nautilus_art_irect_assign (&source,
				   source_x,
				   source_y,
				   frame.x1 - frame.x0 - source_x,
				   frame.y1 - frame.y0 - source_y);

	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf frame */
	target_width = target.x1 - target.x0;
	target_height = target.y1 - target.y0;
	source_width = source.x1 - source.x0;
	source_height = source.y1 - source.y0;

	target.x1 = target.x0 + MIN (target_width, source_width);
	target.y1 = target.y0 + MIN (target_height, source_height);

	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		gdk_pixbuf_render_to_drawable_alpha ((GdkPixbuf *) pixbuf,
						     drawable,
						     source.x0,
						     source.y0,
						     target.x0,
						     target.y0,
						     target.x1 - target.x0,
						     target.y1 - target.y0,
						     alpha_compositing_mode,
						     alpha_threshold,
						     dither,
						     0,
						     0);
	} else {
		gdk_pixbuf_render_to_drawable ((GdkPixbuf *) pixbuf,
					       drawable,
					       gc,
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

}

/**
 * nautilus_gdk_pixbuf_draw_to_pixbuf:
 * @pixbuf: The source pixbuf to draw.
 * @destination_pixbuf: The destination pixbuf.
 * @source_x: The source pixbuf x coordiate to composite from.
 * @source_y: The source pixbuf y coordiate to composite from.
 * @destination_area: The destination area within the destination pixbuf.
 *                    This area will be clipped if invalid in any way.
 *
 * Copy one pixbuf onto another another.  This function has some advantages
 * over plain gdk_pixbuf_composite():
 *
 *   Composition paramters (source coordinate, destination area) are
 *   given in a way that is consistent with the rest of the extensions
 *   in this file.  That is, it matches the declaration of
 *   nautilus_gdk_pixbuf_draw_to_pixbuf_alpha() and 
 *   nautilus_gdk_pixbuf_draw_to_drawable() very closely.
 *
 *   All values are clipped to make sure the are valid.
 *
 */
void
nautilus_gdk_pixbuf_draw_to_pixbuf (const GdkPixbuf *pixbuf,
				    GdkPixbuf *destination_pixbuf,
				    int source_x,
				    int source_y,
				    const ArtIRect *destination_area)
{
	ArtIRect frame;
	ArtIRect target;
	ArtIRect source;
	ArtIRect destination_frame;
	int target_width;
	int target_height;
	int source_width;
	int source_height;
	
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (destination_area->x1 > destination_area->x0);
	g_return_if_fail (destination_area->y1 > destination_area->y0);
	
	frame = nautilus_gdk_pixbuf_get_frame (pixbuf);
	destination_frame = nautilus_gdk_pixbuf_get_frame (destination_pixbuf);

	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < (frame.x1 - frame.x0));
	g_return_if_fail (source_y < (frame.y1 - frame.y0));

	/* Clip the destination area to the destination pixbuf frame */
 	art_irect_intersect (&target, destination_area, &destination_frame);
	if (art_irect_empty (&target)) {
 		return;
 	}

	/* Assign the source area */
	nautilus_art_irect_assign (&source,
				   source_x,
				   source_y,
				   frame.x1 - frame.x0 - source_x,
				   frame.y1 - frame.y0 - source_y);

	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf frame */
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
 * nautilus_gdk_pixbuf_draw_to_pixbuf_alpha:
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
 *   nautilus_gdk_pixbuf_draw_to_pixbuf() and 
 *   nautilus_gdk_pixbuf_draw_to_drawable() very closely.
 *
 *   All values are clipped to make sure the are valid.
 *
 *   Workaround a limitation in gdk_pixbuf_composite() that does not allow
 *   the source (x,y) to be greater than (0,0)
 * 
 */
void
nautilus_gdk_pixbuf_draw_to_pixbuf_alpha (const GdkPixbuf *pixbuf,
					  GdkPixbuf *destination_pixbuf,
					  int source_x,
					  int source_y,
					  const ArtIRect *destination_area,
					  int opacity,
					  GdkInterpType interpolation_mode)
{
	ArtIRect frame;
	ArtIRect target;
	ArtIRect source;
	ArtIRect destination_frame;
	int target_width;
	int target_height;
	int source_width;
	int source_height;

	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (destination_area->x1 > destination_area->x0);
	g_return_if_fail (destination_area->y1 > destination_area->y0);
	g_return_if_fail (opacity >= NAUTILUS_OPACITY_FULL);
	g_return_if_fail (opacity <= NAUTILUS_OPACITY_NONE);
	g_return_if_fail (interpolation_mode >= GDK_INTERP_NEAREST);
	g_return_if_fail (interpolation_mode <= GDK_INTERP_HYPER);
	
	frame = nautilus_gdk_pixbuf_get_frame (pixbuf);
	destination_frame = nautilus_gdk_pixbuf_get_frame (destination_pixbuf);

	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < (frame.x1 - frame.x0));
	g_return_if_fail (source_x < (frame.y1 - frame.y0));

	/* Clip the destination area to the destination pixbuf frame */
 	art_irect_intersect (&target, destination_area, &destination_frame);
 	if (art_irect_empty (&target)) {
 		return;
 	}

	/* Assign the source area */
	nautilus_art_irect_assign (&source,
				   source_x,
				   source_y,
				   frame.x1 - frame.x0 - source_x,
				   frame.y1 - frame.y0 - source_y);
	
	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf frame */
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
		ArtIRect area;
		int width;
		int height;

		width = frame.x1 - frame.x0 - source.x0;
		height = frame.y1 - frame.y0 - source.y0;
		
		area.x0 = source.x0;
		area.y0 = source.y0;
		area.x1 = area.x0 + width;
		area.y1 = area.y0 + height;
		
		pixbuf = nautilus_gdk_pixbuf_new_from_pixbuf_sub_area ((GdkPixbuf *) pixbuf, &area);
	} else {
		gdk_pixbuf_ref ((GdkPixbuf *) pixbuf);
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

	gdk_pixbuf_unref ((GdkPixbuf *) pixbuf);
}

static void
pixbuf_destroy_callback (guchar *pixels,
			 gpointer callback_data)
{
	g_return_if_fail (pixels != NULL);
	g_return_if_fail (callback_data != NULL);

	gdk_pixbuf_unref ((GdkPixbuf *) callback_data);
}

/**
 * nautilus_gdk_pixbuf_new_from_pixbuf_sub_area:
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
 * all taken care for the caller.  All you need to do is gdk_pixbuf_unref()
 * the resulting pixbuf to properly free resources.
 */
GdkPixbuf *
nautilus_gdk_pixbuf_new_from_pixbuf_sub_area (GdkPixbuf *pixbuf,
					      const ArtIRect *area)
{
	GdkPixbuf *sub_pixbuf;
	ArtIRect frame;
	ArtIRect target;
	guchar *pixels;
	
	g_return_val_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf), NULL);
	g_return_val_if_fail (area != NULL, NULL);
	g_return_val_if_fail (area->x1 > area->x0, NULL);
	g_return_val_if_fail (area->y1 > area->y0, NULL);
	
	frame = nautilus_gdk_pixbuf_get_frame (pixbuf);

 	art_irect_intersect (&target, area, &frame);
 	if (art_irect_empty (&target)) {
 		return NULL;
 	}

	/* Since we are going to be sharing the given pixbuf's data, we need 
	 * to ref it.  It will be unreffed in the destroy function above */
	gdk_pixbuf_ref (pixbuf);

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
					       target.x1 - target.x0,
					       target.y1 - target.y0,
					       gdk_pixbuf_get_rowstride (pixbuf),
					       pixbuf_destroy_callback,
					       pixbuf);

	return sub_pixbuf;
}

/**
 * nautilus_gdk_pixbuf_draw_to_pixbuf_tiled:
 * @pixbuf: Source tile pixbuf.
 * @destination_pixbuf: Destination pixbuf.
 * @destination_area: Area of the destination pixbuf to tile.
 * @tile_width: Width of the tile.  This can be less than width of the
 *              tile pixbuf, but not greater.
 * @tile_height: Height of the tile.  This can be less than width of the
 *               tile pixbuf, but not greater.
 * @tile_origin_x: The x coordinate of the tile origin.  Can be negative.
 * @tile_origin_y: The y coordinate of the tile origin.  Can be negative.
 * @opacity: The opacity of the drawn tiles where 0 <= opacity <= 255.
 * @interpolation_mode: The interpolation mode.  See <gdk-pixbuf.h>
 *
 * Fill an area of a pixbuf with a tile.
 */
void
nautilus_gdk_pixbuf_draw_to_pixbuf_tiled (const GdkPixbuf *pixbuf,
					  GdkPixbuf *destination_pixbuf,
					  const ArtIRect *destination_area,
					  int tile_width,
					  int tile_height,
					  int tile_origin_x,
					  int tile_origin_y,
					  guchar opacity,
					  GdkInterpType interpolation_mode)
{
	ArtIRect frame;
	ArtIRect target;
	int x;
	int y;
	NautilusArtIPoint min_point;
	NautilusArtIPoint max_point;
	int num_left;
	int num_above;

	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (tile_width > 0);
	g_return_if_fail (tile_height > 0);
	g_return_if_fail (tile_width <= gdk_pixbuf_get_width (pixbuf));
	g_return_if_fail (tile_height <= gdk_pixbuf_get_height (pixbuf));
	g_return_if_fail (opacity != NAUTILUS_OPACITY_FULL);
	g_return_if_fail (interpolation_mode >= GDK_INTERP_NEAREST);
	g_return_if_fail (interpolation_mode <= GDK_INTERP_HYPER);

	frame = nautilus_gdk_pixbuf_get_frame (destination_pixbuf);

	if (destination_area != NULL) {
		art_irect_intersect (&target, destination_area, &frame);

		if (art_irect_empty (&target)) {
			return;
		}
	} else {
		target = frame;
	}

	/* The number of tiles left and above the target area */
	num_left = (target.x0 - tile_origin_x) / tile_width;
	num_above = (target.y0 - tile_origin_y) / tile_height;
	
	nautilus_art_ipoint_assign (&min_point,
				    tile_origin_x - tile_width + (num_left * tile_width),
				    tile_origin_y - tile_height + (num_above * tile_height));
	
	nautilus_art_ipoint_assign (&max_point,
				    (target.x1 + 2 * tile_width),
				    (target.y1 + 2 * tile_height));
	
	for (y = min_point.y; y <= max_point.y; y += tile_height) {
		for (x = min_point.x; x <= max_point.x; x += tile_width) {
			ArtIRect current;
			ArtIRect area;

			nautilus_art_irect_assign (&current, x, y, tile_width, tile_height);

			/* FIXME: A potential speed improvement here would be to clip only the
			 * first and last rectangles, not the ones in between.  
			 */
			art_irect_intersect (&area, &target, &current);

			if (!art_irect_empty (&area)) {
				g_assert (area.x0 >= x);
				g_assert (area.y0 >= y);

				if (opacity == NAUTILUS_OPACITY_NONE) {
					nautilus_gdk_pixbuf_draw_to_pixbuf (pixbuf,
									    destination_pixbuf,
									    area.x0 - x,
									    area.y0 - y,
									    &area);
				} else {
					nautilus_gdk_pixbuf_draw_to_pixbuf_alpha (pixbuf,
										  destination_pixbuf,
										  area.x0 - x,
										  area.y0 - y,
										  &area,
										  opacity,
										  interpolation_mode);
				}
			}
		}
	}
}

/**
 * nautilus_gdk_pixbuf_show_in_eog:
 * @pixbuf: Pixbuf to show.
 *
 * Show the given pixbuf in eog.  This is very useful for debugging pixbuf
 * stuff.  Perhaps this function should be #ifdef DEBUG or something like that.
 */
void
nautilus_gdk_pixbuf_show_in_eog (const GdkPixbuf *pixbuf)
{
	char *command;
	char *file_name;
	gboolean save_result;

	g_return_if_fail (pixbuf != NULL);

	file_name = g_strdup ("/tmp/nautilus-debug-png-file-XXXXXX");

	if (mktemp (file_name) != file_name) {
		g_free (file_name);
		file_name = g_strdup_printf ("/tmp/isis-debug-png-file.%d", getpid ());
	}

	save_result = nautilus_gdk_pixbuf_save_to_file (pixbuf, file_name);

	if (save_result == FALSE) {
		g_warning ("Failed to save '%s'", file_name);
		g_free (file_name);
		return;
	}
	
	command = g_strdup_printf ("eog %s", file_name);

	system (command);
	g_free (command);
	remove (file_name);
	g_free (file_name);
}
