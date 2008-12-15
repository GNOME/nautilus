/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-labeled-image.c - A labeled image.

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
#include "eel-labeled-image.h"

#include "eel-art-extensions.h"
#include "eel-art-gtk-extensions.h"
#include "eel-debug-drawing.h"
#include "eel-gtk-container.h"
#include "eel-gtk-extensions.h"
#include "eel-gtk-macros.h"
#include "eel-accessibility.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <atk/atkimage.h>

#define DEFAULT_SPACING 0
#define DEFAULT_X_PADDING 0
#define DEFAULT_Y_PADDING 0
#define DEFAULT_X_ALIGNMENT 0.5
#define DEFAULT_Y_ALIGNMENT 0.5

/* Signals */
enum
{
	ACTIVATE,
	LAST_SIGNAL
};

/* Arguments */
enum
{
	PROP_0,
	PROP_FILL,
	PROP_LABEL,
	PROP_LABEL_POSITION,
	PROP_PIXBUF,
	PROP_SHOW_IMAGE,
	PROP_SHOW_LABEL,
	PROP_SPACING,
	PROP_X_ALIGNMENT,
	PROP_X_PADDING,
	PROP_Y_ALIGNMENT,
	PROP_Y_PADDING
};

/* Detail member struct */
struct EelLabeledImageDetails
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

/* derived types so we can add our accessibility interfaces */
static GType         eel_labeled_image_button_get_type        (void);
static GType         eel_labeled_image_check_button_get_type  (void);
static GType         eel_labeled_image_radio_button_get_type  (void);
static GType         eel_labeled_image_toggle_button_get_type (void);


static void          eel_labeled_image_class_init         (EelLabeledImageClass  *labeled_image_class);
static void          eel_labeled_image_init               (EelLabeledImage       *image);
static void          eel_labeled_image_finalize           (GObject               *object);



/* GObjectClass methods */
static void          eel_labeled_image_set_property       (GObject               *object,
							   guint                  property_id,
							   const GValue          *value,
							   GParamSpec            *pspec);
static void          eel_labeled_image_get_property       (GObject               *object,
							   guint                  property_id,
							   GValue                *value,
							   GParamSpec            *pspec);

/* GtkObjectClass methods */
static void          eel_labeled_image_destroy            (GtkObject             *object);

/* GtkWidgetClass methods */
static void          eel_labeled_image_size_request       (GtkWidget             *widget,
							   GtkRequisition        *requisition);
static int           eel_labeled_image_expose_event       (GtkWidget             *widget,
							   GdkEventExpose        *event);
static void          eel_labeled_image_size_allocate      (GtkWidget             *widget,
							   GtkAllocation         *allocation);
static void          eel_labeled_image_map                (GtkWidget             *widget);
static void          eel_labeled_image_unmap              (GtkWidget             *widget);
static AtkObject    *eel_labeled_image_get_accessible     (GtkWidget             *widget);

/* GtkContainerClass methods */
static void          eel_labeled_image_add                (GtkContainer          *container,
							   GtkWidget             *widget);
static void          eel_labeled_image_remove             (GtkContainer          *container,
							   GtkWidget             *widget);
static void          eel_labeled_image_forall             (GtkContainer          *container,
							   gboolean               include_internals,
							   GtkCallback            callback,
							   gpointer               callback_data);

/* Private EelLabeledImage methods */
static EelDimensions labeled_image_get_image_dimensions   (const EelLabeledImage *labeled_image);
static EelDimensions labeled_image_get_label_dimensions   (const EelLabeledImage *labeled_image);
static void          labeled_image_ensure_label           (EelLabeledImage       *labeled_image);
static void          labeled_image_ensure_image           (EelLabeledImage       *labeled_image);
static EelIRect      labeled_image_get_content_bounds     (const EelLabeledImage *labeled_image);
static EelDimensions labeled_image_get_content_dimensions (const EelLabeledImage *labeled_image);
static void          labeled_image_update_alignments      (EelLabeledImage       *labeled_image);
static gboolean      labeled_image_show_label             (const EelLabeledImage *labeled_image);
static gboolean      labeled_image_show_image             (const EelLabeledImage *labeled_image);

static guint labeled_image_signals[LAST_SIGNAL] = { 0 };

EEL_CLASS_BOILERPLATE (EelLabeledImage, eel_labeled_image, GTK_TYPE_CONTAINER)

/* Class init methods */
static void
eel_labeled_image_class_init (EelLabeledImageClass *labeled_image_class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (labeled_image_class);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (labeled_image_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (labeled_image_class);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (labeled_image_class);
	GtkBindingSet *binding_set;

	gobject_class->finalize = eel_labeled_image_finalize;

	/* GObjectClass */
	gobject_class->set_property = eel_labeled_image_set_property;
	gobject_class->get_property = eel_labeled_image_get_property;

	/* GtkObjectClass */
	object_class->destroy = eel_labeled_image_destroy;

 	/* GtkWidgetClass */
 	widget_class->size_request = eel_labeled_image_size_request;
	widget_class->size_allocate = eel_labeled_image_size_allocate;
 	widget_class->expose_event = eel_labeled_image_expose_event;
	widget_class->map = eel_labeled_image_map;
	widget_class->unmap = eel_labeled_image_unmap;
	widget_class->get_accessible = eel_labeled_image_get_accessible;

 	/* GtkContainerClass */
	container_class->add = eel_labeled_image_add;
	container_class->remove = eel_labeled_image_remove;
	container_class->forall = eel_labeled_image_forall;

	labeled_image_signals[ACTIVATE] =
		g_signal_new ("activate",
			      G_TYPE_FROM_CLASS (labeled_image_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EelLabeledImageClass,
					       activate),
			      NULL, NULL, 
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);	
	widget_class->activate_signal = labeled_image_signals[ACTIVATE];

	binding_set = gtk_binding_set_by_class (gobject_class);
	
	gtk_binding_entry_add_signal (binding_set, 
				      GDK_Return, 0,
				      "activate", 0);
	gtk_binding_entry_add_signal (binding_set, 
				      GDK_KP_Enter, 0,
				      "activate", 0);
	gtk_binding_entry_add_signal (binding_set, 
				      GDK_space, 0,
				      "activate", 0);


	/* Properties */
        g_object_class_install_property (
		gobject_class,
		PROP_PIXBUF,
		g_param_spec_object ("pixbuf", NULL, NULL,
				     GDK_TYPE_PIXBUF, G_PARAM_READWRITE));

        g_object_class_install_property (
		gobject_class,
		PROP_LABEL,
		g_param_spec_string ("label", NULL, NULL,
				     "", G_PARAM_READWRITE));


        g_object_class_install_property (
		gobject_class,
		PROP_LABEL_POSITION,
		g_param_spec_enum ("label_position", NULL, NULL,
				   GTK_TYPE_POSITION_TYPE,
				   GTK_POS_BOTTOM,
				   G_PARAM_READWRITE));

        g_object_class_install_property (
		gobject_class,
		PROP_SHOW_LABEL,
		g_param_spec_boolean ("show_label", NULL, NULL,
				      TRUE, G_PARAM_READWRITE)); 
				   
        g_object_class_install_property (
		gobject_class,
		PROP_SHOW_IMAGE,
		g_param_spec_boolean ("show_image", NULL, NULL,
				      TRUE, G_PARAM_READWRITE)); 
				   

	g_object_class_install_property (
		gobject_class,
		PROP_SPACING,
		g_param_spec_uint ("spacing", NULL, NULL,
				   0,
				   G_MAXINT,
				   DEFAULT_SPACING,
				   G_PARAM_READWRITE));

	g_object_class_install_property (
		gobject_class,
		PROP_X_PADDING,
		g_param_spec_int ("x_padding", NULL, NULL,
				  0,
				  G_MAXINT,
				  DEFAULT_X_PADDING,
				  G_PARAM_READWRITE));

	g_object_class_install_property (
		gobject_class,
		PROP_Y_PADDING,
		g_param_spec_int ("y_padding", NULL, NULL,
				  0,
				  G_MAXINT,
				  DEFAULT_Y_PADDING,
				  G_PARAM_READWRITE));

	g_object_class_install_property (
		gobject_class,
		PROP_X_ALIGNMENT,
		g_param_spec_float ("x_alignment", NULL, NULL,
				    0.0,
				    1.0,
				    DEFAULT_X_ALIGNMENT,
				    G_PARAM_READWRITE));
   
	g_object_class_install_property (
		gobject_class,
		PROP_Y_ALIGNMENT,
		g_param_spec_float ("y_alignment", NULL, NULL,
				    0.0,
				    1.0,
				    DEFAULT_Y_ALIGNMENT,
				    G_PARAM_READWRITE));

	g_object_class_install_property (
		gobject_class,
		PROP_FILL,
		g_param_spec_boolean ("fill", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE));
}

static void
eel_labeled_image_init (EelLabeledImage *labeled_image)
{
	GTK_WIDGET_SET_FLAGS (labeled_image, GTK_NO_WINDOW);

	labeled_image->details = g_new0 (EelLabeledImageDetails, 1);
	labeled_image->details->show_label = TRUE;
	labeled_image->details->show_image = TRUE;
	labeled_image->details->label_position = GTK_POS_BOTTOM;
	labeled_image->details->spacing = DEFAULT_SPACING;
	labeled_image->details->x_padding = DEFAULT_X_PADDING;
	labeled_image->details->y_padding = DEFAULT_Y_PADDING;
	labeled_image->details->x_alignment = DEFAULT_X_ALIGNMENT;
	labeled_image->details->y_alignment = DEFAULT_Y_ALIGNMENT;
	labeled_image->details->fixed_image_height = 0;

	eel_labeled_image_set_fill (labeled_image, FALSE);
}

static void
eel_labeled_image_finalize (GObject *object)
{
 	EelLabeledImage *labeled_image;
	
	labeled_image = EEL_LABELED_IMAGE (object);

	g_free (labeled_image->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}


static void
eel_labeled_image_destroy (GtkObject *object)
{
 	EelLabeledImage *labeled_image;
	
	labeled_image = EEL_LABELED_IMAGE (object);

	if (labeled_image->details->image != NULL) {
		gtk_widget_destroy (labeled_image->details->image);
	}

	if (labeled_image->details->label != NULL) {
		gtk_widget_destroy (labeled_image->details->label);
	}

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* GObjectClass methods */
static void
eel_labeled_image_set_property (GObject      *object,
				guint         property_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	EelLabeledImage *labeled_image;
	
	g_assert (EEL_IS_LABELED_IMAGE (object));

 	labeled_image = EEL_LABELED_IMAGE (object);

 	switch (property_id)
	{
	case PROP_PIXBUF:
		eel_labeled_image_set_pixbuf (labeled_image,
					      g_value_get_object (value));
		break;

	case PROP_LABEL:
		eel_labeled_image_set_text (labeled_image, g_value_get_string (value));
		break;

	case PROP_LABEL_POSITION:
		eel_labeled_image_set_label_position (labeled_image,
						      g_value_get_enum (value));
		break;

	case PROP_SHOW_LABEL:
		eel_labeled_image_set_show_label (labeled_image,
						  g_value_get_boolean (value));
		break;

	case PROP_SHOW_IMAGE:
		eel_labeled_image_set_show_image (labeled_image,
						  g_value_get_boolean (value));
		break;

	case PROP_SPACING:
		eel_labeled_image_set_spacing (labeled_image,
					       g_value_get_uint (value));
		break;

	case PROP_X_PADDING:
		eel_labeled_image_set_x_padding (labeled_image,
						 g_value_get_int (value));
		break;

	case PROP_Y_PADDING:
		eel_labeled_image_set_y_padding (labeled_image,
						 g_value_get_int (value));
		break;

	case PROP_X_ALIGNMENT:
		eel_labeled_image_set_x_alignment (labeled_image,
						   g_value_get_float (value));
		break;

	case PROP_Y_ALIGNMENT:
		eel_labeled_image_set_y_alignment (labeled_image,
						   g_value_get_float (value));
		break;

	case PROP_FILL:
		eel_labeled_image_set_fill (labeled_image,
					    g_value_get_boolean (value));
		break;
 	default:
		g_assert_not_reached ();
	}
}

static void
eel_labeled_image_get_property (GObject    *object,
				guint       property_id,
				GValue     *value,
				GParamSpec *pspec)
{
	EelLabeledImage *labeled_image;

	g_assert (EEL_IS_LABELED_IMAGE (object));
	
	labeled_image = EEL_LABELED_IMAGE (object);

 	switch (property_id)
	{
	case PROP_LABEL:
		if (labeled_image->details->label == NULL) {
			g_value_set_string (value, NULL);
		} else {
			g_value_set_string (value, 
					    gtk_label_get_text (GTK_LABEL (
						    labeled_image->details->label)));
		}
		break;

	case PROP_LABEL_POSITION:
		g_value_set_enum (value, eel_labeled_image_get_label_position (labeled_image));
		break;

	case PROP_SHOW_LABEL:
		g_value_set_boolean (value, eel_labeled_image_get_show_label (labeled_image));
		break;

	case PROP_SHOW_IMAGE:
		g_value_set_boolean (value, eel_labeled_image_get_show_image (labeled_image));
		break;

	case PROP_SPACING:
		g_value_set_uint (value, eel_labeled_image_get_spacing (labeled_image));
		break;

	case PROP_X_PADDING:
		g_value_set_int (value, eel_labeled_image_get_x_padding (labeled_image));
		break;

	case PROP_Y_PADDING:
		g_value_set_int (value, eel_labeled_image_get_y_padding (labeled_image));
		break;

	case PROP_X_ALIGNMENT:
		g_value_set_float (value, eel_labeled_image_get_x_alignment (labeled_image));
		break;

	case PROP_Y_ALIGNMENT:
		g_value_set_float (value, eel_labeled_image_get_y_alignment (labeled_image));
		break;

	case PROP_FILL:
		g_value_set_boolean (value, eel_labeled_image_get_fill (labeled_image));
		break;

 	default:
		g_assert_not_reached ();
	}
}

/* GtkWidgetClass methods */
static void
eel_labeled_image_size_request (GtkWidget *widget,
				GtkRequisition *requisition)
{
	EelLabeledImage *labeled_image;
 	EelDimensions content_dimensions;

 	g_assert (EEL_IS_LABELED_IMAGE (widget));
 	g_assert (requisition != NULL);

  	labeled_image = EEL_LABELED_IMAGE (widget);
	
 	content_dimensions = labeled_image_get_content_dimensions (labeled_image);

	requisition->width = 
		MAX (1, content_dimensions.width) +
		2 * labeled_image->details->x_padding;

	requisition->height = 
		MAX (1, content_dimensions.height) +
		2 * labeled_image->details->y_padding;
}

static void
eel_labeled_image_size_allocate (GtkWidget *widget,
				 GtkAllocation *allocation)
{
	EelLabeledImage *labeled_image;
 	EelIRect image_bounds;
	EelIRect label_bounds;

 	g_assert (EEL_IS_LABELED_IMAGE (widget));
 	g_assert (allocation != NULL);

  	labeled_image = EEL_LABELED_IMAGE (widget);

	widget->allocation = *allocation;
	
 	label_bounds = eel_labeled_image_get_label_bounds (labeled_image);
	eel_gtk_container_child_size_allocate (GTK_CONTAINER (widget),
					       labeled_image->details->label,
					       label_bounds);

 	image_bounds = eel_labeled_image_get_image_bounds (labeled_image);
	eel_gtk_container_child_size_allocate (GTK_CONTAINER (widget),
					       labeled_image->details->image,
					       image_bounds);
}

static int
eel_labeled_image_expose_event (GtkWidget *widget,
				GdkEventExpose *event)
{
	EelLabeledImage *labeled_image;
	EelIRect label_bounds;

	g_assert (EEL_IS_LABELED_IMAGE (widget));
	g_assert (GTK_WIDGET_REALIZED (widget));
	g_assert (event != NULL);

  	labeled_image = EEL_LABELED_IMAGE (widget);

	if (GTK_WIDGET_STATE (widget) == GTK_STATE_SELECTED || 
	    GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE) {
		label_bounds = eel_labeled_image_get_label_bounds (EEL_LABELED_IMAGE (widget));

		gtk_paint_flat_box (widget->style,
				    widget->window,
				    GTK_WIDGET_STATE (widget),
				    GTK_SHADOW_NONE,
				    &event->area,
				    widget,
				    "eel-labeled-image",
				    label_bounds.x0, label_bounds.y0,
				    label_bounds.x1 - label_bounds.x0, 
				    label_bounds.y1 - label_bounds.y0);
	}

	if (labeled_image_show_label (labeled_image)) {
		eel_gtk_container_child_expose_event (GTK_CONTAINER (widget),
						      labeled_image->details->label,
						      event);
	}
	
	if (labeled_image_show_image (labeled_image)) {
		eel_gtk_container_child_expose_event (GTK_CONTAINER (widget),
						      labeled_image->details->image,
						      event);
	}

	if (GTK_WIDGET_HAS_FOCUS (widget)) {
		label_bounds = eel_labeled_image_get_image_bounds (EEL_LABELED_IMAGE (widget));
		gtk_paint_focus (widget->style, widget->window,
				 GTK_STATE_NORMAL,
				 &event->area, widget,
				 "eel-focusable-labeled-image",
				 label_bounds.x0, label_bounds.y0,
				 label_bounds.x1 - label_bounds.x0, 
				 label_bounds.y1 - label_bounds.y0);
	}		

	return FALSE;
}

static void
eel_labeled_image_map (GtkWidget *widget)
{
	EelLabeledImage *labeled_image;

	g_assert (EEL_IS_LABELED_IMAGE (widget));
	
	labeled_image = EEL_LABELED_IMAGE (widget);

 	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
	
	if (labeled_image_show_label (labeled_image)) {
		eel_gtk_container_child_map (GTK_CONTAINER (widget), labeled_image->details->label);
	}

	if (labeled_image_show_image (labeled_image)) {
		eel_gtk_container_child_map (GTK_CONTAINER (widget), labeled_image->details->image);
	}
}

static void
eel_labeled_image_unmap (GtkWidget *widget)
{
	EelLabeledImage *labeled_image;

	g_assert (EEL_IS_LABELED_IMAGE (widget));
	
	labeled_image = EEL_LABELED_IMAGE (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
	
	eel_gtk_container_child_unmap (GTK_CONTAINER (widget), labeled_image->details->label);
	eel_gtk_container_child_unmap (GTK_CONTAINER (widget), labeled_image->details->image);
}

/* GtkContainerClass methods */
static void
eel_labeled_image_add (GtkContainer *container,
		       GtkWidget *child)
{
	g_assert (GTK_IS_LABEL (child) || GTK_IS_IMAGE (child));

	eel_gtk_container_child_add (container, child);
}

static void
eel_labeled_image_remove (GtkContainer *container,
			  GtkWidget *child)
{
	EelLabeledImage *labeled_image;
	
	g_assert (GTK_IS_LABEL (child) || GTK_IS_IMAGE (child));
	
	labeled_image = EEL_LABELED_IMAGE (container);;

	g_assert (child == labeled_image->details->image || child == labeled_image->details->label);

	eel_gtk_container_child_remove (container, child);

	if (labeled_image->details->image == child) {
		labeled_image->details->image = NULL;
	}

	if (labeled_image->details->label == child) {
		labeled_image->details->label = NULL;
	}
}

static void
eel_labeled_image_forall (GtkContainer *container,
			  gboolean include_internals,
			  GtkCallback callback,
			  gpointer callback_data)
{
	EelLabeledImage *labeled_image;
	
	g_assert (EEL_IS_LABELED_IMAGE (container));
	g_assert (callback != NULL);
	
	labeled_image = EEL_LABELED_IMAGE (container);

	if (include_internals) {
		if (labeled_image->details->image != NULL) {
			(* callback) (labeled_image->details->image, callback_data);
		}
		
		if (labeled_image->details->label != NULL) {
			(* callback) (labeled_image->details->label, callback_data);
		}
	}
}

/* Private EelLabeledImage methods */
static gboolean
is_fixed_height (const EelLabeledImage *labeled_image)
{
	return labeled_image->details->fixed_image_height > 0;
}

static EelDimensions
labeled_image_get_image_dimensions (const EelLabeledImage *labeled_image)
{
	EelDimensions image_dimensions;
	GtkRequisition image_requisition;	

	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));

	if (!labeled_image_show_image (labeled_image)) {
		return eel_dimensions_empty;
	}
	
	gtk_widget_size_request (labeled_image->details->image, &image_requisition);

	image_dimensions.width = (int) image_requisition.width;
	image_dimensions.height = (int) image_requisition.height;

	if (is_fixed_height (labeled_image)) {
		image_dimensions.height = labeled_image->details->fixed_image_height;
	}

	return image_dimensions;
}

static EelDimensions
labeled_image_get_label_dimensions (const EelLabeledImage *labeled_image)
{
	EelDimensions label_dimensions;
	GtkRequisition label_requisition;	

	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));

	if (!labeled_image_show_label (labeled_image)) {
		return eel_dimensions_empty;
	}
	
	gtk_widget_size_request (labeled_image->details->label, &label_requisition);
	
	label_dimensions.width = (int) label_requisition.width;
	label_dimensions.height = (int) label_requisition.height;

	return label_dimensions;
}

static EelIRect
labeled_image_get_image_bounds_fill (const EelLabeledImage *labeled_image)
{
	EelIRect image_bounds;
	EelDimensions image_dimensions;
	EelIRect content_bounds;
	EelIRect bounds;

	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));

	image_dimensions = labeled_image_get_image_dimensions (labeled_image);

	if (eel_dimensions_are_empty (image_dimensions)) {
		return eel_irect_empty;
	}

	content_bounds = labeled_image_get_content_bounds (labeled_image);
	bounds = eel_gtk_widget_get_bounds (GTK_WIDGET (labeled_image));
	
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

		default:
			image_bounds.x0 = 0;
			image_bounds.y0 = 0;
			image_bounds.x1 = 0;
			image_bounds.y1 = 0;
			g_assert_not_reached ();
		}
	}

	return image_bounds;
}

EelIRect
eel_labeled_image_get_image_bounds (const EelLabeledImage *labeled_image)
{
	EelDimensions image_dimensions;
	EelDimensions label_dimensions;
	GtkRequisition image_requisition;
	EelIRect image_bounds;
	EelIRect content_bounds;

	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), eel_irect_empty);

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

	if (eel_dimensions_are_empty (image_dimensions)) {
		return eel_irect_empty;
	}

	content_bounds = labeled_image_get_content_bounds (labeled_image);
	
	if (!labeled_image_show_label (labeled_image)) {
		image_bounds.x0 = 
			content_bounds.x0 +
			(eel_irect_get_width (content_bounds) - image_dimensions.width) / 2;
		image_bounds.y0 = 
			content_bounds.y0 +
			(eel_irect_get_height (content_bounds) - image_dimensions.height) / 2;
	} else {
		switch (labeled_image->details->label_position) {
		case GTK_POS_LEFT:
			image_bounds.x0 = content_bounds.x1 - image_dimensions.width;
			image_bounds.y0 = 
				content_bounds.y0 +
				(eel_irect_get_height (content_bounds) - image_dimensions.height) / 2;
			break;

		case GTK_POS_RIGHT:
			image_bounds.x0 = content_bounds.x0;
			image_bounds.y0 = 
				content_bounds.y0 +
				(eel_irect_get_height (content_bounds) - image_dimensions.height) / 2;
			break;

		case GTK_POS_TOP:
			image_bounds.x0 = 
				content_bounds.x0 +
				(eel_irect_get_width (content_bounds) - image_dimensions.width) / 2;
			image_bounds.y0 = content_bounds.y1 - image_dimensions.height;
			break;

		case GTK_POS_BOTTOM:
			image_bounds.x0 = 
				content_bounds.x0 +
				(eel_irect_get_width (content_bounds) - image_dimensions.width) / 2;
				
			if (is_fixed_height (labeled_image)) {	
				image_bounds.y0 = content_bounds.y0 + eel_irect_get_height (content_bounds)
					- image_dimensions.height
					- label_dimensions.height
					- labeled_image->details->spacing;
			} else {
				image_bounds.y0 = content_bounds.y0;
			}	

			break;

		default:
			image_bounds.x0 = 0;
			image_bounds.y0 = 0;
			g_assert_not_reached ();
		}
	}
	
	image_bounds.x1 = image_bounds.x0 + image_dimensions.width;
	image_bounds.y1 = image_bounds.y0 + image_dimensions.height;

	return image_bounds;
}

static EelIRect
labeled_image_get_label_bounds_fill (const EelLabeledImage *labeled_image)
{
	EelIRect label_bounds;
	EelDimensions label_dimensions;
	EelIRect content_bounds;
	EelIRect bounds;

	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));

	label_dimensions = labeled_image_get_label_dimensions (labeled_image);

	if (eel_dimensions_are_empty (label_dimensions)) {
		return eel_irect_empty;
	}

	content_bounds = labeled_image_get_content_bounds (labeled_image);
	bounds = eel_gtk_widget_get_bounds (GTK_WIDGET (labeled_image));

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

		default:
			label_bounds.x0 = 0;
			label_bounds.y0 = 0;
			label_bounds.x1 = 0;
			label_bounds.y1 = 0;
			g_assert_not_reached ();
		}
	}

	return label_bounds;
}

EelIRect
eel_labeled_image_get_label_bounds (const EelLabeledImage *labeled_image)
{
	EelIRect label_bounds;
	EelDimensions label_dimensions;
	EelIRect content_bounds;

	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), eel_irect_empty);

	if (labeled_image->details->fill) {
		return labeled_image_get_label_bounds_fill (labeled_image);
	}

	label_dimensions = labeled_image_get_label_dimensions (labeled_image);

	if (eel_dimensions_are_empty (label_dimensions)) {
		return eel_irect_empty;
	}

	content_bounds = labeled_image_get_content_bounds (labeled_image);

	/* Only the label is shown */
	if (!labeled_image_show_image (labeled_image)) {
		label_bounds.x0 = 
			content_bounds.x0 +
			(eel_irect_get_width (content_bounds) - label_dimensions.width) / 2;
		label_bounds.y0 = 
			content_bounds.y0 +
			(eel_irect_get_height (content_bounds) - label_dimensions.height) / 2;
		/* Both label and image are shown */
	} else {
		switch (labeled_image->details->label_position) {
		case GTK_POS_LEFT:
			label_bounds.x0 = content_bounds.x0;
			label_bounds.y0 = 
				content_bounds.y0 +
				(eel_irect_get_height (content_bounds) - label_dimensions.height) / 2;
			break;

		case GTK_POS_RIGHT:
			label_bounds.x0 = content_bounds.x1 - label_dimensions.width;
			label_bounds.y0 = 
				content_bounds.y0 +
				(eel_irect_get_height (content_bounds) - label_dimensions.height) / 2;
			break;

		case GTK_POS_TOP:
			label_bounds.x0 = 
				content_bounds.x0 +
				(eel_irect_get_width (content_bounds) - label_dimensions.width) / 2;
			label_bounds.y0 = content_bounds.y0;
			break;

		case GTK_POS_BOTTOM:
			label_bounds.x0 = 
				content_bounds.x0 +
				(eel_irect_get_width (content_bounds) - label_dimensions.width) / 2;
			label_bounds.y0 = content_bounds.y1 - label_dimensions.height;
			break;

		default:
			label_bounds.x0 = 0;
			label_bounds.y0 = 0;
			g_assert_not_reached ();
		}
	}
	
	label_bounds.x1 = label_bounds.x0 + label_dimensions.width;
	label_bounds.y1 = label_bounds.y0 + label_dimensions.height;

	return label_bounds;
}

static void
labeled_image_update_alignments (EelLabeledImage *labeled_image)
{

	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));

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

static EelDimensions
labeled_image_get_content_dimensions (const EelLabeledImage *labeled_image)
{
	EelDimensions image_dimensions;
	EelDimensions label_dimensions;
	EelDimensions content_dimensions;

	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));

	image_dimensions = labeled_image_get_image_dimensions (labeled_image);
	label_dimensions = labeled_image_get_label_dimensions (labeled_image);

	content_dimensions = eel_dimensions_empty;

	/* Both shown */
	if (!eel_dimensions_are_empty (image_dimensions) && !eel_dimensions_are_empty (label_dimensions)) {
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
	} else if (!eel_dimensions_are_empty (image_dimensions)) {
		content_dimensions.width = image_dimensions.width;
		content_dimensions.height = image_dimensions.height;
		/* Only label shown */
	} else {
		content_dimensions.width = label_dimensions.width;
		content_dimensions.height = label_dimensions.height;
	}

	return content_dimensions;
}

static EelIRect
labeled_image_get_content_bounds (const EelLabeledImage *labeled_image)
{
	EelDimensions content_dimensions;
	EelIRect content_bounds;
	EelIRect bounds;

	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));

	bounds = eel_gtk_widget_get_bounds (GTK_WIDGET (labeled_image));

	content_dimensions = labeled_image_get_content_dimensions (labeled_image);
	content_bounds = eel_irect_align (bounds,
					      content_dimensions.width,
					      content_dimensions.height,
					      labeled_image->details->x_alignment,
					      labeled_image->details->y_alignment);

	return content_bounds;
}

static void
labeled_image_ensure_label (EelLabeledImage *labeled_image)
{
	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->label != NULL) {
		return;
	}

 	labeled_image->details->label = gtk_label_new (NULL);
	gtk_container_add (GTK_CONTAINER (labeled_image), labeled_image->details->label);
	gtk_widget_show (labeled_image->details->label);
}

static void
labeled_image_ensure_image (EelLabeledImage *labeled_image)
{
	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->image != NULL) {
		return;
	}

 	labeled_image->details->image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (labeled_image), labeled_image->details->image);
	gtk_widget_show (labeled_image->details->image);
}

static gboolean
labeled_image_show_image (const EelLabeledImage *labeled_image)
{
	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));
	
	return labeled_image->details->image != NULL && labeled_image->details->show_image;
}

static gboolean
labeled_image_show_label (const EelLabeledImage *labeled_image)
{
	g_assert (EEL_IS_LABELED_IMAGE (labeled_image));

	return labeled_image->details->label != NULL && labeled_image->details->show_label;
}

/**
 * eel_labeled_image_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Returns A newly allocated EelLabeledImage.  If the &text parameter is not
 * NULL then the LabeledImage will show a label.  If the &pixbuf parameter is not
 * NULL then the LabeledImage will show a pixbuf.  Either of these can be NULL at
 * creation time.  
 *
 * Later in the lifetime of the widget you can invoke methods that affect the 
 * label and/or the image.  If at creation time these were NULL, then they will
 * be created as neeeded.
 *
 * Thus, using this widget in place of EelImage or EelLabel is "free" with
 * only the GtkObject and function call overhead.
 *
 */
GtkWidget*
eel_labeled_image_new (const char *text,
		       GdkPixbuf *pixbuf)
{
	EelLabeledImage *labeled_image;

	labeled_image = EEL_LABELED_IMAGE (gtk_widget_new (eel_labeled_image_get_type (), NULL));
	
	if (text != NULL) {
		eel_labeled_image_set_text (labeled_image, text);
	}

	if (pixbuf != NULL) {
		eel_labeled_image_set_pixbuf (labeled_image, pixbuf);
	}

	labeled_image_update_alignments (labeled_image);

	return GTK_WIDGET (labeled_image);
}

/**
 * eel_labeled_image_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @file_name: File name of picture to use for pixbuf.  Cannot be NULL.
 *
 * Returns A newly allocated EelLabeledImage.  If the &text parameter is not
 * NULL then the LabeledImage will show a label.
 *
 */
GtkWidget*
eel_labeled_image_new_from_file_name (const char *text,
				      const char *pixbuf_file_name)
{
	EelLabeledImage *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);

	labeled_image = EEL_LABELED_IMAGE (eel_labeled_image_new (text, NULL));
	eel_labeled_image_set_pixbuf_from_file_name (labeled_image, pixbuf_file_name);
	return GTK_WIDGET (labeled_image);
}

/**
 * eel_labeled_image_set_label_position:
 * @labeled_image: A EelLabeledImage.
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
eel_labeled_image_set_label_position (EelLabeledImage *labeled_image,
				      GtkPositionType label_position)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));
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
 * eel_labeled_image_get_label_postiion:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns an enumeration indicating the position of the label with respect to the image.
 */
GtkPositionType
eel_labeled_image_get_label_position (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->label_position;
}

/**
 * eel_labeled_image_set_show_label:
 * @labeled_image: A EelLabeledImage.
 * @show_image: A boolean value indicating whether the label should be shown.
 *
 * Update the labeled image to either show or hide the internal label widget.
 * This function doesnt have any effect if the LabeledImage doesnt already
 * contain an label.
 */
void
eel_labeled_image_set_show_label (EelLabeledImage *labeled_image,
				  gboolean show_label)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));
	
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
 * eel_labeled_image_get_show_label:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns a boolean value indicating whether the internal label is shown.
 */
gboolean
eel_labeled_image_get_show_label (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->show_label;
}

/**
 * eel_labeled_image_set_show_image:
 * @labeled_image: A EelLabeledImage.
 * @show_image: A boolean value indicating whether the image should be shown.
 *
 * Update the labeled image to either show or hide the internal image widget.
 * This function doesnt have any effect if the LabeledImage doesnt already
 * contain an image.
 */
void
eel_labeled_image_set_show_image (EelLabeledImage *labeled_image,
				  gboolean show_image)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));
	
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
 * eel_labeled_image_get_show_image:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns a boolean value indicating whether the internal image is shown.
 */
gboolean
eel_labeled_image_get_show_image (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->show_image;
}


/**
 * eel_labeled_image_set_fixed_image_height:
 * @labeled_image: A EelLabeledImage.
 * @fixed_image_height: The new fixed image height.
 *
 * Normally, we measure the height of images, but it's sometimes useful
 * to use a fixed height for all the images.  This routine sets the
 * image height to the passed in value
 *
 */
void
eel_labeled_image_set_fixed_image_height (EelLabeledImage *labeled_image,
					  int new_height)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->fixed_image_height == new_height) {
		return;
	}
	
	labeled_image->details->fixed_image_height = new_height;

	labeled_image_update_alignments (labeled_image);
	
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * eel_labeled_image_set_selected:
 * @labeled_image: A EelLabeledImage.
 * @selected: A boolean value indicating whether the labeled image
 * should be selected.
 *
 * Selects or deselects the labeled image.
 *
 */
void            
eel_labeled_image_set_selected (EelLabeledImage *labeled_image,
				gboolean selected)
{
	GtkStateType state;
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	state = selected ? GTK_STATE_SELECTED : GTK_STATE_NORMAL;

	gtk_widget_set_state (GTK_WIDGET (labeled_image), state);
	gtk_widget_set_state (labeled_image->details->image, state);
	gtk_widget_set_state (labeled_image->details->label, state);
}

/**
 * eel_labeled_image_get_selected:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns the selected state of the labeled image.
 *
 */
gboolean        
eel_labeled_image_get_selected (EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), FALSE);

	return GTK_WIDGET (labeled_image)->state == GTK_STATE_SELECTED;
}

/**
 * eel_labeled_image_set_spacing:
 * @labeled_image: A EelLabeledImage.
 * @spacing: The new spacing between label and image.
 *
 * Set the spacing between label and image.  This will only affect
 * the geometry of the widget if both a label and image are currently
 * visible.
 *
 */
void
eel_labeled_image_set_spacing (EelLabeledImage *labeled_image,
			       guint spacing)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->spacing == spacing) {
		return;
	}
	
	labeled_image->details->spacing = spacing;

	labeled_image_update_alignments (labeled_image);
	
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * eel_labeled_image_get_spacing:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns: The spacing between the label and image.
 */
guint
eel_labeled_image_get_spacing (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->spacing;
}

/**
 * eel_labeled_image_set_x_padding:
 * @labeled_image: A EelLabeledImage.
 * @x_padding: The new horizontal padding.
 *
 * Set horizontal padding for the EelLabeledImage.  The padding
 * attribute work just like that in GtkMisc.
 */
void
eel_labeled_image_set_x_padding (EelLabeledImage *labeled_image,
				 int x_padding)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	x_padding = MAX (0, x_padding);
	
	if (labeled_image->details->x_padding == x_padding) {
		return;
	}
	
	labeled_image->details->x_padding = x_padding;
	labeled_image_update_alignments (labeled_image);
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * eel_labeled_image_get_x_padding:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns: The horizontal padding for the LabeledImage's content.
 */
int
eel_labeled_image_get_x_padding (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);

	return labeled_image->details->x_padding;
}

/**
 * eel_labeled_image_set_y_padding:
 * @labeled_image: A EelLabeledImage.
 * @x_padding: The new vertical padding.
 *
 * Set vertical padding for the EelLabeledImage.  The padding
 * attribute work just like that in GtkMisc.
 */
void
eel_labeled_image_set_y_padding (EelLabeledImage *labeled_image,
				 int y_padding)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	y_padding = MAX (0, y_padding);
	
	if (labeled_image->details->y_padding == y_padding) {
		return;
	}
	
	labeled_image->details->y_padding = y_padding;
	labeled_image_update_alignments (labeled_image);
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * eel_labeled_image_get_x_padding:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns: The vertical padding for the LabeledImage's content.
 */
int
eel_labeled_image_get_y_padding (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);

	return labeled_image->details->y_padding;
}

/**
 * eel_labeled_image_set_x_alignment:
 * @labeled_image: A EelLabeledImage.
 * @x_alignment: The new horizontal alignment.
 *
 * Set horizontal alignment for the EelLabeledImage's content.
 * The 'content' is the union of the image and label.  The alignment
 * attribute work just like that in GtkMisc.
 */
void
eel_labeled_image_set_x_alignment (EelLabeledImage *labeled_image,
				   float x_alignment)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	x_alignment = MAX (0, x_alignment);
	x_alignment = MIN (1.0, x_alignment);
	
	if (labeled_image->details->x_alignment == x_alignment) {
		return;
	}
	
	labeled_image->details->x_alignment = x_alignment;
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * eel_labeled_image_get_x_alignment:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns: The horizontal alignment for the LabeledImage's content.
 */
float
eel_labeled_image_get_x_alignment (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);

	return labeled_image->details->x_alignment;
}

/**
 * eel_labeled_image_set_y_alignment:
 * @labeled_image: A EelLabeledImage.
 * @y_alignment: The new vertical alignment.
 *
 * Set vertical alignment for the EelLabeledImage's content.
 * The 'content' is the union of the image and label.  The alignment
 * attribute work just like that in GtkMisc.
 */
void
eel_labeled_image_set_y_alignment (EelLabeledImage *labeled_image,
				   float y_alignment)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	y_alignment = MAX (0, y_alignment);
	y_alignment = MIN (1.0, y_alignment);
	
	if (labeled_image->details->y_alignment == y_alignment) {
		return;
	}
	
	labeled_image->details->y_alignment = y_alignment;
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * eel_labeled_image_get_y_alignment:
 * @labeled_image: A EelLabeledImage.
 *
 * Returns: The vertical alignment for the LabeledImage's content.
 */
float
eel_labeled_image_get_y_alignment (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);

	return labeled_image->details->y_alignment;
}

/**
 * eel_labeled_image_set_fill:
 * @labeled_image: A EelLabeledImage.
 * @fill: A boolean value indicating whether the internal image and label
 * widgets should fill all the available allocation.
 *
 * By default the internal image and label wigets are sized to their natural
 * preferred geometry.  You can use the 'fill' attribute of LabeledImage
 * to have the internal widgets fill as much of the LabeledImage allocation
 * as is available.
 */
void
eel_labeled_image_set_fill (EelLabeledImage *labeled_image,
			    gboolean fill)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));
	
	if (labeled_image->details->fill == fill) {
		return;
	}

	labeled_image->details->fill = fill;

	labeled_image_update_alignments (labeled_image);
	
	gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
}

/**
 * eel_labeled_image_get_fill:
 * @labeled_image: A EelLabeledImage.
 *
 * Retruns a boolean value indicating whether the internal widgets fill
 * all the available allocation.
 */
gboolean
eel_labeled_image_get_fill (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), 0);
	
	return labeled_image->details->fill;
}

static void
eel_labled_set_mnemonic_widget (GtkWidget *image_widget,
				GtkWidget *mnemonic_widget)
{
	EelLabeledImage *image;

	g_assert (EEL_IS_LABELED_IMAGE (image_widget));

	image = EEL_LABELED_IMAGE (image_widget);

	if (image->details->label)
		gtk_label_set_mnemonic_widget
			(GTK_LABEL (image->details->label), mnemonic_widget);
}

/**
 * eel_labeled_image_button_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Create a stock GtkButton with a EelLabeledImage child.
 *
 */
GtkWidget *
eel_labeled_image_button_new (const char *text,
			      GdkPixbuf *pixbuf)
{
	GtkWidget *button;
	GtkWidget *labeled_image;
	
	button = g_object_new (eel_labeled_image_button_get_type (), NULL);
	labeled_image = eel_labeled_image_new (text, pixbuf);
	gtk_container_add (GTK_CONTAINER (button), labeled_image);
	eel_labled_set_mnemonic_widget (labeled_image, button);
	gtk_widget_show (labeled_image);
	
	return button;
}

/**
 * eel_labeled_image_button_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @pixbuf_file_name: Name of pixbuf to use for image.  Cannot be NULL.
 *
 * Create a stock GtkToggleButton with a EelLabeledImage child.
 *
 */
GtkWidget *
eel_labeled_image_button_new_from_file_name (const char *text,
					     const char *pixbuf_file_name)
{
	GtkWidget *button;
	GtkWidget *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);
	
	button = g_object_new (eel_labeled_image_button_get_type (), NULL);
	labeled_image = eel_labeled_image_new_from_file_name (text, pixbuf_file_name);
	gtk_container_add (GTK_CONTAINER (button), labeled_image);
	eel_labled_set_mnemonic_widget (labeled_image, button);
	gtk_widget_show (labeled_image);
	
	return button;
}

/**
 * eel_labeled_image_toggle_button_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Create a stock GtkToggleButton with a EelLabeledImage child.
 *
 */
GtkWidget *
eel_labeled_image_toggle_button_new (const char *text,
				     GdkPixbuf *pixbuf)
{
	GtkWidget *toggle_button;
	GtkWidget *labeled_image;
	
	toggle_button = g_object_new (eel_labeled_image_toggle_button_get_type (), NULL);
	labeled_image = eel_labeled_image_new (text, pixbuf);
	gtk_container_add (GTK_CONTAINER (toggle_button), labeled_image);
	eel_labled_set_mnemonic_widget (labeled_image, toggle_button);
	gtk_widget_show (labeled_image);
	
	return toggle_button;
}

/**
 * eel_labeled_image_toggle_button_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @pixbuf_file_name: Name of pixbuf to use for image.  Cannot be NULL.
 *
 * Create a stock GtkToggleButton with a EelLabeledImage child.
 *
 */
GtkWidget *
eel_labeled_image_toggle_button_new_from_file_name (const char *text,
						    const char *pixbuf_file_name)
{
	GtkWidget *toggle_button;
	GtkWidget *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);
	
	toggle_button = g_object_new (eel_labeled_image_toggle_button_get_type (), NULL);
	labeled_image = eel_labeled_image_new_from_file_name (text, pixbuf_file_name);
	gtk_container_add (GTK_CONTAINER (toggle_button), labeled_image);
	eel_labled_set_mnemonic_widget (labeled_image, toggle_button);
	gtk_widget_show (labeled_image);
	
	return toggle_button;
}

/**
 * eel_labeled_image_toggle_button_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Create a stock GtkToggleButton with a EelLabeledImage child.
 *
 * Returns: the new radio button.
 */
GtkWidget *
eel_labeled_image_radio_button_new (const char *text,
				    GdkPixbuf  *pixbuf)
{
	GtkWidget *radio_button;
	GtkWidget *labeled_image;
	
	radio_button = g_object_new (eel_labeled_image_radio_button_get_type (), NULL);
	labeled_image = eel_labeled_image_new (text, pixbuf);
	gtk_container_add (GTK_CONTAINER (radio_button), labeled_image);
	eel_labled_set_mnemonic_widget (labeled_image, radio_button);
	gtk_widget_show (labeled_image);
	
	return radio_button;
}

/**
 * eel_labeled_image_radio_button_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @pixbuf_file_name: Name of pixbuf to use for image.  Cannot be NULL.
 *
 * Create a stock GtkRadioButton with a EelLabeledImage child.
 *
 * Returns: the new radio button.
 */
GtkWidget *
eel_labeled_image_radio_button_new_from_file_name (const char *text,
						   const char *pixbuf_file_name)
{
	GtkWidget *radio_button;
	GtkWidget *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);
	
	radio_button = g_object_new (eel_labeled_image_radio_button_get_type (), NULL);
	labeled_image = eel_labeled_image_new_from_file_name (text, pixbuf_file_name);
	gtk_container_add (GTK_CONTAINER (radio_button), labeled_image);
	eel_labled_set_mnemonic_widget (labeled_image, radio_button);
	gtk_widget_show (labeled_image);
	
	return radio_button;
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
	g_assert (GTK_IS_WIDGET (widget));

	if (GTK_WIDGET_DRAWABLE (widget)) {
		const int fudge = 4;
		EelIRect bounds;

		bounds = eel_gtk_widget_get_bounds (widget);
		
		bounds.x0 -= fudge;
		bounds.y0 -= fudge;
		bounds.x1 += fudge;
		bounds.y1 += fudge;
		
		gtk_widget_queue_draw_area (widget->parent,
					    bounds.x0,
					    bounds.y0,
					    eel_irect_get_width (bounds),
					    eel_irect_get_height (bounds));
	}
}

static gint
button_focus_out_event_callback (GtkWidget *widget,
				 GdkEventFocus *event,
				 gpointer callback_data)
{
	g_assert (GTK_IS_WIDGET (widget));

	button_leave_callback (widget, callback_data);

	return FALSE;
}

/**
 * eel_labeled_image_check_button_new:
 * @text: Text to use for label or NULL.
 * @pixbuf: Pixbuf to use for image or NULL.
 *
 * Create a stock GtkCheckButton with a EelLabeledImage child.
 *
 */
GtkWidget *
eel_labeled_image_check_button_new (const char *text,
				    GdkPixbuf *pixbuf)
{
	GtkWidget *check_button;
	GtkWidget *labeled_image;
	
	check_button = g_object_new (eel_labeled_image_check_button_get_type (), NULL);
	labeled_image = eel_labeled_image_new (text, pixbuf);
	gtk_container_add (GTK_CONTAINER (check_button), labeled_image);
	eel_labled_set_mnemonic_widget (labeled_image, check_button);
	gtk_widget_show (labeled_image);
	
	/*
	 * Workaround some bugs in GtkCheckButton where the widget 
	 * does not redraw properly after leave or focus out events
	 */
	g_signal_connect (check_button, "leave",
			  G_CALLBACK (button_leave_callback), NULL);
	g_signal_connect (check_button, "focus_out_event",
			  G_CALLBACK (button_focus_out_event_callback), NULL);
	
	return check_button;
}

/**
 * eel_labeled_image_check_button_new_from_file_name:
 * @text: Text to use for label or NULL.
 * @pixbuf_file_name: Name of pixbuf to use for image.  Cannot be NULL.
 *
 * Create a stock GtkCheckButton with a EelLabeledImage child.
 *
 */
GtkWidget *
eel_labeled_image_check_button_new_from_file_name (const char *text,
						   const char *pixbuf_file_name)
{
	GtkWidget *check_button;
	GtkWidget *labeled_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);
	
	check_button = g_object_new (eel_labeled_image_check_button_get_type (), NULL);
	labeled_image = eel_labeled_image_new_from_file_name (text, pixbuf_file_name);
	gtk_container_add (GTK_CONTAINER (check_button), labeled_image);
	eel_labled_set_mnemonic_widget (labeled_image, check_button);
	gtk_widget_show (labeled_image);
	
	return check_button;
}

/*
 * The rest of the methods are proxies for those in EelImage and 
 * EelLabel.  We have all these so that we dont have to expose 
 * our internal widgets at all.  Probably more of these will be added
 * as they are needed.
 */

/**
 * eel_labeled_image_set_pixbuf:
 * @labaled_image: A EelLabeledImage.
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
eel_labeled_image_set_pixbuf (EelLabeledImage *labeled_image,
			      GdkPixbuf *pixbuf)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	if (pixbuf == NULL) {
		if (labeled_image->details->image != NULL) {
			gtk_widget_destroy (labeled_image->details->image);
			labeled_image->details->image = NULL;
		}
		
		gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
	} else {
		labeled_image_ensure_image (labeled_image);
		gtk_image_set_from_pixbuf (GTK_IMAGE (labeled_image->details->image), pixbuf);
	}
}

void
eel_labeled_image_set_pixbuf_from_file_name (EelLabeledImage *labeled_image,
					     const char *pixbuf_file_name)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	labeled_image_ensure_image (labeled_image);
	gtk_image_set_from_file (GTK_IMAGE (labeled_image->details->image), pixbuf_file_name);
}

/**
 * eel_labeled_image_set_text:
 * @labaled_image: A EelLabeledImage.
 * @text: New text (with mnemnonic) to use or NULL.
 *
 * Change the text displayed by the LabeledImage.  Note that the widget display
 * is only updated if the show_label attribute is TRUE.
 *
 * If no internal label widget exists as of yet, a new one will be created.
 *
 * A NULL &text will cause the internal label widget (if alive) to be destroyed.
 */
void
eel_labeled_image_set_text (EelLabeledImage *labeled_image,
			    const char *text)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	if (text == NULL) {
		if (labeled_image->details->label) {
			gtk_widget_destroy (labeled_image->details->label);
			labeled_image->details->label = NULL;
		}
		
		gtk_widget_queue_resize (GTK_WIDGET (labeled_image));
	} else {
		labeled_image_ensure_label (labeled_image);
		gtk_label_set_text_with_mnemonic
			(GTK_LABEL (labeled_image->details->label), text);
	}
}

char *
eel_labeled_image_get_text (const EelLabeledImage *labeled_image)
{
	g_return_val_if_fail (EEL_IS_LABELED_IMAGE (labeled_image), NULL);
	
	if (labeled_image->details->label == NULL) {
		return NULL;
	}

	return g_strdup (gtk_label_get_text (GTK_LABEL (labeled_image->details->label)));
}

void
eel_labeled_image_set_can_focus (EelLabeledImage *labeled_image,
				 gboolean         can_focus)
{
	if (can_focus) {
		GTK_WIDGET_SET_FLAGS
			(GTK_WIDGET (labeled_image), GTK_CAN_FOCUS);
		
	} else {
		GTK_WIDGET_UNSET_FLAGS
			(GTK_WIDGET (labeled_image), GTK_CAN_FOCUS);
	}
}

static AtkObjectClass *a11y_parent_class = NULL;

static void
eel_labeled_image_accessible_initialize (AtkObject *accessible,
					 gpointer   widget)
{
	a11y_parent_class->initialize (accessible, widget);
}

static EelLabeledImage *
get_image (gpointer object)
{
	GtkWidget *widget;

	if (!(widget = GTK_ACCESSIBLE (object)->widget)) {
		return NULL;
	}
	
	if (GTK_IS_BUTTON (widget))
		widget = GTK_BIN (widget)->child;

	return EEL_LABELED_IMAGE (widget);
}

static G_CONST_RETURN gchar *
eel_labeled_image_accessible_get_name (AtkObject *accessible)
{
	EelLabeledImage *labeled_image;

	labeled_image = get_image (accessible);

	if (labeled_image && labeled_image->details &&
	    labeled_image->details->label)
		return gtk_label_get_text
			(GTK_LABEL (labeled_image->details->label));

	g_warning ("no label on '%p'", labeled_image);

	return NULL;
}

static void
eel_labeled_image_accessible_image_get_size (AtkImage *image,
					     gint     *width,
					     gint     *height)
{
	EelLabeledImage *labeled_image;

	labeled_image = get_image (image);

	if (!labeled_image || !labeled_image->details->image) {
		*width = *height = 0;
		return;
	}

	*width = labeled_image->details->image->allocation.width;
	*height = labeled_image->details->image->allocation.height;
}

static void
eel_labeled_image_accessible_image_interface_init (AtkImageIface *iface)
{
	iface->get_image_size = eel_labeled_image_accessible_image_get_size;
}

static void
eel_labeled_image_accessible_class_init (AtkObjectClass *klass)
{
	a11y_parent_class = g_type_class_peek_parent (klass);

	klass->get_name = eel_labeled_image_accessible_get_name;
	klass->initialize = eel_labeled_image_accessible_initialize;
}

enum {
	BUTTON,
	CHECK,
	TOGGLE,
	RADIO,
	PLAIN,
	LAST_ONE
};

static AtkObject *
eel_labeled_image_get_accessible (GtkWidget *widget)
{
	int i;
	static GType types[LAST_ONE] = { 0 };
	const char *tname;
	AtkRole role;
	AtkObject *accessible;

	if ((accessible = eel_accessibility_get_atk_object (widget)))
		return accessible;

	if (GTK_IS_CHECK_BUTTON (widget)) {
		i = BUTTON;
		role = ATK_ROLE_CHECK_BOX;
		tname = "EelLabeledImageCheckButtonAccessible";

	} else if (GTK_IS_TOGGLE_BUTTON (widget)) {
		i = CHECK;
		role = ATK_ROLE_TOGGLE_BUTTON;
		tname = "EelLabeledImageToggleButtonAccessible";

	} else if (GTK_IS_RADIO_BUTTON (widget)) {
		i = RADIO;
		role = ATK_ROLE_RADIO_BUTTON;
		tname = "EelLabeledImageRadioButtonAccessible";

	} else if (GTK_IS_BUTTON (widget)) {
		i = TOGGLE;
		role = ATK_ROLE_PUSH_BUTTON;
		tname = "EelLabeledImagePushButtonAccessible";

	} else { /* plain */
		i = PLAIN;
		role = ATK_ROLE_IMAGE;
		tname = "EelLabeledImagePlainAccessible";
	}

	if (!types [i]) {
		const GInterfaceInfo atk_image_info = {
			(GInterfaceInitFunc) eel_labeled_image_accessible_image_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		types [i] = eel_accessibility_create_derived_type
			(tname, G_TYPE_FROM_INSTANCE (widget),
			 eel_labeled_image_accessible_class_init);

		if (!types [i])
			return NULL;

		g_type_add_interface_static (
			types [i], ATK_TYPE_IMAGE, &atk_image_info);
	}

	accessible = g_object_new (types [i], NULL);
	atk_object_set_role (accessible, role);

	return eel_accessibility_set_atk_object_return (widget, accessible);
}

static void
eel_labeled_image_button_class_init (GtkWidgetClass *klass)
{
	klass->get_accessible = eel_labeled_image_get_accessible;
}

static GType
eel_labeled_image_button_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GtkButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) eel_labeled_image_button_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GtkButton),
			0, /* n_preallocs */
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static
			(GTK_TYPE_BUTTON,
			 "EelLabeledImageButton", &info, 0);
	}

	return type;
}

static GType
eel_labeled_image_check_button_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GtkCheckButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) eel_labeled_image_button_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GtkCheckButton),
			0, /* n_preallocs */
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static
			(GTK_TYPE_CHECK_BUTTON,
			 "EelLabeledImageCheckButton", &info, 0);
	}

	return type;
}

static GType
eel_labeled_image_toggle_button_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GtkToggleButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) eel_labeled_image_button_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GtkToggleButton),
			0, /* n_preallocs */
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static
			(GTK_TYPE_TOGGLE_BUTTON,
			 "EelLabeledImageToggleButton", &info, 0);
	}

	return type;
}


static GType
eel_labeled_image_radio_button_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GtkRadioButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) eel_labeled_image_button_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GtkRadioButton),
			0, /* n_preallocs */
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static
			(GTK_TYPE_RADIO_BUTTON,
			 "EelLabeledImageRadioButton", &info, 0);
	}

	return type;
}
