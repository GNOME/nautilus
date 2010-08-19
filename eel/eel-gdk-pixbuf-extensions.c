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

#include "eel-debug.h"
#include "eel-gdk-extensions.h"
#include "eel-glib-extensions.h"
#include "eel-graphic-effects.h"
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

static guchar
eel_gdk_pixbuf_lighten_pixbuf_component (guchar cur_value,
                                         guint lighten_value)
{
	int new_value = cur_value;
	if (lighten_value > 0) {
		new_value += lighten_value + (new_value >> 3);
		if (new_value > 255) {
			new_value = 255;
		}
	}
	return (guchar) new_value;
}

static GdkPixbuf *
eel_gdk_pixbuf_lighten (GdkPixbuf* src,
                        guint lighten_value)
{
	GdkPixbuf *dest;
	int i, j;
	int width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;

	g_assert (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB);
	g_assert ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4));
	g_assert (gdk_pixbuf_get_bits_per_sample (src) == 8);

	dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			       gdk_pixbuf_get_has_alpha (src),
			       gdk_pixbuf_get_bits_per_sample (src),
			       gdk_pixbuf_get_width (src),
			       gdk_pixbuf_get_height (src));
	
	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		for (j = 0; j < width; j++) {		
			*pixdest++ = eel_gdk_pixbuf_lighten_pixbuf_component (*pixsrc++, lighten_value);
			*pixdest++ = eel_gdk_pixbuf_lighten_pixbuf_component (*pixsrc++, lighten_value);
			*pixdest++ = eel_gdk_pixbuf_lighten_pixbuf_component (*pixsrc++, lighten_value);
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}

GdkPixbuf *
eel_gdk_pixbuf_render (GdkPixbuf *pixbuf,
                       guint render_mode,
                       guint saturation,
                       guint brightness,
                       guint lighten_value,
                       guint color)
{
 	GdkPixbuf *temp_pixbuf, *old_pixbuf;

	if (render_mode == 1) {
	/* lighten icon */
		temp_pixbuf = eel_create_spotlight_pixbuf (pixbuf);
	}
	else if (render_mode == 2) {
	/* colorize icon */
		temp_pixbuf = eel_create_colorized_pixbuf (pixbuf,
				   EEL_RGBA_COLOR_GET_R (color),
				   EEL_RGBA_COLOR_GET_G (color),
				   EEL_RGBA_COLOR_GET_B (color));
	} else if (render_mode == 3) {
	/* monochromely colorize icon */
		old_pixbuf = eel_create_darkened_pixbuf (pixbuf, 0, 255);		
		temp_pixbuf = eel_create_colorized_pixbuf (old_pixbuf,
				   EEL_RGBA_COLOR_GET_R (color),
				   EEL_RGBA_COLOR_GET_G (color),
				   EEL_RGBA_COLOR_GET_B (color));
		g_object_unref (old_pixbuf);
	} else {
		temp_pixbuf = NULL;
	}

	if (saturation < 255 || brightness < 255 || temp_pixbuf == NULL) { // temp_pixbuf == NULL just for safer code (return copy)
		old_pixbuf = temp_pixbuf;
		temp_pixbuf = eel_create_darkened_pixbuf (temp_pixbuf ? temp_pixbuf : pixbuf, saturation, brightness);
		if (old_pixbuf) {
			g_object_unref (old_pixbuf);
		}
	}

	if (lighten_value > 0) {
		old_pixbuf = temp_pixbuf;
  		temp_pixbuf = eel_gdk_pixbuf_lighten (temp_pixbuf ? temp_pixbuf : pixbuf, lighten_value);
		if (old_pixbuf) {
			g_object_unref (old_pixbuf);
		}
	}

	return temp_pixbuf;
}
