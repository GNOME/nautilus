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
#include <libgnomeui/gnome-uidefs.h>

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
