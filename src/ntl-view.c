/* nautilus-view.c
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

#include <gtk/gtksignal.h>
#include <gtk/gtk.h>
#include "ntl-view.h"
#include "ntl-window.h"

enum {
	NOTIFY_LOCATION_CHANGE,
	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_MAIN_WINDOW
};

static void nautilus_view_init       (NautilusView      *view);
static void nautilus_view_constructed(NautilusView      *view);
static void nautilus_view_class_init (NautilusViewClass *klass);
static void nautilus_view_set_arg (GtkObject      *object,
				    GtkArg         *arg,
				    guint	      arg_id);
static void nautilus_view_get_arg (GtkObject      *object,
				    GtkArg         *arg,
				    guint	      arg_id);
static void nautilus_view_size_request (GtkWidget        *widget,
					GtkRequisition   *requisition);
static void nautilus_view_size_allocate (GtkWidget        *widget,
					  GtkAllocation    *allocation);


GtkType
nautilus_view_get_type (void)
{
	static GtkType view_type = 0;

	if (!view_type)	{
		const GtkTypeInfo view_info = {
			"NautilusView",
			sizeof (NautilusView),
			sizeof (NautilusViewClass),
			(GtkClassInitFunc) nautilus_view_class_init,
			(GtkObjectInitFunc) nautilus_view_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		view_type = gtk_type_unique (gtk_bin_get_type(), &view_info);
	}
	
	return view_type;
}

typedef void (*GtkSignal_NONE__BOXED_OBJECT_BOXED) (GtkObject * object,
						    gpointer arg1,
						    GtkObject *arg2,
						    gpointer arg3,
						    gpointer user_data);
static void
gtk_marshal_NONE__BOXED_OBJECT_BOXED (GtkObject * object,
				      GtkSignalFunc func,
				      gpointer func_data,
				      GtkArg * args)
{
	GtkSignal_NONE__BOXED_OBJECT_BOXED rfunc;
	rfunc = (GtkSignal_NONE__BOXED_OBJECT_BOXED) func;
	(*rfunc) (object,
		  GTK_VALUE_BOXED (args[0]),
		  GTK_VALUE_OBJECT (args[1]),
		  GTK_VALUE_BOXED (args[2]),
		  func_data);
}

static void
nautilus_view_class_init (NautilusViewClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  int i;

  object_class = (GtkObjectClass*) klass;
  object_class->set_arg = nautilus_view_set_arg;
  object_class->get_arg = nautilus_view_get_arg;

  widget_class = (GtkWidgetClass*) klass;
  widget_class->size_request = nautilus_view_size_request;
  widget_class->size_allocate = nautilus_view_size_allocate;

  klass->notify_location_change = NULL;

  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
  klass->view_constructed = nautilus_view_constructed;

  i = 0;
  klass->view_signals[i++] = gtk_signal_new("notify_location_change",
					    GTK_RUN_LAST,
					    object_class->type,
					    GTK_SIGNAL_OFFSET (NautilusViewClass, notify_location_change),
					    gtk_marshal_NONE__BOXED_OBJECT_BOXED,
					    GTK_TYPE_NONE, 3, GTK_TYPE_BOXED, GTK_TYPE_OBJECT, GTK_TYPE_BOXED);
  klass->view_signals[i++] = gtk_signal_new("load_state",
					    GTK_RUN_LAST,
					    object_class->type,
					    GTK_SIGNAL_OFFSET (NautilusViewClass, load_state),
					    gtk_marshal_NONE__STRING,
					    GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
  klass->view_signals[i++] = gtk_signal_new("save_state",
					    GTK_RUN_LAST,
					    object_class->type,
					    GTK_SIGNAL_OFFSET (NautilusViewClass, save_state),
					    gtk_marshal_NONE__STRING,
					    GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
  klass->view_signals[i++] = gtk_signal_new("show_properties",
					    GTK_RUN_LAST,
					    object_class->type,
					    GTK_SIGNAL_OFFSET (NautilusViewClass, show_properties),
					    gtk_marshal_NONE__NONE,
					    GTK_TYPE_NONE, 0);
  gtk_object_class_add_signals (object_class, klass->view_signals, i);

  gtk_object_add_arg_type ("NautilusView::main_window",
			   GTK_TYPE_OBJECT,
			   GTK_ARG_READWRITE|GTK_ARG_CONSTRUCT,
			   ARG_MAIN_WINDOW);
  klass->num_construct_args++;
}

static void
nautilus_view_set_arg (GtkObject      *object,
		       GtkArg         *arg,
		       guint	       arg_id)
{
  NautilusView *view;

  view = NAUTILUS_VIEW(object);
  switch(arg_id) {
  case ARG_MAIN_WINDOW:
    view->main_window = GTK_WIDGET(GTK_VALUE_OBJECT(*arg));
    nautilus_view_construct_arg_set(view);
    break;
  }
}

static void
nautilus_view_get_arg (GtkObject      *object,
		       GtkArg         *arg,
		       guint	        arg_id)
{
  switch(arg_id) {
  case ARG_MAIN_WINDOW:
    GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(NAUTILUS_VIEW(object)->main_window);
    break;
  }
}

static void
nautilus_view_init (NautilusView *view)
{
  GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);
}

static void
nautilus_view_constructed(NautilusView *view)
{
}

void
nautilus_view_construct_arg_set(NautilusView *view)
{
  guint nca;
  NautilusViewClass *klass;

  klass = NAUTILUS_VIEW_CLASS(((GtkObject *)view)->klass);
  nca = klass->num_construct_args;
  if(view->construct_arg_count >= nca)
    return;

  view->construct_arg_count++;
  if((view->construct_arg_count >= nca)
     && klass->view_constructed)
    klass->view_constructed(view);
}

void
nautilus_view_request_location_change(NautilusView *view,
				       NautilusLocationReference loc)
{
  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW (view));
  g_return_if_fail (NAUTILUS_VIEW (view)->main_window != NULL);

  nautilus_window_request_location_change(NAUTILUS_WINDOW(view->main_window), loc, GTK_WIDGET(view));
}

static void
nautilus_view_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin;

  bin = GTK_BIN (widget);

  requisition->width = 0;
  requisition->height = 0;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
nautilus_view_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  widget->allocation = child_allocation = *allocation;
  bin = GTK_BIN (widget);

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    gtk_widget_size_allocate (bin->child, &child_allocation);
}

void
nautilus_view_load_client(NautilusView              *view,
			  const char *               iid)
{
  if(view->client)
    {
      g_free(view->iid); view->iid = NULL;
      gtk_container_remove(GTK_CONTAINER(view), view->client); view->client = NULL;
    }

  view->client = gnome_bonobo_widget_new_control((char *)iid);
  g_return_if_fail(view->client);
  view->iid = g_strdup(iid);
  gtk_widget_show(view->client);
  gtk_container_add(GTK_CONTAINER(view), view->client);
}
