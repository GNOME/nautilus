/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
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
#include "nautilus-history-frame.h"
#include "nautilus-window.h"
#include "nautilus-component-adapter-factory.h"

#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <libnautilus/nautilus-view.h>
#include <libnautilus/nautilus-zoomable.h>



enum {
	OPEN_LOCATION,
	OPEN_LOCATION_IN_NEW_WINDOW,
	REPORT_SELECTION_CHANGE,
	REPORT_STATUS,
	REPORT_LOAD_UNDERWAY,
	REPORT_LOAD_PROGRESS,
	REPORT_LOAD_COMPLETE,
	REPORT_LOAD_FAILED,
	REPORT_ACTIVATION_COMPLETE,
	TITLE_CHANGED,
	ZOOM_LEVEL_CHANGED,
	CLIENT_GONE,
	GET_HISTORY_LIST,
	LAST_SIGNAL
};

typedef enum {
	VIEW_FRAME_EMPTY,
	VIEW_FRAME_ACTIVATING,
	VIEW_FRAME_NO_LOCATION,
	VIEW_FRAME_WAITING,
	VIEW_FRAME_UNDERWAY,
	VIEW_FRAME_LOADED,
	VIEW_FRAME_FAILED
} NautilusViewFrameState;

struct NautilusViewFrameDetails {
	NautilusViewFrameState state;
	char *title;
	char *label;

	/* A container to connect our clients to. */
	BonoboUIContainer *ui_container;

	guint check_if_view_is_gone_timeout_id;

	char *activation_iid;
	NautilusBonoboActivate *activate_structure;
};

static void nautilus_view_frame_initialize       (NautilusViewFrame      *view);
static void nautilus_view_frame_destroy          (GtkObject              *view);
static void nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass);

static void view_frame_not_activated             (NautilusViewFrame      *view);
static void view_frame_activated                 (NautilusViewFrame      *view);
static void view_frame_stop_activation           (NautilusViewFrame      *view);


static guint signals[LAST_SIGNAL];

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusViewFrame, nautilus_view_frame, NAUTILUS_TYPE_GENEROUS_BIN)

static void
nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*) klass;
	object_class->destroy = nautilus_view_frame_destroy;
	
	signals[OPEN_LOCATION] =
		gtk_signal_new ("open_location",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   open_location),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[OPEN_LOCATION_IN_NEW_WINDOW] =
		gtk_signal_new ("open_location_in_new_window",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   open_location_in_new_window),
				nautilus_gtk_marshal_NONE__STRING_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_STRING, GTK_TYPE_POINTER);
	signals[REPORT_SELECTION_CHANGE] =
		gtk_signal_new ("report_selection_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_selection_change),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[REPORT_STATUS] =
		gtk_signal_new ("report_status",
				GTK_RUN_LAST,
                    object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_status),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[REPORT_LOAD_UNDERWAY] =
		gtk_signal_new ("report_load_underway",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_load_underway),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[REPORT_LOAD_PROGRESS] =
		gtk_signal_new ("report_load_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_load_progress),
				nautilus_gtk_marshal_NONE__DOUBLE,
				GTK_TYPE_NONE, 1, GTK_TYPE_DOUBLE);
	signals[REPORT_LOAD_COMPLETE] =
		gtk_signal_new ("report_load_complete",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_load_complete),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[REPORT_LOAD_FAILED] =
		gtk_signal_new ("report_load_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_load_failed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[REPORT_ACTIVATION_COMPLETE] =
		gtk_signal_new ("report_activation_complete",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_activation_complete),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	signals[TITLE_CHANGED] =
		gtk_signal_new ("title_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   title_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	signals[ZOOM_LEVEL_CHANGED] =
		gtk_signal_new ("zoom_level_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   zoom_level_changed),
				nautilus_gtk_marshal_NONE__DOUBLE,
				GTK_TYPE_NONE, 1, GTK_TYPE_DOUBLE);

	signals[CLIENT_GONE] =
		gtk_signal_new ("client_gone",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   client_gone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	signals[GET_HISTORY_LIST] =
		gtk_signal_new ("get_history_list",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   get_history_list),
				nautilus_gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER, 0);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_view_frame_initialize (NautilusViewFrame *view)
{
	GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);

	view->details = g_new0 (NautilusViewFrameDetails, 1);
}

static void
nautilus_view_frame_destroy_client (NautilusViewFrame *view)
{
	CORBA_Environment ev;

	if (view->iid == NULL) {
		return;
	}
	
	CORBA_exception_init(&ev);
	
	g_free (view->iid);
	view->iid = NULL;
	
	bonobo_object_unref (BONOBO_OBJECT (view->client_object));
	view->client_object = NULL;

	gtk_container_remove (GTK_CONTAINER (view), view->client_widget);
	view->client_widget = NULL;

	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		bonobo_object_release_unref (view->zoomable, &ev);
	}
	view->zoomable = CORBA_OBJECT_NIL;

	bonobo_object_unref (view->view_frame);
	view->view_frame = NULL;
	/* we can NULL those since we just unref'ed them 
	   with the aggregate view frame. */
	view->history_frame = CORBA_OBJECT_NIL;
	view->zoomable_frame = CORBA_OBJECT_NIL;

	CORBA_exception_free (&ev);

	if (view->details->check_if_view_is_gone_timeout_id != 0) {
		g_source_remove (view->details->check_if_view_is_gone_timeout_id);
		view->details->check_if_view_is_gone_timeout_id = 0;
	}
}

static void
nautilus_view_frame_destroy (GtkObject *object)
{
	NautilusViewFrame *frame;

	frame = NAUTILUS_VIEW_FRAME (object);
	
	nautilus_view_frame_destroy_client (frame);

	bonobo_object_unref (BONOBO_OBJECT (frame->details->ui_container));
	g_free (frame->details->title);
	g_free (frame->details->label);
	g_free (frame->details);
	frame->details = NULL;
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_view_frame_handle_client_destroy (GtkWidget *widget, NautilusViewFrame *view)
{
	/* FIXME bugzilla.eazel.com 2455: Is a destroy really sufficient here? Who does the unref? */
	gtk_object_destroy (GTK_OBJECT (view));
}

static void
nautilus_view_frame_handle_client_gone (GtkObject *object,
					CORBA_Object cobject,
					CORBA_Environment *ev,
					NautilusViewFrame *view)
{
	gtk_signal_emit (object, signals[CLIENT_GONE]);
}

NautilusViewFrame *
nautilus_view_frame_new (BonoboUIContainer *ui_container,
                         NautilusUndoManager *undo_manager)
{
	NautilusViewFrame *view_frame;
	
	view_frame = NAUTILUS_VIEW_FRAME (gtk_widget_new (nautilus_view_frame_get_type (), NULL));
	
	bonobo_object_ref (BONOBO_OBJECT (ui_container));
	view_frame->details->ui_container = ui_container;

	view_frame->undo_manager = undo_manager;
	
	return view_frame;
}

/* stimulus: successful load_client call */
static void
view_frame_activating (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));

	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
		view->details->state = VIEW_FRAME_ACTIVATING;
		return;
		break;
	case VIEW_FRAME_ACTIVATING:
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_FAILED:
		g_assert_not_reached ();
		return;
	}

	g_assert_not_reached ();
}


/* stimulus: unsuccessful activated_component call */
static void
view_frame_not_activated (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_ACTIVATING:
		view->details->state = VIEW_FRAME_FAILED;
		return;
		break;
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_FAILED:
		g_assert_not_reached ();
		return;
		break;
	}

	g_assert_not_reached ();
}

/* stimulus: successful activated_component call */
static void
view_frame_activated (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_ACTIVATING:
		view->details->state = VIEW_FRAME_NO_LOCATION;
		return;
		break;
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_FAILED:
		g_assert_not_reached ();
		return;
		break;
	}

	g_assert_not_reached ();
}


/* stimulus: stop activation */
static void
view_frame_stop_activation (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_ACTIVATING:
		view->details->state = VIEW_FRAME_EMPTY;
		return;
		break;
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_FAILED:
		g_assert_not_reached ();
		return;
		break;
	}

	g_assert_not_reached ();
}



/* this corresponds to the load_location call stimulus */
static void
view_frame_wait (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
		/* Darin: Change to state machine? */
		g_warning ("tried to load location in an empty view frame");
		break;
	case VIEW_FRAME_ACTIVATING:
		view->details->state = VIEW_FRAME_FAILED;
		break;
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
		view->details->state = VIEW_FRAME_WAITING;
		return;
	case VIEW_FRAME_WAITING:
		return;
	case VIEW_FRAME_FAILED:
		g_assert_not_reached ();
		return;
	}

	g_assert_not_reached ();
}


/* this corresponds to the load_underway and load_progress stimulus */
static void
view_frame_underway (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_FAILED:
	case VIEW_FRAME_ACTIVATING:
		g_assert_not_reached ();
		return;
	case VIEW_FRAME_NO_LOCATION:
		g_warning ("got signal from a view frame with no location");
		return;
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_LOADED:
		gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOAD_UNDERWAY]);
		view->details->state = VIEW_FRAME_UNDERWAY;
		return;
	case VIEW_FRAME_UNDERWAY:
		return;
	}

	g_assert_not_reached ();
}

/* stimulus 
   - open_location call from component
   - open_location_in_new_window
   - report_selection_change
   - report_status
   - set_title 
*/
static void
view_frame_wait_is_over (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_FAILED:
	case VIEW_FRAME_ACTIVATING:
		g_assert_not_reached ();
		return;
	case VIEW_FRAME_NO_LOCATION:
		g_warning ("got signal from a view frame with no location");
		return;
	case VIEW_FRAME_WAITING:
		view_frame_underway (view);
		return;
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
		return;
	}

	g_assert_not_reached ();
}


/* stimulus: report_load_complete */

static void
view_frame_loaded (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_FAILED:
	case VIEW_FRAME_ACTIVATING:
		g_assert_not_reached ();
		return;
	case VIEW_FRAME_NO_LOCATION:
		g_warning ("got signal from a view frame with no location");
		return;
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_UNDERWAY:
		gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOAD_COMPLETE]);
		view->details->state = VIEW_FRAME_LOADED;
		return;
	case VIEW_FRAME_LOADED:
		return;
	}

	g_assert_not_reached ();
}

/* stimulus: report_load_failed */

static void
view_frame_failed (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_FAILED:
		g_assert_not_reached ();
		return;
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_ACTIVATING:
		g_warning ("got signal from a view frame with no location");
		return;
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
		gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOAD_FAILED]);
		view->details->state = VIEW_FRAME_FAILED;
		return;
	}

	g_assert_not_reached ();
}

static gboolean
check_if_view_is_gone (gpointer data)
{
	NautilusViewFrame *view;
	CORBA_Environment ev;
	gboolean ok;

	view = NAUTILUS_VIEW_FRAME (data);

	CORBA_exception_init (&ev);
	ok = TRUE;
	if (CORBA_Object_non_existent (bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object)), &ev)) {

		/* FIXME bugzilla.eazel.com 1840: Is a destroy really sufficient here? Who does the unref? 
		 * See bug 1840 for one bad case this destroy is involved in.
		 */
		/* view->details->check_if_view_is_gone_timeout_id used to be
		 * set to zero here, but that's not necessary with the destroy.
		 * If we change the destroy to something else later we might want
		 * to bring that line back.
		 */
		gtk_object_destroy (GTK_OBJECT (view));

		ok = FALSE;
	}
	CORBA_exception_free (&ev);
	
	return ok;
}

static gboolean
nautilus_view_frame_set_to_component (NautilusViewFrame *view, BonoboObjectClient *component)
{
	CORBA_Environment ev;
	Nautilus_View adapted;
	Bonobo_Control control;
	BonoboControlFrame *control_frame;
	

	/* Either create an adapter or query for the Nautilus:View
	 * interface. Either way, we don't need to keep the original
	 * reference around once that happens.
	 */

	adapted = nautilus_component_adapter_factory_create_adapter 
		(nautilus_component_adapter_factory_get (),
		 component);
	bonobo_object_unref (BONOBO_OBJECT (component));

	/* Handle case where we don't know how to host this component. */
  	if (adapted == CORBA_OBJECT_NIL) {
      		return FALSE;
    	}

	nautilus_view_frame_destroy_client (view);

	CORBA_exception_init (&ev);

	/* Store the object away. */
	view->client_object = bonobo_object_client_from_corba (adapted);
	g_assert (!CORBA_Object_non_existent (adapted, &ev));


	/* Get at our client's interfaces. */
	control = bonobo_object_query_interface
		(BONOBO_OBJECT (view->client_object),
		 "IDL:Bonobo/Control:1.0");
	g_assert (control != CORBA_OBJECT_NIL);
	view->zoomable = bonobo_object_query_interface
		(BONOBO_OBJECT (view->client_object), 
		 "IDL:Nautilus/Zoomable:1.0");
	
	/* Start with a view frame interface. */
	view->view_frame = impl_Nautilus_ViewFrame__create (view, &ev);

	/* Add a control frame interface. */
	control_frame = bonobo_control_frame_new (bonobo_object_corba_objref (BONOBO_OBJECT (view->details->ui_container)));
	bonobo_object_add_interface (BONOBO_OBJECT (view->view_frame), 
	                             BONOBO_OBJECT (control_frame));
	bonobo_control_frame_bind_to_control (control_frame, control);
	view->client_widget = bonobo_control_frame_get_widget (control_frame);

	/* Add a zoomable frame interface. */
	view->zoomable_frame = impl_Nautilus_ZoomableFrame__create (view, &ev);
	bonobo_object_add_interface (BONOBO_OBJECT (view->view_frame), 
	                             BONOBO_OBJECT (view->zoomable_frame));

	/* Add a history frame interface. */
	view->history_frame = impl_Nautilus_HistoryFrame__create (view, &ev);
	bonobo_object_add_interface (BONOBO_OBJECT (view->view_frame), 
	                             BONOBO_OBJECT (view->history_frame));

	/* Add an undo context interface. */
	nautilus_undo_manager_add_interface
        	(view->undo_manager, BONOBO_OBJECT (view->view_frame));
	
	bonobo_object_release_unref (control, NULL);
	
	view->iid = g_strdup (view->details->activation_iid);

	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "destroy",
		 GTK_SIGNAL_FUNC (nautilus_view_frame_handle_client_destroy), view,
		 GTK_OBJECT (view));
	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "object_gone",
		 GTK_SIGNAL_FUNC (nautilus_view_frame_handle_client_gone), view,
		 GTK_OBJECT (view));
	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "system_exception",
		 GTK_SIGNAL_FUNC (nautilus_view_frame_handle_client_gone), view,
		 GTK_OBJECT (view));
	gtk_container_add (GTK_CONTAINER (view), view->client_widget);
	gtk_widget_show (view->client_widget);
	CORBA_exception_free (&ev);

	/* FIXME bugzilla.eazel.com 2456: 
	 * Is a hard-coded timeout acceptable? 
	 */
	g_assert (view->details->check_if_view_is_gone_timeout_id == 0);
	view->details->check_if_view_is_gone_timeout_id
		= g_timeout_add (10000, check_if_view_is_gone, view);

	return TRUE;
}


static void
activation_callback (CORBA_Object object_reference, gpointer data)
{
	NautilusViewFrame *view;
	BonoboObjectClient *bonobo_object;

	view = (NautilusViewFrame *) data;

	bonobo_object = bonobo_object_client_from_corba (object_reference);
	nautilus_view_frame_set_to_component (view, bonobo_object);

	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_ACTIVATION_COMPLETE],
			 bonobo_object);

}



void
nautilus_view_frame_load_client_async (NautilusViewFrame *view, 
				       const char *iid)
{
	NautilusBonoboActivate *activate_structure;

	view_frame_activating (view);
	view->details->activation_iid = g_strdup (iid);
	activate_structure = nautilus_bonobo_activate_from_id (iid, 
							       activation_callback,
							       view);

	view->details->activate_structure = activate_structure;
}



/**
 * I left this function around because I was lazy to make the sidebar activation
 * use the async model in the main state machine... there are 2 reasons for not 
 * doing so:
 * - sidebar components should NOT take long to load.
 * - hacking the state machine might take me as long as it took me to get
 *   the core stuff working so... I am not eager to get into this game.
 *
 * As a consequence, the folowing function does quite a few calls to 
 * the state changing functions to simulate async activation... 
 *
 */
gboolean /* returns TRUE if successful */
nautilus_view_frame_load_client (NautilusViewFrame *view, const char *iid)
{
	BonoboObjectClient *component;
  	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	g_return_val_if_fail (view->details->state == VIEW_FRAME_EMPTY, FALSE);

	if (iid == NULL) {
		return FALSE;
        }

	view_frame_activating (view);
	component = bonobo_object_activate (iid, 0);
	if (component == NULL) {
		view_frame_not_activated (view);
		return FALSE;
        }

	view->details->activation_iid = g_strdup (iid);
	view_frame_activated (view);

	return nautilus_view_frame_set_to_component (view, component);
}

void 
nautilus_view_frame_stop_activation (NautilusViewFrame *view)
{
	nautilus_bonobo_activate_stop (view->details->activate_structure);
	view_frame_stop_activation (view);
	nautilus_bonobo_activate_free (view->details->activate_structure);
	view->details->activate_structure = NULL;
}


static void
set_up_for_new_location (NautilusViewFrame *view)
{
	g_free (view->details->title);
	view->details->title = NULL;
	gtk_signal_emit (GTK_OBJECT (view), signals[TITLE_CHANGED]);
}

void
nautilus_view_frame_load_location (NautilusViewFrame *view,
                                   const char *location)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	set_up_for_new_location (view);
	view_frame_wait (view);
	

	/* ORBit does a bad job with Nautilus_URI, so it's not const char *. */
	CORBA_exception_init (&ev);
	Nautilus_View_load_location (bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object)),
				     (Nautilus_URI) location, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME bugzilla.eazel.com 3454: Self-destruct may not be the best way to indicate an error here. */
		gtk_object_destroy (GTK_OBJECT (view));
	}
	CORBA_exception_free (&ev);
}

void
nautilus_view_frame_stop_loading (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	CORBA_exception_init (&ev);
	Nautilus_View_stop_loading (bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object)),
				    &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME bugzilla.eazel.com 3454: Self-destruct may not be the best way to indicate an error here. */
		gtk_object_destroy (GTK_OBJECT (view));
	}
	CORBA_exception_free (&ev);
}

void
nautilus_view_frame_selection_changed (NautilusViewFrame *view,
                                       GList *selection)
{
	Nautilus_URIList *uri_list;
	
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	uri_list = nautilus_uri_list_from_g_list (selection);
	
	CORBA_exception_init (&ev);
	Nautilus_View_selection_changed (bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object)),
					 uri_list, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME bugzilla.eazel.com 3454: Self-destruct may not be the best way to indicate an error here. */
		gtk_object_destroy (GTK_OBJECT (view));
	}
	CORBA_exception_free (&ev);
	
	CORBA_free (uri_list);
}

void
nautilus_view_frame_title_changed (NautilusViewFrame *view,
				   const char *title)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	CORBA_exception_init (&ev);
	Nautilus_View_title_changed (bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object)),
				     title,
				     &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME bugzilla.eazel.com 3454: Self-destruct may not be the best way to indicate an error here. */
		gtk_object_destroy (GTK_OBJECT (view));
	}
	CORBA_exception_free (&ev);
}

gboolean
nautilus_view_frame_is_zoomable (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	gboolean is_zoomable;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	CORBA_exception_init (&ev);
	is_zoomable = !CORBA_Object_is_nil (view->zoomable, &ev);
	CORBA_exception_free (&ev);
	
	return is_zoomable;
}

gdouble
nautilus_view_frame_get_zoom_level (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	double level;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		level = Nautilus_Zoomable__get_zoom_level (view->zoomable, &ev);
	} else {
		level = 1.0;
	}

	CORBA_exception_free (&ev);
	
	return level;
}

void
nautilus_view_frame_set_zoom_level (NautilusViewFrame *view,
				    double zoom_level)
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
	double level;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		level = Nautilus_Zoomable__get_min_zoom_level (view->zoomable, &ev);
	} else {
		level = 1.0;
	}
	
	CORBA_exception_free (&ev);
	
	return level;
}

double
nautilus_view_frame_get_max_zoom_level (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	double level;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		level = Nautilus_Zoomable__get_max_zoom_level (view->zoomable, &ev);
	} else {
		level = 1.0;
	}
	
	CORBA_exception_free (&ev);
	
	return level;
}

GList *
nautilus_view_frame_get_preferred_zoom_levels (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	GList *levels;
	Nautilus_ZoomLevelList *zoom_levels;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		zoom_levels = Nautilus_Zoomable__get_preferred_zoom_levels (view->zoomable, &ev);
		levels = nautilus_g_list_from_ZoomLevelList (zoom_levels);
		CORBA_free (zoom_levels);
		
	} else {
		levels = NULL;
	}
	
	CORBA_exception_free (&ev);
	
	return levels;
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
nautilus_view_frame_get_iid (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
	return view->iid;
}

void
nautilus_view_frame_open_location (NautilusViewFrame *view,
                                   const char *location)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view), signals[OPEN_LOCATION], location);
}

void
nautilus_view_frame_open_location_in_new_window (NautilusViewFrame *view,
                                                 const char *location,
						 GList *selection)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view), signals[OPEN_LOCATION_IN_NEW_WINDOW],
			 location, selection);
}

void
nautilus_view_frame_report_selection_change (NautilusViewFrame *view,
                                             GList *selection)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_SELECTION_CHANGE], selection);
}

void
nautilus_view_frame_report_status (NautilusViewFrame *view,
                                   const char *status)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_STATUS], status);
}

void
nautilus_view_frame_report_load_underway (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	view_frame_underway (view);
}

void
nautilus_view_frame_report_load_progress (NautilusViewFrame *view,
                                          double fraction_done)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	view_frame_underway (view);
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOAD_PROGRESS], fraction_done);
}

void
nautilus_view_frame_report_load_complete (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	view_frame_loaded (view);
}

void
nautilus_view_frame_report_load_failed (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	view_frame_failed (view);
}

void
nautilus_view_frame_set_title (NautilusViewFrame *view,
                               const char *title)
{
	gboolean changed;

	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	g_return_if_fail (title != NULL);

	/* Only do work if the title actually changed. */
	changed = view->details->title == NULL
		|| strcmp (view->details->title, title) != 0;

	g_free (view->details->title);
	view->details->title = g_strdup (title);

	view_frame_wait_is_over (view);
	if (changed) {
		gtk_signal_emit (GTK_OBJECT (view), signals[TITLE_CHANGED]);
	}
}

char *
nautilus_view_frame_get_title (NautilusViewFrame *view)
{
	return g_strdup (view->details->title);
}

void
nautilus_view_frame_zoom_level_changed (NautilusViewFrame *view,
                                        double level)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view), signals[ZOOM_LEVEL_CHANGED], level);
}

char *
nautilus_view_frame_get_label (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
	g_return_val_if_fail (view->details != NULL, NULL);

	return g_strdup (view->details->label);
}

void
nautilus_view_frame_set_label (NautilusViewFrame *view,
                               const char *label)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	g_free (view->details->label);
	view->details->label = g_strdup (label);
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


Nautilus_HistoryList *
nautilus_view_frame_get_history_list (NautilusViewFrame *view)
{
	Nautilus_HistoryList *history_list;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
	
	/* Sent out signal to get the history list. */
	history_list = NULL;
	gtk_signal_emit (GTK_OBJECT (view), 
  			 signals[GET_HISTORY_LIST],
			 &history_list);
  	return history_list;
}









