/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-debug-drawing.c: Eel drawing debugging aids.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "eel-debug-drawing.h"

#include "eel-art-gtk-extensions.h"
#include "eel-debug.h"
#include "eel-gdk-extensions.h"
#include "eel-gdk-extensions.h"
#include "eel-gdk-pixbuf-extensions.h"
#include "eel-gtk-extensions.h"
#include "eel-gtk-extensions.h"
#include "eel-gtk-macros.h"

#include <gtk/gtk.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

/*
 * PixbufViewer is a very simple "private" widget that displays
 * a GdkPixbuf.  It is used by eel_debug_show_pixbuf() to
 * display a pixbuf in an in process pixbuf debugging window.
 *
 * We cant use EelImage for this because part of the reason
 * for pixbuf debugging is to debug EelImage itself.
 */

#define DEBUG_TYPE_PIXBUF_VIEWER debug_pixbuf_viewer_get_type()
#define DEBUG_PIXBUF_VIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEBUG_TYPE_PIXBUF_VIEWER, DebugPixbufViewer))
#define DEBUG_IS_PIXBUF_VIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEBUG_TYPE_PIXBUF_VIEWER))

typedef struct DebugPixbufViewer DebugPixbufViewer;
typedef struct DebugPixbufViewerClass DebugPixbufViewerClass;

static GType   debug_pixbuf_viewer_get_type   (void);
static void    debug_pixbuf_viewer_set_pixbuf (DebugPixbufViewer *viewer,
					       GdkPixbuf         *pixbuf);

struct DebugPixbufViewer
{
	GtkWidget widget;
	GdkPixbuf *pixbuf;
};

struct DebugPixbufViewerClass
{
	GtkWidgetClass parent_class;
};

/* GtkObjectClass methods */
static void debug_pixbuf_viewer_class_init (DebugPixbufViewerClass *pixbuf_viewer_class);
static void debug_pixbuf_viewer_init       (DebugPixbufViewer      *pixbuf_viewer);
static void debug_pixbuf_viewer_finalize         (GObject                *object);

/* GtkWidgetClass methods */
static void debug_pixbuf_viewer_size_request     (GtkWidget              *widget,
						  GtkRequisition         *requisition);
static int  debug_pixbuf_viewer_expose_event     (GtkWidget              *widget,
						  GdkEventExpose         *event);

EEL_CLASS_BOILERPLATE (DebugPixbufViewer, debug_pixbuf_viewer, GTK_TYPE_WIDGET)

static void
debug_pixbuf_viewer_class_init (DebugPixbufViewerClass *pixbuf_viewer_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (pixbuf_viewer_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (pixbuf_viewer_class);

	object_class->finalize = debug_pixbuf_viewer_finalize;
	widget_class->size_request = debug_pixbuf_viewer_size_request;
	widget_class->expose_event = debug_pixbuf_viewer_expose_event;
}

static void
debug_pixbuf_viewer_init (DebugPixbufViewer *viewer)
{
	GTK_WIDGET_UNSET_FLAGS (viewer, GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (viewer, GTK_NO_WINDOW);
}

static void
debug_pixbuf_viewer_finalize (GObject *object)
{
 	DebugPixbufViewer *viewer;

	viewer = DEBUG_PIXBUF_VIEWER (object);
	eel_gdk_pixbuf_unref_if_not_null (viewer->pixbuf);
	viewer->pixbuf = NULL;

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
debug_pixbuf_viewer_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	DebugPixbufViewer *viewer;
	EelDimensions dimensions;

	g_assert (DEBUG_IS_PIXBUF_VIEWER (widget));
	g_assert (requisition != NULL);
	
	viewer = DEBUG_PIXBUF_VIEWER (widget);

	if (viewer->pixbuf != NULL) {
		dimensions = eel_gdk_pixbuf_get_dimensions (viewer->pixbuf);
	} else {
		dimensions = eel_dimensions_empty;
	}
	
   	requisition->width = MAX (2, dimensions.width);
   	requisition->height = MAX (2, dimensions.height);
}

static int
debug_pixbuf_viewer_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
 	DebugPixbufViewer *viewer;
	EelIRect clipped_dirty_area;
	EelIRect dirty_area;
	EelIRect bounds;

	g_assert (DEBUG_IS_PIXBUF_VIEWER (widget));
	g_assert (event != NULL);
	g_assert (event->window == widget->window);
	g_assert (GTK_WIDGET_REALIZED (widget));
	
 	viewer = DEBUG_PIXBUF_VIEWER (widget);

	if (viewer->pixbuf == NULL) {
		return TRUE;
	}
	
	bounds.x0 = widget->allocation.x + (widget->allocation.width - gdk_pixbuf_get_width (viewer->pixbuf)) / 2;
	bounds.y0 = widget->allocation.y + (widget->allocation.height - gdk_pixbuf_get_height (viewer->pixbuf)) / 2;
	bounds.x1 = bounds.x0 + gdk_pixbuf_get_width (viewer->pixbuf);
	bounds.y1 = bounds.y0 + gdk_pixbuf_get_height (viewer->pixbuf);
	
	/* Clip the dirty area to the screen; bail if no work to do */
	dirty_area = eel_gdk_rectangle_to_eel_irect (event->area);
	clipped_dirty_area = eel_gdk_window_clip_dirty_area_to_screen (event->window,
								       dirty_area);
	if (!eel_irect_is_empty (&clipped_dirty_area)) {
		EelIRect clipped_bounds;

		eel_irect_intersect (&clipped_bounds, &bounds, &clipped_dirty_area);

		if (!eel_irect_is_empty (&clipped_bounds)) {
			g_assert (clipped_bounds.x0 >= bounds.x0);
			g_assert (clipped_bounds.y0 >= bounds.y0);
			
			eel_gdk_pixbuf_draw_to_drawable (viewer->pixbuf,
							 event->window,
							 widget->style->white_gc,
							 clipped_bounds.x0 - bounds.x0,
							 clipped_bounds.y0 - bounds.y0,
							 clipped_bounds,
							 GDK_RGB_DITHER_NONE,
							 GDK_PIXBUF_ALPHA_BILEVEL,
							 EEL_STANDARD_ALPHA_THRESHHOLD);
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
	g_assert (DEBUG_IS_PIXBUF_VIEWER (viewer));
	
	if (pixbuf != viewer->pixbuf) {
		eel_gdk_pixbuf_unref_if_not_null (viewer->pixbuf);
		eel_gdk_pixbuf_ref_if_not_null (pixbuf);
		viewer->pixbuf = pixbuf;
		gtk_widget_queue_resize (GTK_WIDGET (viewer));
	}
}

/**
 * eel_debug_draw_rectangle_and_cross:
 * @rectangle: Rectangle bounding the rectangle.
 * @color: Color to use for the rectangle and cross.
 *
 * Draw a rectangle and cross.  Useful for debugging exposure events.
 */
void
eel_debug_draw_rectangle_and_cross (GdkDrawable *drawable,
				    EelIRect rectangle,
				    guint32 color,
				    gboolean draw_cross)
{
	GdkGC *gc;
	GdkColor color_gdk = { 0 };

	int width;
	int height;

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (!eel_irect_is_empty (&rectangle));

	width = rectangle.x1 - rectangle.x0;
	height = rectangle.y1 - rectangle.y0;

 	gc = gdk_gc_new (drawable);
 	gdk_gc_set_function (gc, GDK_COPY);
	
	color_gdk.red   = ((color >> 16) & 0xff) << 8;
	color_gdk.green = ((color >>  8) & 0xff) << 8;
	color_gdk.blue  = ((color      ) & 0xff) << 8;
	gdk_colormap_alloc_color (
		gdk_drawable_get_colormap (drawable),
		&color_gdk, FALSE, FALSE);
	gdk_gc_set_rgb_fg_color (gc, &color_gdk);
	
	gdk_draw_rectangle (drawable,
			    gc,
			    FALSE,
			    rectangle.x0,
			    rectangle.y0,
			    width - 1,
			    height - 1);

	if (draw_cross) {
		gdk_draw_line (drawable,
			       gc,
			       rectangle.x0,
			       rectangle.y0,
			       rectangle.x0 + width - 1,
			       rectangle.y0 + height - 1);
		
		gdk_draw_line (drawable,
			       gc,
			       rectangle.x0 + width - 1,
			       rectangle.y0,
			       rectangle.x0,
			       rectangle.y0 + height - 1);
	}

	g_object_unref (gc);
}

/**
 * eel_debug_show_pixbuf_in_external_viewer:
 * @pixbuf: Pixbuf to show.
 * @viewer_name: Viewer name.
 *
 * Show the given pixbuf in an external out of process viewer.
 * This is very useful for debugging pixbuf stuff.
 *
 * Perhaps this function should be #ifdef DEBUG or something like that.
 */
void
eel_debug_show_pixbuf_in_external_viewer (const GdkPixbuf *pixbuf,
					  const char *viewer_name)
{
	char *command;
	char *file_name;
	gboolean save_result;
	int ignore;
	int fd;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (viewer_name != NULL);

	file_name = g_strdup ("/tmp/eel-debug-png-file-XXXXXX");

	fd = g_mkstemp (file_name); 
	if (fd == -1) {
		g_free (file_name);
		file_name = g_strdup_printf ("/tmp/isis-debug-png-file-%d", getpid ());
	} else {
		close (fd);
	}

	save_result = eel_gdk_pixbuf_save_to_file (pixbuf, file_name);

	if (save_result == FALSE) {
		g_warning ("Failed to save '%s'", file_name);
		g_free (file_name);
		return;
	}
	
	command = g_strdup_printf ("%s %s", viewer_name, file_name);

	ignore = system (command);
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
 * eel_debug_show_pixbuf_in:
 * @pixbuf: Pixbuf to show.
 *
 * Show the given pixbuf in an in process window.
 */
void
eel_debug_show_pixbuf (GdkPixbuf *pixbuf)
{
	if (debug_window == NULL) {
		GtkWidget *vbox;
		
		debug_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (debug_window), vbox);
		gtk_window_set_title (GTK_WINDOW (debug_window), "Pixbuf debugging");
		gtk_window_set_resizable (GTK_WINDOW (debug_window), TRUE);
		gtk_container_set_border_width (GTK_CONTAINER (debug_window), 10);
		g_signal_connect (debug_window, "delete_event", G_CALLBACK (debug_delete_event), NULL);
		
		debug_image = gtk_widget_new (debug_pixbuf_viewer_get_type (), NULL);
		
		gtk_box_pack_start (GTK_BOX (vbox), debug_image, TRUE, TRUE, 0);

		eel_gtk_widget_set_background_color (debug_window, "white");

		eel_debug_call_at_shutdown (destroy_debug_window);
		
		gtk_widget_show (debug_image);
		gtk_widget_show (vbox);
	}

	gtk_widget_show (debug_window);
	debug_pixbuf_viewer_set_pixbuf (DEBUG_PIXBUF_VIEWER (debug_image), pixbuf);

	gdk_window_clear_area_e (debug_window->window, 0, 0, -1, -1);
}

void
eel_debug_pixbuf_draw_point (GdkPixbuf *pixbuf,
			     int x,
			     int y,
			     guint32 color,
			     int opacity)
{
	EelDimensions dimensions;
	guchar *pixels;
	gboolean has_alpha;
	guint pixel_offset;
	guint rowstride;
	guchar red;
	guchar green;
	guchar blue;
	guchar alpha;
	guchar *offset;

	g_return_if_fail (eel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (opacity >= EEL_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity <= EEL_OPACITY_FULLY_OPAQUE);

	dimensions = eel_gdk_pixbuf_get_dimensions (pixbuf);

	g_return_if_fail (x >= 0 && x < dimensions.width);
	g_return_if_fail (y >= 0 && y < dimensions.height);

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	pixel_offset = has_alpha ? 4 : 3;

	red = EEL_RGBA_COLOR_GET_R (color);
	green = EEL_RGBA_COLOR_GET_G (color);
	blue = EEL_RGBA_COLOR_GET_B (color);
	alpha = (guchar) opacity;

	offset = pixels + y * rowstride + x * pixel_offset;

	*(offset + 0) = red;
	*(offset + 1) = green;
	*(offset + 2) = blue;
	
	if (has_alpha) {
		*(offset + 3) = alpha;
	}
}

void
eel_debug_pixbuf_draw_rectangle (GdkPixbuf *pixbuf,
				 gboolean filled,
				 int x0,
				 int y0,
				 int x1,
				 int y1,
				 guint32 color,
				 int opacity)
{
	EelDimensions dimensions;
	int x;
	int y;

	g_return_if_fail (eel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (opacity >= EEL_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity <= EEL_OPACITY_FULLY_OPAQUE);

	dimensions = eel_gdk_pixbuf_get_dimensions (pixbuf);

	if (x0 == -1) {
		x0 = 0;
	}

	if (y0 == -1) {
		y0 = 0;
	}

	if (x1 == -1) {
		x1 = dimensions.width - 1;
	}

	if (y1 == -1) {
		y1 = dimensions.height - 1;
	}

	g_return_if_fail (x1 > x0);
	g_return_if_fail (y1 > y0);
	g_return_if_fail (x0 >= 0 && x0 < dimensions.width);
	g_return_if_fail (y0 >= 0 && y0 < dimensions.height);
	g_return_if_fail (x1 >= 0 && x1 < dimensions.width);
	g_return_if_fail (y1 >= 0 && y1 < dimensions.height);

	if (filled) {
		for (y = y0; y <= y1; y++) {
			for (x = x0; x <= x1; x++) {
				eel_debug_pixbuf_draw_point (pixbuf, x, y, color, opacity);
			}
		}
	} else {
		/* Top / Bottom */
		for (x = x0; x <= x1; x++) {
			eel_debug_pixbuf_draw_point (pixbuf, x, y0, color, opacity);
			eel_debug_pixbuf_draw_point (pixbuf, x, y1, color, opacity);
		}
		
		/* Left / Right */
		for (y = y0; y <= y1; y++) {
			eel_debug_pixbuf_draw_point (pixbuf, x0, y, color, opacity);
			eel_debug_pixbuf_draw_point (pixbuf, x1, y, color, opacity);
		}
	}
}

void
eel_debug_pixbuf_draw_rectangle_inset (GdkPixbuf *pixbuf,
				       gboolean filled,
				       int x0,
				       int y0,
				       int x1,
				       int y1,
				       guint32 color,
				       int opacity,
				       int inset)
{
	EelDimensions dimensions;
	
	g_return_if_fail (eel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (opacity >= EEL_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity <= EEL_OPACITY_FULLY_OPAQUE);

	dimensions = eel_gdk_pixbuf_get_dimensions (pixbuf);

	if (x0 == -1) {
		x0 = 0;
	}

	if (y0 == -1) {
		y0 = 0;
	}

	if (x1 == -1) {
		x1 = dimensions.width - 1;
	}

	if (y1 == -1) {
		y1 = dimensions.height - 1;
	}

	x0 += inset;
	y0 += inset;
	x1 -= inset;
	y1 -= inset;

	g_return_if_fail (x1 > x0);
	g_return_if_fail (y1 > y0);

	eel_debug_pixbuf_draw_rectangle (pixbuf, filled, x0, y0, x1, y1, color, opacity);
}
