/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-wrap-box.c - A table that can wrap its contents as needed.

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

#include "nautilus-wrap-table.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-art-extensions.h"
#include "nautilus-art-gtk-extensions.h"

#include <gtk/gtkmain.h>
#include <gtk/gtkviewport.h>

/* Arguments */
enum
{
	ARG_0,
	ARG_X_SPACING,
	ARG_Y_SPACING,
	ARG_X_JUSTIFICATION,
	ARG_Y_JUSTIFICATION,
	ARG_HOMOGENEOUS,
};

/* Detail member struct */
struct NautilusWrapTableDetails
{
	guint x_spacing;
	guint y_spacing;
	NautilusJustification x_justification;
	NautilusJustification y_justification;
	gboolean homogeneous;
	GList *items;
};

/* GtkObjectClass methods */
static void               nautilus_wrap_table_initialize_class (NautilusWrapTableClass   *wrap_table_class);
static void               nautilus_wrap_table_initialize       (NautilusWrapTable        *wrap);
static void               nautilus_wrap_table_destroy          (GtkObject                *object);
static void               nautilus_wrap_table_set_arg          (GtkObject                *object,
								GtkArg                   *arg,
								guint                     arg_id);
static void               nautilus_wrap_table_get_arg          (GtkObject                *object,
								GtkArg                   *arg,
								guint                     arg_id);
/* GtkWidgetClass methods */
static void               nautilus_wrap_table_size_request     (GtkWidget                *widget,
								GtkRequisition           *requisition);
static int                nautilus_wrap_table_expose_event     (GtkWidget                *widget,
								GdkEventExpose           *event);
static void               nautilus_wrap_table_size_allocate    (GtkWidget                *widget,
								GtkAllocation            *allocation);
static void               nautilus_wrap_table_map              (GtkWidget                *widget);
static void               nautilus_wrap_table_unmap            (GtkWidget                *widget);

/* GtkContainerClass methods */
static void               nautilus_wrap_table_add              (GtkContainer             *container,
								GtkWidget                *widget);
static void               nautilus_wrap_table_remove           (GtkContainer             *container,
								GtkWidget                *widget);
static void               nautilus_wrap_table_forall           (GtkContainer             *container,
								gboolean                  include_internals,
								GtkCallback               callback,
								gpointer                  callback_data);
static GtkType            nautilus_wrap_table_child_type       (GtkContainer             *container);


/* Private NautilusWrapTable methods */
static NautilusDimensions wrap_table_art_irect_max_dimensions  (const NautilusDimensions *one,
								const NautilusDimensions *two);
static NautilusDimensions wrap_table_get_max_child_dimensions  (const NautilusWrapTable  *wrap_table);
static NautilusDimensions wrap_table_get_content_dimensions    (const NautilusWrapTable  *wrap_table);
static ArtIRect           wrap_table_get_content_bounds        (const NautilusWrapTable  *wrap_table);
static NautilusArtIPoint  wrap_table_get_scroll_offset         (const NautilusWrapTable  *wrap_table);
static GtkWidget *        wrap_table_find_child_at_point       (const NautilusWrapTable  *wrap_table,
								int                       x,
								int                       y);
static void               wrap_table_layout                    (NautilusWrapTable        *wrap_table);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusWrapTable, nautilus_wrap_table, GTK_TYPE_CONTAINER)

/* Class init methods */
static void
nautilus_wrap_table_initialize_class (NautilusWrapTableClass *wrap_table_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (wrap_table_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (wrap_table_class);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (wrap_table_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_wrap_table_destroy;
	object_class->set_arg = nautilus_wrap_table_set_arg;
	object_class->get_arg = nautilus_wrap_table_get_arg;
	
 	/* GtkWidgetClass */
 	widget_class->size_request = nautilus_wrap_table_size_request;
	widget_class->size_allocate = nautilus_wrap_table_size_allocate;
 	widget_class->expose_event = nautilus_wrap_table_expose_event;
	widget_class->map = nautilus_wrap_table_map;
	widget_class->unmap = nautilus_wrap_table_unmap;

 	/* GtkContainerClass */
	container_class->add = nautilus_wrap_table_add;
	container_class->remove = nautilus_wrap_table_remove;
	container_class->forall = nautilus_wrap_table_forall;
	container_class->child_type = nautilus_wrap_table_child_type;
  
	/* Arguments */
	gtk_object_add_arg_type ("NautilusWrapTable::x_spacing",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_X_SPACING);
	gtk_object_add_arg_type ("NautilusWrapTable::y_spacing",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_Y_SPACING);
	gtk_object_add_arg_type ("NautilusWrapTable::x_justification",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_X_JUSTIFICATION);
	gtk_object_add_arg_type ("NautilusWrapTable::y_justification",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_Y_JUSTIFICATION);
	gtk_object_add_arg_type ("NautilusWrapTable::homogeneous",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_HOMOGENEOUS);
}

void
nautilus_wrap_table_initialize (NautilusWrapTable *wrap_table)
{
	GTK_WIDGET_SET_FLAGS (wrap_table, GTK_NO_WINDOW);

	wrap_table->details = g_new0 (NautilusWrapTableDetails, 1);
	wrap_table->details->x_justification = NAUTILUS_JUSTIFICATION_BEGINNING;
	wrap_table->details->y_justification = NAUTILUS_JUSTIFICATION_END;
}

/* GtkObjectClass methods */
static void
nautilus_wrap_table_destroy (GtkObject *object)
{
 	NautilusWrapTable *wrap_table;
	
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (object));

	wrap_table = NAUTILUS_WRAP_TABLE (object);

	/*
	 * Chain destroy before freeing our details.
	 * The details will be used in the nautilus_wrap_box_remove () 
	 * method as a result of the children being killed.
	 */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

	g_list_free (wrap_table->details->items);

	g_free (wrap_table->details);
}

static void
nautilus_wrap_table_set_arg (GtkObject *object,
			     GtkArg *arg,
			     guint arg_id)
{
	NautilusWrapTable *wrap_table;
	
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (object));

 	wrap_table = NAUTILUS_WRAP_TABLE (object);

 	switch (arg_id)
	{
	case ARG_X_SPACING:
		nautilus_wrap_table_set_x_spacing (wrap_table, GTK_VALUE_UINT (*arg));
		break;

	case ARG_Y_SPACING:
		nautilus_wrap_table_set_y_spacing (wrap_table, GTK_VALUE_UINT (*arg));
		break;

	case ARG_X_JUSTIFICATION:
		nautilus_wrap_table_set_x_justification (wrap_table, GTK_VALUE_ENUM (*arg));
		break;

	case ARG_Y_JUSTIFICATION:
		nautilus_wrap_table_set_y_justification (wrap_table, GTK_VALUE_ENUM (*arg));
		break;

	case ARG_HOMOGENEOUS:
		nautilus_wrap_table_set_homogeneous (wrap_table, GTK_VALUE_BOOL (*arg));
		break;

 	default:
		g_assert_not_reached ();
	}
}

static void
nautilus_wrap_table_get_arg (GtkObject *object,
			    GtkArg *arg,
			    guint arg_id)
{
	NautilusWrapTable *wrap_table;

	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (object));
	
	wrap_table = NAUTILUS_WRAP_TABLE (object);

 	switch (arg_id)
	{
	case ARG_X_SPACING:
		GTK_VALUE_UINT (*arg) = nautilus_wrap_table_get_x_spacing (wrap_table);
		break;

	case ARG_Y_SPACING:
		GTK_VALUE_UINT (*arg) = nautilus_wrap_table_get_y_spacing (wrap_table);
		break;

	case ARG_X_JUSTIFICATION:
		GTK_VALUE_ENUM (*arg) = nautilus_wrap_table_get_x_justification (wrap_table);
		break;

	case ARG_Y_JUSTIFICATION:
		GTK_VALUE_ENUM (*arg) = nautilus_wrap_table_get_y_justification (wrap_table);
		break;

	case ARG_HOMOGENEOUS:
		GTK_VALUE_BOOL (*arg) = nautilus_wrap_table_get_homogeneous (wrap_table);
		break;

 	default:
		g_assert_not_reached ();
	}
}

/* GtkWidgetClass methods */
static void
nautilus_wrap_table_size_request (GtkWidget *widget,
				  GtkRequisition *requisition)
{
	NautilusWrapTable *wrap_table;
	NautilusDimensions content_dimensions;

 	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (widget));
 	g_return_if_fail (requisition != NULL);

  	wrap_table = NAUTILUS_WRAP_TABLE (widget);

	content_dimensions = wrap_table_get_content_dimensions (wrap_table);

	/* The -1 tells Satan to use as much space as is available */
	requisition->width = -1;
	requisition->height = content_dimensions.height + GTK_CONTAINER (widget)->border_width * 2;
}

static void
nautilus_wrap_table_size_allocate (GtkWidget *widget,
				   GtkAllocation *allocation)
{
	NautilusWrapTable *wrap_table;

 	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (widget));
 	g_return_if_fail (allocation != NULL);
	
  	wrap_table = NAUTILUS_WRAP_TABLE (widget);
	
	widget->allocation = *allocation;

	wrap_table_layout (wrap_table);
}

static int
nautilus_wrap_table_expose_event (GtkWidget *widget,
				 GdkEventExpose *event)
{
	NautilusWrapTable *wrap_table;
	GList *iterator;
	
	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (widget), TRUE);
	g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), TRUE);
	g_return_val_if_fail (event != NULL, TRUE);

  	wrap_table = NAUTILUS_WRAP_TABLE (widget);

	for (iterator = wrap_table->details->items; iterator; iterator = iterator->next) {
		GtkWidget *item;
		GdkEventExpose item_event;

		item = iterator->data;
		item_event = *event;

		if (GTK_WIDGET_DRAWABLE (item) &&
		    GTK_WIDGET_NO_WINDOW (item) &&
		    gtk_widget_intersect (item, &event->area, &item_event.area)) {
			gtk_widget_event (item, (GdkEvent *) &item_event);
		}
	}

	return FALSE;
}

static void
nautilus_wrap_table_map (GtkWidget *widget)
{
	NautilusWrapTable *wrap_table;
	GList *iterator;

	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (widget));
	
	wrap_table = NAUTILUS_WRAP_TABLE (widget);

 	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
	
	for (iterator = wrap_table->details->items; iterator; iterator = iterator->next) {
		GtkWidget *item;

		item = iterator->data;

		if (GTK_WIDGET_VISIBLE (item) && !GTK_WIDGET_MAPPED (item)) {
			gtk_widget_map (item);
		}
	}
}

static void
nautilus_wrap_table_unmap (GtkWidget *widget)
{
	NautilusWrapTable *wrap_table;
	GList *iterator;

	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (widget));
	
	wrap_table = NAUTILUS_WRAP_TABLE (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	for (iterator = wrap_table->details->items; iterator; iterator = iterator->next) {
		GtkWidget *item;

		item = iterator->data;

		if (GTK_WIDGET_VISIBLE (item) && GTK_WIDGET_MAPPED (item)) {
			gtk_widget_unmap (item);
		}
	}
}

/* GtkContainerClass methods */
static void
nautilus_wrap_table_add (GtkContainer *container,
			GtkWidget *child)
{
	NautilusWrapTable *wrap_table;

	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (container));
	g_return_if_fail (GTK_IS_WIDGET (child));

	wrap_table = NAUTILUS_WRAP_TABLE (container);

	gtk_widget_set_parent (child, GTK_WIDGET (container));

	wrap_table->details->items = g_list_append (wrap_table->details->items, child);

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
nautilus_wrap_table_remove (GtkContainer *container,
			    GtkWidget *child)
{
	NautilusWrapTable *wrap_table;
	gboolean child_was_visible;
	
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	wrap_table = NAUTILUS_WRAP_TABLE (container);;

	child_was_visible = GTK_WIDGET_VISIBLE (child);
	gtk_widget_unparent (child);
	wrap_table->details->items = g_list_remove (wrap_table->details->items, child);

	if (child_was_visible) {
		gtk_widget_queue_resize (GTK_WIDGET (container));
	}
}

static void
nautilus_wrap_table_forall (GtkContainer *container,
			    gboolean include_internals,
			    GtkCallback callback,
			    gpointer callback_data)
{
	NautilusWrapTable *wrap_table;
	GList *node;
	GList *next;
	
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (container));
	g_return_if_fail (callback != NULL);
	
	wrap_table = NAUTILUS_WRAP_TABLE (container);;
	
	for (node = wrap_table->details->items; node != NULL; node = next) {
		g_assert (GTK_IS_WIDGET (node->data));
		next = node->next;
		(* callback) (GTK_WIDGET (node->data), callback_data);	
	}
}

static GtkType
nautilus_wrap_table_child_type (GtkContainer   *container)
{
	return GTK_TYPE_WIDGET;
}

/* Private NautilusWrapTable methods */
static void
wrap_table_layout (NautilusWrapTable *wrap_table)
{
	GList *iterator;
	NautilusArtIPoint pos;
	NautilusDimensions max_child_dimensions;
	ArtIRect content_bounds;

 	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table));

	max_child_dimensions = wrap_table_get_max_child_dimensions (wrap_table);
	content_bounds = wrap_table_get_content_bounds (wrap_table);
	pos.x = content_bounds.x0;
	pos.y = content_bounds.y0;

	for (iterator = wrap_table->details->items; iterator; iterator = iterator->next) {
		GtkWidget *item;

		item = iterator->data;
		
		if (GTK_WIDGET_VISIBLE (item)) {
			GtkAllocation item_allocation;

			if (wrap_table->details->homogeneous) {
				item_allocation.x = pos.x;
				item_allocation.y = pos.y;
				item_allocation.width = max_child_dimensions.width;
				item_allocation.height = max_child_dimensions.height;
				
				if ((pos.x + max_child_dimensions.width) > content_bounds.x1) {
					pos.x = content_bounds.x0 + wrap_table->details->x_spacing + max_child_dimensions.width;
					pos.y += (max_child_dimensions.height + wrap_table->details->y_spacing);
					item_allocation.x = content_bounds.x0;
					item_allocation.y = pos.y;
				} else {
					pos.x += (wrap_table->details->x_spacing + max_child_dimensions.width);
				}
			} else {
				GtkRequisition item_requisition;
				
				gtk_widget_size_request (item, &item_requisition);
				
				item_allocation.x = pos.x;
				item_allocation.y = pos.y;
				item_allocation.width = item_requisition.width;
				item_allocation.height = item_requisition.height;
				
				g_assert (item_allocation.width <= max_child_dimensions.width);
				g_assert (item_allocation.height <= max_child_dimensions.height);
				
				if ((pos.x + max_child_dimensions.width) > content_bounds.x1) {
					pos.x = content_bounds.x0 + wrap_table->details->x_spacing + max_child_dimensions.width;
					pos.y += (max_child_dimensions.height + wrap_table->details->y_spacing);
					item_allocation.x = content_bounds.x0;
					item_allocation.y = pos.y;
				} else {
					pos.x += (wrap_table->details->x_spacing + max_child_dimensions.width);
				}
				
				switch (wrap_table->details->x_justification) {
				case NAUTILUS_JUSTIFICATION_MIDDLE:
					item_allocation.x += (max_child_dimensions.width - (int) item_allocation.width) / 2;
					break;
				case NAUTILUS_JUSTIFICATION_END:
					item_allocation.x += (max_child_dimensions.width - (int) item_allocation.width);
					break;
				default:
				}
				
				switch (wrap_table->details->y_justification) {
				case NAUTILUS_JUSTIFICATION_MIDDLE:
					item_allocation.y += (max_child_dimensions.height - (int) item_allocation.height) / 2;
					break;
				case NAUTILUS_JUSTIFICATION_END:
					item_allocation.y += (max_child_dimensions.height - (int) item_allocation.height);
					break;
				default:
				}
			}
			
			gtk_widget_size_allocate (item, &item_allocation);
		}
	}
}

static NautilusDimensions
wrap_table_art_irect_max_dimensions (const NautilusDimensions *one,
				     const NautilusDimensions *two)
{
	NautilusDimensions max_dimensions;

	g_return_val_if_fail (one != NULL, NAUTILUS_DIMENSIONS_EMPTY);
	g_return_val_if_fail (two != NULL, NAUTILUS_DIMENSIONS_EMPTY);

	max_dimensions.width = MAX (one->width, two->width);
	max_dimensions.height = MAX (one->height, two->height);

	return max_dimensions;
}

static NautilusDimensions
wrap_table_get_max_child_dimensions (const NautilusWrapTable *wrap_table)
{
	NautilusDimensions max_dimensions;
	GList *iterator;

  	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), NAUTILUS_DIMENSIONS_EMPTY);

	max_dimensions = NAUTILUS_DIMENSIONS_EMPTY;

	for (iterator = wrap_table->details->items; iterator; iterator = iterator->next) {
		GtkWidget *child;
		
		child = iterator->data;
		
		if (GTK_WIDGET_VISIBLE (child)) {
 			GtkRequisition child_requisition;
			NautilusDimensions child_dimensions;

 			gtk_widget_size_request (child, &child_requisition);

			child_dimensions.width = (int) child_requisition.width;
			child_dimensions.height = (int) child_requisition.height;

			max_dimensions = wrap_table_art_irect_max_dimensions (&child_dimensions, &max_dimensions);
		}
	}

	return max_dimensions;
}

static int
wrap_table_get_num_fitting (int available,
			    int spacing,
			    int max_child_size)
{
	int num;

  	g_return_val_if_fail (available >= 0, 0);
  	g_return_val_if_fail (max_child_size > 0, 0);
  	g_return_val_if_fail (spacing >= 0, 0);
	
	num = (available + spacing) / (max_child_size + spacing);
	num = MAX (num, 1);

	return num;
}

static NautilusDimensions
wrap_table_get_content_dimensions (const NautilusWrapTable *wrap_table)
{
	NautilusDimensions content_dimensions;
	guint num_children;

  	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), NAUTILUS_DIMENSIONS_EMPTY);

	content_dimensions = NAUTILUS_DIMENSIONS_EMPTY;
	
	num_children = g_list_length (wrap_table->details->items);

	if (num_children > 0) {
		NautilusDimensions max_child_dimensions;
		NautilusDimensions dimensions;
		int num_cols;
		int num_rows;

		dimensions = nautilus_gtk_widget_get_dimensions (GTK_WIDGET (wrap_table));
		max_child_dimensions = wrap_table_get_max_child_dimensions (wrap_table);

		max_child_dimensions.width = MAX (max_child_dimensions.width, 1);
		max_child_dimensions.height = MAX (max_child_dimensions.height, 1);

		num_cols = wrap_table_get_num_fitting (dimensions.width,
						       wrap_table->details->x_spacing,
						       max_child_dimensions.width);
		num_rows = num_children / num_cols;
		num_rows = MAX (num_rows, 1);
		
		if ((num_children % num_rows) > 0) {
			num_rows++;
		}
		
		content_dimensions.width = dimensions.width;
		content_dimensions.height = num_rows * max_child_dimensions.height;
	}

	return content_dimensions;
}

static ArtIRect
wrap_table_get_content_bounds (const NautilusWrapTable *wrap_table)
{
	ArtIRect content_bounds;

  	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), NAUTILUS_ART_IRECT_EMPTY);

	content_bounds = nautilus_gtk_widget_get_bounds (GTK_WIDGET (wrap_table));

	content_bounds.x0 += GTK_CONTAINER (wrap_table)->border_width;
	content_bounds.y0 += GTK_CONTAINER (wrap_table)->border_width;
	content_bounds.x1 -= GTK_CONTAINER (wrap_table)->border_width;
	content_bounds.y1 -= GTK_CONTAINER (wrap_table)->border_width;

	return content_bounds;
}

static NautilusArtIPoint
wrap_table_get_scroll_offset (const NautilusWrapTable *wrap_table)
{
	NautilusArtIPoint scroll_offset;
	GtkWidget *parent;

  	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), NAUTILUS_ART_IPOINT_ZERO);

	scroll_offset = NAUTILUS_ART_IPOINT_ZERO;

	parent = GTK_WIDGET (wrap_table)->parent;

	/*
	 * FIXME: It lame we have to hardcode for a possible viewport 
	 * parent here.  Theres probably a better way to do this
	 */
	if (GTK_IS_VIEWPORT (parent)) {
		gdk_window_get_position (GTK_VIEWPORT (parent)->bin_window,
					 &scroll_offset.x,
					 &scroll_offset.y);
	}

	return scroll_offset;
}

static GtkWidget *
wrap_table_find_child_at_point (const NautilusWrapTable *wrap_table,
			       int x,
			       int y)
{
	GList *iterator;
	
  	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), NULL);

	for (iterator = wrap_table->details->items; iterator; iterator = iterator->next) {
		GtkWidget *child;
		
		child = iterator->data;
		
		if (GTK_WIDGET_VISIBLE (child)) {
			ArtIRect child_bounds;

			child_bounds = nautilus_gtk_widget_get_bounds (child);
 			
			if (nautilus_art_irect_contains_point (&child_bounds, x, y)) {
				return child;
			}
		}
	}

	return NULL;
}

/**
 * nautilus_wrap_table_new:
 *
 */
GtkWidget*
nautilus_wrap_table_new (gboolean homogeneous)
{
	NautilusWrapTable *wrap_table;

	wrap_table = NAUTILUS_WRAP_TABLE (gtk_widget_new (nautilus_wrap_table_get_type (), NULL));

	nautilus_wrap_table_set_homogeneous (wrap_table, homogeneous);
	
	return GTK_WIDGET (wrap_table);
}

/**
 * nautilus_wrap_table_set_x_spacing:
 * @wrap_table: A NautilusWrapTable.
 * @x_spacing: The new horizontal spacing between wraps.
 *
 */
void
nautilus_wrap_table_set_x_spacing (NautilusWrapTable *wrap_table,
				   guint x_spacing)
{
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table));
	
	if (wrap_table->details->x_spacing == x_spacing) {
		return;
	}
	
	wrap_table->details->x_spacing = x_spacing;

	gtk_widget_queue_resize (GTK_WIDGET (wrap_table));
}

/**
 * nautilus_wrap_table_get_item_spacing:
 * @wrap_table: A NautilusWrapTable.
 *
 * Returns: The horizontal spacing between wraps.
 */
guint
nautilus_wrap_table_get_x_spacing (const NautilusWrapTable *wrap_table)
{
	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), 0);
	
	return wrap_table->details->x_spacing;
}

/**
 * nautilus_wrap_table_set_y_spacing:
 * @wrap_table: A NautilusWrapTable.
 * @y_spacing: The new horizontal spacing between wraps.
 *
 */
void
nautilus_wrap_table_set_y_spacing (NautilusWrapTable *wrap_table,
				   guint y_spacing)
{
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table));
	
	if (wrap_table->details->y_spacing == y_spacing) {
		return;
	}
	
	wrap_table->details->y_spacing = y_spacing;

	gtk_widget_queue_resize (GTK_WIDGET (wrap_table));
}

/**
 * nautilus_wrap_table_get_item_spacing:
 * @wrap_table: A NautilusWrapTable.
 *
 * Returns: The horizontal spacing between wraps.
 */
guint
nautilus_wrap_table_get_y_spacing (const NautilusWrapTable *wrap_table)
{
	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), 0);
	
	return wrap_table->details->y_spacing;
}

/**
 * nautilus_wrap_table_find_child_at_event_point:
 * @wrap_table: A NautilusWrapTable.
 * @x: Event x;
 * @y: Event y;
 *
 * Returns: Child found at given coordinates or NULL of no child is found.
 */
GtkWidget *
nautilus_wrap_table_find_child_at_event_point (const NautilusWrapTable *wrap_table,
					       int x,
					       int y)
{
	NautilusArtIPoint scroll_offset;

	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), NULL);

	scroll_offset = wrap_table_get_scroll_offset (wrap_table);
	
	return wrap_table_find_child_at_point (wrap_table,
					      x + ABS (scroll_offset.x),
					      y + ABS (scroll_offset.y));
}

/**
 * nautilus_wrap_table_set_x_justification:
 * @wrap_table: A NautilusWrapTable.
 * @x_justification: The new horizontal justification between wraps.
 *
 */
void
nautilus_wrap_table_set_x_justification (NautilusWrapTable *wrap_table,
					 NautilusJustification x_justification)
{
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table));
	g_return_if_fail (x_justification >= NAUTILUS_JUSTIFICATION_BEGINNING);
	g_return_if_fail (x_justification <= NAUTILUS_JUSTIFICATION_END);
	
	if (wrap_table->details->x_justification == x_justification) {
		return;
	}
	
	wrap_table->details->x_justification = x_justification;
	gtk_widget_queue_resize (GTK_WIDGET (wrap_table));
}

/**
 * nautilus_wrap_table_get_item_justification:
 * @wrap_table: A NautilusWrapTable.
 *
 * Returns: The horizontal justification between wraps.
 */
NautilusJustification 
nautilus_wrap_table_get_x_justification (const NautilusWrapTable *wrap_table)
{
	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), 0);
	
	return wrap_table->details->x_justification;
}

/**
 * nautilus_wrap_table_set_y_justification:
 * @wrap_table: A NautilusWrapTable.
 * @y_justification: The new horizontal justification between wraps.
 *
 */
void
nautilus_wrap_table_set_y_justification (NautilusWrapTable *wrap_table,
					 NautilusJustification y_justification)
{
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table));
	g_return_if_fail (y_justification >= NAUTILUS_JUSTIFICATION_BEGINNING);
	g_return_if_fail (y_justification <= NAUTILUS_JUSTIFICATION_END);
	
	if (wrap_table->details->y_justification == y_justification) {
		return;
	}
	
	wrap_table->details->y_justification = y_justification;
	gtk_widget_queue_resize (GTK_WIDGET (wrap_table));
}

/**
 * nautilus_wrap_table_get_item_justification:
 * @wrap_table: A NautilusWrapTable.
 *
 * Returns: The horizontal justification between wraps.
 */
NautilusJustification 
nautilus_wrap_table_get_y_justification (const NautilusWrapTable *wrap_table)
{
	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), 0);
	
	return wrap_table->details->y_justification;
}

/**
 * nautilus_wrap_table_set_homogeneous:
 * @wrap_table: A NautilusWrapTable.
 * @homogeneous: The new horizontal spacing between wraps.
 *
 */
void
nautilus_wrap_table_set_homogeneous (NautilusWrapTable *wrap_table,
				     gboolean homogeneous)
{
	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table));
	
	if (wrap_table->details->homogeneous == homogeneous) {
		return;
	}
	
	wrap_table->details->homogeneous = homogeneous;

	gtk_widget_queue_resize (GTK_WIDGET (wrap_table));
}

/**
 * nautilus_wrap_table_get_item_spacing:
 * @wrap_table: A NautilusWrapTable.
 *
 * Returns: The horizontal spacing between wraps.
 */
gboolean
nautilus_wrap_table_get_homogeneous (const NautilusWrapTable *wrap_table)
{
	g_return_val_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table), FALSE);
	
	return wrap_table->details->homogeneous;
}

/**
 * nautilus_wrap_table_reorder_child:
 * @wrap_table: A NautilusWrapTable.
 * @child: Child to reorder.
 * @position: New position to put child at.
 *
 * Reorder the given chilren into the given position.
 *
 * Position is interpreted as follows:
 *
 *  0 - Place child at start of table.
 * -1 - Place child at end of table.
 *  n - Place child at nth position.  Count starts at 0.
 */
void
nautilus_wrap_table_reorder_child (NautilusWrapTable *wrap_table,
				   GtkWidget *child,
				   int position)
{
	GList *node;
	gboolean found_child = FALSE;

	g_return_if_fail (NAUTILUS_IS_WRAP_TABLE (wrap_table));
	g_return_if_fail (g_list_length (wrap_table->details->items) > 0);

	if (position == -1) {
		position = g_list_length (wrap_table->details->items) - 1;
	}

	g_return_if_fail (position >= 0);
	g_return_if_fail ((guint) position < g_list_length (wrap_table->details->items));

	for (node = wrap_table->details->items; node != NULL; node = node->next) {
		GtkWidget *next_child;
		next_child = node->data;
		
		if (next_child == child) {
			g_assert (found_child == FALSE);
			found_child = TRUE;
		}
	}

	g_return_if_fail (found_child);

	wrap_table->details->items = g_list_remove (wrap_table->details->items, child);
	wrap_table->details->items = g_list_insert (wrap_table->details->items, child, position);

	gtk_widget_queue_resize (GTK_WIDGET (wrap_table));
}
