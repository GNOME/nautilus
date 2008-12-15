/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gtk-container.c - Functions to simplify the implementations of 
  			 GtkContainer widgets.

   Copyright (C) 2001 Ramiro Estrugo.

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
#include "eel-gtk-container.h"
#include "eel-art-extensions.h"

/**
 * eel_gtk_container_child_expose_event:
 * 
 * @container: A GtkContainer widget.
 * @child: A child of @container or NULL;
 * @event: The expose event.
 *
 * Forward an expose event to a child if needed.  It is valid to give a NULL @child.
 * In that case this function is a noop.  Proper clipping is done to ensure that the @child
 * does indeed need to be forwarded the exposure event.  Finally, the forwarding 
 * only occurs if the child is a NO_WINDOW widget.  Of course, it is valid to feed
 * non NO_WINDOW widgets to this function, in which case this function is a noop.
 */
void
eel_gtk_container_child_expose_event (GtkContainer *container,
				      GtkWidget *child,
				      GdkEventExpose *event)
{
	g_return_if_fail (GTK_IS_CONTAINER (container));

	if (child == NULL) {
		return;
	}

	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_container_propagate_expose (container, child, event);
}

/**
 * eel_gtk_container_child_map:
 * 
 * @container: A GtkContainer widget.
 * @child: A child of @container or NULL;
 *
 * Map a child if needed.  This is usually called from the "GtkWidget::map"
 * method of the @container widget.  If @child is NULL, then this function is a noop.
 */
void
eel_gtk_container_child_map (GtkContainer *container,
			     GtkWidget *child)
{
	g_return_if_fail (GTK_IS_CONTAINER (container));

	if (child == NULL) {
		return;
	}

	g_return_if_fail (child->parent == GTK_WIDGET (container));

	if (GTK_WIDGET_VISIBLE (child) && !GTK_WIDGET_MAPPED (child)) {
		gtk_widget_map (child);
	}
}

/**
 * eel_gtk_container_child_unmap:
 * 
 * @container: A GtkContainer widget.
 * @child: A child of @container or NULL;
 *
 * Unmap a child if needed.  This is usually called from the "GtkWidget::unmap"
 * method of the @container widget.  If @child is NULL, then this function is a noop.
 */
void
eel_gtk_container_child_unmap (GtkContainer *container,
			       GtkWidget *child)
{
	g_return_if_fail (GTK_IS_CONTAINER (container));

	if (child == NULL) {
		return;
	}

	g_return_if_fail (child->parent == GTK_WIDGET (container));
	
	if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_MAPPED (child)) {
		gtk_widget_unmap (child);
	}
}

/**
 * eel_gtk_container_child_add:
 * 
 * @container: A GtkContainer widget.
 * @child: A non NULL unparented child.
 *
 * Add a @child to a @container.  The @child is realized, mapped
 * and resized if needed.  This is usually called from the "GtkContainer::add"
 * method of the @container.  The @child cannot be NULL.
 */
void
eel_gtk_container_child_add (GtkContainer *container,
			     GtkWidget *child)
{
	g_return_if_fail (GTK_IS_CONTAINER (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
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

/**
 * eel_gtk_container_child_remove:
 * 
 * @container: A GtkContainer widget.
 * @child: A non NULL child of @container.
 *
 * Remove @child from @container.  The @container is resized if needed.
 * This is usually called from the "GtkContainer::remove" method of the
 * @container.  The child cannot be NULL.
 */
void
eel_gtk_container_child_remove (GtkContainer *container,
				GtkWidget *child)
{
	gboolean child_was_visible;

	g_return_if_fail (GTK_IS_CONTAINER (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (child->parent == GTK_WIDGET (container));
	
	child_was_visible = GTK_WIDGET_VISIBLE (child);
	
	gtk_widget_unparent (child);

	if (child_was_visible) {
		gtk_widget_queue_resize (GTK_WIDGET (container));
	}
}

/**
 * eel_gtk_container_child_size_allocate:
 * 
 * @container: A GtkContainer widget.
 * @child: A child of @container or NULL;
 *
 * Invoke the "GtkWidget::size_allocate" method of @child.  
 * This function is usually called from the "GtkWidget::size_allocate"
 * method of @container.  The child can be NULL, in which case this 
 * function is a noop.
 */
void
eel_gtk_container_child_size_allocate (GtkContainer *container,
				       GtkWidget *child,
				       EelIRect child_geometry)
{
	GtkAllocation child_allocation;

	g_return_if_fail (GTK_IS_CONTAINER (container));

	if (child == NULL) {
		return;
	}

	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (child->parent == GTK_WIDGET (container));

 	if (eel_irect_is_empty (&child_geometry)) {
		return;
	}

	child_allocation.x = child_geometry.x0;
	child_allocation.y = child_geometry.y0;
	child_allocation.width = eel_irect_get_width (child_geometry);
	child_allocation.height = eel_irect_get_height (child_geometry);
	
	gtk_widget_size_allocate (child, &child_allocation);
}
