/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-background.c: Object for the background of a widget.
 
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
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-background-canvas-group.h"

#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>

#include "nautilus-background.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gtk-macros.h"

static void nautilus_background_canvas_group_initialize_class (gpointer         klass);
static void nautilus_background_canvas_group_initialize       (gpointer         object,
							       gpointer         klass);
static void nautilus_background_canvas_group_draw             (GnomeCanvasItem *item,
							       GdkDrawable     *drawable,
							       int              x,
							       int              y,
							       int              width,
							       int              height);
static void nautilus_background_canvas_group_render           (GnomeCanvasItem *item,
							       GnomeCanvasBuf  *buffer);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBackgroundCanvasGroup, nautilus_background_canvas_group, GNOME_TYPE_CANVAS_GROUP)

static void
nautilus_background_canvas_group_initialize_class (gpointer klass)
{
	GNOME_CANVAS_ITEM_CLASS (klass)->draw = nautilus_background_canvas_group_draw;
	GNOME_CANVAS_ITEM_CLASS (klass)->render = nautilus_background_canvas_group_render;
}

static void
nautilus_background_canvas_group_initialize (gpointer object, gpointer klass)
{
}

static void
nautilus_background_canvas_group_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				       int drawable_corner_x, int drawable_corner_y,
				       int drawable_width, int drawable_height)
{
	NautilusBackground *background;
	GdkGC *gc;
	GdkRectangle rectangle;

	/* Draw the background. */
	background = nautilus_get_widget_background (GTK_WIDGET (item->canvas));

	/* If GtkStyle handled it, then we don't want to bother doing
	 * any additional work. It would be way slow to draw again.
	 */
	if (nautilus_background_is_too_complex_for_gtk_style (background)) {
		/* Create a new gc each time.
		 * If this is a speed problem, we can create one and keep it around,
		 * but it's a bit more complicated to ensure that it's always compatible
		 * with whatever drawable is passed in.
		 */
		gc = gdk_gc_new (drawable);

		/* The rectangle is the size of the entire viewed area
		 * of the canvas. The corner is determined by the
		 * current scroll position of the GtkLayout, and the
		 * size is determined by the current size of the
		 * widget. Since 0,0 is the corner of the drawable,
		 * we need to offset the rectangle so it's relative to
		 * the drawable's coordinates.
		 */
	       	rectangle.x = GTK_LAYOUT (item->canvas)->xoffset - drawable_corner_x;
		rectangle.y = GTK_LAYOUT (item->canvas)->yoffset - drawable_corner_y;
		rectangle.width = GTK_WIDGET (item->canvas)->allocation.width;
		rectangle.height = GTK_WIDGET (item->canvas)->allocation.height;

		nautilus_background_draw (background, drawable, gc, &rectangle,
					  -drawable_corner_x, -drawable_corner_y);

		gdk_gc_unref (gc);
	}

	/* Call through to the GnomeCanvasGroup implementation, which
	 * will draw all the canvas items.
	 */
	NAUTILUS_CALL_PARENT_CLASS (GNOME_CANVAS_ITEM_CLASS, draw,
				    (item, drawable,
				     drawable_corner_x, drawable_corner_y,
				     drawable_width, drawable_height));				     
}


/* draw the background for the anti-aliased canvas case */
static void
nautilus_background_canvas_group_render (GnomeCanvasItem *item, GnomeCanvasBuf *buffer)
{
	NautilusBackground *background;	
	double left, top, bottom, right;
	int entire_width, entire_height;
			
	background = nautilus_get_widget_background (GTK_WIDGET (item->canvas));
	if (background != NULL) {
		/* FIXME: Don't we want to draw the background for the size of the window
		 * rather than the whole canvas? This won't work right for gradients, will it?
		 */
		gnome_canvas_get_scroll_region (GNOME_CANVAS(item->canvas),
						&left, &top, &right, &bottom);

		/* FIXME: Icons can go past bounds? News to me! (Darin)
		 * This slop value of 24 must die.
		 */
		entire_width = right - left + 24; /* add some slop since icons can go past bounds */
		entire_height = bottom - top + 24;

		nautilus_background_draw_aa (background, buffer, entire_width, entire_height);

		/* FIXME: Shouldn't nautilus_background_draw_aa do these? */
		buffer->is_bg = FALSE;
		buffer->is_buf = TRUE;
	} else {
		/* FIXME: Why do we need this in this case? */
		gnome_canvas_buf_ensure_buf (buffer);
	}
	
	/* Call through to the GnomeCanvasGroup implementation, which will draw all
	 * the canvas items.
	 */
	NAUTILUS_CALL_PARENT_CLASS (GNOME_CANVAS_ITEM_CLASS, render, (item, buffer));
}
