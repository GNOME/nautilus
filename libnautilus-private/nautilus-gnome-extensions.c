/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gnome-extensions.c - implementation of new functions that operate on
                                 gnome classes. Perhaps some of these should be
  			         rolled into gnome someday.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

#include <sys/stat.h>

#include "nautilus-gnome-extensions.h"

#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rect.h>
#include "nautilus-gdk-extensions.h"
#include <libgnome/gnome-util.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

void
nautilus_gnome_canvas_world_to_window_rectangle (GnomeCanvas *canvas,
						 const ArtDRect *world_rect,
						 ArtIRect *window_rect)
{
	double x0, y0, x1, y1;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (world_rect != NULL);
	g_return_if_fail (window_rect != NULL);

	gnome_canvas_world_to_window (canvas,
				      world_rect->x0,
				      world_rect->y0,
				      &x0, &y0);
	gnome_canvas_world_to_window (canvas,
				      world_rect->x1,
				      world_rect->y1,
				      &x1, &y1);

	window_rect->x0 = x0;
	window_rect->y0 = y0;
	window_rect->x1 = x1;
	window_rect->y1 = y1;
}

void
nautilus_gnome_canvas_world_to_canvas_rectangle (GnomeCanvas *canvas,
						 const ArtDRect *world_rect,
						 ArtIRect *canvas_rect)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (world_rect != NULL);
	g_return_if_fail (canvas_rect != NULL);

	gnome_canvas_w2c (canvas,
			  world_rect->x0,
			  world_rect->y0,
			  &canvas_rect->x0,
			  &canvas_rect->y0);
	gnome_canvas_w2c (canvas,
			  world_rect->x1,
			  world_rect->y1,
			  &canvas_rect->x1,
			  &canvas_rect->y1);
}

gboolean
nautilus_art_irect_contains_irect (const ArtIRect *outer_rect,
				   const ArtIRect *inner_rect)
{
	g_return_val_if_fail (outer_rect != NULL, FALSE);
	g_return_val_if_fail (inner_rect != NULL, FALSE);

	return outer_rect->x0 <= inner_rect->x0
		&& outer_rect->y0 <= inner_rect->y0
		&& outer_rect->x1 >= inner_rect->x1
		&& outer_rect->y1 >= inner_rect->y1; 
}

gboolean
nautilus_art_irect_hits_irect (const ArtIRect *rect_a,
			       const ArtIRect *rect_b)
{
	ArtIRect intersection;

	g_return_val_if_fail (rect_a != NULL, FALSE);
	g_return_val_if_fail (rect_b != NULL, FALSE);

	art_irect_intersect (&intersection, rect_a, rect_b);
	return !art_irect_empty (&intersection);
}

gboolean
nautilus_art_irect_equal (const ArtIRect *rect_a,
			  const ArtIRect *rect_b)
{
	g_return_val_if_fail (rect_a != NULL, FALSE);
	g_return_val_if_fail (rect_b != NULL, FALSE);

	return rect_a->x0 == rect_b->x0
		&& rect_a->y0 == rect_b->y0
		&& rect_a->x1 == rect_b->x1
		&& rect_a->y1 == rect_b->y1;
}

gboolean
nautilus_art_drect_equal (const ArtDRect *rect_a,
			  const ArtDRect *rect_b)
{
	g_return_val_if_fail (rect_a != NULL, FALSE);
	g_return_val_if_fail (rect_b != NULL, FALSE);

	return rect_a->x0 == rect_b->x0
		&& rect_a->y0 == rect_b->y0
		&& rect_a->x1 == rect_b->x1
		&& rect_a->y1 == rect_b->y1;
}

void
nautilus_gnome_canvas_item_get_current_canvas_bounds (GnomeCanvasItem *item,
						      ArtIRect *bounds)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (bounds != NULL);

	bounds->x0 = item->x1;
	bounds->y0 = item->y1;
	bounds->x1 = item->x2;
	bounds->y1 = item->y2;
}

void
nautilus_gnome_canvas_item_request_redraw (GnomeCanvasItem *item)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	gnome_canvas_request_redraw (item->canvas,
				     item->x1, item->y1,
				     item->x2, item->y2);
}

void
nautilus_gnome_canvas_request_redraw_rectangle (GnomeCanvas *canvas,
						const ArtIRect *canvas_rectangle)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	gnome_canvas_request_redraw (canvas,
				     canvas_rectangle->x0, canvas_rectangle->y0,
				     canvas_rectangle->x1, canvas_rectangle->y1);
}

void
nautilus_gnome_canvas_item_get_world_bounds (GnomeCanvasItem *item,
					     ArtDRect *world_bounds)
{
	gnome_canvas_item_get_bounds (item,
				      &world_bounds->x0,
				      &world_bounds->y0,
				      &world_bounds->x1,
				      &world_bounds->y1);
	if (item->parent != NULL) {
		gnome_canvas_item_i2w (item->parent,
				       &world_bounds->x0,
				       &world_bounds->y0);
		gnome_canvas_item_i2w (item->parent,
				       &world_bounds->x1,
				       &world_bounds->y1);
	}
}

/**
 * nautilus_gnome_canvas_fill_with_gradient, for the anti-aliased canvas:
 * @buffer: canvas buffer to draw into.
 * @full_rect: rectangle of entire canvas for gradient color selection
 * @start_color: Color for the left or top; pixel value does not matter.
 * @end_color: Color for the right or bottom; pixel value does not matter.
 * @horizontal: TRUE if the color changes from left to right. FALSE if from top to bottom.
 *
 * Fill the rectangle with a gradient.
 * The color changes from start_color to end_color.
 * This effect works best on true color displays.
 *
 * note that most of this routine is a clone of nautilus_fill_rectangle_with_gradient
 * from nautilus-gdk-extensions.
 */

#define GRADIENT_BAND_SIZE 4

void
nautilus_gnome_canvas_fill_with_gradient (GnomeCanvasBuf *buffer,
					  int entire_width, int entire_height,
					  guint32 start_rgb,
					  guint32 end_rgb,
					  gboolean horizontal)
{
	GdkRectangle band_box;
	guchar *bufptr;
	gint16 *position;
	guint16 *size;
	gint num_bands;
	guint16 last_band_size;
	gdouble fraction;
	gint y, band;
	gint red_value, green_value, blue_value;
	guint32 band_rgb;

	g_return_if_fail (horizontal == FALSE || horizontal == TRUE);

	if (entire_width == 0 || entire_height == 0) {
		return;
	}

	/* Set up the band box so we can access it the same way for horizontal or vertical. */
	band_box.x = buffer->rect.x0;
	band_box.y = buffer->rect.y0;
	band_box.width = buffer->rect.x1 - buffer->rect.x0;
	band_box.height = buffer->rect.y1 - buffer->rect.y0;

	position = horizontal ? &band_box.x : &band_box.y;
	size = horizontal ? &band_box.width : &band_box.height;

	/* Figure out how many bands we will need. */
	num_bands = (*size + GRADIENT_BAND_SIZE - 1) / GRADIENT_BAND_SIZE;
	last_band_size = GRADIENT_BAND_SIZE - (GRADIENT_BAND_SIZE * num_bands - *size);

	/* Change the band box to be the size of a single band. */
	*size = GRADIENT_BAND_SIZE;
	
	/* Fill each band with a separate nautilus_draw_rectangle call. */
	for (band = 0; band < num_bands; band++) {
		/* Compute a new color value for each band. */
		
		if (horizontal) {
			fraction = (double) *position / (double) entire_width;
		} else {
			fraction = (double) *position / (double) entire_height;
		}
		if (fraction > 1.0) {
			fraction = 1.0;
		} else if (fraction < 0.0) {
			fraction = 0.0;
		}
							
		band_rgb = nautilus_interpolate_color (fraction, start_rgb, end_rgb);
		red_value = band_rgb >> 16;
		green_value = (band_rgb >> 8) & 0xff;
		blue_value = band_rgb & 0xff;

		/* Last band may need to be a bit smaller to avoid writing outside the box.
		 * This is more efficient than changing and restoring the clip.
		 */
		if (band == num_bands - 1) {
			*size = last_band_size;
		}
		
		/* use libart to fill the band rectangle with the color */
		if (!horizontal)
			bufptr = buffer->buf + (buffer->buf_rowstride * band * GRADIENT_BAND_SIZE);
		else
			bufptr = buffer->buf + (3 * band * GRADIENT_BAND_SIZE);
					
		for (y = band_box.y; y < (band_box.y + band_box.height); y++) {
			art_rgb_fill_run(bufptr,
					 red_value,
					 green_value,
					 blue_value,
					 band_box.width);
			bufptr += buffer->buf_rowstride; 
		}
	
		*position += *size;
	}
}

GtkButton *
nautilus_gnome_dialog_get_button_by_index (GnomeDialog *dialog, int index)
{
	gpointer data;

	g_return_val_if_fail (GNOME_IS_DIALOG (dialog), NULL);
	g_return_val_if_fail (index >= 0, NULL);

	data = g_list_nth_data (GNOME_DIALOG (dialog)->buttons, index);
	if (data == NULL) {
		return NULL;
	}

	return GTK_BUTTON (data);
}

void
nautilus_gnome_canvas_item_request_update_deep (GnomeCanvasItem *item)
{
	GList *p;

	gnome_canvas_item_request_update (item);
	if (GNOME_IS_CANVAS_GROUP (item)) {
		for (p = GNOME_CANVAS_GROUP (item)->item_list; p != NULL; p = p->next) {
			nautilus_gnome_canvas_item_request_update_deep (p->data);
		}
	}
}

void
nautilus_gnome_canvas_request_update_all (GnomeCanvas *canvas)
{
	nautilus_gnome_canvas_item_request_update_deep (canvas->root);
}

/* The gnome_canvas_set_scroll_region function doesn't do an update,
 * even though it should. The update is in there with an #if 0 around
 * it, with no explanation of why it's commented out. For now, work
 * around this by requesting an update explicitly.
 */
void
nautilus_gnome_canvas_set_scroll_region (GnomeCanvas *canvas,
					 double x1, double y1,
					 double x2, double y2)
{
	double old_x1, old_y1, old_x2, old_y2;

	/* Change the scroll region and do an update if it changes. */
	gnome_canvas_get_scroll_region (canvas, &old_x1, &old_y1, &old_x2, &old_y2);
	if (old_x1 != x1 || old_y1 != y1 || old_x2 != x2 || old_y2 != y2) {
		gnome_canvas_set_scroll_region (canvas, x1, y1, x2, y2);
		nautilus_gnome_canvas_request_update_all (canvas);
		gnome_canvas_item_request_update (canvas->root);
	}
}

/* The code in GnomeCanvas (the scroll_to function to be exact) always
 * centers the contents of the canvas if the contents are smaller than
 * the canvas, and it does some questionable math when computing
 * that. This code is working to undo that mistake.
 */
void
nautilus_gnome_canvas_set_scroll_region_left_justify (GnomeCanvas *canvas,
						      double x1, double y1,
						      double x2, double y2)
{
	double height, width;

	/* To work around the logic in scroll_to that centers the
	 * canvas contents if they are smaller than the canvas widget,
	 * we must do the exact opposite of what it does. The -1 here
	 * is due to the ill-conceived ++ in scroll_to.
	 */
	width = (GTK_WIDGET (canvas)->allocation.width - 1) / canvas->pixels_per_unit;
	height = (GTK_WIDGET (canvas)->allocation.height - 1) / canvas->pixels_per_unit;
	nautilus_gnome_canvas_set_scroll_region
		(canvas, x1, y1,
		 MAX (x2, x1 + width), MAX (y2, y1 + height));
}


/* Code from GMC, contains all the voodoo needed to start
 * a terminal from the file manager nicely
 */

static int
max_open_files (void)
{
	static int files;

	if (files != 0) {
		return files;
	}

#ifdef HAVE_SYSCONF
	files = sysconf (_SC_OPEN_MAX);
	if (files != -1)
		return files;
#endif
#ifdef OPEN_MAX
	files = OPEN_MAX;
#else
	files = 256;
#endif
	return files;
}

static int 
nautilus_gnome_terminal_shell_execute (const char *shell, const char *command)
{
	struct sigaction ignore, save_intr, save_quit, save_stop;
	int status, i;
	int pid;
	
	ignore.sa_handler = SIG_IGN;
	sigemptyset (&ignore.sa_mask);
	ignore.sa_flags = 0;
	status = 0;
    
	sigaction (SIGINT, &ignore, &save_intr);    
	sigaction (SIGQUIT, &ignore, &save_quit);

	pid = fork ();
	if (pid < 0){
		return -1;
	}
	
	if (pid == 0){
		int top;
		struct sigaction default_pipe;

		top = max_open_files ();
		sigaction (SIGINT,  &save_intr, NULL);
		sigaction (SIGQUIT, &save_quit, NULL);

		/*
		 * reset sigpipe
		 */
		default_pipe.sa_handler = SIG_DFL;
		sigemptyset (&default_pipe.sa_mask);
		default_pipe.sa_flags = 0;
		
		sigaction (SIGPIPE, &default_pipe, NULL);
		
		for (i = 0; i < top; i++)
			close (i);

		/* Setup the file descriptor for the child */
		   
		/* stdin */
		open ("/dev/null", O_APPEND);

		/* stdout */
		open ("/dev/null", O_RDONLY);

		/* stderr */
		open ("/dev/null", O_RDONLY);
		
		pid = fork ();
		if (pid == 0){
			execl (shell, shell, "-c", command, (char *) 0);
			/* See note below for why we use _exit () */
			_exit (127);		/* Exec error */
		}
		/* We need to use _exit instead of exit to avoid
		 * calling the atexit handlers (specifically the gdk atexit
		 * handler
		 */
		_exit (0);
	}
	waitpid (pid, &status, 0);
	sigaction (SIGINT,  &save_intr, NULL);
	sigaction (SIGQUIT, &save_quit, NULL);
	sigaction (SIGTSTP, &save_stop, NULL);

	return WEXITSTATUS(status);
}

void
nautilus_gnome_open_terminal (const char *command)
{
	char *terminal_path;
	char *shell;
	gboolean quote_all;
	char *command_line;


	quote_all = FALSE;

	/* figure out whichever shell we are using */
    	shell = gnome_util_user_shell ();

	/* Look up a well-known terminal app */
	terminal_path = gnome_is_program_in_path ("gnome-terminal");
	if (terminal_path != NULL) {
		/* apparently gnome-terminal needs it's input nicely quoted and the other
		 * terminals don't
		 */
		quote_all = TRUE;
	}
	
	if (terminal_path == NULL) {
		terminal_path = gnome_is_program_in_path ("dtterm");
	}
	
	if (terminal_path == NULL) {
		terminal_path = gnome_is_program_in_path ("nxterm");
	}
	
	if (terminal_path == NULL) {
		terminal_path = gnome_is_program_in_path ("dtterm");
	}
	
	if (terminal_path == NULL) {
		terminal_path = gnome_is_program_in_path ("color-xterm");
	}
	
	if (terminal_path == NULL) {
		terminal_path = gnome_is_program_in_path ("rxvt");
	}
	
	if (terminal_path == NULL) {
		terminal_path = gnome_is_program_in_path ("xterm");
	}
	

	if (terminal_path == NULL){
		g_message (" Could not start a terminal ");
	} else if (command){
		if (quote_all) {
			command_line = g_strconcat (terminal_path, " -e '", command, "'", NULL);
		} else {
			command_line = g_strconcat (terminal_path, " -e ", command, NULL);
		}
		nautilus_gnome_terminal_shell_execute (shell, command_line);
		g_free (command_line);
	} else {
		nautilus_gnome_terminal_shell_execute (shell, terminal_path);
	}


	g_free (shell);
	g_free (terminal_path);
}
