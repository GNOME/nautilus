/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-generous-bin.c: Subclass of GtkVBox that clips off
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
#include "nautilus-generous-bin.h"

#include "nautilus-gtk-macros.h"

static void nautilus_generous_bin_initialize_class (NautilusGenerousBinClass *class);
static void nautilus_generous_bin_initialize       (NautilusGenerousBin      *box);
static void nautilus_generous_bin_size_request     (GtkWidget                *widget,
						    GtkRequisition           *requisition);
static void nautilus_generous_bin_size_allocate    (GtkWidget                *widget,
						    GtkAllocation            *allocation);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusGenerousBin, nautilus_generous_bin, GTK_TYPE_BIN)

static void
nautilus_generous_bin_initialize_class (NautilusGenerousBinClass *klass)
{
	GTK_WIDGET_CLASS (klass)->size_request = nautilus_generous_bin_size_request;
	GTK_WIDGET_CLASS (klass)->size_allocate = nautilus_generous_bin_size_allocate;
}

static void
nautilus_generous_bin_initialize (NautilusGenerousBin *bin)
{
}

static void
nautilus_generous_bin_size_request (GtkWidget *widget,
				    GtkRequisition *requisition)
{
	GtkBin *bin;
	GtkRequisition child_requisition;
	
	bin = GTK_BIN (widget);
	
	requisition->width = 0;
	requisition->height = 0;
	
	if (bin->child != NULL && GTK_WIDGET_VISIBLE (bin->child)) {
		gtk_widget_size_request (bin->child, &child_requisition);
		
		requisition->width += child_requisition.width;
		requisition->height += child_requisition.height;
	}
}

static void
nautilus_generous_bin_size_allocate (GtkWidget *widget,
				     GtkAllocation *allocation)
{
	GtkBin *bin;
	GtkAllocation child_allocation;
	
	bin = GTK_BIN (widget);
	
	widget->allocation = *allocation;

	if (bin->child != NULL && GTK_WIDGET_VISIBLE (bin->child)) {
		child_allocation = *allocation;
		gtk_widget_size_allocate (bin->child, &child_allocation);
	}
}
