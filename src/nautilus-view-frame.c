/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */

/* nautilus-view-frame.c: Widget and CORBA machinery for hosting a NautilusView */

#include <config.h>
#include "nautilus-view-frame.h"

#include "nautilus-application.h"
#include "nautilus-component-adapter-factory.h"
#include "nautilus-signaller.h"
#include "nautilus-view-frame-private.h"
#include "nautilus-window.h"
#include <bonobo/bonobo-zoomable-frame.h>
#include <bonobo/bonobo-zoomable.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>
#include <libnautilus/nautilus-idle-queue.h>
#include <libnautilus/nautilus-view.h>

enum {
	CHANGE_SELECTION,
	CHANGE_STATUS,
	FAILED,
	GET_HISTORY_LIST,
	GO_BACK,
	LOAD_COMPLETE,
	LOAD_PROGRESS_CHANGED,
	LOAD_UNDERWAY,
	OPEN_LOCATION_FORCE_NEW_WINDOW,
	OPEN_LOCATION_IN_THIS_WINDOW,
	OPEN_LOCATION_PREFER_EXISTING_WINDOW,
	REPORT_LOCATION_CHANGE,
	REPORT_REDIRECT,
	TITLE_CHANGED,
	VIEW_LOADED,
	ZOOM_LEVEL_CHANGED,
	ZOOM_PARAMETERS_CHANGED,
	LAST_SIGNAL
};

typedef enum {
	VIEW_FRAME_EMPTY,
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
        char *view_iid;

        /* The view frame Bonobo objects. */
        BonoboObject *view_frame;
	BonoboControlFrame *control_frame;
        BonoboZoomableFrame *zoomable_frame;
        
        /* The view CORBA object. */
        Nautilus_View view;

	/* A container to connect our clients to. */
	BonoboUIContainer *ui_container;
        NautilusUndoManager *undo_manager;

	NautilusBonoboActivationHandle *activation_handle;

	NautilusIdleQueue *idle_queue;

	guint failed_idle_id;
	guint socket_gone_idle_id;
};

static void nautilus_view_frame_initialize       (NautilusViewFrame      *view);
static void nautilus_view_frame_destroy          (GtkObject              *view);
static void nautilus_view_frame_finalize         (GtkObject              *view);
static void nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass);
static void nautilus_view_frame_map              (GtkWidget              *view);
static void send_history                         (NautilusViewFrame      *view);

static guint signals[LAST_SIGNAL];

EEL_DEFINE_CLASS_BOILERPLATE (NautilusViewFrame,
			      nautilus_view_frame,
			      EEL_TYPE_GENEROUS_BIN)

void
nautilus_view_frame_queue_incoming_call (PortableServer_Servant servant,
					 NautilusViewFrameFunction call,
					 gpointer callback_data,
					 GDestroyNotify destroy_callback_data)
{
	NautilusViewFrame *view;

	view = ((impl_POA_Nautilus_ViewFrame *) servant)->view;
	if (view == NULL) {
		if (destroy_callback_data != NULL) {
			(* destroy_callback_data) (callback_data);
		}
		return;
	}

	nautilus_idle_queue_add (view->details->idle_queue,
				 (GFunc) call,
				 view,
				 callback_data,
				 destroy_callback_data);
}

static void
nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->destroy = nautilus_view_frame_destroy;
	object_class->finalize = nautilus_view_frame_finalize;

	widget_class->map = nautilus_view_frame_map;
	
	signals[CHANGE_SELECTION] = gtk_signal_new
		("change_selection",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    change_selection),
		 gtk_marshal_NONE__POINTER,
		 GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[CHANGE_STATUS] = gtk_signal_new
		("change_status",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    change_status),
		 gtk_marshal_NONE__STRING,
		 GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[FAILED] = gtk_signal_new
		("failed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    failed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[GET_HISTORY_LIST] = gtk_signal_new
		("get_history_list",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    get_history_list),
		 eel_gtk_marshal_POINTER__NONE,
		 GTK_TYPE_POINTER, 0);
	signals[GO_BACK] = gtk_signal_new
		("go_back",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    go_back),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[LOAD_COMPLETE] = gtk_signal_new
		("load_complete",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    load_complete),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[LOAD_PROGRESS_CHANGED] = gtk_signal_new
		("load_progress_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    load_progress_changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[LOAD_UNDERWAY] = gtk_signal_new
		("load_underway",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    load_underway),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[OPEN_LOCATION_FORCE_NEW_WINDOW] = gtk_signal_new
		("open_location_force_new_window",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    open_location_force_new_window),
		 eel_gtk_marshal_NONE__STRING_POINTER,
		 GTK_TYPE_NONE, 2, GTK_TYPE_STRING, GTK_TYPE_POINTER);
	signals[OPEN_LOCATION_IN_THIS_WINDOW] = gtk_signal_new
		("open_location_in_this_window",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    open_location_in_this_window),
		 gtk_marshal_NONE__STRING,
		 GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[OPEN_LOCATION_PREFER_EXISTING_WINDOW] = gtk_signal_new
		("open_location_prefer_existing_window",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    open_location_in_this_window),
		 gtk_marshal_NONE__STRING,
		 GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[REPORT_LOCATION_CHANGE] = gtk_signal_new
		("report_location_change",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    report_location_change),
		 eel_gtk_marshal_NONE__STRING_POINTER_STRING,
		 GTK_TYPE_NONE, 3, GTK_TYPE_STRING, GTK_TYPE_POINTER, GTK_TYPE_STRING);
	signals[REPORT_REDIRECT] = gtk_signal_new
		("report_redirect",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    report_redirect),
		 eel_gtk_marshal_NONE__STRING_STRING_POINTER_STRING,
		 GTK_TYPE_NONE, 4, GTK_TYPE_STRING, GTK_TYPE_STRING, GTK_TYPE_POINTER, GTK_TYPE_STRING);
	signals[TITLE_CHANGED] = gtk_signal_new
		("title_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    title_changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[VIEW_LOADED] = gtk_signal_new
		("view_loaded",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    view_loaded),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[ZOOM_LEVEL_CHANGED] = gtk_signal_new
		("zoom_level_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    zoom_level_changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[ZOOM_PARAMETERS_CHANGED] = gtk_signal_new
		("zoom_parameters_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    zoom_parameters_changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_view_frame_initialize (NautilusViewFrame *view)
{
	GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);

	view->details = g_new0 (NautilusViewFrameDetails, 1);

	view->details->idle_queue = nautilus_idle_queue_new ();

	gtk_signal_connect_object_while_alive (nautilus_signaller_get_current (),
					       "history_list_changed",
					       send_history,
					       GTK_OBJECT (view));
	gtk_signal_connect_object_while_alive (GTK_OBJECT (nautilus_icon_factory_get ()),
					       "icons_changed",
					       send_history,
					       GTK_OBJECT (view));
}

static void
stop_activation (NautilusViewFrame *view)
{
	nautilus_bonobo_activate_cancel (view->details->activation_handle);
	view->details->activation_handle = NULL;
}

static void
destroy_view (NautilusViewFrame *view)
{
	CORBA_Environment ev;

	if (view->details->view == CORBA_OBJECT_NIL) {
		return;
	}
	
	g_free (view->details->view_iid);
	view->details->view_iid = NULL;

	CORBA_exception_init (&ev);
	CORBA_Object_release (view->details->view, &ev);
	CORBA_exception_free (&ev);
	view->details->view = CORBA_OBJECT_NIL;

	nautilus_bonobo_object_call_when_remote_object_disappears
		(view->details->view_frame, CORBA_OBJECT_NIL, NULL, NULL);
	bonobo_object_unref (view->details->view_frame);
	view->details->view_frame = NULL;
	view->details->control_frame = NULL;
	view->details->zoomable_frame = NULL;

	if (view->details->ui_container->win != NULL) {
		bonobo_window_deregister_dead_components (view->details->ui_container->win);
	}
	bonobo_object_unref (BONOBO_OBJECT (view->details->ui_container));
	view->details->ui_container = NULL;
}

static void
nautilus_view_frame_destroy (GtkObject *object)
{
	NautilusViewFrame *view;

	view = NAUTILUS_VIEW_FRAME (object);
	
	stop_activation (view);
	destroy_view (view);

	nautilus_idle_queue_destroy (view->details->idle_queue);

	if (view->details->failed_idle_id != 0) {
		gtk_idle_remove (view->details->failed_idle_id);
		view->details->failed_idle_id = 0;
	}
	if (view->details->socket_gone_idle_id != 0) {
		gtk_idle_remove (view->details->socket_gone_idle_id);
		view->details->socket_gone_idle_id = 0;
	}

	/* It's good to be in "failed" state while shutting down
	 * (makes operations all be quiet no-ops). But we don't want
	 * to send out a "failed" signal.
	 */
	view->details->state = VIEW_FRAME_FAILED;
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_view_frame_finalize (GtkObject *object)
{
	NautilusViewFrame *view;

	view = NAUTILUS_VIEW_FRAME (object);

	/* The "destroy" put us in a failed state. */
	g_assert (view->details->state == VIEW_FRAME_FAILED);

	g_free (view->details->title);
	g_free (view->details->label);
	g_free (view->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, finalize, (object));
}

static void
emit_zoom_parameters_changed (NautilusViewFrame *view)
{
	if (view->details->zoomable_frame != NULL) {
		gtk_signal_emit (GTK_OBJECT (view), signals[ZOOM_PARAMETERS_CHANGED]);
	}
}

/* stimulus: successful activated_component call */
static void
view_frame_activated (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
		view->details->state = VIEW_FRAME_NO_LOCATION;
		gtk_signal_emit (GTK_OBJECT (view), signals[VIEW_LOADED]);
		emit_zoom_parameters_changed (view);
		send_history (view);
		return;
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

/* this corresponds to the load_location call stimulus */
static void
view_frame_wait (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
		g_warning ("tried to load location in an empty view frame");
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
		g_assert_not_reached ();
		return;
	case VIEW_FRAME_NO_LOCATION:
		g_warning ("got signal from a view frame with no location");
		return;
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_LOADED:
		view->details->state = VIEW_FRAME_UNDERWAY;
		gtk_signal_emit (GTK_OBJECT (view), signals[LOAD_UNDERWAY]);
		return;
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_FAILED:
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
		g_assert_not_reached ();
		return;
	case VIEW_FRAME_NO_LOCATION:
		g_warning ("got signal from a view frame with no location");
		return;
	case VIEW_FRAME_WAITING:
		view_frame_underway (view);
		/* fall through */
	case VIEW_FRAME_UNDERWAY:
		view->details->state = VIEW_FRAME_LOADED;
		gtk_signal_emit (GTK_OBJECT (view), signals[LOAD_COMPLETE]);
		return;
	case VIEW_FRAME_LOADED:
	case VIEW_FRAME_FAILED:
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
	case VIEW_FRAME_LOADED:
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_WAITING:
		view->details->state = VIEW_FRAME_FAILED;
		stop_activation (view);
		destroy_view (view);
		gtk_signal_emit (GTK_OBJECT (view), signals[FAILED]);
		return;
	case VIEW_FRAME_FAILED:
		return;
	}

	g_assert_not_reached ();
}

NautilusViewFrame *
nautilus_view_frame_new (BonoboUIContainer *ui_container,
                         NautilusUndoManager *undo_manager)
{
	NautilusViewFrame *view_frame;
	
	view_frame = NAUTILUS_VIEW_FRAME (gtk_widget_new (nautilus_view_frame_get_type (), NULL));
	
	bonobo_object_ref (BONOBO_OBJECT (ui_container));
	view_frame->details->ui_container = ui_container;
	view_frame->details->undo_manager = undo_manager;
	
	return view_frame;
}

static void
emit_zoom_parameters_changed_callback (gpointer data,
				       gpointer callback_data)
{
	emit_zoom_parameters_changed (NAUTILUS_VIEW_FRAME (data));
}

static void
zoom_parameters_changed_callback (BonoboZoomableFrame *zframe,
				  NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));

	nautilus_idle_queue_add	(view->details->idle_queue,
				 emit_zoom_parameters_changed_callback,
				 view,
				 NULL,
				 NULL);
}

static void
emit_zoom_level_changed_callback (gpointer data,
				  gpointer callback_data)
{
	gtk_signal_emit (GTK_OBJECT (data),
			 signals[ZOOM_LEVEL_CHANGED],
			 * (float *) callback_data);
}

static void
zoom_level_changed_callback (BonoboZoomableFrame *zframe,
			     float zoom_level,
			     NautilusViewFrame *view)
{
	float *copy;

	g_assert (NAUTILUS_IS_VIEW_FRAME (view));

	copy = g_new (float, 1);
	*copy = zoom_level;
	nautilus_idle_queue_add	(view->details->idle_queue,
				 emit_zoom_level_changed_callback,
				 view,
				 copy,
				 g_free);
}

static void
create_corba_objects (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	Bonobo_Control control;
	Bonobo_Zoomable zoomable;

	CORBA_exception_init (&ev);

	/* Create a view frame. */
	view->details->view_frame = impl_Nautilus_ViewFrame__create (view, &ev);
	g_assert (ev._major == CORBA_NO_EXCEPTION);

	/* Create a control frame. */
	control = Bonobo_Unknown_queryInterface
		(view->details->view, "IDL:Bonobo/Control:1.0", &ev);
	g_assert (ev._major == CORBA_NO_EXCEPTION);
	view->details->control_frame = bonobo_control_frame_new
		(bonobo_object_corba_objref (BONOBO_OBJECT (view->details->ui_container)));
	bonobo_control_frame_bind_to_control (view->details->control_frame, control);
	bonobo_object_release_unref (control, NULL);

	/* Create a zoomable frame. */
	zoomable = Bonobo_Unknown_queryInterface
		(view->details->view, "IDL:Bonobo/Zoomable:1.0", &ev);
	if (ev._major == CORBA_NO_EXCEPTION
	    && !CORBA_Object_is_nil (zoomable, &ev)
	    && ev._major == CORBA_NO_EXCEPTION) {
		view->details->zoomable_frame = bonobo_zoomable_frame_new ();
		bonobo_zoomable_frame_bind_to_zoomable (view->details->zoomable_frame, zoomable);
		bonobo_object_release_unref (zoomable, NULL);
	}

	CORBA_exception_free (&ev);

	/* Aggregate all the interfaces into one object. */
	bonobo_object_add_interface (BONOBO_OBJECT (view->details->view_frame), 
	                             BONOBO_OBJECT (view->details->control_frame));
 	if (view->details->zoomable_frame != NULL) {
 		bonobo_object_add_interface (BONOBO_OBJECT (view->details->view_frame), 
 					     BONOBO_OBJECT (view->details->zoomable_frame));
 	}
	nautilus_undo_manager_add_interface (view->details->undo_manager,
					     BONOBO_OBJECT (view->details->view_frame));
}


static gboolean
view_frame_failed_callback (gpointer callback_data)
{
	NautilusViewFrame *view;

	view = NAUTILUS_VIEW_FRAME (callback_data);
	view->details->failed_idle_id = 0;
	view_frame_failed (view);
	return FALSE;
}

static void
queue_view_frame_failed (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->failed_idle_id == 0)
		view->details->failed_idle_id =
			gtk_idle_add (view_frame_failed_callback, view);
}

static void
view_frame_failed_cover (BonoboObject *object,
			 gpointer callback_data)
{
	view_frame_failed (NAUTILUS_VIEW_FRAME (callback_data));
}

static gboolean
check_socket_gone_idle_callback (gpointer callback_data)
{
	NautilusViewFrame *frame;
	GtkWidget *widget;
	GList *children;

	frame = NAUTILUS_VIEW_FRAME (callback_data);
	
	frame->details->socket_gone_idle_id = 0;

	widget = bonobo_control_frame_get_widget (frame->details->control_frame);

	/* This relies on details of the BonoboControlFrame
	 * implementation, specifically that's there's one level of
	 * hierarchy between the widget returned by get_widget and the
	 * actual plug.
	 */
	children = gtk_container_children (GTK_CONTAINER (widget));
	g_list_free (children);

	/* If there's nothing inside the widget at all, that means
	 * that the socket went away because the remote plug went away.
	 */
	if (children == NULL) {
		view_frame_failed (frame);
	}

	return FALSE;
}

static void
check_socket_gone_callback (GtkContainer *container,
			    GtkWidget *widget,
			    gpointer callback_data)
{
	NautilusViewFrame *frame;

	frame = NAUTILUS_VIEW_FRAME (callback_data);

	/* There are two times the socket will be destroyed in Bonobo.
	 * One is when a local control decides to not use the socket.
	 * The other is when the remote plug goes away. The way to
	 * tell these apart is to wait until idle time. At idle time,
	 * if there's nothing in the container, then that means the
	 * real socket went away. If it was just the local control
	 * deciding not to use the socket, there will be another
	 * widget in there.
	 */
	if (frame->details->socket_gone_idle_id == 0) {
		frame->details->socket_gone_idle_id = gtk_idle_add
			(check_socket_gone_idle_callback, callback_data);
	}
}

static void
attach_view (NautilusViewFrame *view,
	     BonoboObjectClient *client)
{
	CORBA_Environment ev;
	GtkWidget *widget;
  	
	/* Either create an adapter or query for the Nautilus:View
	 * interface. Either way, we don't need to keep the original
	 * reference around once that happens.
	 */
	view->details->view = nautilus_component_adapter_factory_create_adapter 
		(nautilus_component_adapter_factory_get (), client);

	/* Handle case where we don't know how to host this component. */
  	if (view->details->view == CORBA_OBJECT_NIL) {
		view_frame_failed (view);
      		return;
    	}

	create_corba_objects (view);

	CORBA_exception_init (&ev);
	Bonobo_Unknown_unref (view->details->view, &ev);
	CORBA_exception_free (&ev);

	widget = bonobo_control_frame_get_widget (view->details->control_frame);

	gtk_signal_connect_object_while_alive
		(GTK_OBJECT (view->details->view_frame),
		 "destroy",
		 view_frame_failed, GTK_OBJECT (view));
	gtk_signal_connect_object_while_alive
		(GTK_OBJECT (view->details->view_frame),
		 "system_exception",
		 queue_view_frame_failed, GTK_OBJECT (view));

	gtk_signal_connect_object_while_alive
		(GTK_OBJECT (view->details->control_frame),
		 "system_exception",
		 queue_view_frame_failed, GTK_OBJECT (view));

	gtk_signal_connect_while_alive
		(GTK_OBJECT (widget),
		 "remove",
		 check_socket_gone_callback, view,
		 GTK_OBJECT (view));

	if (view->details->zoomable_frame != NULL) {
		gtk_signal_connect_object_while_alive
			(GTK_OBJECT (view->details->zoomable_frame),
			 "system_exception",
			 queue_view_frame_failed, GTK_OBJECT (view));

		gtk_signal_connect_while_alive
			(GTK_OBJECT (view->details->zoomable_frame),
			 "zoom_parameters_changed",
			 zoom_parameters_changed_callback, view,
			 GTK_OBJECT (view));
		gtk_signal_connect_while_alive
			(GTK_OBJECT (view->details->zoomable_frame),
			 "zoom_level_changed",
			 GTK_SIGNAL_FUNC (zoom_level_changed_callback), view,
			 GTK_OBJECT (view));
	}

	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (view), widget);
	
	view_frame_activated (view);

	nautilus_bonobo_object_call_when_remote_object_disappears
		(view->details->view_frame,
		 view->details->view,
		 view_frame_failed_cover,
		 view);
}

static void
activation_callback (NautilusBonoboActivationHandle *handle,
		     Bonobo_Unknown activated_object,
		     gpointer callback_data)
{
	NautilusViewFrame *view;
	BonoboObjectClient *component;

	view = NAUTILUS_VIEW_FRAME (callback_data);
	g_assert (view->details->activation_handle == handle);

	view->details->activation_handle = NULL;

	if (activated_object == CORBA_OBJECT_NIL) {
		view_frame_failed (view);
	} else {
		component = bonobo_object_client_from_corba (activated_object);
		attach_view (view, component);
		bonobo_object_unref (BONOBO_OBJECT (component));
	}
}

void
nautilus_view_frame_load_view (NautilusViewFrame *view, 
			       const char *view_iid)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	g_return_if_fail (view_iid != NULL);

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	g_return_if_fail (view->details->state == VIEW_FRAME_EMPTY);
	g_assert (view->details->view_iid == NULL);
	g_assert (view->details->activation_handle == NULL);

	view->details->view_iid = g_strdup (view_iid);
	view->details->activation_handle = nautilus_bonobo_activate_from_id
		(view_iid, activation_callback, view);
}

void
nautilus_view_frame_load_location (NautilusViewFrame *view,
                                   const char *location)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	g_free (view->details->title);
	view->details->title = NULL;
	gtk_signal_emit (GTK_OBJECT (view), signals[TITLE_CHANGED]);

	view_frame_wait (view);
	
	/* ORBit does a bad job with Nautilus_URI, so it's not const char *. */
	CORBA_exception_init (&ev);
	Nautilus_View_load_location (view->details->view,
				     (Nautilus_URI) location, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		view_frame_failed (view);
	}
	CORBA_exception_free (&ev);
}

void
nautilus_view_frame_stop (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}
	
	stop_activation (view);

	if (view->details->view != CORBA_OBJECT_NIL) {
		CORBA_exception_init (&ev);
		Nautilus_View_stop_loading (view->details->view, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			view_frame_failed (view);
		}
		CORBA_exception_free (&ev);
	}
}

void
nautilus_view_frame_selection_changed (NautilusViewFrame *view,
                                       GList *selection)
{
	Nautilus_URIList *uri_list;
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	if (view->details->view == CORBA_OBJECT_NIL) {
		return;
	}

	uri_list = nautilus_uri_list_from_g_list (selection);
	
	CORBA_exception_init (&ev);
	Nautilus_View_selection_changed (view->details->view, uri_list, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		view_frame_failed (view);
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
	
	if (view->details->view == CORBA_OBJECT_NIL) {
		return;
	}

	CORBA_exception_init (&ev);
	Nautilus_View_title_changed (view->details->view, title, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		view_frame_failed (view);
	}
	CORBA_exception_free (&ev);
}

gboolean
nautilus_view_frame_get_is_zoomable (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);

	return view->details->zoomable_frame != NULL;
}

double
nautilus_view_frame_get_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);

	if (view->details->zoomable_frame == NULL) {
		return 0.0;
	}

	return (double) bonobo_zoomable_frame_get_zoom_level (view->details->zoomable_frame);
}

void
nautilus_view_frame_set_zoom_level (NautilusViewFrame *view,
				    double zoom_level)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_set_zoom_level (view->details->zoomable_frame, (float) zoom_level);
}

double
nautilus_view_frame_get_min_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);

	if (view->details->zoomable_frame == NULL) {
		return 0.0;
	}

	return (double) bonobo_zoomable_frame_get_min_zoom_level (view->details->zoomable_frame);
}

double
nautilus_view_frame_get_max_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);
	
	if (view->details->zoomable_frame == NULL) {
		return 0.0;
	}

	return (double) bonobo_zoomable_frame_get_max_zoom_level (view->details->zoomable_frame);
}

gboolean
nautilus_view_frame_get_has_min_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	if (view->details->zoomable_frame == NULL) {
		return FALSE;
	}

	return bonobo_zoomable_frame_has_min_zoom_level (view->details->zoomable_frame);
}

gboolean
nautilus_view_frame_get_has_max_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	if (view->details->zoomable_frame == NULL) {
		return FALSE;
	}

	return bonobo_zoomable_frame_has_max_zoom_level (view->details->zoomable_frame);
}

gboolean
nautilus_view_frame_get_is_continuous (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	if (view->details->zoomable_frame == NULL) {
		return FALSE;
	}

	return bonobo_zoomable_frame_is_continuous (view->details->zoomable_frame);
}

GList *
nautilus_view_frame_get_preferred_zoom_levels (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);

	if (view->details->zoomable_frame == NULL) {
		return NULL;
	}

	return bonobo_zoomable_frame_get_preferred_zoom_levels (view->details->zoomable_frame);
}

void
nautilus_view_frame_zoom_in (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
 
	if (view->details->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_zoom_in (view->details->zoomable_frame);
}

void
nautilus_view_frame_zoom_out (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	if (view->details->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_zoom_out (view->details->zoomable_frame);
}

void
nautilus_view_frame_zoom_to_fit (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	if (view->details->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_zoom_to_fit (view->details->zoomable_frame);
}

const char *
nautilus_view_frame_get_view_iid (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);

	return view->details->view_iid;
}

void
nautilus_view_frame_open_location_in_this_window (NautilusViewFrame *view,
						  const char *location)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[OPEN_LOCATION_IN_THIS_WINDOW], location);
}

void
nautilus_view_frame_open_location_prefer_existing_window (NautilusViewFrame *view,
							  const char *location)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[OPEN_LOCATION_PREFER_EXISTING_WINDOW], location);
}

void
nautilus_view_frame_open_location_force_new_window (NautilusViewFrame *view,
						    const char *location,
						    GList *selection)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[OPEN_LOCATION_FORCE_NEW_WINDOW],
			 location, selection);
}

void
nautilus_view_frame_report_location_change (NautilusViewFrame *view,
					    const char *location,
					    GList *selection,
					    const char *title)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	g_free (view->details->title);
	view->details->title = g_strdup (title);

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[REPORT_LOCATION_CHANGE],
			 location, selection, title);
}

void
nautilus_view_frame_report_redirect (NautilusViewFrame *view,
				     const char *from_location,
				     const char *to_location,
				     GList *selection,
				     const char *title)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	g_free (view->details->title);
	view->details->title = g_strdup (title);

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[REPORT_REDIRECT],
			 from_location, to_location, selection, title);
}

void
nautilus_view_frame_report_selection_change (NautilusViewFrame *view,
                                             GList *selection)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[CHANGE_SELECTION], selection);
}

void
nautilus_view_frame_report_status (NautilusViewFrame *view,
                                   const char *status)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_wait_is_over (view);
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[CHANGE_STATUS], status);
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

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_underway (view);
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[LOAD_PROGRESS_CHANGED]);
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

/* return the Bonobo_Control CORBA object associated with the view frame */
Bonobo_Control
nautilus_view_frame_get_control (NautilusViewFrame *view)
{
	if (view->details->control_frame == NULL) {
		return CORBA_OBJECT_NIL;
	}
	return bonobo_control_frame_get_control (view->details->control_frame);
}

void
nautilus_view_frame_go_back (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	gtk_signal_emit (GTK_OBJECT (view), signals[GO_BACK]);
}

void
nautilus_view_frame_set_title (NautilusViewFrame *view,
                               const char *title)
{
	gboolean changed;

	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	g_return_if_fail (title != NULL);

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

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
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);

	return g_strdup (view->details->title);
}

char *
nautilus_view_frame_get_label (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);

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

/* Activate the underlying control frame whenever the view is mapped.
 * This causes the view to merge its menu items, for example. For
 * sidebar panels, it might be a little late to merge them at map
 * time. What's a better time?
 */
static void
nautilus_view_frame_map (GtkWidget *view_as_widget)
{
	NautilusViewFrame *view;

	view = NAUTILUS_VIEW_FRAME (view_as_widget);

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, map, (view_as_widget));

	if (view->details->control_frame != NULL) {
		bonobo_control_frame_control_activate (view->details->control_frame);
	}
}

static Nautilus_History *
get_history_list (NautilusViewFrame *view)
{
	Nautilus_History *history_list;
	
	history_list = NULL;
	gtk_signal_emit (GTK_OBJECT (view), 
  			 signals[GET_HISTORY_LIST],
			 &history_list);
  	return history_list;
}

static void
send_history (NautilusViewFrame *view)
{
	Nautilus_History *history;
	CORBA_Environment ev;

	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	if (view->details->view == CORBA_OBJECT_NIL) {
		return;
	}

	history = get_history_list (view);
	if (history == NULL) {
		return;
	}
	
	CORBA_exception_init (&ev);
	Nautilus_View_history_changed (view->details->view, history, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		view_frame_failed (view);
	}
	CORBA_exception_free (&ev);
	
	CORBA_free (history);
}

gboolean
nautilus_view_frame_get_is_view_loaded (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_FAILED:
		return FALSE;
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
		return TRUE;
	}

	g_assert_not_reached ();
	return FALSE;
}
