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

/* nautilus-view-frame-corba.c: CORBA server implementation of the object
   representing a data view frame. */

#include <config.h>

#include "nautilus-view-frame-private.h"
#include "nautilus-window.h"
#include <bonobo/bonobo-main.h>
#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>
#include <libnautilus/nautilus-view.h>
#include <eel/eel-gtk-extensions.h>

typedef struct {
	char *from_location;
	char *location;
	GList *selection;
	char *title;
} LocationPlus;

static void impl_Nautilus_ViewFrame_open_location_in_this_window         (PortableServer_Servant  servant,
									  Nautilus_URI            location,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_open_location_prefer_existing_window (PortableServer_Servant  servant,
									  Nautilus_URI            location,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_open_location_force_new_window       (PortableServer_Servant  servant,
									  Nautilus_URI            location,
									  const Nautilus_URIList *selection,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_report_location_change               (PortableServer_Servant  servant,
									  Nautilus_URI            location,
									  const Nautilus_URIList *selection,
									  const CORBA_char       *title,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_report_redirect                      (PortableServer_Servant  servant,
									  Nautilus_URI            from_location,
									  Nautilus_URI            to_location,
									  const Nautilus_URIList *selection,
									  const CORBA_char       *title,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_report_selection_change              (PortableServer_Servant  servant,
									  const Nautilus_URIList *selection,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_report_status                        (PortableServer_Servant  servant,
									  const CORBA_char       *status,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_report_load_underway                 (PortableServer_Servant  servant,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_report_load_progress                 (PortableServer_Servant  servant,
									  CORBA_float             fraction_done,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_report_load_complete                 (PortableServer_Servant  servant,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_report_load_failed                   (PortableServer_Servant  servant,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_set_title                            (PortableServer_Servant  servant,
									  const CORBA_char       *title,
									  CORBA_Environment      *ev);
static void impl_Nautilus_ViewFrame_go_back                              (PortableServer_Servant  servant,
									  CORBA_Environment      *ev);

POA_Nautilus_ViewFrame__epv impl_Nautilus_ViewFrame_epv =
{
	NULL,
	&impl_Nautilus_ViewFrame_open_location_in_this_window,
	&impl_Nautilus_ViewFrame_open_location_prefer_existing_window,
	&impl_Nautilus_ViewFrame_open_location_force_new_window,
	&impl_Nautilus_ViewFrame_report_location_change,
	&impl_Nautilus_ViewFrame_report_redirect,
	&impl_Nautilus_ViewFrame_report_selection_change,
	&impl_Nautilus_ViewFrame_report_status,
	&impl_Nautilus_ViewFrame_report_load_underway,
	&impl_Nautilus_ViewFrame_report_load_progress,
	&impl_Nautilus_ViewFrame_report_load_complete,
	&impl_Nautilus_ViewFrame_report_load_failed,
	&impl_Nautilus_ViewFrame_set_title,
	&impl_Nautilus_ViewFrame_go_back,
};

static PortableServer_ServantBase__epv base_epv;
POA_Nautilus_ViewFrame__vepv impl_Nautilus_ViewFrame_vepv =
{
	&base_epv,
	NULL,
	&impl_Nautilus_ViewFrame_epv
};

static void
impl_Nautilus_ViewFrame__destroy (BonoboObject *object,
				  impl_POA_Nautilus_ViewFrame *servant)
{
	PortableServer_ObjectId *object_id;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	eel_nullify_cancel (&servant->view);
	object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
	PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
	CORBA_free (object_id);
	object->servant = NULL;
	
	POA_Nautilus_ViewFrame__fini ((PortableServer_Servant) servant, &ev);

	g_free (servant);
	CORBA_exception_free (&ev);
}

BonoboObject *
impl_Nautilus_ViewFrame__create (NautilusViewFrame *view, CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	impl_POA_Nautilus_ViewFrame *servant;

	impl_Nautilus_ViewFrame_vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();

	servant = g_new0 (impl_POA_Nautilus_ViewFrame, 1);
	servant->servant.vepv = &impl_Nautilus_ViewFrame_vepv;
	servant->view = view;
	POA_Nautilus_ViewFrame__init ((PortableServer_Servant) servant, ev);
  
	bonobo_object = bonobo_object_new_from_servant (servant);
  
	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (impl_Nautilus_ViewFrame__destroy), servant);

	eel_nullify_when_destroyed (&servant->view);
  
	return bonobo_object;
}

static void
list_free_deep_callback (gpointer callback_data)
{
	eel_g_list_free_deep (callback_data);
}

static void
free_location_plus_callback (gpointer callback_data)
{
	LocationPlus *location_plus;

	location_plus = callback_data;
	g_free (location_plus->from_location);
	g_free (location_plus->location);
	eel_g_list_free_deep (location_plus->selection);
	g_free (location_plus->title);
	g_free (location_plus);
}

static void
open_in_this_window (NautilusViewFrame *view,
		     gpointer callback_data)
{
	nautilus_view_frame_open_location_in_this_window (view, callback_data);
}

static void
open_prefer_existing_window (NautilusViewFrame *view,
			     gpointer callback_data)
{
	nautilus_view_frame_open_location_prefer_existing_window (view, callback_data);
}

static void
open_force_new_window (NautilusViewFrame *view,
		       gpointer callback_data)
{
	LocationPlus *location_plus;

	location_plus = callback_data;
	nautilus_view_frame_open_location_force_new_window
		(view,
		 location_plus->location,
		 location_plus->selection);
}

static void
report_location_change (NautilusViewFrame *view,
			gpointer callback_data)
{
	LocationPlus *location_plus;

	location_plus = callback_data;
	nautilus_view_frame_report_location_change
		(view,
		 location_plus->location,
		 location_plus->selection,
		 location_plus->title);
}

static void
report_redirect (NautilusViewFrame *view,
		 gpointer callback_data)
{
	LocationPlus *location_plus;

	location_plus = callback_data;
	nautilus_view_frame_report_redirect
		(view,
		 location_plus->from_location,
		 location_plus->location,
		 location_plus->selection,
		 location_plus->title);
}

static void
report_selection_change (NautilusViewFrame *view,
			 gpointer callback_data)
{
	nautilus_view_frame_report_selection_change (view, callback_data);
}

static void
report_status (NautilusViewFrame *view,
	       gpointer callback_data)
{
	nautilus_view_frame_report_status (view, callback_data);
}

static void
report_load_underway (NautilusViewFrame *view,
		      gpointer callback_data)
{
	nautilus_view_frame_report_load_underway (view);
}

static void
report_load_progress (NautilusViewFrame *view,
		      gpointer callback_data)
{
	nautilus_view_frame_report_load_progress (view, * (float *) callback_data);
}

static void
report_load_complete (NautilusViewFrame *view,
		      gpointer callback_data)
{
	nautilus_view_frame_report_load_complete (view);
}

static void
report_load_failed (NautilusViewFrame *view,
		      gpointer callback_data)
{
	nautilus_view_frame_report_load_failed (view);
}

static void
set_title (NautilusViewFrame *view,
	   gpointer callback_data)
{
	nautilus_view_frame_set_title (view, callback_data);
}

static void
go_back (NautilusViewFrame *view,
	 gpointer callback_data)
{
	nautilus_view_frame_go_back (view);
}

static void
impl_Nautilus_ViewFrame_open_location_in_this_window (PortableServer_Servant servant,
						      Nautilus_URI location,
						      CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant,
		 open_in_this_window,
		 g_strdup (location),
		 g_free);
}

static void
impl_Nautilus_ViewFrame_open_location_prefer_existing_window (PortableServer_Servant servant,
							      Nautilus_URI location,
							      CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant,
		 open_prefer_existing_window,
		 g_strdup (location),
		 g_free);
}

static void
impl_Nautilus_ViewFrame_open_location_force_new_window (PortableServer_Servant servant,
							Nautilus_URI location,
							const Nautilus_URIList *selection,
							CORBA_Environment *ev)
{
	LocationPlus *location_plus;

	location_plus = g_new0 (LocationPlus, 1);
	location_plus->location = g_strdup (location);
	location_plus->selection = nautilus_g_list_from_uri_list (selection);

	nautilus_view_frame_queue_incoming_call
		(servant,
		 open_force_new_window,
		 location_plus,
		 free_location_plus_callback);
}

static void
impl_Nautilus_ViewFrame_report_location_change (PortableServer_Servant servant,
						Nautilus_URI location,
						const Nautilus_URIList *selection,
						const CORBA_char *title,
						CORBA_Environment *ev)
{
	LocationPlus *location_plus;

	location_plus = g_new0 (LocationPlus, 1);
	location_plus->location = g_strdup (location);
	location_plus->selection = nautilus_g_list_from_uri_list (selection);
	location_plus->title = g_strdup (title);

	nautilus_view_frame_queue_incoming_call
		(servant,
		 report_location_change,
		 location_plus,
		 free_location_plus_callback);
}

static void
impl_Nautilus_ViewFrame_report_redirect (PortableServer_Servant servant,
					 Nautilus_URI from_location,
					 Nautilus_URI to_location,
					 const Nautilus_URIList *selection,
					 const CORBA_char *title,
					 CORBA_Environment *ev)
{
	LocationPlus *location_plus;

	location_plus = g_new0 (LocationPlus, 1);
	location_plus->from_location = g_strdup (from_location);
	location_plus->location = g_strdup (to_location);
	location_plus->selection = nautilus_g_list_from_uri_list (selection);
	location_plus->title = g_strdup (title);

	nautilus_view_frame_queue_incoming_call
		(servant,
		 report_redirect,
		 location_plus,
		 free_location_plus_callback);
}

static void
impl_Nautilus_ViewFrame_report_selection_change (PortableServer_Servant servant,
						 const Nautilus_URIList *selection,
						 CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant,
		 report_selection_change,
		 nautilus_g_list_from_uri_list (selection),
		 list_free_deep_callback);
}

static void
impl_Nautilus_ViewFrame_report_status (PortableServer_Servant servant,
				       const CORBA_char *status,
				       CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant,
		 report_status,
		 g_strdup (status),
		 g_free);
}

static void
impl_Nautilus_ViewFrame_report_load_underway (PortableServer_Servant servant,
					      CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant,
		 report_load_underway,
		 NULL,
		 NULL);
}

static void
impl_Nautilus_ViewFrame_report_load_progress (PortableServer_Servant servant,
					      CORBA_float fraction_done,
					      CORBA_Environment *ev)
{
	float *copy;

	copy = g_new (float, 1);
	*copy = fraction_done;
	nautilus_view_frame_queue_incoming_call
		(servant,
		 report_load_progress,
		 copy,
		 g_free);
}

static void
impl_Nautilus_ViewFrame_report_load_complete (PortableServer_Servant servant,
					      CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant,
		 report_load_complete,
		 NULL,
		 NULL);
}

static void
impl_Nautilus_ViewFrame_report_load_failed (PortableServer_Servant servant,
					    CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant,
		 report_load_failed,
		 NULL,
		 NULL);
}

static void
impl_Nautilus_ViewFrame_set_title (PortableServer_Servant servant,
				   const CORBA_char *title,
				   CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant,
		 set_title,
		 g_strdup (title),
		 g_free);
}

static void
impl_Nautilus_ViewFrame_go_back (PortableServer_Servant servant,
				 CORBA_Environment *ev)
{
	nautilus_view_frame_queue_incoming_call
		(servant, go_back, NULL, NULL);
}
