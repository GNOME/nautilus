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
#include "nautilus-image.h"

#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
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
static GtkWidget *debug_image = NULL;

static void
debug_delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_widget_hide (widget);
}

static void
destroy_debug_window (void)
{
	if (debug_window != NULL) {
		gtk_widget_destroy (debug_window);
		debug_window = NULL;
	}
}

void
nautilus_debug_show_pixbuf (const GdkPixbuf *pixbuf)
{
	if (debug_window == NULL) {
		GtkWidget *vbox;
		
		debug_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (debug_window), vbox);
		gtk_window_set_title (GTK_WINDOW (debug_window), "Image debugging");
		gtk_window_set_policy (GTK_WINDOW (debug_window), TRUE, TRUE, FALSE);
		gtk_container_set_border_width (GTK_CONTAINER (debug_window), 10);
		gtk_signal_connect (GTK_OBJECT (debug_window), "delete_event", GTK_SIGNAL_FUNC (debug_delete_event), NULL);
		
		debug_image = nautilus_image_new ();
		
		gtk_box_pack_start (GTK_BOX (vbox), debug_image, TRUE, TRUE, 0);

		nautilus_gtk_widget_set_background_color (debug_window, "white");

		g_atexit (destroy_debug_window);
		
		gtk_widget_show (debug_image);
		gtk_widget_show (vbox);
	}

	gtk_widget_show (debug_window);
	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (debug_image), (GdkPixbuf *) pixbuf);
}
