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
#include <bonobo/bonobo-control-frame.h>
#include <bonobo/bonobo-event-source.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-zoomable-frame.h>
#include <bonobo/bonobo-zoomable.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-marshal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-marshal.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus/nautilus-idle-queue.h>
#include <libnautilus/nautilus-view.h>
#include <nautilus-marshal.h>
#include <string.h>

enum {
	CHANGE_SELECTION,
	CHANGE_STATUS,
	FAILED,
	GET_HISTORY_LIST,
	GO_BACK,
	CLOSE_WINDOW,
	LOAD_COMPLETE,
	LOAD_PROGRESS_CHANGED,
	LOAD_UNDERWAY,
	OPEN_LOCATION,
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
	BonoboEventSource *event_source;
	BonoboControlFrame *control_frame;
        BonoboZoomableFrame *zoomable_frame;

        /* The view CORBA object. */
        Nautilus_View view;

	/* The positionable CORBA object. */
	Nautilus_ScrollPositionable positionable;
	
	/* A container to connect our clients to. */
	BonoboUIContainer *ui_container;
        NautilusUndoManager *undo_manager;

	NautilusBonoboActivationHandle *activation_handle;

	NautilusIdleQueue *idle_queue;

	guint failed_idle_id;
	guint socket_gone_idle_id;

	/* zoom data */
	float zoom_level;
	float min_zoom_level;
	float max_zoom_level;
	gboolean has_min_zoom_level;
	gboolean has_max_zoom_level;
	GList *zoom_levels;

	Nautilus_WindowType window_type;
};

static void nautilus_view_frame_init       (NautilusViewFrame      *view);
static void nautilus_view_frame_class_init (NautilusViewFrameClass *klass);
static void send_history                   (NautilusViewFrame      *view);

static guint signals[LAST_SIGNAL];

EEL_CLASS_BOILERPLATE (NautilusViewFrame,
		       nautilus_view_frame,
		       GTK_TYPE_HBOX)

void
nautilus_view_frame_queue_incoming_call (PortableServer_Servant servant,
					 NautilusViewFrameFunction call,
					 gpointer callback_data,
					 GDestroyNotify destroy_callback_data)
{
	NautilusViewFrame *view;

	view = nautilus_view_frame_from_servant (servant);
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
nautilus_view_frame_init (NautilusViewFrame *view)
{
	GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);

	view->details = g_new0 (NautilusViewFrameDetails, 1);

	view->details->idle_queue = nautilus_idle_queue_new ();

	g_signal_connect_object (nautilus_signaller_get_current (),
				 "history_list_changed",
				 G_CALLBACK (send_history),
				 view, G_CONNECT_SWAPPED);
	g_signal_connect_object (nautilus_icon_factory_get (),
				 "icons_changed",
				 G_CALLBACK (send_history),
				 view, G_CONNECT_SWAPPED);
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
	BonoboUIEngine *engine;

	if (view->details->view == CORBA_OBJECT_NIL) {
		return;
	}
	
	g_free (view->details->view_iid);
	view->details->view_iid = NULL;

	if (view->details->positionable != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (view->details->positionable, NULL);
		view->details->positionable = CORBA_OBJECT_NIL;
	}
	
	CORBA_Object_release (view->details->view, NULL);
	view->details->view = CORBA_OBJECT_NIL;

	bonobo_object_unref (view->details->view_frame);
	view->details->view_frame = NULL;
	view->details->control_frame = NULL;
	view->details->zoomable_frame = NULL;

	engine = bonobo_ui_container_get_engine (view->details->ui_container);
	if (engine != NULL) {
		bonobo_ui_engine_deregister_dead_components (engine);
	}
	bonobo_object_unref (view->details->ui_container);
	view->details->ui_container = NULL;
}

static void
shut_down (NautilusViewFrame *view)
{
	/* It's good to be in "failed" state while shutting down
	 * (makes operations all be quiet no-ops). But we don't want
	 * to send out a "failed" signal.
	 */
	view->details->state = VIEW_FRAME_FAILED;

	stop_activation (view);
	destroy_view (view);

	if (view->details->idle_queue != NULL) {
		nautilus_idle_queue_destroy (view->details->idle_queue);
		view->details->idle_queue = NULL;
	}

	if (view->details->failed_idle_id != 0) {
		g_source_remove (view->details->failed_idle_id);
		view->details->failed_idle_id = 0;
	}
	if (view->details->socket_gone_idle_id != 0) {
		g_source_remove (view->details->socket_gone_idle_id);
		view->details->socket_gone_idle_id = 0;
	}
}

static void
nautilus_view_frame_unrealize (GtkWidget *widget)
{
	shut_down (NAUTILUS_VIEW_FRAME (widget));
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, unrealize, (widget));
}

static void
nautilus_view_frame_destroy (GtkObject *object)
{
	shut_down (NAUTILUS_VIEW_FRAME (object));
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_view_frame_finalize (GObject *object)
{
	NautilusViewFrame *view;

	view = NAUTILUS_VIEW_FRAME (object);

	shut_down (view);

	g_free (view->details->title);
	g_free (view->details->label);
	g_free (view->details);
	
	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
emit_zoom_parameters_changed (NautilusViewFrame *view)
{	
	eel_g_list_free_deep (view->details->zoom_levels);

	if (view->details->zoomable_frame) {
		view->details->min_zoom_level = bonobo_zoomable_frame_get_min_zoom_level (view->details->zoomable_frame);
		view->details->max_zoom_level = bonobo_zoomable_frame_get_max_zoom_level (view->details->zoomable_frame);
		view->details->has_min_zoom_level = bonobo_zoomable_frame_has_min_zoom_level (view->details->zoomable_frame);
		view->details->has_max_zoom_level = bonobo_zoomable_frame_has_max_zoom_level (view->details->zoomable_frame);
		view->details->zoom_levels = bonobo_zoomable_frame_get_preferred_zoom_levels (view->details->zoomable_frame);
		
		g_signal_emit (view, signals[ZOOM_PARAMETERS_CHANGED], 0);
	} else {
		view->details->min_zoom_level = 0.0;
		view->details->max_zoom_level = 0.0;
		view->details->has_min_zoom_level = FALSE;
		view->details->has_max_zoom_level = FALSE;
		view->details->zoom_levels = NULL;
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
		g_signal_emit (view, signals[VIEW_LOADED], 0);
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
		g_signal_emit (view, signals[LOAD_UNDERWAY], 0);
		return;
	case VIEW_FRAME_UNDERWAY:
	case VIEW_FRAME_FAILED:
		return;
	}

	g_assert_not_reached ();
}

/* stimulus 
   - open_location call from component
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
		g_signal_emit (view, signals[LOAD_COMPLETE], 0);
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
		g_signal_emit (view, signals[FAILED], 0);
		return;
	case VIEW_FRAME_FAILED:
		return;
	}

	g_assert_not_reached ();
}

NautilusViewFrame *
nautilus_view_frame_new (BonoboUIContainer *ui_container,
                         NautilusUndoManager *undo_manager,
			 Nautilus_WindowType window_type)
{
	NautilusViewFrame *view_frame;
	
	view_frame = NAUTILUS_VIEW_FRAME (gtk_widget_new (nautilus_view_frame_get_type (), NULL));
	
	bonobo_object_ref (ui_container);
	view_frame->details->ui_container = ui_container;
	view_frame->details->undo_manager = undo_manager;
	view_frame->details->window_type = window_type;
	
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
	NautilusViewFrame *view;
	
	view = NAUTILUS_VIEW_FRAME (data);
	
	view->details->zoom_level = bonobo_zoomable_frame_get_zoom_level (view->details->zoomable_frame);

	g_signal_emit (view,
			 signals[ZOOM_LEVEL_CHANGED], 0,
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

enum {
	BONOBO_PROPERTY_TITLE,
	BONOBO_PROPERTY_HISTORY,
	BONOBO_PROPERTY_SELECTION,
	BONOBO_PROPERTY_WINDOW_TYPE,
};

static Nautilus_History *
get_history_list (NautilusViewFrame *view)
{
	Nautilus_History *history_list;
	
	history_list = NULL;
	g_signal_emit (view, 
		       signals[GET_HISTORY_LIST], 0,
		       &history_list);
  	return history_list;
}

static void
nautilus_view_frame_get_prop (BonoboPropertyBag *bag,
			      BonoboArg         *arg,
			      guint              arg_id,
			      CORBA_Environment *ev,
			      gpointer           user_data)
{
	NautilusViewFrame *view = user_data;

	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (user_data));

	switch (arg_id) {
	case BONOBO_PROPERTY_TITLE:
		BONOBO_ARG_SET_STRING (arg, view->details->title);
		break;

	case BONOBO_PROPERTY_HISTORY:
		CORBA_free (arg->_value);
		arg->_value = get_history_list (view);
		break;

	case BONOBO_PROPERTY_SELECTION:
		g_warning ("NautilusViewFrame: selection fetch not yet implemented");
		break;

	case BONOBO_PROPERTY_WINDOW_TYPE :
		BONOBO_ARG_SET_GENERAL (arg, view->details->window_type,
					TC_Nautilus_WindowType,
					Nautilus_WindowType,
					NULL);
		break;
	default:
		g_warning ("NautilusViewFrame: Unknown property idx %d", arg_id);
		break;
	}
}

static BonoboPropertyBag *
create_ambient_properties (NautilusViewFrame *view)
{
	BonoboPropertyBag *bag;

	bag = bonobo_property_bag_new (nautilus_view_frame_get_prop, NULL, view);
	
	bonobo_property_bag_add
		(bag,
		 "title",
		 BONOBO_PROPERTY_TITLE,
		 TC_CORBA_string,
		 NULL,
		 _("a title"),
		 BONOBO_PROPERTY_READABLE);

	bonobo_property_bag_add
		(bag,
		 "history",
		 BONOBO_PROPERTY_HISTORY,
		 TC_Nautilus_History,
		 NULL,
		 _("the browse history"),
		 BONOBO_PROPERTY_READABLE);

	bonobo_property_bag_add
		(bag,
		 "selection",
		 BONOBO_PROPERTY_SELECTION,
		 TC_Nautilus_URIList,
		 NULL,
		 _("the current selection"),
		 BONOBO_PROPERTY_READABLE);

	bonobo_property_bag_add
		(bag,
		 "window-type",
		 BONOBO_PROPERTY_WINDOW_TYPE,
		 TC_Nautilus_WindowType,
		 NULL,
		 _("the type of window the view is embedded in"),
		 BONOBO_PROPERTY_READABLE);

	view->details->event_source = bag->es;

	return bag;
}

static void
create_corba_objects (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	Bonobo_Control control;
	Bonobo_Zoomable zoomable;
	Nautilus_ScrollPositionable positionable;
	BonoboPropertyBag *bag;

	CORBA_exception_init (&ev);

	/* Create a view frame. */
	view->details->view_frame = nautilus_view_frame_create_corba_part (view);

	/* Create a control frame. */
	control = Bonobo_Unknown_queryInterface
		(view->details->view, "IDL:Bonobo/Control:1.0", &ev);
	g_assert (! BONOBO_EX (&ev));

	view->details->control_frame = bonobo_control_frame_new
		(BONOBO_OBJREF (view->details->ui_container));

	bag = create_ambient_properties (view);
	bonobo_control_frame_set_propbag (view->details->control_frame, bag);
	bonobo_object_unref (bag);

	bonobo_control_frame_bind_to_control (view->details->control_frame, control, NULL);

	bonobo_object_release_unref (control, NULL);

	/* Create a zoomable frame. */
	zoomable = Bonobo_Unknown_queryInterface
		(view->details->view, "IDL:Bonobo/Zoomable:1.0", &ev);
	if (!BONOBO_EX (&ev)
	    && !CORBA_Object_is_nil (zoomable, &ev)
	    && !BONOBO_EX (&ev)) {
		view->details->zoomable_frame = bonobo_zoomable_frame_new ();
		bonobo_zoomable_frame_bind_to_zoomable
			(view->details->zoomable_frame, zoomable, NULL);
		bonobo_object_release_unref (zoomable, NULL);
	}

	CORBA_exception_free (&ev);

	CORBA_exception_init (&ev);
	
	positionable = Bonobo_Unknown_queryInterface
		(view->details->view, "IDL:Nautilus/ScrollPositionable:1.0", &ev);
	if (!BONOBO_EX (&ev)
	    && !CORBA_Object_is_nil (positionable, &ev)
	    && !BONOBO_EX (&ev)) {
		view->details->positionable = positionable;
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

	if (view->details->failed_idle_id == 0) {
		view->details->failed_idle_id =
			g_idle_add (view_frame_failed_callback, view);
	}
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
	children = gtk_container_get_children (GTK_CONTAINER (widget));
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
		frame->details->socket_gone_idle_id = g_idle_add
			(check_socket_gone_idle_callback, callback_data);
	}
}

static void
attach_view (NautilusViewFrame *view,
	     Bonobo_Unknown component)
{
	CORBA_Environment ev;
	GtkWidget *widget;
  	
	/* Either create an adapter or query for the Nautilus:View
	 * interface. Either way, we don't need to keep the original
	 * reference around once that happens.
	 */
	view->details->view = nautilus_component_adapter_factory_create_adapter 
		(nautilus_component_adapter_factory_get (), component);

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

	g_signal_connect_object	(view->details->view_frame, "destroy",
				 G_CALLBACK (view_frame_failed), view, G_CONNECT_SWAPPED);
	g_signal_connect_object	(view->details->view_frame, "system_exception",
				 G_CALLBACK (queue_view_frame_failed), view, G_CONNECT_SWAPPED);
	g_signal_connect_object	(view->details->control_frame, "system_exception",
				 G_CALLBACK (queue_view_frame_failed), view, G_CONNECT_SWAPPED);
	g_signal_connect_object	(widget, "remove",
				 G_CALLBACK (check_socket_gone_callback), view, 0);

	if (view->details->zoomable_frame != NULL) {
		g_signal_connect_object	(view->details->zoomable_frame, "system_exception",
					 G_CALLBACK (queue_view_frame_failed), view, G_CONNECT_SWAPPED);
		g_signal_connect_object (view->details->zoomable_frame, "zoom_parameters_changed",
					 G_CALLBACK (zoom_parameters_changed_callback), view, 0);
		g_signal_connect_object (view->details->zoomable_frame, "zoom_level_changed",
					 G_CALLBACK (zoom_level_changed_callback), view, 0);
	}

	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (view), widget);
	
	view_frame_activated (view);

	/* The frame might already be mapped when the view is activated.
	 * This means we won't get the mapped signal while its active, so
	 * activate it now.
	 */
	if (GTK_WIDGET_MAPPED (GTK_WIDGET (view))) {
		bonobo_control_frame_control_activate (view->details->control_frame);
	}
}

static void
activation_callback (NautilusBonoboActivationHandle *handle,
		     Bonobo_Unknown activated_object,
		     gpointer callback_data)
{
	NautilusViewFrame *view;

	view = NAUTILUS_VIEW_FRAME (callback_data);
	g_assert (view->details->activation_handle == handle);

	view->details->activation_handle = NULL;

	if (activated_object == CORBA_OBJECT_NIL) {
		view_frame_failed (view);
	} else {
		attach_view (view, activated_object);
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
	g_signal_emit (view, signals[TITLE_CHANGED], 0);

	view_frame_wait (view);
	
	CORBA_exception_init (&ev);
	Nautilus_View_load_location (view->details->view, location, &ev);
	if (BONOBO_EX (&ev)) {
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
		if (BONOBO_EX (&ev)) {
			view_frame_failed (view);
		}
		CORBA_exception_free (&ev);
	}
}

void
nautilus_view_frame_selection_changed (NautilusViewFrame *view,
                                       GList *selection)
{
	BonoboArg *arg;
	CORBA_Environment ev;
	Nautilus_URIList *uri_list;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	if (view->details->view == CORBA_OBJECT_NIL) {
		return;
	}

	if (!bonobo_event_source_has_listener
	    (view->details->event_source,
	     "Bonobo/Property:change:selection")) {
		return;
	}

	uri_list = nautilus_uri_list_from_g_list (selection);
	
	CORBA_exception_init (&ev);

	arg = bonobo_arg_new_from (TC_Nautilus_URIList, uri_list);
	CORBA_free (uri_list);
	
	bonobo_event_source_notify_listeners
		(view->details->event_source,
		 "Bonobo/Property:change:selection", arg, &ev);
	
	CORBA_free (arg);

	if (BONOBO_EX (&ev)) {
		view_frame_failed (view);
	}

	CORBA_exception_free (&ev);
}

void
nautilus_view_frame_title_changed (NautilusViewFrame *view,
				   const char *title)
{
	BonoboArg arg;
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	if (view->details->view == CORBA_OBJECT_NIL) {
		return;
	}

	CORBA_exception_init (&ev);

	arg._type = TC_CORBA_string;
	arg._value = &title;

	bonobo_event_source_notify_listeners
		(view->details->event_source,
		 "Bonobo/Property:change:title", &arg, &ev);

	if (BONOBO_EX (&ev)) {
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

float
nautilus_view_frame_get_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);

	if (view->details->zoomable_frame == NULL) {
		return 0.0;
	}

	return view->details->zoom_level;
}

void
nautilus_view_frame_set_zoom_level (NautilusViewFrame *view,
				    float zoom_level)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->zoomable_frame == NULL) {
		return;
	}

	bonobo_zoomable_frame_set_zoom_level (view->details->zoomable_frame, zoom_level);
}

float
nautilus_view_frame_get_min_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);

	if (view->details->zoomable_frame == NULL) {
		return 0.0;
	}

	return view->details->min_zoom_level;
}

float
nautilus_view_frame_get_max_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0.0);
	
	if (view->details->zoomable_frame == NULL) {
		return 0.0;
	}

	return view->details->max_zoom_level;
}

gboolean
nautilus_view_frame_get_has_min_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	if (view->details->zoomable_frame == NULL) {
		return FALSE;
	}

	return view->details->has_min_zoom_level;
}

gboolean
nautilus_view_frame_get_has_max_zoom_level (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	if (view->details->zoomable_frame == NULL) {
		return FALSE;
	}

	return view->details->has_max_zoom_level;
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

gboolean
nautilus_view_frame_get_can_zoom_in (NautilusViewFrame *view)
{       
	return !view->details->has_max_zoom_level ||
                (view->details->zoom_level
                 < view->details->max_zoom_level);

}

gboolean
nautilus_view_frame_get_can_zoom_out (NautilusViewFrame *view)
{       
	return !view->details->has_min_zoom_level ||
                (view->details->zoom_level
                 > view->details->min_zoom_level);
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

char *
nautilus_view_frame_get_first_visible_file (NautilusViewFrame *view)
{
	Nautilus_URI uri;
	char *ret;

	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);

	ret = NULL;
	if (view->details->positionable) {
		CORBA_Environment ev;
		
		CORBA_exception_init (&ev);
		uri = Nautilus_ScrollPositionable_get_first_visible_file (view->details->positionable, &ev);
		ret = g_strdup (uri);
		CORBA_free (uri);
		CORBA_exception_free (&ev);
	}

	return ret;
}

void
nautilus_view_frame_scroll_to_file (NautilusViewFrame *view,
				    const char        *uri)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->positionable) {
		CORBA_Environment ev;
		
		CORBA_exception_init (&ev);

		Nautilus_ScrollPositionable_scroll_to_file (view->details->positionable,
							    uri,
							    &ev);
		CORBA_exception_free (&ev);
	}
}


const char *
nautilus_view_frame_get_view_iid (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);

	return view->details->view_iid;
}

void
nautilus_view_frame_open_location (NautilusViewFrame *view,
				   const char *location,
				   Nautilus_ViewFrame_OpenMode mode,
				   Nautilus_ViewFrame_OpenFlags flags,
				   GList *selection)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->state == VIEW_FRAME_FAILED) {
		return;
	}

	view_frame_wait_is_over (view);
	g_signal_emit (view,
		       signals[OPEN_LOCATION], 0,
		       location, mode, flags, selection);
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
	g_signal_emit (view,
		       signals[REPORT_LOCATION_CHANGE], 0,
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
	g_signal_emit (view,
		       signals[REPORT_REDIRECT], 0,
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
	g_signal_emit (view,
			 signals[CHANGE_SELECTION], 0, selection);
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
	g_signal_emit (view,
		       signals[CHANGE_STATUS], 0, status);
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
	g_signal_emit (view,
		       signals[LOAD_PROGRESS_CHANGED], 0);
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

	g_signal_emit (view, signals[GO_BACK], 0);
}

void
nautilus_view_frame_close_window (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	g_signal_emit (view, signals[CLOSE_WINDOW], 0);
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
		g_signal_emit (view, signals[TITLE_CHANGED], 0);
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

static void
send_history (NautilusViewFrame *view)
{
	Nautilus_History *history;
	CORBA_Environment ev;
	BonoboArg *arg;

	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

	if (view->details->view == CORBA_OBJECT_NIL) {
		return;
	}

	if (!bonobo_event_source_has_listener
	    (view->details->event_source,
	     "Bonobo/Property:change:history")) {
		return;
	}

	history = get_history_list (view);
	if (history == CORBA_OBJECT_NIL) {
		return;
	}

	CORBA_exception_init (&ev);

	arg = bonobo_arg_new_from (TC_Nautilus_History, history);

	CORBA_free (history);
	
	bonobo_event_source_notify_listeners
		(view->details->event_source,
		 "Bonobo/Property:change:history", arg, &ev);
	
	CORBA_free (arg);

	if (BONOBO_EX (&ev)) {
		view_frame_failed (view);
	}

	CORBA_exception_free (&ev);
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

static void
nautilus_view_frame_class_init (NautilusViewFrameClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_view_frame_finalize;
	GTK_OBJECT_CLASS (class)->destroy = nautilus_view_frame_destroy;
	GTK_WIDGET_CLASS (class)->unrealize = nautilus_view_frame_unrealize;
	GTK_WIDGET_CLASS (class)->map = nautilus_view_frame_map;
	
	signals[CHANGE_SELECTION] = g_signal_new
		("change_selection",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  change_selection),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__POINTER,
		 G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[CHANGE_STATUS] = g_signal_new
		("change_status",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  change_status),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__STRING,
		 G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[FAILED] = g_signal_new
		("failed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  failed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[GET_HISTORY_LIST] = g_signal_new
		("get_history_list",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  get_history_list),
		 NULL, NULL,
		 nautilus_marshal_POINTER__VOID,
		 G_TYPE_POINTER, 0);
	signals[GO_BACK] = g_signal_new
		("go_back",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  go_back),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[CLOSE_WINDOW] = g_signal_new
		("close_window",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  close_window),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[LOAD_COMPLETE] = g_signal_new
		("load_complete",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  load_complete),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[LOAD_PROGRESS_CHANGED] = g_signal_new
		("load_progress_changed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  load_progress_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[LOAD_UNDERWAY] = g_signal_new
		("load_underway",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  load_underway),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[OPEN_LOCATION] = g_signal_new
		("open_location",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  open_location),
		 NULL, NULL,
		 eel_marshal_VOID__STRING_LONG_LONG_POINTER,
		 G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_LONG, G_TYPE_LONG, G_TYPE_POINTER);
	signals[REPORT_LOCATION_CHANGE] = g_signal_new
		("report_location_change",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  report_location_change),
		 NULL, NULL,
		 eel_marshal_VOID__STRING_POINTER_STRING,
		 G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING);
	signals[REPORT_REDIRECT] = g_signal_new
		("report_redirect",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  report_redirect),
		 NULL, NULL,
		 eel_marshal_VOID__STRING_STRING_POINTER_STRING,
		 G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING);
	signals[TITLE_CHANGED] = g_signal_new
		("title_changed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  title_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[VIEW_LOADED] = g_signal_new
		("view_loaded",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  view_loaded),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[ZOOM_LEVEL_CHANGED] = g_signal_new
		("zoom_level_changed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  zoom_level_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[ZOOM_PARAMETERS_CHANGED] = g_signal_new
		("zoom_parameters_changed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusViewFrameClass, 
				  zoom_parameters_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
}
