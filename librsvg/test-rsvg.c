/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   test-rsvg.c: Command line utility for exercising rsvg.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Raph Levien <raph@artofcode.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <popt.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "rsvg.h"


/* The following routine is lifted wholesale from nautilus-icon-factory.c.
   It should find a permanent home somewhere else, at which point it should
   be deleted here and simply linked. -RLL
*/

/* utility routine for saving a pixbuf to a png file.
 * This was adapted from Iain Holmes' code in gnome-iconedit, and probably
 * should be in a utility library, possibly in gdk-pixbuf itself.
 * 
 * It is split up into save_pixbuf_to_file and save_pixbuf_to_file_internal
 * to work around a gcc warning about handle possibly getting clobbered by
 * longjmp. Declaring handle 'volatile FILE *' didn't work as it should have.
 */
static gboolean
save_pixbuf_to_file_internal (GdkPixbuf *pixbuf, char *filename, FILE *handle)
{
  	char *buffer;
	gboolean has_alpha;
	int width, height, depth, rowstride;
  	guchar *pixels;
  	png_structp png_ptr;
  	png_infop info_ptr;
  	png_text text[2];
  	int i;
	
	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		return FALSE;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct (&png_ptr, (png_infopp)NULL);
	    	return FALSE;
	}

	if (setjmp (png_ptr->jmpbuf)) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		return FALSE;
	}

	png_init_io (png_ptr, (FILE *)handle);

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
	text[0].text = filename;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = "Test-Rsvg";
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

	return TRUE;
}

static gboolean
save_pixbuf_to_file (GdkPixbuf *pixbuf, char *filename)
{
	FILE *handle;
	gboolean result;

	g_return_val_if_fail (pixbuf != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (filename[0] != '\0', FALSE);

	if (!strcmp (filename, "-")) {
		handle = stdout;
	} else {
		handle = fopen (filename, "wb");
	}

        if (handle == NULL) {
        	return FALSE;
	}

	result = save_pixbuf_to_file_internal (pixbuf, filename, handle);
	if (!result || handle != stdout)
		fclose (handle);

	return result;
}

int
main (int argc, char **argv)
{
	FILE *f;
	char *out_fn;
	GdkPixbuf *pixbuf;
	char *zoom_str = "1.0";
	int n_iter = 1;
	poptContext optCtx;
	struct poptOption optionsTable[] = {
		{ "zoom", 'z', POPT_ARG_STRING, &zoom_str, 0, NULL, "zoom factor" },
		{ "num-iter", 'n', POPT_ARG_INT, &n_iter, 0, NULL, "number of iterations" },
		POPT_AUTOHELP
		{ NULL, 0, 0, NULL, 0 }
	};
	char c;
	const char * const *args;
	int i;

	optCtx = poptGetContext ("test-rsvg", argc, (const char **)argv, optionsTable, 0);

	c = poptGetNextOpt (optCtx);
	args = poptGetArgs (optCtx);

	for (i = 0; i < n_iter; i++) {
		if (args == NULL || args[0] == NULL) {
			if (n_iter > 1) {
				fprintf (stderr, "Can't do multiple iterations on stdin\n");
				exit (1);
			}

			f = stdin;
			out_fn = "-";
		} else {
			f = fopen(args[0], "r");
			if (f == NULL) {
				fprintf(stderr, "Error opening source file %s\n", argv[0]);
			}
			if (args[1] == NULL)
				out_fn = "-";
			else
				out_fn = (char *)args[1];
		}

		pixbuf = rsvg_render_file (f, atof (zoom_str));

		if (f != stdin)
			fclose(f);

		if (pixbuf != NULL) {
			if (n_iter > 1)
				gdk_pixbuf_unref (pixbuf);
			else
				save_pixbuf_to_file (pixbuf, out_fn);
		} else {
			fprintf (stderr, "Error loading SVG file.\n");
			return 1;
		}
	}

	return 0;
}
