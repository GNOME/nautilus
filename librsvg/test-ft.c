/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   test-ft.c: Testbed for freetype/libart integration.
 
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
#include <math.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <freetype/freetype.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_alphagamma.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_affine.h>
#include "art_render.h"
#include "art_render_mask.h"

#include "rsvg.h"
#include "rsvg-ft.h"

#if 0
typedef struct _ArtMaskSourceFT ArtMaskSourceFT;

struct _ArtMaskSourceFT {
	ArtMaskSource super;
	ArtRender *render;
	art_boolean first;
	const RsvgFTFont *font;
	const char *text;
};

void
art_render_freetype(ArtRender * render, const RsvgFTFont * font,
		    const char *text, double sx, double sy,
		    const double affine[6]);

static void
art_render_ft_done(ArtRenderCallback * self, ArtRender * render)
{
	art_free(self);
}

static int
art_render_ft_can_drive(ArtMaskSource * self, ArtRender * render)
{
	return 0;
}

static void
art_render_ft_render(ArtRenderCallback * self, ArtRender * render,
		     art_u8 * dest, int y)
{
}

static void
art_render_ft_prepare(ArtMaskSource *self, ArtRender *render,
		      art_boolean first)
{
	ArtMaskSourceFT *z = (ArtMaskSourceFT *) self;

	z->first = first;
	z->super.super.render = art_render_ft_render;
}

void
art_render_freetype(ArtRender * render, const RsvgFTFont *font,
		    const char *text, double sx, double sy,
		    const double affine[6])
{
	ArtMaskSourceFT *mask_source = art_new(ArtMaskSourceFT, 1);

	mask_source->super.super.render = NULL;
	mask_source->super.super.done = art_render_ft_done;
	mask_source->super.can_drive = art_render_ft_can_drive;
	mask_source->super.invoke_driver = NULL;
	mask_source->super.prepare = art_render_ft_prepare;
	mask_source->render = render;
	mask_source->font = font;
	mask_source->text = text;

	art_render_add_mask_source(render, &mask_source->super);
}
#endif

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

static void test_pixmap_destroy(guchar * pixels, gpointer data)
{
	g_free(pixels);
}

#if 0
/**
 * pixbuf_from_rsvg_ft_glyph: Create a GdkPixbuf from a glyph.
 * @glyph: The glyph to render as a pixbuf.
 * @rgb: Foreground color for rendering.
 * 
 * Renders a glyph as a transparent pixbuf, with foreground color @rgb.
 *
 * Return value: the resulting GdkPixbuf.
 **/
static GdkPixbuf *
pixbuf_from_rsvg_ft_glyph (RsvgFTGlyph *glyph, guint32 rgb)
{
	GdkPixbuf *pixbuf;
	int width = glyph->x1 - glyph->x0;
	int height = glyph->y1 - glyph->y0;
	int rowstride;
	art_u8 *pixels;
	int x, y;
	const guchar *src_line;
	art_u8 *dst_line;
	const art_u8 r = (rgb >> 16) & 0xff;
	const art_u8 g = (rgb >> 8) & 0xff;
	const art_u8 b = rgb & 0xff;

	fprintf (stderr, "xpen, ypen = (%f, %f)\n", glyph->xpen, glyph->ypen);

	rowstride = width << 2;
	pixels = g_new (art_u8, rowstride * height);
	src_line = glyph->buf;
	dst_line = pixels;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			dst_line[x * 4] = r;
			dst_line[x * 4 + 1] = g;
			dst_line[x * 4 + 2] = b;
			dst_line[x * 4 + 3] = src_line[x];
		}
		src_line += glyph->rowstride;
		dst_line += rowstride;
	}
	pixbuf = gdk_pixbuf_new_from_data (pixels,
					   GDK_COLORSPACE_RGB,
					   1, 8,
					   width, height, rowstride,
					   test_pixmap_destroy,
					   NULL);
	return pixbuf;
}
#endif

static GdkPixbuf *
glyph_render_test (RsvgFTGlyph *glyph, int glyph_xy[2]) {
	GdkPixbuf *pixbuf;
	art_u8 *pixels;
	int width;
	int height;
	int rowstride;
	ArtRender *render;
	ArtPixMaxDepth color[3] = {ART_PIX_MAX_FROM_8(0x80), 0, 0 };

	width = glyph->width;
	height = glyph->height;

	width = 200;
	height = 200;

	rowstride = width << 2;
	pixels = g_new (art_u8, rowstride * height);

	render = art_render_new (0, 0, width, height,
				 pixels, rowstride,
				 3, 8, ART_ALPHA_SEPARATE, NULL);
	art_render_image_solid (render, color);
	art_render_mask (render,
			 glyph_xy[0], glyph_xy[1],
			 glyph_xy[0] + glyph->width, glyph_xy[1] + glyph->height,
			 glyph->buf, glyph->rowstride);
	art_render_invoke (render);

	pixbuf = gdk_pixbuf_new_from_data (pixels,
					   GDK_COLORSPACE_RGB,
					   1, 8,
					   width, height, rowstride,
					   test_pixmap_destroy,
					   NULL);
	return pixbuf;
}

int main(int argc, char **argv)
{
	char *out_fn;
	GdkPixbuf *pixbuf;
	char *zoom_str = "1.0";
	int n_iter = 1;
	
 	gint	font_width = 36;
 	gint	font_height = 36;
	char	*font_file_name = "/usr/share/fonts/default/Type1/n021003l.pfb";

	poptContext optCtx;
	struct poptOption optionsTable[] = 
	{
		{"zoom", 'z', POPT_ARG_STRING, &zoom_str, 0, NULL, "zoom factor"},
		{"num-iter", 'n', POPT_ARG_INT, &n_iter, 0, NULL, "number of iterations"},
		{"font-width", 'w', POPT_ARG_INT, &font_width, 0, NULL, "Font Width"},
		{"font-height", 'h', POPT_ARG_INT, &font_height, 0, NULL, "Font Height"},
		{"font-file-name", 'f', POPT_ARG_STRING, &font_file_name, 0, NULL, "Font File Name"},
		POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
	};
	char c;
	const char *const *args;
	int i;

	RsvgFTCtx *ctx;
	RsvgFTFontHandle fh;

#if 1
	const double affine[6] = { .707, -.707, .707, .707, 10, 150 };
#else
 	double affine[6];
	art_affine_identity (affine);
#endif

	optCtx =
	    poptGetContext("test-ft", argc, (const char **) argv,
			   optionsTable, 0);

	c = poptGetNextOpt(optCtx);
	args = poptGetArgs(optCtx);

	if (args == NULL || args[0] == NULL)
		out_fn = "-";
	else
		out_fn = (char *) args[0];

	ctx = rsvg_ft_ctx_new ();
	fh = rsvg_ft_intern (ctx, font_file_name);

	for (i = 0; i < n_iter; i++) {
		RsvgFTGlyph *glyph;
		int glyph_xy[2];


		glyph = rsvg_ft_render_string (ctx, fh, 
					       "graphic(s)", 
					       strlen ("graphic(s)"),
					       font_width, 
					       font_height,
					       affine, 
					       glyph_xy);

/* 		fprintf (stderr, "xpen, ypen = (%f, %f)\n", glyph->xpen, glyph->ypen); */
/* 		fprintf (stderr, "glyph_xy = (%d, %d)\n", glyph_xy[0], glyph_xy[1]); */

 		glyph_xy[0] = 0; /* += glyph->xpen; */
 		glyph_xy[1] = 0; /* += glyph->ypen; */

		pixbuf = glyph_render_test (glyph, glyph_xy);

		if (pixbuf != NULL) {
			if (n_iter > 1)
				gdk_pixbuf_unref(pixbuf);
			else
				save_pixbuf_to_file(pixbuf, out_fn);
		} else {
			fprintf(stderr, "Error rendering.\n");
			return 1;
		}
	}

	return 0;
}
