/* gnome-druid.c
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

#include "nautilus-druid.h"
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnome/gnome-i18n.h>

struct _NautilusDruidPrivate
{
	NautilusDruidPage *current;
	GList *children;

	gboolean show_finish : 1; /* if TRUE, then we are showing the finish button instead of the next button */
};

enum {
	CANCEL,
	LAST_SIGNAL
};
static void nautilus_druid_init		(NautilusDruid		 *druid);
static void nautilus_druid_class_init	(NautilusDruidClass	 *klass);
static void nautilus_druid_destroy         (GtkObject               *object);
static void nautilus_druid_size_request    (GtkWidget               *widget,
					 GtkRequisition          *requisition);
static void nautilus_druid_size_allocate   (GtkWidget               *widget,
					 GtkAllocation           *allocation);
static void nautilus_druid_draw            (GtkWidget               *widget,
					 GdkRectangle            *area);
static gint nautilus_druid_expose          (GtkWidget               *widget,
					 GdkEventExpose          *event);
static void nautilus_druid_map             (GtkWidget               *widget);
static void nautilus_druid_unmap           (GtkWidget               *widget);
static GtkType nautilus_druid_child_type   (GtkContainer            *container);
static void nautilus_druid_add             (GtkContainer            *widget,
					 GtkWidget               *page);
static void nautilus_druid_remove          (GtkContainer            *widget,
					 GtkWidget               *child);
static void nautilus_druid_forall          (GtkContainer            *container,
					 gboolean                include_internals,
					 GtkCallback             callback,
					 gpointer                callback_data);
static void nautilus_druid_back_callback   (GtkWidget               *button,
					 NautilusDruid              *druid);
static void nautilus_druid_next_callback   (GtkWidget               *button,
					 NautilusDruid              *druid);
static void nautilus_druid_cancel_callback (GtkWidget               *button,
					 GtkWidget               *druid);
static GtkContainerClass *parent_class = NULL;
static guint druid_signals[LAST_SIGNAL] = { 0 };


GtkType
nautilus_druid_get_type (void)
{
  static GtkType druid_type = 0;

  if (!druid_type)
    {
      static const GtkTypeInfo druid_info =
      {
        "NautilusDruid",
        sizeof (NautilusDruid),
        sizeof (NautilusDruidClass),
        (GtkClassInitFunc) nautilus_druid_class_init,
        (GtkObjectInitFunc) nautilus_druid_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      druid_type = gtk_type_unique (gtk_container_get_type (), &druid_info);
    }

  return druid_type;
}

static void
nautilus_druid_class_init (NautilusDruidClass *klass)
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
				GTK_SIGNAL_OFFSET (NautilusDruidClass, cancel),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals (object_class, druid_signals, LAST_SIGNAL);
	
	object_class->destroy = nautilus_druid_destroy;
	widget_class->size_request = nautilus_druid_size_request;
	widget_class->size_allocate = nautilus_druid_size_allocate;
	widget_class->map = nautilus_druid_map;
	widget_class->unmap = nautilus_druid_unmap;
	widget_class->draw = nautilus_druid_draw;
	widget_class->expose_event = nautilus_druid_expose;

	container_class->forall = nautilus_druid_forall;
	container_class->add = nautilus_druid_add;
	container_class->remove = nautilus_druid_remove;
	container_class->child_type = nautilus_druid_child_type;
}

static void
nautilus_druid_init (NautilusDruid *druid)
{
	GtkWidget *pixmap;

	druid->_priv = g_new0(NautilusDruidPrivate, 1);

	/* set up the buttons */
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (druid), GTK_NO_WINDOW);
	pixmap =  gnome_stock_new_with_icon(GNOME_STOCK_BUTTON_PREV);
	druid->back = gnome_pixmap_button (pixmap, _("Back"));
	GTK_WIDGET_SET_FLAGS (druid->back, GTK_CAN_DEFAULT);
	druid->next = gnome_stock_or_ordinary_button (GNOME_STOCK_BUTTON_NEXT);
	GTK_WIDGET_SET_FLAGS (druid->next, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS (druid->next, GTK_HAS_FOCUS);
	druid->cancel = gnome_stock_or_ordinary_button (GNOME_STOCK_BUTTON_CANCEL);
	GTK_WIDGET_SET_FLAGS (druid->cancel, GTK_CAN_DEFAULT);
	pixmap =  gnome_stock_new_with_icon(GNOME_STOCK_BUTTON_APPLY);
	druid->finish = gnome_pixmap_button (pixmap, _("Finish"));
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
	druid->_priv->current = NULL;
	druid->_priv->children = NULL;
	druid->_priv->show_finish = FALSE;
	gtk_signal_connect (GTK_OBJECT (druid->back),
			    "clicked",
			    nautilus_druid_back_callback,
			    druid);
	gtk_signal_connect (GTK_OBJECT (druid->next),
			    "clicked",
			    nautilus_druid_next_callback,
			    druid);
	gtk_signal_connect (GTK_OBJECT (druid->cancel),
			    "clicked",
			    nautilus_druid_cancel_callback,
			    druid);
	gtk_signal_connect (GTK_OBJECT (druid->finish),
			    "clicked",
			    nautilus_druid_next_callback,
			    druid);
}



static void
nautilus_druid_destroy (GtkObject *object)
{
	NautilusDruid *druid;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (object));

	druid = NAUTILUS_DRUID (object);

        if(GTK_OBJECT_CLASS(parent_class)->destroy)
        	GTK_OBJECT_CLASS(parent_class)->destroy(object);

	gtk_widget_destroy (druid->back);
	druid->back = NULL;
	gtk_widget_destroy (druid->next);
	druid->next = NULL;
	gtk_widget_destroy (druid->cancel);
	druid->cancel = NULL;
	gtk_widget_destroy (druid->finish);
	druid->finish = NULL;
	g_list_free (druid->_priv->children);
        druid->_priv->children = NULL;

	g_free(druid->_priv);
	druid->_priv = NULL;
}

static void
nautilus_druid_size_request (GtkWidget *widget,
			  GtkRequisition *requisition)
{
	guint16 temp_width, temp_height;
	GList *list;
	NautilusDruid *druid;
	GtkRequisition child_requisition;
	NautilusDruidPage *child;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));

	druid = NAUTILUS_DRUID (widget);
	temp_height = temp_width = 0;

	/* We find the maximum size of all children widgets */
	for (list = druid->_priv->children; list; list = list->next) {
		child = NAUTILUS_DRUID_PAGE (list->data);
		if (GTK_WIDGET_VISIBLE (child)) {
			gtk_widget_size_request (GTK_WIDGET (child), &child_requisition);
			temp_width = MAX (temp_width, child_requisition.width);
			temp_height = MAX (temp_height, child_requisition.height);
			if (GTK_WIDGET_MAPPED (child) && child != druid->_priv->current)
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

	/* pick which is bigger, the buttons, or the NautilusDruidPages */
	requisition->width = MAX (temp_width, requisition->width);
	requisition->height += temp_height + GNOME_PAD_SMALL * 2;
	/* And finally, put the side padding in */
	requisition->width += GNOME_PAD_SMALL *2;
}
static void
nautilus_druid_size_allocate (GtkWidget *widget,
			   GtkAllocation *allocation)
{
	NautilusDruid *druid;
	GtkAllocation child_allocation;
	gint button_height;
	GList *list;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));

	druid = NAUTILUS_DRUID (widget);
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

	/* Put up the NautilusDruidPage */
	child_allocation.x = allocation->x + GNOME_PAD_SMALL;
	child_allocation.y = allocation->y + GNOME_PAD_SMALL;
	child_allocation.width =
		((allocation->width - 2* GNOME_PAD_SMALL) > 0) ?
		(allocation->width - 2* GNOME_PAD_SMALL):0;
	child_allocation.height =
		((allocation->height - 3 * GNOME_PAD_SMALL - button_height) > 0) ?
		(allocation->height - 3 * GNOME_PAD_SMALL - button_height):0;
	for (list = druid->_priv->children; list; list=list->next) {
		if (GTK_WIDGET_VISIBLE (list->data)) {
			gtk_widget_size_allocate (GTK_WIDGET (list->data), &child_allocation);
		}
	}
}

static GtkType
nautilus_druid_child_type (GtkContainer *container)
{
	return nautilus_druid_page_get_type ();
}

static void
nautilus_druid_map (GtkWidget *widget)
{
	NautilusDruid *druid;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));

	druid = NAUTILUS_DRUID (widget);
	GTK_WIDGET_SET_FLAGS (druid, GTK_MAPPED);

	gtk_widget_map (druid->back);
	if (druid->_priv->show_finish)
		gtk_widget_map (druid->finish);
	else
		gtk_widget_map (druid->next);
	gtk_widget_map (druid->cancel);
	if (druid->_priv->current &&
	    GTK_WIDGET_VISIBLE (druid->_priv->current) &&
	    !GTK_WIDGET_MAPPED (druid->_priv->current)) {
		gtk_widget_map (GTK_WIDGET (druid->_priv->current));
	}
}

static void
nautilus_druid_unmap (GtkWidget *widget)
{
	NautilusDruid *druid;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));

	druid = NAUTILUS_DRUID (widget);
	GTK_WIDGET_UNSET_FLAGS (druid, GTK_MAPPED);

	gtk_widget_unmap (druid->back);
	if (druid->_priv->show_finish)
		gtk_widget_unmap (druid->finish);
	else
		gtk_widget_unmap (druid->next);
	gtk_widget_unmap (druid->cancel);
	if (druid->_priv->current &&
	    GTK_WIDGET_VISIBLE (druid->_priv->current) &&
	    GTK_WIDGET_MAPPED (druid->_priv->current))
		gtk_widget_unmap (GTK_WIDGET (druid->_priv->current));
}
static void
nautilus_druid_add (GtkContainer *widget,
		 GtkWidget *page)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));
	g_return_if_fail (page != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (page));

	nautilus_druid_append_page (NAUTILUS_DRUID (widget), NAUTILUS_DRUID_PAGE (page));
}
static void
nautilus_druid_remove (GtkContainer *widget,
		    GtkWidget *child)
{
	NautilusDruid *druid;
	GList *list;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));
	g_return_if_fail (child != NULL);

	druid = NAUTILUS_DRUID (widget);

	list = g_list_find (druid->_priv->children, child);
	/* Is it a page? */ 
	if (list != NULL) {
		/* If we are mapped and visible, we want to deal with changing the page. */
		if ((GTK_WIDGET_MAPPED (GTK_WIDGET (widget))) &&
		    (list->data == (gpointer) druid->_priv->current) &&
		    (list->next != NULL)) {
			nautilus_druid_set_page (druid, NAUTILUS_DRUID_PAGE (list->next->data));
		}
	}
	druid->_priv->children = g_list_remove (druid->_priv->children, child);
	gtk_widget_unparent (child);
}

static void
nautilus_druid_forall (GtkContainer *container,
		    gboolean      include_internals,
		    GtkCallback   callback,
		    gpointer      callback_data)
{
	NautilusDruid *druid;
	NautilusDruidPage *child;
	GList *children;

	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (container));
	g_return_if_fail (callback != NULL);

	druid = NAUTILUS_DRUID (container);

	children = druid->_priv->children;
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
nautilus_druid_draw (GtkWidget    *widget,
		  GdkRectangle *area)
{
	NautilusDruid *druid;
	GdkRectangle child_area;
	GtkWidget *child;
	GList *children;
  
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (widget));

	if (GTK_WIDGET_DRAWABLE (widget)) {
		druid = NAUTILUS_DRUID (widget);
		children = druid->_priv->children;

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
nautilus_druid_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
	NautilusDruid *druid;
	GtkWidget *child;
	GdkEventExpose child_event;
	GList *children;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DRUID (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		druid = NAUTILUS_DRUID (widget);
		child_event = *event;
		children = druid->_priv->children;

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
nautilus_druid_back_callback (GtkWidget *button, NautilusDruid *druid)
{
	GList *list;
	g_return_if_fail (druid->_priv->current != NULL);

	if (nautilus_druid_page_back (druid->_priv->current))
		return;

	/* Make sure that we have a next list item */
	list = g_list_find (druid->_priv->children, druid->_priv->current);
	g_return_if_fail (list->prev != NULL);
	nautilus_druid_set_page (druid, NAUTILUS_DRUID_PAGE (list->prev->data));
}
static void
nautilus_druid_next_callback (GtkWidget *button, NautilusDruid *druid)
{
	GList *list;
	g_return_if_fail (druid->_priv->current != NULL);

	if (druid->_priv->show_finish == FALSE) {
		if (nautilus_druid_page_next (druid->_priv->current))
			return;

		/* Make sure that we have a next list item */
		/* FIXME: we want to find the next VISIBLE one... */
		list = g_list_find (druid->_priv->children, druid->_priv->current);
		g_return_if_fail (list->next != NULL);
		nautilus_druid_set_page (druid, NAUTILUS_DRUID_PAGE (list->next->data));
	} else {
		nautilus_druid_page_finish (druid->_priv->current);
	}
}
static void
nautilus_druid_cancel_callback (GtkWidget *button, GtkWidget *druid)
{
     if (NAUTILUS_DRUID (druid)->_priv->current) {
	     if (nautilus_druid_page_cancel (NAUTILUS_DRUID (druid)->_priv->current))
		     return;

	     gtk_signal_emit (GTK_OBJECT (druid), druid_signals [CANCEL]);
     }
}

/* Public Functions */
GtkWidget *
nautilus_druid_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_druid_get_type ()));
}

/**
 * nautilus_druid_set_buttons_sensitive
 * @druid: A Druid.
 * @back_sensitive: The sensitivity of the back button.
 * @next_sensitive: The sensitivity of the next button.
 * @cancel_sensitive: The sensitivity of the cancel button.
 *
 * Description: Sets the sensitivity of the @druid's control-buttons.  If the
 * variables are TRUE, then they will be clickable.  This function is used
 * primarily by the actual NautilusDruidPage widgets.
 **/

void
nautilus_druid_set_buttons_sensitive (NautilusDruid *druid,
				   gboolean back_sensitive,
				   gboolean next_sensitive,
				   gboolean cancel_sensitive)
{
	g_return_if_fail (druid != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (druid));

	gtk_widget_set_sensitive (druid->back, back_sensitive);
	gtk_widget_set_sensitive (druid->next, next_sensitive);
	gtk_widget_set_sensitive (druid->cancel, cancel_sensitive);
}
/**
 * nautilus_druid_set_show_finish
 * @druid: A Druid widget.
 # @show_finish: If TRUE, then the "Cancel" button is changed to be "Finish"
 *
 * Description: Sets the text on the last button on the @druid.  If @show_finish
 * is TRUE, then the text becomes "Finish".  If @show_finish is FALSE, then the
 * text becomes "Cancel".
 **/
void
nautilus_druid_set_show_finish (NautilusDruid *druid,
			     gboolean show_finish)
{
	g_return_if_fail (druid != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (druid));

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
	druid->_priv->show_finish = show_finish;
}
/**
 * nautilus_druid_prepend_page:
 * @druid: A Druid widget.
 * @page: The page to be inserted.
 * 
 * Description: This will prepend a NautilusDruidPage into the internal list of
 * pages that the @druid has.
 **/
void
nautilus_druid_prepend_page (NautilusDruid *druid,
			  NautilusDruidPage *page)
{
	g_return_if_fail (druid != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (druid));
	g_return_if_fail (page != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (page));

	nautilus_druid_insert_page (druid, NULL, page);
}
/**
 * nautilus_druid_insert_page:
 * @druid: A Druid widget.
 * @back_page: The page prior to the page to be inserted.
 * @page: The page to insert.
 * 
 * Description: This will insert @page after @back_page into the list of
 * internal pages that the @druid has.  If @back_page is not present in the list
 * or NULL, @page will be prepended to the list.
 **/
void
nautilus_druid_insert_page (NautilusDruid *druid,
			 NautilusDruidPage *back_page,
			 NautilusDruidPage *page)
{
	GList *list;

	g_return_if_fail (druid != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (druid));
	g_return_if_fail (page != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (page));

	list = g_list_find (druid->_priv->children, back_page);
	if (list == NULL)
		druid->_priv->children = g_list_prepend (druid->_priv->children, page);
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
	if (druid->_priv->children->next == NULL)
		nautilus_druid_set_page (druid, page);
}

/**
 * nautilus_druid_append_page: 
 * @druid: A Druid widget.
 * @page: The page to be appended.
 * 
 * Description: This will append @page onto the end of the internal list.  
 **/
void nautilus_druid_append_page (NautilusDruid *druid,
			      NautilusDruidPage *page)
{
	GList *list;
	g_return_if_fail (druid != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (druid));
	g_return_if_fail (page != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (page));

	list = g_list_last (druid->_priv->children);
	if (list) {
		nautilus_druid_insert_page (druid, NAUTILUS_DRUID_PAGE (list->data), page);
	} else {
		nautilus_druid_insert_page (druid, NULL, page);
	}	
}
/**
 * nautilus_druid_set_page:
 * @druid: A Druid widget.
 * @page: The page to be brought to the foreground.
 * 
 * Description: This will make @page the currently showing page in the druid.
 * @page must already be in the druid.
 **/
void
nautilus_druid_set_page (NautilusDruid *druid,
		      NautilusDruidPage *page)
{
	GList *list;
	GtkWidget *old = NULL;
	g_return_if_fail (druid != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID (druid));
	g_return_if_fail (page != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE (page));

	if (druid->_priv->current == page)
	     return;
	list = g_list_find (druid->_priv->children, page);
	g_return_if_fail (list != NULL);

	if ((druid->_priv->current) && (GTK_WIDGET_VISIBLE (druid->_priv->current)) && (GTK_WIDGET_MAPPED (druid))) {
		old = GTK_WIDGET (druid->_priv->current);
	}
	druid->_priv->current = NAUTILUS_DRUID_PAGE (list->data);
	nautilus_druid_page_prepare (druid->_priv->current);
	if (GTK_WIDGET_VISIBLE (druid->_priv->current) && (GTK_WIDGET_MAPPED (druid))) {
		gtk_widget_map (GTK_WIDGET (druid->_priv->current));
	}
	if (old && GTK_WIDGET_MAPPED (old))
	  gtk_widget_unmap (old);
}
