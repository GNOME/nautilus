/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-buffered-widget.h - A buffered widget for alpha compositing.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-buffered-widget.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-string.h"
#include "nautilus-background.h"

#include <gtk/gtksignal.h>

#include <librsvg/rsvg-ft.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_alphagamma.h>
#include <libart_lgpl/art_affine.h>

#include <librsvg/art_render.h>
#include <librsvg/art_render_mask.h>

#include <math.h>
#include <string.h>
#include <png.h>

/* Arguments */
enum
{
	ARG_0,
	ARG_BACKGROUND_COLOR,
	ARG_BACKGROUND_TYPE,
	ARG_BUFFERED_WIDGET,
	ARG_PLACEMENT_TYPE,
};

/* Detail member struct */
struct _NautilusBufferedWidgetDetail
{
	GdkGC			*copy_area_gc;
	GdkPixbuf		*buffer_pixbuf;
	GdkPixbuf		*tile_pixbuf;
};

/* GdkGC refcounting macros */			\
#define NAUTILUS_GDK_GC_UNREF_IF(_gc)		\
NAUTILUS_MACRO_BEGIN				\
        if ((_gc) != NULL) {			\
	        gdk_gc_unref (_gc);		\
		(_gc) = NULL;			\
	}					\
NAUTILUS_MACRO_END

#define NAUTILUS_GDK_GC_REF_IF(_gc)		\
NAUTILUS_MACRO_BEGIN				\
        if ((_gc) != NULL) {			\
	        gdk_gc_ref (_gc);		\
	}					\
NAUTILUS_MACRO_END

/* GtkObjectClass methods */
static void       nautilus_buffered_widget_initialize_class     (NautilusBufferedWidgetClass  *buffered_widget_class);
static void       nautilus_buffered_widget_initialize           (NautilusBufferedWidget       *buffered_widget);
static void       nautilus_buffered_widget_destroy              (GtkObject                    *object);
static void       nautilus_buffered_widget_set_arg              (GtkObject                    *object,
								 GtkArg                       *arg,
								 guint                         arg_id);
static void       nautilus_buffered_widget_get_arg              (GtkObject                    *object,
								 GtkArg                       *arg,
								 guint                         arg_id);

/* GtkWidgetClass methods */
static void       nautilus_buffered_widget_realize              (GtkWidget                    *widget);
static void       nautilus_buffered_widget_draw                 (GtkWidget                    *widget,
								 GdkRectangle                 *area);
static void       nautilus_buffered_widget_size_allocate        (GtkWidget                    *widget,
								 GtkAllocation                *allocation);

/* GtkWidgetClass event methods */
static gint       nautilus_buffered_widget_expose               (GtkWidget                    *widget,
								 GdkEventExpose               *event);
/* Private NautilusBufferedWidget things */
static void       background_appearance_changed_callback        (NautilusBackground           *background,
								 gpointer                      callback_data);
static GdkPixbuf* buffered_widget_create_pixbuf_from_background (const NautilusBufferedWidget *buffered_widget,
								 GdkGC                        *gc);
static void       buffered_widget_update_pixbuf                 (NautilusBufferedWidget       *buffered_widget);
static GtkWidget *nautilus_gtk_widget_find_background_ancestor  (GtkWidget                    *widget);
static void       nautilus_gdk_pixbuf_tile_alpha                (GdkPixbuf                    *pixbuf,
								 const GdkPixbuf              *tile_pixbuf,
								 guint                         tile_width,
								 guint                         tile_height,
								 gint                          tile_origin_x,
								 gint                          tile_origin_y,
								 GdkInterpType                 interpolation_mode,
								 guchar                        overall_alpha);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBufferedWidget, nautilus_buffered_widget, GTK_TYPE_MISC)

/* Class init methods */
static void
nautilus_buffered_widget_initialize_class (NautilusBufferedWidgetClass *buffered_widget_class)
{
	GtkObjectClass		*object_class = GTK_OBJECT_CLASS (buffered_widget_class);
	GtkWidgetClass		*widget_class = GTK_WIDGET_CLASS (buffered_widget_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_buffered_widget_destroy;
	object_class->set_arg = nautilus_buffered_widget_set_arg;
	object_class->get_arg = nautilus_buffered_widget_get_arg;

	/* GtkWidgetClass */
 	widget_class->realize = nautilus_buffered_widget_realize;
	widget_class->draw = nautilus_buffered_widget_draw;
	widget_class->expose_event = nautilus_buffered_widget_expose;
	widget_class->size_allocate = nautilus_buffered_widget_size_allocate;

	/* NautilusBufferedWidgetClass */
	buffered_widget_class->render_buffer_pixbuf = NULL;
}

void
nautilus_buffered_widget_initialize (NautilusBufferedWidget *buffered_widget)
{
	GTK_WIDGET_UNSET_FLAGS (buffered_widget, GTK_CAN_FOCUS);

	GTK_WIDGET_SET_FLAGS (buffered_widget, GTK_NO_WINDOW);

	buffered_widget->detail = g_new (NautilusBufferedWidgetDetail, 1);

	buffered_widget->detail->copy_area_gc = NULL;
	buffered_widget->detail->buffer_pixbuf = NULL;
	buffered_widget->detail->tile_pixbuf = NULL;
}

/* GtkObjectClass methods */
static void
nautilus_buffered_widget_destroy (GtkObject *object)
{
 	NautilusBufferedWidget *buffered_widget;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (object));

	buffered_widget = NAUTILUS_BUFFERED_WIDGET (object);

	NAUTILUS_GDK_GC_UNREF_IF (buffered_widget->detail->copy_area_gc);
	nautilus_gdk_pixbuf_unref_if_not_null (buffered_widget->detail->buffer_pixbuf);
	buffered_widget->detail->buffer_pixbuf = NULL;
	nautilus_gdk_pixbuf_unref_if_not_null (buffered_widget->detail->tile_pixbuf);
	buffered_widget->detail->tile_pixbuf = NULL;
	NAUTILUS_GDK_GC_UNREF_IF (buffered_widget->detail->copy_area_gc);

	g_free (buffered_widget->detail);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_buffered_widget_set_arg (GtkObject	*object,
				  GtkArg	*arg,
				  guint	arg_id)
{
	NautilusBufferedWidget		*buffered_widget;

 	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (object));

 	buffered_widget = NAUTILUS_BUFFERED_WIDGET (object);

#if 0
 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		buffered_widget->detail->placement_type = GTK_VALUE_ENUM (*arg);
		break;

 	default:
		g_assert_not_reached ();
	}
#endif
}

static void
nautilus_buffered_widget_get_arg (GtkObject	*object,
			GtkArg		*arg,
			guint		arg_id)
{
	NautilusBufferedWidget	*buffered_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (object));
	
	buffered_widget = NAUTILUS_BUFFERED_WIDGET (object);

#if 0
 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		GTK_VALUE_ENUM (*arg) = buffered_widget->detail->placement_type;
		break;
		
 	default:
		g_assert_not_reached ();
	}
#endif
}

/* GtkWidgetClass methods */
static void
nautilus_buffered_widget_realize (GtkWidget *widget)
{
	GtkWidget		*background_ancestor;
	NautilusBufferedWidget	*buffered_widget;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (widget));
	
	buffered_widget = NAUTILUS_BUFFERED_WIDGET (widget);

	/* Chain realize */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

	/* Create GCs */
	buffered_widget->detail->copy_area_gc = nautilus_gdk_create_copy_area_gc (widget->window);

	background_ancestor = nautilus_gtk_widget_find_background_ancestor (widget);

	if (background_ancestor != NULL) {
		NautilusBackground	*background;

		background = nautilus_get_widget_background (background_ancestor);
		g_assert (NAUTILUS_IS_BACKGROUND (background));
		
		gtk_signal_connect (GTK_OBJECT (background),
				    "appearance_changed",
				    background_appearance_changed_callback,
				    GTK_OBJECT (buffered_widget));
	}
	else {
		/* g_print ("%s: No background found.\n", __FUNCTION__); */

		/* FIXME: In this case, we should set a flag that indicates
		 * we need to check later for the precense of a background.
		 * Otherwise, we wont get background changes notifications,
		 * if the background gets attatched after we have been 
		 * realized.
		 *
		 * Users of this code can easily work around this problem
		 * by attatching a background before the widget is realized,
		 * which is usually the case.
		 */
	}
}

static void
nautilus_buffered_widget_draw (GtkWidget *widget, GdkRectangle *area)
{
	NautilusBufferedWidget	*buffered_widget;
	GdkPoint	source_point;
	GdkRectangle	destination_area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (widget));
	g_return_if_fail (area != NULL);
	g_return_if_fail (GTK_WIDGET_REALIZED (widget));

	buffered_widget = NAUTILUS_BUFFERED_WIDGET (widget);

 	if (buffered_widget->detail->buffer_pixbuf == NULL) {
		buffered_widget_update_pixbuf (buffered_widget);
 	}

	source_point.x = 0;
	source_point.y = 0;

	destination_area.x = widget->allocation.x;
	destination_area.y = widget->allocation.y;
	destination_area.width = widget->allocation.width;
	destination_area.height = widget->allocation.height;

	nautilus_gdk_pixbuf_render_to_drawable (buffered_widget->detail->buffer_pixbuf,
						widget->window,
						buffered_widget->detail->copy_area_gc,
						&source_point,
						&destination_area,
						GDK_INTERP_BILINEAR);
}

static void
nautilus_buffered_widget_size_allocate (GtkWidget *widget, GtkAllocation* allocation)
{
	NautilusBufferedWidget *buffered_widget;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (widget));
	g_return_if_fail (allocation != NULL);

	buffered_widget = NAUTILUS_BUFFERED_WIDGET (widget);

	/* Assign the new allocation */
	widget->allocation.x = allocation->x;
	widget->allocation.y = allocation->y;
	widget->allocation.width = MAX (1, allocation->width);
	widget->allocation.height = MAX (1, allocation->height);

	nautilus_gdk_pixbuf_unref_if_not_null (buffered_widget->detail->buffer_pixbuf);
	buffered_widget->detail->buffer_pixbuf = NULL;
}

static gint
nautilus_buffered_widget_expose (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusBufferedWidget	*buffered_widget;
	
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (widget), FALSE);
	
	buffered_widget = NAUTILUS_BUFFERED_WIDGET (widget);
	
	nautilus_buffered_widget_draw (widget, &event->area);
	
	return TRUE;
}

static GtkWidget *
nautilus_gtk_widget_find_background_ancestor (GtkWidget *widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	while (widget != NULL) {
		if (nautilus_widget_has_attached_background (widget)) {
			return widget;
		}

		widget = widget->parent;
	}

	return NULL;
}

static void
nautilus_gdk_pixbuf_tile_alpha (GdkPixbuf		*pixbuf,
				const GdkPixbuf		*tile_pixbuf,
				guint			tile_width,
				guint			tile_height,
				gint			tile_origin_x,
				gint			tile_origin_y,
				GdkInterpType		interpolation_mode,
				guchar			overall_alpha)
{
	gint	x;
	gint	y;
	guchar	*pixels;
	guchar	*tile_pixels;
	guint	num_ver_iterations;
	guint	num_hor_iterations;
	guint	i;
	guint	j;
	guint	width;
	guint	height;
	
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (tile_pixbuf != NULL);
	g_return_if_fail (tile_width <= gdk_pixbuf_get_width (tile_pixbuf));
	g_return_if_fail (tile_height <= gdk_pixbuf_get_height (tile_pixbuf));
	g_return_if_fail (gdk_pixbuf_get_pixels (pixbuf) != NULL);
	g_return_if_fail (gdk_pixbuf_get_pixels (tile_pixbuf) != NULL);

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	tile_pixels = gdk_pixbuf_get_pixels (tile_pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	num_ver_iterations = ceil (height / tile_height) + 1;
	num_hor_iterations = ceil (width / tile_width) + 1;

	y = 0;

	for (j = 0; j < num_ver_iterations; j++)
	{
		x = 0;

		for (i = 0; i < num_hor_iterations; i++)
		{
			guint   copy_width;
			guint   copy_height;
			gint    dst_x;
			gint    dst_y;
			gint    dst_x2;
			gint    dst_y2;

			dst_x = x;
			dst_y = y;

			copy_width = tile_width;
			copy_height = tile_height;

			dst_x2 = dst_x + copy_width;
			dst_y2 = dst_y + copy_height;

			if (dst_x2 > width)
			{
				copy_width -= (dst_x2 - width);
			}

			if (dst_y2 > height)
			{
				copy_height -= (dst_y2 - height);
			}

			if (copy_width > 0 && copy_height > 0)
			{
				GdkRectangle destination_area;

				destination_area.x = dst_x;
				destination_area.y = dst_y;
				destination_area.width = copy_width;
				destination_area.height = copy_height;
				
				nautilus_gdk_pixbuf_render_to_pixbuf_alpha (tile_pixbuf,
									    pixbuf,
									    &destination_area,
									    interpolation_mode,
									    overall_alpha);
			}

			x += tile_width;
		}

		y += tile_height;
	}
}

/* Private NautilusBufferedWidget things */
static void
buffered_widget_update_pixbuf (NautilusBufferedWidget *buffered_widget)
{
	GtkWidget	*widget;
	GdkPixbuf	*pixbuf_without_alpha;
	ArtIRect	clip_rect;
	GdkPoint	destination_point;
	NautilusBufferedWidgetClass *buffered_widget_class;

	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (buffered_widget));

	widget = GTK_WIDGET (buffered_widget);

	nautilus_gdk_pixbuf_unref_if_not_null (buffered_widget->detail->buffer_pixbuf);

	pixbuf_without_alpha = buffered_widget_create_pixbuf_from_background (buffered_widget, 
									      buffered_widget->detail->copy_area_gc);
	buffered_widget->detail->buffer_pixbuf = gdk_pixbuf_add_alpha (pixbuf_without_alpha, FALSE, 0, 0, 0);
	
	gdk_pixbuf_unref (pixbuf_without_alpha);

	g_assert (buffered_widget->detail->buffer_pixbuf != NULL);

	clip_rect.x0 = 0;
	clip_rect.y0 = 0;
	
	clip_rect.x1 = widget->allocation.width;
	clip_rect.y1 = widget->allocation.height;

	destination_point.x = 0;
	destination_point.y = 0;

	buffered_widget_class = NAUTILUS_BUFFERED_WIDGET_CLASS (NAUTILUS_CLASS (buffered_widget));
	g_assert (buffered_widget_class != NULL);
	g_assert (buffered_widget_class->render_buffer_pixbuf != NULL);

	if (buffered_widget->detail->tile_pixbuf != NULL) {
		nautilus_gdk_pixbuf_tile_alpha (buffered_widget->detail->buffer_pixbuf,
						buffered_widget->detail->tile_pixbuf,
						gdk_pixbuf_get_width (buffered_widget->detail->tile_pixbuf),
						gdk_pixbuf_get_height (buffered_widget->detail->tile_pixbuf),
						0,
						0,
						GDK_INTERP_BILINEAR,
						255); /* image->detail->overall_alpha */
	}

  	NAUTILUS_CALL_VIRTUAL (NAUTILUS_BUFFERED_WIDGET_CLASS, buffered_widget, render_buffer_pixbuf, 
  			       (buffered_widget, buffered_widget->detail->buffer_pixbuf));
}

static GdkPixbuf*
buffered_widget_create_pixbuf_from_background (const NautilusBufferedWidget *buffered_widget, 
					       GdkGC		*gc)
{
	GtkWidget		*widget;
	GdkPixbuf		*pixbuf;
	GtkWidget		*background_ancestor;
	
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (buffered_widget), NULL);
	g_return_val_if_fail (gc != NULL, NULL);

	widget = GTK_WIDGET (buffered_widget);

	background_ancestor = nautilus_gtk_widget_find_background_ancestor (widget);

	if (background_ancestor != NULL) {
		NautilusBackground	*background;
		GdkPixmap		*pixmap;
		GdkRectangle		background_area;

		background = nautilus_get_widget_background (background_ancestor);
		g_assert (NAUTILUS_IS_BACKGROUND (background));
		
		background_area.x = 0;
		background_area.y = 0;
		background_area.width = background_ancestor->allocation.width;
		background_area.height = background_ancestor->allocation.height;
		
		pixmap = gdk_pixmap_new (widget->window, background_area.width, background_area.height, -1);
		
		nautilus_background_draw (background, pixmap, gc, &background_area, 0, 0);
		
		pixbuf = gdk_pixbuf_get_from_drawable (NULL,
						       pixmap,
						       gdk_rgb_get_cmap (),
						       widget->allocation.x,
						       widget->allocation.y,
						       0,
						       0,
						       widget->allocation.width,
						       widget->allocation.height);

		gdk_pixmap_unref (pixmap);
	}
	else {
		GdkPixmap *pixmap;

		pixmap = gdk_pixmap_new (widget->window, widget->allocation.width, widget->allocation.height, -1);
		
		gtk_paint_box (widget->style,
			       pixmap,
			       GTK_STATE_NORMAL,
			       GTK_SHADOW_NONE,
			       NULL,
			       widget,
			       "entry_bg",
			       0,
			       0,
			       widget->allocation.width,
			       widget->allocation.height);
		
		pixbuf = gdk_pixbuf_get_from_drawable (NULL,
						       pixmap,
						       gdk_rgb_get_cmap (),
						       0,
						       0,
						       0,
						       0,
						       widget->allocation.width,
						       widget->allocation.height);
	}
	
	g_assert (pixbuf != NULL);

	return pixbuf;
}

static void
background_appearance_changed_callback (NautilusBackground *background, 
					gpointer callback_data)
{
	NautilusBufferedWidget *buffered_widget;

	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (callback_data));

	buffered_widget = NAUTILUS_BUFFERED_WIDGET (callback_data);

	nautilus_buffered_widget_clear_buffer (buffered_widget);

	gtk_widget_queue_draw (GTK_WIDGET (buffered_widget));
}

/**
 * nautilus_buffered_widget_clear_buffer:
 *
 * @buffered_widget: A NautilusBufferedWidget
 *
 * Clear the internal buffer so that the next time the widget is drawn,
 * the buffer will be re-composited.  This is useful when you've manually
 * done something to the widget that changes it appearance.  This is mostly
 * useful for subclasses.
 */
void
nautilus_buffered_widget_clear_buffer (NautilusBufferedWidget *buffered_widget)
{
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (buffered_widget));
	
	nautilus_gdk_pixbuf_unref_if_not_null (buffered_widget->detail->buffer_pixbuf);
	buffered_widget->detail->buffer_pixbuf = NULL;
}

/**
 * nautilus_buffered_widget_set_tile_pixbuf:
 *
 * @buffered_widget: A NautilusBufferedWidget
 * @pixbuf:          The new tile pixbuf
 *
 * Change the tile pixbuf.  A 'pixbuf' value of NULL, means dont use a
 * tile pixbuf - this is the default behavior for the widget.
 */
void
nautilus_buffered_widget_set_tile_pixbuf (NautilusBufferedWidget          *buffered_widget,
					  GdkPixbuf              *pixbuf)
{
	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (buffered_widget));

	if (pixbuf != buffered_widget->detail->tile_pixbuf)
	{
		nautilus_gdk_pixbuf_unref_if_not_null (buffered_widget->detail->tile_pixbuf);
		
		nautilus_gdk_pixbuf_ref_if_not_null (pixbuf);
		
		buffered_widget->detail->tile_pixbuf = pixbuf;
	}

	gtk_widget_queue_resize (GTK_WIDGET (buffered_widget));
}

/**
 * nautilus_buffered_widget_get_tile_pixbuf:
 *
 * @buffered_widget: A NautilusBufferedWidget
 *
 * Return value: A reference to the tile_pixbuf.  Needs to be unreferenced with 
 * gdk_pixbuf_unref()
 */
GdkPixbuf*
nautilus_buffered_widget_get_tile_pixbuf (const NautilusBufferedWidget *buffered_widget)
{
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (buffered_widget), NULL);

	nautilus_gdk_pixbuf_ref_if_not_null (buffered_widget->detail->tile_pixbuf);
	
	return buffered_widget->detail->tile_pixbuf;
}
