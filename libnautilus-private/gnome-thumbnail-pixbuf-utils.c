/*
 * gnome-thumbnail-pixbuf-utils.c: Utilities for handling pixbufs when thumbnailing
 *
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include "gnome-thumbnail.h"

#define LOAD_BUFFER_SIZE 65536

GdkPixbuf * _gnome_thumbnail_load_scaled_jpeg (const char *uri,
					       int         target_width,
					       int         target_height);


GdkPixbuf *
gnome_thumbnail_load_pixbuf (const char *uri)
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
					      bytes_read,
					      NULL)) {
			result = GNOME_VFS_ERROR_WRONG_FORMAT;
			break;
		}
	}

	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		gdk_pixbuf_loader_close (loader, NULL);
		g_object_unref (loader);
		gnome_vfs_close (handle);
		return NULL;
	}

	gnome_vfs_close (handle);
	gdk_pixbuf_loader_close (loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf != NULL) {
		g_object_ref (pixbuf);
	}
	g_object_unref (loader);

	return pixbuf;
}

GdkPixbuf *
gnome_thumbnail_scale_down_pixbuf (GdkPixbuf *pixbuf,
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



#ifdef HAVE_LIBJPEG

#include <setjmp.h>

#include <stdio.h>

/* Workaround broken libjpeg defining these that may
 * collide w/ the ones in config.h
 */
#undef HAVE_STDDEF_H
#undef HAVE_STDLIB_H
#include <jpeglib.h>

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#define BUFFER_SIZE 16384

typedef struct {
	struct jpeg_source_mgr pub;	/* public fields */
	GnomeVFSHandle *handle;
	JOCTET buffer[BUFFER_SIZE];
} Source;

typedef struct {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
} ErrorHandlerData;

static void
fatal_error_handler (j_common_ptr cinfo)
{
	ErrorHandlerData *data;

	data = (ErrorHandlerData *) cinfo->err;
	longjmp (data->setjmp_buffer, 1);
}

static void
output_message_handler (j_common_ptr cinfo)
{
	/* If we don't supply this handler, libjpeg reports errors
	 * directly to stderr.
	 */
}

static void
init_source (j_decompress_ptr cinfo)
{
}

static gboolean
fill_input_buffer (j_decompress_ptr cinfo)
{
	Source *src;
	GnomeVFSFileSize nbytes;
	GnomeVFSResult result;
	
	src = (Source *) cinfo->src;
	result = gnome_vfs_read (src->handle,
				 src->buffer,
				 G_N_ELEMENTS (src->buffer),
				 &nbytes);
	
	if (result != GNOME_VFS_OK || nbytes == 0) {
		/* return a fake EOI marker so we will eventually terminate */
		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		nbytes = 2;
	}
	
	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	
	return TRUE;
}

static void
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	Source *src;

	src = (Source *) cinfo->src;
	if (num_bytes > 0) {
		while (num_bytes > (long) src->pub.bytes_in_buffer) {
			num_bytes -= (long) src->pub.bytes_in_buffer;
			fill_input_buffer (cinfo);
		}
		src->pub.next_input_byte += (size_t) num_bytes;
		src->pub.bytes_in_buffer -= (size_t) num_bytes;
	}
}

static void
term_source (j_decompress_ptr cinfo)
{
}

static void
vfs_src (j_decompress_ptr cinfo, GnomeVFSHandle *handle)
{
	Source *src;
	
	if (cinfo->src == NULL) {	/* first time for this JPEG object? */
		cinfo->src = &(g_new (Source, 1))->pub;
	}

	src = (Source *) cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
	src->pub.term_source = term_source;
	src->handle = handle;
	src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
	src->pub.next_input_byte = NULL; /* until buffer loaded */
}

static int
calculate_divisor (int width,
		   int height,
		   int target_width,
		   int target_height)
{
	if (width/8 > target_width && height/8 > target_height) {
		return 8;
	}
	if (width/4 > target_width && height/4 > target_height) {
		return 4;
	}
	if (width/2 > target_width && height/2 > target_height) {
		return 2;
	}
	return 1;
}

static void
free_buffer (guchar *pixels, gpointer data)
{
	g_free (pixels);
}

GdkPixbuf *
_gnome_thumbnail_load_scaled_jpeg (const char *uri,
				   int         target_width,
				   int         target_height)
{
	struct jpeg_decompress_struct cinfo;
	ErrorHandlerData jerr;
	GnomeVFSHandle *handle;
	unsigned char *lines[1];
	guchar * volatile buffer;
	guchar * volatile pixels;
	guchar *ptr;
	GnomeVFSResult result;
	unsigned int i;
	
	result = gnome_vfs_open (&handle,
				 uri,
				 GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		return NULL;
	}
	
	cinfo.err = jpeg_std_error (&jerr.pub);
	jerr.pub.error_exit = fatal_error_handler;
	jerr.pub.output_message = output_message_handler;

	buffer = NULL;
	pixels = NULL;
	if (setjmp (jerr.setjmp_buffer)) {
		/* Handle a JPEG error. */
		jpeg_destroy_decompress (&cinfo);
		gnome_vfs_close (handle);
		g_free (buffer);
		g_free (pixels);
		return NULL;
	}

	jpeg_create_decompress (&cinfo);

	vfs_src (&cinfo, handle);

	jpeg_read_header (&cinfo, TRUE);

	cinfo.scale_num = 1;
	cinfo.scale_denom = calculate_divisor (cinfo.image_width,
					       cinfo.image_height,
					       target_width,
					       target_height);
	cinfo.dct_method = JDCT_FASTEST;
	cinfo.do_fancy_upsampling = FALSE;
	
	jpeg_start_decompress (&cinfo);

	pixels = g_malloc (cinfo.output_width *	cinfo.output_height * 3);

	ptr = pixels;
	if (cinfo.num_components == 1) {
		/* Allocate extra buffer for grayscale data */
		buffer = g_malloc (cinfo.output_width);
		lines[0] = buffer;
	} else {
		lines[0] = pixels;
	}
	
	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines (&cinfo, lines, 1);
		
		if (cinfo.num_components == 1) {
			/* Convert grayscale to rgb */
			for (i = 0; i < cinfo.output_width; i++) {
				ptr[i*3] = buffer[i];
				ptr[i*3+1] = buffer[i];
				ptr[i*3+2] = buffer[i];
			}
			ptr += cinfo.output_width * 3;
		} else {
			lines[0] += cinfo.output_width * 3;
		}
	}

	g_free (buffer);
	buffer = NULL;
	
	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);
	
	gnome_vfs_close (handle);
	
	return gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, FALSE, 8,
					 cinfo.output_width,
					 cinfo.output_height,
					 cinfo.output_width * 3,
					 free_buffer, NULL);
}

#endif /* HAVE_LIBJPEG */

