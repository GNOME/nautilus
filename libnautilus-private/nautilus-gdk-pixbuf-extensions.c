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
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-glib-extensions.h"

#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>

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
