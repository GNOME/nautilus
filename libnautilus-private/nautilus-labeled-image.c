/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-labeled-image.c - A labeled image.

   Copyright (C) 2000 Eazel, Inc.

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

#include "nautilus-labeled-image.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-art-extensions.h"
#include "nautilus-art-gtk-extensions.h"
#include "nautilus-label.h"
#include "nautilus-image.h"
#include "nautilus-debug-drawing.h"

#include <gtk/gtkbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcheckbutton.h>

#define DEFAULT_SPACING 0
#define DEFAULT_X_PADDING 0
#define DEFAULT_Y_PADDING 0
#define DEFAULT_X_ALIGNMENT 0.5
#define DEFAULT_Y_ALIGNMENT 0.5

/* Arguments */
enum
{
	ARG_0,
	ARG_FILL,
	ARG_LABEL,
	ARG_LABEL_POSITION,
	ARG_PIXBUF,
	ARG_SHOW_IMAGE,
	ARG_SHOW_LABEL,
	ARG_SPACING,
	ARG_X_ALIGNMENT,
	ARG_X_PADDING,
	ARG_Y_ALIGNMENT,
	ARG_Y_PADDING,
};

/* Detail member struct */
struct _NautilusLabeledImageDetails
{
	GtkWidget *image;
	GtkWidget *label;
	GtkPositionType label_position;
	gboolean show_label;
	gboolean show_image;
	guint spacing;
	float x_alignment;
	float y_alignment;
	int x_padding;
	int y_padding;
	int fixed_image_height;
	gboolean fill;
};

/* GtkObjectClass methods */
static void               nautilus_labeled_image_initialize_class (NautilusLabeledImageClass  *labeled_image_class);
static void               nautilus_labeled_image_initialize       (NautilusLabeledImage       *image);
static void               nautilus_labeled_image_destroy          (GtkObject                  *object);
static void               nautilus_labeled_image_set_arg          (GtkObject                  *object,
								   GtkArg                     *arg,
								   guint                       arg_id);
static void               nautilus_labeled_image_get_arg          (GtkObject                  *object,
								   GtkArg                     *arg,
								   guint                       arg_id);
/* GtkWidgetClass methods */
static void               nautilus_labeled_image_size_request     (GtkWidget                  *widget,
								   GtkRequisition             *requisition);
static int                nautilus_labeled_image_expose_event     (GtkWidget                  *widget,
								   GdkEventExpose             *event);
static void               nautilus_labeled_image_size_allocate    (GtkWidget                  *widget,
								   GtkAllocation              *allocation);
static void               nautilus_labeled_image_map              (GtkWidget                  *widget);
static void               nautilus_labeled_image_unmap            (GtkWidget                  *widget);

/* GtkContainerClass methods */
static void               nautilus_labeled_image_add              (GtkContainer               *container,
								   GtkWidget                  *widget);
static void               nautilus_labeled_image_remove           (GtkContainer               *container,
								   GtkWidget                  *widget);
static void               nautilus_labeled_image_forall           (GtkContainer               *container,
								   gboolean                    include_internals,
								   GtkCallback                 callback,
								   gpointer                    callback_data);

/* Private NautilusLabeledImage methods */
static NautilusDimensions labeled_image_get_image_dimensions      (const NautilusLabeledImage *labeled_image);
static NautilusDimensions labeled_image_get_label_dimensions      (const NautilusLabeledImage *labeled_image);
static void               labeled_image_ensure_label              (NautilusLabeledImage       *labeled_image);
static void               labeled_image_ensure_image              (NautilusLabeledImage       *labeled_image);
static ArtIRect           labeled_image_get_content_bounds        (const NautilusLabeledImage *labeled_image);
static NautilusDimensions labeled_image_get_content_dimensions    (const NautilusLabeledImage *labeled_image);
static void               labeled_image_update_alignments         (NautilusLabeledImage       *labeled_image);
static gboolean           labeled_image_show_label                (const NautilusLabeledImage *labeled_image);
static gboolean           labeled_image_show_image                (const NautilusLabeledImage *labeled_image);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusLabeledImage, nautilus_labeled_image, GTK_TYPE_CONTAINER)

/* Class init methods */
static void
nautilus_labeled_image_initialize_class (NautilusLabeledImageClass *labeled_image_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (labeled_image_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (labeled_image_class);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (labeled_image_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_labeled_image_destroy;
	object_class->set_arg = nautilus_labeled_image_set_arg;
	object_class->get_arg = nautilus_labeled_image_get_arg;
	
 	/* GtkWidgetClass */
 	widget_class->size_request = nautilus_labeled_image_size_request;
	widget_class->size_allocate = nautilus_labeled_image_size_allocate;
 	widget_class->expose_event = nautilus_labeled_image_expose_event;
	widget_class->map = nautilus_labeled_image_map;
	widget_class->unmap = nautilus_labeled_image_unmap;

 	/* GtkContainerClass */
	container_class->add = nautilus_labeled_image_add;
	container_class->remove = nautilus_labeled_image_remove;
	container_class->forall = nautilus_labeled_image_forall;
  
	/* Arguments */
	gtk_object_add_arg_type ("NautilusLabeledImage::pixbuf",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PIXBUF);
	gtk_object_add_arg_type ("NautilusLabeledImage::label",
				 GTK_TYPE_STRING,
				 GTK_ARG_READWRITE,
				 ARG_LABEL);
	gtk_object_add_arg_type ("NautilusLabeledImage::label_position",
				 GTK_TYPE_POSITION_TYPE,
				 GTK_ARG_READWRITE,
				 ARG_LABEL_POSITION);
	gtk_object_add_arg_type ("NautilusLabeledImage::show_label",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_SHOW_LABEL);
	gtk_object_add_arg_type ("NautilusLabeledImage::show_image",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_SHOW_IMAGE);
	gtk_object_add_arg_type ("NautilusLabeledImage::spacing",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SPACING);
	gtk_object_add_arg_type ("NautilusLabeledImage::x_padding",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_X_PADDING);
	gtk_object_add_arg_type ("NautilusLabeledImage::y_padding",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_Y_PADDING);
	gtk_object_add_arg_type ("NautilusLabeledImage::x_alignment",
				 GTK_TYPE_FLOAT,
				 GTK_ARG_READWRITE,
				 ARG_X_ALIGNMENT);
	gtk_object_add_arg_type ("NautilusLabeledImage::y_alignment",
				 GTK_TYPE_FLOAT,
				 GTK_ARG_READWRITE,
				 ARG_Y_ALIGNMENT);
	gtk_object_add_arg_type ("NautilusLabeledImage::fill",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_FILL);
}

void
nautilus_labeled_image_initialize (NautilusLabeledImage *labeled_image)
{
	GTK_WIDGET_SET_FLAGS (labeled_image, GTK_NO_WINDOW);

	labeled_image->details = g_new0 (NautilusLabeledImageDetails, 1);
	labeled_image->details->show_label = TRUE;
	labeled_image->details->show_image = TRUE;
	labeled_image->details->label_position = GTK_POS_BOTTOM;
	labeled_image->details->spacing = DEFAULT_SPACING;
	labeled_image->details->x_padding = DEFAULT_X_PADDING;
	labeled_image->details->y_padding = DEFAULT_Y_PADDING;
	labeled_image->details->x_alignment = DEFAULT_X_ALIGNMENT;
	labeled_image->details->y_alignment = DEFAULT_Y_ALIGNMENT;
	labeled_image->details->fixed_image_height = 0;

	nautilus_labeled_image_set_fill (labeled_image, FALSE);
}

/* GtkObjectClass methods */
static void
nautilus_labeled_image_destroy (GtkObject *object)
{
 	NautilusLabeledImage *labeled_image;
	
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (object));

	labeled_image = NAUTILUS_LABELED_IMAGE (object);

	if (labeled_image->details->image != NULL) {
		gtk_widget_destroy (labeled_image->details->image);
		labeled_image->details->image = NULL;
	}

	if (labeled_image->details->label != NULL) {
		gtk_widget_destroy (labeled_image->details->label);
		labeled_image->details->label = NULL;
	}

	g_free (labeled_image->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_labeled_image_set_arg (GtkObject *object,
				GtkArg *arg,
				guint arg_id)
{
	NautilusLabeledImage *labeled_image;
	
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (object));

 	labeled_image = NAUTILUS_LABELED_IMAGE (object);

 	switch (arg_id)
	{
	case ARG_PIXBUF:
		nautilus_labeled_image_set_pixbuf (labeled_image, (GdkPixbuf *) GTK_VALUE_POINTER (*arg));
		break;

	case ARG_LABEL:
		nautilus_labeled_image_set_text (labeled_image, GTK_VALUE_STRING (*arg));
		break;

	case ARG_LABEL_POSITION:
		nautilus_labeled_image_set_label_position (labeled_image, GTK_VALUE_ENUM (*arg));
		break;

	case ARG_SHOW_LABEL:
		nautilus_labeled_image_set_show_label (labeled_image, GTK_VALUE_BOOL (*arg));
		break;

	case ARG_SHOW_IMAGE:
		nautilus_labeled_image_set_show_image (labeled_image, GTK_VALUE_BOOL (*arg));
		break;

	case ARG_SPACING:
		nautilus_labeled_image_set_spacing (labeled_image, GTK_VALUE_UINT (*arg));
		break;

	case ARG_X_PADDING:
		nautilus_labeled_image_set_x_padding (labeled_image, GTK_VALUE_INT (*arg));
		break;

	case ARG_Y_PADDING:
		nautilus_labeled_image_set_y_padding (labeled_image, GTK_VALUE_INT (*arg));
		break;

	case ARG_X_ALIGNMENT:
		nautilus_labeled_image_set_x_alignment (labeled_image, GTK_VALUE_FLOAT (*arg));
		break;

	case ARG_Y_ALIGNMENT:
		nautilus_labeled_image_set_y_alignment (labeled_image, GTK_VALUE_FLOAT (*arg));
		break;

	case ARG_FILL:
		nautilus_labeled_image_set_fill (labeled_image, GTK_VALUE_BOOL (*arg));
		break;
 	default:
		g_assert_not_reached ();
	}
}

static void
nautilus_labeled_image_get_arg (GtkObject *object,
				GtkArg *arg,
				guint arg_id)
{
	NautilusLabeledImage *labeled_image;

	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (object));
	
	labeled_image = NAUTILUS_LABELED_IMAGE (object);

 	switch (arg_id)
	{
	case ARG_PIXBUF:
		GTK_VALUE_POINTER (*arg) = nautilus_labeled_image_get_pixbuf (labeled_image);
		break;

	case ARG_LABEL:
		GTK_VALUE_STRING (*arg) = nautilus_labeled_image_get_text (labeled_image);
		break;

	case ARG_LABEL_POSITION:
		GTK_VALUE_ENUM (*arg) = nautilus_labeled_image_get_label_position (labeled_image);
		break;

	case ARG_SHOW_LABEL:
		GTK_VALUE_BOOL (*arg) = nautilus_labeled_image_get_show_label (labeled_image);
		break;

	case ARG_SHOW_IMAGE:
		GTK_VALUE_BOOL (*arg) = nautilus_labeled_image_get_show_image (labeled_image);
		break;

	case ARG_SPACING:
		GTK_VALUE_UINT (*arg) = nautilus_labeled_image_get_spacing (labeled_image);
		break;

	case ARG_X_PADDING:
		GTK_VALUE_INT (*arg) = nautilus_labeled_image_get_x_padding (labeled_image);
		break;

	case ARG_Y_PADDING:
		GTK_VALUE_INT (*arg) = nautilus_labeled_image_get_y_padding (labeled_image);
		break;

	case ARG_X_ALIGNMENT:
		GTK_VALUE_FLOAT (*arg) = nautilus_labeled_image_get_x_alignment (labeled_image);
		break;

	case ARG_Y_ALIGNMENT:
		GTK_VALUE_FLOAT (*arg) = nautilus_labeled_image_get_y_alignment (labeled_image);
		break;

	case ARG_FILL:
		GTK_VALUE_BOOL (*arg) = nautilus_labeled_image_get_fill (labeled_image);
		break;

 	default:
		g_assert_not_reached ();
	}
}

/* GtkWidgetClass methods */
static void
nautilus_labeled_image_size_request (GtkWidget *widget,
				     GtkRequisition *requisition)
{
	NautilusLabeledImage *labeled_image;
 	NautilusDimensions content_dimensions;

 	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget));
 	g_return_if_fail (requisition != NULL);

  	labeled_image = NAUTILUS_LABELED_IMAGE (widget);
	
 	content_dimensions = labeled_image_get_content_dimensions (labeled_image);

	requisition->width = 
		MAX (1, content_dimensions.width) +
		2 * labeled_image->details->x_padding;

	requisition->height = 
		MAX (1, content_dimensions.height) +
		2 * labeled_image->details->y_padding;
}

static void
nautilus_labeled_image_size_allocate (GtkWidget *widget,
				      GtkAllocation *allocation)
{
	NautilusLabeledImage *labeled_image;
 	ArtIRect image_bounds;
	ArtIRect label_bounds;

 	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget));
 	g_return_if_fail (allocation != NULL);

  	labeled_image = NAUTILUS_LABELED_IMAGE (widget);

	widget->allocation = *allocation;
	
 	label_bounds = nautilus_labeled_image_get_label_bounds (labeled_image);
	if (!art_irect_empty (&label_bounds)) {
		GtkAllocation label_allocation;

		label_allocation.x = label_bounds.x0;
		label_allocation.y = label_bounds.y0;
		label_allocation.width = nautilus_art_irect_get_width (&label_bounds);
		label_allocation.height = nautilus_art_irect_get_height (&label_bounds);

		gtk_widget_size_allocate (labeled_image->details->label, &label_allocation);
	}

 	image_bounds = nautilus_labeled_image_get_image_bounds (labeled_image);
	if (!art_irect_empty (&image_bounds)) {
		GtkAllocation image_allocation;
		
		image_allocation.x = image_bounds.x0;
		image_allocation.y = image_bounds.y0;
		image_allocation.width = nautilus_art_irect_get_width (&image_bounds);
		image_allocation.height = nautilus_art_irect_get_height (&image_bounds);

		gtk_widget_size_allocate (labeled_image->details->image, &image_allocation);
	}
}

static int
nautilus_labeled_image_expose_event (GtkWidget *widget,
				     GdkEventExpose *event)
{
	NautilusLabeledImage *labeled_image;

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget), TRUE);
	g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), TRUE);
	g_return_val_if_fail (event != NULL, TRUE);

  	labeled_image = NAUTILUS_LABELED_IMAGE (widget);

	if (labeled_image_show_label (labeled_image)) {
		GdkEventExpose label_event;
		
		label_event = *event;

		if (GTK_WIDGET_DRAWABLE (labeled_image->details->label) &&
		    GTK_WIDGET_NO_WINDOW (labeled_image->details->label) &&
		    gtk_widget_intersect (labeled_image->details->label, &event->area, &label_event.area)) {
			gtk_widget_event (labeled_image->details->label, (GdkEvent *) &label_event);
		}
	}
	
	if (labeled_image_show_image (labeled_image)) {
		GdkEventExpose image_event;

		image_event = *event;

		if (GTK_WIDGET_DRAWABLE (labeled_image->details->image) &&
		    GTK_WIDGET_NO_WINDOW (labeled_image->details->image) &&
		    gtk_widget_intersect (labeled_image->details->image, &event->area, &image_event.area)) {
			gtk_widget_event (labeled_image->details->image, (GdkEvent *) &image_event);
		}
	}

	return FALSE;
}

static void
nautilus_labeled_image_map (GtkWidget *widget)
{
	NautilusLabeledImage *labeled_image;

	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget));
	
	labeled_image = NAUTILUS_LABELED_IMAGE (widget);

 	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
	
	if (labeled_image_show_label (labeled_image)) {
		if (GTK_WIDGET_VISIBLE (labeled_image->details->label) &&
		    !GTK_WIDGET_MAPPED (labeled_image->details->label)) {
			gtk_widget_map (labeled_image->details->label);
		}
	}

	if (labeled_image_show_image (labeled_image)) {
		if (GTK_WIDGET_VISIBLE (labeled_image->details->image) &&
		    !GTK_WIDGET_MAPPED (labeled_image->details->image)) {
			gtk_widget_map (labeled_image->details->image);
		}
	}
}

static void
nautilus_labeled_image_unmap (GtkWidget *widget)
{
	NautilusLabeledImage *labeled_image;

	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget));
	
	labeled_image = NAUTILUS_LABELED_IMAGE (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
	
	if (labeled_image->details->label != NULL) {
		if (GTK_WIDGET_VISIBLE (labeled_image->details->label) &&
		    GTK_WIDGET_MAPPED (labeled_image->details->label)) {
			gtk_widget_unmap (labeled_image->details->label);
		}
	}

	if (labeled_image->details->image != NULL) {
		if (GTK_WIDGET_VISIBLE (labeled_image->details->image) &&
		    GTK_WIDGET_MAPPED (labeled_image->details->image)) {
			gtk_widget_unmap (labeled_image->details->image);
		}
	}
}

/* GtkContainerClass methods */
static void
nautilus_labeled_image_add (GtkContainer *container,
			    GtkWidget *child)
{
	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (container));
	g_return_if_fail (NAUTILUS_IS_LABEL (child) || NAUTILUS_IS_IMAGE (child));

	gtk_widget_set_parent (child, GTK_WIDGET (container));

	if (GTK_WIDGET_REALIZED (container)) {
		gtk_widget_realize (child);
	}
	
	if (GTK_WIDGET_VISIBLE (container) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (container)) {
			gtk_widget_map (child);
		}
		
		gtk_widget_queue_resize (child);
	}
}

static void
nautilus_labeled_image_remove (GtkContainer *container,
			       GtkWidget *child)
{
	NautilusLabeledImage *labeled_image;
	gboolean child_was_visible;
	
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (container));
	g_return_if_fail (NAUTILUS_IS_LABEL (child) || NAUTILUS_IS_IMAGE (child));
	
	labeled_image = NAUTILUS_LABELED_IMAGE (container);;

	g_return_if_fail (child == labeled_image->details->image || child == labeled_image->details->label);

	child_was_visible = GTK_WIDGET_VISIBLE (child);

	gtk_widget_unparent (child);

	if (child_was_visible) {
		gtk_widget_queue_resize (GTK_WIDGET (container));
	}

	if (labeled_image->details->image == child) {
		labeled_image->details->image = NULL;
	}

	if (labeled_image->details->label == child) {
		labeled_image->details->label = NULL;
	}
}

static void
nautilus_labeled_image_forall (GtkContainer *container,
			       gboolean include_internals,
			       GtkCallback callback,
			       gpointer callback_data)
{
	NautilusLabeledImage *labeled_image;
	
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (container));
	g_return_if_fail (callback != NULL);
	
	labeled_image = NAUTILUS_LABELED_IMAGE (container);;
	
	if (include_internals) {
		if (labeled_image->details->image != NULL) {
			(* callback) (labeled_image->details->image, callback_data);
		}

		if (labeled_image->details->label != NULL) {
			(* callback) (labeled_image->details->label, callback_data);
		}
	}
}

/* Private NautilusLabeledImage methods */
static gboolean
is_fixed_height (const NautilusLabeledImage *labeled_image)
{
	return labeled_image->details->fixed_image_height > 0;
}

static NautilusDimensions
labeled_image_get_image_dimensions (const NautilusLabeledImage *labeled_image)
{
	NautilusDimensions image_dimensions;
	GtkRequisition image_requisition;	

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NAUTILUS_DIMENSIONS_EMPTY);

	if (!labeled_image_show_image (labeled_image)) {
		return NAUTILUS_DIMENSIONS_EMPTY;
	}
	
	gtk_widget_size_request (labeled_image->details->image, &image_requisition);

	image_dimensions.width = (int) image_requisition.width;
	image_dimensions.height = (int) image_requisition.height;

	if (is_fixed_height (labeled_image)) {
		image_dimensions.height = labeled_image->details->fixed_image_height;
	}

	return image_dimensions;
}

static NautilusDimensions
labeled_image_get_label_dimensions (const NautilusLabeledImage *labeled_image)
{
	NautilusDimensions label_dimensions;
	GtkRequisition label_requisition;	

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NAUTILUS_DIMENSIONS_EMPTY);

	if (!labeled_image_show_label (labeled_image)) {
		return NAUTILUS_DIMENSIONS_EMPTY;
	}
	
	gtk_widget_size_request (labeled_image->details->label, &label_requisition);
	
	label_dimensions.width = (int) label_requisition.width;
	label_dimensions.height = (int) label_requisition.height;

	return label_dimensions;
}

static ArtIRect
labeled_image_get_image_bounds_fill (const NautilusLabeledImage *labeled_image)
{
	ArtIRect image_bounds;
	NautilusDimensions image_dimensions;
	ArtIRect content_bounds;
	ArtIRect bounds;

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NAUTILUS_ART_IRECT_EMPTY);

	image_dimensions = labeled_image_get_image_dimensions (labeled_image);

	if (nautilus_dimensions_empty (&image_dimensions)) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	content_bounds = labeled_image_get_content_bounds (labeled_image);
	bounds = nautilus_gtk_widget_get_bounds (GTK_WIDGET (labeled_image));
	
	if (!labeled_image_show_label (labeled_image)) {
		image_bounds = bounds;
	} else {
		switch (labeled_image->details->label_position) {
		case GTK_POS_LEFT:
			image_bounds.y0 = bounds.y0;
			image_bounds.x0 = content_bounds.x1 - image_dimensions.width;
			image_bounds.y1 = bounds.y1;
			image_bounds.x1 = bounds.x1;
			break;

		case GTK_POS_RIGHT:
			image_bounds.y0 = bounds.y0;
			image_bounds.x0 = bounds.x0;
			image_bounds.y1 = bounds.y1;
			image_bounds.x1 = content_bounds.x0 + image_dimensions.width;
			break;

		case GTK_POS_TOP:
			image_bounds.x0 = bounds.x0;
			image_bounds.y0 = content_bounds.y1 - image_dimensions.height;
			image_bounds.x1 = bounds.x1;
			image_bounds.y1 = bounds.y1;
			break;

		case GTK_POS_BOTTOM:
			image_bounds.x0 = bounds.x0;
			image_bounds.y0 = bounds.y0;
			image_bounds.x1 = bounds.x1;
			image_bounds.y1 = content_bounds.y0 + image_dimensions.height;
			break;
		}
	}

	return image_bounds;
}

ArtIRect
nautilus_labeled_image_get_image_bounds (const NautilusLabeledImage *labeled_image)
{
	NautilusDimensions image_dimensions;
	NautilusDimensions label_dimensions;
	GtkRequisition image_requisition;
	ArtIRect image_bounds;
	ArtIRect content_bounds;

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NAUTILUS_ART_IRECT_EMPTY);

	if (labeled_image->details->fill) {
		return labeled_image_get_image_bounds_fill (labeled_image);
	}

	/* get true real dimensions if we're in fixed height mode */
	if (is_fixed_height (labeled_image) && labeled_image_show_image (labeled_image)) {
		gtk_widget_size_request (labeled_image->details->image, &image_requisition);
		image_dimensions.width = (int) image_requisition.width;
		image_dimensions.height = (int) image_requisition.height;
	} else {
		image_dimensions = labeled_image_get_image_dimensions (labeled_image);
	}
	
	label_dimensions = labeled_image_get_label_dimensions (labeled_image);

	if (nautilus_dimensions_empty (&image_dimensions)) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	content_bounds = labeled_image_get_content_bounds (labeled_image);
	
	if (!labeled_image_show_label (labeled_image)) {
		image_bounds.x0 = 
			content_bounds.x0 +
			(nautilus_art_irect_get_width (&content_bounds) - image_dimensions.width) / 2;
		image_bounds.y0 = 
			content_bounds.y0 +
			(nautilus_art_irect_get_height (&content_bounds) - image_dimensions.height) / 2;
	} else {
		switch (labeled_image->details->label_position) {
		case GTK_POS_LEFT:
			image_bounds.x0 = content_bounds.x1 - image_dimensions.width;
			image_bounds.y0 = 
				content_bounds.y0 +
				(nautilus_art_irect_get_height (&content_bounds) - image_dimensions.height) / 2;
			break;

		case GTK_POS_RIGHT:
			image_bounds.x0 = content_bounds.x0;
			image_bounds.y0 = 
				content_bounds.y0 +
				(nautilus_art_irect_get_height (&content_bounds) - image_dimensions.height) / 2;
			break;

		case GTK_POS_TOP:
			image_bounds.x0 = 
				content_bounds.x0 +
				(nautilus_art_irect_get_width (&content_bounds) - image_dimensions.width) / 2;
			image_bounds.y0 = content_bounds.y1 - image_dimensions.height;
			break;

		case GTK_POS_BOTTOM:
			image_bounds.x0 = 
				content_bounds.x0 +
				(nautilus_art_irect_get_width (&content_bounds) - image_dimensions.width) / 2;
				
			if (is_fixed_height (labeled_image)) {	
				image_bounds.y0 = content_bounds.y0 + nautilus_art_irect_get_height (&content_bounds)
						  - image_dimensions.height
						  - label_dimensions.height
						  - labeled_image->details->spacing;
			} else {
				image_bounds.y0 = content_bounds.y0;
			}	

			break;
		}
	}
	
	image_bounds.x1 = image_bounds.x0 + image_dimensions.width;
	image_bounds.y1 = image_bounds.y0 + image_dimensions.height;

	return image_bounds;
}

static ArtIRect
labeled_image_get_label_bounds_fill (const NautilusLabeledImage *labeled_image)
{
	ArtIRect label_bounds;
	NautilusDimensions label_dimensions;
	ArtIRect content_bounds;
	ArtIRect bounds;

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NAUTILUS_ART_IRECT_EMPTY);

	label_dimensions = labeled_image_get_label_dimensions (labeled_image);

	if (nautilus_dimensions_empty (&label_dimensions)) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	content_bounds = labeled_image_get_content_bounds (labeled_image);
	bounds = nautilus_gtk_widget_get_bounds (GTK_WIDGET (labeled_image));

	/* Only the label is shown */
	if (!labeled_image_show_image (labeled_image)) {
		label_bounds = bounds;
	/* Both label and image are shown */
	} else {
		switch (labeled_image->details->label_position) {
		case GTK_POS_LEFT:
			label_bounds.y0 = bounds.y0;
			label_bounds.x0 = bounds.x0;
			label_bounds.y1 = bounds.y1;
			label_bounds.x1 = content_bounds.x0 + label_dimensions.width;
			break;

		case GTK_POS_RIGHT:
			label_bounds.y0 = bounds.y0;
			label_bounds.x0 = content_bounds.x1 - label_dimensions.width;
			label_bounds.y1 = bounds.y1;
			label_bounds.x1 = bounds.x1;
			break;

		case GTK_POS_TOP:
			label_bounds.x0 = bounds.x0;
			label_bounds.y0 = bounds.y0;
			label_bounds.x1 = bounds.x1;
			label_bounds.y1 = content_bounds.y0 + label_dimensions.height;
			break;

		case GTK_POS_BOTTOM:
			label_bounds.x0 = bounds.x0;
			label_bounds.y0 = content_bounds.y1 - label_dimensions.height;
			label_bounds.x1 = bounds.x1;
			label_bounds.y1 = bounds.y1;
			break;
		}
	}

	return label_bounds;
}

ArtIRect
nautilus_labeled_image_get_label_bounds (const NautilusLabeledImage *labeled_image)
{
	ArtIRect label_bounds;
	NautilusDimensions label_dimensions;
	ArtIRect content_bounds;

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NAUTILUS_ART_IRECT_EMPTY);

	if (labeled_image->details->fill) {
		return labeled_image_get_label_bounds_fill (labeled_image);
	}

	label_dimensions = labeled_image_get_label_dimensions (labeled_image);

	if (nautilus_dimensions_empty (&label_dimensions)) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	content_bounds = labeled_image_get_content_bounds (labeled_image);

	/* Only the label is shown */
	if (!labeled_image_show_image (labeled_image)) {
		label_bounds.x0 = 
			content_bounds.x0 +
			(nautilus_art_irect_get_width (&content_bounds) - label_dimensions.width) / 2;
		label_bounds.y0 = 
			content_bounds.y0 +
			(nautilus_art_irect_get_height (&content_bounds) - label_dimensions.height) / 2;
	/* Both label and image are shown */
	} else {
		switch (labeled_image->details->label_position) {
		case GTK_POS_LEFT:
			label_bounds.x0 = content_bounds.x0;
			label_bounds.y0 = 
				content_bounds.y0 +
				(nautilus_art_irect_get_height (&content_bounds) - label_dimensions.height) / 2;
			break;

		case GTK_POS_RIGHT:
			label_bounds.x0 = content_bounds.x1 - label_dimensions.width;
			label_bounds.y0 = 
				content_bounds.y0 +
				(nautilus_art_irect_get_height (&content_bounds) - label_dimensions.height) / 2;
			break;

		case GTK_POS_TOP:
			label_bounds.x0 = 
				content_bounds.x0 +
				(nautilus_art_irect_get_width (&content_bounds) - label_dimensions.width) / 2;
			label_bounds.y0 = content_bounds.y0;
			break;

		case GTK_POS_BOTTOM:
			label_bounds.x0 = 
				content_bounds.x0 +
				(nautilus_art_irect_get_width (&content_bounds) - label_dimensions.width) / 2;
			label_bounds.y0 = content_bounds.y1 - label_dimensions.height;
			break;
		}
	}
	
	label_bounds.x1 = label_bounds.x0 + label_dimensions.width;
	label_bounds.y1 = label_bounds.y0 + label_dimensions.height;

	return label_bounds;
}

static void
labeled_image_update_alignments (NautilusLabeledImage *labeled_image)
{

	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	if (labeled_image->details->label != NULL) {
		float x_alignment;
		float y_alignment;
		
		if (labeled_image->details->fill) {	
			x_alignment = GTK_MISC (labeled_image->details->label)->xalign;
			y_alignment = GTK_MISC (labeled_image->details->label)->yalign;
			
			/* Only the label is shown */
			if (!labeled_image_show_image (labeled_image)) {
				x_alignment = 0.5;
				y_alignment = 0.5;
			/* Both label and image are shown */
			} else {
				switch (labeled_image->details->label_position) {
				case GTK_POS_LEFT:
					x_alignment = 1.0;
					y_alignment = 0.5;
					break;
					
				case GTK_POS_RIGHT:
					x_alignment = 0.0;
					y_alignment = 0.5;
					break;
					
				case GTK_POS_TOP:
					x_alignment = 0.5;
					y_alignment = 1.0;
					break;
					
				case GTK_POS_BOTTOM:
					x_alignment = 0.5;
					y_alignment = 0.0;
					break;
				}
				
			}

			gtk_misc_set_alignment (GTK_MISC (labeled_image->details->label),
						x_alignment,
						y_alignment);
		}
	}

	if (labeled_image->details->image != NULL) {
		float x_alignment;
		float y_alignment;
		
		if (labeled_image->details->fill) {	
			x_alignment = GTK_MISC (labeled_image->details->image)->xalign;
			y_alignment = GTK_MISC (labeled_image->details->image)->yalign;
			
			/* Only the image is shown */
			if (!labeled_image_show_label (labeled_image)) {
				x_alignment = 0.5;
				y_alignment = 0.5;
			/* Both label and image are shown */
			} else {
				switch (labeled_image->details->label_position) {
				case GTK_POS_LEFT:
					x_alignment = 0.0;
					y_alignment = 0.5;
					break;

				case GTK_POS_RIGHT:
					x_alignment = 1.0;
					y_alignment = 0.5;
					break;
					
				case GTK_POS_TOP:
					x_alignment = 0.5;
					y_alignment = 0.0;
					break;
					
				case GTK_POS_BOTTOM:
					x_alignment = 0.5;
					y_alignment = 1.0;
					break;
				}
			}
			
			gtk_misc_set_alignment (GTK_MISC (labeled_image->details->image),
						x_alignment,
						y_alignment);
		}
	}
}

static NautilusDimensions
labeled_image_get_content_dimensions (const NautilusLabeledImage *labeled_image)
{
	NautilusDimensions image_dimensions;
	NautilusDimensions label_dimensions;
	NautilusDimensions content_dimensions;

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NAUTILUS_DIMENSIONS_EMPTY);

	image_dimensions = labeled_image_get_image_dimensions (labeled_image);
	label_dimensions = labeled_image_get_label_dimensions (labeled_image);

	content_dimensions = NAUTILUS_DIMENSIONS_EMPTY;

	/* Both shown */
	if (!nautilus_dimensions_empty (&image_dimensions) && !nautilus_dimensions_empty (&label_dimensions)) {
		content_dimensions.width = 
			image_dimensions.width + labeled_image->details->spacing + label_dimensions.width;
		switch (labeled_image->details->label_position) {
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
			content_dimensions.width = 
				image_dimensions.width + labeled_image->details->spacing + label_dimensions.width;
			content_dimensions.height = MAX (image_dimensions.height, label_dimensions.height);
			break;

		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
			content_dimensions.width = MAX (image_dimensions.width, label_dimensions.width);
			content_dimensions.height = 
				image_dimensions.height + labeled_image->details->spacing + label_dimensions.height;
			break;
		}
	/* Only image shown */
	} else if (!nautilus_dimensions_empty (&image_dimensions)) {
		content_dimensions.width = image_dimensions.width;
		content_dimensions.height = image_dimensions.height;
	/* Only label shown */
	} else {
		content_dimensions.width = label_dimensions.width;
		content_dimensions.height = label_dimensions.height;
	}

	return content_dimensions;
}

static ArtIRect
labeled_image_get_content_bounds (const NautilusLabeledImage *labeled_image)
{
	NautilusDimensions content_dimensions;
	ArtIRect content_bounds;
	ArtIRect bounds;

	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NAUTILUS_ART_IRECT_EMPTY);

	bounds = nautilus_gtk_widget_get_bounds (GTK_WIDGET (labeled_image));

	content_dimensions = labeled_image_get_content_dimensions (labeled_image);
	content_bounds = nautilus_art_irect_align (&bounds,
						   content_dimensions.width,
						   content_dimensions.height,
						   labeled_image->details->x_alignment,
						   labeled_image->details->y_alignment);

	return content_bounds;
}

static void
labeled_image_ensure_label (NautilusLabeledImage *labeled_image)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->label != NULL) {
		return;
	}

 	labeled_image->details->label = nautilus_label_new (NULL);
	gtk_container_add (GTK_CONTAINER (labeled_image), labeled_image->details->label);
	gtk_widget_show (labeled_image->details->label);
}

static void
labeled_image_ensure_image (NautilusLabeledImage *labeled_image)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->image != NULL) {
		return;
	}

 	labeled_image->details->image = nautilus_image_new (NULL);
	gtk_container_add (GTK_CONTAINER (labeled_image), labeled_image->details->image);
	gtk_widget_show (labeled_image->details->image);
}

static gboolean
labeled_image_show_image (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), FALSE);
	
	return labeled_image->details->image != NULL && labeled_image->details->show_image;
}

static gboolean
labeled_image_show_label (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), FALSE);

	return labeled_image->details->label != NULL && labeled_image->details->show_label;
}

/**
 * nautilus_labeled_image_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Returns A newly allocated NautilusLabeledImage.  If the &text parameter is not
 * NULL then the LabeledImage will show a label.  If the &pixbuf parameter is not
 * NULL then the LabeledImage will show a pixbuf.  Either of these can be NULL at
 * creation time.  
 *
 * Later in the lifetime of the widget you can invoke methods that affect the 
 * label and/or the image.  If at creation time these were NULL, then they will
 * be created as neeeded.
 *
 * Thus, using this widget in place of NautilusImage or NautilusLabel is "free" with
 * only the GtkObject and function call overhead.
 *
 */
GtkWidget*
nautilus_labeled_image_new (const char *text,
			    GdkPixbuf *pixbuf)
{
	NautilusLabeledImage *labeled_image;

	labeled_image = NAUTILUS_LABELED_IMAGE (gtk_widget_new (nautilus_labeled_image_get_type (), NULL));
	
	if (text != NULL) {
		nautilus_labeled_image_set_text (labeled_image, text);
	}

	if (pixbuf != NULL) {
		nautilus_labeled_image_set_pixbuf (labeled_image, pixbuf);
	}

	labeled_image_update_alignments (labeled_image);

	return GTK_WIDGET (labeled_image);
}

/**
 * nautilus_labeled_image_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @file_name: File name of picture to use for pixbuf.  Cannot be NULL.
 *
 * Returns A newly allocated NautilusLabeledImage.  If the &text parameter is not
 * NULL then the LabeledImage will show a label.
 *
 */
GtkWidget*
nautilus_labeled_image_new_from_file_name (const char *text,
					   const char *pixbuf_file_name)
{
	NautilusLabeledImage *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);

	labeled_image = NAUTILUS_LABELED_IMAGE (nautilus_labeled_image_new (text, NULL));
	nautilus_labeled_image_set_pixbuf_from_file_name (labeled_image, pixbuf_file_name);
	return GTK_WIDGET (labeled_image);
}

/**
 * nautilus_labeled_image_set_label_position:
 * @labeled_image: A NautilusLabeledImage.
 * @label_position: The position of the label with respect to the image.
 *
 * Set the position of the label with respect to the image as follows:
 *
 * GTK_POS_LEFT:
 *   [ <label> <image> ]
 *
 * GTK_POS_RIGHT:
 *   [ <image> <label> ]
 *
 * GTK_POS_TOP:
 *   [ <label> ]
 *   [ <image> ]
 *
 * GTK_POS_BOTTOM:
 *   [ <image> ]
 *   [ <label> ]
 *
 */
void
nautilus_labeled_image_set_label_position (NautilusLabeledImage *labeled_image,
					   GtkPositionType label_position)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	g_return_if_fail (label_position >= GTK_POS_LEFT);
	g_return_if_fail (label_position <= GTK_POS_BOTTOM);
	
	if (labeled_image->details->label_position == label_position) {
		return;
	}

	labeled_image->details->label_position = label_position;

	labeled_image_update_alignments (labeled_image);

	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_label_postiion:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Returns an enumeration indicating the position of the label with respect to the image.
 */
GtkPositionType
nautilus_labeled_image_get_label_position (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->label_position;
}

/**
 * nautilus_labeled_image_set_show_label:
 * @labeled_image: A NautilusLabeledImage.
 * @show_image: A boolean value indicating whether the label should be shown.
 *
 * Update the labeled image to either show or hide the internal label widget.
 * This function doesnt have any effect if the LabeledImage doesnt already
 * contain an label.
 */
void
nautilus_labeled_image_set_show_label (NautilusLabeledImage *labeled_image,
				       gboolean show_label)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->show_label == show_label) {
		return;
	}

	labeled_image->details->show_label = show_label;

	if (labeled_image->details->label != NULL) {
		if (labeled_image->details->show_label) {
			gtk_widget_show (labeled_image->details->label);
		} else {
			gtk_widget_hide (labeled_image->details->label);
		}
	}

	labeled_image_update_alignments (labeled_image);

	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_show_label:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Returns a boolean value indicating whether the internal label is shown.
 */
gboolean
nautilus_labeled_image_get_show_label (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->show_label;
}

/**
 * nautilus_labeled_image_set_show_image:
 * @labeled_image: A NautilusLabeledImage.
 * @show_image: A boolean value indicating whether the image should be shown.
 *
 * Update the labeled image to either show or hide the internal image widget.
 * This function doesnt have any effect if the LabeledImage doesnt already
 * contain an image.
 */
void
nautilus_labeled_image_set_show_image (NautilusLabeledImage *labeled_image,
				       gboolean show_image)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->show_image == show_image) {
		return;
	}

	labeled_image->details->show_image = show_image;

	if (labeled_image->details->image != NULL) {
		if (labeled_image->details->show_image) {
			gtk_widget_show (labeled_image->details->image);
		} else {
			gtk_widget_hide (labeled_image->details->image);
		}
	}

	labeled_image_update_alignments (labeled_image);

	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_show_image:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Returns a boolean value indicating whether the internal image is shown.
 */
gboolean
nautilus_labeled_image_get_show_image (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->show_image;
}


/**
 * nautilus_labeled_image_set_fixed_image_height:
 * @labeled_image: A NautilusLabeledImage.
 * @fixed_image_height: The new fixed image height.
 *
 * Normally, we measure the height of images, but it's sometimes useful
 * to use a fixed height for all the images.  This routine sets the
 * image height to the passed in value
 *
 */
void
nautilus_labeled_image_set_fixed_image_height (NautilusLabeledImage *labeled_image,
						int new_height)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->fixed_image_height == new_height) {
		return;
	}
	
	labeled_image->details->fixed_image_height = new_height;

	labeled_image_update_alignments (labeled_image);
	
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_set_spacing:
 * @labeled_image: A NautilusLabeledImage.
 * @spacing: The new spacing between label and image.
 *
 * Set the spacing between label and image.  This will only affect
 * the geometry of the widget if both a label and image are currently
 * visible.
 *
 */
void
nautilus_labeled_image_set_spacing (NautilusLabeledImage *labeled_image,
				    guint spacing)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->spacing == spacing) {
		return;
	}
	
	labeled_image->details->spacing = spacing;

	labeled_image_update_alignments (labeled_image);
	
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_spacing:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Returns: The spacing between the label and image.
 */
guint
nautilus_labeled_image_get_spacing (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->spacing;
}

/**
 * nautilus_labeled_image_set_x_padding:
 * @labeled_image: A NautilusLabeledImage.
 * @x_padding: The new horizontal padding.
 *
 * Set horizontal padding for the NautilusLabeledImage.  The padding
 * attribute work just like that in GtkMisc.
 */
void
nautilus_labeled_image_set_x_padding (NautilusLabeledImage *labeled_image,
				      int x_padding)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	x_padding = MAX (0, x_padding);
	
	if (labeled_image->details->x_padding == x_padding) {
		return;
	}
	
	labeled_image->details->x_padding = x_padding;
	labeled_image_update_alignments (labeled_image);
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_x_padding:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Returns: The horizontal padding for the LabeledImage's content.
 */
int
nautilus_labeled_image_get_x_padding (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);

	return labeled_image->details->x_padding;
}

/**
 * nautilus_labeled_image_set_y_padding:
 * @labeled_image: A NautilusLabeledImage.
 * @x_padding: The new vertical padding.
 *
 * Set vertical padding for the NautilusLabeledImage.  The padding
 * attribute work just like that in GtkMisc.
 */
void
nautilus_labeled_image_set_y_padding (NautilusLabeledImage *labeled_image,
				      int y_padding)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	y_padding = MAX (0, y_padding);
	
	if (labeled_image->details->y_padding == y_padding) {
		return;
	}
	
	labeled_image->details->y_padding = y_padding;
	labeled_image_update_alignments (labeled_image);
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_x_padding:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Returns: The vertical padding for the LabeledImage's content.
 */
int
nautilus_labeled_image_get_y_padding (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);

	return labeled_image->details->y_padding;
}

/**
 * nautilus_labeled_image_set_x_alignment:
 * @labeled_image: A NautilusLabeledImage.
 * @x_alignment: The new horizontal alignment.
 *
 * Set horizontal alignment for the NautilusLabeledImage's content.
 * The 'content' is the union of the image and label.  The alignment
 * attribute work just like that in GtkMisc.
 */
void
nautilus_labeled_image_set_x_alignment (NautilusLabeledImage *labeled_image,
				      float x_alignment)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	x_alignment = MAX (0, x_alignment);
	x_alignment = MIN (1.0, x_alignment);
	
	if (labeled_image->details->x_alignment == x_alignment) {
		return;
	}
	
	labeled_image->details->x_alignment = x_alignment;
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_x_alignment:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Returns: The horizontal alignment for the LabeledImage's content.
 */
float
nautilus_labeled_image_get_x_alignment (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);

	return labeled_image->details->x_alignment;
}

/**
 * nautilus_labeled_image_set_y_alignment:
 * @labeled_image: A NautilusLabeledImage.
 * @y_alignment: The new vertical alignment.
 *
 * Set vertical alignment for the NautilusLabeledImage's content.
 * The 'content' is the union of the image and label.  The alignment
 * attribute work just like that in GtkMisc.
 */
void
nautilus_labeled_image_set_y_alignment (NautilusLabeledImage *labeled_image,
					float y_alignment)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	y_alignment = MAX (0, y_alignment);
	y_alignment = MIN (1.0, y_alignment);
	
	if (labeled_image->details->y_alignment == y_alignment) {
		return;
	}
	
	labeled_image->details->y_alignment = y_alignment;
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_y_alignment:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Returns: The vertical alignment for the LabeledImage's content.
 */
float
nautilus_labeled_image_get_y_alignment (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);

	return labeled_image->details->y_alignment;
}

/**
 * nautilus_labeled_image_set_fill:
 * @labeled_image: A NautilusLabeledImage.
 * @fill: A boolean value indicating whether the internal image and label
 * widgets should fill all the available allocation.
 *
 * By default the internal image and label wigets are sized to their natural
 * preferred geometry.  You can use the 'fill' attribute of LabeledImage
 * to have the internal widgets fill as much of the LabeledImage allocation
 * as is available.  This is useful if you install a tile_pixbuf and want it
 * to cover the whole widget, and not just the areas occupied by the internal
 * widgets.
 */
void
nautilus_labeled_image_set_fill (NautilusLabeledImage *labeled_image,
				 gboolean fill)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->fill == fill) {
		return;
	}

	labeled_image->details->fill = fill;

	labeled_image_update_alignments (labeled_image);
	
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * nautilus_labeled_image_get_fill:
 * @labeled_image: A NautilusLabeledImage.
 *
 * Retruns a boolean value indicating whether the internal widgets fill
 * all the available allocation.
 */
gboolean
nautilus_labeled_image_get_fill (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->fill;
}

/**
 * nautilus_labeled_image_button_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Create a stock GtkButton with a NautilusLabeledImage child.
 *
 */
GtkWidget *
nautilus_labeled_image_button_new (const char *text,
				   GdkPixbuf *pixbuf)
{
	GtkWidget *button;
	GtkWidget *labeled_image;
	
	button = gtk_button_new ();
	labeled_image = nautilus_labeled_image_new (text, pixbuf);
	gtk_container_add (GTK_CONTAINER (button), labeled_image);
	gtk_widget_show (labeled_image);
	
	return button;
}

/**
 * nautilus_labeled_image_button_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @pixbuf_file_name: Name of pixbuf to use for image.  Cannot be NULL.
 *
 * Create a stock GtkToggleButton with a NautilusLabeledImage child.
 *
 */
GtkWidget *
nautilus_labeled_image_button_new_from_file_name (const char *text,
							 const char *pixbuf_file_name)
{
	GtkWidget *button;
	GtkWidget *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);
	
	button = gtk_button_new ();
	labeled_image = nautilus_labeled_image_new_from_file_name (text, pixbuf_file_name);
	gtk_container_add (GTK_CONTAINER (button), labeled_image);
	gtk_widget_show (labeled_image);
	
	return button;
}

/**
 * nautilus_labeled_image_toggle_button_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Create a stock GtkToggleButton with a NautilusLabeledImage child.
 *
 */
GtkWidget *
nautilus_labeled_image_toggle_button_new (const char *text,
					  GdkPixbuf *pixbuf)
{
	GtkWidget *toggle_button;
	GtkWidget *labeled_image;
	
	toggle_button = gtk_toggle_button_new ();
	labeled_image = nautilus_labeled_image_new (text, pixbuf);
	gtk_container_add (GTK_CONTAINER (toggle_button), labeled_image);
	gtk_widget_show (labeled_image);
	
	return toggle_button;
}

/**
 * nautilus_labeled_image_toggle_button_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @pixbuf_file_name: Name of pixbuf to use for image.  Cannot be NULL.
 *
 * Create a stock GtkToggleButton with a NautilusLabeledImage child.
 *
 */
GtkWidget *
nautilus_labeled_image_toggle_button_new_from_file_name (const char *text,
							 const char *pixbuf_file_name)
{
	GtkWidget *toggle_button;
	GtkWidget *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);
	
	toggle_button = gtk_toggle_button_new ();
	labeled_image = nautilus_labeled_image_new_from_file_name (text, pixbuf_file_name);
	gtk_container_add (GTK_CONTAINER (toggle_button), labeled_image);
	gtk_widget_show (labeled_image);
	
	return toggle_button;
}

/*
 * Workaround some bugs in GtkCheckButton where the widget 
 * does not redraw properly after leave or focus out events
 * 
 * The workaround is to draw a little bit more than the 
 * widget itself - 4 pixels worth.  For some reason the
 * widget does not properly redraw its edges.
 */
static void
button_leave_callback (GtkWidget *widget,
		       gpointer callback_data)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (GTK_WIDGET_DRAWABLE (widget)) {
		const int fudge = 4;
		ArtIRect bounds;

		bounds = nautilus_gtk_widget_get_bounds (widget);
		
		bounds.x0 -= fudge;
		bounds.y0 -= fudge;
		bounds.x1 += fudge;
		bounds.y1 += fudge;
		
		gtk_widget_queue_draw_area (widget->parent,
					    bounds.x0,
					    bounds.y0,
					    nautilus_art_irect_get_width (&bounds),
					    nautilus_art_irect_get_height (&bounds));
	}
}

static gint
button_focus_out_event_callback (GtkWidget *widget,
				 GdkEventFocus *event,
				 gpointer callback_data)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	button_leave_callback (widget, callback_data);

	return FALSE;
}

/**
 * nautilus_labeled_image_check_button_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Create a stock GtkCheckButton with a NautilusLabeledImage child.
 *
 */
GtkWidget *
nautilus_labeled_image_check_button_new (const char *text,
					 GdkPixbuf *pixbuf)
{
	GtkWidget *check_button;
	GtkWidget *labeled_image;
	
	check_button = gtk_check_button_new ();
	labeled_image = nautilus_labeled_image_new (text, pixbuf);
	gtk_container_add (GTK_CONTAINER (check_button), labeled_image);
	gtk_widget_show (labeled_image);
	
	/*
	 * Workaround some bugs in GtkCheckButton where the widget 
	 * does not redraw properly after leave or focus out events
	 */
	gtk_signal_connect_while_alive (GTK_OBJECT (check_button),
					"leave",
					GTK_SIGNAL_FUNC (button_leave_callback),
					NULL,
					GTK_OBJECT (check_button));
	gtk_signal_connect_while_alive (GTK_OBJECT (check_button),
					"focus_out_event",
					GTK_SIGNAL_FUNC (button_focus_out_event_callback),
					NULL,
					GTK_OBJECT (check_button));
	
	return check_button;
}

/**
 * nautilus_labeled_image_check_button_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @pixbuf_file_name: Name of pixbuf to use for image.  Cannot be NULL.
 *
 * Create a stock GtkCheckButton with a NautilusLabeledImage child.
 *
 */
GtkWidget *
nautilus_labeled_image_check_button_new_from_file_name (const char *text,
							const char *pixbuf_file_name)
{
	GtkWidget *check_button;
	GtkWidget *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);
	
	check_button = gtk_check_button_new ();
	labeled_image = nautilus_labeled_image_new_from_file_name (text, pixbuf_file_name);
	gtk_container_add (GTK_CONTAINER (check_button), labeled_image);
	gtk_widget_show (labeled_image);
	
	return check_button;
}

/*
 * The rest of the methods are proxies for those in NautilusImage and 
 * NautilusLabel.  We have all these so that we dont have to expose 
 * our internal widgets at all.  Probably more of these will be added
 * as they are needed.
 */

/**
 * nautilus_labeled_image_set_pixbuf:
 * @labaled_image: A NautilusLabeledImage.
 * @pixbuf: New pixbuf to use or NULL.
 *
 * Change the pixbuf displayed by the LabeledImage.  Note that the widget display
 * is only updated if the show_image attribute is TRUE.
 *
 * If no internal image widget exists as of yet, a new one will be created.
 *
 * A NULL &pixbuf will cause the internal image widget (if alive) to be destroyed.
 */
void
nautilus_labeled_image_set_pixbuf (NautilusLabeledImage *labeled_image,
				   GdkPixbuf *pixbuf)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	if (pixbuf == NULL) {
		if (labeled_image->details->image != NULL) {
			gtk_widget_destroy (labeled_image->details->image);
			labeled_image->details->image = NULL;
		}
		
		gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
	} else {
		labeled_image_ensure_image (labeled_image);
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (labeled_image->details->image), pixbuf);
	}
}

void
nautilus_labeled_image_set_pixbuf_from_file_name (NautilusLabeledImage *labeled_image,
						  const char *pixbuf_file_name)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	labeled_image_ensure_image (labeled_image);
	nautilus_image_set_pixbuf_from_file_name (NAUTILUS_IMAGE (labeled_image->details->image), pixbuf_file_name);
}

void
nautilus_labeled_image_set_tile_pixbuf (NautilusLabeledImage *labeled_image,
					GdkPixbuf *tile_pixbuf)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	if (labeled_image->details->image != NULL) {
		nautilus_image_set_tile_pixbuf (NAUTILUS_IMAGE (labeled_image->details->image), tile_pixbuf);
		nautilus_image_set_tile_mode_horizontal (NAUTILUS_IMAGE (labeled_image->details->image),
							 NAUTILUS_SMOOTH_TILE_ANCESTOR);
		nautilus_image_set_tile_mode_vertical (NAUTILUS_IMAGE (labeled_image->details->image),
						       NAUTILUS_SMOOTH_TILE_ANCESTOR);
	}

	if (labeled_image->details->label != NULL) {
		nautilus_label_set_tile_pixbuf (NAUTILUS_LABEL (labeled_image->details->label), tile_pixbuf);
		nautilus_label_set_tile_mode_horizontal (NAUTILUS_LABEL (labeled_image->details->label),
							 NAUTILUS_SMOOTH_TILE_ANCESTOR);
		nautilus_label_set_tile_mode_vertical (NAUTILUS_LABEL (labeled_image->details->label),
						       NAUTILUS_SMOOTH_TILE_ANCESTOR);
	}
}

void
nautilus_labeled_image_set_tile_pixbuf_from_file_name (NautilusLabeledImage *labeled_image,
						       const char *pixbuf_file_name)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->image != NULL) {
		nautilus_image_set_tile_pixbuf_from_file_name (NAUTILUS_IMAGE (labeled_image->details->image),
							       pixbuf_file_name);
		nautilus_image_set_tile_mode_horizontal (NAUTILUS_IMAGE (labeled_image->details->image),
							 NAUTILUS_SMOOTH_TILE_ANCESTOR);
		nautilus_image_set_tile_mode_vertical (NAUTILUS_IMAGE (labeled_image->details->image),
						       NAUTILUS_SMOOTH_TILE_ANCESTOR);
	}

	if (labeled_image->details->label != NULL) {
		nautilus_label_set_tile_pixbuf_from_file_name (NAUTILUS_LABEL (labeled_image->details->label),
							       pixbuf_file_name);
		nautilus_label_set_tile_mode_horizontal (NAUTILUS_LABEL (labeled_image->details->label),
							 NAUTILUS_SMOOTH_TILE_ANCESTOR);
		nautilus_label_set_tile_mode_vertical (NAUTILUS_LABEL (labeled_image->details->label),
						       NAUTILUS_SMOOTH_TILE_ANCESTOR);
	}
}

GdkPixbuf*
nautilus_labeled_image_get_pixbuf (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NULL);

	if (labeled_image->details->image == NULL) {
		return NULL;
	}
	
	return nautilus_image_get_pixbuf (NAUTILUS_IMAGE (labeled_image->details->image));
}

/**
 * nautilus_labeled_image_set_text:
 * @labaled_image: A NautilusLabeledImage.
 * @text: New text to use or NULL.
 *
 * Change the text displayed by the LabeledImage.  Note that the widget display
 * is only updated if the show_label attribute is TRUE.
 *
 * If no internal label widget exists as of yet, a new one will be created.
 *
 * A NULL &text will cause the internal label widget (if alive) to be destroyed.
 */
void
nautilus_labeled_image_set_text (NautilusLabeledImage *labeled_image,
				const char *text)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	if (text == NULL) {
		if (labeled_image->details->label) {
			gtk_widget_destroy (labeled_image->details->label);
			labeled_image->details->label = NULL;
		}
		
		gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
	} else {
		labeled_image_ensure_label (labeled_image);
		nautilus_label_set_text (NAUTILUS_LABEL (labeled_image->details->label), text);
	}
}

char *
nautilus_labeled_image_get_text (const NautilusLabeledImage *labeled_image)
{
	g_return_val_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image), NULL);
	
	if (labeled_image->details->label == NULL) {
		return NULL;
	}

	return nautilus_label_get_text (NAUTILUS_LABEL (labeled_image->details->label));
}

void
nautilus_labeled_image_make_bold (NautilusLabeledImage *labeled_image)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	labeled_image_ensure_label (labeled_image);
	nautilus_label_make_bold (NAUTILUS_LABEL (labeled_image->details->label));
}

void
nautilus_labeled_image_make_larger (NautilusLabeledImage *labeled_image,
				    guint num_sizes)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	labeled_image_ensure_label (labeled_image);
	nautilus_label_make_larger (NAUTILUS_LABEL (labeled_image->details->label), num_sizes);
}

void
nautilus_labeled_image_make_smaller (NautilusLabeledImage *labeled_image,
				    guint num_sizes)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	labeled_image_ensure_label (labeled_image);
	nautilus_label_make_smaller (NAUTILUS_LABEL (labeled_image->details->label), num_sizes);
}

void
nautilus_labeled_image_set_tile_width (NautilusLabeledImage *labeled_image,
				       int tile_width)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	if (labeled_image->details->image != NULL) {
		nautilus_image_set_tile_width (NAUTILUS_IMAGE (labeled_image->details->image),
					       tile_width);
	}

	if (labeled_image->details->label != NULL) {
		nautilus_label_set_tile_width (NAUTILUS_LABEL (labeled_image->details->label),
					       tile_width);
	}
}

void
nautilus_labeled_image_set_tile_height (NautilusLabeledImage *labeled_image,
					int tile_height)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	if (labeled_image->details->image != NULL) {
		nautilus_image_set_tile_height (NAUTILUS_IMAGE (labeled_image->details->image),
						tile_height);
	}

	if (labeled_image->details->label != NULL) {
		nautilus_label_set_tile_height (NAUTILUS_LABEL (labeled_image->details->label),
						tile_height);
	}
}

void
nautilus_labeled_image_set_background_mode (NautilusLabeledImage *labeled_image,
					    NautilusSmoothBackgroundMode background_mode)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	g_return_if_fail (background_mode >= NAUTILUS_SMOOTH_BACKGROUND_GTK);
	g_return_if_fail (background_mode <= NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);

	if (labeled_image->details->image != NULL) {
		nautilus_image_set_background_mode (NAUTILUS_IMAGE (labeled_image->details->image),
						    background_mode);
	}

	if (labeled_image->details->label != NULL) {
		nautilus_label_set_background_mode (NAUTILUS_LABEL (labeled_image->details->label),
						    background_mode);
	}
}

void
nautilus_labeled_image_set_solid_background_color (NautilusLabeledImage *labeled_image,
						   guint32 solid_background_color)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->image != NULL) {
		nautilus_image_set_solid_background_color (NAUTILUS_IMAGE (labeled_image->details->image),
						    solid_background_color);
	}

	if (labeled_image->details->label != NULL) {
		nautilus_label_set_solid_background_color (NAUTILUS_LABEL (labeled_image->details->label),
						    solid_background_color);
	}
}

void
nautilus_labeled_image_set_smooth_drop_shadow_offset (NautilusLabeledImage *labeled_image,
						      guint drop_shadow_offset)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->label != NULL) {
		nautilus_label_set_smooth_drop_shadow_offset (NAUTILUS_LABEL (labeled_image->details->label),
							      drop_shadow_offset);
	}
}

void
nautilus_labeled_image_set_smooth_drop_shadow_color (NautilusLabeledImage *labeled_image,
						     guint32 drop_shadow_color)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->label != NULL) {
		nautilus_label_set_smooth_drop_shadow_color (NAUTILUS_LABEL (labeled_image->details->label),
							     drop_shadow_color);
	}
}

void
nautilus_labeled_image_set_text_color (NautilusLabeledImage *labeled_image,
				       guint32 text_color)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	if (labeled_image->details->label != NULL) {
		nautilus_label_set_text_color (NAUTILUS_LABEL (labeled_image->details->label),
					       text_color);
	}
}

void
nautilus_labeled_image_set_label_never_smooth (NautilusLabeledImage *labeled_image,
					       gboolean never_smooth)
{
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (labeled_image));

	if (labeled_image->details->label != NULL) {
		nautilus_label_set_never_smooth (NAUTILUS_LABEL (labeled_image->details->label),
						 never_smooth);
	}
}
