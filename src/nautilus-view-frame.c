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
/* ntl-view.c: Implementation of the object representing a data view,
   and its associated CORBA object for proxying requests into this
   object. */

#include <gtk/gtksignal.h>
#include <gtk/gtk.h>
#include "nautilus.h"

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
  GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);
  view->type = NV_NONE;
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

static void
nautilus_view_activate_uri(GnomeControlFrame *frame, const char *uri, gboolean relative, NautilusView *view)
{
  Nautilus_NavigationRequestInfo nri;
  g_assert(!relative);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = (char *)uri;
  nautilus_window_request_location_change(NAUTILUS_WINDOW(view->main_window), &nri, GTK_WIDGET(view));
}

static void
destroy_nautilus_view(NautilusView *view)
{
  CORBA_Environment ev;

  CORBA_exception_init(&ev);

  CORBA_Object_release(view->u.nautilus_view_info.view_client, &ev);
  gnome_object_destroy(view->u.nautilus_view_info.control_frame);
}

static void
destroy_bonobo_subdoc_view(NautilusView *view)
{
  gnome_object_destroy(view->u.bonobo_subdoc_info.container);
}

static void
destroy_bonobo_control_view(NautilusView *view)
{
  gnome_object_destroy(view->u.bonobo_control_info.control_frame);
}


static gboolean
bonobo_control_try_load_client(NautilusView *view, CORBA_Object obj)
{
  GNOME_Control control;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);

  control = (GNOME_Control) obj;
  
  view->u.nautilus_view_info.control_frame = GNOME_OBJECT(gnome_control_frame_new());
  gnome_object_add_interface(GNOME_OBJECT(view->u.bonobo_control_info.control_frame), view->view_frame);
  
  gnome_control_frame_bind_to_control(GNOME_CONTROL_FRAME(view->u.bonobo_control_info.control_frame),
                                      control);
  GNOME_Unknown_unref(control, &ev);
  view->client_widget =
    gnome_control_frame_get_widget(GNOME_CONTROL_FRAME(view->u.bonobo_control_info.control_frame));
  
  view->type = NV_BONOBO_CONTROL;
  gtk_signal_connect(GTK_OBJECT(view->u.bonobo_control_info.control_frame),
                     "activate_uri", GTK_SIGNAL_FUNC(nautilus_view_activate_uri), view);

  CORBA_exception_free(&ev);
  
  return TRUE;
}


static gboolean
bonobo_subdoc_try_load_client(NautilusView *view, CORBA_Object obj)
{
      CORBA_Environment ev;

      CORBA_exception_init(&ev);

      view->type = NV_BONOBO_SUBDOC;
      
      view->u.bonobo_subdoc_info.container = GNOME_OBJECT(gnome_container_new());
      gnome_object_add_interface(GNOME_OBJECT(view->u.bonobo_subdoc_info.container), view->view_frame);
      
      view->u.bonobo_subdoc_info.client_site =
        GNOME_OBJECT(gnome_client_site_new(GNOME_CONTAINER(view->u.bonobo_subdoc_info.container)));
      gnome_client_site_bind_embeddable(GNOME_CLIENT_SITE(view->u.bonobo_subdoc_info.client_site),
                                        view->client_object);
      gnome_container_add(GNOME_CONTAINER(view->u.bonobo_subdoc_info.container), view->u.bonobo_subdoc_info.client_site);
      view->u.bonobo_subdoc_info.view_frame =
        GNOME_OBJECT(gnome_client_site_new_view(GNOME_CLIENT_SITE(view->u.bonobo_subdoc_info.client_site)));
      if(!view->u.bonobo_subdoc_info.view_frame)
        {
          gnome_object_destroy(GNOME_OBJECT(view->client_object));
          gnome_object_destroy(view->u.bonobo_subdoc_info.container);
          gnome_object_destroy(view->u.bonobo_subdoc_info.client_site);
          GNOME_Unknown_unref(obj, &ev);
          CORBA_Object_release(obj, &ev);
          gnome_object_destroy(view->view_frame);
          view->type = NV_NONE;
          return FALSE;
        }
      
      view->client_widget = gnome_view_frame_get_wrapper(GNOME_VIEW_FRAME(view->u.bonobo_subdoc_info.view_frame));

      CORBA_exception_free(&ev);
      
      return TRUE;
}


static gboolean
nautilus_view_try_load_client(NautilusView *view, CORBA_Object obj)
{
  GNOME_Control control;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);

  view->u.nautilus_view_info.view_client = obj;
  
  view->type = NV_NAUTILUS_VIEW;
  control =
    GNOME_Unknown_query_interface(gnome_object_corba_objref(GNOME_OBJECT(view->client_object)), "IDL:GNOME/Control:1.0", &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    control = CORBA_OBJECT_NIL;
  
  if(CORBA_Object_is_nil(control, &ev))
    {
      gnome_object_unref(GNOME_OBJECT(view->client_object));
      GNOME_Unknown_unref(view->u.nautilus_view_info.view_client, &ev);
      CORBA_Object_release(view->u.nautilus_view_info.view_client, &ev);
      gnome_object_destroy(view->view_frame);
      view->type = NV_NONE;
      return FALSE;
    }
  
  view->u.nautilus_view_info.control_frame = GNOME_OBJECT(gnome_control_frame_new());
  gnome_object_add_interface(GNOME_OBJECT(view->u.nautilus_view_info.control_frame), view->view_frame);
  
  gnome_control_frame_bind_to_control(GNOME_CONTROL_FRAME(view->u.nautilus_view_info.control_frame), control);
  GNOME_Unknown_unref(control, &ev);
  view->client_widget = gnome_control_frame_get_widget(GNOME_CONTROL_FRAME(view->u.nautilus_view_info.control_frame));

  CORBA_exception_free(&ev);

  return TRUE;
}

gboolean /* returns TRUE if successful */
nautilus_view_load_client(NautilusView *view, const char *iid)
{
  CORBA_Object obj;
  CORBA_Environment ev;

  g_return_val_if_fail(iid, FALSE);

  CORBA_exception_init(&ev);

  if(view->type != NV_NONE)
    {
      g_free(view->iid); view->iid = NULL;
      gtk_container_remove(GTK_CONTAINER(view), view->client_widget); view->client_widget = NULL;

      switch(view->type)
        {
        case NV_NAUTILUS_VIEW:
          destroy_nautilus_view(view);
          break;
        case NV_BONOBO_SUBDOC:
          destroy_bonobo_subdoc_view(view);
          break;
        case NV_BONOBO_CONTROL:
          destroy_bonobo_control_view(view);
          break;
        default:
          break;
        }
      view->type = NV_NONE;
      gnome_object_destroy(GNOME_OBJECT(view->client_object)); view->client_object = NULL;
    }

  view->client_object = gnome_object_activate(iid, 0);
  if(!view->client_object)
    return FALSE;

  view->view_frame = impl_Nautilus_ViewFrame__create(view, &ev);

  /* Now figure out which type of embedded object it is: */

  /* Is it a Nautilus View? */
  obj = GNOME_Unknown_query_interface(gnome_object_corba_objref(GNOME_OBJECT(view->client_object)),
                                                   "IDL:Nautilus/View:1.0", &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    obj = CORBA_OBJECT_NIL;

  if(!CORBA_Object_is_nil(obj, &ev))
    {
      if (!nautilus_view_try_load_client(view, obj))
        {
          return FALSE;
        }
    }
  else
    {
      /* Is it a Bonobo Embeddable? */
      obj = 
        GNOME_Unknown_query_interface(gnome_object_corba_objref(GNOME_OBJECT(view->client_object)),
                                      "IDL:GNOME/Embeddable:1.0", &ev);

      if(ev._major != CORBA_NO_EXCEPTION)
        obj = CORBA_OBJECT_NIL;

      if(!CORBA_Object_is_nil(obj, &ev))
        {
          if (!bonobo_subdoc_try_load_client(view, obj)) 
            {
              g_warning("We don't know how to embed implementation %s", iid);
              return FALSE;
            }
        }
      else
        {
          /* Is it a Bonobo Control? */
          obj =
            GNOME_Unknown_query_interface(gnome_object_corba_objref(GNOME_OBJECT(view->client_object)),
                                          "IDL:GNOME/Control:1.0", &ev);
          if(ev._major != CORBA_NO_EXCEPTION)
            obj= CORBA_OBJECT_NIL;
          
          if(!CORBA_Object_is_nil(obj, &ev))
            {
              if (!bonobo_control_try_load_client(view, obj)) {
                return FALSE;
              } else {
                gnome_object_destroy(GNOME_OBJECT(view->client_object));
                gnome_object_destroy(view->view_frame);
                view->type = NV_NONE;
                return FALSE;
              }

            }
        }
    }
      
  view->iid = g_strdup(iid);

  gtk_widget_show(view->client_widget);
  gtk_container_add(GTK_CONTAINER(view), view->client_widget);
  CORBA_exception_free(&ev);

  return TRUE;
}

static void
nautilus_notify_location_change(NautilusView *view, Nautilus_NavigationInfo *real_nav_ctx)
{
  CORBA_Environment ev;
  
  CORBA_exception_init(&ev);

  Nautilus_View_notify_location_change(view->u.nautilus_view_info.view_client, real_nav_ctx, &ev);

  CORBA_exception_free(&ev);
}

static void
bonobo_subdoc_notify_location_change(NautilusView *view, Nautilus_NavigationInfo *real_nav_ctx)
{
  GNOME_PersistFile persist;
  CORBA_Environment ev;
  
  CORBA_exception_init(&ev);
  
  persist = gnome_object_client_query_interface(view->client_object, "IDL:GNOME/PersistFile:1.0",
                                                NULL);
  if(!CORBA_Object_is_nil(persist, &ev))
    {
      GNOME_PersistFile_load(persist, real_nav_ctx->actual_uri, &ev);
      GNOME_Unknown_unref(persist, &ev);
      CORBA_Object_release(persist, &ev);
    }
  else if((persist = gnome_object_client_query_interface(view->client_object, "IDL:GNOME/PersistStream:1.0",
                                                         NULL))
          && !CORBA_Object_is_nil(persist, &ev))
    {
      GnomeStream *stream;
      
      stream = gnome_stream_fs_open(real_nav_ctx->actual_uri, GNOME_Storage_READ);
      GNOME_PersistStream_load (persist,
                                (GNOME_Stream) gnome_object_corba_objref (GNOME_OBJECT (stream)),
                                &ev);
      GNOME_Unknown_unref(persist, &ev);
      CORBA_Object_release(persist, &ev);
    }
  
  CORBA_exception_free(&ev);
}      

  
static void
nautilus_view_notify_location_change(NautilusView *view,
				     Nautilus_NavigationInfo *nav_context)
{
  Nautilus_NavigationInfo real_nav_ctx;

  real_nav_ctx = *nav_context;
  g_assert(real_nav_ctx.requested_uri);
#define DEFAULT_STRING(x) if(!real_nav_ctx.x) real_nav_ctx.x = ""
  if(!real_nav_ctx.actual_uri) real_nav_ctx.actual_uri = real_nav_ctx.requested_uri;
  DEFAULT_STRING(content_type);

  DEFAULT_STRING(referring_uri);
  if(!real_nav_ctx.actual_referring_uri) real_nav_ctx.actual_referring_uri = real_nav_ctx.referring_uri;
  DEFAULT_STRING(referring_content_type);

  switch(view->type)
    {
    case NV_NAUTILUS_VIEW:
      nautilus_notify_location_change(view, &real_nav_ctx);
      break;
    case NV_BONOBO_SUBDOC:
      bonobo_subdoc_notify_location_change(view, &real_nav_ctx);
      break;
    default:
      g_warning("Unhandled view type %d", view->type);
      break;
    }

}

static void
nautilus_view_notify_selection_change(NautilusView *view,
				      Nautilus_SelectionInfo *nav_context)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);

  g_return_if_fail(view->type == NV_NAUTILUS_VIEW);

  Nautilus_View_notify_selection_change(view->u.nautilus_view_info.view_client, nav_context, &ev);

  CORBA_exception_free(&ev);
}

static void
nautilus_view_load_state(NautilusView *view, const char *config_path)
{
  CORBA_Environment ev;

  g_return_if_fail(view->type == NV_NAUTILUS_VIEW);

  CORBA_exception_init(&ev);

  Nautilus_View_load_state(view->u.nautilus_view_info.view_client, (char *)config_path, &ev);

  CORBA_exception_free(&ev);
}

static void
nautilus_view_save_state(NautilusView *view, const char *config_path)
{
  CORBA_Environment ev;

  g_return_if_fail(view->type == NV_NAUTILUS_VIEW);

  CORBA_exception_init(&ev);

  Nautilus_View_save_state(view->u.nautilus_view_info.view_client, (char *)config_path, &ev);

  CORBA_exception_free(&ev);
}

static void
nautilus_view_show_properties(NautilusView *view)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);

  g_return_if_fail(view->type == NV_NAUTILUS_VIEW);

  Nautilus_View_show_properties(view->u.nautilus_view_info.view_client, &ev);

  CORBA_exception_free(&ev);
}

const char *
nautilus_view_get_iid(NautilusView *view)
{
  return view->iid;
}

CORBA_Object
nautilus_view_get_client_objref(NautilusView *view)
{
  if(view->type == NV_NAUTILUS_VIEW)
    return view->u.nautilus_view_info.view_client;
  else
    return CORBA_OBJECT_NIL;
}

static GnomeObject *
nautilus_get_control_frame(NautilusView *view)
{
  return view->u.nautilus_view_info.control_frame;
}

static GnomeObject *
bonobo_control_get_control_frame(NautilusView *view)
{
  return view->u.bonobo_control_info.control_frame;
}

static GnomeObject *
bonobo_subdoc_get_control_frame(NautilusView *view)
{
  return view->u.bonobo_subdoc_info.view_frame;
}


GnomeObject *
nautilus_view_get_control_frame(NautilusView *view)
{
  switch(view->type)
    {
    case NV_NAUTILUS_VIEW:
      return nautilus_get_control_frame(view);
      break;
    case NV_BONOBO_CONTROL:
      return bonobo_control_get_control_frame(view);
      break;
    case NV_BONOBO_SUBDOC:
      return bonobo_subdoc_get_control_frame(view);
      break;
    default:
      g_warning("Can't get the control frame for this type of view (%d)", view->type);
      break;
    }

  return NULL;
}

CORBA_Object
nautilus_view_get_objref(NautilusView *view)
{
  return gnome_object_corba_objref(view->view_frame);
}
