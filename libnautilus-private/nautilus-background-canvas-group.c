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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "nautilus-background-canvas-group.h"

#include <libgnomeui/gnome-canvas.h>
#include "nautilus-background.h"
#include "nautilus-gtk-macros.h"

static void nautilus_background_canvas_group_initialize_class (gpointer klass);
static void nautilus_background_canvas_group_initialize (gpointer object, gpointer klass);
static void nautilus_background_canvas_group_destroy (GtkObject *object);
static void nautilus_background_canvas_group_finalize (GtkObject *object);
static void nautilus_background_canvas_group_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
						   int x, int y, int width, int height);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBackgroundCanvasGroup, nautilus_background_canvas_group, GNOME_TYPE_CANVAS_GROUP)

static void
nautilus_background_canvas_group_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *canvas_item_class;

	object_class = GTK_OBJECT_CLASS (klass);
	canvas_item_class = GNOME_CANVAS_ITEM_CLASS (klass);
	
	object_class->destroy = nautilus_background_canvas_group_destroy;
	object_class->finalize = nautilus_background_canvas_group_finalize;

	canvas_item_class->draw = nautilus_background_canvas_group_draw;
}

static void
nautilus_background_canvas_group_initialize (gpointer object, gpointer klass)
{
}

static void
nautilus_background_canvas_group_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_background_canvas_group_finalize (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_background_canvas_group_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				       int drawable_corner_x, int drawable_corner_y,
				       int drawable_width, int drawable_height)
{
	NautilusBackground *background;

	/* Draw the background. */
	background = nautilus_get_widget_background(GTK_WIDGET (item->canvas));
	if (background != NULL) {
		GdkGC *gc;
		GdkColormap *colormap;
		GdkRectangle rectangle;

		/* Create a new gc each time.
		   If this is a speed problem, we can create one and keep it around,
		   but it's a bit more complicated to ensure that it's always compatible
		   with whatever drawable is passed in to use.
		*/
		gc = gdk_gc_new (drawable);

		colormap = gtk_widget_get_colormap (GTK_WIDGET (item->canvas));

		/* The rectangle is the size of the entire viewed area of the canvas.
		   The corner is determined by the current scroll position of the
		   GtkLayout, and the size is determined by the current size of the widget.
		   Since 0,0 is the corner of the drawable, we need to offset the rectangle
		   so it's relative to the drawable's coordinates.
		*/
	       	rectangle.x = GTK_LAYOUT (item->canvas)->xoffset - drawable_corner_x;
		rectangle.y = GTK_LAYOUT (item->canvas)->yoffset - drawable_corner_y;
		rectangle.width = GTK_WIDGET (item->canvas)->allocation.width;
		rectangle.height = GTK_WIDGET (item->canvas)->allocation.height;

		nautilus_background_draw (background, drawable, gc, colormap, &rectangle);

		gdk_gc_unref (gc);
	}

	/* Call through to the GnomeCanvasGroup implementation, which will draw all
	   the canvas items.
	*/
	NAUTILUS_CALL_PARENT_CLASS (GNOME_CANVAS_ITEM_CLASS, draw,
				    (item, drawable,
				     drawable_corner_x, drawable_corner_y,
				     drawable_width, drawable_height));
}
