/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-debug-drawing.c: Nautilus drawing debugging aids.
 
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
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-debug-drawing.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"

#include <gtk/gtkwindow.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtksignal.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

void
nautilus_debug_draw_rectangle_and_cross (GdkWindow *window,
					 const ArtIRect *rectangle,
					 guint32 color)
{
	GdkGC *gc;

	int width;
	int height;

	g_return_if_fail (window != NULL);
	g_return_if_fail (rectangle != NULL);
	g_return_if_fail (rectangle->x1 > rectangle->x0);
	g_return_if_fail (rectangle->y1 > rectangle->y0);

	width = rectangle->x1 - rectangle->x0;
	height = rectangle->y1 - rectangle->y0;

 	gc = gdk_gc_new (window);
 	gdk_gc_set_function (gc, GDK_COPY);
	gdk_rgb_gc_set_foreground (gc, color);
	
	gdk_draw_rectangle (window,
			    gc,
			    FALSE,
			    rectangle->x0,
			    rectangle->y0,
			    width - 1,
			    height - 1);
	
	gdk_draw_line (window,
		       gc,
		       rectangle->x0,
		       rectangle->y0,
		       rectangle->x0 + width - 1,
		       rectangle->y0 + height - 1);
	
	gdk_draw_line (window,
		       gc,
		       rectangle->x0 + width - 1,
		       rectangle->y0,
		       rectangle->x0,
		       rectangle->y0 + height - 1);

	gdk_gc_unref (gc);
}

/**
 * nautilus_gdk_pixbuf_show_in_eog:
 * @pixbuf: Pixbuf to show.
 *
 * Show the given pixbuf in eog.  This is very useful for debugging pixbuf
 * stuff.  Perhaps this function should be #ifdef DEBUG or something like that.
 */
void
nautilus_debug_show_pixbuf_in_eog (const GdkPixbuf *pixbuf)
{
	char *command;
	char *file_name;
	gboolean save_result;

	g_return_if_fail (pixbuf != NULL);

	file_name = g_strdup ("/tmp/nautilus-debug-png-file-XXXXXX");

	if (mktemp (file_name) != file_name) {
		g_free (file_name);
		file_name = g_strdup_printf ("/tmp/isis-debug-png-file.%d", getpid ());
	}

	save_result = nautilus_gdk_pixbuf_save_to_file (pixbuf, file_name);

	if (save_result == FALSE) {
		g_warning ("Failed to save '%s'", file_name);
		g_free (file_name);
		return;
	}
	
	command = g_strdup_printf ("eog %s", file_name);

	system (command);
	g_free (command);
	remove (file_name);
	g_free (file_name);
}

static GtkWidget *debug_window = NULL;
static GtkWidget *debug_event_box = NULL;
static GdkPixbuf *debug_pixbuf = NULL;
static const guint debug_border_width = 10;

static void
debug_delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_widget_hide (widget);
}

static void
event_box_draw (GtkWidget *widget,
		GdkRectangle *area,
		gpointer callback_data)
{
	ArtIRect pixbuf_frame;
	ArtIRect pixbuf_bounds;

	g_return_if_fail (GTK_IS_EVENT_BOX (widget));

	if (debug_pixbuf == NULL) {
		return;
	}
	
	pixbuf_frame = nautilus_gdk_pixbuf_get_frame (debug_pixbuf);
	
	if (art_irect_empty (&pixbuf_frame)) {
		return;
	}

	pixbuf_bounds.x0 = debug_border_width;
	pixbuf_bounds.y0 = debug_border_width;
	pixbuf_bounds.x1 = pixbuf_bounds.x0 + pixbuf_frame.x1;
	pixbuf_bounds.y1 = pixbuf_bounds.y0 + pixbuf_frame.y1;
	
	nautilus_gdk_pixbuf_draw_to_drawable (debug_pixbuf,
					      debug_event_box->window,
					      debug_event_box->style->white_gc,
					      0,
					      0,
					      &pixbuf_bounds,
					      GDK_RGB_DITHER_NONE,
					      GDK_PIXBUF_ALPHA_BILEVEL,
					      NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
}

static int
event_box_expose_event (GtkWidget *widget,
			GdkEventExpose *event,
			gpointer callback_data)
{
	GdkRectangle area;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (widget), TRUE);
	g_return_val_if_fail (event != NULL, TRUE);

	area.x = widget->allocation.x;
	area.y = widget->allocation.y;
	area.width = widget->allocation.width;
	area.height = widget->allocation.height;

	event_box_draw (widget, &area, callback_data);

	return TRUE;
}

static void
destroy_debug_window (void)
{
	if (debug_window != NULL) {
		gtk_widget_destroy (debug_window);
		debug_window = NULL;
		nautilus_gdk_pixbuf_unref_if_not_null (debug_pixbuf);
	}
}

void
nautilus_debug_show_pixbuf (const GdkPixbuf *pixbuf)
{

	if (debug_window == NULL) {
		debug_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (debug_window), "Pixbuf debugging");
		gtk_window_set_policy (GTK_WINDOW (debug_window), TRUE, TRUE, FALSE);
		gtk_container_set_border_width (GTK_CONTAINER (debug_window), debug_border_width);
		
		debug_event_box = gtk_event_box_new ();
		gtk_container_add (GTK_CONTAINER (debug_window), debug_event_box);

		gtk_signal_connect (GTK_OBJECT (debug_window),
				    "delete_event",
				    GTK_SIGNAL_FUNC (debug_delete_event),
				    NULL);

		gtk_signal_connect (GTK_OBJECT (debug_event_box),
				    "draw",
				    GTK_SIGNAL_FUNC (event_box_draw),
				    NULL);

		gtk_signal_connect (GTK_OBJECT (debug_event_box),
				    "expose_event",
				    GTK_SIGNAL_FUNC (event_box_expose_event),
				    NULL);

		nautilus_gtk_widget_set_background_color (debug_event_box, "white");
		
		g_atexit (destroy_debug_window);
		
 		gtk_widget_show (debug_event_box);
	}
	
	nautilus_gdk_pixbuf_unref_if_not_null (debug_pixbuf);
	debug_pixbuf = (GdkPixbuf *) pixbuf;
	nautilus_gdk_pixbuf_ref_if_not_null (debug_pixbuf);

	if (debug_pixbuf != NULL) {
		ArtIRect pixbuf_frame;
		
		pixbuf_frame = nautilus_gdk_pixbuf_get_frame (pixbuf);
		
		if (!art_irect_empty (&pixbuf_frame)) {
			gtk_widget_set_usize (debug_window,
					      pixbuf_frame.x1 + 2 * debug_border_width,
					      pixbuf_frame.y1 + 2 * debug_border_width);
		}
	}

	gtk_widget_show (debug_event_box);
	gtk_widget_show (debug_window);

	{
		GdkEventExpose event;
  
		if (GTK_WIDGET_DRAWABLE (debug_event_box))
		{
			event.type = GDK_EXPOSE;
			event.send_event = TRUE;
			event.window = debug_event_box->window;
			event.area.x = debug_event_box->allocation.x;
			event.area.y = debug_event_box->allocation.y;
			event.area.width = debug_event_box->allocation.width;
			event.area.height = debug_event_box->allocation.height;
			event.count = 0;
			
			gdk_window_ref (event.window);
			gtk_widget_event (debug_event_box, (GdkEvent*) &event);
			gdk_window_unref (event.window);
		}
	}
}
