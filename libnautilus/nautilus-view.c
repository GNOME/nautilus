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
#include "nautilus-view.h"

#include "nautilus-bonobo-workarounds.h"
#include "nautilus-idle-queue.h"
#include "nautilus-undo.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <eel/eel-gtk-macros.h>

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
	BonoboControl *control;
	NautilusIdleQueue *incoming_queue;
	NautilusIdleQueue *outgoing_queue;
};

typedef struct {
	POA_Nautilus_View servant;
	NautilusView *bonobo_object;
} impl_POA_Nautilus_View;

typedef void (* ViewFunction) (NautilusView *view,
			       gpointer callback_data);

typedef struct {
	char *from_location;
	char *location;
	GList *selection;
	char *title;
} LocationPlus;

static void impl_Nautilus_View_load_location     (PortableServer_Servant  servant,
						  CORBA_char             *location,
						  CORBA_Environment      *ev);
static void impl_Nautilus_View_stop_loading      (PortableServer_Servant  servant,
						  CORBA_Environment      *ev);
static void impl_Nautilus_View_selection_changed (PortableServer_Servant  servant,
						  const Nautilus_URIList *selection,
						  CORBA_Environment      *ev);
static void impl_Nautilus_View_title_changed     (PortableServer_Servant  servant,
						  const CORBA_char       *title,
						  CORBA_Environment      *ev);
static void impl_Nautilus_View_history_changed   (PortableServer_Servant  servant,
						  const Nautilus_History *history,
						  CORBA_Environment      *ev);
static void nautilus_view_initialize             (NautilusView           *view);
static void nautilus_view_destroy                (GtkObject              *object);
static void nautilus_view_initialize_class       (NautilusViewClass      *klass);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusView, nautilus_view, BONOBO_OBJECT_TYPE)

static POA_Nautilus_View__epv libnautilus_Nautilus_View_epv =
{
	NULL,
	impl_Nautilus_View_load_location,
	impl_Nautilus_View_stop_loading,
	impl_Nautilus_View_selection_changed,
	impl_Nautilus_View_title_changed,
	impl_Nautilus_View_history_changed
};

static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_View__vepv impl_Nautilus_View_vepv =
{
	&base_epv,
	NULL,
	&libnautilus_Nautilus_View_epv
};

static void
queue_incoming_call (PortableServer_Servant servant,
		     ViewFunction call,
		     gpointer callback_data,
		     GDestroyNotify destroy_callback_data)
{
	NautilusView *view;

	view = ((impl_POA_Nautilus_View *) servant)->bonobo_object;

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
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[LOAD_LOCATION],
			 callback_data);
}

static void
call_stop_loading (NautilusView *view,
		   gpointer callback_data)
{
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[STOP_LOADING]);
}

static void
call_selection_changed (NautilusView *view,
			gpointer callback_data)
{
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[SELECTION_CHANGED],
			 callback_data);
}

static void
call_title_changed (NautilusView *view,
		    gpointer callback_data)
{
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[TITLE_CHANGED],
			 callback_data);
}

static void
call_history_changed (NautilusView *view,
		      gpointer callback_data)
{
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[HISTORY_CHANGED],
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
				  CORBA_char *location,
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
impl_Nautilus_View_selection_changed (PortableServer_Servant servant,
				      const Nautilus_URIList *selection,
				      CORBA_Environment *ev)
{
	queue_incoming_call (servant,
			     call_selection_changed,
			     nautilus_g_list_from_uri_list (selection),
			     list_deep_free_cover);
}

static void 
impl_Nautilus_View_title_changed (PortableServer_Servant servant,
				  const CORBA_char *title,
				  CORBA_Environment *ev)
{
	queue_incoming_call (servant,
			     call_title_changed,
			     g_strdup (title),
			     g_free);
}

static void 
impl_Nautilus_View_history_changed (PortableServer_Servant servant,
				    const Nautilus_History *history,
				    CORBA_Environment *ev)
{
	queue_incoming_call (servant,
			     call_history_changed,
			     history_dup (history),
			     CORBA_free);
}

static void
impl_Nautilus_View__destroy (BonoboObject *object,
			     PortableServer_Servant servant)
{
	PortableServer_ObjectId *object_id;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
	PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
	CORBA_free (object_id);

	object->servant = NULL;
	
	POA_Nautilus_View__fini (servant, &ev);
	g_free (servant);

	CORBA_exception_free (&ev);
}

static Nautilus_ViewFrame
impl_Nautilus_View__create (NautilusView *bonobo_object,
			    CORBA_Environment * ev)
{
	impl_POA_Nautilus_View *servant;
	
	impl_Nautilus_View_vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();

	servant = g_new0 (impl_POA_Nautilus_View, 1);
	servant->servant.vepv = &impl_Nautilus_View_vepv;
	POA_Nautilus_View__init ((PortableServer_Servant) servant, ev);

	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (impl_Nautilus_View__destroy), servant);
	
	servant->bonobo_object = bonobo_object;
	return bonobo_object_activate_servant (BONOBO_OBJECT (bonobo_object), servant);
}

static void
nautilus_view_initialize_class (NautilusViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*) klass;

	object_class->destroy = nautilus_view_destroy;
	
	signals[LOAD_LOCATION] =
		gtk_signal_new ("load_location",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusViewClass, load_location),
			       gtk_marshal_NONE__STRING,
			       GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[STOP_LOADING] =
		gtk_signal_new ("stop_loading",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusViewClass, stop_loading),
			       gtk_marshal_NONE__NONE,
			       GTK_TYPE_NONE, 0);
	signals[SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusViewClass, selection_changed),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[TITLE_CHANGED] =
		gtk_signal_new ("title_changed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusViewClass, title_changed),
			       gtk_marshal_NONE__STRING,
			       GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[HISTORY_CHANGED] =
		gtk_signal_new ("history_changed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusViewClass, history_changed),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_view_initialize (NautilusView *view)
{
	CORBA_Environment ev;

	view->details = g_new0 (NautilusViewDetails, 1);
	
	view->details->incoming_queue = nautilus_idle_queue_new ();
	view->details->outgoing_queue = nautilus_idle_queue_new ();

	CORBA_exception_init (&ev);
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
	return nautilus_view_construct_from_bonobo_control 
		(NAUTILUS_VIEW (gtk_object_new (NAUTILUS_TYPE_VIEW, NULL)), 
		 control);
}

NautilusView *
nautilus_view_construct (NautilusView   *view,
			 GtkWidget      *widget)
{
	return nautilus_view_construct_from_bonobo_control
		(view, bonobo_control_new (widget));
}

static void
set_frame_callback (BonoboControl *control,
		    gpointer callback_data)
{
	nautilus_bonobo_object_force_destroy_when_owner_disappears
		(BONOBO_OBJECT (control),
		 bonobo_control_get_control_frame (control));
}

static void
widget_destroyed_callback (GtkWidget *widget,
			   gpointer callback_data)
{
	g_assert (NAUTILUS_IS_VIEW (callback_data));

	nautilus_bonobo_object_force_destroy_later
		(BONOBO_OBJECT (callback_data));
}

NautilusView *
nautilus_view_construct_from_bonobo_control (NautilusView   *view,
					     BonoboControl  *control)
{
	GtkWidget *widget;

	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	g_return_val_if_fail (BONOBO_IS_CONTROL (control), NULL);

	view->details->control = control;
	bonobo_object_add_interface (BONOBO_OBJECT (view), BONOBO_OBJECT (control));
	nautilus_undo_set_up_bonobo_control (control);

	gtk_signal_connect (GTK_OBJECT (control), "set_frame",
			    set_frame_callback, NULL);

	widget = bonobo_control_get_widget (control);
	gtk_signal_connect_while_alive (GTK_OBJECT (widget), "destroy",
					widget_destroyed_callback, view,
					GTK_OBJECT (view));

	return view;
}

static void
nautilus_view_destroy (GtkObject *object)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (object);

	nautilus_idle_queue_destroy (view->details->incoming_queue);
	nautilus_idle_queue_destroy (view->details->outgoing_queue);

	g_free (view->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static Nautilus_ViewFrame
view_frame_call_begin (NautilusView *view, CORBA_Environment *ev)
{
	Nautilus_ViewFrame view_frame;

	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), CORBA_OBJECT_NIL);
	
	CORBA_exception_init (ev);

	view_frame = Bonobo_Unknown_queryInterface 
		(bonobo_control_get_control_frame (nautilus_view_get_bonobo_control (view)),
		 "IDL:Nautilus/ViewFrame:1.0", ev);

	if (ev->_major != CORBA_NO_EXCEPTION) {
		view_frame = CORBA_OBJECT_NIL;
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

/* Can't use the one in libnautilus-private. */
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
	ui_container = bonobo_control_get_remote_ui_container (view->details->control);
	bonobo_ui_component_set_container (ui_component, ui_container);
	bonobo_object_release_unref (ui_container, NULL);

	/* Set up the UI from an XML file. */
	bonobo_ui_util_set_ui (ui_component, datadir, ui_file_name, application_name);

	return ui_component;
}
