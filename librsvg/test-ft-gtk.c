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
#include <sys/time.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <freetype/freetype.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_alphagamma.h>
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
	GtkWidget *status;
	double start_time;
	gboolean do_drawing;
	gboolean do_scrolling;
	gboolean do_invert;
};

static double
timing_get_time (void)
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday (&tv, &tz);

	return tv.tv_sec + 1e-6 * tv.tv_usec;
}

static void invert_glyph (guchar *buf, int rowstride, int width, int height)
{
	int x, y;
	int first;
	int n_words;
	int last;
	guint32 *middle;

	if (width >= 8 && ((rowstride & 3) == 0)) {
		first = (-(long)buf) & 3;
		n_words = (width - first) >> 2;
		last = first + (n_words << 2);

		for (y = 0; y < height; y++) {
			middle = (guint32 *)(buf + first);
			for (x = 0; x < first; x++)
				buf[x] = ~buf[x];
			for (x = 0; x < n_words; x++)
				middle[x] = ~middle[x];
			for (x = last; x < width; x++)
				buf[x] = ~buf[x];
			buf += rowstride;
		}
	} else {
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++)
				buf[x] = ~buf[x];
			buf += rowstride;
		}
	}
}

static void draw_line (TestCtx *ctx, int line_num, ArtIRect *rect)
{
	GtkWidget *drawingarea = ctx->drawingarea;
	int y0;
	RsvgFTGlyph *glyph;
	const double affine[6] = { 1, 0, 0, 1, 5, 12 };
	int glyph_xy[2];
	ArtIRect line_rect, clear_rect, glyph_rect, draw_rect;
	int width;

	width = drawingarea->allocation.width;

	y0 = line_num * ctx->y_sp - ctx->y_scroll;
	if (line_num < 0 || line_num >= ctx->n_lines) {
		if (ctx->do_drawing) {
			gdk_draw_rectangle (drawingarea->window,
					    drawingarea->style->white_gc,
					    TRUE,
					    0, y0, width, ctx->y_sp);
		}
	} else {
		guchar *buf;
		int rowstride;

		glyph = rsvg_ft_render_string (ctx->ctx, ctx->fh,
					       ctx->lines[line_num],
					       strlen (ctx->lines[line_num]),
					       14, 14,
					       affine,
					       glyph_xy);
		rowstride = glyph->rowstride;

		glyph_rect.x0 = glyph_xy[0];
		glyph_rect.y0 = y0 + glyph_xy[1];
		glyph_rect.x1 = glyph_rect.x0 + glyph->width;
		glyph_rect.y1 = glyph_rect.y0 + glyph->height;
		line_rect.x0 = 0;
		line_rect.y0 = y0;
		line_rect.x1 = width;
		line_rect.y1 = y0 + ctx->y_sp;
		art_irect_intersect (&clear_rect, rect, &line_rect);

		if (ctx->do_drawing)
			gdk_draw_rectangle (drawingarea->window,
					    drawingarea->style->white_gc,
					    TRUE,
					    clear_rect.x0, clear_rect.y0,
					    clear_rect.x1 - clear_rect.x0,
					    clear_rect.y1 - clear_rect.y0);

		art_irect_intersect (&draw_rect, rect, &glyph_rect);
		if (!art_irect_empty (&draw_rect) && ctx->do_drawing) {
			buf = glyph->buf +
				draw_rect.x0 - glyph_rect.x0 +
				rowstride * (draw_rect.y0 - glyph_rect.y0);
			if (ctx->do_invert) {
				invert_glyph (buf, rowstride,
					      draw_rect.x1 - draw_rect.x0,
					      draw_rect.y1 - draw_rect.y0);
			}
			gdk_draw_gray_image (drawingarea->window,
					     drawingarea->style->white_gc,
					     draw_rect.x0, draw_rect.y0,
					     draw_rect.x1 - draw_rect.x0,
					     draw_rect.y1 - draw_rect.y0,
					     GDK_RGB_DITHER_NONE,
					     buf,
					     rowstride);
		}
		rsvg_ft_glyph_unref (glyph);
	}
}

static gint
test_expose (GtkWidget *widget, GdkEventExpose *event, TestCtx *ctx)
{
	int line0, line1;
	int line;
	ArtIRect rect;

	rect.x0 = event->area.x;
	rect.y0 = event->area.y;
	rect.x1 = rect.x0 + event->area.width;
	rect.y1 = rect.y0 + event->area.height;
	line0 = (rect.y0 + ctx->y_scroll) / ctx->y_sp;
	line1 = (rect.y1 + ctx->y_scroll + ctx->y_sp - 1) / ctx->y_sp;
	for (line = line0; line < line1; line++) {
#ifdef VERBOSE
		g_print ("drawing line %d of [%d..%d]\n", line, line0, line1 - 1);
#endif
		draw_line (ctx, line, &rect);
	}
	return FALSE;
}

static void
scroll_to (TestCtx *ctx, int new_y)
{
	GtkWidget *drawingarea = ctx->drawingarea;
	int scroll_amt = new_y - ctx->y_scroll;
	int width = drawingarea->allocation.width;
	int height = drawingarea->allocation.height;
	int y0, y1;
	GdkEventExpose expose;

	if (scroll_amt == 0)
		return;

#ifdef VERBOSE
	g_print ("scrolling to %d\n", new_y);
#endif
	if (scroll_amt > 0 && scroll_amt < height) {
		y0 = height - scroll_amt;
		y1 = height;
		if (ctx->do_scrolling) {
			gdk_draw_pixmap (drawingarea->window,
					 drawingarea->style->white_gc,
					 drawingarea->window,
					 0, scroll_amt,
					 0, 0,
					 width, y0);
		}
	} else if (scroll_amt < 0 && -scroll_amt < height) {
		y0 = 0;
		y1 = -scroll_amt;
		if (ctx->do_scrolling) {
			gdk_draw_pixmap (drawingarea->window,
					 drawingarea->style->white_gc,
					 drawingarea->window,
					 0, 0,
					 0, y1,
					 width, height - y1);
		}
	} else {
		y0 = 0;
		y1 = height;
	}
	ctx->y_scroll = new_y;
	expose.area.x = 0;
	expose.area.width = width;
	expose.area.y = y0;
	expose.area.height = y1 - y0;
	test_expose (drawingarea, &expose, ctx);
}

static gboolean scroll_idler (gpointer data)
{
	TestCtx *ctx = (TestCtx *)data;
	GtkWidget *drawingarea = ctx->drawingarea;
	int width = drawingarea->allocation.width;
	int height = drawingarea->allocation.height;

	if ((ctx->y_scroll + height) < ctx->n_lines * ctx->y_sp) {
		scroll_to (ctx, ctx->y_scroll + 100);
		return TRUE;
	} else {
		double elapsed;
		char str[128];

		scroll_to (ctx, 0);
		elapsed = timing_get_time () - ctx->start_time;
		sprintf (str, "%g seconds to scroll, %g Mpix/s",
			 elapsed,
			 width * ctx->y_sp * ctx->n_lines * 1e-6 / elapsed);
		gtk_label_set_text (GTK_LABEL (ctx->status), str);
		return FALSE;
	}
}

static void
check_toggle (GtkWidget *button, int state, gboolean *bool) {
	*bool = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
}

static GtkWidget *
check_button (const char *label, gboolean *bool) {
	GtkWidget *result;

	result = gtk_check_button_new_with_label (label);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (result), *bool);
	gtk_signal_connect (GTK_OBJECT (result), "state_changed",
			    (GtkSignalFunc) check_toggle, bool);
	return result;
}

static void
start_scrolling (GtkWidget *widget, TestCtx *ctx)
{
	scroll_to (ctx, 0);
	gtk_idle_add (scroll_idler, ctx);
	ctx->start_time = timing_get_time ();
	gtk_label_set_text (GTK_LABEL (ctx->status), "Scrolling...");
}

static void
test_ft_quit (GtkWidget *widget, TestCtx *ctx)
{
	rsvg_ft_ctx_done (ctx->ctx);
	gtk_main_quit ();
}

static TestCtx *new_test_window (const char *fn, const char *afn,
				 int width, int height)
{
	GtkWidget *topwin;
	GtkWidget *vbox;
	GtkWidget *buttonbar;
	GtkWidget *button;
	GtkWidget *drawingarea;
	TestCtx *ctx;

	ctx = g_new (TestCtx, 1);

	topwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (topwin), "destroy",
			    (GtkSignalFunc) test_ft_quit, ctx);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (topwin), vbox);

	drawingarea = gtk_drawing_area_new ();
	gtk_drawing_area_size (GTK_DRAWING_AREA (drawingarea), width, height);
	gtk_box_pack_start (GTK_BOX (vbox), drawingarea, TRUE, TRUE, 0);

	ctx->ctx = rsvg_ft_ctx_new ();
	ctx->fh = rsvg_ft_intern (ctx->ctx, fn);
	if (afn) rsvg_ft_font_attach (ctx->ctx, ctx->fh, afn);
	ctx->n_lines = 0;
	ctx->lines = NULL;
	ctx->y_sp = 16;
	ctx->y_scroll = 0;
	ctx->drawingarea = drawingarea;

	ctx->do_drawing = TRUE;
	ctx->do_scrolling = TRUE;
	ctx->do_invert = TRUE;

	gtk_signal_connect (GTK_OBJECT (drawingarea), "expose_event",
			    (GtkSignalFunc) test_expose, ctx);

	buttonbar = gtk_hbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (vbox), buttonbar, FALSE, FALSE, 0);

	button = check_button ("Do drawing", &ctx->do_drawing);
	gtk_container_add (GTK_CONTAINER (buttonbar), button);

	button = check_button ("Do scrolling", &ctx->do_scrolling);
	gtk_container_add (GTK_CONTAINER (buttonbar), button);

	button = check_button ("Do invert", &ctx->do_invert);
	gtk_container_add (GTK_CONTAINER (buttonbar), button);

	button = gtk_button_new_with_label ("Start scroll test");
	gtk_container_add (GTK_CONTAINER (buttonbar), button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    (GtkSignalFunc) start_scrolling, ctx);

	ctx->status = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (vbox), ctx->status, FALSE, FALSE, 2);

	gtk_widget_show_all (topwin);

	return ctx;
}

static void set_text (TestCtx *ctx, const char *fn) {
	FILE *f;
	char line[256];
	int n_lines;
	char **lines;

	lines = NULL;

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

		g_assert (lines != NULL);

		len = strlen (line);
	        if (len > 0 && line[len - 1] == '\n')
			line[--len] = 0;
	        while (len > 0 && line[len - 1] == '\r')
			line[--len] = 0;
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
	char	*add_font_file_name = NULL;
	char *text_file_name = "rsvg-ft.c";

	poptContext optCtx;
	struct poptOption optionsTable[] = 
	{
		{"zoom", 'z', POPT_ARG_STRING, &zoom_str, 0, NULL, "zoom factor"},
		{"font-width", 'w', POPT_ARG_INT, &font_width, 0, NULL, "Font Width"},
		{"font-height", 'h', POPT_ARG_INT, &font_height, 0, NULL, "Font Height"},
		{"font-file-name", 'f', POPT_ARG_STRING, &font_file_name, 0, NULL, "Font File Name"},
		{"add-font-file-name", 'a', POPT_ARG_STRING, &add_font_file_name, 0, NULL, "Additional Font File Name"},
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

	ctx = new_test_window (font_file_name, add_font_file_name, 640, 480);

	set_text (ctx, text_file_name);

	gtk_main ();

	return 0;
}
