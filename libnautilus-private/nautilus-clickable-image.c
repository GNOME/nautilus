/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-clickable-image.c - A clickable image widget.

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

#include "nautilus-clickable-image.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-art-gtk-extensions.h"
#include "nautilus-graphic-effects.h"

#include <gtk/gtkmain.h>

/* Arguments */
enum
{
	ARG_0,
	ARG_POINTER_INSIDE,
};

/* Signals */
typedef enum
{
	CLICKED,
	ENTER,
	LEAVE,
	LAST_SIGNAL
} ImageSignal;

/* Signals */
static guint clickable_image_signals[LAST_SIGNAL] = { 0 };

/* Detail member struct */
struct _NautilusClickableImageDetails
{
 	gboolean pointer_inside;
	gboolean prelight;

	GdkPixbuf *pixbuf;
	GdkPixbuf *prelight_pixbuf;
};

/* GtkObjectClass methods */
static void nautilus_clickable_image_initialize_class (NautilusClickableImageClass *image_class);
static void nautilus_clickable_image_initialize       (NautilusClickableImage      *image);
static void nautilus_clickable_image_destroy          (GtkObject                   *object);
static void nautilus_clickable_image_get_arg          (GtkObject                   *object,
						       GtkArg                      *arg,
						       guint                        arg_id);
/* GtkWidgetClass methods */
static int  nautilus_clickable_image_expose_event     (GtkWidget                   *widget,
						       GdkEventExpose              *event);
static void nautilus_clickable_image_realize          (GtkWidget                   *widget);

/* Ancestor callbacks */
static int  ancestor_enter_notify_event               (GtkWidget                   *widget,
						       GdkEventCrossing            *event,
						       gpointer                     event_data);
static int  ancestor_leave_notify_event               (GtkWidget                   *widget,
						       GdkEventCrossing            *event,
						       gpointer                     event_data);
static int  ancestor_motion_notify_event              (GtkWidget                   *widget,
						       GdkEventMotion              *event,
						       gpointer                     event_data);
static int  ancestor_button_press_event               (GtkWidget                   *widget,
						       GdkEventButton              *event,
						       gpointer                     event_data);
static int  ancestor_button_release_event             (GtkWidget                   *widget,
						       GdkEventButton              *event,
						       gpointer                     event_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusClickableImage, nautilus_clickable_image, NAUTILUS_TYPE_LABELED_IMAGE)

/* Class init methods */
static void
nautilus_clickable_image_initialize_class (NautilusClickableImageClass *image_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (image_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (image_class);
	
	/* GtkObjectClass */
	object_class->destroy = nautilus_clickable_image_destroy;
	object_class->get_arg = nautilus_clickable_image_get_arg;
	
 	/* GtkWidgetClass */
 	widget_class->expose_event = nautilus_clickable_image_expose_event;
 	widget_class->realize = nautilus_clickable_image_realize;

	/* Signals */
	clickable_image_signals[CLICKED] = gtk_signal_new ("clicked",
							   GTK_RUN_LAST,
							   object_class->type,
							   0,
							   gtk_marshal_NONE__NONE,
							   GTK_TYPE_NONE, 
							   0);

	clickable_image_signals[ENTER] = gtk_signal_new ("enter",
							 GTK_RUN_LAST,
							 object_class->type,
							 0,
							 gtk_marshal_NONE__NONE,
							 GTK_TYPE_NONE, 
							 0);

	clickable_image_signals[LEAVE] = gtk_signal_new ("leave",
							 GTK_RUN_LAST,
							 object_class->type,
							 0,
							 gtk_marshal_NONE__NONE,
							 GTK_TYPE_NONE, 
							 0);

	gtk_object_class_add_signals (object_class, clickable_image_signals, LAST_SIGNAL);

	/* Arguments */
	gtk_object_add_arg_type ("NautilusClickableImage::pointer_inside",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READABLE,
				 ARG_POINTER_INSIDE);
}

void
nautilus_clickable_image_initialize (NautilusClickableImage *image)
{
	image->details = g_new0 (NautilusClickableImageDetails, 1);
}

/* GtkObjectClass methods */
static void
nautilus_clickable_image_destroy (GtkObject *object)
{
 	NautilusClickableImage *clickable_image;
	
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (object));

	clickable_image = NAUTILUS_CLICKABLE_IMAGE (object);

	g_free (clickable_image->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_clickable_image_get_arg (GtkObject *object,
				  GtkArg *arg,
				  guint arg_id)
{
	NautilusClickableImage *clickable_image;

	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (object));
	
	clickable_image = NAUTILUS_CLICKABLE_IMAGE (object);

 	switch (arg_id)
	{
	case ARG_POINTER_INSIDE:
		GTK_VALUE_BOOL (*arg) = clickable_image->details->pointer_inside;
		break;

 	default:
		g_assert_not_reached ();
	}
}

/* GtkWidgetClass methods */
static void
nautilus_clickable_image_realize (GtkWidget *widget)
{
	NautilusClickableImage *clickable_image;
	GtkWidget *windowed_ancestor;

	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (widget));
	
	clickable_image = NAUTILUS_CLICKABLE_IMAGE (widget);

	/* Chain realize */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

	windowed_ancestor = nautilus_gtk_widget_find_windowed_ancestor (widget);
	g_assert (GTK_IS_WIDGET (windowed_ancestor));
	
	gtk_widget_add_events (windowed_ancestor,
			       GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_POINTER_MOTION_MASK);

	nautilus_gtk_signal_connect_while_realized (GTK_OBJECT (windowed_ancestor),
						    "enter_notify_event",
						    GTK_SIGNAL_FUNC (ancestor_enter_notify_event),
						    widget,
						    widget);
	
	nautilus_gtk_signal_connect_while_realized (GTK_OBJECT (windowed_ancestor),
						    "leave_notify_event",
						    GTK_SIGNAL_FUNC (ancestor_leave_notify_event),
						    widget,
						    widget);
	
	nautilus_gtk_signal_connect_while_realized (GTK_OBJECT (windowed_ancestor),
						    "motion_notify_event",
						    GTK_SIGNAL_FUNC (ancestor_motion_notify_event),
						    widget,
						    widget);
	
	nautilus_gtk_signal_connect_while_realized (GTK_OBJECT (windowed_ancestor),
						    "button_press_event",
						    GTK_SIGNAL_FUNC (ancestor_button_press_event),
						    widget,
						    widget);
	
	nautilus_gtk_signal_connect_while_realized (GTK_OBJECT (windowed_ancestor),
						    "button_release_event",
						    GTK_SIGNAL_FUNC (ancestor_button_release_event),
						    widget,
						    widget);
}

static void
label_enter (NautilusClickableImage *clickable_image)
{
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (clickable_image));
	
	clickable_image->details->pointer_inside = TRUE;

	if (clickable_image->details->prelight) {
		nautilus_labeled_image_set_pixbuf (NAUTILUS_LABELED_IMAGE (clickable_image),
						   clickable_image->details->prelight_pixbuf); 
	}

	gtk_widget_set_state (GTK_WIDGET (clickable_image), GTK_STATE_PRELIGHT);

	gtk_signal_emit (GTK_OBJECT (clickable_image), 
			 clickable_image_signals[ENTER],
			 clickable_image);

}

static void
label_leave (NautilusClickableImage *clickable_image)
{
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (clickable_image));
	
	clickable_image->details->pointer_inside = FALSE;

	if (clickable_image->details->prelight) {
		nautilus_labeled_image_set_pixbuf (NAUTILUS_LABELED_IMAGE (clickable_image),
						   clickable_image->details->pixbuf); 
	}

	gtk_widget_set_state (GTK_WIDGET (clickable_image), GTK_STATE_NORMAL);

	gtk_signal_emit (GTK_OBJECT (clickable_image), 
			 clickable_image_signals[LEAVE],
			 clickable_image);
}

static void
label_handle_motion (NautilusClickableImage *clickable_image,
		     int x,
		     int y)
{
	ArtIRect bounds;

	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (clickable_image));

	bounds = nautilus_gtk_widget_get_bounds (GTK_WIDGET (clickable_image));
	
	if (nautilus_art_irect_contains_point (&bounds, x, y)) {
		/* Inside */
		if (!clickable_image->details->pointer_inside) {
			label_enter (clickable_image);
		}
	} else {
		/* Outside */
		if (clickable_image->details->pointer_inside) {
			label_leave (clickable_image);
		}
	}
}

static void
label_handle_button_press (NautilusClickableImage *clickable_image)
{
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (clickable_image));

	gtk_widget_set_state (GTK_WIDGET (clickable_image), GTK_STATE_ACTIVE);
	gtk_widget_queue_draw (GTK_WIDGET (clickable_image));
}

static void
label_handle_button_release (NautilusClickableImage *clickable_image)
{
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (clickable_image));

	gtk_widget_set_state (GTK_WIDGET (clickable_image), GTK_STATE_PRELIGHT);
	gtk_widget_queue_draw (GTK_WIDGET (clickable_image));

	gtk_signal_emit (GTK_OBJECT (clickable_image), 
			 clickable_image_signals[CLICKED],
			 clickable_image);
}

static void
adjust_coordinates_for_window (GdkWindow *widget_window, 
			       GdkWindow *event_window,
			       int *x,
			       int *y)
{
	GdkWindow *window;
	int wx, wy;
	
	/* Viewports place their children in a different GdkWindow
	 * than their own widget window (perhaps other containers do
	 * this to). Therefore if the event we got is on a different
	 * GdkWindow than our own, adjust the coordinates.
	 */

	window = widget_window;

	while (window != event_window && window != NULL) {
		gdk_window_get_position	 (window,
					  &wx,
					  &wy);

		*x -= wx;
		*y -= wy;

		window = gdk_window_get_parent (window);
	}
}

static int
ancestor_enter_notify_event (GtkWidget *widget,
			     GdkEventCrossing *event,
			     gpointer event_data)
{
	int x, y;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	x = event->x;
	y = event->y;

	adjust_coordinates_for_window (GTK_WIDGET (event_data)->window,
				       widget->window,
				       &x, &y);

	label_handle_motion (NAUTILUS_CLICKABLE_IMAGE (event_data), x, y);

	return FALSE;
}

static int
ancestor_leave_notify_event (GtkWidget *widget,
			     GdkEventCrossing *event,
			     gpointer event_data)
{
	int x, y;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	x = event->x;
	y = event->y;

	adjust_coordinates_for_window (GTK_WIDGET (event_data)->window,
				       widget->window,
				       &x, &y);

	label_handle_motion (NAUTILUS_CLICKABLE_IMAGE (event_data), x, y);

	return FALSE;
}

static int
ancestor_motion_notify_event (GtkWidget *widget,
			      GdkEventMotion *event,
			      gpointer event_data)
{
	int x, y;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	x = event->x;
	y = event->y;

	adjust_coordinates_for_window (GTK_WIDGET (event_data)->window,
				       widget->window,
				       &x, &y);

	label_handle_motion (NAUTILUS_CLICKABLE_IMAGE (event_data), x, y);

	return FALSE;
}

static int
ancestor_button_press_event (GtkWidget *widget,
			     GdkEventButton *event,
			     gpointer event_data)
{
  	NautilusClickableImage *clickable_image;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

 	clickable_image = NAUTILUS_CLICKABLE_IMAGE (event_data);

	gtk_grab_add (widget);

	if (clickable_image->details->pointer_inside) {
		label_handle_button_press (NAUTILUS_CLICKABLE_IMAGE (event_data));
	}

	return FALSE;
}

static int
ancestor_button_release_event (GtkWidget *widget,
			       GdkEventButton *event,
			       gpointer event_data)
{
  	NautilusClickableImage *clickable_image;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

 	clickable_image = NAUTILUS_CLICKABLE_IMAGE (event_data);

	gtk_grab_remove (widget);

	if (clickable_image->details->pointer_inside) {
		label_handle_button_release (NAUTILUS_CLICKABLE_IMAGE (event_data));
	}

	return FALSE;
}

static int
nautilus_clickable_image_expose_event (GtkWidget *widget,
				       GdkEventExpose *event)
{
  	NautilusClickableImage *clickable_image;

	g_return_val_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (widget), TRUE);
	g_return_val_if_fail (event != NULL, TRUE);
	g_return_val_if_fail (event->window == widget->window, TRUE);
	g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), TRUE);
	
 	clickable_image = NAUTILUS_CLICKABLE_IMAGE (widget);

	/* Chain expose */
	return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, expose_event, (widget, event));
}

static void 
nautilus_clickable_image_set_up_pixbufs (NautilusClickableImage *clickable_image)
{
	clickable_image->details->pixbuf =
		nautilus_labeled_image_get_pixbuf (NAUTILUS_LABELED_IMAGE (clickable_image));

	clickable_image->details->prelight_pixbuf = nautilus_create_spotlight_pixbuf 
		(clickable_image->details->pixbuf);
} 

/* Public NautilusClickableImage methods */

/**
 * nautilus_clickable_image_new:
 *
 * @text: Text for label or NULL.
 * @pixbuf: A GdkPixbuf to or NULL.

 * Create a clickable image with a solid background.
 *
 * Return value: The newly allocated NautilusClickableImage with the
 * given text and pixbuf.  Either of these two can be NULL.
 */
GtkWidget*
nautilus_clickable_image_new (const char *text,
			      GdkPixbuf *pixbuf)
{
	NautilusClickableImage *clickable_image;
	
	clickable_image = NAUTILUS_CLICKABLE_IMAGE (gtk_widget_new (nautilus_clickable_image_get_type (), NULL));
	
	if (pixbuf != NULL) {
		nautilus_labeled_image_set_pixbuf (NAUTILUS_LABELED_IMAGE (clickable_image), pixbuf);
	}

	if (text != NULL) {
		nautilus_labeled_image_set_text (NAUTILUS_LABELED_IMAGE (clickable_image), text);
	}

	return GTK_WIDGET (clickable_image);
}

GtkWidget*
nautilus_clickable_image_new_from_file_name (const char *text,
					     const char *pixbuf_file_name)
{
	NautilusClickableImage *clickable_image;

	g_return_val_if_fail (pixbuf_file_name != NULL, NULL);
	
	clickable_image = NAUTILUS_CLICKABLE_IMAGE (nautilus_clickable_image_new (text, NULL));
	
	nautilus_labeled_image_set_pixbuf_from_file_name (NAUTILUS_LABELED_IMAGE (clickable_image), pixbuf_file_name);

	return GTK_WIDGET (clickable_image);
}

/**
 * nautilus_clickable_image_new_solid:
 *
 * @text: Text for label or NULL.
 * @pixbuf: A GdkPixbuf to or NULL.
 * @drop_shadow_offset: Drop shadow offset.
 * @drop_shadow_color: Drop shadow color.
 * @text_color: Text color.
 * @x_alignment: Horizontal alignment.
 * @y_alignmtn: Vertical alignment.
 * @x_padding: Horizontal padding.
 * @y_padding: Vertical padding.
 * @background_color: Background color.
 * @tile_pixbuf: A GdkPixbuf or NULL.
 *
 * Create a clickable image with a solid background.
 *
 * Return value: The newly allocated NautilusClickableImage with the
 * given attributes.
 */
GtkWidget *
nautilus_clickable_image_new_solid (const char *text,
				    GdkPixbuf *pixbuf,
				    guint drop_shadow_offset,
				    guint32 drop_shadow_color,
				    guint32 text_color,
				    float x_alignment,
				    float y_alignment,
				    int x_padding,
				    int y_padding,
				    guint32 background_color,
				    GdkPixbuf *tile_pixbuf)
{
	NautilusClickableImage *clickable_image;
	NautilusLabeledImage *labeled_image;
	
	clickable_image = NAUTILUS_CLICKABLE_IMAGE (gtk_widget_new (nautilus_clickable_image_get_type (), NULL));

	labeled_image = NAUTILUS_LABELED_IMAGE (clickable_image);

	if (pixbuf != NULL) {
		nautilus_labeled_image_set_pixbuf (labeled_image, pixbuf);
	}

	if (text != NULL) {
		nautilus_labeled_image_set_text (labeled_image, text);
	}

	nautilus_labeled_image_set_background_mode (labeled_image, NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_labeled_image_set_solid_background_color (labeled_image, background_color);
	nautilus_labeled_image_set_smooth_drop_shadow_color (labeled_image, drop_shadow_color);
	nautilus_labeled_image_set_smooth_drop_shadow_offset (labeled_image, drop_shadow_offset);
	nautilus_labeled_image_set_text_color (labeled_image, text_color);
	
	if (tile_pixbuf != NULL) {
			nautilus_labeled_image_set_tile_pixbuf (labeled_image, tile_pixbuf);
	}

	nautilus_labeled_image_set_x_padding (labeled_image, x_padding);
	nautilus_labeled_image_set_y_padding (labeled_image, y_padding);
	nautilus_labeled_image_set_x_alignment (labeled_image, x_alignment);
	nautilus_labeled_image_set_y_alignment (labeled_image, y_alignment);
	
	return GTK_WIDGET (clickable_image);
}

/**
 * nautilus_clickable_image_new:
 *
 * @clickable_image: A NautilusClickableImage
 * @prelight: a gboolean specifying wether to prelight
 *
 * Enable or disable auto-prelight mode. You can't change the pixbuf
 * while prelight mode is on; as a workaround, you can turn prelight
 * off, change the pixbuf, and then turn it back on.
 * 
 */

void
nautilus_clickable_image_set_prelight (NautilusClickableImage *clickable_image,
				       gboolean prelight)
{
	/* FIXME: you can't change the pixbuf with prelight mode on */

	if (!clickable_image->details->prelight && prelight) {
		nautilus_clickable_image_set_up_pixbufs (clickable_image);
		clickable_image->details->prelight = TRUE;
		if (clickable_image->details->pointer_inside) {
			nautilus_labeled_image_set_pixbuf 
				(NAUTILUS_LABELED_IMAGE (clickable_image),
				 clickable_image->details->prelight_pixbuf);
		}
	}

	if (clickable_image->details->prelight && !prelight) {
		if (clickable_image->details->pointer_inside) {
			nautilus_labeled_image_set_pixbuf 
				(NAUTILUS_LABELED_IMAGE (clickable_image),
				 clickable_image->details->pixbuf);
		}

		gdk_pixbuf_unref (clickable_image->details->pixbuf);
		clickable_image->details->pixbuf = NULL;
		gdk_pixbuf_unref (clickable_image->details->prelight_pixbuf);
		clickable_image->details->prelight_pixbuf = NULL;
		clickable_image->details->prelight = FALSE;
	}
}
