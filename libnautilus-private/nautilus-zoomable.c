/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
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
 *  Author: Maciej Stachowiak <mjs@eazel.com>
 *
 */

/* nautilus-zoomable.c: FIXME */

#include <config.h>
#include "nautilus-zoomable.h"

#include <libnautilus/nautilus-gtk-macros.h>
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-control.h>

NautilusZoomable *foo;

struct _NautilusZoomablePrivate {
  BonoboObject *control;

  gdouble                   zoom_level;
  gdouble                   min_zoom_level;
  gdouble                   max_zoom_level;
  gboolean                  is_continuous;
  
  Nautilus_ZoomableFrame zoomable_frame;
};

enum {
  SET_ZOOM_LEVEL,
  ZOOM_IN,
  ZOOM_OUT,
  ZOOM_TO_FIT,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_BONOBO_CONTROL,
  ARG_MIN_ZOOM_LEVEL,
  ARG_MAX_ZOOM_LEVEL,
  ARG_IS_CONTINUOUS
};

static guint nautilus_zoomable_signals[LAST_SIGNAL];

typedef struct {
  POA_Nautilus_View servant;
  gpointer bonobo_object;

  NautilusZoomable *gtk_object;
} impl_POA_Nautilus_Zoomable;


void nautilus_zoomable_real_set_bonobo_control (NautilusZoomable *view,
						BonoboObject     *bonobo_control);

static CORBA_double  impl_Nautilus_Zoomable__get_zoom_level      (impl_POA_Nautilus_Zoomable *servant,
								  CORBA_Environment          *ev);
static void          impl_Nautilus_Zoomable__set_zoom_level      (impl_POA_Nautilus_Zoomable *servant,
								  const CORBA_double          zoom_level,
								  CORBA_Environment          *ev);
static CORBA_double  impl_Nautilus_Zoomable__get_min_zoom_level  (impl_POA_Nautilus_Zoomable *servant,
								  CORBA_Environment          *ev);
static CORBA_double  impl_Nautilus_Zoomable__get_max_zoom_level  (impl_POA_Nautilus_Zoomable *servant,
								  CORBA_Environment          *ev);
static CORBA_boolean impl_Nautilus_Zoomable__get_is_continuous   (impl_POA_Nautilus_Zoomable *servant,
								  CORBA_Environment          *ev);
static void          impl_Nautilus_Zoomable_zoom_in              (impl_POA_Nautilus_Zoomable *servant,
								  CORBA_Environment          *ev);
static void          impl_Nautilus_Zoomable_zoom_out             (impl_POA_Nautilus_Zoomable *servant,
								  CORBA_Environment          *ev);
static void          impl_Nautilus_Zoomable_zoom_to_fit          (impl_POA_Nautilus_Zoomable *servant,
								  CORBA_Environment          *ev);


POA_Nautilus_Zoomable__epv libnautilus_Nautilus_Zoomable_epv =
{
  NULL,			/* _private */
  (gpointer) &impl_Nautilus_Zoomable__get_zoom_level,
  (gpointer) &impl_Nautilus_Zoomable__set_zoom_level,
  (gpointer) &impl_Nautilus_Zoomable__get_min_zoom_level,
  (gpointer) &impl_Nautilus_Zoomable__get_max_zoom_level,
  (gpointer) &impl_Nautilus_Zoomable__get_is_continuous,
  (gpointer) &impl_Nautilus_Zoomable_zoom_in,
  (gpointer) &impl_Nautilus_Zoomable_zoom_out,
  (gpointer) &impl_Nautilus_Zoomable_zoom_to_fit
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_Zoomable__vepv impl_Nautilus_Zoomable_vepv =
{
  &base_epv,
  NULL,
  &libnautilus_Nautilus_Zoomable_epv
};

static CORBA_double
impl_Nautilus_Zoomable__get_zoom_level (impl_POA_Nautilus_Zoomable *servant,
					CORBA_Environment          *ev)
{
  return servant->gtk_object->private->zoom_level;
}

static void 
impl_Nautilus_Zoomable__set_zoom_level (impl_POA_Nautilus_Zoomable *servant,
					const CORBA_double          zoom_level,
					CORBA_Environment          *ev)
{
  gtk_signal_emit (GTK_OBJECT (servant->gtk_object), nautilus_zoomable_signals[SET_ZOOM_LEVEL], zoom_level);
  
}

static CORBA_double
impl_Nautilus_Zoomable__get_min_zoom_level (impl_POA_Nautilus_Zoomable *servant,
					    CORBA_Environment      *ev)
{
  return servant->gtk_object->private->min_zoom_level;
}
static CORBA_double
impl_Nautilus_Zoomable__get_max_zoom_level (impl_POA_Nautilus_Zoomable *servant,
					    CORBA_Environment      *ev)
{
  return servant->gtk_object->private->max_zoom_level;
}

static CORBA_boolean
impl_Nautilus_Zoomable__get_is_continuous (impl_POA_Nautilus_Zoomable *servant,
					   CORBA_Environment      *ev)
{
  return servant->gtk_object->private->is_continuous;
}

static void
impl_Nautilus_Zoomable_zoom_in (impl_POA_Nautilus_Zoomable *servant,
				CORBA_Environment          *ev)
{
  gtk_signal_emit (GTK_OBJECT (servant->gtk_object), nautilus_zoomable_signals[ZOOM_IN]);
}

static void
impl_Nautilus_Zoomable_zoom_out (impl_POA_Nautilus_Zoomable *servant,
				 CORBA_Environment          *ev)
{
  gtk_signal_emit (GTK_OBJECT (servant->gtk_object), nautilus_zoomable_signals[ZOOM_OUT]);
}

static void
impl_Nautilus_Zoomable_zoom_to_fit (impl_POA_Nautilus_Zoomable *servant,
				    CORBA_Environment      *ev)
{
  gtk_signal_emit (GTK_OBJECT (servant->gtk_object), nautilus_zoomable_signals[ZOOM_TO_FIT]);
}

static void
impl_Nautilus_Zoomable__destroy(BonoboObject *obj, impl_POA_Nautilus_Zoomable *servant)
{
  PortableServer_ObjectId *objid;
  CORBA_Environment ev;
  void (*servant_destroy_func) (PortableServer_Servant servant, CORBA_Environment *ev);

  CORBA_exception_init(&ev);

  servant_destroy_func = NAUTILUS_ZOOMABLE_CLASS (GTK_OBJECT (servant->gtk_object)->klass)->servant_destroy_func;
  objid = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
  PortableServer_POA_deactivate_object (bonobo_poa (), objid, &ev);
  CORBA_free (objid);
  obj->servant = NULL;

  servant_destroy_func ((PortableServer_Servant) servant, &ev);
  g_free (servant);
  CORBA_exception_free(&ev);
}

static Nautilus_Zoomable
impl_Nautilus_Zoomable__create(NautilusZoomable *zoomable, CORBA_Environment * ev)
{
  Nautilus_Zoomable retval;
  impl_POA_Nautilus_Zoomable *servant;
  void (*servant_init_func) (PortableServer_Servant servant, CORBA_Environment *ev);

  NautilusZoomableClass *zoomable_class = NAUTILUS_ZOOMABLE_CLASS (GTK_OBJECT(zoomable)->klass);

  servant_init_func = zoomable_class->servant_init_func;
  servant = g_new0 (impl_POA_Nautilus_Zoomable, 1);
  servant->servant.vepv = zoomable_class->vepv;
  if (!servant->servant.vepv->Bonobo_Unknown_epv)
    servant->servant.vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
  servant_init_func ((PortableServer_Servant) servant, ev);

  servant->gtk_object = zoomable;

  retval = bonobo_object_activate_servant (BONOBO_OBJECT (zoomable), servant);

  gtk_signal_connect (GTK_OBJECT (zoomable), "destroy", GTK_SIGNAL_FUNC (impl_Nautilus_Zoomable__destroy), servant);

  return retval;
}


typedef gboolean (*GtkSignal_NONE__DOUBLE) (GtkObject * object,
					    gdouble arg1,
					    gpointer user_data);

static void
gtk_marshal_NONE__DOUBLE (GtkObject * object,
			  GtkSignalFunc func,
			  gpointer func_data, GtkArg * args)
{
  GtkSignal_NONE__DOUBLE rfunc;
  rfunc = (GtkSignal_NONE__DOUBLE) func;
  (*rfunc) (object,
	    GTK_VALUE_DOUBLE (args[0]), func_data);
}


static void nautilus_zoomable_initialize_class (NautilusZoomableClass *klass);
static void nautilus_zoomable_initialize       (NautilusZoomable      *zoomable);
static void nautilus_zoomable_destroy          (NautilusZoomable      *view);
static void nautilus_zoomable_set_arg          (GtkObject             *object,
						GtkArg                *arg,
						guint	               arg_id);
static void nautilus_zoomable_get_arg          (GtkObject             *object,
						GtkArg                *arg,
						guint	               arg_id);


NAUTILUS_DEFINE_CLASS_BOILERPLATE(NautilusZoomable, nautilus_zoomable, BONOBO_OBJECT_TYPE)



static void
nautilus_zoomable_initialize_class (NautilusZoomableClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;
  object_class->destroy = (void (*)(GtkObject*))nautilus_zoomable_destroy;
  object_class->set_arg = nautilus_zoomable_set_arg;
  object_class->get_arg = nautilus_zoomable_get_arg;

  parent_class = gtk_type_class (gtk_type_parent (object_class->type));

  klass->servant_init_func = POA_Nautilus_Zoomable__init;
  klass->servant_destroy_func = POA_Nautilus_Zoomable__fini;
  klass->vepv = &impl_Nautilus_Zoomable_vepv;

  nautilus_zoomable_signals[SET_ZOOM_LEVEL] =
    gtk_signal_new("set_zoom_level",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusZoomableClass, set_zoom_level),
		   gtk_marshal_NONE__DOUBLE,
		   GTK_TYPE_NONE, 1, GTK_TYPE_DOUBLE);
  nautilus_zoomable_signals[ZOOM_IN] = 
    gtk_signal_new("zoom_in",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusZoomableClass, zoom_in),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  nautilus_zoomable_signals[ZOOM_OUT] = 
    gtk_signal_new("zoom_out",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusZoomableClass, zoom_out),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  nautilus_zoomable_signals[ZOOM_TO_FIT] = 
    gtk_signal_new("zoom_to_fit",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET (NautilusZoomableClass, zoom_to_fit),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  gtk_object_class_add_signals (object_class, nautilus_zoomable_signals, LAST_SIGNAL);

  gtk_object_add_arg_type ("NautilusZoomable::bonobo_control",
			   GTK_TYPE_OBJECT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT | GTK_ARG_CONSTRUCT_ONLY,
			   ARG_BONOBO_CONTROL);
  gtk_object_add_arg_type ("NautilusZoomable::min_zoom_level",
			   GTK_TYPE_DOUBLE,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT | GTK_ARG_CONSTRUCT_ONLY,
			   ARG_MIN_ZOOM_LEVEL);
  gtk_object_add_arg_type ("NautilusZoomable::max_zoom_level",
			   GTK_TYPE_DOUBLE,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT | GTK_ARG_CONSTRUCT_ONLY,
			   ARG_MAX_ZOOM_LEVEL);
  gtk_object_add_arg_type ("NautilusZoomable::is_continuous",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT | GTK_ARG_CONSTRUCT_ONLY,
			   ARG_IS_CONTINUOUS);
}

static void
nautilus_zoomable_set_arg (GtkObject      *object,
			   GtkArg         *arg,
			   guint	   arg_id)
{
  NautilusZoomable *zoomable;

  zoomable = NAUTILUS_ZOOMABLE (object);

  switch(arg_id) {
  case ARG_BONOBO_CONTROL:
    nautilus_zoomable_real_set_bonobo_control (zoomable, 
					       (BonoboObject *) GTK_VALUE_OBJECT (*arg));
  case ARG_MIN_ZOOM_LEVEL:
    zoomable->private->min_zoom_level = GTK_VALUE_DOUBLE (*arg);
  case ARG_MAX_ZOOM_LEVEL:
    zoomable->private->max_zoom_level = GTK_VALUE_DOUBLE (*arg);
  case ARG_IS_CONTINUOUS:
    zoomable->private->is_continuous = GTK_VALUE_BOOL (*arg);
  }
}

static void
nautilus_zoomable_get_arg (GtkObject      *object,
			     GtkArg         *arg,
			     guint	     arg_id)
{
  NautilusZoomable *view;

  view = NAUTILUS_ZOOMABLE (object);

  switch(arg_id) {
  case ARG_BONOBO_CONTROL:
    GTK_VALUE_OBJECT (*arg) = GTK_OBJECT (nautilus_zoomable_get_bonobo_control (NAUTILUS_ZOOMABLE (object)));
  case ARG_MIN_ZOOM_LEVEL:
    GTK_VALUE_DOUBLE (*arg) = NAUTILUS_ZOOMABLE (object)->private->min_zoom_level;
  case ARG_MAX_ZOOM_LEVEL:
    GTK_VALUE_DOUBLE (*arg) = NAUTILUS_ZOOMABLE (object)->private->max_zoom_level;
  case ARG_IS_CONTINUOUS:
    GTK_VALUE_DOUBLE (*arg) = NAUTILUS_ZOOMABLE (object)->private->is_continuous;
  }
}

static void
nautilus_zoomable_initialize (NautilusZoomable *zoomable)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);

  zoomable->private = g_new0 (NautilusZoomablePrivate, 1);

  bonobo_object_construct (BONOBO_OBJECT (zoomable), impl_Nautilus_Zoomable__create (zoomable, &ev));

  CORBA_exception_free(&ev);
}

NautilusZoomable *
nautilus_zoomable_new (GtkWidget *widget, 
		       gdouble min_zoom_level, 
		       gdouble max_zoom_level, 
		       gboolean is_continuous)
{
  BonoboObject *control;

  control = BONOBO_OBJECT (bonobo_control_new (widget));

  return nautilus_zoomable_new_from_bonobo_control (control,
						    min_zoom_level, 
						    max_zoom_level, 
						    is_continuous);
}

NautilusZoomable *
nautilus_zoomable_new_from_bonobo_control (BonoboObject *bonobo_control,
					   gdouble min_zoom_level, 
					   gdouble max_zoom_level, 
					   gboolean is_continuous)
{
  NautilusZoomable *zoomable;
  
  zoomable = NAUTILUS_ZOOMABLE (gtk_object_new (NAUTILUS_TYPE_ZOOMABLE,
						"bonobo_control", bonobo_control,
						"min_zoom_level", min_zoom_level,
						"max_zoom_level", max_zoom_level,
						"is_continuous",  is_continuous,
						NULL));

  return zoomable;
}

static void
nautilus_zoomable_destroy (NautilusZoomable *view)
{
  g_free (view->private);

  NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, GTK_OBJECT (view));
}

static gboolean
nautilus_zoomable_ensure_zoomable_frame (NautilusZoomable *view)
{
  CORBA_Environment ev;

  g_assert (view != NULL);
  g_assert (NAUTILUS_IS_ZOOMABLE (view));

  CORBA_exception_init (&ev);

  if (CORBA_Object_is_nil (view->private->zoomable_frame, &ev))
    view->private->zoomable_frame = Bonobo_Unknown_query_interface 
      (bonobo_control_get_control_frame 
       (BONOBO_CONTROL (nautilus_zoomable_get_bonobo_control (view))),
       "IDL:Nautilus/ZoomableFrame:1.0", &ev);

  if (ev._major != CORBA_NO_EXCEPTION)
    view->private->zoomable_frame = CORBA_OBJECT_NIL;

  if (CORBA_Object_is_nil (view->private->zoomable_frame, &ev)) {
    CORBA_exception_free (&ev);
    return FALSE;
  } else {
    CORBA_exception_free (&ev);
    return TRUE;
  }
}

void
nautilus_zoomable_set_zoom_level (NautilusZoomable *view,
				  gdouble           zoom_level)
{
  CORBA_Environment ev;

  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_ZOOMABLE (view));

  CORBA_exception_init (&ev);

  view->private->zoom_level = zoom_level;

  if (nautilus_zoomable_ensure_zoomable_frame (view)) {
    Nautilus_ZoomableFrame_notify_zoom_level (view->private->zoomable_frame, zoom_level, &ev);
    if(ev._major != CORBA_NO_EXCEPTION)
      {
	CORBA_Object_release(view->private->zoomable_frame, &ev);
	view->private->zoomable_frame = CORBA_OBJECT_NIL;
      }
  }
  
  CORBA_exception_free (&ev);
}

BonoboObject *
nautilus_zoomable_get_bonobo_control (NautilusZoomable *view)
{
  return view->private->control;
}

void
nautilus_zoomable_real_set_bonobo_control (NautilusZoomable *view,
					     BonoboObject      *bonobo_control)
{
  CORBA_Environment ev;

  CORBA_exception_init(&ev);

  /* FIXME: what if this fails? Create a new control, or bomb somehow? */
  view->private->control = bonobo_object_query_local_interface (bonobo_control, "IDL:Bonobo/Control:1.0");

  /* We don't need the extra ref the above creates. */
  bonobo_object_unref (view->private->control); 

  bonobo_object_add_interface (BONOBO_OBJECT (view), view->private->control);

  CORBA_exception_free(&ev);
}
