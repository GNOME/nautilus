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
				  GdkPixbuf                *pixbuf);

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
		load_done (handle, result, NULL);
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
	GdkPixbuf *pixbuf;

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

	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		pixbuf = NULL;
	} else {
		pixbuf = gdk_pixbuf_loader_get_pixbuf (handle->loader);
	}

	load_done (handle, result, pixbuf);
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
load_done (NautilusPixbufLoadHandle *handle,
	   GnomeVFSResult result,
	   GdkPixbuf *pixbuf)
{
	if (handle->vfs_handle != NULL) {
		gnome_vfs_async_close (handle->vfs_handle, file_closed_callback, NULL);
	}
	(* handle->callback) (result, pixbuf, handle->callback_data);
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

/* FIXME
 * This fn is only used by some test code, it should probably be removed
 */
void
nautilus_gdk_pixbuf_render_to_pixbuf_tiled (GdkPixbuf *source_pixbuf,
					    GdkPixbuf *destination_pixbuf,
					    const GdkRectangle *rect,
					    int x_dither,
					    int y_dither)
{
	int x, y;
	int start_x, start_y;
	int end_x, end_y;
	int tile_x, tile_y;
	int blit_x, blit_y;
	int tile_width, tile_height;
	int blit_width, blit_height;
	int tile_offset_x, tile_offset_y;

	tile_width = gdk_pixbuf_get_width (source_pixbuf);
	tile_height = gdk_pixbuf_get_height (source_pixbuf);

	tile_offset_x = (rect->x - x_dither) % tile_width;
	if (tile_offset_x < 0) {
		tile_offset_x += tile_width;
	}
	g_assert (tile_offset_x >= 0 && tile_offset_x < tile_width);

	tile_offset_y = (rect->y - y_dither) % tile_height;
	if (tile_offset_y < 0) {
		tile_offset_y += tile_height;
	}
	g_assert (tile_offset_y >= 0 && tile_offset_y < tile_height);

	start_x = rect->x - tile_offset_x;
	start_y = rect->y - tile_offset_y;

	end_x = rect->x + rect->width;
	end_y = rect->y + rect->height;

	for (x = start_x; x < end_x; x += tile_width) {
		blit_x = MAX (x, rect->x);
		tile_x = blit_x - x;
		blit_width = MIN (tile_width, end_x - x) - tile_x;
		
		for (y = start_y; y < end_y; y += tile_height) {
			blit_y = MAX (y, rect->y);
			tile_y = blit_y - y;
			blit_height = MIN (tile_height, end_y - y) - tile_y;
			
// 			gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
// 						       tile_x, tile_y,
// 						       blit_x, blit_y, blit_width, blit_height,
// 						       dither, x_dither, y_dither);

// 			gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
// 						       tile_x, tile_y,
// 						       blit_x, blit_y, blit_width, blit_height,
// 						       dither, x_dither, y_dither);

			gdk_pixbuf_copy_area (source_pixbuf,
					      tile_x, 
					      tile_y,
					      blit_width,
					      blit_height,
					      destination_pixbuf,
					      blit_x,
					      blit_y);

// 		/* blit the pixbuf to the drawable */
// 		gdk_pixbuf_render_to_drawable (pixbuf,
// 					       drawable,
// 					       gc,
// 					       0,
// 					       0,
// 					       rectangle->x,
// 					       rectangle->y,
// 					       rectangle->width,
// 					       rectangle->height,
// 					       GDK_RGB_DITHER_NORMAL,
// 					       origin_x,
// 					       origin_y);

// 		gdk_pixbuf_copy_area (pixbuf,
// 				      0,
// 				      0,
// 				      rectangle->width,
// 				      rectangle->height,
// 				      destination_pixbuf,
// 				      rectangle->x,
// 				      rectangle->y);

		}
	}
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

/**
 * nautilus_gdk_pixbuf_draw_text
 * @pixbuf: A GdkPixbuf.
 * @font: A GdkFont.
 * @destination_rect: An ArtIRect - the destination bounding box for the text.
 * @text: A string - the text to draw.
 * @overall_alpha: The overall alpha to use when compositing the text on the pixbuf
 *
 * Draw text onto a GdkPixbuf using the given font and rect
 **/
void
nautilus_gdk_pixbuf_draw_text (GdkPixbuf	*pixbuf,
			       const GdkFont	*font,
			       const double	font_scale,
			       const ArtIRect	*destination_rect,
			       const char	*text,
			       guint		text_color,
			       guint		overall_alpha)
{
	ArtIRect	pixbuf_rect;
	ArtIRect	text_rect;
	int		dest_width, dest_height;
	int		width, height;
	GdkVisual	*visual;
	GdkPixmap	*pixmap;
	GdkGC		*gc;
	GdkColormap	*colormap;
	int		y;
	const char	*line;
	const char	*end_of_line;
	int		line_length;
	GdkPixbuf	*text_pixbuf;
	GdkPixbuf	*text_pixbuf_with_alpha;
	guchar		*pixels;
	
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (font != NULL);
	g_return_if_fail (destination_rect != NULL);
	g_return_if_fail (text != NULL);
	g_return_if_fail (nautilus_strlen (text) > 0);
	g_return_if_fail (overall_alpha <= 255);

	/* Compute the intersection of the text rectangle with the pixbuf.
	 * This is only to prevent pathological cases from upsetting the
	 * GdkPixbuf routines. It should not happen in any normal circumstance.
	 */
	pixbuf_rect.x0 = 0;
	pixbuf_rect.y0 = 0;
	pixbuf_rect.x1 = gdk_pixbuf_get_width (pixbuf);
	pixbuf_rect.y1 = gdk_pixbuf_get_height (pixbuf);
	art_irect_intersect (&text_rect, destination_rect, &pixbuf_rect);

	/* Get the system visual. I wish I could do this all in 1-bit mode,
	 * but I can't.
	 */
	visual = gdk_visual_get_system ();

	/* Allocate a GdkPixmap of the appropriate size. */
	dest_width  = text_rect.x1 - text_rect.x0;
	dest_height = text_rect.y1 - text_rect.y0;
	width = dest_width / font_scale;
	height = dest_height / font_scale;
	
	/* cut out if it's too small for comfort */
	if (width <= 8 || height <= 8)
		return;
		
	pixmap = gdk_pixmap_new (NULL, width, height, visual->depth);
	gc = gdk_gc_new (pixmap);

	/* Set up a white background. */
	gdk_rgb_gc_set_foreground (gc, text_color == NAUTILUS_RGB_COLOR_WHITE ? NAUTILUS_RGB_COLOR_BLACK : NAUTILUS_RGB_COLOR_WHITE);
	gdk_draw_rectangle (pixmap, gc, TRUE, 0, 0, width, height);
	
	/* Set up the text color. */
	gdk_rgb_gc_set_foreground (gc, text_color);
	gdk_gc_set_font (gc, (GdkFont *) font);
	line = text;

	/* FIXME bugzilla.eazel.com 2559:
	 * The iteration code should work with strings that dont have 
	 * new lines.  Its broken right now for single line strings.  The 
	 * (y + font->ascent) <= height test always fails and no text is drawn.
	 */
	if (strchr (line, '\n')) {
		for (y = font->ascent;
		     y + font->descent <= height;
		     y += font->ascent + font->descent) {
			
			/* Extract the next line of text. */
			end_of_line = strchr (line, '\n');
			line_length = end_of_line == NULL
				? strlen (line)
				: end_of_line - line;
			
			/* Draw the next line of text. */
			gdk_draw_text (pixmap, (GdkFont *) font, gc, 0, y,
				       line, line_length);
			
			/* Move on to the subsequent line. */
			line = end_of_line == NULL
				? ""
				: end_of_line + 1;
		}
	}
	else {
		/* Draw the next line of text. */
		gdk_draw_text (pixmap, (GdkFont *) font, gc, 0, font->ascent,
			       line, strlen (line));
	}
	gdk_gc_unref (gc);

	/* Convert into a GdkPixbuf with gdk_pixbuf_get_from_drawable. */
	colormap = gdk_colormap_new (visual, FALSE);
	text_pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, colormap,
						    0, 0,
						    0, 0, width, height);
	gdk_colormap_unref (colormap);
	gdk_pixmap_unref (pixmap);
	
	/* White is not always FF FF FF. So we get the top left corner pixel. */
	pixels = gdk_pixbuf_get_pixels (text_pixbuf);
	text_pixbuf_with_alpha = gdk_pixbuf_add_alpha 
		(text_pixbuf,
		 TRUE, pixels[0], pixels[1], pixels[2]);
	gdk_pixbuf_unref (text_pixbuf);
	
	/* composite using scale factor */
	gdk_pixbuf_composite (text_pixbuf_with_alpha,
			      pixbuf,
			      text_rect.x0,
			      text_rect.y0,
			      dest_width, dest_height,
			      text_rect.x0,
			      text_rect.y0,
			      font_scale, font_scale,
			      GDK_INTERP_BILINEAR,
			      overall_alpha);

	gdk_pixbuf_unref (text_pixbuf_with_alpha);
}

/**
 * nautilus_gdk_pixbuf_fill_rectangle:
 * @pixbuf: Target pixbuf to fill into.
 * @rectangle: Rectangle to fill.
 * @color: The color to use.
 *
 * Fill the rectangle with the the given color.
 * Use the given rectangle if not NULL.
 * If rectangle is NULL, fill the whole pixbuf.
 */
void
nautilus_gdk_pixbuf_fill_rectangle_with_color (GdkPixbuf		*pixbuf,
					       const GdkRectangle	*rectangle,
					       guint32			color)
{
	GdkRectangle	area;
	guchar		red;
	guchar		green;
	guchar		blue;
	guchar		alpha;

	guint		width;
	guint		height;
	guchar		*pixels;
	gboolean	has_alpha;
	guint		pixel_offset;
	guint		rowstride;

	int		x;
	int		y;

	int		x1;
	int		y1;
	int		x2;
	int		y2;
	guchar		*row_offset;

	g_return_if_fail (pixbuf != NULL);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

	pixel_offset = has_alpha ? 4 : 3;

	if (rectangle != NULL)
	{
		area = *rectangle;
	}
	else
	{
		area.x = 0;
		area.y = 0;
		area.width = width;
		area.height = height;
	}

	red = NAUTILUS_RGBA_COLOR_GET_R (color);
	green = NAUTILUS_RGBA_COLOR_GET_G (color);
	blue = NAUTILUS_RGBA_COLOR_GET_B (color);
	alpha = NAUTILUS_RGBA_COLOR_GET_A (color);

	x1 = area.x;
	y1 = area.y;
	x2 = area.x + area.width;
	y2 = area.y + area.height;

	row_offset = pixels + y1 * rowstride;

	for (y = y1; y < y2; y++)
	{
		guchar *offset = row_offset + (x1 * pixel_offset);
		
		for (x = x1; x < x2; x++)
		{
			*(offset++) = red;
			*(offset++) = green;
			*(offset++) = blue;
			
			if (has_alpha)
			{
				*(offset++) = alpha;
			}
			
		}

		row_offset += rowstride;
	}
}

void
nautilus_gdk_pixbuf_render_to_drawable (const GdkPixbuf		*pixbuf,
					GdkDrawable		*drawable,
					GdkGC			*gc,
					const GdkPoint		*source_point,
					const GdkRectangle	*destination_area,
					GdkRgbDither		dither)
{
	GdkPoint	src;
	GdkRectangle	dst;
	GdkPoint	end;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (drawable != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (source_point != NULL);
	g_return_if_fail (destination_area != NULL);
 	g_return_if_fail (destination_area->width > 0);
 	g_return_if_fail (destination_area->height > 0);
	g_return_if_fail (source_point->x >= 0);
	g_return_if_fail (source_point->y >= 0);

	g_assert (gdk_pixbuf_get_width (pixbuf) > 0);
	g_assert (gdk_pixbuf_get_height (pixbuf) > 0);

	src = *source_point;
	dst = *destination_area;

	/* Clip to the left edge of the drawable */
	if (dst.x < 0)
	{
		src.x += ABS (dst.x);
		dst.x = 0;
	}

	/* Clip to the top edge of the drawable */
	if (dst.y < 0)
	{
		src.y += ABS (dst.y);
		dst.y = 0;
	}
	
	end.x = src.x + dst.width;
	end.y = src.y + dst.height;
	
	if (end.x >= gdk_pixbuf_get_width (pixbuf))
	{
		g_assert (dst.width >= (end.x - gdk_pixbuf_get_width (pixbuf)));
		
		dst.width -= (end.x - gdk_pixbuf_get_width (pixbuf));
	}

	if (end.y >= gdk_pixbuf_get_height (pixbuf))
	{
		g_assert (dst.height >= (end.y - gdk_pixbuf_get_height (pixbuf)));

		dst.height -= (end.y - gdk_pixbuf_get_height (pixbuf));
	}

	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		gdk_pixbuf_render_to_drawable_alpha ((GdkPixbuf *) pixbuf,
				       drawable,
				       src.x,
				       src.y,
				       dst.x,
				       dst.y,
				       dst.width,
				       dst.height,
				       GDK_PIXBUF_ALPHA_FULL,
				       128,
				       dither,
				       0,
				       0);

	} else {
		gdk_pixbuf_render_to_drawable ((GdkPixbuf *) pixbuf,
				       drawable,
				       gc,
				       src.x,
				       src.y,
				       dst.x,
				       dst.y,
				       dst.width,
				       dst.height,
				       dither,
				       0,
				       0);
	}
}

void
nautilus_gdk_pixbuf_render_to_pixbuf (const GdkPixbuf		*pixbuf,
				      GdkPixbuf			*destination_pixbuf,
				      const GdkPoint		*source_point,
				      const GdkRectangle	*destination_area)
{
	GdkPoint	src;
	GdkRectangle	dst;
	GdkPoint	end;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (source_point != NULL);
	g_return_if_fail (source_point->x >= 0);
	g_return_if_fail (source_point->y >= 0);
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (destination_area->width > 0);
	g_return_if_fail (destination_area->height > 0);

	src = *source_point;
	dst = *destination_area;

	/* Clip to the left edge of the drawable */
	if (dst.x < 0)
	{
		src.x += ABS (dst.x);
		dst.x = 0;
	}

	/* Clip to the top edge of the drawable */
	if (dst.y < 0)
	{
		src.y += ABS (dst.y);
		dst.y = 0;
	}
	
	end.x = src.x + dst.width;
	end.y = src.y + dst.height;
	
	if (end.x >= gdk_pixbuf_get_width (pixbuf))
	{
		g_assert (dst.width >= (end.x - gdk_pixbuf_get_width (pixbuf)));

		dst.width -= (end.x - gdk_pixbuf_get_width (pixbuf));
	}

	if (end.y >= gdk_pixbuf_get_height (pixbuf))
	{
		g_assert (dst.height >= (end.y - gdk_pixbuf_get_height (pixbuf)));

		dst.height -= (end.y - gdk_pixbuf_get_height (pixbuf));
	}

	gdk_pixbuf_copy_area ((GdkPixbuf *) pixbuf,
			      src.x,
			      src.y,
			      dst.width,
			      dst.height,
			      destination_pixbuf,
			      dst.x,
			      dst.y);
}

void
nautilus_gdk_pixbuf_render_to_pixbuf_alpha (const GdkPixbuf	*pixbuf,
					    GdkPixbuf		*destination_pixbuf,
					    const GdkRectangle	*destination_area,
					    GdkInterpType	interpolation_mode,
					    guchar		overall_alpha)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (destination_area->width > 0);
	g_return_if_fail (destination_area->height > 0);

	gdk_pixbuf_composite (pixbuf,
			      destination_pixbuf,
			      destination_area->x,
			      destination_area->y,
			      destination_area->width,
			      destination_area->height,
			      (double) destination_area->x,
			      (double) destination_area->y,
			      1.0,
			      1.0,
			      interpolation_mode,
			      overall_alpha);
}

/* utility routine for saving a pixbuf to a png file.
 * This was adapted from Iain Holmes' code in gnome-iconedit, and probably
 * should be in a utility library, possibly in gdk-pixbuf itself.
 */
gboolean
nautilus_gdk_pixbuf_save_to_file (GdkPixbuf                  *pixbuf,
				  const char                 *file_name)
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
