/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-image-table.c - An image table.

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

#include "nautilus-image-table.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-art-extensions.h"
#include "nautilus-art-gtk-extensions.h"

#include <gtk/gtkmain.h>

/* Arguments */
enum
{
	ARG_0,
	ARG_CHILD_UNDER_POINTER
};

/* Detail member struct */
struct NautilusImageTableDetails
{
	GtkWidget *windowed_ancestor;
	guint enter_notify_connection_id;
	guint leave_notify_connection_id;
	guint motion_notify_connection_id;
	guint button_press_connection_id;
	guint button_release_connection_id;
	GtkWidget *child_under_pointer;
	GtkWidget *child_being_pressed;
};

/* Signals */
typedef enum
{
	CHILD_ENTER,
	CHILD_LEAVE,
	CHILD_PRESSED,
	CHILD_RELEASED,
	CHILD_CLICKED,
	LAST_SIGNAL
} ImageTableSignals;

/* Signals */
static guint image_table_signals[LAST_SIGNAL] = { 0 };

/* GtkObjectClass methods */
static void    nautilus_image_table_initialize_class (NautilusImageTableClass *image_table_class);
static void    nautilus_image_table_initialize       (NautilusImageTable      *image);
static void    nautilus_image_table_destroy          (GtkObject               *object);

/* GtkWidgetClass methods */
static void    nautilus_image_table_realize          (GtkWidget               *widget);
static void    nautilus_image_table_unrealize        (GtkWidget               *widget);

/* GtkContainerClass methods */
static void    nautilus_image_table_add              (GtkContainer            *container,
						      GtkWidget               *widget);
static void    nautilus_image_table_remove           (GtkContainer            *container,
						      GtkWidget               *widget);
static GtkType nautilus_image_table_child_type       (GtkContainer            *container);

/* Private NautilusImageTable methods */

/* Ancestor callbacks */
static int     ancestor_enter_notify_event           (GtkWidget               *widget,
						      GdkEventCrossing        *event,
						      gpointer                 event_data);
static int     ancestor_leave_notify_event           (GtkWidget               *widget,
						      GdkEventCrossing        *event,
						      gpointer                 event_data);
static int     ancestor_motion_notify_event          (GtkWidget               *widget,
						      GdkEventMotion          *event,
						      gpointer                 event_data);
static int     ancestor_button_press_event           (GtkWidget               *widget,
						      GdkEventButton          *event,
						      gpointer                 event_data);
static int     ancestor_button_release_event         (GtkWidget               *widget,
						      GdkEventButton          *event,
						      gpointer                 event_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusImageTable, nautilus_image_table, NAUTILUS_TYPE_WRAP_TABLE)

/* Class init methods */
static void
nautilus_image_table_initialize_class (NautilusImageTableClass *image_table_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (image_table_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (image_table_class);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (image_table_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_image_table_destroy;
	
 	/* GtkWidgetClass */
 	widget_class->realize = nautilus_image_table_realize;
 	widget_class->unrealize = nautilus_image_table_unrealize;

 	/* GtkContainerClass */
	container_class->add = nautilus_image_table_add;
	container_class->remove = nautilus_image_table_remove;
	container_class->child_type = nautilus_image_table_child_type;
  
	/* Signals */
	image_table_signals[CHILD_ENTER] = gtk_signal_new ("child_enter",
							   GTK_RUN_LAST,
							   object_class->type,
							   0,
							   gtk_marshal_NONE__POINTER_POINTER,
							   GTK_TYPE_NONE,
							   2,
							   GTK_TYPE_POINTER,
							   GTK_TYPE_POINTER);
	image_table_signals[CHILD_LEAVE] = gtk_signal_new ("child_leave",
							   GTK_RUN_LAST,
							   object_class->type,
							   0,
							   gtk_marshal_NONE__POINTER_POINTER,
							   GTK_TYPE_NONE,
							   2,
							   GTK_TYPE_POINTER,
							   GTK_TYPE_POINTER);
	image_table_signals[CHILD_PRESSED] = gtk_signal_new ("child_pressed",
							     GTK_RUN_LAST,
							     object_class->type,
							     0,
							     gtk_marshal_NONE__POINTER_POINTER,
							     GTK_TYPE_NONE,
							     2,
							     GTK_TYPE_POINTER,
							     GTK_TYPE_POINTER);
	image_table_signals[CHILD_RELEASED] = gtk_signal_new ("child_released",
							      GTK_RUN_LAST,
							      object_class->type,
							      0,
							      gtk_marshal_NONE__POINTER_POINTER,
							      GTK_TYPE_NONE,
							      2,
							      GTK_TYPE_POINTER,
							      GTK_TYPE_POINTER);
	image_table_signals[CHILD_CLICKED] = gtk_signal_new ("child_clicked",
							     GTK_RUN_LAST,
							     object_class->type,
							     0,
							     gtk_marshal_NONE__POINTER_POINTER,
							     GTK_TYPE_NONE,
							     2,
							     GTK_TYPE_POINTER,
							     GTK_TYPE_POINTER);
	
	gtk_object_class_add_signals (object_class, image_table_signals, LAST_SIGNAL);
}

void
nautilus_image_table_initialize (NautilusImageTable *image_table)
{
	GTK_WIDGET_SET_FLAGS (image_table, GTK_NO_WINDOW);

	image_table->details = g_new0 (NautilusImageTableDetails, 1);
}

/* GtkObjectClass methods */
static void
nautilus_image_table_destroy (GtkObject *object)
{
 	NautilusImageTable *image_table;
	
	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (object));

	image_table = NAUTILUS_IMAGE_TABLE (object);

	g_free (image_table->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* GtkWidgetClass methods */
static void
nautilus_image_table_realize (GtkWidget *widget)
{
	NautilusImageTable *image_table;

	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (widget));
	
	image_table = NAUTILUS_IMAGE_TABLE (widget);

	/* Chain realize */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

	image_table->details->windowed_ancestor = nautilus_gtk_widget_find_windowed_ancestor (widget);
	g_assert (GTK_IS_WIDGET (image_table->details->windowed_ancestor));
	
	gtk_widget_add_events (image_table->details->windowed_ancestor,
			       GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_POINTER_MOTION_MASK);

	image_table->details->enter_notify_connection_id =
		gtk_signal_connect (GTK_OBJECT (image_table->details->windowed_ancestor),
				    "enter_notify_event",
				    GTK_SIGNAL_FUNC (ancestor_enter_notify_event),
				    widget);
	
	image_table->details->leave_notify_connection_id =
		gtk_signal_connect (GTK_OBJECT (image_table->details->windowed_ancestor),
				    "leave_notify_event",
				    GTK_SIGNAL_FUNC (ancestor_leave_notify_event),
				    widget);
	
	image_table->details->motion_notify_connection_id =
		gtk_signal_connect (GTK_OBJECT (image_table->details->windowed_ancestor),
				    "motion_notify_event",
				    GTK_SIGNAL_FUNC (ancestor_motion_notify_event),
				    widget);
	
	image_table->details->button_press_connection_id =
		gtk_signal_connect (GTK_OBJECT (image_table->details->windowed_ancestor),
				    "button_press_event",
				    GTK_SIGNAL_FUNC (ancestor_button_press_event),
				    widget);
	
	image_table->details->button_release_connection_id =
		gtk_signal_connect (GTK_OBJECT (image_table->details->windowed_ancestor),
				    "button_release_event",
				    GTK_SIGNAL_FUNC (ancestor_button_release_event),
				    widget);
}

static void
nautilus_image_table_unrealize (GtkWidget *widget)
{
	NautilusImageTable *image_table;

	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (widget));

	image_table = NAUTILUS_IMAGE_TABLE (widget);

	gtk_signal_disconnect (GTK_OBJECT (image_table->details->windowed_ancestor),
			       image_table->details->enter_notify_connection_id);
	
	gtk_signal_disconnect (GTK_OBJECT (image_table->details->windowed_ancestor),
			       image_table->details->leave_notify_connection_id);
	
	gtk_signal_disconnect (GTK_OBJECT (image_table->details->windowed_ancestor),
			       image_table->details->motion_notify_connection_id);
	
	gtk_signal_disconnect (GTK_OBJECT (image_table->details->windowed_ancestor),
			       image_table->details->button_press_connection_id);
	
	gtk_signal_disconnect (GTK_OBJECT (image_table->details->windowed_ancestor),
			       image_table->details->button_release_connection_id);

	image_table->details->windowed_ancestor = NULL;
	image_table->details->enter_notify_connection_id = 0;
	image_table->details->leave_notify_connection_id = 0;
	image_table->details->motion_notify_connection_id = 0;
	image_table->details->button_press_connection_id = 0;
	image_table->details->button_release_connection_id = 0;

	/* Chain unrealize */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, unrealize, (widget));
}

/* GtkContainerClass methods */
static void
nautilus_image_table_add (GtkContainer *container,
			  GtkWidget *child)
{
	NautilusImageTable *image_table;
	
	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (container));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (child));

	image_table = NAUTILUS_IMAGE_TABLE (container);

	NAUTILUS_CALL_PARENT_CLASS (GTK_CONTAINER_CLASS, add, (container, child));
}

static void
nautilus_image_table_remove (GtkContainer *container,
			     GtkWidget *child)
{
	NautilusImageTable *image_table;
	
	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (container));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (child));
	
	image_table = NAUTILUS_IMAGE_TABLE (container);;

	if (child == image_table->details->child_under_pointer) {
		image_table->details->child_under_pointer = NULL;
	}

	if (child == image_table->details->child_being_pressed) {
		image_table->details->child_being_pressed = NULL;
	}

	NAUTILUS_CALL_PARENT_CLASS (GTK_CONTAINER_CLASS, remove, (container, child));
}

static GtkType
nautilus_image_table_child_type (GtkContainer *container)
{
	return NAUTILUS_TYPE_LABELED_IMAGE;
}

/* Private NautilusImageTable methods */
static void
image_table_handle_motion (NautilusImageTable *image_table,
			   int x,
			   int y,
			   GdkEvent *event)
{
	GtkWidget *child;
	GtkWidget *leave_emit_child = NULL;
	GtkWidget *enter_emit_child = NULL;

	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (image_table));

	child = nautilus_wrap_table_find_child_at_event_point (NAUTILUS_WRAP_TABLE (image_table), x, y);

	if (child == image_table->details->child_under_pointer) {
		return;
	}

	if (child != NULL) {
		if (image_table->details->child_under_pointer != NULL) {
			leave_emit_child = image_table->details->child_under_pointer;
		}

		image_table->details->child_under_pointer = child;
		enter_emit_child = image_table->details->child_under_pointer;
	} else {
		if (image_table->details->child_under_pointer != NULL) {
			leave_emit_child = image_table->details->child_under_pointer;
		}

		image_table->details->child_under_pointer = NULL;
	}

	if (leave_emit_child != NULL) {
		gtk_signal_emit (GTK_OBJECT (image_table), 
				 image_table_signals[CHILD_LEAVE],
				 leave_emit_child,
				 event);
	}

	if (enter_emit_child != NULL) {
		gtk_signal_emit (GTK_OBJECT (image_table), 
				 image_table_signals[CHILD_ENTER],
				 enter_emit_child,
				 event);
	}
}

static int
ancestor_enter_notify_event (GtkWidget *widget,
			     GdkEventCrossing *event,
			     gpointer event_data)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE_TABLE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	image_table_handle_motion (NAUTILUS_IMAGE_TABLE (event_data), event->x, event->y, (GdkEvent *) event);

	return FALSE;
}

static int
ancestor_leave_notify_event (GtkWidget *widget,
			     GdkEventCrossing *event,
			     gpointer event_data)
{
	ArtIRect bounds;
	int x = -1;
	int y = -1;
	
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE_TABLE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	bounds = nautilus_irect_gtk_widget_get_bounds (GTK_WIDGET (event_data));
	
	if (nautilus_art_irect_contains_point (&bounds, event->x, event->y)) {
		x = event->x;
		y = event->y;
	}

	image_table_handle_motion (NAUTILUS_IMAGE_TABLE (event_data), x, y, (GdkEvent *) event);
	
	return FALSE;
}

static int
ancestor_motion_notify_event (GtkWidget *widget,
			      GdkEventMotion *event,
			      gpointer event_data)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE_TABLE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	image_table_handle_motion (NAUTILUS_IMAGE_TABLE (event_data), (int) event->x, (int) event->y, (GdkEvent *) event);

	return FALSE;
}

static int
ancestor_button_press_event (GtkWidget *widget,
			     GdkEventButton *event,
			     gpointer event_data)
{
  	NautilusImageTable *image_table;
	GtkWidget *child;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE_TABLE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

 	image_table = NAUTILUS_IMAGE_TABLE (event_data);

	gtk_grab_add (widget);

	child = nautilus_wrap_table_find_child_at_event_point (NAUTILUS_WRAP_TABLE (image_table), event->x, event->y);

	if (child != NULL) {
		if (child == image_table->details->child_under_pointer) {
			image_table->details->child_being_pressed = child;
			gtk_signal_emit (GTK_OBJECT (image_table), 
					 image_table_signals[CHILD_PRESSED],
					 child,
					 event);
		}
	}

	return FALSE;
}

static int
ancestor_button_release_event (GtkWidget *widget,
			       GdkEventButton *event,
			       gpointer event_data)
{
  	NautilusImageTable *image_table;
	GtkWidget *child;
	GtkWidget *released_emit_child = NULL;
	GtkWidget *clicked_emit_child = NULL;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE_TABLE (event_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

 	image_table = NAUTILUS_IMAGE_TABLE (event_data);

	gtk_grab_remove (widget);

	child = nautilus_wrap_table_find_child_at_event_point (NAUTILUS_WRAP_TABLE (image_table), event->x, event->y);

	if (image_table->details->child_being_pressed != NULL) {
		released_emit_child = image_table->details->child_being_pressed;
	}

	if (child != NULL) {
		if (child == image_table->details->child_being_pressed) {
			clicked_emit_child = child;
		}
	}
	
	image_table->details->child_being_pressed = NULL;

	if (released_emit_child != NULL) {
		gtk_signal_emit (GTK_OBJECT (image_table), 
				 image_table_signals[CHILD_RELEASED],
				 released_emit_child,
				 event);
	}
	
	if (clicked_emit_child != NULL) {
		gtk_signal_emit (GTK_OBJECT (image_table), 
				 image_table_signals[CHILD_CLICKED],
				 clicked_emit_child,
				 event);
	}
	
	return FALSE;
}

/**
 * nautilus_image_table_new:
 */
GtkWidget*
nautilus_image_table_new (gboolean homogeneous)
{
	NautilusImageTable *image_table;

	image_table = NAUTILUS_IMAGE_TABLE (gtk_widget_new (nautilus_image_table_get_type (), NULL));

	nautilus_wrap_table_set_homogeneous (NAUTILUS_WRAP_TABLE (image_table), homogeneous);
	
	return GTK_WIDGET (image_table);
}
