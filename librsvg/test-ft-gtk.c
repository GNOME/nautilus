/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   test-ft-gtk.c: Testbed for freetype/libart integration.
 
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

#include <gtk/gtk.h>

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

typedef struct _TestCtx TestCtx;

struct _TestCtx {
	RsvgFTCtx *ctx;
	RsvgFTFontHandle fh;
	int n_lines;
	char **lines;
	int y_sp;
	int y_scroll;
	GtkWidget *drawingarea;
	int width;
	int height;
};

static void invert_glyph (guchar *buf, int rowstride, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y++) {
		int n_words = (width + 3) >> 2;
		for (x = 0; x < n_words; x++)
			((guint32 *)buf)[x] = ~((guint32 *)buf)[x];
		buf += rowstride;
	}
}

static void draw_line (TestCtx *ctx, int line_num)
{
	GtkWidget *drawingarea = ctx->drawingarea;
	int y0;
	RsvgFTGlyph *glyph;
	const double affine[6] = { 1, 0, 0, 1, 5, 12 };
	int glyph_xy[2];

	y0 = line_num * ctx->y_sp - ctx->y_scroll;
	if (line_num < 0 || line_num >= ctx->n_lines) {
		gdk_draw_rectangle (drawingarea->window,
				    drawingarea->style->white_gc,
				    TRUE,
				    0, y0, ctx->width, ctx->y_sp);
	} else {
		int draw_x0, draw_x1, draw_y0, draw_y1;
		guchar *buf;
		int rowstride;

		glyph = rsvg_ft_render_string (ctx->ctx, ctx->fh,
					       ctx->lines[line_num],
					       14, 14,
					       affine,
					       glyph_xy);
		rowstride = glyph->rowstride;
		buf = glyph->buf;

		draw_x0 = glyph_xy[0];
		draw_y0 = y0 + glyph_xy[1];
		draw_x1 = draw_x0 + glyph->width;
		draw_y1 = draw_y0 + glyph->height;
		g_print ("(%d, %d) - (%d, %d)\n",
			 draw_x0, draw_y0, draw_x1, draw_y1);
		gdk_draw_rectangle (drawingarea->window,
				    drawingarea->style->white_gc,
				    TRUE,
				    0, y0, ctx->width, ctx->y_sp);
#if 0
		gdk_draw_rectangle (drawingarea->window,
				    drawingarea->style->black_gc,
				    TRUE,
				    draw_x0, draw_y0,
				    draw_x1 - draw_x0, draw_y1 - draw_y0);
#endif
		invert_glyph (buf, rowstride,
			      draw_x1 - draw_x0, draw_y1 - draw_y0);
		gdk_draw_gray_image (drawingarea->window,
				     drawingarea->style->white_gc,
				     draw_x0, draw_y0,
				     draw_x1 - draw_x0, draw_y1 - draw_y0,
				     GDK_RGB_DITHER_NONE,
				     buf,
				     rowstride);
		rsvg_ft_glyph_unref (glyph);
	}
}

static gint
test_expose (GtkWidget *widget, GdkEventExpose *event, TestCtx *ctx)
{
	/* todo: figure out which lines to redraw based on expose area */
	draw_line (ctx, 2);
	return FALSE;
}

static TestCtx *new_test_window (const char *fn, int width, int height)
{
	GtkWidget *topwin;
	GtkWidget *vbox;
	GtkWidget *drawingarea;
	TestCtx *ctx;

	ctx = g_new (TestCtx, 1);

	topwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (topwin), "destroy",
			    (GtkSignalFunc) gtk_main_quit, NULL);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (topwin), vbox);

	drawingarea = gtk_drawing_area_new ();
	gtk_drawing_area_size (GTK_DRAWING_AREA (drawingarea), width, height);
	gtk_container_add (GTK_CONTAINER (vbox), drawingarea);

	ctx->ctx = rsvg_ft_ctx_new ();
	ctx->fh = rsvg_ft_intern (ctx->ctx, fn);
	ctx->n_lines = 0;
	ctx->lines = NULL;
	ctx->y_sp = 16;
	ctx->y_scroll = 0;
	ctx->drawingarea = drawingarea;
	ctx->width = width;
	ctx->height = height;

	gtk_signal_connect (GTK_OBJECT (drawingarea), "expose_event",
			    (GtkSignalFunc) test_expose, ctx);

	gtk_widget_show_all (topwin);

	return ctx;
}

static void set_text (TestCtx *ctx, const char *fn) {
	FILE *f;
	char line[256];
	int n_lines;
	char **lines;

	f = fopen (fn, "r");
	if (f == NULL) {
		g_warning ("Error opening file %s\n", fn);
		return;
	}
	n_lines = 0;
	for (;;) {
		int len;

		if (fgets (line, sizeof(line), f) == NULL)
			break;
		if (n_lines == 0)
			lines = g_new (char *, 1);
		else if (!(n_lines & (n_lines - 1))) {
			lines = g_renew (char *, lines, n_lines << 1);
		}
		len = strlen (line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = 0;
		lines[n_lines++] = g_strdup (line);
	}
	fclose (f);
	ctx->n_lines = n_lines;
	ctx->lines = lines;
}

int main(int argc, char **argv)
{
	char *zoom_str = "1.0";
	
 	gint	font_width = 36;
 	gint	font_height = 36;
	char	*font_file_name = "/usr/share/fonts/default/Type1/n021003l.pfb";
	char *text_file_name = "rsvg-ft.c";

	poptContext optCtx;
	struct poptOption optionsTable[] = 
	{
		{"zoom", 'z', POPT_ARG_STRING, &zoom_str, 0, NULL, "zoom factor"},
		{"font-width", 'w', POPT_ARG_INT, &font_width, 0, NULL, "Font Width"},
		{"font-height", 'h', POPT_ARG_INT, &font_height, 0, NULL, "Font Height"},
		{"font-file-name", 'f', POPT_ARG_STRING, &font_file_name, 0, NULL, "Font File Name"},
		{"text-file-name", 't', POPT_ARG_STRING, &text_file_name, 0, NULL, "Text"},
		POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
	};
	char c;
	const char *const *args;
	TestCtx *ctx;

	gtk_init (&argc, &argv);

	gdk_rgb_init ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	optCtx =
	    poptGetContext("test-ft", argc, (const char **) argv,
			   optionsTable, 0);

	c = poptGetNextOpt(optCtx);
	args = poptGetArgs(optCtx);

	ctx = new_test_window (font_file_name, 640, 480);

	set_text (ctx, text_file_name);

	gtk_main ();

	return 0;
}
