/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-horizontal-splitter.c - A horizontal splitter with a semi gradient look

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
#include "nautilus-horizontal-splitter.h"

#include <eel/eel-gtk-macros.h>
#include <stdlib.h>

struct NautilusHorizontalSplitterDetails {
	double press_x;
	guint32 press_time;
	int press_position;
	int saved_size;
};

#define CLOSED_THRESHOLD 4
#define NOMINAL_SIZE 148
#define SPLITTER_CLICK_SLOP 1
#define SPLITTER_CLICK_TIMEOUT	400

static void nautilus_horizontal_splitter_class_init (NautilusHorizontalSplitterClass *horizontal_splitter_class);
static void nautilus_horizontal_splitter_init       (NautilusHorizontalSplitter      *horizontal_splitter);

EEL_CLASS_BOILERPLATE (NautilusHorizontalSplitter,
		       nautilus_horizontal_splitter,
		       GTK_TYPE_HPANED)

static void
nautilus_horizontal_splitter_init (NautilusHorizontalSplitter *horizontal_splitter)
{
	horizontal_splitter->details = g_new0 (NautilusHorizontalSplitterDetails, 1);
}

static void
nautilus_horizontal_splitter_finalize (GObject *object)
{
	NautilusHorizontalSplitter *horizontal_splitter;
	
	horizontal_splitter = NAUTILUS_HORIZONTAL_SPLITTER (object);

	g_free (horizontal_splitter->details);
	
	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
splitter_expand (NautilusHorizontalSplitter *splitter, int position)
{
	g_assert (NAUTILUS_IS_HORIZONTAL_SPLITTER (splitter));

	if (position >= CLOSED_THRESHOLD) {
		return;
	}

	position = splitter->details->saved_size;
	if (position < CLOSED_THRESHOLD) {
		position = NOMINAL_SIZE;
	}
	
	gtk_paned_set_position (GTK_PANED (splitter), position);
}

static void
splitter_collapse (NautilusHorizontalSplitter *splitter, int position)
{
	g_assert (NAUTILUS_IS_HORIZONTAL_SPLITTER (splitter));

	splitter->details->saved_size = position;
	gtk_paned_set_position (GTK_PANED (splitter), 0);
}

static void
splitter_toggle (NautilusHorizontalSplitter *splitter, int position)
{
	g_assert (NAUTILUS_IS_HORIZONTAL_SPLITTER (splitter));

	if (gtk_paned_get_position (GTK_PANED (splitter)) >= CLOSED_THRESHOLD) {
		nautilus_horizontal_splitter_collapse (splitter);
	} else {
		nautilus_horizontal_splitter_expand (splitter);
	}
}

static void
splitter_hide (NautilusHorizontalSplitter *splitter)
{
	GtkPaned *parent;

	parent = GTK_PANED (splitter);

	gtk_widget_hide (parent->child1);
}

static void
splitter_show (NautilusHorizontalSplitter *splitter)
{
	GtkPaned *parent;

	parent = GTK_PANED (splitter);

	gtk_widget_show (parent->child1);
}

static gboolean
splitter_is_hidden (NautilusHorizontalSplitter *splitter)
{
	GtkPaned *parent;
	
	parent = GTK_PANED (splitter);

	return GTK_WIDGET_VISIBLE (parent->child1);
}

void
nautilus_horizontal_splitter_expand (NautilusHorizontalSplitter *splitter)
{
	splitter_expand (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

void
nautilus_horizontal_splitter_hide (NautilusHorizontalSplitter *splitter)
{
	splitter_hide (splitter);
}

void
nautilus_horizontal_splitter_show (NautilusHorizontalSplitter *splitter)
{
	splitter_show (splitter);
}

gboolean
nautilus_horizontal_splitter_is_hidden (NautilusHorizontalSplitter *splitter)
{
	return splitter_is_hidden (splitter);
}

void
nautilus_horizontal_splitter_collapse (NautilusHorizontalSplitter *splitter)
{
	splitter_collapse (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

/* routine to toggle the open/closed state of the splitter */
void
nautilus_horizontal_splitter_toggle_position (NautilusHorizontalSplitter *splitter)
{
	splitter_toggle (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

/* NautilusHorizontalSplitter public methods */
GtkWidget *
nautilus_horizontal_splitter_new (void)
{
	return gtk_widget_new (nautilus_horizontal_splitter_get_type (), NULL);
}

/* handle mouse downs by remembering the position and the time */
static gboolean
nautilus_horizontal_splitter_button_press (GtkWidget *widget, GdkEventButton *event)
{
	gboolean result;
	NautilusHorizontalSplitter *splitter;
	int position;
	
	splitter = NAUTILUS_HORIZONTAL_SPLITTER (widget);

	position = gtk_paned_get_position (GTK_PANED (widget));

	result = EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, button_press_event, (widget, event));

	if (result) {
		splitter->details->press_x = event->x;
		splitter->details->press_time = event->time;
		splitter->details->press_position = position;
	}

	return result;
}

/* handle mouse ups by seeing if it was a tap and toggling the open state accordingly */
static gboolean
nautilus_horizontal_splitter_button_release (GtkWidget *widget, GdkEventButton *event)
{
	gboolean result;
	NautilusHorizontalSplitter *splitter;
	int delta, delta_time;
	splitter = NAUTILUS_HORIZONTAL_SPLITTER (widget);

	result = EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, button_release_event, (widget, event));

	if (result) {
		delta = abs (event->x - splitter->details->press_x);
		delta_time = event->time - splitter->details->press_time;
		if (delta < SPLITTER_CLICK_SLOP && delta_time < SPLITTER_CLICK_TIMEOUT)  {
			nautilus_horizontal_splitter_toggle_position (splitter);
		}
	}

	return result;
}

static void
nautilus_horizontal_splitter_size_allocate (GtkWidget     *widget,
					    GtkAllocation *allocation)
{
	gint border_width;
	GtkPaned *paned;
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
      
	paned = GTK_PANED (widget);
	border_width = GTK_CONTAINER (paned)->border_width;

	widget->allocation = *allocation;

	if (paned->child2 != NULL && GTK_WIDGET_VISIBLE (paned->child2)) { 
		EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate,
				 (widget, allocation));
	} else if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1)) {

		if (GTK_WIDGET_REALIZED (widget)) {
			gdk_window_hide (paned->handle);
		}

		gtk_widget_get_child_requisition (paned->child1,
						  &child_requisition);
		
		child_allocation.x = widget->allocation.x + border_width;
		child_allocation.y = widget->allocation.y + border_width;
		child_allocation.width = MIN (child_requisition.width,
					      allocation->width - 2 * border_width);
		child_allocation.height = MIN (child_requisition.height,
					       allocation->height - 2 * border_width);
		
		gtk_widget_size_allocate (paned->child1, &child_allocation);
	} else
		if (GTK_WIDGET_REALIZED (widget)) {
			gdk_window_hide (paned->handle);
		}

}

static void
nautilus_horizontal_splitter_class_init (NautilusHorizontalSplitterClass *class)
{
	GtkWidgetClass *widget_class;
	
	widget_class = GTK_WIDGET_CLASS (class);

	G_OBJECT_CLASS (class)->finalize = nautilus_horizontal_splitter_finalize;

	widget_class->size_allocate = nautilus_horizontal_splitter_size_allocate;
	widget_class->button_press_event = nautilus_horizontal_splitter_button_press;
	widget_class->button_release_event = nautilus_horizontal_splitter_button_release;
}

void
nautilus_horizontal_splitter_pack2 (NautilusHorizontalSplitter *splitter,
				    GtkWidget                  *child2)
{
	GtkPaned *paned;
	
	g_return_if_fail (GTK_IS_WIDGET (child2));
	g_return_if_fail (NAUTILUS_IS_HORIZONTAL_SPLITTER (splitter));

	paned = GTK_PANED (splitter);
	gtk_paned_pack2 (paned, child2, TRUE, TRUE);
}
