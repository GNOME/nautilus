/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-image.c - A widget to display a alpha composited pixbufs.

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

#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"

/* Arguments */
enum
{
	ARG_0,
	ARG_BACKGROUND_COLOR,
	ARG_BACKGROUND_TYPE,
	ARG_IMAGE,
	ARG_PLACEMENT_TYPE,
};

/* Detail member struct */
struct _NautilusImageDetail
{
	GdkPixbuf		*pixbuf;
	guchar			overall_alpha;
	NautilusImageAlphaMode	alpha_mode;
};

/* GtkObjectClass methods */
static void nautilus_image_initialize_class (NautilusImageClass     *image_class);
static void nautilus_image_initialize       (NautilusImage          *image);
static void nautilus_image_destroy          (GtkObject              *object);
static void nautilus_image_set_arg          (GtkObject              *object,
					     GtkArg                 *arg,
					     guint                   arg_id);
static void nautilus_image_get_arg          (GtkObject              *object,
					     GtkArg                 *arg,
					     guint                   arg_id);





/* GtkWidgetClass methods */
static void nautilus_image_size_request     (GtkWidget              *widget,
					     GtkRequisition         *requisition);
static void nautilus_image_draw             (GtkWidget              *widget,
					     GdkRectangle           *area);





/* GtkWidgetClass event methods */
static gint nautilus_image_expose_event     (GtkWidget              *widget,
					     GdkEventExpose         *event);




/* NautilusBufferedWidgetClass methods */
static void render_buffer_pixbuf            (NautilusBufferedWidget *buffered_widget,
					     GdkPixbuf              *buffer,
					     int                     horizontal_offset,
					     int                     vertical_offset);
static void nautilus_image_paint            (NautilusImage          *image,
					     const GdkRectangle     *area);



NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusImage, nautilus_image, NAUTILUS_TYPE_BUFFERED_WIDGET)

/* Class init methods */
static void
nautilus_image_initialize_class (NautilusImageClass *image_class)
{
	GtkObjectClass			*object_class = GTK_OBJECT_CLASS (image_class);
	GtkWidgetClass			*widget_class = GTK_WIDGET_CLASS (image_class);
	NautilusBufferedWidgetClass	*buffered_widget_class = NAUTILUS_BUFFERED_WIDGET_CLASS (image_class);

#if 0
	/* Arguments */
	gtk_object_add_arg_type ("NautilusImage::placement_type",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_PLACEMENT_TYPE);

	gtk_object_add_arg_type ("NautilusImage::background_type",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_TYPE);

	gtk_object_add_arg_type ("NautilusImage::background_color",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_COLOR);

	gtk_object_add_arg_type ("NautilusImage::image",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE,
				 ARG_IMAGE);
#endif

	/* GtkObjectClass */
	object_class->destroy = nautilus_image_destroy;
	object_class->set_arg = nautilus_image_set_arg;
	object_class->get_arg = nautilus_image_get_arg;

	/* GtkWidgetClass */
	widget_class->size_request = nautilus_image_size_request;
	widget_class->draw = nautilus_image_draw;
	widget_class->expose_event = nautilus_image_expose_event;

	/* NautilusBufferedWidgetClass */
	buffered_widget_class->render_buffer_pixbuf = render_buffer_pixbuf;
}

void
nautilus_image_initialize (NautilusImage *image)
{
	image->detail = g_new (NautilusImageDetail, 1);

	image->detail->pixbuf = NULL;
	image->detail->overall_alpha = 255;
	image->detail->alpha_mode = NAUTILUS_IMAGE_FULL_ALPHA;
}

/* GtkObjectClass methods */
static void
nautilus_image_destroy (GtkObject *object)
{
 	NautilusImage *image;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (object));

	image = NAUTILUS_IMAGE (object);

	nautilus_gdk_pixbuf_unref_if_not_null (image->detail->pixbuf);
	image->detail->pixbuf = NULL;

	g_free (image->detail);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_image_set_arg (GtkObject	*object,
			     GtkArg	*arg,
			     guint	arg_id)
{
	NautilusImage		*image;

 	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (object));

 	image = NAUTILUS_IMAGE (object);

#if 0
 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		image->detail->placement_type = GTK_VALUE_ENUM (*arg);
		break;

	case ARG_BACKGROUND_TYPE:
		image->detail->background_type = GTK_VALUE_ENUM (*arg);
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
#endif
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

#if 0
 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		GTK_VALUE_ENUM (*arg) = image->detail->placement_type;
		break;
		
	case ARG_BACKGROUND_TYPE:
		GTK_VALUE_ENUM (*arg) = image->detail->background_type;
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
#endif
}

/* GtkWidgetClass methods */
static void
nautilus_image_size_request (GtkWidget		*widget,
			     GtkRequisition	*requisition)
{
	NautilusImage	*image;
	GtkMisc		*misc;
	guint		pixbuf_width = 0;
	guint		pixbuf_height = 0;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (requisition != NULL);

	image = NAUTILUS_IMAGE (widget);
	misc = GTK_MISC (widget);

	if (image->detail->pixbuf != NULL) {
		pixbuf_width = gdk_pixbuf_get_width (image->detail->pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (image->detail->pixbuf);
	}

   	requisition->width = MAX (2, pixbuf_width);
   	requisition->height = MAX (2, pixbuf_height);

   	requisition->width += misc->xpad * 2;
   	requisition->height += misc->ypad * 2;
}

static void
nautilus_image_draw (GtkWidget *widget, GdkRectangle *area)
{
	NautilusImage *image;

	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (area != NULL);
	g_return_if_fail (GTK_WIDGET_REALIZED (widget));

	image = NAUTILUS_IMAGE (widget);

	if (image->detail->alpha_mode == NAUTILUS_IMAGE_FULL_ALPHA) {
		NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, draw, (widget, area));
	}
	else {
		nautilus_image_paint (image, area);
	}
}

static gint
nautilus_image_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusImage	*image;
	
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	image = NAUTILUS_IMAGE (widget);
	
	if (image->detail->alpha_mode == NAUTILUS_IMAGE_FULL_ALPHA) {
		return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, expose_event, (widget, event));
	}
	else {
		nautilus_image_paint (image, &event->area);
	}
	
	return TRUE;
}

/* Private NautilusImage things */
static void
render_buffer_pixbuf (NautilusBufferedWidget	*buffered_widget,
		      GdkPixbuf			*buffer,
		      int			horizontal_offset,
		      int			vertical_offset)
{
	NautilusImage	*image;
	GtkWidget	*widget;

	g_return_if_fail (NAUTILUS_IS_IMAGE (buffered_widget));
	g_return_if_fail (buffer != NULL);

	image = NAUTILUS_IMAGE (buffered_widget);
	widget = GTK_WIDGET (buffered_widget);

	if (image->detail->pixbuf != NULL) {
		gint x;
		gint y;
		guint width = gdk_pixbuf_get_width (image->detail->pixbuf);
		guint height = gdk_pixbuf_get_height (image->detail->pixbuf);

		if (width <= widget->allocation.width) {
			x = (widget->allocation.width - width) / 2;
		}
		else {
			x = - (width - widget->allocation.width) / 2;
		}

		if (height <= widget->allocation.height) {
			y = (widget->allocation.height - height) / 2;
		}
		else {
			y = - (height - widget->allocation.height) / 2;
		}

		gdk_pixbuf_composite (image->detail->pixbuf,
				      buffer, 
				      x,
				      y,
				      width,
				      height,
				      (double) x,
				      (double) y,
				      1.0,
				      1.0,
				      GDK_INTERP_BILINEAR,
				      image->detail->overall_alpha);
	}
}

static void
nautilus_image_paint (NautilusImage		*image,
		      const GdkRectangle	*area)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (area != NULL);
	g_return_if_fail (image->detail->alpha_mode == NAUTILUS_IMAGE_THRESHOLD_ALPHA);

	if (!GTK_WIDGET_REALIZED (image)) {
		return;
	}

	if (image->detail->pixbuf != NULL) {
		GtkWidget *widget;
		gint x;
		gint y;
		guint width = gdk_pixbuf_get_width (image->detail->pixbuf);
		guint height = gdk_pixbuf_get_height (image->detail->pixbuf);
		
		widget = GTK_WIDGET (image);
		
		if (width <= widget->allocation.width) {
			x = (widget->allocation.width - width) / 2;
		}
		else {
			x = - (width - widget->allocation.width) / 2;
		}
		
		if (height <= widget->allocation.height) {
			y = (widget->allocation.height - height) / 2;
		}
		else {
			y = - (height - widget->allocation.height) / 2;
		}

		x += widget->allocation.x;
		y += widget->allocation.y;
		
		gdk_pixbuf_render_to_drawable_alpha (image->detail->pixbuf, 
						     widget->window,
						     0,
						     0,
						     x,
						     y,
						     width,
						     height,
						     GDK_PIXBUF_ALPHA_BILEVEL,
						     128,
						     GDK_RGB_DITHER_NORMAL,
						     x,
						     y);
	}
}

/* Public NautilusImage */
GtkWidget *
nautilus_image_new (void)
{
	return gtk_widget_new (nautilus_image_get_type (), NULL);
}

void
nautilus_image_set_pixbuf (NautilusImage *image, GdkPixbuf *pixbuf)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	if (pixbuf != image->detail->pixbuf)
	{
		nautilus_gdk_pixbuf_unref_if_not_null (image->detail->pixbuf);
		nautilus_gdk_pixbuf_ref_if_not_null (pixbuf);
		image->detail->pixbuf = pixbuf;
	}

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

GdkPixbuf*
nautilus_image_get_pixbuf (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NULL);

	nautilus_gdk_pixbuf_ref_if_not_null (image->detail->pixbuf);
	
	return image->detail->pixbuf;
}

void
nautilus_image_set_overall_alpha (NautilusImage	*image,
				  guchar	pixbuf_alpha)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->detail->overall_alpha = pixbuf_alpha;

	nautilus_buffered_widget_clear_buffer (NAUTILUS_BUFFERED_WIDGET (image));
	
	gtk_widget_queue_draw (GTK_WIDGET (image));
}

/**
 * nautilus_image_set_alpha_mode:
 *
 * @image: A NautilusImage
 * @alpha_mode:      The new alpha mode
 *
 * Change the rendering mode for the widget.  In FULL_ALPHA mode, the
 * widget's background will be peeked to do full alpha compositing.
 * In THRESHOLD_ALPHA mode, all compositing is done by thresholding 
 * GdkPixbuf operations - this is more effecient, but doesnt look as 
 * smooth.
 */
void
nautilus_image_set_alpha_mode (NautilusImage			*image,
			       NautilusImageAlphaMode	alpha_mode)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (alpha_mode >= NAUTILUS_IMAGE_FULL_ALPHA);
	g_return_if_fail (alpha_mode <= NAUTILUS_IMAGE_THRESHOLD_ALPHA);
	
	if (alpha_mode != image->detail->alpha_mode)
	{
		image->detail->alpha_mode = alpha_mode;
		
		nautilus_buffered_widget_clear_buffer (NAUTILUS_BUFFERED_WIDGET (image));

		gtk_widget_queue_draw (GTK_WIDGET (image));
	}
}

/**
 * nautilus_image_get_alpha_mode:
 *
 * @image: A NautilusImage
 *
 * Return value: The current alpha mode.
 */
NautilusImageAlphaMode
nautilus_image_get_alpha_mode (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);

	return image->detail->alpha_mode;
}
