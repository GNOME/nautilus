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
 *           Darin Adler <darin@eazel.com>
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
#include <libnautilus-extensions/nautilus-gtk-macros.h>

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
	NautilusIdleQueue *idle_queue;
};

typedef struct {
	POA_Nautilus_View servant;
	NautilusView *bonobo_object;
} impl_POA_Nautilus_View;

typedef void (* ViewFunction) (NautilusView *view,
			       gpointer callback_data);

typedef struct {
	ViewFunction call;
	gpointer callback_data;
	GDestroyNotify destroy_callback_data;
} IncomingCall;

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

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusView, nautilus_view, BONOBO_OBJECT_TYPE)

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

	nautilus_idle_queue_add (view->details->idle_queue,
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
	
	view->details->idle_queue = nautilus_idle_queue_new ();

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

NautilusView *
nautilus_view_construct_from_bonobo_control (NautilusView   *view,
					     BonoboControl  *control)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	g_return_val_if_fail (BONOBO_IS_CONTROL (control), NULL);

	view->details->control = control;
	bonobo_object_add_interface (BONOBO_OBJECT (view), BONOBO_OBJECT (control));
	nautilus_undo_set_up_bonobo_control (control);

	return view;
}

static void
nautilus_view_destroy (GtkObject *object)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (object);

	nautilus_idle_queue_destroy (view->details->idle_queue);

	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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
		bonobo_object_release_unref (frame, ev);
	}

	CORBA_exception_free (ev);
}

void
nautilus_view_open_location_in_this_window (NautilusView *view,
					    const char *location)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_open_location_in_this_window
			(view_frame, (CORBA_char *) location, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_open_location_prefer_existing_window (NautilusView *view,
						    const char *location)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_open_location_prefer_existing_window
			(view_frame, (CORBA_char *) location, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_open_location_force_new_window (NautilusView *view,
					      const char *location,
					      GList *selection)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	Nautilus_URIList *uri_list;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		uri_list = nautilus_uri_list_from_g_list (selection);
		Nautilus_ViewFrame_open_location_force_new_window
			(view_frame, (CORBA_char *) location, uri_list, &ev);
		CORBA_free (uri_list);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_report_location_change (NautilusView *view,
				      const char *location,
				      GList *selection,
				      const char *title)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	Nautilus_URIList *uri_list;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		uri_list = nautilus_uri_list_from_g_list (selection);
		Nautilus_ViewFrame_report_location_change
			(view_frame, (CORBA_char *) location, uri_list, (CORBA_char *) title, &ev);
		CORBA_free (uri_list);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_report_selection_change (NautilusView *view,
				       GList *selection)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	Nautilus_URIList *uri_list;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		uri_list = nautilus_uri_list_from_g_list (selection);
		Nautilus_ViewFrame_report_selection_change (view_frame, uri_list, &ev);
		CORBA_free (uri_list);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_report_status (NautilusView *view,
			     const char *status)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_status (view_frame, status, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_report_load_underway (NautilusView *view)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_load_underway (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_report_load_progress (NautilusView *view,
				    double fraction_done)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_load_progress (view_frame, fraction_done, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_report_load_complete (NautilusView *view)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_load_complete (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_report_load_failed (NautilusView *view)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_report_load_failed (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_set_title (NautilusView *view,
			 const char *title)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_set_title (view_frame, title, &ev);
	}
	view_frame_call_end (view_frame, &ev);
}

void
nautilus_view_go_back (NautilusView *view)
{
	CORBA_Environment ev;
	Nautilus_ViewFrame view_frame;
	
	view_frame = view_frame_call_begin (view, &ev);
	if (view_frame != CORBA_OBJECT_NIL) {
		Nautilus_ViewFrame_go_back (view_frame, &ev);
	}
	view_frame_call_end (view_frame, &ev);
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
