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
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@eazel.com>
 *
 */

/* nautilus-view-frame.c: Widget and CORBA machinery for hosting a NautilusView */

#include <config.h>
#include "nautilus-view-frame-private.h"

#include "nautilus-application.h"
#include "nautilus-component-adapter-factory.h"
#include "nautilus-signaller.h"
#include "nautilus-window.h"
#include <bonobo/bonobo-zoomable-frame.h>
#include <bonobo/bonobo-zoomable.h>
#include <gtk/gtksignal.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <libnautilus/nautilus-view.h>

/* FIXME bugzilla.eazel.com 2456: Is a hard-coded 12 seconds wait to
 * detect that a view is gone acceptable? Can a component that is
 * working still take 12 seconds to respond?
 */
/* Milliseconds */
#define ATTACH_CLIENT_TIMEOUT	12000

enum {
	CHANGE_SELECTION,
	CHANGE_STATUS,
	CLIENT_LOADED,
	FAILED,
	GET_HISTORY_LIST,
	LOAD_COMPLETE,
	LOAD_PROGRESS_CHANGED,
	LOAD_UNDERWAY,
	OPEN_LOCATION_FORCE_NEW_WINDOW,
	OPEN_LOCATION_IN_THIS_WINDOW,
	OPEN_LOCATION_PREFER_EXISTING_WINDOW,
	TITLE_CHANGED,
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

	/* A container to connect our clients to. */
	BonoboUIContainer *ui_container;

	guint check_if_view_is_gone_timeout_id;

	char *activation_iid;
	NautilusBonoboActivationHandle *activation_handle;
};

static void nautilus_view_frame_initialize       (NautilusViewFrame      *view);
static void nautilus_view_frame_destroy          (GtkObject              *view);
static void nautilus_view_frame_finalize         (GtkObject              *view);
static void nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass);
static void nautilus_view_frame_map              (GtkWidget              *view);
static void send_history                         (NautilusViewFrame      *view);

static guint signals[LAST_SIGNAL];

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusViewFrame,
				   nautilus_view_frame,
				   NAUTILUS_TYPE_GENEROUS_BIN)

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
	signals[CLIENT_LOADED] = gtk_signal_new
		("client_loaded",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    client_loaded),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
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
		 nautilus_gtk_marshal_POINTER__NONE,
		 GTK_TYPE_POINTER, 0);
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
		 nautilus_gtk_marshal_NONE__STRING_POINTER,
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
	signals[TITLE_CHANGED] = gtk_signal_new
		("title_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
				    title_changed),
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
nautilus_view_frame_destroy_client (NautilusViewFrame *view)
{
	if (view->iid == NULL) {
		return;
	}
	
	g_free (view->iid);
	view->iid = NULL;
	
	bonobo_object_unref (BONOBO_OBJECT (view->client_object));
	view->client_object = NULL;

	gtk_container_remove (GTK_CONTAINER (view), view->client_widget);
	view->client_widget = NULL;

	bonobo_object_unref (view->view_frame);
	view->view_frame = NULL;

	/* We can NULL this since we just unref'ed it as part of the
	 * aggregate view frame.
	 */
	view->zoomable_frame = NULL;

	if (view->details->ui_container->win != NULL) {
		bonobo_window_deregister_dead_components (view->details->ui_container->win);
	}
	
	bonobo_object_unref (BONOBO_OBJECT (view->details->ui_container));

	if (view->details->check_if_view_is_gone_timeout_id != 0) {
		g_source_remove (view->details->check_if_view_is_gone_timeout_id);
		view->details->check_if_view_is_gone_timeout_id = 0;
	}
}

static void
nautilus_view_frame_destroy (GtkObject *object)
{
	NautilusViewFrame *view;

	view = NAUTILUS_VIEW_FRAME (object);
	
	nautilus_view_frame_destroy_client (view);
	view->details->state = VIEW_FRAME_EMPTY;
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_view_frame_finalize (GtkObject *object)
{
	NautilusViewFrame *view;

	view = NAUTILUS_VIEW_FRAME (object);

	g_free (view->details->title);
	g_free (view->details->label);
	g_free (view->details->activation_iid);
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, finalize, (object));
}

/* stimulus: successful activated_component call */
static void
view_frame_activated (NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
		view->details->state = VIEW_FRAME_NO_LOCATION;
		gtk_signal_emit (GTK_OBJECT (view), signals[CLIENT_LOADED]);
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
	case VIEW_FRAME_FAILED:
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
	case VIEW_FRAME_FAILED:
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
		gtk_signal_emit (GTK_OBJECT (view), signals[FAILED]);
		return;
	case VIEW_FRAME_FAILED:
		g_assert_not_reached ();
		return;
	}

	g_assert_not_reached ();
}

static void
nautilus_view_frame_handle_client_destroy (GtkWidget *widget,
					   NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	bonobo_window_deregister_dead_components (view->details->ui_container->win);
	view_frame_failed (view);
}

static void
nautilus_view_frame_handle_client_gone (GtkObject *object,
					CORBA_Object cobject,
					CORBA_Environment *ev,
					NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_VIEW_FRAME (view));
	bonobo_window_deregister_dead_components (view->details->ui_container->win);
	view_frame_failed (view);
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

static Nautilus_ViewFrame
get_CORBA_object (NautilusViewFrame *view)
{
	return bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object));
}

static gboolean
check_if_view_is_gone (gpointer callback_data)
{
	NautilusViewFrame *view;
	CORBA_Environment ev;
	gboolean view_is_gone;

	view = NAUTILUS_VIEW_FRAME (callback_data);

	CORBA_exception_init (&ev);
	view_is_gone = CORBA_Object_non_existent (get_CORBA_object (view), &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		view_is_gone = TRUE;
	}
	CORBA_exception_free (&ev);
	
	if (!view_is_gone) {
		return TRUE;
	}

	view->details->check_if_view_is_gone_timeout_id = 0;
	bonobo_window_deregister_dead_components (view->details->ui_container->win);
	view_frame_failed (view);
	return FALSE;
}

static void
zoom_level_changed_callback (BonoboZoomableFrame *zframe,
			     float zoom_level,
			     NautilusViewFrame *view)
{
	g_return_if_fail (zframe != NULL);
	g_return_if_fail (BONOBO_IS_ZOOMABLE_FRAME (zframe));
	g_return_if_fail (view != NULL);
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	gtk_signal_emit (GTK_OBJECT (view), signals[ZOOM_LEVEL_CHANGED], zoom_level);
}

static void
zoom_parameters_changed_callback (BonoboZoomableFrame *zframe,
				  NautilusViewFrame *view)
{
	g_return_if_fail (zframe != NULL);
	g_return_if_fail (BONOBO_IS_ZOOMABLE_FRAME (zframe));
	g_return_if_fail (view != NULL);
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	gtk_signal_emit (GTK_OBJECT (view), signals[ZOOM_PARAMETERS_CHANGED]);
}

static gboolean
attach_client (NautilusViewFrame *view, BonoboObjectClient *client)
{
	CORBA_Environment ev;
	Nautilus_View adapted;
	Bonobo_Control control;
	BonoboControlFrame *control_frame;
	NautilusComponentAdapterFactory *adapter_factory;
	Bonobo_Zoomable zoomable;
  	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);

	/* Either create an adapter or query for the Nautilus:View
	 * interface. Either way, we don't need to keep the original
	 * reference around once that happens.
	 */

	adapter_factory = nautilus_component_adapter_factory_get ();

	if (adapter_factory == NULL) {
		return FALSE;
	}

	adapted = nautilus_component_adapter_factory_create_adapter 
		(adapter_factory,
		 client);
	bonobo_object_unref (BONOBO_OBJECT (client));

	/* Handle case where we don't know how to host this component. */
  	if (adapted == CORBA_OBJECT_NIL) {
      		return FALSE;
    	}

	nautilus_view_frame_destroy_client (view);

	CORBA_exception_init (&ev);

	/* Store the object away. */
	view->client_object = bonobo_object_client_from_corba (adapted);
	g_assert (!CORBA_Object_non_existent (adapted, &ev));
	g_assert (ev._major == CORBA_NO_EXCEPTION);

	/* Get at our client's interfaces. */
	control = bonobo_object_query_interface
		(BONOBO_OBJECT (view->client_object),
		 "IDL:Bonobo/Control:1.0");
	g_assert (control != CORBA_OBJECT_NIL);

	/* Add a zoomable frame interface. */
	zoomable = Bonobo_Unknown_queryInterface
		(control, "IDL:Bonobo/Zoomable:1.0", &ev);
	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (zoomable, &ev)) {
		view->zoomable_frame = bonobo_zoomable_frame_new ();

		gtk_signal_connect (GTK_OBJECT (view->zoomable_frame), "zoom_level_changed",
				    GTK_SIGNAL_FUNC (zoom_level_changed_callback), view);
		gtk_signal_connect (GTK_OBJECT (view->zoomable_frame), "zoom_parameters_changed",
				    GTK_SIGNAL_FUNC (zoom_parameters_changed_callback), view);

		bonobo_zoomable_frame_bind_to_zoomable (view->zoomable_frame, zoomable);
		bonobo_object_release_unref (zoomable, NULL);
	}

	/* Start with a view frame interface. */
	view->view_frame = impl_Nautilus_ViewFrame__create (view, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME bugzilla.eazel.com 5041: Cleanup needed here. */
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	/* Add a control frame interface. */
	control_frame = bonobo_control_frame_new (bonobo_object_corba_objref (BONOBO_OBJECT (view->details->ui_container)));
	bonobo_object_add_interface (BONOBO_OBJECT (view->view_frame), 
	                             BONOBO_OBJECT (control_frame));
	bonobo_control_frame_bind_to_control (control_frame, control);
	view->client_widget = bonobo_control_frame_get_widget (control_frame);

 	if (view->zoomable_frame != NULL) {
 		bonobo_object_add_interface (BONOBO_OBJECT (view->view_frame), 
 					     BONOBO_OBJECT (view->zoomable_frame));
  
 		gtk_signal_emit_by_name (GTK_OBJECT (view->zoomable_frame),
					 "zoom_parameters_changed");
 	}

	/* Add an undo context interface. */
	nautilus_undo_manager_add_interface
        	(view->undo_manager, BONOBO_OBJECT (view->view_frame));

	bonobo_object_release_unref (control, NULL);
	
	view->iid = g_strdup (view->details->activation_iid);

	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "destroy",
		 nautilus_view_frame_handle_client_destroy, view,
		 GTK_OBJECT (view));
	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "object_gone",
		 nautilus_view_frame_handle_client_gone, view,
		 GTK_OBJECT (view));
	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "system_exception",
		 nautilus_view_frame_handle_client_gone, view,
		 GTK_OBJECT (view));
	gtk_container_add (GTK_CONTAINER (view), view->client_widget);
	gtk_widget_show (view->client_widget);

	g_assert (view->details->check_if_view_is_gone_timeout_id == 0);
	view->details->check_if_view_is_gone_timeout_id
		= g_timeout_add (ATTACH_CLIENT_TIMEOUT, check_if_view_is_gone, view);

	return TRUE;
}

static void
activation_callback (NautilusBonoboActivationHandle *handle,
		     Bonobo_Unknown activated_object,
		     gpointer callback_data)
{
	NautilusViewFrame *view;
	BonoboObjectClient *bonobo_object;

	view = NAUTILUS_VIEW_FRAME (callback_data);
	g_assert (view->details->activation_handle == handle);

	view->details->activation_handle = NULL;
	bonobo_object = bonobo_object_client_from_corba (activated_object);
	attach_client (view, bonobo_object);
}

void
nautilus_view_frame_load_client_async (NautilusViewFrame *view, 
				       const char *iid)
{
	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view->details->activation_iid = g_strdup (iid);
	view->details->activation_handle = nautilus_bonobo_activate_from_id
		(iid, activation_callback, view);
}

void
nautilus_view_frame_load_client (NautilusViewFrame *view, const char *iid)
{
	BonoboObjectClient *component;
  	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	g_return_if_fail (view->details->state == VIEW_FRAME_EMPTY);

	if (iid == NULL) {
		view_frame_failed (view);
		return;
        }

	component = bonobo_object_activate (iid, 0);
	if (component == NULL) {
		view_frame_failed (view);
		return;
        }

	view->details->activation_iid = g_strdup (iid);

	if (!attach_client (view, component)) {
		view_frame_failed (view);
		return;
	}

	view_frame_activated (view);
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
	Nautilus_View_load_location (get_CORBA_object (view),
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
	
	CORBA_exception_init (&ev);
	Nautilus_View_stop_loading (get_CORBA_object (view), &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		view_frame_failed (view);
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
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	uri_list = nautilus_uri_list_from_g_list (selection);
	
	CORBA_exception_init (&ev);
	Nautilus_View_selection_changed (get_CORBA_object (view), uri_list, &ev);
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
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	CORBA_exception_init (&ev);
	Nautilus_View_title_changed (get_CORBA_object (view), title, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		view_frame_failed (view);
	}
	CORBA_exception_free (&ev);
}

gboolean
nautilus_view_frame_is_zoomable (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);

	if (view->details->state == VIEW_FRAME_FAILED) {
		return FALSE;
	}

	return view->zoomable_frame != NULL;
}

double
nautilus_view_frame_get_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);

	if (view->details->state == VIEW_FRAME_FAILED) {
		return 0.0;
	}

	if (view->zoomable_frame == NULL) {
		return 0.0;
	}

	return (double) bonobo_zoomable_frame_get_zoom_level (view->zoomable_frame);
}

void
nautilus_view_frame_set_zoom_level (NautilusViewFrame *view,
				    double zoom_level)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	if (view->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_set_zoom_level (view->zoomable_frame, (float) zoom_level);
}

double
nautilus_view_frame_get_min_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);

	if (view->details->state == VIEW_FRAME_FAILED) {
		return 0.0;
	}

	if (view->zoomable_frame == NULL) {
		return 0.0;
	}

	return (double) bonobo_zoomable_frame_get_min_zoom_level (view->zoomable_frame);
}

double
nautilus_view_frame_get_max_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return 0.0;
	}

	if (view->zoomable_frame == NULL) {
		return 0.0;
	}

	return (double) bonobo_zoomable_frame_get_max_zoom_level (view->zoomable_frame);
}

gboolean
nautilus_view_frame_get_has_min_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return FALSE;
	}

	if (view->zoomable_frame == NULL) {
		return FALSE;
	}

	return bonobo_zoomable_frame_has_min_zoom_level (view->zoomable_frame);
}

gboolean
nautilus_view_frame_get_has_max_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return FALSE;
	}

	if (view->zoomable_frame == NULL) {
		return FALSE;
	}

	return bonobo_zoomable_frame_has_max_zoom_level (view->zoomable_frame);
}

gboolean
nautilus_view_frame_get_is_continuous (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return FALSE;
	}

	if (view->zoomable_frame == NULL) {
		return FALSE;
	}

	return bonobo_zoomable_frame_is_continuous (view->zoomable_frame);
}

GList *
nautilus_view_frame_get_preferred_zoom_levels (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);

	if (view->details->state == VIEW_FRAME_FAILED) {
		return FALSE;
	}

	if (view->zoomable_frame == NULL) {
		return NULL;
	}

	return bonobo_zoomable_frame_get_preferred_zoom_levels (view->zoomable_frame);
}

void
nautilus_view_frame_zoom_in (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
 
	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	if (view->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_zoom_in (view->zoomable_frame);
}

void
nautilus_view_frame_zoom_out (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	if (view->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_zoom_out (view->zoomable_frame);
}

void
nautilus_view_frame_zoom_to_fit (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	if (view->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_zoom_to_fit (view->zoomable_frame);
}

const char *
nautilus_view_frame_get_iid (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
	return view->iid;
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
	
	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

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

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_loaded (view);
}

void
nautilus_view_frame_report_load_failed (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_failed (view);
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

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	g_free (view->details->label);
	view->details->label = g_strdup (label);
}

/* Calls activate on the underlying control frame. */
static void
nautilus_view_frame_map (GtkWidget *view_as_widget)
{
	NautilusViewFrame *view;
	BonoboControlFrame *control_frame;

	view = NAUTILUS_VIEW_FRAME (view_as_widget);

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, map, (view_as_widget));

	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_WAITING:
		g_warning ("a view frame was mapped before it was underway");
		break;
	case VIEW_FRAME_FAILED:
		return;
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
		break;
	}

	control_frame = BONOBO_CONTROL_FRAME (bonobo_object_query_local_interface 
					      (view->view_frame, "IDL:Bonobo/ControlFrame:1.0"));
	bonobo_control_frame_control_activate (control_frame);
	bonobo_object_unref (BONOBO_OBJECT (control_frame));
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
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_FAILED:
		return;
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_LOADED:
		break;
	}

	history = get_history_list (view);
	if (history == NULL) {
		return;
	}
	
	CORBA_exception_init (&ev);
	Nautilus_View_history_changed (get_CORBA_object (view), history, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		view_frame_failed (view);
	}
	CORBA_exception_free (&ev);
	
	CORBA_free (history);
}

gboolean
nautilus_view_frame_get_is_underway (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	switch (view->details->state) {
	case VIEW_FRAME_EMPTY:
	case VIEW_FRAME_NO_LOCATION:
	case VIEW_FRAME_FAILED:
	case VIEW_FRAME_WAITING:
	case VIEW_FRAME_LOADED:
		return FALSE;
	case VIEW_FRAME_UNDERWAY:
		return TRUE;
	}

	g_assert_not_reached ();
	return FALSE;
}
