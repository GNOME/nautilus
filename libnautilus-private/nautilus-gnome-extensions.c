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
#include "nautilus-gnome-extensions.h"

#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rect.h>
#include "nautilus-gdk-extensions.h"
#include "nautilus-string.h"

static void turn_on_line_wrap_flag_callback (GtkWidget *widget, gpointer callback_data);

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
		
		if (horizontal)
			fraction = (double) *position / (double) entire_width;
		else
			fraction = (double) *position / (double) entire_height;
					
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
			bufptr = buffer->buf + (4 * band * GRADIENT_BAND_SIZE);
			
		
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

static const char **
convert_varargs_to_name_array (va_list args)
{
	GPtrArray *resizeable_array;
	const char *name;
	const char **plain_ole_array;
	
	resizeable_array = g_ptr_array_new ();

	do {
		name = va_arg (args, const char *);
		g_ptr_array_add (resizeable_array, (gpointer) name);
	} while (name != NULL);

	plain_ole_array = (const char **) resizeable_array->pdata;
	
	g_ptr_array_free (resizeable_array, FALSE);

	return plain_ole_array;
}

int
nautilus_simple_dialog (GtkWidget *parent, const char *text, const char *title, ...)
{
	va_list button_title_args;
	const char **button_titles;
        GtkWidget *dialog;
        GtkWidget *top_widget;
        GtkWidget *prompt_widget;
	
	/* Create the dialog. */
	va_start (button_title_args, title);
	button_titles = convert_varargs_to_name_array (button_title_args);
	va_end (button_title_args);
        dialog = gnome_dialog_newv (title, button_titles);
	g_free (button_titles);
	
	/* Allow close. */
        gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
        gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);
	
	/* Parent it if asked to. */
        if (parent != NULL) {
		top_widget = gtk_widget_get_toplevel (parent);
		if (GTK_IS_WINDOW (top_widget)) {
			gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (top_widget));
		}
	}
	
	/* Title it if asked to. */
	if (text != NULL) {
		prompt_widget = gtk_label_new (text);
		gtk_label_set_line_wrap (GTK_LABEL (prompt_widget), TRUE);
		gtk_label_set_justify (GTK_LABEL (prompt_widget),
				       GTK_JUSTIFY_LEFT);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
				    prompt_widget,
				    TRUE, TRUE, GNOME_PAD);
	}
	
	/* Run it. */
        gtk_widget_show_all (dialog);
        return gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
turn_on_line_wrap_flag (GtkWidget *widget, const char *message)
{
	char *text;

	/* Turn on the flag if we find a label with the message
	 * in it.
	 */
	if (GTK_IS_LABEL (widget)) {
		gtk_label_get (GTK_LABEL (widget), &text);
		if (nautilus_strcmp (text, message) == 0) {
			gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		}
	}

	/* Recurse for children. */
	if (GTK_IS_CONTAINER (widget)) {
		gtk_container_foreach (GTK_CONTAINER (widget),
				       turn_on_line_wrap_flag_callback,
				       (char *) message);
	}
}

static void
turn_on_line_wrap_flag_callback (GtkWidget *widget, gpointer callback_data)
{
	turn_on_line_wrap_flag (widget, callback_data);
}

/* Shamelessly stolen from gnome-dialog-util.c: */
static GtkWidget *
show_ok_box (const char *message,
	     const char *type,
	     GtkWindow *parent)
{  
	GtkWidget *box;

	box = gnome_message_box_new
		(message, type, GNOME_STOCK_BUTTON_OK, NULL);
	
	/* A bit of a hack. We want to use gnome_message_box_new,
	 * but we want the message to be wrapped. So, we search
	 * for the label with this message so we can mark it.
	 */
	turn_on_line_wrap_flag (box, message);

	if (parent != NULL) {
		gnome_dialog_set_parent (GNOME_DIALOG(box), parent);
	}
	gtk_widget_show (box);
	return box;
}

GtkWidget *
nautilus_warning_dialog (const char *warning)
{
	return show_ok_box (warning, GNOME_MESSAGE_BOX_WARNING, NULL);
}

GtkWidget *
nautilus_warning_dialog_parented (const char *warning,
				  GtkWindow *parent)
{
	return show_ok_box (warning, GNOME_MESSAGE_BOX_WARNING, parent);
}

GtkWidget *
nautilus_error_dialog (const char *error)
{
	return show_ok_box (error, GNOME_MESSAGE_BOX_ERROR, NULL);
}

GtkWidget *
nautilus_error_dialog_parented (const char *error,
				GtkWindow *parent)
{
	return show_ok_box (error, GNOME_MESSAGE_BOX_ERROR, parent);
}
