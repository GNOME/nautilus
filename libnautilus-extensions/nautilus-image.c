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
	GdkPixbuf	*pixbuf;
	guchar		overall_alpha;
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


/* NautilusBufferedWidgetClass methods */
static void render_buffer_pixbuf            (NautilusBufferedWidget *buffered_widget,
					     GdkPixbuf              *buffer);


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

	/* NautilusBufferedWidgetClass */
	buffered_widget_class->render_buffer_pixbuf = render_buffer_pixbuf;
}

void
nautilus_image_initialize (NautilusImage *image)
{
	image->detail = g_new (NautilusImageDetail, 1);

	image->detail->pixbuf = NULL;
	image->detail->overall_alpha = 255;
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
	guint		pixbuf_width = 0;
	guint		pixbuf_height = 0;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (requisition != NULL);

	image = NAUTILUS_IMAGE (widget);

	if (image->detail->pixbuf != NULL) {
		pixbuf_width = gdk_pixbuf_get_width (image->detail->pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (image->detail->pixbuf);
	}

   	requisition->width = MAX (2, pixbuf_width);
   	requisition->height = MAX (2, pixbuf_height);
}

/* Private NautilusImage things */
static void
render_buffer_pixbuf (NautilusBufferedWidget *buffered_widget, GdkPixbuf *buffer)
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

/* Public NautilusImage */
GtkWidget*
nautilus_image_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_image_get_type ()));
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
