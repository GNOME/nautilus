/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-view.c: Implementation of the object representing a data
   view, and its associated CORBA object for proxying requests into
   this object. */

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

typedef struct {
  POA_Nautilus_ViewFrame servant;
  gpointer gnome_object;

  NautilusView *view;
} impl_POA_Nautilus_ViewFrame;

static Nautilus_ViewWindow
impl_Nautilus_ViewFrame__get_main_window(impl_POA_Nautilus_ViewFrame *servant,
                                         CORBA_Environment *ev);
static void
impl_Nautilus_ViewFrame_request_location_change(impl_POA_Nautilus_ViewFrame * servant,
						Nautilus_NavigationRequestInfo * navinfo,
						CORBA_Environment * ev);

static void
impl_Nautilus_ViewFrame_request_selection_change(impl_POA_Nautilus_ViewFrame * servant,
						 Nautilus_SelectionRequestInfo * selinfo,
						 CORBA_Environment * ev);
static void
impl_Nautilus_ViewFrame_request_status_change(impl_POA_Nautilus_ViewFrame * servant,
                                              Nautilus_StatusRequestInfo * statinfo,
                                              CORBA_Environment * ev);

POA_Nautilus_ViewFrame__epv impl_Nautilus_ViewFrame_epv =
{
   NULL,			/* _private */
   (void(*))&impl_Nautilus_ViewFrame__get_main_window,
   (void(*))&impl_Nautilus_ViewFrame_request_location_change,
   (void(*))&impl_Nautilus_ViewFrame_request_selection_change,
   (void(*))&impl_Nautilus_ViewFrame_request_status_change
};

static PortableServer_ServantBase__epv base_epv = { NULL};

static POA_Nautilus_ViewFrame__vepv impl_Nautilus_ViewFrame_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_ViewFrame_epv
};

static void
impl_Nautilus_ViewFrame__destroy(GnomeObject *obj, impl_POA_Nautilus_ViewFrame *servant)
{
   PortableServer_ObjectId *objid;
   CORBA_Environment ev;
   NautilusViewClass *klass;
   void (*servant_destroy_func)(PortableServer_Servant, CORBA_Environment *);

   klass = NAUTILUS_VIEW_CLASS(GTK_OBJECT(servant->view)->klass);

   CORBA_exception_init(&ev);

   objid = PortableServer_POA_servant_to_id(bonobo_poa(), servant, &ev);
   PortableServer_POA_deactivate_object(bonobo_poa(), objid, &ev);
   CORBA_free(objid);
   obj->servant = NULL;

   servant_destroy_func = klass->servant_destroy_func;
   servant_destroy_func((PortableServer_Servant) servant, &ev);
   g_free(servant);
   CORBA_exception_free(&ev);
}

static GnomeObject *
impl_Nautilus_ViewFrame__create(NautilusView *view, CORBA_Environment * ev)
{
   GnomeObject *retval;
   impl_POA_Nautilus_ViewFrame *newservant;
   NautilusViewClass *klass;
   void (*servant_init_func)(PortableServer_Servant, CORBA_Environment *);

   klass = NAUTILUS_VIEW_CLASS(GTK_OBJECT(view)->klass);
   newservant = g_new0(impl_POA_Nautilus_ViewFrame, 1);
   newservant->servant.vepv = klass->vepv;
   if(!newservant->servant.vepv->GNOME_Unknown_epv)
     newservant->servant.vepv->GNOME_Unknown_epv = gnome_object_get_epv();
   newservant->view = view;
   servant_init_func = klass->servant_init_func;
   servant_init_func((PortableServer_Servant) newservant, ev);

   retval = gnome_object_new_from_servant(newservant);

   gtk_signal_connect(GTK_OBJECT(retval), "destroy", GTK_SIGNAL_FUNC(impl_Nautilus_ViewFrame__destroy), newservant);

   return retval;
}

static Nautilus_ViewWindow
impl_Nautilus_ViewFrame__get_main_window(impl_POA_Nautilus_ViewFrame *servant,
                                         CORBA_Environment *ev)
{
  return CORBA_Object_duplicate(gnome_object_corba_objref(NAUTILUS_WINDOW(servant->view->main_window)->ntl_viewwindow), ev);
}

static void
impl_Nautilus_ViewFrame_request_location_change(impl_POA_Nautilus_ViewFrame * servant,
						Nautilus_NavigationRequestInfo * navinfo,
						CORBA_Environment * ev)
{
  nautilus_view_request_location_change(servant->view, navinfo);
}

static void
impl_Nautilus_ViewFrame_request_selection_change(impl_POA_Nautilus_ViewFrame * servant,
						 Nautilus_SelectionRequestInfo * selinfo,
						 CORBA_Environment * ev)
{
  nautilus_view_request_selection_change(servant->view, selinfo);
}

static void
impl_Nautilus_ViewFrame_request_status_change(impl_POA_Nautilus_ViewFrame * servant,
                                              Nautilus_StatusRequestInfo * statinfo,
                                              CORBA_Environment * ev)
{
  nautilus_view_request_status_change(servant->view, statinfo);
}

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
static void nautilus_view_notify_location_change(NautilusView *view,
						 Nautilus_NavigationInfo *nav_context);
static void nautilus_view_notify_selection_change(NautilusView *view,
						  Nautilus_SelectionInfo *nav_context);
static void nautilus_view_load_state(NautilusView *view, const char *config_path);
static void nautilus_view_save_state(NautilusView *view, const char *config_path);
static void nautilus_view_show_properties(NautilusView *view);

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

#if 0
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
#endif

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

  klass->servant_init_func = POA_Nautilus_ViewFrame__init;
  klass->servant_destroy_func = POA_Nautilus_ViewFrame__fini;
  klass->vepv = &impl_Nautilus_ViewFrame_vepv;

  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
  klass->view_constructed = nautilus_view_constructed;
  klass->notify_location_change = nautilus_view_notify_location_change;
  klass->notify_selection_change = nautilus_view_notify_selection_change;
  klass->load_state = nautilus_view_load_state;
  klass->save_state = nautilus_view_save_state;
  klass->show_properties = nautilus_view_show_properties;

  i = 0;
  klass->view_signals[i++] = gtk_signal_new("notify_location_change",
					    GTK_RUN_LAST,
					    object_class->type,
					    GTK_SIGNAL_OFFSET (NautilusViewClass, notify_location_change),
					    gtk_marshal_NONE__BOXED,
					    GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
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
  klass->view_signals[i++] = gtk_signal_new("notify_selection_change",
					    GTK_RUN_LAST,
					    object_class->type,
					    GTK_SIGNAL_OFFSET (NautilusViewClass, notify_selection_change),
					    gtk_marshal_NONE__BOXED,
					    GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
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
  CORBA_Environment ev;
  GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);

  CORBA_exception_init(&ev);
  view->view_frame = impl_Nautilus_ViewFrame__create(view, &ev);
  CORBA_exception_free(&ev);
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
				      Nautilus_NavigationRequestInfo *loc)
{
  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW (view));
  g_return_if_fail (NAUTILUS_VIEW (view)->main_window != NULL);

  nautilus_window_request_location_change(NAUTILUS_WINDOW(view->main_window), loc, GTK_WIDGET(view));
}

void
nautilus_view_request_selection_change (NautilusView              *view,
					Nautilus_SelectionRequestInfo *loc)
{
  nautilus_window_request_selection_change(NAUTILUS_WINDOW(view->main_window), loc, GTK_WIDGET(view));  
}

void
nautilus_view_request_status_change    (NautilusView              *view,
                                        Nautilus_StatusRequestInfo *loc)
{
  nautilus_window_request_status_change(NAUTILUS_WINDOW(view->main_window), loc, GTK_WIDGET(view));  
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

static void
nautilus_view_activate_uri(GnomeControlFrame *frame, const char *uri, gboolean relative, NautilusView *view)
{
  Nautilus_NavigationRequestInfo nri;
  g_assert(!relative);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = (char *)uri;
  nautilus_view_request_location_change(view, &nri);
}

void
nautilus_view_load_client(NautilusView *view, const char *iid)
{
  GnomeControlFrame *frame;
  CORBA_Environment ev;

  g_return_if_fail(iid);

  if(view->client)
    {
      g_free(view->iid); view->iid = NULL;
      gtk_container_remove(GTK_CONTAINER(view), view->client); view->client = NULL;
    }

  view->client = gnome_bonobo_widget_new_subdoc((char *)iid, NAUTILUS_WINDOW(view->main_window)->uih);
  g_return_if_fail(view->client);
  CORBA_exception_init(&ev);
  CORBA_Object_release(view->view_client, &ev);
  frame = gnome_bonobo_widget_get_control_frame(GNOME_BONOBO_WIDGET(view->client));
  gnome_object_add_interface(GNOME_OBJECT(frame), view->view_frame);
  view->view_client = GNOME_Unknown_query_interface(gnome_control_frame_get_control(GNOME_CONTROL_FRAME(frame)),
						    "IDL:Nautilus/View:1.0", &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    view->view_client = CORBA_OBJECT_NIL;

  view->iid = g_strdup(iid);

  gtk_signal_connect(GTK_OBJECT(frame), "activate_uri", GTK_SIGNAL_FUNC(nautilus_view_activate_uri), view);
  gtk_widget_show(view->client);
  gtk_container_add(GTK_CONTAINER(view), view->client);
  CORBA_exception_free(&ev);
}

static void
nautilus_view_notify_location_change(NautilusView *view,
				     Nautilus_NavigationInfo *nav_context)
{
  CORBA_Environment ev;

  CORBA_exception_init(&ev);
  g_return_if_fail(!CORBA_Object_is_nil(view->view_client, &ev));

  Nautilus_View_notify_location_change(view->view_client, nav_context, &ev);

  CORBA_exception_free(&ev);
}

static void
nautilus_view_notify_selection_change(NautilusView *view,
				      Nautilus_SelectionInfo *nav_context)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);
  g_return_if_fail(!CORBA_Object_is_nil(view->view_client, &ev));

  Nautilus_View_notify_selection_change(view->view_client, nav_context, &ev);

  CORBA_exception_free(&ev);
}

static void
nautilus_view_load_state(NautilusView *view, const char *config_path)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);
  g_return_if_fail(!CORBA_Object_is_nil(view->view_client, &ev));

  Nautilus_View_load_state(view->view_client, (char *)config_path, &ev);

  CORBA_exception_free(&ev);
}

static void
nautilus_view_save_state(NautilusView *view, const char *config_path)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);
  g_return_if_fail(!CORBA_Object_is_nil(view->view_client, &ev));

  Nautilus_View_save_state(view->view_client, (char *)config_path, &ev);

  CORBA_exception_free(&ev);
}

static void
nautilus_view_show_properties(NautilusView *view)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);
  g_return_if_fail(!CORBA_Object_is_nil(view->view_client, &ev));

  Nautilus_View_show_properties(view->view_client, &ev);

  CORBA_exception_free(&ev);
}

