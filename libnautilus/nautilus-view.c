/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000, 2001 Eazel, Inc.
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
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Maciej Stachowiak <mjs@eazel.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */

/* nautilus-view.c: Implementation for object that represents a
   nautilus view implementation. */

#include <config.h>
#include <string.h>
#include "nautilus-view.h"

#include "nautilus-idle-queue.h"
#include "nautilus-undo.h"
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-event-source.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-utils.h>

enum {
	HISTORY_CHANGED,
	LOAD_LOCATION,
	SELECTION_CHANGED,
	STOP_LOADING,
	TITLE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct NautilusViewDetails {
	BonoboControl           *control;

	NautilusViewListenerMask listen_mask;
	BonoboListener          *listener;
	Bonobo_EventSource       event_source;

	NautilusIdleQueue       *incoming_queue;
	NautilusIdleQueue       *outgoing_queue;
};

typedef void (* ViewFunction) (NautilusView *view,
			       gpointer callback_data);

typedef struct {
	char *from_location;
	char *location;
	GList *selection;
	char *title;
} LocationPlus;

BONOBO_CLASS_BOILERPLATE_FULL (NautilusView, nautilus_view, Nautilus_View,
			       BonoboObject, BONOBO_OBJECT_TYPE)

static void
queue_incoming_call (PortableServer_Servant servant,
		     ViewFunction call,
		     gpointer callback_data,
		     GDestroyNotify destroy_callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (bonobo_object_from_servant (servant));

	nautilus_idle_queue_add (view->details->incoming_queue,
				 (GFunc) call,
				 view,
				 callback_data,
				 destroy_callback_data);
}

static void
queue_outgoing_call (NautilusView *view,
		     ViewFunction call,
		     gpointer callback_data,
		     GDestroyNotify destroy_callback_data)
{
	nautilus_idle_queue_add (view->details->outgoing_queue,
				 (GFunc) call,
				 view,
				 callback_data,
				 destroy_callback_data);
}

GList *
nautilus_g_list_from_uri_list (const Nautilus_URIList *uri_list)
{
	GList *list;
	guint i;

	list = NULL;
	for (i = 0; i < uri_list->_length; i++) {
		list = g_list_prepend
			(list, g_strdup (uri_list->_buffer[i]));
	}
	return g_list_reverse (list);
}

/* Must CORBA_free this list before destroying the URI's in
 * the source list.
 */
Nautilus_URIList *
nautilus_uri_list_from_g_list (GList *list)
{
	int length;
	Nautilus_URIList *uri_list;
	int i;
	GList *p;

	length = g_list_length (list);

	uri_list = Nautilus_URIList__alloc ();
	uri_list->_maximum = length;
	uri_list->_length = length;
	uri_list->_buffer = CORBA_sequence_Nautilus_URI_allocbuf (length);
	for (i = 0, p = list; i < length; i++, p = p->next) {
		g_assert (p != NULL);
		uri_list->_buffer[i] = CORBA_string_dup (p->data);
	}
	CORBA_sequence_set_release (uri_list, CORBA_TRUE);

	return uri_list;
}

static void
call_load_location (NautilusView *view,
		    gpointer callback_data)
{
	g_signal_emit (view,
		       signals[LOAD_LOCATION], 0,
		       callback_data);
}

static void
call_stop_loading (NautilusView *view,
		   gpointer callback_data)
{
	g_signal_emit (view,
		       signals[STOP_LOADING], 0);
}

static void
call_selection_changed (NautilusView *view,
			gpointer callback_data)
{
	g_signal_emit (view,
		       signals[SELECTION_CHANGED], 0,
		       callback_data);
}

static void
call_title_changed (NautilusView *view,
		    gpointer callback_data)
{
	g_signal_emit (view,
		       signals[TITLE_CHANGED], 0,
		       callback_data);
}

static void
call_history_changed (NautilusView *view,
		      gpointer callback_data)
{
	g_signal_emit (view,
		       signals[HISTORY_CHANGED], 0,
		       callback_data);
}

static void
list_deep_free_cover (gpointer callback_data)
{
	gnome_vfs_list_deep_free (callback_data);
}

static Nautilus_History *
history_dup (const Nautilus_History *history)
{
	Nautilus_History *dup;
	int length, i;

	length = history->_length;

	dup = Nautilus_History__alloc ();
	dup->_maximum = length;
	dup->_length = length;
	dup->_buffer = CORBA_sequence_Nautilus_HistoryItem_allocbuf (length);
	for (i = 0; i < length; i++) {
		dup->_buffer[i].title = CORBA_string_dup (history->_buffer[i].title);
		dup->_buffer[i].location = CORBA_string_dup (history->_buffer[i].location);
		dup->_buffer[i].icon = CORBA_string_dup (history->_buffer[i].icon);
	}
	CORBA_sequence_set_release (dup, CORBA_TRUE);

	return dup;
}

static void
impl_Nautilus_View_load_location (PortableServer_Servant servant,
				  const CORBA_char *location,
				  CORBA_Environment *ev)
{
	queue_incoming_call (servant,
			     call_load_location,
			     g_strdup (location),
			     g_free);
}

static void
impl_Nautilus_View_stop_loading (PortableServer_Servant servant,
				 CORBA_Environment *ev)
{
	queue_incoming_call (servant,
			     call_stop_loading,
			     NULL,
			     NULL);
}

static void 
nautilus_view_frame_property_changed_callback (BonoboListener    *listener,
					       const char        *event_name, 
					       const CORBA_any   *any,
					       CORBA_Environment *ev,
					       gpointer           user_data)
{
	NautilusView  *view;
	ViewFunction   callback;
	gpointer       callback_data;
	GDestroyNotify destroy_callback_data;

	view = user_data;

	g_return_if_fail (NAUTILUS_IS_VIEW (user_data));

	if (strcmp (event_name, "Bonobo/Property:change:title") == 0) {

		callback = call_title_changed;
		callback_data = g_strdup (BONOBO_ARG_GET_STRING (any));
		destroy_callback_data = g_free;

	} else if (strcmp (event_name, "Bonobo/Property:change:history") == 0) {


		callback = call_history_changed;
		callback_data = history_dup (any->_value);
		destroy_callback_data = CORBA_free;
					 
	} else if (strcmp (event_name, "Bonobo/Property:change:selection") == 0) {

		callback = call_selection_changed;
		callback_data = nautilus_g_list_from_uri_list (any->_value);
		destroy_callback_data = list_deep_free_cover;
					 
	} else {
		g_warning ("Unknown event '%s'", event_name);
		return;
	}

	nautilus_idle_queue_add (view->details->incoming_queue,
				 (GFunc) callback,
				 view,
				 callback_data,
				 destroy_callback_data);
}

static void
append_mask (GString *str, const char *mask_element)
{
	if (str->len) {
		g_string_append_c (str, ',');
	}
	g_string_append (str, mask_element);
}

/*
 * FIXME: we should use this 'set_frame' method to keep
 * a cached CORBA_Object_duplicated reference to the
 * remote NautilusViewFrame, since this would save lots
 * of remote QI / unref traffic in all the methods that
 * use view_frame_call_begin.
 */
static void
nautilus_view_set_frame (NautilusView       *view,
			 Bonobo_ControlFrame frame)
{
	BonoboListener    *listener;
	CORBA_Environment  ev;
	Bonobo_EventSource es;
	Bonobo_PropertyBag pbag;
	GString           *mask;

	if (view->details->listen_mask == 0) { /* Defer until we need to */
		return;
	}

	CORBA_exception_init (&ev);

	if ((listener = view->details->listener) != NULL) {
		view->details->listener = NULL;

		bonobo_event_source_client_remove_listener (
			view->details->event_source,
			BONOBO_OBJREF (listener), NULL);

		CORBA_Object_release (view->details->event_source, &ev);
		bonobo_object_unref (BONOBO_OBJECT (listener));
	}

	if (frame != CORBA_OBJECT_NIL) {

		pbag = bonobo_control_get_ambient_properties (
			view->details->control, &ev);

		if (BONOBO_EX (&ev)) {
			goto add_failed;
		}
		if (pbag == CORBA_OBJECT_NIL) {
			g_warning ("NautilusView: contractural breakage - "
				   "NautilusViewFrame has no ambient property bag");
			goto add_failed;
		}

		es = Bonobo_Unknown_queryInterface (
			pbag, "IDL:Bonobo/EventSource:1.0", &ev);

		if (BONOBO_EX (&ev)) {
			goto add_failed;
		}

		if (es == CORBA_OBJECT_NIL) {
			g_warning ("Contractural breakage - NautilusViewFrame's "
				   "ambient property bag has no event source");
			goto add_failed;
		}

		view->details->event_source = CORBA_Object_duplicate (es, &ev);
		bonobo_object_release_unref (es, &ev);

		listener = bonobo_listener_new (
			nautilus_view_frame_property_changed_callback, view);

		view->details->listener = listener;

		mask = g_string_sized_new (128);

		if (view->details->listen_mask & NAUTILUS_VIEW_LISTEN_TITLE)
			append_mask (mask, "Bonobo/Property:change:title");

		if (view->details->listen_mask & NAUTILUS_VIEW_LISTEN_HISTORY)
			append_mask (mask, "Bonobo/Property:change:history");

		if (view->details->listen_mask & NAUTILUS_VIEW_LISTEN_SELECTION)
			append_mask (mask, "Bonobo/Property:change:selection");

		Bonobo_EventSource_addListenerWithMask (
			es, BONOBO_OBJREF (listener), mask->str, &ev);

		g_string_free (mask, TRUE);
	}

 add_failed:
	CORBA_exception_free (&ev);
}

static void
nautilus_view_set_frame_callback (BonoboControl *control,
				  NautilusView  *view)
{
	Bonobo_ControlFrame frame;

	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	frame = bonobo_control_get_control_frame (control, NULL);

	nautilus_view_set_frame (view, frame);
}

static void
nautilus_view_instance_init (NautilusView *view)
{
	view->details = g_new0 (NautilusViewDetails, 1);
	
	view->details->incoming_queue = nautilus_idle_queue_new ();
	view->details->outgoing_queue = nautilus_idle_queue_new ();
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
	return nautilus_view_construct_from_bonobo_control 
		(NAUTILUS_VIEW (g_object_new (NAUTILUS_TYPE_VIEW, NULL)), 
		 control);
}

NautilusView *
nautilus_view_construct (NautilusView   *view,
			 GtkWidget      *widget)
{
	return nautilus_view_construct_from_bonobo_control
		(view, bonobo_control_new (widget));
}

NautilusView *
nautilus_view_construct_from_bonobo_control (NautilusView   *view,
					     BonoboControl  *control)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	g_return_val_if_fail (BONOBO_IS_CONTROL (control), NULL);

	view->details->control = control;
	bonobo_object_add_interface (BONOBO_OBJECT (view), BONOBO_OBJECT (control));
	nautilus_undo_set_up_bonobo_control (control);

	g_signal_connect (G_OBJECT (control), "set_frame",
			  G_CALLBACK (nautilus_view_set_frame_callback),
			  view);

	return view;
}

static void
nautilus_view_finalize (GObject *object)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (object);

	nautilus_idle_queue_destroy (view->details->incoming_queue);
	nautilus_idle_queue_destroy (view->details->outgoing_queue);

	g_free (view->details);
	
	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_view_dispose (GObject *object)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (object);

	nautilus_view_set_frame (view, CORBA_OBJECT_NIL);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static Nautilus_ViewFrame
view_frame_call_begin (NautilusView *view, CORBA_Environment *ev)
{
	Bonobo_ControlFrame control_frame;
	Nautilus_ViewFrame view_frame;

	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), CORBA_OBJECT_NIL);
	
	CORBA_exception_init (ev);

	control_frame = bonobo_control_get_control_frame (nautilus_view_get_bonobo_control (view), ev);
	if (ev->_major != CORBA_NO_EXCEPTION || control_frame == CORBA_OBJECT_NIL) {
		return CORBA_OBJECT_NIL;
	}

	view_frame = Bonobo_Unknown_queryInterface (control_frame, "IDL:Nautilus/ViewFrame:1.0", ev);
	if (ev->_major != CORBA_NO_EXCEPTION) {
		return CORBA_OBJECT_NIL;
	}

	return view_frame;
}

static void
view_frame_call_end (Nautilus_ViewFrame frame, CORBA_Environment *ev)
{
	if (frame != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (frame, NULL);
	}

	CORBA_exception_free (ev);
}

/* don't use the one in eel to avoid creating a dependency on eel */
static GList *
str_list_copy (GList *original)
{
	GList *copy, *node;
	
	copy = NULL;
	for (node = original; node != NULL; node = node->next) {
		copy = g_list_prepend (copy, g_strdup (node->data));
	}
	return g_list_reverse (copy);
}

static void
list_free_deep_callback (gpointer callback_data)
{
	gnome_vfs_list_deep_free (callback_data);
}

static void
free_location_plus_callback (gpointer callback_data)
{
	LocationPlus *location_plus;

	location_plus = callback_data;
	g_free (location_plus->from_location);
	g_free (location_plus->location);
	gnome_vfs_list_deep_free (location_plus->selection);
	g_free (location_plus->title);
	g_free (location_plus);
}

static void
call_open_location_in_this_window (NautilusView *view,
				   gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_open_location_in_this_window
			(view_frame, callback_data, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_open_location_prefer_existing_window (NautilusView *view,
					   gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_open_location_prefer_existing_window
			(view_frame, callback_data, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_open_location_force_new_window (NautilusView *view,
				     gpointer callback_data)
{
	LocationPlus *location_plus;
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	Nautilus_URIList *uri_list;

	location_plus = callback_data;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		uri_list = nautilus_uri_list_from_g_list (location_plus->selection);
		Nautilus_ViewFrame_open_location_force_new_window
			(view_frame, location_plus->location, uri_list, &ev);
		CORBA_free (uri_list);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_report_location_change (NautilusView *view,
			     gpointer callback_data)
{
	LocationPlus *location_plus;
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	Nautilus_URIList *uri_list;

	location_plus = callback_data;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		uri_list = nautilus_uri_list_from_g_list (location_plus->selection);
		Nautilus_ViewFrame_report_location_change
			(view_frame,
			 location_plus->location,
			 uri_list,
			 location_plus->title,
			 &ev);
		CORBA_free (uri_list);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_report_redirect (NautilusView *view,
		      gpointer callback_data)
{
	LocationPlus *location_plus;
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	Nautilus_URIList *uri_list;

	location_plus = callback_data;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		uri_list = nautilus_uri_list_from_g_list (location_plus->selection);
		Nautilus_ViewFrame_report_redirect
			(view_frame,
			 location_plus->from_location,
			 location_plus->location,
			 uri_list,
			 location_plus->title,
			 &ev);
		CORBA_free (uri_list);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_report_selection_change (NautilusView *view,
			      gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	Nautilus_URIList *uri_list;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		uri_list = nautilus_uri_list_from_g_list (callback_data);
		Nautilus_ViewFrame_report_selection_change (view_frame, uri_list, &ev);
		CORBA_free (uri_list);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_report_status (NautilusView *view,
		    gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_status (view_frame, callback_data, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_report_load_underway (NautilusView *view,
			   gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_load_underway (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_report_load_progress (NautilusView *view,
			   gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_load_progress
			(view_frame, * (double *) callback_data, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_report_load_complete (NautilusView *view,
			   gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_load_complete (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_report_load_failed (NautilusView *view,
			 gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_load_failed (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_set_title (NautilusView *view,
		gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_set_title (view_frame, callback_data, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

static void
call_go_back (NautilusView *view,
	      gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_go_back (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}


static void
call_close_window (NautilusView *view,
		   gpointer callback_data)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_close_window (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}


void
nautilus_view_open_location_in_this_window (NautilusView *view,
					    const char *location)
{
	queue_outgoing_call (view,
			     call_open_location_in_this_window,
			     g_strdup (location),
			     g_free);
}

void
nautilus_view_open_location_prefer_existing_window (NautilusView *view,
						    const char *location)
{
	queue_outgoing_call (view,
			     call_open_location_prefer_existing_window,
			     g_strdup (location),
			     g_free);
}

void
nautilus_view_open_location_force_new_window (NautilusView *view,
					      const char *location,
					      GList *selection)
{
	LocationPlus *location_plus;

	location_plus = g_new0 (LocationPlus, 1);
	location_plus->location = g_strdup (location);
	location_plus->selection = str_list_copy (selection);

	queue_outgoing_call (view,
			     call_open_location_force_new_window,
			     location_plus,
			     free_location_plus_callback);
}

void
nautilus_view_report_location_change (NautilusView *view,
				      const char *location,
				      GList *selection,
				      const char *title)
{
	LocationPlus *location_plus;

	location_plus = g_new0 (LocationPlus, 1);
	location_plus->location = g_strdup (location);
	location_plus->selection = str_list_copy (selection);
	location_plus->title = g_strdup (title);

	queue_outgoing_call (view,
			     call_report_location_change,
			     location_plus,
			     free_location_plus_callback);
}

void
nautilus_view_report_redirect (NautilusView *view,
			       const char *from_location,
			       const char *to_location,
			       GList *selection,
			       const char *title)
{
	LocationPlus *location_plus;

	location_plus = g_new0 (LocationPlus, 1);
	location_plus->from_location = g_strdup (from_location);
	location_plus->location = g_strdup (to_location);
	location_plus->selection = str_list_copy (selection);
	location_plus->title = g_strdup (title);

	queue_outgoing_call (view,
			     call_report_redirect,
			     location_plus,
			     free_location_plus_callback);
}

void
nautilus_view_report_selection_change (NautilusView *view,
				       GList *selection)
{
	queue_outgoing_call (view,
			     call_report_selection_change,
			     str_list_copy (selection),
			     list_free_deep_callback);
}

void
nautilus_view_report_status (NautilusView *view,
			     const char *status)
{
	queue_outgoing_call (view,
			     call_report_status,
			     g_strdup (status),
			     g_free);
}

void
nautilus_view_report_load_underway (NautilusView *view)
{
	queue_outgoing_call (view,
			     call_report_load_underway,
			     NULL,
			     NULL);
}

void
nautilus_view_report_load_progress (NautilusView *view,
				    double fraction_done)
{
	queue_outgoing_call (view,
			     call_report_load_progress,
			     g_memdup (&fraction_done, sizeof (double)),
			     g_free);
}

void
nautilus_view_report_load_complete (NautilusView *view)
{
	queue_outgoing_call (view,
			     call_report_load_complete,
			     NULL,
			     NULL);
}

void
nautilus_view_report_load_failed (NautilusView *view)
{
	queue_outgoing_call (view,
			     call_report_load_failed,
			     NULL,
			     NULL);
}

void
nautilus_view_set_title (NautilusView *view,
			 const char *title)
{
	queue_outgoing_call (view,
			     call_set_title,
			     g_strdup (title),
			     g_free);
}

void
nautilus_view_go_back (NautilusView *view)
{
	queue_outgoing_call (view,
			     call_go_back,
			     NULL,
			     NULL);
}

void
nautilus_view_close_window (NautilusView *view)
{
	queue_outgoing_call (view,
			     call_close_window,
			     NULL,
			     NULL);
}


BonoboControl *
nautilus_view_get_bonobo_control (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	return view->details->control;
}

BonoboUIComponent *
nautilus_view_set_up_ui (NautilusView *view,
			 const char *datadir,
			 const char *ui_file_name,
			 const char *application_name)
{
	BonoboUIComponent *ui_component;
	Bonobo_UIContainer ui_container;

	/* Get the UI component that's pre-made by the control. */
	ui_component = bonobo_control_get_ui_component (view->details->control);

	/* Connect the UI component to the control frame's UI container. */
	ui_container = bonobo_control_get_remote_ui_container (view->details->control, NULL);
	bonobo_ui_component_set_container (ui_component, ui_container, NULL);
	bonobo_object_release_unref (ui_container, NULL);

	/* Set up the UI from an XML file. */
	bonobo_ui_util_set_ui (ui_component, datadir, ui_file_name, application_name, NULL);

	return ui_component;
}

static void
nautilus_view_class_init (NautilusViewClass *klass)
{
	POA_Nautilus_View__epv *epv = &klass->epv;
	
	G_OBJECT_CLASS (klass)->finalize = nautilus_view_finalize;
	G_OBJECT_CLASS (klass)->dispose = nautilus_view_dispose;
	
	signals[LOAD_LOCATION] =
		g_signal_new ("load_location",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, load_location),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[STOP_LOADING] =
		g_signal_new ("stop_loading",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, stop_loading),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[SELECTION_CHANGED] =
		g_signal_new ("selection_changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, selection_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[TITLE_CHANGED] =
		g_signal_new ("title_changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, title_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[HISTORY_CHANGED] =
		g_signal_new ("history_changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, history_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER);

	epv->load_location     = impl_Nautilus_View_load_location;
	epv->stop_loading      = impl_Nautilus_View_stop_loading;
}

Bonobo_PropertyBag
nautilus_view_get_ambient_properties (NautilusView      *view,
				      CORBA_Environment *opt_ev)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	return bonobo_control_get_ambient_properties (
		view->details->control, opt_ev);
}

void
nautilus_view_set_listener_mask (NautilusView            *view,
				 NautilusViewListenerMask mask)
{
	if (!view->details->listen_mask) {
		Bonobo_ControlFrame frame;

		view->details->listen_mask = mask;
		
		frame = bonobo_control_get_control_frame (
			view->details->control, NULL);

		if (frame != CORBA_OBJECT_NIL) {
			nautilus_view_set_frame (view, frame);
		}
	}
}
