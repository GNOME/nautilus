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
*/

#include <config.h>
#include <math.h>
#include "nautilus-file-utilities.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-string.h"

#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>

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
	char *local_path;
	GnomeVFSFileSize bytes_read;
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;	

	g_return_val_if_fail (uri != NULL, NULL);

	/* FIXME #1964: unfortunately, there are bugs in the gdk_pixbuf_loader
	   stuff that make it not work for various image types like .xpms.
	   Since gdk_pixbuf_new_from_file uses different, bug-free code,
	   we call that when the file is local.  This should be fixed
	   (in gdk_pixbuf) eventually, then this hack can be removed */
	   
	local_path = nautilus_get_local_path_from_uri (uri);
	if (local_path != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (local_path);
		g_free (local_path);
		return pixbuf;
	}
	
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

	if (result != GNOME_VFS_OK) {
		gtk_object_unref (GTK_OBJECT (loader));
		gnome_vfs_close (handle);
		return NULL;
	}

	gnome_vfs_close (handle);

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

	if (result != GNOME_VFS_OK) {
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

void
nautilus_gdk_pixbuf_render_to_drawable_tiled (GdkPixbuf *pixbuf,
					      GdkDrawable *drawable,
					      GdkGC *gc,
					      const GdkRectangle *rect,
					      GdkRgbDither dither,
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

	tile_width = gdk_pixbuf_get_width (pixbuf);
	tile_height = gdk_pixbuf_get_height (pixbuf);

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
			
			gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
						       tile_x, tile_y,
						       blit_x, blit_y, blit_width, blit_height,
						       dither, x_dither, y_dither);
		}
	}
}

/* return the average value of each component */
void 
nautilus_gdk_pixbuf_average_value (GdkPixbuf *pixbuf, GdkColor *color)
{
	int red_total, green_total, blue_total, count;
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
		pixsrc = original_pixels + row * row_stride;
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

/* scale the passed in pixbuf to conform to the passed-in maximum width and height */
/* utility routine to scale the passed-in pixbuf to be smaller than the maximum allowed size, if necessary */
GdkPixbuf *
nautilus_gdk_pixbuf_scale_to_fit (GdkPixbuf *pixbuf, int max_width, int max_height)
{
	double scale_factor;
	double h_scale = 1.0;
	double v_scale = 1.0;

	int width  = gdk_pixbuf_get_width(pixbuf);
	int height = gdk_pixbuf_get_height(pixbuf);
	
	if (width > max_width) {
		h_scale = max_width / (double) width;
	}
	if (height > max_height) {
		v_scale = max_height  / (double) height;
	}
	scale_factor = MIN (h_scale, v_scale);
	
	if (scale_factor < 1.0) {
		GdkPixbuf *scaled_pixbuf;
		/* the width and scale factor are always > 0, so it's OK to round by adding here */
		int scaled_width  = floor(width * scale_factor + .5);
		int scaled_height = floor(height * scale_factor + .5);
				
		scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_BILINEAR);	
		gdk_pixbuf_unref (pixbuf);
		pixbuf = scaled_pixbuf;
	}
	
	return pixbuf;
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
	
	pixmap = gdk_pixmap_new (NULL, width, height, visual->depth);
	gc = gdk_gc_new (pixmap);

	/* Set up a white background. */
	gdk_rgb_gc_set_foreground (gc, NAUTILUS_RGB_COLOR_WHITE);
	gdk_draw_rectangle (pixmap, gc, TRUE, 0, 0, width, height);
	
	/* Draw black text. */
	gdk_rgb_gc_set_foreground (gc, NAUTILUS_RGB_COLOR_BLACK);
	gdk_gc_set_font (gc, (GdkFont *) font);
	line = text;

	/* FIXME bugzilla.eazel.com xxxx:
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

/* The following function is a carbon copy of the above except it draws 
 * white text.  This is a hack workaround for a demo.  The correct fix is
 * to make the above function work with an arbitrary text color as an rgb 
 * value.  This work is happening on the HEAD of nautilus.
 */
void
nautilus_gdk_pixbuf_draw_text_white (GdkPixbuf		*pixbuf,
				     const GdkFont	*font,
				     const ArtIRect	*destination_rect,
				     const char		*text,
				     guint		overall_alpha) {

	ArtIRect	pixbuf_rect;
	ArtIRect	text_rect;
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
	width = text_rect.x1 - text_rect.x0;
	height = text_rect.y1 - text_rect.y0;
	pixmap = gdk_pixmap_new (NULL, width, height, visual->depth);
	gc = gdk_gc_new (pixmap);

	/* Set up a white background. */
	gdk_rgb_gc_set_background (gc, NAUTILUS_RGB_COLOR_BLACK);
	gdk_draw_rectangle (pixmap, gc, TRUE, 0, 0, width, height);

	/* Draw black text. */
	gdk_rgb_gc_set_foreground (gc, NAUTILUS_RGB_COLOR_WHITE);
	gdk_gc_set_font (gc, (GdkFont *) font);
	line = text;

	/* FIXME bugzilla.eazel.com xxxx:
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

	gdk_pixbuf_composite (text_pixbuf_with_alpha,
			      pixbuf,
			      text_rect.x0,
			      text_rect.y0,
			      width, height,
			      text_rect.x0,
			      text_rect.y0,
			      1, 1,
			      GDK_INTERP_BILINEAR,
			      overall_alpha);

	gdk_pixbuf_unref (text_pixbuf_with_alpha);
}

