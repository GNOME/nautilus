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

#include <config.h>
#include "nautilus-view-frame-private.h"

#include "nautilus-application.h"
#include "nautilus-window.h"
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>

enum {
  REQUEST_LOCATION_CHANGE,
  REQUEST_SELECTION_CHANGE,
  REQUEST_STATUS_CHANGE,
  REQUEST_PROGRESS_CHANGE,
  REQUEST_TITLE_CHANGE,
  NOTIFY_ZOOM_LEVEL,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_MAIN_WINDOW
};

static void nautilus_view_frame_initialize       (NautilusViewFrame      *view);
static void nautilus_view_frame_destroy          (GtkObject              *view);
static void nautilus_view_frame_constructed      (NautilusViewFrame      *view);
static void nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass);
static void nautilus_view_frame_set_arg          (GtkObject              *object,
                                                  GtkArg                 *arg,
                                                  guint                   arg_id);
static void nautilus_view_frame_get_arg          (GtkObject              *object,
                                                  GtkArg                 *arg,
                                                  guint                   arg_id);

static guint signals[LAST_SIGNAL];

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusViewFrame, nautilus_view_frame, NAUTILUS_TYPE_GENEROUS_BIN)

static void
nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;
  object_class->destroy = nautilus_view_frame_destroy;
  object_class->set_arg = nautilus_view_frame_set_arg;
  object_class->get_arg = nautilus_view_frame_get_arg;

  klass->servant_init_func = POA_Nautilus_ViewFrame__init;
  klass->servant_destroy_func = POA_Nautilus_ViewFrame__fini;
  klass->vepv = &impl_Nautilus_ViewFrame_vepv;

  klass->zoomable_servant_init_func = POA_Nautilus_ZoomableFrame__init;
  klass->zoomable_servant_destroy_func = POA_Nautilus_ZoomableFrame__fini;
  klass->zoomable_vepv = &impl_Nautilus_ZoomableFrame_vepv;

  /* klass->request_location_change = NULL; */
  /* klass->request_selection_change = NULL; */
  /* klass->request_status_change = NULL; */
  /* klass->request_progress_change = NULL; */
  klass->view_constructed = nautilus_view_frame_constructed;

  signals[REQUEST_LOCATION_CHANGE] =
    gtk_signal_new ("request_location_change",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
                                       request_location_change),
                    gtk_marshal_NONE__BOXED,
                    GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  signals[REQUEST_SELECTION_CHANGE] =
    gtk_signal_new ("request_selection_change",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
                                       request_selection_change),
                    gtk_marshal_NONE__BOXED,
                    GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  signals[REQUEST_STATUS_CHANGE] =
    gtk_signal_new ("request_status_change",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
                                       request_status_change),
                    gtk_marshal_NONE__BOXED,
                    GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  signals[REQUEST_PROGRESS_CHANGE] =
    gtk_signal_new ("request_progress_change",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
                                       request_progress_change),
                    gtk_marshal_NONE__STRING,
                    GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
  signals[REQUEST_TITLE_CHANGE] =
    gtk_signal_new ("request_title_change",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
                                       request_title_change),
                    gtk_marshal_NONE__BOXED,
                    GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
  
  signals[NOTIFY_ZOOM_LEVEL] =
    gtk_signal_new ("notify_zoom_level",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
                                       notify_zoom_level),
                    nautilus_gtk_marshal_NONE__DOUBLE,
                    GTK_TYPE_NONE, 1, GTK_TYPE_DOUBLE);
  
  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
    
  gtk_object_add_arg_type ("NautilusViewFrame::main_window",
			   GTK_TYPE_OBJECT,
			   GTK_ARG_READWRITE,
			   ARG_MAIN_WINDOW);

  klass->num_construct_args++;
}

static void
nautilus_view_frame_set_arg (GtkObject      *object,
                             GtkArg         *arg,
                             guint	       arg_id)
{
  NautilusViewFrame *view;

  view = NAUTILUS_VIEW_FRAME(object);
  switch(arg_id) {
  case ARG_MAIN_WINDOW:
    view->main_window = GTK_WIDGET(GTK_VALUE_OBJECT(*arg));
    nautilus_view_frame_construct_arg_set(view);
    break;
  }
}

static void
nautilus_view_frame_get_arg (GtkObject      *object,
                             GtkArg         *arg,
                             guint	        arg_id)
{
  switch(arg_id) {
  case ARG_MAIN_WINDOW:
    GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(NAUTILUS_VIEW_FRAME(object)->main_window);
    break;
  }
}

static void
nautilus_view_frame_initialize (NautilusViewFrame *view)
{
  GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);	
}

static void
nautilus_view_frame_destroy_client(NautilusViewFrame *view)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);
 
  if(!view->component_class)
    return;

  g_free(view->iid); view->iid = NULL;

  bonobo_object_unref (BONOBO_OBJECT (view->client_object)); view->client_object = NULL;

  gtk_container_remove (GTK_CONTAINER(view), view->client_widget); view->client_widget = NULL;

  if (! CORBA_Object_is_nil (view->zoomable, &ev)) {
    Bonobo_Unknown_unref (view->zoomable, &ev);
    view->zoomable = CORBA_OBJECT_NIL;
  }

  if(view->component_class->destroy) {
    view->component_class->destroy(view, &ev);
  }

  /* FIXME bugzilla.eazel.com 917: This should be bonobo_object_unref,
   * but there is a circular reference that prevents it from working.
   * Once that's fixed, we'd really like to change it to unref instead of destroy.
   */
  bonobo_object_destroy (view->view_frame);
  view->view_frame = NULL;

  view->component_class = NULL;
  view->component_data = NULL;

  CORBA_exception_free(&ev);
}

static void
nautilus_view_frame_destroy(GtkObject      *view)
{
  NautilusViewFrame *nview = NAUTILUS_VIEW_FRAME(view);

  if(nview->timer_id)
    {
      g_source_remove(nview->timer_id);
      nview->timer_id = 0;
    }

  nautilus_view_frame_destroy_client(nview);

  NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (view));
}

static void
nautilus_view_frame_constructed(NautilusViewFrame *view)
{
}

void
nautilus_view_frame_construct_arg_set(NautilusViewFrame *view)
{
  guint nca;
  NautilusViewFrameClass *klass;

  klass = NAUTILUS_VIEW_FRAME_CLASS(((GtkObject *)view)->klass);
  nca = klass->num_construct_args;
  if(view->construct_arg_count >= nca)
    return;

	view->construct_arg_count++;
	if((view->construct_arg_count >= nca) && klass->view_constructed) {
		klass->view_constructed(view);
	}
}

extern NautilusViewComponentType nautilus_view_component_type; /* ntl-view-nautilus.c */
extern NautilusViewComponentType bonobo_subdoc_component_type; /* ntl-view-bonobo-subdoc.c */
extern NautilusViewComponentType bonobo_control_component_type; /* ntl-view-bonobo-control.c */

static gboolean
nautilus_view_frame_handle_client_destroy(GtkWidget *widget, NautilusViewFrame *view)
{
  gtk_object_destroy(GTK_OBJECT(view));
  return TRUE;
}

static void
nautilus_view_frame_handle_client_destroy_2(GtkObject *object, CORBA_Object cobject, CORBA_Environment *ev, NautilusViewFrame *view)
{
  NautilusWindow *window;

  window = NAUTILUS_WINDOW (view->main_window);
  nautilus_window_remove_sidebar_panel (window, view);
  if (view == window->content_view)
    nautilus_window_set_content_view (window, NULL);
}

gboolean /* returns TRUE if successful */
nautilus_view_frame_load_client(NautilusViewFrame *view, const char *iid)
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

	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);

	if (iid == NULL)
		return FALSE;

	CORBA_exception_init(&ev);

	nautilus_view_frame_destroy_client(view);

	view->client_object = bonobo_object_activate(iid, 0);
	if(!view->client_object)
		return FALSE;

	view->view_frame = impl_Nautilus_ViewFrame__create(view, &ev);
	view->zoomable_frame = impl_Nautilus_ZoomableFrame__create(view, &ev);

	/* Add undo manager to component */
	nautilus_undo_manager_add_interface
        	(NAUTILUS_UNDO_MANAGER (NAUTILUS_APP (NAUTILUS_WINDOW (view->main_window)->app)->undo_manager),
                 BONOBO_OBJECT (view->view_frame));

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

  	if (!view->component_class) {
     		/* Nothing matched */
      		nautilus_view_frame_destroy_client(view);
      		return FALSE;
    	}
      
	view->iid = g_strdup(iid);

	gtk_signal_connect_while_alive(GTK_OBJECT(view->client_object), "destroy",
                                 GTK_SIGNAL_FUNC(nautilus_view_frame_handle_client_destroy), view,
                                 GTK_OBJECT(view));
	gtk_signal_connect_while_alive(GTK_OBJECT(view->client_object), "object_gone",
                                 GTK_SIGNAL_FUNC(nautilus_view_frame_handle_client_destroy_2), view,
                                 GTK_OBJECT(view));
	gtk_signal_connect_while_alive(GTK_OBJECT(view->client_object), "system_exception",
                                 GTK_SIGNAL_FUNC(nautilus_view_frame_handle_client_destroy_2), view,
                                 GTK_OBJECT(view));
	gtk_container_add(GTK_CONTAINER(view), view->client_widget);
	gtk_widget_show(view->client_widget);
	CORBA_exception_free(&ev);

	return TRUE;
}

void
nautilus_view_frame_notify_location_change(NautilusViewFrame *view,
				     Nautilus_NavigationInfo *nav_context)
{
  Nautilus_NavigationInfo real_nav_ctx;
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  g_return_if_fail(view->component_class);

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
nautilus_view_frame_notify_selection_change(NautilusViewFrame *view,
				      Nautilus_SelectionInfo *nav_context)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  g_return_if_fail(view->component_class);

  CORBA_exception_init(&ev);

  if(view->component_class->notify_selection_change)
    view->component_class->notify_selection_change(view, nav_context, &ev);

  CORBA_exception_free(&ev);
}

void
nautilus_view_frame_load_state(NautilusViewFrame *view, const char *config_path)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  g_return_if_fail(view->component_class);

  CORBA_exception_init(&ev);

  if(view->component_class->load_state)
    view->component_class->load_state(view, config_path, &ev);

  CORBA_exception_free(&ev);
}

void
nautilus_view_frame_save_state(NautilusViewFrame *view, const char *config_path)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  g_return_if_fail(view->component_class);

  CORBA_exception_init(&ev);

  if(view->component_class->save_state)
    view->component_class->save_state(view, config_path, &ev);

  CORBA_exception_free(&ev);
}

void
nautilus_view_frame_show_properties(NautilusViewFrame *view)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  g_return_if_fail(view->component_class);

  CORBA_exception_init(&ev);

  if(view->component_class->show_properties)
    view->component_class->show_properties(view, &ev);

  CORBA_exception_free(&ev);
}

void
nautilus_view_frame_stop_location_change(NautilusViewFrame *view)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  g_return_if_fail(view->component_class);

  CORBA_exception_init(&ev);

  if(view->component_class->stop_location_change)
    view->component_class->stop_location_change(view, &ev);

  CORBA_exception_free(&ev);
}


gboolean
nautilus_view_frame_is_zoomable (NautilusViewFrame *view)
{
  CORBA_Environment ev;
  gboolean retval;

  g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);

  CORBA_exception_init (&ev);

  retval = CORBA_Object_is_nil (view->zoomable, &ev);

  CORBA_exception_free (&ev);

  return retval;
}

gdouble
nautilus_view_frame_get_zoom_level (NautilusViewFrame *view)
{
  CORBA_Environment ev;
  gdouble retval;

  g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);

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
nautilus_view_frame_set_zoom_level (NautilusViewFrame *view,
                              gdouble       zoom_level)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    Nautilus_Zoomable__set_zoom_level (view->zoomable, zoom_level, &ev);
  } else {
    /* do nothing */
  }

  CORBA_exception_free (&ev);
}

gdouble
nautilus_view_frame_get_min_zoom_level (NautilusViewFrame *view)
{
  CORBA_Environment ev;
  gdouble retval;

  g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);

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
nautilus_view_frame_get_max_zoom_level (NautilusViewFrame *view)
{
  CORBA_Environment ev;
  gdouble retval;

  g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);

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
nautilus_view_frame_get_is_continuous (NautilusViewFrame *view)
{
  CORBA_Environment ev;
  gboolean retval;

  g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);

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
nautilus_view_frame_zoom_in (NautilusViewFrame *view)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    Nautilus_Zoomable_zoom_in (view->zoomable, &ev);
  } else {
    /* do nothing */
  }

  CORBA_exception_free (&ev);
}


void
nautilus_view_frame_zoom_out (NautilusViewFrame *view)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    Nautilus_Zoomable_zoom_out (view->zoomable, &ev);
  } else {
    /* do nothing */
  }

  CORBA_exception_free (&ev);
}


void
nautilus_view_frame_zoom_to_fit (NautilusViewFrame *view)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

  CORBA_exception_init (&ev);

  if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
    Nautilus_Zoomable_zoom_to_fit (view->zoomable, &ev);
  } else {
    /* do nothing */
  }

  CORBA_exception_free (&ev);
}


const char *
nautilus_view_frame_get_iid(NautilusViewFrame *view)
{
  g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
  return view->iid;
}

CORBA_Object
nautilus_view_frame_get_client_objref(NautilusViewFrame *view)
{
  g_return_val_if_fail (view == NULL || NAUTILUS_IS_VIEW_FRAME (view), NULL);
  return view?bonobo_object_corba_objref(BONOBO_OBJECT(view->client_object)):NULL;
}

CORBA_Object
nautilus_view_frame_get_objref(NautilusViewFrame *view)
{
  g_return_val_if_fail (view == NULL || NAUTILUS_IS_VIEW_FRAME (view), NULL);
  return view?bonobo_object_corba_objref(view->view_frame):NULL;
}


void
nautilus_view_frame_request_location_change(NautilusViewFrame *view,
                                            const Nautilus_NavigationRequestInfo *loc)
{
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  gtk_signal_emit(GTK_OBJECT(view), signals[REQUEST_LOCATION_CHANGE], loc);
}

void
nautilus_view_frame_request_selection_change (NautilusViewFrame              *view,
                                              const Nautilus_SelectionRequestInfo *loc)
{
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  gtk_signal_emit(GTK_OBJECT(view), signals[REQUEST_SELECTION_CHANGE], loc);
}

void
nautilus_view_frame_request_status_change    (NautilusViewFrame              *view,
                                              const Nautilus_StatusRequestInfo *loc)
{
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  gtk_signal_emit(GTK_OBJECT(view), signals[REQUEST_STATUS_CHANGE], loc);
}

void
nautilus_view_frame_request_progress_change(NautilusViewFrame              *view,
                                            const Nautilus_ProgressRequestInfo *loc)
{
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  gtk_signal_emit(GTK_OBJECT(view), signals[REQUEST_PROGRESS_CHANGE], loc);
}

void
nautilus_view_frame_request_title_change (NautilusViewFrame *view,
                                          const char *title)
{
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  gtk_signal_emit (GTK_OBJECT (view), signals[REQUEST_TITLE_CHANGE], title);
}

void
nautilus_view_frame_notify_zoom_level (NautilusViewFrame *view,
                                       gdouble       level)
{
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  gtk_signal_emit (GTK_OBJECT (view), signals[NOTIFY_ZOOM_LEVEL], level);
}


static gboolean
check_object(NautilusViewFrame *view)
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
nautilus_view_frame_set_active_errors(NautilusViewFrame *view, gboolean enabled)
{
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
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

char *
nautilus_view_frame_get_label (NautilusViewFrame *view)
{
  g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
  return g_strdup (view->label);
}

void
nautilus_view_frame_set_label (NautilusViewFrame *view,
                               const char *label)
{
  g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
  g_free (view->label);
  view->label = g_strdup (label);
}

/* Calls activate on the underlying control frame. */
void
nautilus_view_frame_activate (NautilusViewFrame *view)
{
  BonoboControlFrame *control_frame;

  control_frame = BONOBO_CONTROL_FRAME (bonobo_object_query_local_interface 
                                        (view->view_frame, "IDL:Bonobo/ControlFrame:1.0"));
  bonobo_control_frame_control_activate (control_frame);
  bonobo_object_unref (BONOBO_OBJECT (control_frame));
}
