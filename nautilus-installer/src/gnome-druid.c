/* gnome-druid.c
 * Copyright (C) 1999 Red Hat, Inc.
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

#include <config.h>

#include <libtrilobite/trilobite-i18n.h>

#include "gnome-druid.h"
#include "fake-stock.h"

enum {
	CANCEL,
	LAST_SIGNAL
};
static void gnome_druid_init		(GnomeDruid		 *druid);
static void gnome_druid_class_init	(GnomeDruidClass	 *klass);
static void gnome_druid_destroy         (GtkObject               *object);
static void gnome_druid_size_request    (GtkWidget               *widget,
					 GtkRequisition          *requisition);
static void gnome_druid_size_allocate   (GtkWidget               *widget,
					 GtkAllocation           *allocation);
static void gnome_druid_draw            (GtkWidget               *widget,
					 GdkRectangle            *area);
static gint gnome_druid_expose          (GtkWidget               *widget,
					 GdkEventExpose          *event);
static void gnome_druid_map             (GtkWidget               *widget);
static void gnome_druid_unmap           (GtkWidget               *widget);
static GtkType gnome_druid_child_type   (GtkContainer            *container);
static void gnome_druid_add             (GtkContainer            *widget,
					 GtkWidget               *page);
static void gnome_druid_remove          (GtkContainer            *widget,
					 GtkWidget               *child);
static void gnome_druid_forall          (GtkContainer            *container,
					 gboolean                include_internals,
					 GtkCallback             callback,
					 gpointer                callback_data);
static void gnome_druid_back_callback   (GtkWidget               *button,
					 GnomeDruid              *druid);
static void gnome_druid_next_callback   (GtkWidget               *button,
					 GnomeDruid              *druid);
static void gnome_druid_cancel_callback (GtkWidget               *button,
					 GtkWidget               *druid);
static GtkContainerClass *parent_class = NULL;
static guint druid_signals[LAST_SIGNAL] = { 0 };


GtkType
gnome_druid_get_type (void)
{
  static GtkType druid_type = 0;

  if (!druid_type)
    {
      static const GtkTypeInfo druid_info =
      {
        "GnomeDruid",
        sizeof (GnomeDruid),
        sizeof (GnomeDruidClass),
        (GtkClassInitFunc) gnome_druid_class_init,
        (GtkObjectInitFunc) gnome_druid_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      druid_type = gtk_type_unique (gtk_container_get_type (), &druid_info);
    }

  return druid_type;
}

static void
gnome_druid_class_init (GnomeDruidClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	object_class = (GtkObjectClass*) klass;
	widget_class = (GtkWidgetClass*) klass;
	container_class = (GtkContainerClass*) klass;
	parent_class = gtk_type_class (gtk_container_get_type ());

	druid_signals[CANCEL] = 
		gtk_signal_new ("cancel",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeDruidClass, cancel),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals (object_class, druid_signals, LAST_SIGNAL);
	
	object_class->destroy = gnome_druid_destroy;
	widget_class->size_request = gnome_druid_size_request;
	widget_class->size_allocate = gnome_druid_size_allocate;
	widget_class->map = gnome_druid_map;
	widget_class->unmap = gnome_druid_unmap;
	widget_class->draw = gnome_druid_draw;
	widget_class->expose_event = gnome_druid_expose;

	container_class->forall = gnome_druid_forall;
	container_class->add = gnome_druid_add;
	container_class->remove = gnome_druid_remove;
	container_class->child_type = gnome_druid_child_type;
}


static void
gnome_druid_init (GnomeDruid *druid)
{
	GtkWidget *pixmap;

	/* set up the buttons */
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (druid), GTK_NO_WINDOW);

	pixmap = fake_stock_pixmap_new_from_xpm_data (stock_left_arrow_xpm);
	druid->back = fake_stock_pixmap_button (pixmap, _("Back"));
	GTK_WIDGET_SET_FLAGS (druid->back, GTK_CAN_DEFAULT);

	pixmap = fake_stock_pixmap_new_from_xpm_data (stock_right_arrow_xpm);
	druid->next = fake_stock_pixmap_button (pixmap, _("Next"));
	GTK_WIDGET_SET_FLAGS (druid->next, GTK_CAN_DEFAULT);

	pixmap = fake_stock_pixmap_new_from_xpm_data (stock_button_cancel_xpm);
	druid->cancel = fake_stock_pixmap_button (pixmap, _("Cancel"));
	GTK_WIDGET_SET_FLAGS (druid->cancel, GTK_CAN_DEFAULT);

	pixmap = fake_stock_pixmap_new_from_xpm_data (stock_button_apply_xpm);
	druid->finish = fake_stock_pixmap_button (pixmap, _("Finish"));
	GTK_WIDGET_SET_FLAGS (druid->finish, GTK_CAN_DEFAULT);

	gtk_widget_set_parent (druid->back, GTK_WIDGET (druid));
	gtk_widget_set_parent (druid->next, GTK_WIDGET (druid));
	gtk_widget_set_parent (druid->cancel, GTK_WIDGET (druid));
	gtk_widget_set_parent (druid->finish, GTK_WIDGET (druid));
	gtk_widget_show (druid->back);
	gtk_widget_show (druid->next);
	gtk_widget_show (druid->cancel);
	gtk_widget_show (druid->finish);

	/* other flags */
	druid->current = NULL;
	druid->children = NULL;
	druid->show_finish = FALSE;
	gtk_signal_connect (GTK_OBJECT (druid->back),
			    "clicked",
			    gnome_druid_back_callback,
			    druid);
	gtk_signal_connect (GTK_OBJECT (druid->next),
			    "clicked",
			    gnome_druid_next_callback,
			    druid);
	gtk_signal_connect (GTK_OBJECT (druid->cancel),
			    "clicked",
			    gnome_druid_cancel_callback,
			    druid);
	gtk_signal_connect (GTK_OBJECT (druid->finish),
			    "clicked",
			    gnome_druid_next_callback,
			    druid);
}



static void
gnome_druid_destroy (GtkObject *object)
{
	GnomeDruid *druid;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_DRUID (object));

	druid = GNOME_DRUID (object);

	if(druid->back) {
		gtk_widget_destroy (druid->back);
		druid->back = NULL;
	}
	if(druid->next) {
		gtk_widget_destroy (druid->next);
		druid->next = NULL;
	}
	if(druid->cancel) {
		gtk_widget_destroy (druid->cancel);
		druid->cancel = NULL;
	}
	if(druid->finish) {
		gtk_widget_destroy (druid->finish);
		druid->finish = NULL;
	}

	/* Remove all children, we set current to NULL so
	 * that the remove code doesn't try to do anything funny */
	druid->current = NULL;
	while (druid->children != NULL) {
		GnomeDruidPage *child = druid->children->data;
		gtk_container_remove (GTK_CONTAINER (druid), GTK_WIDGET(child));
	}

        GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gnome_druid_size_request (GtkWidget *widget,
			  GtkRequisition *requisition)
{
	guint16 temp_width, temp_height;
	GList *list;
	GnomeDruid *druid;
	GtkRequisition child_requisition;
	GnomeDruidPage *child;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_DRUID (widget));

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
	
        requisition->width = temp_width + 2 * GNOME_PAD_SMALL;
        requisition->height = temp_height + 2 * GNOME_PAD_SMALL;

	/* In an Attempt to show how the widgets are packed,
	 * here's a little diagram.
	 * 
	 * ------------- [  Back  ] [  Next  ]    [ Cancel ]
	 *    \
	 *     This part needs to be at least 1 button width.
	 *     In addition, there is 1/4 X Button width between Cancel and Next,
	 *     and a GNOME_PAD_SMALL between Next and Back.
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

	temp_width += GNOME_PAD_SMALL * 2;
	temp_height += GNOME_PAD_SMALL;
	/* FIXME. do we need to do something with the buttons requisition? */
	temp_width = temp_width * 17/4  + GNOME_PAD_SMALL * 3;

	/* pick which is bigger, the buttons, or the GnomeDruidPages */
	requisition->width = MAX (temp_width, requisition->width);
	requisition->height += temp_height + GNOME_PAD_SMALL * 2;
	/* And finally, put the side padding in */
	requisition->width += GNOME_PAD_SMALL *2;
}
static void
gnome_druid_size_allocate (GtkWidget *widget,
			   GtkAllocation *allocation)
{
	GnomeDruid *druid;
	GtkAllocation child_allocation;
	gint button_height;
	GList *list;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_DRUID (widget));

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
	child_allocation.x = allocation->x + allocation->width - GNOME_PAD_SMALL - child_allocation.width;
	child_allocation.y = allocation->y + allocation->height - GNOME_PAD_SMALL - child_allocation.height;
	gtk_widget_size_allocate (druid->cancel, &child_allocation);
	child_allocation.x -= (child_allocation.width * 5 / 4);
	gtk_widget_size_allocate (druid->next, &child_allocation);
	gtk_widget_size_allocate (druid->finish, &child_allocation);
	child_allocation.x -= (GNOME_PAD_SMALL + child_allocation.width);
	gtk_widget_size_allocate (druid->back, &child_allocation);

	/* Put up the GnomeDruidPage */
	child_allocation.x = allocation->x + GNOME_PAD_SMALL;
	child_allocation.y = allocation->y + GNOME_PAD_SMALL;
	child_allocation.width =
		((allocation->width - 2* GNOME_PAD_SMALL) > 0) ?
		(allocation->width - 2* GNOME_PAD_SMALL):0;
	child_allocation.height =
		((allocation->height - 3 * GNOME_PAD_SMALL - button_height) > 0) ?
		(allocation->height - 3 * GNOME_PAD_SMALL - button_height):0;
	for (list = druid->children; list; list=list->next) {
		if (GTK_WIDGET_VISIBLE (list->data)) {
			gtk_widget_size_allocate (GTK_WIDGET (list->data), &child_allocation);
		}
	}
}

static GtkType
gnome_druid_child_type (GtkContainer *container)
{
	return gnome_druid_page_get_type ();
}

static void
gnome_druid_map (GtkWidget *widget)
{
	GnomeDruid *druid;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_DRUID (widget));

	druid = GNOME_DRUID (widget);
	GTK_WIDGET_SET_FLAGS (druid, GTK_MAPPED);

	gtk_widget_map (druid->back);
	if (druid->show_finish)
		gtk_widget_map (druid->finish);
	else
		gtk_widget_map (druid->next);
	gtk_widget_map (druid->cancel);
	if (druid->current &&
	    GTK_WIDGET_VISIBLE (druid->current) &&
	    !GTK_WIDGET_MAPPED (druid->current)) {
		gtk_widget_map (GTK_WIDGET (druid->current));
	}
}

static void
gnome_druid_unmap (GtkWidget *widget)
{
	GnomeDruid *druid;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_DRUID (widget));

	druid = GNOME_DRUID (widget);
	GTK_WIDGET_UNSET_FLAGS (druid, GTK_MAPPED);

	gtk_widget_unmap (druid->back);
	if (druid->show_finish)
		gtk_widget_unmap (druid->finish);
	else
		gtk_widget_unmap (druid->next);
	gtk_widget_unmap (druid->cancel);
	if (druid->current &&
	    GTK_WIDGET_VISIBLE (druid->current) &&
	    GTK_WIDGET_MAPPED (druid->current))
		gtk_widget_unmap (GTK_WIDGET (druid->current));
}
static void
gnome_druid_add (GtkContainer *widget,
		 GtkWidget *page)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_DRUID (widget));
	g_return_if_fail (page != NULL);
	g_return_if_fail (GNOME_IS_DRUID_PAGE (page));

	gnome_druid_append_page (GNOME_DRUID (widget), GNOME_DRUID_PAGE (page));
}
static void
gnome_druid_remove (GtkContainer *widget,
		    GtkWidget *child)
{
	GnomeDruid *druid;
	GList *list;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_DRUID (widget));
	g_return_if_fail (child != NULL);

	druid = GNOME_DRUID (widget);

	list = g_list_find (druid->children, child);
	/* Is it a page? */ 
	if (list != NULL) {
		/* If we are mapped and visible, we want to deal with changing the page. */
		if ((GTK_WIDGET_MAPPED (GTK_WIDGET (widget))) &&
		    (list->data == (gpointer) druid->current) &&
		    (list->next != NULL)) {
			gnome_druid_set_page (druid, GNOME_DRUID_PAGE (list->next->data));
		}
	}
	druid->children = g_list_remove (druid->children, child);
	gtk_widget_unparent (child);
}

static void
gnome_druid_forall (GtkContainer *container,
		    gboolean      include_internals,
		    GtkCallback   callback,
		    gpointer      callback_data)
{
	GnomeDruid *druid;
	GnomeDruidPage *child;
	GList *children;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_DRUID (container));
	g_return_if_fail (callback != NULL);

	druid = GNOME_DRUID (container);

	children = druid->children;
	while (children) {
		child = children->data;
		children = children->next;

		(* callback) (GTK_WIDGET (child), callback_data);
	}
	if (include_internals) {
		(* callback) (druid->back, callback_data);
		(* callback) (druid->next, callback_data);
		(* callback) (druid->cancel, callback_data);
		(* callback) (druid->finish, callback_data);
	}
}
static void
gnome_druid_draw (GtkWidget    *widget,
		  GdkRectangle *area)
{
	GnomeDruid *druid;
	GdkRectangle child_area;
	GtkWidget *child;
	GList *children;
  
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_DRUID (widget));

	if (GTK_WIDGET_DRAWABLE (widget)) {
		druid = GNOME_DRUID (widget);
		children = druid->children;

		while (children) {
			child = GTK_WIDGET (children->data);
			children = children->next;
	     
			if (GTK_WIDGET_DRAWABLE (child) && gtk_widget_intersect (child, area, &child_area)) {
				gtk_widget_draw (child, &child_area);
			}
		}
		child = druid->back;
		if (GTK_WIDGET_DRAWABLE (child) && gtk_widget_intersect (child, area, &child_area))
			gtk_widget_draw (child, &child_area);
		child = druid->next;
		if (GTK_WIDGET_DRAWABLE (child) && gtk_widget_intersect (child, area, &child_area))
			gtk_widget_draw (child, &child_area);
		child = druid->cancel;
		if (GTK_WIDGET_DRAWABLE (child) && gtk_widget_intersect (child, area, &child_area))
			gtk_widget_draw (child, &child_area);
		child = druid->finish;
		if (GTK_WIDGET_DRAWABLE (child) && gtk_widget_intersect (child, area, &child_area))
			gtk_widget_draw (child, &child_area);
	}
}

static gint
gnome_druid_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
	GnomeDruid *druid;
	GtkWidget *child;
	GdkEventExpose child_event;
	GList *children;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_DRUID (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		druid = GNOME_DRUID (widget);
		child_event = *event;
		children = druid->children;

		while (children) {
			child = GTK_WIDGET (children->data);
			children = children->next;

			if (GTK_WIDGET_DRAWABLE (child) &&
			    GTK_WIDGET_NO_WINDOW (child) &&
			    gtk_widget_intersect (child, &event->area, &child_event.area)) {
				gtk_widget_event (child, (GdkEvent*) &child_event);
			}
		}
		child = druid->back;
		if (GTK_WIDGET_DRAWABLE (child) &&
		    GTK_WIDGET_NO_WINDOW (child) &&
		    gtk_widget_intersect (child, &event->area, &child_event.area))
			gtk_widget_event (child, (GdkEvent*) &child_event);
		child = druid->next;
		if (GTK_WIDGET_DRAWABLE (child) &&
		    GTK_WIDGET_NO_WINDOW (child) &&
		    gtk_widget_intersect (child, &event->area, &child_event.area))
			gtk_widget_event (child, (GdkEvent*) &child_event);
		child = druid->cancel;
		if (GTK_WIDGET_DRAWABLE (child) &&
		    GTK_WIDGET_NO_WINDOW (child) &&
		    gtk_widget_intersect (child, &event->area, &child_event.area))
			gtk_widget_event (child, (GdkEvent*) &child_event);
		child = druid->finish;
		if (GTK_WIDGET_DRAWABLE (child) &&
		    GTK_WIDGET_NO_WINDOW (child) &&
		    gtk_widget_intersect (child, &event->area, &child_event.area))
			gtk_widget_event (child, (GdkEvent*) &child_event);
	}
	return FALSE;
}

static void
gnome_druid_back_callback (GtkWidget *button, GnomeDruid *druid)
{
	GList *list;
	g_return_if_fail (druid->current != NULL);

	if (gnome_druid_page_back (druid->current))
		return;

	/* Make sure that we have a next list item */
	list = g_list_find (druid->children, druid->current);
	g_return_if_fail (list->prev != NULL);
	gnome_druid_set_page (druid, GNOME_DRUID_PAGE (list->prev->data));
}
static void
gnome_druid_next_callback (GtkWidget *button, GnomeDruid *druid)
{
	GList *list;
	g_return_if_fail (druid->current != NULL);

	if (druid->show_finish == FALSE) {
		if (gnome_druid_page_next (druid->current))
			return;

		/* Make sure that we have a next list item */
		/* FIXME: we want to find the next VISIBLE one... */
		list = g_list_find (druid->children, druid->current);
		g_return_if_fail (list->next != NULL);
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (list->next->data));
	} else {
		gnome_druid_page_finish (druid->current);
	}
}
static void
gnome_druid_cancel_callback (GtkWidget *button, GtkWidget *druid)
{
     if (GNOME_DRUID (druid)->current) {
	     if (gnome_druid_page_cancel (GNOME_DRUID (druid)->current))
		     return;

	     gtk_signal_emit (GTK_OBJECT (druid), druid_signals [CANCEL]);
     }
}

/* Public Functions */
GtkWidget *
gnome_druid_new (void)
{
	return GTK_WIDGET (gtk_type_new (gnome_druid_get_type ()));
}

/**
 * gnome_druid_set_buttons_sensitive
 * @druid: A Druid.
 * @back_sensitive: The sensitivity of the back button.
 * @next_sensitive: The sensitivity of the next button.
 * @cancel_sensitive: The sensitivity of the cancel button.
 *
 * Description: Sets the sensitivity of the @druid's control-buttons.  If the
 * variables are TRUE, then they will be clickable.  This function is used
 * primarily by the actual GnomeDruidPage widgets.
 **/

void
gnome_druid_set_buttons_sensitive (GnomeDruid *druid,
				   gboolean back_sensitive,
				   gboolean next_sensitive,
				   gboolean cancel_sensitive)
{
	g_return_if_fail (druid != NULL);
	g_return_if_fail (GNOME_IS_DRUID (druid));

	gtk_widget_set_sensitive (druid->back, back_sensitive);
	gtk_widget_set_sensitive (druid->next, next_sensitive);
	gtk_widget_set_sensitive (druid->cancel, cancel_sensitive);
}
/**
 * gnome_druid_set_show_finish
 * @druid: A Druid widget.
 # @show_finish: If TRUE, then the "Cancel" button is changed to be "Finish"
 *
 * Description: Sets the text on the last button on the @druid.  If @show_finish
 * is TRUE, then the text becomes "Finish".  If @show_finish is FALSE, then the
 * text becomes "Cancel".
 **/
void
gnome_druid_set_show_finish (GnomeDruid *druid,
			     gboolean show_finish)
{
	g_return_if_fail (druid != NULL);
	g_return_if_fail (GNOME_IS_DRUID (druid));

	if (show_finish) {
		if (GTK_WIDGET_MAPPED (druid->next)) {
			gtk_widget_unmap (druid->next);
			gtk_widget_map (druid->finish);
		}
	} else {
		if (GTK_WIDGET_MAPPED (druid->finish)) {
			gtk_widget_unmap (druid->finish);
			gtk_widget_map (druid->next);
		}
	}
	druid->show_finish = show_finish;
}
/**
 * gnome_druid_prepend_page:
 * @druid: A Druid widget.
 * @page: The page to be inserted.
 * 
 * Description: This will prepend a GnomeDruidPage into the internal list of
 * pages that the @druid has.
 **/
void
gnome_druid_prepend_page (GnomeDruid *druid,
			  GnomeDruidPage *page)
{
	g_return_if_fail (druid != NULL);
	g_return_if_fail (GNOME_IS_DRUID (druid));
	g_return_if_fail (page != NULL);
	g_return_if_fail (GNOME_IS_DRUID_PAGE (page));

	gnome_druid_insert_page (druid, NULL, page);
}
/**
 * gnome_druid_insert_page:
 * @druid: A Druid widget.
 * @back_page: The page prior to the page to be inserted.
 * @page: The page to insert.
 * 
 * Description: This will insert @page after @back_page into the list of
 * internal pages that the @druid has.  If @back_page is not present in the list
 * or NULL, @page will be prepended to the list.
 **/
void
gnome_druid_insert_page (GnomeDruid *druid,
			 GnomeDruidPage *back_page,
			 GnomeDruidPage *page)
{
	GList *list;

	g_return_if_fail (druid != NULL);
	g_return_if_fail (GNOME_IS_DRUID (druid));
	g_return_if_fail (page != NULL);
	g_return_if_fail (GNOME_IS_DRUID_PAGE (page));

	list = g_list_find (druid->children, back_page);
	if (list == NULL)
		druid->children = g_list_prepend (druid->children, page);
	else {
		GList *new_el = g_list_alloc ();
		new_el->next = list->next;
		new_el->prev = list;
		if (new_el->next) 
			new_el->next->prev = new_el;
		new_el->prev->next = new_el;
		new_el->data = (gpointer) page;
	}
	gtk_widget_set_parent (GTK_WIDGET (page), GTK_WIDGET (druid));

	if (GTK_WIDGET_REALIZED (GTK_WIDGET (druid)))
		gtk_widget_realize (GTK_WIDGET (page));

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (druid)) && GTK_WIDGET_VISIBLE (GTK_WIDGET (page))) {
		if (GTK_WIDGET_MAPPED (GTK_WIDGET (page)))
			gtk_widget_unmap (GTK_WIDGET (page));
		gtk_widget_queue_resize (GTK_WIDGET (druid));
	}
	
	/* if it's the first and only page, we want to bring it to the foreground. */
	if (druid->children->next == NULL)
		gnome_druid_set_page (druid, page);
}

/**
 * gnome_druid_append_page: 
 * @druid: A Druid widget.
 * @page: The page to be appended.
 * 
 * Description: This will append @page onto the end of the internal list.  
 **/
void gnome_druid_append_page (GnomeDruid *druid,
			      GnomeDruidPage *page)
{
	GList *list;
	g_return_if_fail (druid != NULL);
	g_return_if_fail (GNOME_IS_DRUID (druid));
	g_return_if_fail (page != NULL);
	g_return_if_fail (GNOME_IS_DRUID_PAGE (page));

	list = g_list_last (druid->children);
	if (list) {
		gnome_druid_insert_page (druid, GNOME_DRUID_PAGE (list->data), page);
	} else {
		gnome_druid_insert_page (druid, NULL, page);
	}	
}
/**
 * gnome_druid_set_page:
 * @druid: A Druid widget.
 * @page: The page to be brought to the foreground.
 * 
 * Description: This will make @page the currently showing page in the druid.
 * @page must already be in the druid.
 **/
void
gnome_druid_set_page (GnomeDruid *druid,
		      GnomeDruidPage *page)
{
	GList *list;
	GtkWidget *old = NULL;
	g_return_if_fail (druid != NULL);
	g_return_if_fail (GNOME_IS_DRUID (druid));
	g_return_if_fail (page != NULL);
	g_return_if_fail (GNOME_IS_DRUID_PAGE (page));

	if (druid->current == page)
	     return;
	list = g_list_find (druid->children, page);
	g_return_if_fail (list != NULL);

	if ((druid->current) && (GTK_WIDGET_VISIBLE (druid->current)) && (GTK_WIDGET_MAPPED (druid))) {
		old = GTK_WIDGET (druid->current);
	}
	druid->current = GNOME_DRUID_PAGE (list->data);
	gnome_druid_page_prepare (druid->current);
	if (GTK_WIDGET_VISIBLE (druid->current) && (GTK_WIDGET_MAPPED (druid))) {
		gtk_widget_map (GTK_WIDGET (druid->current));
	}
	if (old && GTK_WIDGET_MAPPED (old))
	  gtk_widget_unmap (old);
}
