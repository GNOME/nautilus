/* gnome-druid-page.c
 * Copyright (C) 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
*/

#include <config.h>

#include "nautilus-druid-page.h"

enum {
	NEXT,
	PREPARE,
	BACK,
	FINISH,
	CANCEL,
	LAST_SIGNAL
};

static void nautilus_druid_page_init		(NautilusDruidPage		 *druid_page);
static void nautilus_druid_page_class_init  	(NautilusDruidPageClass	 *klass);
static void nautilus_druid_page_size_request       (GtkWidget               *widget, 
						 GtkRequisition          *requisition);
static void nautilus_druid_page_size_allocate      (GtkWidget		 *widget,
						 GtkAllocation           *allocation);
static void nautilus_druid_page_draw               (GtkWidget               *widget,
						 GdkRectangle            *area);
static gint nautilus_druid_page_expose             (GtkWidget               *widget,
						 GdkEventExpose          *event);
static void nautilus_druid_page_realize            (GtkWidget		 *widget);

static GtkBinClass *parent_class = NULL;
static guint druid_page_signals[LAST_SIGNAL] = { 0 };


GtkType
nautilus_druid_page_get_type (void)
{
	static GtkType druid_page_type = 0;

	if (!druid_page_type) {
		static const GtkTypeInfo druid_page_info =
		{
			"NautilusDruidPage",
			sizeof (NautilusDruidPage),
			sizeof (NautilusDruidPageClass),
			(GtkClassInitFunc) nautilus_druid_page_class_init,
			(GtkObjectInitFunc) nautilus_druid_page_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		druid_page_type = gtk_type_unique (gtk_bin_get_type (), &druid_page_info);
	}

	return druid_page_type;
}

static void
nautilus_druid_page_class_init (NautilusDruidPageClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass*) klass;
	widget_class = (GtkWidgetClass*) klass;
	parent_class = gtk_type_class (gtk_bin_get_type ());

	druid_page_signals[NEXT] = 
		gtk_signal_new ("next",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDruidPageClass, next),
				gtk_marshal_BOOL__POINTER,
				GTK_TYPE_BOOL, 1,
				GTK_TYPE_POINTER);
	druid_page_signals[PREPARE] = 
		gtk_signal_new ("prepare",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDruidPageClass, prepare),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	druid_page_signals[BACK] = 
		gtk_signal_new ("back",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDruidPageClass, back),
				gtk_marshal_BOOL__POINTER,
				GTK_TYPE_BOOL, 1,
				GTK_TYPE_POINTER);
	druid_page_signals[FINISH] = 
		gtk_signal_new ("finish",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDruidPageClass, finish),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	druid_page_signals[CANCEL] = 
		gtk_signal_new ("cancel",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDruidPageClass, cancel),
				gtk_marshal_BOOL__POINTER,
				GTK_TYPE_BOOL, 1,
				GTK_TYPE_POINTER);


	gtk_object_class_add_signals (object_class, druid_page_signals, LAST_SIGNAL);

	widget_class->size_request = nautilus_druid_page_size_request;
	widget_class->size_allocate = nautilus_druid_page_size_allocate;
	widget_class->draw = nautilus_druid_page_draw;
	widget_class->expose_event = nautilus_druid_page_expose;
	widget_class->realize = nautilus_druid_page_realize;
}


static void
nautilus_druid_page_init (NautilusDruidPage *druid_page)
{
	druid_page->_priv = NULL;
	/* Note: If you add any privates make sure to add a destroy method and
	 * free the private struct */
	GTK_WIDGET_UNSET_FLAGS (druid_page, GTK_NO_WINDOW);
}
static void
nautilus_druid_page_size_request (GtkWidget *widget, 
			       GtkRequisition *requisition)
{
	GtkBin *bin;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (widget));
	g_return_if_fail (requisition != NULL);
	bin = GTK_BIN (widget);
	requisition->width = GTK_CONTAINER (widget)->border_width * 2;
	requisition->height = GTK_CONTAINER (widget)->border_width * 2;

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child)) {
		GtkRequisition child_requisition;

		gtk_widget_size_request (bin->child, &child_requisition);

		requisition->width += child_requisition.width;
		requisition->height += child_requisition.height;
	}
}
static void
nautilus_druid_page_size_allocate (GtkWidget *widget,
				GtkAllocation *allocation)
{
	GtkBin *bin;
	GtkAllocation child_allocation;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (widget));
	g_return_if_fail (allocation != NULL);
	widget->allocation = *allocation;
	bin = GTK_BIN (widget);

	child_allocation.x = 0;
	child_allocation.y = 0;
	child_allocation.width = MAX (allocation->width - GTK_CONTAINER (widget)->border_width * 2, 0);
	child_allocation.height = MAX (allocation->height - GTK_CONTAINER (widget)->border_width * 2, 0);
	
	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (widget->window,
					allocation->x + GTK_CONTAINER (widget)->border_width,
					allocation->y + GTK_CONTAINER (widget)->border_width,
					child_allocation.width,
					child_allocation.height);
	}
	if (bin->child) {
		gtk_widget_size_allocate (bin->child, &child_allocation);
	}
}
static void
nautilus_druid_page_paint (GtkWidget     *widget,
			GdkRectangle *area)
{
	gtk_paint_flat_box (widget->style, widget->window, GTK_STATE_NORMAL, 
			    GTK_SHADOW_NONE, area, widget, "base", 0, 0, -1, -1);
}

static void
nautilus_druid_page_draw (GtkWidget               *widget,
		       GdkRectangle            *area)
{
	GdkRectangle child_area;

	if (!GTK_WIDGET_APP_PAINTABLE (widget))
		nautilus_druid_page_paint (widget, area);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		GdkRectangle tmp_area;

		tmp_area = *area;
		tmp_area.x -= GTK_CONTAINER (widget)->border_width;
		tmp_area.y -= GTK_CONTAINER (widget)->border_width;

		if (GTK_BIN (widget)->child && gtk_widget_intersect (GTK_BIN (widget)->child, &tmp_area, &child_area)) {
			gtk_widget_draw (GTK_BIN (widget)->child, &child_area);
		}
	}
}
static gint
nautilus_druid_page_expose (GtkWidget               *widget,
			 GdkEventExpose          *event)
{
	GdkEventExpose child_event;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DRUID_PAGE (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (!GTK_WIDGET_APP_PAINTABLE (widget))
		nautilus_druid_page_paint (widget, &event->area);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		child_event = *event;
		if (GTK_BIN (widget)->child && GTK_WIDGET_NO_WINDOW (GTK_BIN (widget)->child) &&
		    gtk_widget_intersect (GTK_BIN (widget)->child, &event->area, &child_event.area)) {
			gtk_widget_event (GTK_BIN (widget)->child, (GdkEvent*) &child_event);
		}
	}

	return FALSE;
}

static void
nautilus_druid_page_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	gint border_width;
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
	
	border_width = GTK_CONTAINER (widget)->border_width;

	attributes.x = widget->allocation.x + border_width;
	attributes.y = widget->allocation.y + border_width;
	attributes.width = widget->allocation.width - 2*border_width;
	attributes.height = widget->allocation.height - 2*border_width;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget)
		| GDK_BUTTON_MOTION_MASK
		| GDK_BUTTON_PRESS_MASK
		| GDK_BUTTON_RELEASE_MASK
		| GDK_EXPOSURE_MASK
		| GDK_ENTER_NOTIFY_MASK
		| GDK_LEAVE_NOTIFY_MASK;
	
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  
	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);
	gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}
/**
 * nautilus_druid_page_next:
 * @druid_page: A DruidPage widget.
 * 
 * Description: This will emit the "next" signal for that particular page.  It
 * is called by gnome-druid exclusively.  It is expected that non-linear Druid's
 * will override this signal and return TRUE if it handles changing pages.  
 * 
 * Return value: This function will return FALSE by default.
 **/
/* Public functions */
gboolean
nautilus_druid_page_next     (NautilusDruidPage *druid_page)
{
	gboolean retval = FALSE;
	g_return_val_if_fail (druid_page != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DRUID_PAGE (druid_page), FALSE);
	gtk_signal_emit (GTK_OBJECT (druid_page), druid_page_signals [NEXT], GTK_WIDGET (druid_page)->parent, &retval);
	return retval;
}
/**
 * nautilus_druid_page_prepare:
 * @druid_page: A DruidPage widget.
 * 
 * Description: This emits the "prepare" signal for the page.  It is called by
 * gnome-druid exclusively.
 **/
void
nautilus_druid_page_prepare  (NautilusDruidPage *druid_page)
{
	g_return_if_fail (druid_page != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (druid_page));

	gtk_signal_emit (GTK_OBJECT (druid_page), druid_page_signals [PREPARE], GTK_WIDGET (druid_page)->parent);
}
/**
 * nautilus_druid_page_back:
 * @druid_page: A DruidPage widget.
 * 
 * Description: This will emit the "back" signal for that particular page.  It
 * is called by gnome-druid exclusively.  It is expected that non-linear Druid's
 * will override this signal and return TRUE if it handles changing pages.
 * 
 * Return value: This function will return FALSE by default.
 **/
gboolean
nautilus_druid_page_back     (NautilusDruidPage *druid_page)
{
	gboolean retval = FALSE;
	g_return_val_if_fail (druid_page != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DRUID_PAGE (druid_page), FALSE);
  
	gtk_signal_emit (GTK_OBJECT (druid_page), druid_page_signals [BACK], GTK_WIDGET (druid_page)->parent, &retval);
	return retval;
}
/**
 * nautilus_druid_page_finish:
 * @druid_page: A DruidPage widget.
 * 
 * Description: This emits the "finish" signal for the page.  It is called by
 * gnome-druid exclusively.
 **/
void
nautilus_druid_page_finish   (NautilusDruidPage *druid_page)
{
	g_return_if_fail (druid_page != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (druid_page));
  
	gtk_signal_emit (GTK_OBJECT (druid_page), druid_page_signals [FINISH], GTK_WIDGET (druid_page)->parent);
}
/**
 * nautilus_druid_page_cancel:
 * @druid_page: A DruidPage widget.
 * 
 * Description: This will emit the "cancel" signal for that particular page.  It
 * is called by gnome-druid exclusively.  It is expected that a Druid will
 * override this signal and return TRUE if it does not want to exit.
 * 
 * Return value: This function will return FALSE by default.
 **/
gboolean
nautilus_druid_page_cancel   (NautilusDruidPage *druid_page)
{
	gboolean retval = FALSE;
	g_return_val_if_fail (druid_page != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DRUID_PAGE (druid_page), FALSE);
	gtk_signal_emit (GTK_OBJECT (druid_page), druid_page_signals [CANCEL], GTK_WIDGET (druid_page)->parent, &retval);
	return retval;
}
