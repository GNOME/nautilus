/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* ntl-view-frame.c: Implementation for object that represents a
   nautilus view implementation. */

#include "ntl-view-frame.h"

#include <gtk/gtk.h>
#include "libnautilus.h"

enum {
  NOTIFY_LOCATION_CHANGE,
  NOTIFY_SELECTION_CHANGE,
  LOAD_STATE,
  SAVE_STATE,
  SHOW_PROPERTIES,
  STOP_LOCATION_CHANGE,
  LAST_SIGNAL
};

static guint nautilus_view_frame_signals[LAST_SIGNAL];

typedef struct {
  POA_Nautilus_View servant;
  gpointer gnome_object;

  NautilusViewFrame *view;
} impl_POA_Nautilus_View;

static void
impl_Nautilus_View_save_state(impl_POA_Nautilus_View * servant,
			      CORBA_char * config_path,
			      CORBA_Environment * ev);

static void
impl_Nautilus_View_load_state(impl_POA_Nautilus_View * servant,
			      CORBA_char * config_path,
			      CORBA_Environment * ev);

static void
impl_Nautilus_View_notify_location_change(impl_POA_Nautilus_View * servant,
					  Nautilus_NavigationInfo * navinfo,
					  CORBA_Environment * ev);

static void
impl_Nautilus_View_show_properties(impl_POA_Nautilus_View * servant,
				   CORBA_Environment * ev);

static void
impl_Nautilus_View_notify_selection_change(impl_POA_Nautilus_View * servant,
					   Nautilus_SelectionInfo * selinfo,
					   CORBA_Environment * ev);

static void
impl_Nautilus_View_stop_location_change(impl_POA_Nautilus_View * servant,
					CORBA_Environment * ev);

POA_Nautilus_View__epv libnautilus_Nautilus_View_epv =
{
  NULL,			/* _private */
  (gpointer) & impl_Nautilus_View_save_state,
  (gpointer) & impl_Nautilus_View_load_state,
  (gpointer) & impl_Nautilus_View_notify_location_change,
  (gpointer) & impl_Nautilus_View_show_properties,
  (gpointer) & impl_Nautilus_View_notify_selection_change,
  (gpointer) & impl_Nautilus_View_stop_location_change
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_View__vepv impl_Nautilus_View_vepv =
{
  &base_epv,
  NULL,
  &libnautilus_Nautilus_View_epv
};

static void
impl_Nautilus_View_save_state(impl_POA_Nautilus_View * servant,
			      CORBA_char * config_path,
			      CORBA_Environment * ev)
{
  gtk_signal_emit(GTK_OBJECT(servant->view), nautilus_view_frame_signals[SAVE_STATE], config_path);
}

static void
impl_Nautilus_View_load_state(impl_POA_Nautilus_View * servant,
			      CORBA_char * config_path,
			      CORBA_Environment * ev)
{
  gtk_signal_emit(GTK_OBJECT(servant->view), nautilus_view_frame_signals[LOAD_STATE], config_path);
}

static void
impl_Nautilus_View_notify_location_change(impl_POA_Nautilus_View * servant,
					  Nautilus_NavigationInfo * navinfo,
					  CORBA_Environment * ev)
{
  gtk_signal_emit(GTK_OBJECT(servant->view), nautilus_view_frame_signals[NOTIFY_LOCATION_CHANGE], navinfo);
}

static void
impl_Nautilus_View_show_properties(impl_POA_Nautilus_View * servant,
				   CORBA_Environment * ev)
{
  gtk_signal_emit(GTK_OBJECT(servant->view), nautilus_view_frame_signals[SHOW_PROPERTIES]);
}

static void
impl_Nautilus_View_notify_selection_change(impl_POA_Nautilus_View * servant,
					   Nautilus_SelectionInfo * selinfo,
					   CORBA_Environment * ev)
{
  gtk_signal_emit(GTK_OBJECT(servant->view), nautilus_view_frame_signals[NOTIFY_SELECTION_CHANGE], selinfo);
}

static void
impl_Nautilus_View_stop_location_change(impl_POA_Nautilus_View * servant,
					CORBA_Environment * ev)
{
  gtk_signal_emit(GTK_OBJECT(servant->view), nautilus_view_frame_signals[STOP_LOCATION_CHANGE]);
}


static void
impl_Nautilus_View__destroy(GnomeObject *obj, impl_POA_Nautilus_View *servant)
{
  PortableServer_ObjectId *objid;
  CORBA_Environment ev;
  void (*servant_destroy_func)(PortableServer_Servant servant, CORBA_Environment *ev);

  CORBA_exception_init(&ev);

  servant_destroy_func = NAUTILUS_VIEW_FRAME_CLASS(GTK_OBJECT(servant->view)->klass)->servant_destroy_func;
  objid = PortableServer_POA_servant_to_id(bonobo_poa(), servant, &ev);
  PortableServer_POA_deactivate_object(bonobo_poa(), objid, &ev);
  CORBA_free(objid);
  obj->servant = NULL;

  servant_destroy_func((PortableServer_Servant) servant, &ev);
  g_free(servant);
  CORBA_exception_free(&ev);
}

static GnomeObject *
impl_Nautilus_View__create(NautilusViewFrame *view, CORBA_Environment * ev)
{
  GnomeObject *retval;
  impl_POA_Nautilus_View *newservant;
  void (*servant_init_func)(PortableServer_Servant servant, CORBA_Environment *ev);
  NautilusViewFrameClass *view_class = NAUTILUS_VIEW_FRAME_CLASS(GTK_OBJECT(view)->klass);

  servant_init_func = view_class->servant_init_func;
  newservant = g_new0(impl_POA_Nautilus_View, 1);
  newservant->servant.vepv = view_class->vepv;
  if(!newservant->servant.vepv->GNOME_Unknown_epv)
    newservant->servant.vepv->GNOME_Unknown_epv = gnome_object_get_epv();
  servant_init_func((PortableServer_Servant) newservant, ev);

  newservant->view = view;

  retval = gnome_object_new_from_servant(newservant);

  gtk_signal_connect(GTK_OBJECT(retval), "destroy", GTK_SIGNAL_FUNC(impl_Nautilus_View__destroy), newservant);

  return retval;
}

static void nautilus_view_frame_init       (NautilusViewFrame      *view);
static void nautilus_view_frame_destroy       (NautilusViewFrame      *view);
static void nautilus_view_frame_class_init (NautilusViewFrameClass *klass);
static void nautilus_view_frame_set_arg (GtkObject      *object,
					  GtkArg         *arg,
					  guint	      arg_id);
static void nautilus_view_frame_get_arg (GtkObject      *object,
					  GtkArg         *arg,
					  guint	      arg_id);
static void nautilus_view_frame_size_request (GtkWidget        *widget,
					       GtkRequisition   *requisition);
static void nautilus_view_frame_size_allocate (GtkWidget        *widget,
						GtkAllocation    *allocation);

GtkType
nautilus_view_frame_get_type (void)
{
  static GtkType view_frame_type = 0;

  if (!view_frame_type)
    {
      const GtkTypeInfo view_frame_info =
      {
	"NautilusViewFrame",
	sizeof (NautilusViewFrame),
	sizeof (NautilusViewFrameClass),
	(GtkClassInitFunc) nautilus_view_frame_class_init,
	(GtkObjectInitFunc) nautilus_view_frame_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      view_frame_type = gtk_type_unique (gtk_bin_get_type(), &view_frame_info);
    }
	
  return view_frame_type;
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
nautilus_view_frame_class_init (NautilusViewFrameClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) klass;
  object_class->destroy = (void (*)(GtkObject*))nautilus_view_frame_destroy;
  object_class->set_arg = nautilus_view_frame_set_arg;
  object_class->get_arg = nautilus_view_frame_get_arg;

  widget_class = (GtkWidgetClass*) klass;
  widget_class->size_request = nautilus_view_frame_size_request;
  widget_class->size_allocate = nautilus_view_frame_size_allocate;

  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
  klass->servant_init_func = POA_Nautilus_View__init;
  klass->servant_destroy_func = POA_Nautilus_View__fini;
  klass->vepv = &impl_Nautilus_View_vepv;

  nautilus_view_frame_signals[NOTIFY_LOCATION_CHANGE] =
    gtk_signal_new("notify_location_change",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusViewFrameClass, notify_location_change),
		   gtk_marshal_NONE__BOXED,
		   GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  nautilus_view_frame_signals[NOTIFY_SELECTION_CHANGE] = 
    gtk_signal_new("notify_selection_change",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusViewFrameClass, notify_selection_change),
		   gtk_marshal_NONE__BOXED,
		   GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  nautilus_view_frame_signals[LOAD_STATE] = 
    gtk_signal_new("load_state",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusViewFrameClass, load_state),
		   gtk_marshal_NONE__STRING,
		   GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
  nautilus_view_frame_signals[SAVE_STATE] = 
    gtk_signal_new("save_state",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusViewFrameClass, save_state),
		   gtk_marshal_NONE__STRING,
		   GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
  nautilus_view_frame_signals[SHOW_PROPERTIES] = 
    gtk_signal_new("show_properties",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusViewFrameClass, show_properties),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  nautilus_view_frame_signals[STOP_LOCATION_CHANGE] = 
    gtk_signal_new("stop_location_change",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusViewFrameClass, stop_location_change),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  gtk_object_class_add_signals (object_class, nautilus_view_frame_signals, LAST_SIGNAL);
}

static void
nautilus_view_frame_set_arg (GtkObject      *object,
			      GtkArg         *arg,
			      guint	       arg_id)
{
}

static void
nautilus_view_frame_get_arg (GtkObject      *object,
			      GtkArg         *arg,
			      guint	        arg_id)
{
}

static void
nautilus_view_frame_init (NautilusViewFrame *view)
{
  CORBA_Environment ev;
  GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);

  view->control = GNOME_OBJECT(gnome_control_new(GTK_WIDGET(view)));

  CORBA_exception_init(&ev);
  view->view_server = impl_Nautilus_View__create(view, &ev);
  gnome_object_add_interface(view->control, view->view_server);
  CORBA_exception_free(&ev);
}

static void
nautilus_view_frame_destroy (NautilusViewFrame *view)
{
  NautilusViewFrameClass *klass = NAUTILUS_VIEW_FRAME_CLASS(GTK_OBJECT(view)->klass);

  gnome_object_destroy(view->view_server);
  gnome_object_destroy(view->control);

  if(((GtkObjectClass *)klass->parent_class)->destroy)
    ((GtkObjectClass *)klass->parent_class)->destroy((GtkObject *)view);
}

void
nautilus_view_frame_request_location_change(NautilusViewFrame *view,
					     Nautilus_NavigationRequestInfo *loc)
{
  CORBA_Environment ev;

  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

  CORBA_exception_init(&ev);
  if(CORBA_Object_is_nil(view->view_frame, &ev))
    view->view_frame = GNOME_Unknown_query_interface(gnome_control_get_control_frame(GNOME_CONTROL(view->control)),
						     "IDL:Nautilus/ViewFrame:1.0", &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    view->view_frame = CORBA_OBJECT_NIL;
  if(CORBA_Object_is_nil(view->view_frame, &ev))
    return;

  Nautilus_ViewFrame_request_location_change(view->view_frame, loc, &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    {
      CORBA_Object_release(view->view_frame, &ev);
      view->view_frame = CORBA_OBJECT_NIL;
    }
  
  CORBA_exception_free(&ev);
}

void
nautilus_view_frame_request_selection_change (NautilusViewFrame              *view,
					       Nautilus_SelectionRequestInfo *loc)
{
  CORBA_Environment ev;

  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

  CORBA_exception_init(&ev);
  if(CORBA_Object_is_nil(view->view_frame, &ev))
    view->view_frame = GNOME_Unknown_query_interface(gnome_control_get_control_frame(GNOME_CONTROL(view->control)),
						     "IDL:Nautilus/ViewFrame:1.0", &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    view->view_frame = CORBA_OBJECT_NIL;
  if(CORBA_Object_is_nil(view->view_frame, &ev))
    return;

  Nautilus_ViewFrame_request_selection_change(view->view_frame, loc, &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    {
      CORBA_Object_release(view->view_frame, &ev);
      view->view_frame = CORBA_OBJECT_NIL;
    }
  
  CORBA_exception_free(&ev);
}

void
nautilus_view_frame_request_status_change    (NautilusViewFrame        *view,
					       Nautilus_StatusRequestInfo *loc)
{
  CORBA_Environment ev;

  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

  CORBA_exception_init(&ev);
  if(CORBA_Object_is_nil(view->view_frame, &ev))
    view->view_frame = GNOME_Unknown_query_interface(gnome_control_get_control_frame(GNOME_CONTROL(view->control)),
						     "IDL:Nautilus/ViewFrame:1.0", &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    view->view_frame = CORBA_OBJECT_NIL;
  if(CORBA_Object_is_nil(view->view_frame, &ev))
    return;

  Nautilus_ViewFrame_request_status_change(view->view_frame, loc, &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    {
      CORBA_Object_release(view->view_frame, &ev);
      view->view_frame = CORBA_OBJECT_NIL;
    }
  
  CORBA_exception_free(&ev);
}

void
nautilus_view_frame_request_progress_change(NautilusViewFrame        *view,
					     Nautilus_ProgressRequestInfo *loc)
{
  CORBA_Environment ev;

  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

  CORBA_exception_init(&ev);
  if(CORBA_Object_is_nil(view->view_frame, &ev))
    view->view_frame = GNOME_Unknown_query_interface(gnome_control_get_control_frame(GNOME_CONTROL(view->control)),
						     "IDL:Nautilus/ViewFrame:1.0", &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    view->view_frame = CORBA_OBJECT_NIL;
  if(CORBA_Object_is_nil(view->view_frame, &ev))
    return;

  Nautilus_ViewFrame_request_progress_change(view->view_frame, loc, &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    {
      CORBA_Object_release(view->view_frame, &ev);
      view->view_frame = CORBA_OBJECT_NIL;
    }
  
  CORBA_exception_free(&ev);
}

static void
nautilus_view_frame_size_request (GtkWidget      *widget,
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
nautilus_view_frame_size_allocate (GtkWidget     *widget,
				    GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  widget->allocation = child_allocation = *allocation;
  bin = GTK_BIN (widget);

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    gtk_widget_size_allocate (bin->child, &child_allocation);
}

GnomeObject *
nautilus_view_frame_get_gnome_object    (NautilusViewFrame        *view)
{
  return view->control;
}
