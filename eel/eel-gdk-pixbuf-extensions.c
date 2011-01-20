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

