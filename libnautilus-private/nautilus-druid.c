/* gnome-druid.c
 * Copyright (C) 1999 Red Hat, Inc.
 * Copyright (C) 2000 Eazel, Inc.
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

#include "nautilus-druid.h"
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnome/gnome-i18n.h>

#include <eel/eel-gtk-macros.h>

static void nautilus_druid_initialize	(NautilusDruid		 *druid);
static void nautilus_druid_initialize_class(NautilusDruidClass	 *klass);
static void nautilus_druid_size_request (GtkWidget               *widget,
					 GtkRequisition          *requisition);
static void nautilus_druid_size_allocate(GtkWidget               *widget,
					 GtkAllocation           *allocation);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusDruid, nautilus_druid, GNOME_TYPE_DRUID)

static void
nautilus_druid_initialize_class (NautilusDruidClass *klass)
{
	GtkWidgetClass *widget_class;

	widget_class = (GtkWidgetClass*) klass;

	parent_class = gtk_type_class (gtk_container_get_type ());

	widget_class->size_request = nautilus_druid_size_request;
	widget_class->size_allocate = nautilus_druid_size_allocate;
}

static void
nautilus_druid_initialize (NautilusDruid *druid)
{
}

static void
nautilus_druid_size_request (GtkWidget *widget,
			     GtkRequisition *requisition)
{
	guint16 temp_width, temp_height;
	GList *list;
	GnomeDruid *druid;
	GtkRequisition child_requisition;
	GnomeDruidPage *child;
	int border;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));

	border = GTK_CONTAINER(widget)->border_width;

	druid = GNOME_DRUID (widget);
	temp_height = temp_width = 0;

	/* We find the maximum size of all children widgets */
	for (list = druid->children; list; list = list->next) {
		child = GNOME_DRUID_PAGE (list->data);
		if (GTK_WIDGET_VISIBLE (child)) {
			gtk_widget_size_request (GTK_WIDGET (child), &child_requisition);
			temp_width = MAX (temp_width, child_requisition.width);
			temp_height = MAX (temp_height, child_requisition.height);
			if (GTK_WIDGET_MAPPED (child) && child != druid->current)
				gtk_widget_unmap (GTK_WIDGET(child));
		}
	}
	
        requisition->width = temp_width + 2 * border;
        requisition->height = temp_height + 2 * border;

	/* In an Attempt to show how the widgets are packed,
	 * here's a little diagram.
	 * 
	 * [ Cancel ] ------------- [  Back  ] [  Next  ]
	 *                |
	 *     This part needs to be at least 1 button width.
	 *     In addition, there is a GNOME_PAD_SMALL between Next and Back.
	 */
	/* our_button width is temp_width and temp_height */
	temp_height = 0;
	temp_width = 0;

	gtk_widget_size_request (druid->back, &child_requisition);
	temp_width = MAX (temp_width, child_requisition.width);
	temp_height = MAX (temp_height, child_requisition.height);

	gtk_widget_size_request (druid->next, &child_requisition);
	temp_width = MAX (temp_width, child_requisition.width);
	temp_height = MAX (temp_height, child_requisition.height);

	gtk_widget_size_request (druid->cancel, &child_requisition);
	temp_width = MAX (temp_width, child_requisition.width);
	temp_height = MAX (temp_height, child_requisition.height);

	gtk_widget_size_request (druid->finish, &child_requisition);
	temp_width = MAX (temp_width, child_requisition.width);
	temp_height = MAX (temp_height, child_requisition.height);

	temp_width += border * 2;
	temp_height += GNOME_PAD_SMALL;
	temp_width = temp_width * 4  + GNOME_PAD_SMALL * 3;

	/* pick which is bigger, the buttons, or the NautilusDruidPages */
	requisition->width = MAX (temp_width, requisition->width);
	requisition->height += temp_height;
}

static void
nautilus_druid_size_allocate (GtkWidget *widget,
			      GtkAllocation *allocation)
{
	GnomeDruid *druid;
	GtkAllocation child_allocation;
	gint button_height;
	GList *list;
	int border;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));

	druid = GNOME_DRUID (widget);
	widget->allocation = *allocation;

	/* deal with the buttons */
	child_allocation.width = child_allocation.height = 0;
	child_allocation.width = druid->back->requisition.width;
	child_allocation.height = druid->back->requisition.height;
	child_allocation.width = MAX (child_allocation.width,
			    druid->next->requisition.width);
	child_allocation.height = MAX (child_allocation.height,
			    druid->next->requisition.height);
	child_allocation.width = MAX (child_allocation.width,
			    druid->cancel->requisition.width);
	child_allocation.height = MAX (child_allocation.height,
			    druid->cancel->requisition.height);

	child_allocation.height += GNOME_PAD_SMALL;
	button_height = child_allocation.height;
	child_allocation.width += 2 * GNOME_PAD_SMALL;
	child_allocation.y = allocation->y + allocation->height -
		GNOME_PAD_SMALL - child_allocation.height;

	/* allocate cancel */
	child_allocation.x = allocation->x + GNOME_PAD_SMALL;
	gtk_widget_size_allocate (druid->cancel, &child_allocation);


	/* Allocate next/finish */
	child_allocation.x = allocation->x + allocation->width -
		GNOME_PAD_SMALL - child_allocation.width;
	gtk_widget_size_allocate (druid->next, &child_allocation);
	gtk_widget_size_allocate (druid->finish, &child_allocation);

	/* Allocate back */
	child_allocation.x -= (GNOME_PAD_SMALL + child_allocation.width);
	gtk_widget_size_allocate (druid->back, &child_allocation);

	border = GTK_CONTAINER (widget)->border_width;

	/* Put up the GnomeDruidPage */
	child_allocation.x = allocation->x + border;
	child_allocation.y = allocation->y + border;
	child_allocation.width =
		((allocation->width - 2 * border) > 0) ?
		(allocation->width - 2 * border):0;
	child_allocation.height =
		((allocation->height - 2 * border - GNOME_PAD_SMALL - button_height) > 0) ?
		(allocation->height - 2 * border - GNOME_PAD_SMALL - button_height):0;
	for (list = druid->children; list; list=list->next) {
		if (GTK_WIDGET_VISIBLE (list->data)) {
			gtk_widget_size_allocate (GTK_WIDGET (list->data), &child_allocation);
		}
	}
}

/* Public methods */
GtkWidget *
nautilus_druid_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_DRUID, NULL);
}
