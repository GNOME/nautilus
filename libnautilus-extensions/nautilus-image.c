/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-image.c - A widget to display a composited pixbuf.

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
#include "nautilus-image.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"

#include <math.h>
#include <string.h>

/* FIXME bugzilla.eazel.com 1612: 
 * We should use NautilusBackground for the background.  This will simplify
 * lots of things, be more efficient, and remove the need for a lot of the
 * tiling code.
 */

/* Arguments */
enum
{
	ARG_0,
	ARG_BACKGROUND_COLOR,
	ARG_BACKGROUND_PIXBUF,
	ARG_BACKGROUND_TYPE,
	ARG_IMAGE,
	ARG_PLACEMENT_TYPE,
};

/* Detail member struct */
struct _NautilusImageDetail
{
	/* Attributes */
	NautilusImagePlacementType	placement_type;
	NautilusImageBackgroundType	background_type;
	GdkPixbuf			*background_pixbuf;
	guint32				background_color;
	GdkPixbuf			*pixbuf;
	GdkPoint			background_tile_origin;
	guchar				overall_alpha;
	gboolean			background_tile_screen_relative;

	gchar				*label_text;
	GdkFont				*label_font;

	GdkGC				*copy_area_gc;
	GdkPixbuf			*buffer;

	/* Offsets */
	guint				left_offset;
	guint				right_offset;
	guint				top_offset;
	guint				bottom_offset;

	guint				extra_width;
	guint				extra_height;
};

/* GtkObjectClass methods */
static void        nautilus_image_initialize_class            (NautilusImageClass *image_class);
static void        nautilus_image_initialize                  (NautilusImage      *image);
static void        nautilus_image_destroy                     (GtkObject          *object);
static void        nautilus_image_set_arg                     (GtkObject          *object,
							       GtkArg             *arg,
							       guint               arg_id);
static void        nautilus_image_get_arg                     (GtkObject          *object,
							       GtkArg             *arg,
							       guint               arg_id);



/* GtkWidgetClass methods */
static void        nautilus_image_map                         (GtkWidget          *widget);
static void        nautilus_image_unmap                       (GtkWidget          *widget);
static void        nautilus_image_realize                     (GtkWidget          *widget);
static void        nautilus_image_unrealize                   (GtkWidget          *widget);
static void        nautilus_image_draw                        (GtkWidget          *widget,
							       GdkRectangle       *area);
static void        nautilus_image_size_request                (GtkWidget          *widget,
							       GtkRequisition     *requisition);
static void        nautilus_image_size_allocate               (GtkWidget          *widget,
							       GtkAllocation      *allocation);


/* GtkWidgetClass event methods */
static gint        nautilus_image_expose                      (GtkWidget          *widget,
							       GdkEventExpose     *event);

/* Private NautilusImage things */
static void        ensure_buffer_size                         (NautilusImage      *image,
							       guint               width,
							       guint               height);
static GdkGC *     nautilus_gdk_create_copy_area_gc           (GdkWindow          *window);
static void        nautilus_gdk_pixbuf_render_to_drawable     (const GdkPixbuf    *pixbuf,
							       GdkDrawable        *drawable,
							       GdkGC              *gc,
							       const GdkPoint     *source_point,
							       const GdkRectangle *destination_area,
							       GdkRgbDither        dither);
static void        nautilus_gdk_pixbuf_render_to_pixbuf       (const GdkPixbuf    *pixbuf,
							       GdkPixbuf          *destination_pixbuf,
							       const GdkPoint     *source_point,
							       const GdkRectangle *destination_area);
static void        nautilus_gdk_pixbuf_render_to_pixbuf_alpha (const GdkPixbuf    *pixbuf,
							       GdkPixbuf          *destination_pixbuf,
							       const GdkRectangle *destination_area,
							       GdkInterpType       interpolation_mode,
							       guchar              overall_alpha);
static void        gdk_string_dimensions                      (const GdkFont      *font,
							       const gchar        *string,
							       GtkRequisition     *size);
static void        nautilus_gdk_pixbuf_set_to_color           (GdkPixbuf          *pixbuf,
							       guint32             color);
static void        nautilus_gdk_pixbuf_tile                   (GdkPixbuf          *pixbuf,
							       const GdkPixbuf    *tile_pixbuf,
							       guint               tile_width,
							       guint               tile_height,
							       gint                tile_origin_x,
							       gint                tile_origin_y);
static void        nautilus_gdk_pixbuf_tile_alpha             (GdkPixbuf          *pixbuf,
							       const GdkPixbuf    *tile_pixbuf,
							       guint               tile_width,
							       guint               tile_height,
							       gint                tile_origin_x,
							       gint                tile_origin_y,
							       GdkInterpType       interpolation_mode,
							       guchar              overall_alpha);
#define NAUTILUS_ALPHA_NONE 255

#define NAUTILUS_MACRO_BEGIN		G_STMT_START {
#define NAUTILUS_MACRO_END		} G_STMT_END

/* GdkPixbuf refcounting macros */
#define NAUTILUS_GDK_PIXBUF_UNREF_IF(_pixbuf)	\
NAUTILUS_MACRO_BEGIN				\
        if ((_pixbuf) != NULL) {		\
	        gdk_pixbuf_unref (_pixbuf);	\
		(_pixbuf) = NULL;		\
	}					\
NAUTILUS_MACRO_END

#define NAUTILUS_GDK_PIXBUF_REF_IF(_pixbuf)	\
NAUTILUS_MACRO_BEGIN				\
        if ((_pixbuf) != NULL) {		\
	        gdk_pixbuf_ref (_pixbuf);	\
	}					\
NAUTILUS_MACRO_END

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

/* GdkFont refcounting macros */
#define NAUTILUS_GDK_FONT_UNREF_IF(_font)	\
NAUTILUS_MACRO_BEGIN				\
        if ((_font) != NULL) {			\
	        gdk_font_unref (_font);		\
		(_font) = NULL;			\
	}					\
NAUTILUS_MACRO_END

#define NAUTILUS_GDK_FONT_REF_IF(_font)		\
NAUTILUS_MACRO_BEGIN				\
        if ((_font) != NULL) {			\
	        gdk_font_ref (_font);		\
	}					\
NAUTILUS_MACRO_END

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusImage, nautilus_image, GTK_TYPE_MISC)

/* Class init methods */
static void
nautilus_image_initialize_class (NautilusImageClass *image_class)
{
	GtkObjectClass		*object_class = GTK_OBJECT_CLASS (image_class);
	GtkWidgetClass		*widget_class = GTK_WIDGET_CLASS (image_class);

	/* Arguments */
	gtk_object_add_arg_type ("NautilusImage::placement_type",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_PLACEMENT_TYPE);

	gtk_object_add_arg_type ("NautilusImage::background_type",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_TYPE);

	gtk_object_add_arg_type ("NautilusImage::background_pixbuf",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_PIXBUF);

	gtk_object_add_arg_type ("NautilusImage::background_color",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_COLOR);

	gtk_object_add_arg_type ("NautilusImage::image",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE,
				 ARG_IMAGE);

	/* GtkObjectClass */
	object_class->destroy = nautilus_image_destroy;
	object_class->set_arg = nautilus_image_set_arg;
	object_class->get_arg = nautilus_image_get_arg;

	/* GtkWidgetClass */
 	widget_class->realize = nautilus_image_realize;
 	widget_class->unrealize = nautilus_image_unrealize;
	widget_class->draw = nautilus_image_draw;
	widget_class->map = nautilus_image_map;
	widget_class->unmap = nautilus_image_unmap;
	widget_class->expose_event = nautilus_image_expose;
	widget_class->size_request = nautilus_image_size_request;
	widget_class->size_allocate = nautilus_image_size_allocate;
}

void
nautilus_image_initialize (NautilusImage *image)
{
	GTK_WIDGET_SET_FLAGS (image, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (image, GTK_NO_WINDOW);

	image->detail = g_new (NautilusImageDetail, 1);

	image->detail->placement_type = NAUTILUS_IMAGE_PLACEMENT_CENTER;

	image->detail->background_color = NAUTILUS_RGB_COLOR_WHITE;
	image->detail->background_type = NAUTILUS_IMAGE_BACKGROUND_SOLID;
	image->detail->background_pixbuf = NULL;

	image->detail->pixbuf = NULL;

	image->detail->buffer = NULL;
	image->detail->copy_area_gc = NULL;
	image->detail->label_text = NULL;
	image->detail->label_font = gdk_font_load ("fixed");

	image->detail->background_tile_origin.x = 0;
	image->detail->background_tile_origin.y = 0;
	image->detail->overall_alpha = NAUTILUS_ALPHA_NONE;
	image->detail->background_tile_screen_relative = TRUE;

	image->detail->left_offset = 0;
	image->detail->right_offset = 0;
	image->detail->top_offset = 0;
	image->detail->bottom_offset = 0;

	image->detail->extra_width = 0;
	image->detail->extra_height = 0;
}

/* GtkObjectClass methods */
static void
nautilus_image_destroy (GtkObject *object)
{
 	NautilusImage *image;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (object));

	image = NAUTILUS_IMAGE (object);

	NAUTILUS_GDK_GC_UNREF_IF (image->detail->copy_area_gc);
	NAUTILUS_GDK_FONT_UNREF_IF (image->detail->label_font);

	NAUTILUS_GDK_PIXBUF_UNREF_IF (image->detail->buffer);
	NAUTILUS_GDK_PIXBUF_UNREF_IF (image->detail->background_pixbuf);
	NAUTILUS_GDK_PIXBUF_UNREF_IF (image->detail->pixbuf);

	g_free (image->detail->label_text);

	g_free (image->detail);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_image_set_arg (GtkObject	*object,
			GtkArg		*arg,
			guint		arg_id)
{
	NautilusImage		*image;

 	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (object));

 	image = NAUTILUS_IMAGE (object);

 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		image->detail->placement_type = GTK_VALUE_ENUM (*arg);
		break;

	case ARG_BACKGROUND_TYPE:
		image->detail->background_type = GTK_VALUE_ENUM (*arg);
		break;

	case ARG_BACKGROUND_PIXBUF:
		nautilus_image_set_background_pixbuf (image, (GdkPixbuf*) GTK_VALUE_OBJECT (*arg));
		break;

	case ARG_BACKGROUND_COLOR:
		image->detail->background_color = GTK_VALUE_UINT (*arg);
		break;

	case ARG_IMAGE:
		nautilus_image_set_pixbuf (image, (GdkPixbuf*) GTK_VALUE_OBJECT (*arg));
		break;

 	default:
		g_assert_not_reached ();
	}
}

static void
nautilus_image_get_arg (GtkObject	*object,
			GtkArg		*arg,
			guint		arg_id)
{
	NautilusImage	*image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (object));
	
	image = NAUTILUS_IMAGE (object);

 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		GTK_VALUE_ENUM (*arg) = image->detail->placement_type;
		break;
		
	case ARG_BACKGROUND_TYPE:
		GTK_VALUE_ENUM (*arg) = image->detail->background_type;
		break;

	case ARG_BACKGROUND_PIXBUF:
		GTK_VALUE_OBJECT (*arg) = (GtkObject *) nautilus_image_get_background_pixbuf (image);
		break;

	case ARG_BACKGROUND_COLOR:
		GTK_VALUE_UINT (*arg) = image->detail->background_color;
		break;

	case ARG_IMAGE:
		GTK_VALUE_OBJECT (*arg) = (GtkObject *) nautilus_image_get_pixbuf (image);
		break;

 	default:
		g_assert_not_reached ();
	}
}

/* GtkWidgetClass methods */
static void
nautilus_image_realize (GtkWidget *widget)
{
	NautilusImage	*image;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	
	image = NAUTILUS_IMAGE (widget);

	/* Chain realize */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

	/* Create GCs */
	image->detail->copy_area_gc = nautilus_gdk_create_copy_area_gc (widget->window);
}

static void
nautilus_image_unrealize (GtkWidget *widget)
{
	NautilusImage	*image;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	
	image = NAUTILUS_IMAGE (widget);

	NAUTILUS_GDK_GC_UNREF_IF (image->detail->copy_area_gc);

	/* Chain unrealize */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, unrealize, (widget));
}

static void
nautilus_image_draw (GtkWidget *widget, GdkRectangle *area)
{
	NautilusImage	*image;
	GdkPoint	source_point;
 	GdkRectangle	destination_area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (area != NULL);

	image = NAUTILUS_IMAGE (widget);

	source_point.x = area->x;
	source_point.y = area->y;
	
 	destination_area.x = area->x;
 	destination_area.y = area->y;
 	destination_area.width = widget->allocation.width - area->x;
 	destination_area.height = widget->allocation.height - area->y;

	nautilus_gdk_pixbuf_render_to_drawable (image->detail->buffer,
						widget->window,
						image->detail->copy_area_gc,
						&source_point,
						&destination_area,
						GDK_INTERP_NEAREST);
}

static void
nautilus_image_size_allocate (GtkWidget *widget, GtkAllocation* allocation)
{
	NautilusImage *image;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (allocation != NULL);

	image = NAUTILUS_IMAGE (widget);

	/* Assign the new allocation */
	widget->allocation.x = allocation->x;
	widget->allocation.y = allocation->y;
	widget->allocation.width = MAX (1, allocation->width);
	widget->allocation.height = MAX (1, allocation->height);

	/* If the widget is realized, move and resize its window */
	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window,
					allocation->x, 
					allocation->y,
					allocation->width, 
					allocation->height);

		if (image->detail->background_tile_screen_relative)
		{
			GdkWindow	*top_level;
			gint	top_level_x;
			gint	top_level_y;
			gint	image_x;
			gint	image_y;
			

			top_level = gdk_window_get_toplevel (widget->window);
			g_assert (top_level != NULL);

			gdk_window_get_origin (top_level, &top_level_x, &top_level_y);
			gdk_window_get_origin (widget->window, &image_x, &image_y);

#if 0
			g_print ("2 %p:top_level = (%d,%d), %p:image = (%d,%d), diff = (%d,%d)\n",
				 top_level,
				 top_level_x,
				 top_level_y,
				 widget->window,
				 image_x,
				 image_y,
				 top_level_x - image_x,
				 top_level_y - image_y);
#endif

 			image->detail->background_tile_origin.x = ABS (top_level_x - image_x);
 			image->detail->background_tile_origin.y = ABS (top_level_y - image_y);
		}
	}

	ensure_buffer_size (image, allocation->width, allocation->height);

	switch (image->detail->background_type)
	{
	case NAUTILUS_IMAGE_BACKGROUND_PIXBUF:
		if (image->detail->background_pixbuf != NULL)
		{
			nautilus_gdk_pixbuf_tile (image->detail->buffer,
						  image->detail->background_pixbuf,
						  gdk_pixbuf_get_width (image->detail->background_pixbuf),
						  gdk_pixbuf_get_height (image->detail->background_pixbuf),
						  image->detail->background_tile_origin.x, 
						  image->detail->background_tile_origin.y);
		}
		break;

	case NAUTILUS_IMAGE_BACKGROUND_SOLID:
		nautilus_gdk_pixbuf_set_to_color (image->detail->buffer, image->detail->background_color);
		break;
	}

	if (image ->detail->pixbuf != NULL)
	{
		switch (image->detail->placement_type)
		{
		case NAUTILUS_IMAGE_PLACEMENT_CENTER:
		{
			gint x;
			gint y;
			
			x = (widget->allocation.width - gdk_pixbuf_get_width (image->detail->pixbuf)) / 2;
			y = (widget->allocation.height - gdk_pixbuf_get_height (image->detail->pixbuf)) / 2;
			
			gdk_pixbuf_composite (image->detail->pixbuf,
					      image->detail->buffer, 
					      x,
					      y,
					      gdk_pixbuf_get_width (image->detail->pixbuf),
					      gdk_pixbuf_get_height (image->detail->pixbuf),
					      (double) x,
					      (double) y,
					      1.0,
					      1.0,
					      GDK_INTERP_BILINEAR,
					      image->detail->overall_alpha);
		}
		break;
		
		case NAUTILUS_IMAGE_PLACEMENT_TILE:
		{
			nautilus_gdk_pixbuf_tile_alpha (image->detail->buffer,
							image->detail->pixbuf,
							gdk_pixbuf_get_width (image->detail->pixbuf),
							gdk_pixbuf_get_height (image->detail->pixbuf),
							0,
							0,
							GDK_INTERP_BILINEAR,
							image->detail->overall_alpha);
		}
		break;
		}
	}

	if (image ->detail->label_text != NULL)
	{
		GtkRequisition text_size;
		gint x;
		gint y;
		ArtIRect text_rect;
			
		g_assert (image->detail->label_font != NULL);
		
		gdk_string_dimensions (image->detail->label_font,
				       image ->detail->label_text,
				       &text_size);

		x = widget->allocation.width - text_size.width - image->detail->right_offset;
		y = image->detail->top_offset;

// 		x = (widget->allocation.width - text_size.width) / 2;
//  		y = (widget->allocation.height - text_size.height) / 2;

		text_rect.x0 = x;
		text_rect.y0 = y;
		text_rect.x1 = x + text_size.width;
		text_rect.y1 = y + text_size.height;

/* FIXME bugzilla.eazel.com xxxx: 
 * Need to be able to pass in a rgb colot into the draw_text function.
 */
		nautilus_gdk_pixbuf_draw_text_white (image->detail->buffer,
						     image->detail->label_font,
						     &text_rect,
						     image->detail->label_text,
						     image->detail->overall_alpha);
	}
}

static void
nautilus_image_size_request (GtkWidget	*widget,
			       GtkRequisition	*requisition)
{
	GtkRequisition	pixbuf_size;
	GtkRequisition	text_size;
	NautilusImage	*image;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (requisition != NULL);

	image = NAUTILUS_IMAGE (widget);
	
   	requisition->width = 10;
   	requisition->height = 10;

	pixbuf_size.width = 0;
	pixbuf_size.height = 0;
	text_size.width = 0;
	text_size.height = 0;
	
	if (image ->detail->pixbuf != NULL)
	{
		pixbuf_size.width = gdk_pixbuf_get_width (image->detail->pixbuf);
		pixbuf_size.height = gdk_pixbuf_get_height (image->detail->pixbuf);
	}

	if (image ->detail->label_text != NULL)
	{
		g_assert (image->detail->label_font != NULL);

		gdk_string_dimensions (image->detail->label_font,
				       image ->detail->label_text,
				       &text_size);
	}

   	requisition->width = 
		MAX (pixbuf_size.width, text_size.width) +
		image->detail->extra_width;

   	requisition->height = 
		MAX (pixbuf_size.height, text_size.height) +
		image->detail->extra_height;
}

static void
nautilus_image_map (GtkWidget *widget)
{
	NautilusImage *image;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	
	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

	image = NAUTILUS_IMAGE (widget);
	
	gdk_window_show (widget->window);
}

static void
nautilus_image_unmap (GtkWidget *widget)
{
	NautilusImage *image;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);

	image = NAUTILUS_IMAGE (widget);
	
	gdk_window_hide (widget->window);
}

static gint
nautilus_image_expose (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusImage	*image;
	
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (widget), FALSE);
	
	image = NAUTILUS_IMAGE (widget);
	
	nautilus_image_draw (widget, &event->area);
	
	return TRUE;
}

/* Private NautilusImage things */
static void
ensure_buffer_size (NautilusImage	*image,
		    guint		new_width,
		    guint		new_height)
{
	guint old_width = 0;
	guint old_height = 0;

	g_assert (image != NULL);
	g_assert (NAUTILUS_IS_IMAGE (image));

	if (new_width == 0 || new_height == 0) {
		return;
	}

	if (image->detail->buffer != NULL) {
		old_width = gdk_pixbuf_get_width (image->detail->buffer);
		old_height = gdk_pixbuf_get_height (image->detail->buffer);
	}
	
	if (old_width < new_width || old_height < new_height) {
		GdkPixbuf *new_pixbuf = NULL;

		new_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, new_width, new_height);
		
		if (image->detail->buffer) {
			gdk_pixbuf_unref (image->detail->buffer);
		}
		
		image->detail->buffer = new_pixbuf;
	}
}

static GdkGC *
nautilus_gdk_create_copy_area_gc (GdkWindow	*window)
{
	GdkGC *copy_area_gc;

	g_return_val_if_fail (window != NULL, NULL);

	copy_area_gc = gdk_gc_new (window);

	gdk_gc_set_function (copy_area_gc, GDK_COPY);

	return copy_area_gc;
}

static void
nautilus_gdk_pixbuf_render_to_drawable (const GdkPixbuf		*pixbuf,
					GdkDrawable		*drawable,
					GdkGC			*gc,
					const GdkPoint		*source_point,
					const GdkRectangle	*destination_area,
					GdkRgbDither		dither)
{
	GdkPoint	src;
	GdkRectangle	dst;
	GdkPoint	end;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (drawable != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (source_point != NULL);
	g_return_if_fail (destination_area != NULL);
 	g_return_if_fail (destination_area->width > 0);
 	g_return_if_fail (destination_area->height > 0);
	g_return_if_fail (source_point->x >= 0);
	g_return_if_fail (source_point->y >= 0);

	g_assert (gdk_pixbuf_get_width (pixbuf) > 0);
	g_assert (gdk_pixbuf_get_height (pixbuf) > 0);

	src = *source_point;
	dst = *destination_area;

	/* Clip to the left edge of the drawable */
	if (dst.x < 0)
	{
		src.x += ABS (dst.x);
		dst.x = 0;
	}

	/* Clip to the top edge of the drawable */
	if (dst.y < 0)
	{
		src.y += ABS (dst.y);
		dst.y = 0;
	}
	
	end.x = src.x + dst.width;
	end.y = src.y + dst.height;
	
	if (end.x >= gdk_pixbuf_get_width (pixbuf))
	{
		g_assert (dst.width >= (end.x - gdk_pixbuf_get_width (pixbuf)));

		dst.width -= (end.x - gdk_pixbuf_get_width (pixbuf));
	}

	if (end.y >= gdk_pixbuf_get_height (pixbuf))
	{
		g_assert (dst.height >= (end.y - gdk_pixbuf_get_height (pixbuf)));

		dst.height -= (end.y - gdk_pixbuf_get_height (pixbuf));
	}

	gdk_pixbuf_render_to_drawable ((GdkPixbuf *) pixbuf,
				       drawable,
				       gc,
				       src.x,
				       src.y,
				       dst.x,
				       dst.y,
				       dst.width,
				       dst.height,
				       dither,
				       0,
				       0);
}

static void
nautilus_gdk_pixbuf_render_to_pixbuf (const GdkPixbuf		*pixbuf,
				      GdkPixbuf			*destination_pixbuf,
				      const GdkPoint		*source_point,
				      const GdkRectangle	*destination_area)
{
	GdkPoint	src;
	GdkRectangle	dst;
	GdkPoint	end;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (source_point != NULL);
	g_return_if_fail (source_point->x >= 0);
	g_return_if_fail (source_point->y >= 0);
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (destination_area->width > 0);
	g_return_if_fail (destination_area->height > 0);

	src = *source_point;
	dst = *destination_area;

	/* Clip to the left edge of the drawable */
	if (dst.x < 0)
	{
		src.x += ABS (dst.x);
		dst.x = 0;
	}

	/* Clip to the top edge of the drawable */
	if (dst.y < 0)
	{
		src.y += ABS (dst.y);
		dst.y = 0;
	}
	
	end.x = src.x + dst.width;
	end.y = src.y + dst.height;
	
	if (end.x >= gdk_pixbuf_get_width (pixbuf))
	{
		g_assert (dst.width >= (end.x - gdk_pixbuf_get_width (pixbuf)));

		dst.width -= (end.x - gdk_pixbuf_get_width (pixbuf));
	}

	if (end.y >= gdk_pixbuf_get_height (pixbuf))
	{
		g_assert (dst.height >= (end.y - gdk_pixbuf_get_height (pixbuf)));

		dst.height -= (end.y - gdk_pixbuf_get_height (pixbuf));
	}

	gdk_pixbuf_copy_area ((GdkPixbuf *) pixbuf,
			      src.x,
			      src.y,
			      dst.width,
			      dst.height,
			      destination_pixbuf,
			      dst.x,
			      dst.y);
}

static void
nautilus_gdk_pixbuf_render_to_pixbuf_alpha (const GdkPixbuf        *pixbuf,
					    GdkPixbuf              *destination_pixbuf,
					    const GdkRectangle      *destination_area,
					    GdkInterpType     interpolation_mode,
					    guchar                    overall_alpha)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (destination_area->width > 0);
	g_return_if_fail (destination_area->height > 0);

	gdk_pixbuf_composite (pixbuf,
			      destination_pixbuf,
			      destination_area->x,
			      destination_area->y,
			      destination_area->width,
			      destination_area->height,
			      (double) destination_area->x,
			      (double) destination_area->y,
			      1.0,
			      1.0,
			      interpolation_mode,
			      overall_alpha);
}

static void
gdk_string_dimensions (const GdkFont	*font,
		       const gchar	*string,
		       GtkRequisition	*size)
{
	g_return_if_fail (font != NULL);
	g_return_if_fail (size != NULL);

	size->width = 0;
	size->height = 0;

	if (string && (strlen (string) > 0))
	{
		gint	width;
		gint	lbearing;
		gint	rbearing;
		gint	ascent;
		gint	descent;
		
		gdk_text_extents ((GdkFont *) font,
				  string,
				  strlen (string),
				  &lbearing,
				  &rbearing,
				  &width,
				  &ascent,
				  &descent);

		size->width = width;
		size->height = ascent + descent;
	}
}

static void
nautilus_gdk_pixbuf_set_to_color (GdkPixbuf	*pixbuf,
				  guint32     color)
{
	guchar			*pixels;
	guint			width;
	guint			height;
	guint			rowstride;
	guint			x;
	guint			y;
	guchar			*offset;

	g_return_if_fail (pixbuf != NULL);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	if (gdk_pixbuf_get_has_alpha (pixbuf))
	{
		for (y = 0; y < height; y++) 
		{
			offset = pixels + y * rowstride;
			
			for (x = 0; x < width; x++) 
			{
				*offset++ = NAUTILUS_RGBA_COLOR_GET_R (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_G (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_B (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_A (color);
			}
		}
	}
	else
	{
		for (y = 0; y < height; y++) 
		{
			offset = pixels + y * rowstride;
			
			for (x = 0; x < width; x++) 
			{
				*offset++ = NAUTILUS_RGBA_COLOR_GET_R (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_G (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_B (color);
			}
		}
	}
}

/* FIXME bugzilla.eazel.com 1612: 
 * Tile origin should be respected.  Should be fixed when I use NautilusBackground.
 */
static void
nautilus_gdk_pixbuf_tile (GdkPixbuf		*pixbuf,
			  const GdkPixbuf	*tile_pixbuf,
			  guint			tile_width,
			  guint			tile_height,
			  gint			tile_origin_x,
			  gint			tile_origin_y)
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
				GdkPoint	source_point;
				GdkRectangle	destination_area;

				source_point.x = 0;
				source_point.y = 0;

				destination_area.x = dst_x;
				destination_area.y = dst_y;
				destination_area.width = copy_width;
				destination_area.height = copy_height;
				
				nautilus_gdk_pixbuf_render_to_pixbuf (tile_pixbuf,
								      pixbuf,
								      &source_point,
								      &destination_area);
			}

			x += tile_width;
		}

		y += tile_height;
	}
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

/* Public NautilusImage */
GtkWidget*
nautilus_image_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_image_get_type ()));
}

void
nautilus_image_set_background_pixbuf (NautilusImage *image, GdkPixbuf *background_pixbuf)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	if (background_pixbuf != image->detail->background_pixbuf)
	{
		NAUTILUS_GDK_PIXBUF_UNREF_IF (image->detail->background_pixbuf);
		
		NAUTILUS_GDK_PIXBUF_REF_IF (background_pixbuf);
		
		image->detail->background_pixbuf = background_pixbuf;
	}

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

GdkPixbuf*
nautilus_image_get_background_pixbuf (const NautilusImage *image)
{
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NULL);

	NAUTILUS_GDK_PIXBUF_REF_IF (image->detail->background_pixbuf);
	
	return image->detail->background_pixbuf;
}

void
nautilus_image_set_background_type (NautilusImage *image, NautilusImageBackgroundType background_type)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	if (background_type == image->detail->background_type)
	{
		return;
	}

	image->detail->background_type = background_type;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

NautilusImageBackgroundType
nautilus_image_get_background_type (const NautilusImage *image)
{
	g_return_val_if_fail (image != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);
	
	return image->detail->background_type;
}

void
nautilus_image_set_background_color (NautilusImage *image, guint32 background_color)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	if (background_color == image->detail->background_color)
	{
		return;
	}

	image->detail->background_color = background_color;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

guint32
nautilus_image_get_background_color (const NautilusImage *image)
{
	g_return_val_if_fail (image != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);
	
	return image->detail->background_color;
}

void
nautilus_image_set_placement_type (NautilusImage *image, NautilusImagePlacementType placement_type)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	if (placement_type == image->detail->placement_type)
	{
		return;
	}

	image->detail->placement_type = placement_type;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

NautilusImagePlacementType
nautilus_image_get_placement_type (const NautilusImage *image)
{
	g_return_val_if_fail (image != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);
	
	return image->detail->placement_type;
}

void
nautilus_image_set_pixbuf (NautilusImage *image, GdkPixbuf *pixbuf)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	if (pixbuf != image->detail->pixbuf)
	{
		NAUTILUS_GDK_PIXBUF_UNREF_IF (image->detail->pixbuf);
		
		NAUTILUS_GDK_PIXBUF_REF_IF (pixbuf);
		
		image->detail->pixbuf = pixbuf;
	}

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

GdkPixbuf*
nautilus_image_get_pixbuf (const NautilusImage *image)
{
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NULL);

	NAUTILUS_GDK_PIXBUF_REF_IF (image->detail->pixbuf);
	
	return image->detail->pixbuf;
}

void
nautilus_image_set_overall_alpha (NautilusImage	*image,
				   guchar		pixbuf_alpha)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->detail->overall_alpha = pixbuf_alpha;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

void
nautilus_image_set_label_text (NautilusImage	*image,
				 const gchar		*text)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	g_free (image->detail->label_text);
	
	image->detail->label_text = text ? g_strdup (text) : NULL;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

gchar*
nautilus_image_get_label_text (NautilusImage *image)
{
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NULL);

	return image->detail->label_text ? g_strdup (image->detail->label_text) : NULL;
}

void
nautilus_image_set_label_font (NautilusImage	*image,
				 GdkFont		*font)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	NAUTILUS_GDK_FONT_UNREF_IF (image->detail->label_font);

	image->detail->label_font = font;

	NAUTILUS_GDK_FONT_REF_IF (image->detail->label_font);

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

GdkFont *
nautilus_image_get_label_font (NautilusImage *image)
{
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NULL);

	NAUTILUS_GDK_FONT_REF_IF (image->detail->label_font);

	return image->detail->label_font;
}

void
nautilus_image_set_left_offset (NautilusImage	*image,
				  guint			left_offset)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->detail->left_offset = left_offset;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

void
nautilus_image_set_right_offset (NautilusImage	*image,
				   guint		right_offset)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->detail->right_offset = right_offset;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

void
nautilus_image_set_top_offset (NautilusImage	*image,
				 guint			top_offset)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->detail->top_offset = top_offset;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

void
nautilus_image_set_bottom_offset (NautilusImage	*image,
				    guint		bottom_offset)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->detail->bottom_offset = bottom_offset;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

void
nautilus_image_set_extra_width (NautilusImage	*image,
				    guint		extra_width)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->detail->extra_width = extra_width;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

void
nautilus_image_set_extra_height (NautilusImage	*image,
				    guint		extra_height)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->detail->extra_height = extra_height;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

