/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-keep-last-vertical-box.c: Subclass of GtkVBox that clips off
 				      items that don't fit, except the last one.

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

   Author: John Sullivan <sullivan@eazel.com>,
 */

#include <config.h>
#include "nautilus-keep-last-vertical-box.h"

#include <eel/eel-gtk-macros.h>

static void     nautilus_keep_last_vertical_box_class_init  (NautilusKeepLastVerticalBoxClass *class);
static void     nautilus_keep_last_vertical_box_init        (NautilusKeepLastVerticalBox      *box);
static void	nautilus_keep_last_vertical_box_size_allocate 	  (GtkWidget 			    *widget, 
								   GtkAllocation 		    *allocation);

EEL_CLASS_BOILERPLATE (NautilusKeepLastVerticalBox, nautilus_keep_last_vertical_box, GTK_TYPE_VBOX)

/* Standard class initialization function */
static void
nautilus_keep_last_vertical_box_class_init (NautilusKeepLastVerticalBoxClass *klass)
{
	GtkWidgetClass *widget_class;

	widget_class = (GtkWidgetClass *) klass;

	widget_class->size_allocate = nautilus_keep_last_vertical_box_size_allocate;
}

/* Standard object initialization function */
static void
nautilus_keep_last_vertical_box_init (NautilusKeepLastVerticalBox *box)
{
}


/* nautilus_keep_last_vertical_box_new:
 * 
 * Create a new vertical box that clips off items from the end that don't
 * fit, except the last item, which is always kept. When packing this widget
 * into another vbox, use TRUE for expand and TRUE for fill or this class's
 * special clipping magic won't work because this widget's allocation might
 * be larger than the available space.
 * 
 * @spacing: Vertical space between items.
 * 
 * Return value: A new NautilusKeepLastVerticalBox
 */
GtkWidget *
nautilus_keep_last_vertical_box_new (gint spacing)
{
	NautilusKeepLastVerticalBox *box;

	box = NAUTILUS_KEEP_LAST_VERTICAL_BOX (gtk_widget_new (nautilus_keep_last_vertical_box_get_type (), NULL));

	GTK_BOX (box)->spacing = spacing;

	/* If homogeneous is TRUE and there are too many items to fit
	 * naturally, they will be squashed together to fit in the space.
	 * We want the ones that don't fit to be not shown at all, so
	 * we set homogeneous to FALSE.
	 */
	GTK_BOX (box)->homogeneous = FALSE;

	return GTK_WIDGET (box);
}

static void	
nautilus_keep_last_vertical_box_size_allocate (GtkWidget *widget, 
					       GtkAllocation *allocation)
{
	GtkBox *box;
	GtkBoxChild *last_child, *child;
	GList *children;
	GtkAllocation last_child_allocation, child_allocation, tiny_allocation;

	g_return_if_fail (NAUTILUS_IS_KEEP_LAST_VERTICAL_BOX (widget));
	g_return_if_fail (allocation != NULL);

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	box = GTK_BOX (widget);
	children = g_list_last (box->children);

	if (children != NULL) {
		last_child = children->data;
		children = children->prev;

		last_child_allocation = last_child->widget->allocation;

		/* If last child doesn't fit vertically, prune items from the end of the
		 * list one at a time until it does.
		 */
		if (last_child_allocation.y + last_child_allocation.height >
		    allocation->y + allocation->height) {

			while (children != NULL) {
				child = children->data;
				children = children->prev;

				child_allocation = child->widget->allocation;

				/* Reallocate this child's position so that it does not appear.
				 * Setting the width & height to 0 is not enough, as
				 * one pixel is still drawn. Must also move it outside
				 * visible range. For the cases I've seen, -1, -1 works fine.
				 * This might not work in all future cases. Alternatively, the
				 * items that don't fit could be hidden, but that would interfere
				 * with having other hidden children.
				 * 
				 * Note that these children are having their size allocated twice,
				 * once by gtk_vbox_size_allocate and then again here. I don't
				 * know of any problems with this, but holler if you do.
				 */
				tiny_allocation.x = tiny_allocation.y = -1;
				tiny_allocation.height = tiny_allocation.width = 0;
				gtk_widget_size_allocate (child->widget, &tiny_allocation);

				/* We're done if the special last item fits now. */
				if (child_allocation.y + last_child_allocation.height <=
				    allocation->y + allocation->height) {
					last_child_allocation.y = child_allocation.y;
					gtk_widget_size_allocate (last_child->widget, &last_child_allocation);
					break;
				}

				/* If the special last item still doesn't fit, but we've
				 * run out of earlier items, then the special last item is
				 * just too darn tall. Let's squash it down to fit in the box's
				 * allocation.
				 */
				if (children == NULL) {
					last_child_allocation.y = allocation->y;
					last_child_allocation.height = allocation->height;
					gtk_widget_size_allocate (last_child->widget, &last_child_allocation);
				}
			}
		}
	}
}		       			      
