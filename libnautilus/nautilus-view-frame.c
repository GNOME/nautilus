/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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

/* nautilus-view-frame.c: Implementation for object that represents a
   nautilus view implementation. */

#include <config.h>
#include "nautilus-view-frame.h"

#include <gtk/gtksignal.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-control.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-undo-context.h>
#include <libnautilus/nautilus-undo-manager.h>

enum {
	NOTIFY_LOCATION_CHANGE,
	NOTIFY_SELECTION_CHANGE,
	LOAD_STATE,
	SAVE_STATE,
	SHOW_PROPERTIES,
	STOP_LOCATION_CHANGE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct NautilusViewDetails {
	BonoboControl *control;
};

typedef struct {
	POA_Nautilus_View servant;
	gpointer bonobo_object;
	
	NautilusView *view;
} impl_POA_Nautilus_View;

static void impl_Nautilus_View_save_state              (PortableServer_Servant         servant,
							const CORBA_char              *config_path,
							CORBA_Environment             *ev);
static void impl_Nautilus_View_load_state              (PortableServer_Servant         servant,
							const CORBA_char              *config_path,
							CORBA_Environment             *ev);
static void impl_Nautilus_View_notify_location_change  (PortableServer_Servant         servant,
							const Nautilus_NavigationInfo *navinfo,
							CORBA_Environment             *ev);
static void impl_Nautilus_View_stop_location_change    (PortableServer_Servant         servant,
							CORBA_Environment             *ev);
static void impl_Nautilus_View_notify_selection_change (PortableServer_Servant         servant,
							const Nautilus_SelectionInfo  *selinfo,
							CORBA_Environment             *ev);
static void impl_Nautilus_View_show_properties         (PortableServer_Servant         servant,
							CORBA_Environment             *ev);
static void nautilus_view_initialize                   (NautilusView                  *view);
static void nautilus_view_destroy                      (NautilusView                  *view);
static void nautilus_view_initialize_class             (NautilusViewClass             *klass);

POA_Nautilus_View__epv libnautilus_Nautilus_View_epv =
{
	NULL,			/* _private */
	&impl_Nautilus_View_save_state,
	&impl_Nautilus_View_load_state,
	&impl_Nautilus_View_notify_location_change,
	&impl_Nautilus_View_stop_location_change,
	&impl_Nautilus_View_notify_selection_change,
	&impl_Nautilus_View_show_properties
};

static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_View__vepv impl_Nautilus_View_vepv =
{
	&base_epv,
	NULL,
	&libnautilus_Nautilus_View_epv
};

static void
impl_Nautilus_View_save_state (PortableServer_Servant servant,
			       const CORBA_char *config_path,
			       CORBA_Environment *ev)
{
	gtk_signal_emit (GTK_OBJECT (((impl_POA_Nautilus_View *) servant)->view),
			 signals[SAVE_STATE], config_path);
}

static void
impl_Nautilus_View_load_state (PortableServer_Servant servant,
			       const CORBA_char *config_path,
			       CORBA_Environment *ev)
{
	gtk_signal_emit (GTK_OBJECT (((impl_POA_Nautilus_View *) servant)->view),
			 signals[LOAD_STATE], config_path);
}

static void
impl_Nautilus_View_notify_location_change (PortableServer_Servant servant,
					   const Nautilus_NavigationInfo *navinfo,
					   CORBA_Environment *ev)
{
	gtk_signal_emit (GTK_OBJECT (((impl_POA_Nautilus_View *) servant)->view),
			 signals[NOTIFY_LOCATION_CHANGE], navinfo);
}

static void
impl_Nautilus_View_show_properties (PortableServer_Servant servant,
				    CORBA_Environment *ev)
{
	gtk_signal_emit (GTK_OBJECT (((impl_POA_Nautilus_View *) servant)->view),
			 signals[SHOW_PROPERTIES]);
}

static void
impl_Nautilus_View_notify_selection_change (PortableServer_Servant servant,
					    const Nautilus_SelectionInfo *selinfo,
					    CORBA_Environment *ev)
{
	gtk_signal_emit (GTK_OBJECT (((impl_POA_Nautilus_View *) servant)->view),
			 signals[NOTIFY_SELECTION_CHANGE], selinfo);
}

static void
impl_Nautilus_View_stop_location_change (PortableServer_Servant servant,
					 CORBA_Environment *ev)
{
	gtk_signal_emit (GTK_OBJECT (((impl_POA_Nautilus_View *) servant)->view),
			 signals[STOP_LOCATION_CHANGE]);
}

static void
impl_Nautilus_View__destroy (BonoboObject *obj, impl_POA_Nautilus_View *servant)
{
	PortableServer_ObjectId *objid;
	CORBA_Environment ev;
	void (* servant_destroy_func) (PortableServer_Servant servant, CORBA_Environment *ev);
	
	CORBA_exception_init(&ev);
	
	servant_destroy_func = NAUTILUS_VIEW_CLASS (GTK_OBJECT (servant->view)->klass)->servant_destroy_func;
	objid = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
	PortableServer_POA_deactivate_object (bonobo_poa (), objid, &ev);
	CORBA_free (objid);
	obj->servant = NULL;
	
	servant_destroy_func ((PortableServer_Servant) servant, &ev);
	g_free (servant);
	CORBA_exception_free(&ev);
}

static Nautilus_ViewFrame
impl_Nautilus_View__create (NautilusView *view, CORBA_Environment * ev)
{
	Nautilus_ViewFrame retval;
	
	impl_POA_Nautilus_View *newservant;
	void (*servant_init_func) (PortableServer_Servant servant, CORBA_Environment *ev);
	NautilusViewClass *view_class = NAUTILUS_VIEW_CLASS (GTK_OBJECT(view)->klass);
	
	servant_init_func = view_class->servant_init_func;
	newservant = g_new0 (impl_POA_Nautilus_View, 1);
	newservant->servant.vepv = view_class->vepv;
	if (!newservant->servant.vepv->Bonobo_Unknown_epv)
		newservant->servant.vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	servant_init_func ((PortableServer_Servant) newservant, ev);
	
	newservant->view = view;
	
	retval = bonobo_object_activate_servant (BONOBO_OBJECT (view), newservant);
	
	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    GTK_SIGNAL_FUNC (impl_Nautilus_View__destroy), newservant);
	
	return retval;
}

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusView, nautilus_view, BONOBO_OBJECT_TYPE)

static void
nautilus_view_initialize_class (NautilusViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*) klass;
	object_class->destroy = (void (*)(GtkObject *)) nautilus_view_destroy;
	
	klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
	klass->servant_init_func = POA_Nautilus_View__init;
	klass->servant_destroy_func = POA_Nautilus_View__fini;
	klass->vepv = &impl_Nautilus_View_vepv;
	
	signals[NOTIFY_LOCATION_CHANGE] =
		gtk_signal_new ("notify_location_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusViewClass, notify_location_change),
			       gtk_marshal_NONE__BOXED,
			       GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
	signals[NOTIFY_SELECTION_CHANGE] =
		gtk_signal_new ("notify_selection_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusViewClass, notify_selection_change),
			       gtk_marshal_NONE__BOXED,
			       GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
	signals[LOAD_STATE] =
		gtk_signal_new ("load_state",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusViewClass, load_state),
			       gtk_marshal_NONE__STRING,
			       GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[SAVE_STATE] =
		gtk_signal_new ("save_state",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewClass, save_state),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[SHOW_PROPERTIES] =
		gtk_signal_new ("show_properties",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewClass, show_properties),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[STOP_LOCATION_CHANGE] =
		gtk_signal_new ("stop_location_change",
			        GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewClass, stop_location_change),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_view_initialize (NautilusView *view)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	view->details = g_new0 (NautilusViewDetails, 1);
	
	bonobo_object_construct
		(BONOBO_OBJECT (view),
		 impl_Nautilus_View__create (view, &ev));
	
	CORBA_exception_free (&ev);
}

NautilusView *
nautilus_view_new (GtkWidget *widget)
{
	return nautilus_view_new_from_bonobo_control
		(bonobo_control_new (widget));
}

NautilusView *
nautilus_view_new_from_bonobo_control (BonoboControl *control)
{
	NautilusView *view;
	
	view = NAUTILUS_VIEW (gtk_type_new (NAUTILUS_TYPE_VIEW));
	view->details->control = control;
	bonobo_object_add_interface (BONOBO_OBJECT (view), BONOBO_OBJECT (control));
	nautilus_undo_set_up_bonobo_control (control);

	return view;
}

static void
nautilus_view_destroy (NautilusView *view)
{
	NautilusViewClass *klass;
	
	klass = NAUTILUS_VIEW_CLASS (GTK_OBJECT (view)->klass);
	
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, GTK_OBJECT (view));
}

static Nautilus_ViewFrame
get_view_frame (NautilusView *view, CORBA_Environment *ev)
{
	return Bonobo_Unknown_query_interface 
		(bonobo_control_get_control_frame (nautilus_view_get_bonobo_control (view)),
		 "IDL:Nautilus/ViewFrame:1.0", ev);
}

void
nautilus_view_request_location_change (NautilusView *view,
				       Nautilus_NavigationRequestInfo *request)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	CORBA_exception_init (&ev);
	
	view_frame = get_view_frame (view, &ev);
	Nautilus_ViewFrame_request_location_change (view_frame, request, &ev);
	CORBA_Object_release (view_frame, &ev);
	
	CORBA_exception_free (&ev);
}

void
nautilus_view_request_selection_change (NautilusView *view,
					Nautilus_SelectionRequestInfo *request)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	CORBA_exception_init (&ev);
	
	view_frame = get_view_frame (view, &ev);
	Nautilus_ViewFrame_request_selection_change (view_frame, request, &ev);
	CORBA_Object_release (view_frame, &ev);
	
	CORBA_exception_free(&ev);
}

void
nautilus_view_request_status_change (NautilusView *view,
				     Nautilus_StatusRequestInfo *request)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	CORBA_exception_init (&ev);
	
	view_frame = get_view_frame (view, &ev);
	Nautilus_ViewFrame_request_status_change (view_frame, request, &ev);
	CORBA_Object_release (view_frame, &ev);
	
	CORBA_exception_free (&ev);
}

void
nautilus_view_request_progress_change(NautilusView *view,
				      Nautilus_ProgressRequestInfo *request)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	CORBA_exception_init (&ev);
	
	view_frame = get_view_frame (view, &ev);
	Nautilus_ViewFrame_request_progress_change (view_frame, request, &ev);
	CORBA_Object_release (view_frame, &ev);
	
	CORBA_exception_free (&ev);
}


void
nautilus_view_request_title_change (NautilusView *view,
				    const char *new_title)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	CORBA_exception_init (&ev);
	
	view_frame = get_view_frame (view, &ev);
	Nautilus_ViewFrame_request_title_change (view_frame, new_title, &ev);
	CORBA_Object_release (view_frame, &ev);
	
	CORBA_exception_free (&ev);
}

BonoboControl *
nautilus_view_get_bonobo_control (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	bonobo_object_ref (BONOBO_OBJECT (view->details->control));
	return view->details->control;
}

CORBA_Object
nautilus_view_get_main_window (NautilusView *view)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	Nautilus_ViewWindow window;

	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	view_frame = get_view_frame (view, &ev);
	window = Nautilus_ViewFrame__get_main_window (view_frame, &ev);
	CORBA_Object_release (view_frame, &ev);
	
	CORBA_exception_free (&ev);

	return window;
}
