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
#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-art-gtk-extensions.h"

#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtksignal.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

/*
 * PixbufViewer is a very simple "private" widget that displays
 * a GdkPixbuf.  It is used by nautilus_debug_show_pixbuf() to
 * display a pixbuf in an in process pixbuf debugging window.
 *
 * We cant use NautilusImage for this because part of the reason
 * for pixbuf debugging is to debug NautilusImage itself.
 */
#define DEBUG_TYPE_PIXBUF_VIEWER (debug_pixbuf_viewer_get_type ())
#define DEBUG_PIXBUF_VIEWER(obj) (GTK_CHECK_CAST ((obj), DEBUG_TYPE_PIXBUF_VIEWER, DebugPixbufViewer))
#define DEBUG_IS_PIXBUF_VIEWER(obj) (GTK_CHECK_TYPE ((obj), DEBUG_TYPE_PIXBUF_VIEWER))

typedef struct _DebugPixbufViewer DebugPixbufViewer;
typedef struct _DebugPixbufViewerClass DebugPixbufViewerClass;

GtkType     debug_pixbuf_viewer_get_type   (void);
static void debug_pixbuf_viewer_set_pixbuf (DebugPixbufViewer *viewer,
					    GdkPixbuf         *pixbuf);

struct _DebugPixbufViewer
{
	GtkWidget widget;
	GdkPixbuf *pixbuf;
};

struct _DebugPixbufViewerClass
{
	GtkWidgetClass parent_class;
};

/* GtkObjectClass methods */
static void debug_pixbuf_viewer_initialize_class (DebugPixbufViewerClass *pixbuf_viewer_class);
static void debug_pixbuf_viewer_initialize       (DebugPixbufViewer      *pixbuf_viewer);
static void debug_pixbuf_viewer_destroy          (GtkObject              *object);

/* GtkWidgetClass methods */
static void debug_pixbuf_viewer_size_request     (GtkWidget              *widget,
						  GtkRequisition         *requisition);
static int  debug_pixbuf_viewer_expose_event     (GtkWidget              *widget,
						  GdkEventExpose         *event);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (DebugPixbufViewer, debug_pixbuf_viewer, GTK_TYPE_WIDGET)

static void
debug_pixbuf_viewer_initialize_class (DebugPixbufViewerClass *pixbuf_viewer_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (pixbuf_viewer_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (pixbuf_viewer_class);

	object_class->destroy = debug_pixbuf_viewer_destroy;
	widget_class->size_request = debug_pixbuf_viewer_size_request;
	widget_class->expose_event = debug_pixbuf_viewer_expose_event;
}

static void
debug_pixbuf_viewer_initialize (DebugPixbufViewer *viewer)
{
	GTK_WIDGET_UNSET_FLAGS (viewer, GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (viewer, GTK_NO_WINDOW);
}

static void
debug_pixbuf_viewer_destroy (GtkObject *object)
{
 	DebugPixbufViewer *viewer;

	g_return_if_fail (DEBUG_IS_PIXBUF_VIEWER (object));

	viewer = DEBUG_PIXBUF_VIEWER (object);
	nautilus_gdk_pixbuf_unref_if_not_null (viewer->pixbuf);
	viewer->pixbuf = NULL;

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
debug_pixbuf_viewer_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	DebugPixbufViewer *viewer;
	ArtIRect frame;

	g_return_if_fail (DEBUG_IS_PIXBUF_VIEWER (widget));
	g_return_if_fail (requisition != NULL);
	
	viewer = DEBUG_PIXBUF_VIEWER (widget);

	frame = (viewer->pixbuf != NULL) ?
		nautilus_gdk_pixbuf_get_frame (viewer->pixbuf) :
		NAUTILUS_ART_IRECT_EMPTY;
	
   	requisition->width = MAX (2, frame.x1);
   	requisition->height = MAX (2, frame.y1);
}

static int
debug_pixbuf_viewer_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
 	DebugPixbufViewer *viewer;
	ArtIRect clipped_dirty_area;
	ArtIRect dirty_area;
	ArtIRect bounds;

	g_return_val_if_fail (DEBUG_IS_PIXBUF_VIEWER (widget), TRUE);
	g_return_val_if_fail (event != NULL, TRUE);
	g_return_val_if_fail (event->window == widget->window, TRUE);
	g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), TRUE);
	
 	viewer = DEBUG_PIXBUF_VIEWER (widget);

	if (viewer->pixbuf == NULL) {
		return TRUE;
	}
	
	bounds.x0 = widget->allocation.x + (widget->allocation.width - gdk_pixbuf_get_width (viewer->pixbuf)) / 2;
	bounds.y0 = widget->allocation.y + (widget->allocation.height - gdk_pixbuf_get_height (viewer->pixbuf)) / 2;
	bounds.x1 = bounds.x0 + gdk_pixbuf_get_width (viewer->pixbuf);
	bounds.y1 = bounds.y0 + gdk_pixbuf_get_height (viewer->pixbuf);
	
	/* Clip the dirty area to the screen; bail if no work to do */
	dirty_area = nautilus_irect_assign_gdk_rectangle (&event->area);
	clipped_dirty_area = nautilus_irect_gdk_window_clip_dirty_area_to_screen (event->window,
										  &dirty_area);
	if (!art_irect_empty (&clipped_dirty_area)) {
		ArtIRect clipped_bounds;

		art_irect_intersect (&clipped_bounds, &bounds, &clipped_dirty_area);

		if (!art_irect_empty (&clipped_bounds)) {
			g_assert (clipped_bounds.x0 >= bounds.x0);
			g_assert (clipped_bounds.y0 >= bounds.y0);
			
			nautilus_gdk_pixbuf_draw_to_drawable (viewer->pixbuf,
							      event->window,
							      widget->style->white_gc,
							      clipped_bounds.x0 - bounds.x0,
							      clipped_bounds.y0 - bounds.y0,
							      &clipped_bounds,
							      GDK_RGB_DITHER_NONE,
							      GDK_PIXBUF_ALPHA_BILEVEL,
							      NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
		}
	}

	bounds.x0 -= 1;
	bounds.y0 -= 1;
	bounds.x1 += 1;
	bounds.y1 += 1;

	return TRUE;
}

static void
debug_pixbuf_viewer_set_pixbuf (DebugPixbufViewer *viewer, GdkPixbuf *pixbuf)
{
	g_return_if_fail (DEBUG_IS_PIXBUF_VIEWER (viewer));
	
	if (pixbuf != viewer->pixbuf) {
		nautilus_gdk_pixbuf_unref_if_not_null (viewer->pixbuf);
		nautilus_gdk_pixbuf_ref_if_not_null (pixbuf);
		viewer->pixbuf = pixbuf;
		gtk_widget_queue_resize (GTK_WIDGET (viewer));
	}
}

/**
 * nautilus_debug_draw_rectangle_and_cross:
 * @rectangle: Rectangle bounding the rectangle.
 * @color: Color to use for the rectangle and cross.
 *
 * Draw a rectangle and cross.  Useful for debugging exposure events.
 */
void
nautilus_debug_draw_rectangle_and_cross (GdkDrawable *drawable,
					 const ArtIRect *rectangle,
					 guint32 color,
					 gboolean draw_cross)
{
	GdkGC *gc;

	int width;
	int height;

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (rectangle != NULL);
	g_return_if_fail (rectangle->x1 > rectangle->x0);
	g_return_if_fail (rectangle->y1 > rectangle->y0);

	width = rectangle->x1 - rectangle->x0;
	height = rectangle->y1 - rectangle->y0;

 	gc = gdk_gc_new (drawable);
 	gdk_gc_set_function (gc, GDK_COPY);
	gdk_rgb_gc_set_foreground (gc, color);
	
	gdk_draw_rectangle (drawable,
			    gc,
			    FALSE,
			    rectangle->x0,
			    rectangle->y0,
			    width - 1,
			    height - 1);

	if (draw_cross) {
		gdk_draw_line (drawable,
			       gc,
			       rectangle->x0,
			       rectangle->y0,
			       rectangle->x0 + width - 1,
			       rectangle->y0 + height - 1);
		
		gdk_draw_line (drawable,
			       gc,
			       rectangle->x0 + width - 1,
			       rectangle->y0,
			       rectangle->x0,
			       rectangle->y0 + height - 1);
	}

	gdk_gc_unref (gc);
}

/**
 * nautilus_debug_show_pixbuf_in_eog:
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

/**
 * nautilus_debug_show_pixbuf_in:
 * @pixbuf: Pixbuf to show.
 *
 * Show the given pixbuf in an in process window.
 */
void
nautilus_debug_show_pixbuf (const GdkPixbuf *pixbuf)
{
	if (debug_window == NULL) {
		GtkWidget *vbox;
		
		debug_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (debug_window), vbox);
		gtk_window_set_title (GTK_WINDOW (debug_window), "Pixbuf debugging");
		gtk_window_set_policy (GTK_WINDOW (debug_window), TRUE, TRUE, FALSE);
		gtk_container_set_border_width (GTK_CONTAINER (debug_window), 10);
		gtk_signal_connect (GTK_OBJECT (debug_window), "delete_event", GTK_SIGNAL_FUNC (debug_delete_event), NULL);
		
		debug_image = gtk_widget_new (debug_pixbuf_viewer_get_type (), NULL);
		
		gtk_box_pack_start (GTK_BOX (vbox), debug_image, TRUE, TRUE, 0);

		nautilus_gtk_widget_set_background_color (debug_window, "white");

		g_atexit (destroy_debug_window);
		
		gtk_widget_show (debug_image);
		gtk_widget_show (vbox);
	}

	gtk_widget_show (debug_window);
	debug_pixbuf_viewer_set_pixbuf (DEBUG_PIXBUF_VIEWER (debug_image), (GdkPixbuf *) pixbuf);

	gdk_window_clear_area_e (debug_window->window, 0, 0, -1, -1);
}
