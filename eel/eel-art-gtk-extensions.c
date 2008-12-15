/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-eel-gtk-extensions.c - Access gtk/gdk attributes as libeel rectangles.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PEELICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "eel-art-gtk-extensions.h"
#include <gdk/gdkx.h>

/**
 * eel_gdk_rectangle_to_eel_irect:
 * @gdk_rectangle: The source GdkRectangle.
 *
 * Return value: An EelIRect representation of the GdkRectangle.
 *
 * This is a very simple conversion of rectangles from the Gdk to the Libeel
 * universe.  This is useful in code that does clipping (or other operations)
 * using libeel and has a GdkRectangle to work with - for example expose_event()
 * in GtkWidget's. 
 */
EelIRect
eel_gdk_rectangle_to_eel_irect (GdkRectangle gdk_rectangle)
{
	EelIRect rectangle;

	rectangle.x0 = gdk_rectangle.x;
	rectangle.y0 = gdk_rectangle.y;
	rectangle.x1 = rectangle.x0 + (int) gdk_rectangle.width;
	rectangle.y1 = rectangle.y0 + (int) gdk_rectangle.height;

	return rectangle;
}

/**
 * eel_screen_get_dimensions:
 *
 * Return value: The screen dimensions.
 *
 */
EelDimensions
eel_screen_get_dimensions (void)
{
	EelDimensions screen_dimensions;

	screen_dimensions.width = gdk_screen_width ();
	screen_dimensions.height = gdk_screen_height ();
	
	g_assert (screen_dimensions.width > 0);
	g_assert (screen_dimensions.height > 0);

	return screen_dimensions;
}

/**
 * eel_gdk_window_get_bounds:
 * @gdk_window: The source GdkWindow.
 *
 * Return value: An EelIRect representation of the given GdkWindow's geometry
 * relative to its parent in the Gdk window hierarchy.
 *
 */
EelIRect
eel_gdk_window_get_bounds (GdkWindow *gdk_window)
{
	EelIRect bounds;
	int width;
	int height;

	g_return_val_if_fail (gdk_window != NULL, eel_irect_empty);

	gdk_window_get_position (gdk_window, &bounds.x0, &bounds.y0);
	gdk_drawable_get_size (gdk_window, &width, &height);

	bounds.x1 = bounds.x0 + width;
	bounds.y1 = bounds.y0 + height;

	return bounds;
}

/**
 * eel_gdk_window_get_bounds:
 * @gdk_window: The source GdkWindow.
 *
 * Return value: An EelIRect representation of the given GdkWindow's geometry
 * relative to the screen.
 *
 */
EelIRect
eel_gdk_window_get_screen_relative_bounds (GdkWindow *gdk_window)
{
	EelIRect screen_bounds;
	int width;
	int height;
	
	g_return_val_if_fail (gdk_window != NULL, eel_irect_empty);

	if (!gdk_window_get_origin (gdk_window,
				    &screen_bounds.x0,
				    &screen_bounds.y0)) {
		return eel_irect_empty;
	}
	
	gdk_drawable_get_size (gdk_window, &width, &height);
	
	screen_bounds.x1 = screen_bounds.x0 + width;
	screen_bounds.y1 = screen_bounds.y0 + height;
	
	return screen_bounds;
}

/**
 * eel_gtk_widget_get_bounds:
 * @gtk_widget: The source GtkWidget.
 *
 * Return value: An EelIRect representation of the given GtkWidget's geometry
 * relative to its parent.  In the Gtk universe this is known as "allocation."
 *
 */
EelIRect
eel_gtk_widget_get_bounds (GtkWidget *gtk_widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (gtk_widget), eel_irect_empty);
	
	return eel_irect_assign (gtk_widget->allocation.x,
				     gtk_widget->allocation.y,
				     (int) gtk_widget->allocation.width,
				     (int) gtk_widget->allocation.height);
}

/**
 * eel_gtk_widget_get_dimensions:
 * @gtk_widget: The source GtkWidget.
 *
 * Return value: The widget's dimensions.  The returned dimensions are only valid
 *               after the widget's geometry has been "allocated" by its container.
 */
EelDimensions
eel_gtk_widget_get_dimensions (GtkWidget *gtk_widget)
{
	EelDimensions dimensions;
	
	g_return_val_if_fail (GTK_IS_WIDGET (gtk_widget), eel_dimensions_empty);
	
	dimensions.width = (int) gtk_widget->allocation.width;
	dimensions.height = (int) gtk_widget->allocation.height;
	
	return dimensions;
}

/**
 * eel_gtk_widget_get_preferred_dimensions:
 * @gtk_widget: The source GtkWidget.
 *
 * Return value: The widget's preferred dimensions.  The preferred dimensions are
 *               computed by calling the widget's 'size_request' method and thus
 *               could potentially be expensive for complicated widgets.
 */
EelDimensions
eel_gtk_widget_get_preferred_dimensions (GtkWidget *gtk_widget)
{
	GtkRequisition requisition;
	EelDimensions preferred_dimensions;

	g_return_val_if_fail (GTK_IS_WIDGET (gtk_widget), eel_dimensions_empty);

	gtk_widget_size_request (gtk_widget, &requisition);

	preferred_dimensions.width = (int) requisition.width;
	preferred_dimensions.height = (int) requisition.height;

	return preferred_dimensions;
}

/**
 * eel_gdk_window_clip_dirty_area_to_screen:
 * @gdk_window: The GdkWindow that the damage occured on.
 * @dirty_area: The dirty area as an EelIRect.
 *
 * Return value: An EelIRect of the dirty area clipped to the screen.
 *
 * This function is useful to do less work in expose_event() GtkWidget methods.
 * It also ensures that any drawing that the widget does is actually onscreen.
 */
EelIRect
eel_gdk_window_clip_dirty_area_to_screen (GdkWindow *gdk_window,
					  EelIRect dirty_area)
{
	EelIRect clipped;
	EelDimensions screen_dimensions;
	EelIRect screen_relative_bounds;
	int dirty_width;
	int dirty_height;

	g_return_val_if_fail (gdk_window != NULL, eel_irect_empty);

	dirty_width = dirty_area.x1 - dirty_area.x0;
	dirty_height = dirty_area.y1 - dirty_area.y0;

	g_return_val_if_fail (dirty_width > 0, eel_irect_empty);
	g_return_val_if_fail (dirty_height > 0, eel_irect_empty);

	screen_dimensions = eel_screen_get_dimensions ();
	screen_relative_bounds = eel_gdk_window_get_screen_relative_bounds (gdk_window);
	
	/* Window is obscured by left edge of screen */
	if ((screen_relative_bounds.x0 + dirty_area.x0) < 0) {
		int clipped_width = screen_relative_bounds.x0 + dirty_area.x0 + dirty_width;
		clipped.x0 = dirty_area.x0 + dirty_width - clipped_width;
		clipped.x1 = clipped.x0 + clipped_width;
	} else {
		clipped.x0 = dirty_area.x0;
		clipped.x1 = clipped.x0 + dirty_width;
	}
	
	/* Window is obscured by right edge of screen */
	if (screen_relative_bounds.x1 > screen_dimensions.width) {
 		int obscured_width;
		
		obscured_width = 
			screen_relative_bounds.x0 + dirty_area.x0 + dirty_width - screen_dimensions.width;
		
		if (obscured_width > 0) {
			clipped.x1 -= obscured_width;
		}
	}

	/* Window is obscured by top edge of screen */
	if ((screen_relative_bounds.y0 + dirty_area.y0) < 0) {
		int clipped_height = screen_relative_bounds.y0 + dirty_area.y0 + dirty_height;
		clipped.y0 = dirty_area.y0 + dirty_height - clipped_height;
		clipped.y1 = clipped.y0 + clipped_height;
	} else {
		clipped.y0 = dirty_area.y0;
		clipped.y1 = clipped.y0 + dirty_height;
	}
	
	/* Window is obscured by bottom edge of screen */
	if (screen_relative_bounds.y1 > screen_dimensions.height) {
 		int obscured_height;
		
		obscured_height = 
			screen_relative_bounds.y0 + dirty_area.y0 + dirty_height - screen_dimensions.height;
		
		if (obscured_height > 0) {
			clipped.y1 -= obscured_height;
		}
	}

	if (eel_irect_is_empty (&clipped)) {
		clipped = eel_irect_empty;
	}

	return clipped;
}

GdkRectangle
eel_irect_to_gdk_rectangle (EelIRect rectangle)
{
	GdkRectangle gdk_rect;

	gdk_rect.x = rectangle.x0;
	gdk_rect.y = rectangle.y0;
	gdk_rect.width = eel_irect_get_width (rectangle);
	gdk_rect.height = eel_irect_get_height (rectangle);

	return gdk_rect;
}

EelDimensions
eel_gdk_window_get_dimensions (GdkWindow *gdk_window)
{
	EelDimensions dimensions;
	
	g_return_val_if_fail (gdk_window != NULL, eel_dimensions_empty);

	gdk_drawable_get_size (gdk_window, &dimensions.width, &dimensions.height);

	return dimensions;
}

EelIPoint
eel_gdk_get_pointer_position (void)
{

	EelIPoint position;

	gdk_window_get_pointer (gdk_get_default_root_window (),
				&position.x,
				&position.y,
				NULL);
	
	return position;
}
