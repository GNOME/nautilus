/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus clients.
 *
 *  Copyright (C) 1999 Red Hat, Inc.
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
/* ntl-view-client.c: Implementation for object that represents a nautilus view implementation. */

#include <gtk/gtk.h>
#include "libnautilus.h"

typedef struct {
  POA_Nautilus_View servant;
  gpointer gnome_object;

  NautilusViewClient *view;
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

POA_Nautilus_View__epv libnautilus_Nautilus_View_epv =
{
  NULL,			/* _private */
  (gpointer) & impl_Nautilus_View_save_state,
  (gpointer) & impl_Nautilus_View_load_state,
  (gpointer) & impl_Nautilus_View_notify_location_change,
  (gpointer) & impl_Nautilus_View_show_properties,
  (gpointer) & impl_Nautilus_View_notify_selection_change
};

static PortableServer_ServantBase__epv base_epv = { NULL};

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
  gtk_signal_emit_by_name(GTK_OBJECT(servant->view), "save_state", config_path);
}

static void
impl_Nautilus_View_load_state(impl_POA_Nautilus_View * servant,
			      CORBA_char * config_path,
			      CORBA_Environment * ev)
{
  gtk_signal_emit_by_name(GTK_OBJECT(servant->view), "load_state", config_path);
}

static void
impl_Nautilus_View_notify_location_change(impl_POA_Nautilus_View * servant,
					  Nautilus_NavigationInfo * navinfo,
					  CORBA_Environment * ev)
{
  gtk_signal_emit_by_name(GTK_OBJECT(servant->view), "notify_location_change", navinfo);
}

static void
impl_Nautilus_View_show_properties(impl_POA_Nautilus_View * servant,
				   CORBA_Environment * ev)
{
  gtk_signal_emit_by_name(GTK_OBJECT(servant->view), "show_properties");
}

static void
impl_Nautilus_View_notify_selection_change(impl_POA_Nautilus_View * servant,
					   Nautilus_SelectionInfo * selinfo,
					   CORBA_Environment * ev)
{
  gtk_signal_emit_by_name(GTK_OBJECT(servant->view), "notify_selection_change", selinfo);
}


static void
impl_Nautilus_View__destroy(GnomeObject *obj, impl_POA_Nautilus_View *servant)
{
  PortableServer_ObjectId *objid;
  CORBA_Environment ev;
  void (*servant_destroy_func)(PortableServer_Servant servant, CORBA_Environment *ev);

  CORBA_exception_init(&ev);

  servant_destroy_func = NAUTILUS_VIEW_CLIENT_CLASS(GTK_OBJECT(servant->view)->klass)->servant_destroy_func;
  objid = PortableServer_POA_servant_to_id(bonobo_poa(), servant, &ev);
  PortableServer_POA_deactivate_object(bonobo_poa(), objid, &ev);
  CORBA_free(objid);
  obj->servant = NULL;

  servant_destroy_func((PortableServer_Servant) servant, &ev);
  g_free(servant);
  CORBA_exception_free(&ev);
}

static GnomeObject *
impl_Nautilus_View__create(NautilusViewClient *view, CORBA_Environment * ev)
{
  GnomeObject *retval;
  impl_POA_Nautilus_View *newservant;
  void (*servant_init_func)(PortableServer_Servant servant, CORBA_Environment *ev);
  NautilusViewClientClass *view_class = NAUTILUS_VIEW_CLIENT_CLASS(GTK_OBJECT(view)->klass);

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

static void nautilus_view_client_init       (NautilusViewClient      *view);
static void nautilus_view_client_destroy       (NautilusViewClient      *view);
static void nautilus_view_client_class_init (NautilusViewClientClass *klass);
static void nautilus_view_client_set_arg (GtkObject      *object,
					  GtkArg         *arg,
					  guint	      arg_id);
static void nautilus_view_client_get_arg (GtkObject      *object,
					  GtkArg         *arg,
					  guint	      arg_id);
static void nautilus_view_client_size_request (GtkWidget        *widget,
					       GtkRequisition   *requisition);
static void nautilus_view_client_size_allocate (GtkWidget        *widget,
						GtkAllocation    *allocation);

GtkType
nautilus_view_client_get_type (void)
{
  static GtkType view_client_type = 0;

  if (!view_client_type)
    {
      const GtkTypeInfo view_client_info =
      {
	"NautilusViewClient",
	sizeof (NautilusViewClient),
	sizeof (NautilusViewClientClass),
	(GtkClassInitFunc) nautilus_view_client_class_init,
	(GtkObjectInitFunc) nautilus_view_client_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      view_client_type = gtk_type_unique (gtk_bin_get_type(), &view_client_info);
    }
	
  return view_client_type;
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
nautilus_view_client_class_init (NautilusViewClientClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  int i;

  object_class = (GtkObjectClass*) klass;
  object_class->destroy = (void (*)(GtkObject*))nautilus_view_client_destroy;
  object_class->set_arg = nautilus_view_client_set_arg;
  object_class->get_arg = nautilus_view_client_get_arg;

  widget_class = (GtkWidgetClass*) klass;
  widget_class->size_request = nautilus_view_client_size_request;
  widget_class->size_allocate = nautilus_view_client_size_allocate;

  klass->notify_location_change = NULL;

  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
  klass->servant_init_func = POA_Nautilus_View__init;
  klass->servant_destroy_func = POA_Nautilus_View__fini;
  klass->vepv = &impl_Nautilus_View_vepv;

  i = 0;
  klass->view_client_signals[i++] = gtk_signal_new("notify_location_change",
						   GTK_RUN_LAST,
						   object_class->type,
						   GTK_SIGNAL_OFFSET (NautilusViewClientClass, notify_location_change),
						   gtk_marshal_NONE__BOXED,
						   GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  klass->view_client_signals[i++] = gtk_signal_new("load_state",
						   GTK_RUN_LAST,
						   object_class->type,
						   GTK_SIGNAL_OFFSET (NautilusViewClientClass, load_state),
						   gtk_marshal_NONE__STRING,
						   GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
  klass->view_client_signals[i++] = gtk_signal_new("save_state",
						   GTK_RUN_LAST,
						   object_class->type,
						   GTK_SIGNAL_OFFSET (NautilusViewClientClass, save_state),
						   gtk_marshal_NONE__STRING,
						   GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
  klass->view_client_signals[i++] = gtk_signal_new("show_properties",
						   GTK_RUN_LAST,
						   object_class->type,
						   GTK_SIGNAL_OFFSET (NautilusViewClientClass, show_properties),
						   gtk_marshal_NONE__NONE,
						   GTK_TYPE_NONE, 0);
  klass->view_client_signals[i++] = gtk_signal_new("notify_selection_change",
						   GTK_RUN_LAST,
						   object_class->type,
						   GTK_SIGNAL_OFFSET (NautilusViewClientClass, notify_selection_change),
						   gtk_marshal_NONE__BOXED,
						   GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  gtk_object_class_add_signals (object_class, klass->view_client_signals, i);
}

static void
nautilus_view_client_set_arg (GtkObject      *object,
			      GtkArg         *arg,
			      guint	       arg_id)
{
}

static void
nautilus_view_client_get_arg (GtkObject      *object,
			      GtkArg         *arg,
			      guint	        arg_id)
{
}

static void
nautilus_view_client_init (NautilusViewClient *view)
{
  CORBA_Environment ev;
  GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);

  view->control = GNOME_OBJECT(gnome_control_new(GTK_WIDGET(view)));

  CORBA_exception_init(&ev);
  view->view_client = impl_Nautilus_View__create(view, &ev);
  gnome_object_add_interface(view->control, view->view_client);
  CORBA_exception_free(&ev);
}

static void
nautilus_view_client_destroy (NautilusViewClient *view)
{
  NautilusViewClientClass *klass = NAUTILUS_VIEW_CLIENT_CLASS(GTK_OBJECT(view)->klass);

  gnome_object_destroy(view->view_client);
  gnome_object_destroy(view->control);

  if(((GtkObjectClass *)klass->parent_class)->destroy)
    ((GtkObjectClass *)klass->parent_class)->destroy((GtkObject *)view);
}

void
nautilus_view_client_request_location_change(NautilusViewClient *view,
					     Nautilus_NavigationRequestInfo *loc)
{
  CORBA_Environment ev;

  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW_CLIENT (view));

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
nautilus_view_client_request_selection_change (NautilusViewClient              *view,
					       Nautilus_SelectionRequestInfo *loc)
{
  CORBA_Environment ev;

  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW_CLIENT (view));

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
nautilus_view_client_request_status_change    (NautilusViewClient        *view,
					       Nautilus_StatusRequestInfo *loc)
{
  CORBA_Environment ev;

  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW_CLIENT (view));

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

static void
nautilus_view_client_size_request (GtkWidget      *widget,
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
nautilus_view_client_size_allocate (GtkWidget     *widget,
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
nautilus_view_client_get_gnome_object    (NautilusViewClient        *view)
{
  return view->control;
}
