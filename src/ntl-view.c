/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
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
/* ntl-view.c: Implementation of the object representing a data view,
   and its associated CORBA object for proxying requests into this
   object. */

#include "ntl-view-private.h"
#include "nautilus.h"
#include <libnautilus/nautilus-gtk-extensions.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>

enum {
  REQUEST_LOCATION_CHANGE,
  REQUEST_SELECTION_CHANGE,
  REQUEST_STATUS_CHANGE,
  REQUEST_PROGRESS_CHANGE,
  NOTIFY_ZOOM_LEVEL,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_MAIN_WINDOW
};




static void nautilus_view_init       (NautilusView      *view);
static void nautilus_view_destroy    (GtkObject      *view);
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


static guint nautilus_view_signals[LAST_SIGNAL];


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

  object_class = (GtkObjectClass*) klass;
  object_class->destroy = nautilus_view_destroy;
  object_class->set_arg = nautilus_view_set_arg;
  object_class->get_arg = nautilus_view_get_arg;

  widget_class = (GtkWidgetClass*) klass;
  widget_class->size_request = nautilus_view_size_request;
  widget_class->size_allocate = nautilus_view_size_allocate;

  klass->servant_init_func = POA_Nautilus_ViewFrame__init;
  klass->servant_destroy_func = POA_Nautilus_ViewFrame__fini;
  klass->vepv = &impl_Nautilus_ViewFrame_vepv;

  klass->zoomable_servant_init_func = POA_Nautilus_ZoomableFrame__init;
  klass->zoomable_servant_destroy_func = POA_Nautilus_ZoomableFrame__fini;
  klass->zoomable_vepv = &impl_Nautilus_ZoomableFrame_vepv;

  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
  /* klass->request_location_change = NULL; */
  /* klass->request_selection_change = NULL; */
  /* klass->request_status_change = NULL; */
  /* klass->request_progress_change = NULL; */
  klass->view_constructed = nautilus_view_constructed;

  nautilus_view_signals[REQUEST_LOCATION_CHANGE] = gtk_signal_new("request_location_change",
                                                                  GTK_RUN_LAST,
                                                                  object_class->type,
                                                                  GTK_SIGNAL_OFFSET (NautilusViewClass, 
                                                                                     request_location_change),
                                                                  gtk_marshal_NONE__BOXED,
                                                                  GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  nautilus_view_signals[REQUEST_SELECTION_CHANGE] = gtk_signal_new("request_selection_change",
                                                                   GTK_RUN_LAST,
                                                                   object_class->type,
                                                                   GTK_SIGNAL_OFFSET (NautilusViewClass, 
                                                                                      request_selection_change),
                                                                   gtk_marshal_NONE__BOXED,
                                                                   GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  nautilus_view_signals[REQUEST_STATUS_CHANGE] = gtk_signal_new("request_status_change",
                                                                GTK_RUN_LAST,
                                                                object_class->type,
                                                                GTK_SIGNAL_OFFSET (NautilusViewClass, 
                                                                                   request_status_change),
                                                                gtk_marshal_NONE__BOXED,
                                                                GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  nautilus_view_signals[REQUEST_PROGRESS_CHANGE] = gtk_signal_new("request_progress_change",
                                                                   GTK_RUN_LAST,
                                                                   object_class->type,
                                                                   GTK_SIGNAL_OFFSET (NautilusViewClass, 
                                                                                      request_progress_change),
                                                                   gtk_marshal_NONE__BOXED,
                                                                   GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);

  nautilus_view_signals[NOTIFY_ZOOM_LEVEL] = gtk_signal_new("notify_zoom_level",
                                                            GTK_RUN_LAST,
                                                            object_class->type,
                                                            GTK_SIGNAL_OFFSET (NautilusViewClass, 
                                                                               notify_zoom_level),
                                                            nautilus_gtk_marshal_NONE__DOUBLE,
                                                            GTK_TYPE_NONE, 1, GTK_TYPE_DOUBLE);

  gtk_object_class_add_signals (object_class, nautilus_view_signals, LAST_SIGNAL);
  

  gtk_object_add_arg_type ("NautilusView::main_window",
			   GTK_TYPE_OBJECT,
			   GTK_ARG_READWRITE,
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
nautilus_view_destroy_client(NautilusView *view)
{
  if(!view->component_class)
    return;

  g_free(view->iid); view->iid = NULL;

  bonobo_object_unref (BONOBO_OBJECT (view->client_object)); view->client_object = NULL;

  gtk_container_remove (GTK_CONTAINER(view), view->client_widget); view->client_widget = NULL;

  if(view->component_class->destroy)
    {
      CORBA_Environment ev;
      CORBA_exception_init(&ev);
      view->component_class->destroy(view, &ev);
      CORBA_exception_free(&ev);
    }

  bonobo_object_unref (view->view_frame); view->view_frame = NULL;

  view->component_class = NULL;
  view->component_data = NULL;
}

static void
nautilus_view_destroy(GtkObject      *view)
{
  NautilusViewClass *klass = NAUTILUS_VIEW_CLASS(view->klass);
  NautilusView *nview = NAUTILUS_VIEW(view);

  if(nview->timer_id)
    {
      g_source_remove(nview->timer_id);
      nview->timer_id = 0;
    }

  nautilus_view_destroy_client(nview);

  if(GTK_OBJECT_CLASS(klass->parent_class)->destroy)
    GTK_OBJECT_CLASS(klass->parent_class)->destroy(view);
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

extern NautilusViewComponentType nautilus_view_component_type; /* ntl-view-nautilus.c */
extern NautilusViewComponentType bonobo_subdoc_component_type; /* ntl-view-bonobo-subdoc.c */
extern NautilusViewComponentType bonobo_control_component_type; /* ntl-view-bonobo-control.c */

static gboolean
nautilus_view_handle_client_destroy(GtkWidget *widget, NautilusView *view)
{
  gtk_object_destroy(GTK_OBJECT(view));
  return TRUE;
}

static void
nautilus_view_handle_client_destroy_2(GtkObject *object, CORBA_Object cobject, CORBA_Environment *ev, NautilusView *view)
{
  /* ICK! */
  if(NAUTILUS_IS_META_VIEW(view))
    nautilus_window_remove_meta_view(NAUTILUS_WINDOW(view->main_window), view);
  else if(NAUTILUS_IS_CONTENT_VIEW(view))
    nautilus_window_set_content_view(NAUTILUS_WINDOW(view->main_window), NULL);
}

gboolean /* returns TRUE if successful */
nautilus_view_load_client(NautilusView *view, const char *iid)
{
  CORBA_Object obj;
  CORBA_Object zoomable;

  CORBA_Environment ev;
  int i;
  NautilusViewComponentType *component_types[] = {
    &nautilus_view_component_type,
    &bonobo_subdoc_component_type,
    &bonobo_control_component_type,
    NULL
  };

  if (iid == NULL)
    return FALSE;

  CORBA_exception_init(&ev);

  nautilus_view_destroy_client(view);

  view->client_object = bonobo_object_activate(iid, 0);
  if(!view->client_object)
    return FALSE;

  view->view_frame = impl_Nautilus_ViewFrame__create(view, &ev);
  view->zoomable_frame = impl_Nautilus_ZoomableFrame__create(view, &ev);

  /* Now figure out which type of embedded object it is: */

  for(i = 0; component_types[i] && !view->component_class; i++)
    {
      obj = Bonobo_Unknown_query_interface(bonobo_object_corba_objref(BONOBO_OBJECT(view->client_object)),
                                          component_types[i]->primary_repoid, &ev);
      if(ev._major != CORBA_NO_EXCEPTION)
        obj = CORBA_OBJECT_NIL;

      if(CORBA_Object_is_nil(obj, &ev))
        continue;

      zoomable = bonobo_object_query_interface (BONOBO_OBJECT (view->client_object), 
                                                "IDL:Nautilus/Zoomable:1.0");

      view->zoomable = zoomable;

      if(component_types[i]->try_load(view, obj, &ev))
        view->component_class = component_types[i];

      Bonobo_Unknown_unref(obj, &ev);
      CORBA_Object_release(obj, &ev);

      if (view->component_class)
        break;
    }

  if (!view->component_class)
    {
      /* Nothing matched */
      nautilus_view_destroy_client(view);

      return FALSE;
    }
      
  view->iid = g_strdup(iid);

  gtk_signal_connect_while_alive(GTK_OBJECT(view->client_object), "destroy",
                                 GTK_SIGNAL_FUNC(nautilus_view_handle_client_destroy), view,
                                 GTK_OBJECT(view));
  gtk_signal_connect_while_alive(GTK_OBJECT(view->client_object), "object_gone",
                                 GTK_SIGNAL_FUNC(nautilus_view_handle_client_destroy_2), view,
                                 GTK_OBJECT(view));
  gtk_signal_connect_while_alive(GTK_OBJECT(view->client_object), "system_exception",
                                 GTK_SIGNAL_FUNC(nautilus_view_handle_client_destroy_2), view,
                                 GTK_OBJECT(view));
  gtk_container_add(GTK_CONTAINER(view), view->client_widget);
  gtk_widget_show(view->client_widget);
  CORBA_exception_free(&ev);

  return TRUE;
}

void
nautilus_view_notify_location_change(NautilusView *view,
				     Nautilus_NavigationInfo *nav_context)
{
  Nautilus_NavigationInfo real_nav_ctx;
  CORBA_Environment ev;

  g_return_if_fail(view);
  g_return_if_fail(view->component_class);
  g_return_if_fail(NAUTILUS_VIEW(view));

  real_nav_ctx = *nav_context;
  g_assert(real_nav_ctx.requested_uri);
#define DEFAULT_STRING(x) if(!real_nav_ctx.x) real_nav_ctx.x = ""
  if(!real_nav_ctx.actual_uri) real_nav_ctx.actual_uri = real_nav_ctx.requested_uri;
  DEFAULT_STRING(content_type);

  DEFAULT_STRING(referring_uri);
  if(!real_nav_ctx.actual_referring_uri) real_nav_ctx.actual_referring_uri = real_nav_ctx.referring_uri;
  DEFAULT_STRING(referring_content_type);

  CORBA_exception_init(&ev);
  if(view->component_class->notify_location_change)
    view->component_class->notify_location_change(view, &real_nav_ctx, &ev);
  CORBA_exception_free(&ev);
}

void
nautilus_view_notify_selection_change(NautilusView *view,
				      Nautilus_SelectionInfo *nav_context)
{
  CORBA_Environment ev;

  g_return_if_fail(view);
  g_return_if_fail(view->component_class);
  g_return_if_fail(NAUTILUS_VIEW(view));

  CORBA_exception_init(&ev);

  if(view->component_class->notify_selection_change)
    view->component_class->notify_selection_change(view, nav_context, &ev);

  CORBA_exception_free(&ev);
}

void
nautilus_view_load_state(NautilusView *view, const char *config_path)
{
  CORBA_Environment ev;

  g_return_if_fail(view);
  g_return_if_fail(view->component_class);
  g_return_if_fail(NAUTILUS_VIEW(view));

  CORBA_exception_init(&ev);

  if(view->component_class->load_state)
    view->component_class->load_state(view, config_path, &ev);

  CORBA_exception_free(&ev);
}

void
nautilus_view_save_state(NautilusView *view, const char *config_path)
{
  CORBA_Environment ev;

  g_return_if_fail(view);
  g_return_if_fail(view->component_class);

  CORBA_exception_init(&ev);

  if(view->component_class->save_state)
    view->component_class->save_state(view, config_path, &ev);

  CORBA_exception_free(&ev);
}

void
nautilus_view_show_properties(NautilusView *view)
{
  CORBA_Environment ev;

  g_return_if_fail(view);
  g_return_if_fail(view->component_class);

  CORBA_exception_init(&ev);

  if(view->component_class->show_properties)
    view->component_class->show_properties(view, &ev);

  CORBA_exception_free(&ev);
}

void
nautilus_view_stop_location_change(NautilusView *view)
{
  CORBA_Environment ev;

  g_return_if_fail(view);
  g_return_if_fail(view->component_class);

  CORBA_exception_init(&ev);

  if(view->component_class->stop_location_change)
    view->component_class->stop_location_change(view, &ev);

  CORBA_exception_free(&ev);
}


gboolean
nautilus_view_is_zoomable (NautilusView *view)
{
  CORBA_Environment ev;
  gboolean retval;

  CORBA_exception_init (&ev);

  retval = CORBA_Object_is_nil (view->zoomable, &ev);

  CORBA_exception_free (&ev);

  return retval;
}

gdouble
nautilus_view_get_zoom_level (NautilusView *view)
{
  CORBA_Environment ev;
  gdouble retval;

  g_return_val_if_fail (view, 0);

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    retval = Nautilus_Zoomable__get_zoom_level (view->zoomable, &ev);
  } else {
    retval = 1.0;
  }

  CORBA_exception_free (&ev);

  return retval;
}

void
nautilus_view_set_zoom_level (NautilusView *view,
                              gdouble       zoom_level)
{
  CORBA_Environment ev;

  g_return_if_fail (view);

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    Nautilus_Zoomable__set_zoom_level (view->zoomable, zoom_level, &ev);
  } else {
    /* do nothing */
  }

  CORBA_exception_free (&ev);
}

gdouble
nautilus_view_get_min_zoom_level (NautilusView *view)
{
  CORBA_Environment ev;
  gdouble retval;

  g_return_val_if_fail (view, 0);

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    retval = Nautilus_Zoomable__get_min_zoom_level (view->zoomable, &ev);
  } else {
    retval = 1.0;
  }

  CORBA_exception_free (&ev);

  return retval;
}

gdouble
nautilus_view_get_max_zoom_level (NautilusView *view)
{
  CORBA_Environment ev;
  gdouble retval;

  g_return_val_if_fail (view, 0);

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    retval = Nautilus_Zoomable__get_max_zoom_level (view->zoomable, &ev);
  } else {
    retval = 1.0;
  }

  CORBA_exception_free (&ev);

  return retval;
}

gboolean
nautilus_view_get_is_continuous (NautilusView *view)
{
  CORBA_Environment ev;
  gboolean retval;

  g_return_val_if_fail (view, FALSE);

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    retval = Nautilus_Zoomable__get_zoom_level (view->zoomable, &ev);
  } else {
    retval = FALSE;
  }

  CORBA_exception_free (&ev);

  return retval;
}


void
nautilus_view_zoom_in (NautilusView *view)
{
  CORBA_Environment ev;

  g_return_if_fail(view);

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    Nautilus_Zoomable_zoom_in (view->zoomable, &ev);
  } else {
    /* do nothing */
  }

  CORBA_exception_free (&ev);
}


void
nautilus_view_zoom_out (NautilusView *view)
{
  CORBA_Environment ev;

  g_return_if_fail(view);

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    Nautilus_Zoomable_zoom_out (view->zoomable, &ev);
  } else {
    /* do nothing */
  }

  CORBA_exception_free (&ev);
}


void
nautilus_view_zoom_to_fit (NautilusView *view)
{
  CORBA_Environment ev;

  g_return_if_fail(view);

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    Nautilus_Zoomable_zoom_to_fit (view->zoomable, &ev);
  } else {
    /* do nothing */
  }

  CORBA_exception_free (&ev);
}


const char *
nautilus_view_get_iid(NautilusView *view)
{
  return view->iid;
}

CORBA_Object
nautilus_view_get_client_objref(NautilusView *view)
{
  return view?bonobo_object_corba_objref(BONOBO_OBJECT(view->client_object)):NULL;
}

CORBA_Object
nautilus_view_get_objref(NautilusView *view)
{
  return view?bonobo_object_corba_objref(view->view_frame):NULL;
}


void
nautilus_view_request_location_change(NautilusView *view,
                                      Nautilus_NavigationRequestInfo *loc)
{
  gtk_signal_emit(GTK_OBJECT(view), nautilus_view_signals[REQUEST_LOCATION_CHANGE], loc);
}

void
nautilus_view_request_selection_change (NautilusView              *view,
                                        Nautilus_SelectionRequestInfo *loc)
{
  gtk_signal_emit(GTK_OBJECT(view), nautilus_view_signals[REQUEST_SELECTION_CHANGE], loc);
}

void
nautilus_view_request_status_change    (NautilusView              *view,
                                        Nautilus_StatusRequestInfo *loc)
{
  gtk_signal_emit(GTK_OBJECT(view), nautilus_view_signals[REQUEST_STATUS_CHANGE], loc);
}

void
nautilus_view_request_progress_change(NautilusView              *view,
                                      Nautilus_ProgressRequestInfo *loc)
{
  gtk_signal_emit(GTK_OBJECT(view), nautilus_view_signals[REQUEST_PROGRESS_CHANGE], loc);
}

void
nautilus_view_notify_zoom_level (NautilusView *view,
                                 gdouble       level)
{
  gtk_signal_emit (GTK_OBJECT (view), nautilus_view_signals[NOTIFY_ZOOM_LEVEL], level);
}


static gboolean
check_object(NautilusView *view)
{
  CORBA_Environment ev;
  gboolean retval = TRUE;
  CORBA_exception_init(&ev);

  g_assert(!view->checking);

  view->checking++;

  if(CORBA_Object_non_existent(bonobo_object_corba_objref(BONOBO_OBJECT(view->client_object)), &ev))
    {
      view->timer_id = 0;
      gtk_object_destroy(GTK_OBJECT(view));
      retval = FALSE;
    }

  CORBA_exception_free(&ev);
  view->checking--;

  return retval;
}

void
nautilus_view_set_active_errors(NautilusView *view, gboolean enabled)
{
  if(enabled)
    {
      if(!view->timer_id)
        view->timer_id = g_timeout_add(2000, (GSourceFunc)check_object, view);
    }
  else
    {
      if(view->timer_id)
        {
          g_source_remove(view->timer_id);
          view->timer_id = 0;
        }
    }
}
