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
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-view.h>

typedef struct {
	BonoboObject parent;
	NautilusViewFrame *widget;
} NautilusViewFrameCorbaPart;

typedef struct {
	BonoboObjectClass parent;
	POA_Nautilus_ViewFrame__epv epv;
} NautilusViewFrameCorbaPartClass;

typedef struct {
	char *from_location;
	char *location;
	GList *selection;
	char *title;
} LocationPlus;

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
						      const CORBA_char *location,
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
							      const CORBA_char *location,
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
							const CORBA_char *location,
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
						const CORBA_char *location,
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
					 const CORBA_char *from_location,
					 const CORBA_char *to_location,
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

static void
nautilus_view_frame_corba_part_class_init (NautilusViewFrameCorbaPartClass *class)
{
	class->epv.open_location_in_this_window = impl_Nautilus_ViewFrame_open_location_in_this_window;
	class->epv.open_location_prefer_existing_window = impl_Nautilus_ViewFrame_open_location_prefer_existing_window;
	class->epv.open_location_force_new_window = impl_Nautilus_ViewFrame_open_location_force_new_window;
	class->epv.report_location_change = impl_Nautilus_ViewFrame_report_location_change;
	class->epv.report_redirect = impl_Nautilus_ViewFrame_report_redirect;
	class->epv.report_selection_change = impl_Nautilus_ViewFrame_report_selection_change;
	class->epv.report_status = impl_Nautilus_ViewFrame_report_status;
	class->epv.report_load_underway = impl_Nautilus_ViewFrame_report_load_underway;
	class->epv.report_load_progress = impl_Nautilus_ViewFrame_report_load_progress;
	class->epv.report_load_complete = impl_Nautilus_ViewFrame_report_load_complete;
	class->epv.report_load_failed = impl_Nautilus_ViewFrame_report_load_failed;
	class->epv.set_title = impl_Nautilus_ViewFrame_set_title;
	class->epv.go_back = impl_Nautilus_ViewFrame_go_back;
}

static void
nautilus_view_frame_corba_part_init (NautilusViewFrameCorbaPart *frame)
{
}

static GType nautilus_view_frame_corba_part_get_type (void);

EEL_DEFINE_BONOBO_BOILERPLATE (NautilusViewFrameCorbaPart,
			       nautilus_view_frame_corba_part,
			       BONOBO_OBJECT_TYPE)

#define NAUTILUS_TYPE_VIEW_FRAME_CORBA_PART nautilus_view_frame_corba_part_get_type ()
#define NAUTILUS_VIEW_FRAME_CORBA_PART(object) G_TYPE_CHECK_INSTANCE_CAST ((object), NAUTILUS_TYPE_VIEW_FRAME_CORBA_PART, NautilusViewFrameCorbaPart)

BonoboObject *
nautilus_view_frame_create_corba_part (NautilusViewFrame *widget)
{
	NautilusViewFrameCorbaPart *part;

	part = NAUTILUS_VIEW_FRAME_CORBA_PART (gtk_object_new (NAUTILUS_TYPE_VIEW_FRAME_CORBA_PART, NULL));
	part->widget = widget;
	return BONOBO_OBJECT (part);
}

NautilusViewFrame *
nautilus_view_frame_from_servant (PortableServer_Servant servant)
{
	return NAUTILUS_VIEW_FRAME_CORBA_PART (bonobo_object_from_servant (servant))->widget;
}
