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

/* nautilus-view-frame-corba.c: CORBA server implementation of the object
   representing a data view frame. */

#define DEBUG_MATHIEU 0

#include <config.h>

#include "nautilus-view-frame-private.h"
#include "nautilus-window.h"
#include <bonobo/bonobo-main.h>
#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>
#include <libnautilus/nautilus-view.h>

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

POA_Nautilus_ViewFrame__epv impl_Nautilus_ViewFrame_epv =
{
	NULL,
	&impl_Nautilus_ViewFrame_open_location_in_this_window,
	&impl_Nautilus_ViewFrame_open_location_prefer_existing_window,
	&impl_Nautilus_ViewFrame_open_location_force_new_window,
	&impl_Nautilus_ViewFrame_report_selection_change,
	&impl_Nautilus_ViewFrame_report_status,
	&impl_Nautilus_ViewFrame_report_load_underway,
	&impl_Nautilus_ViewFrame_report_load_progress,
	&impl_Nautilus_ViewFrame_report_load_complete,
	&impl_Nautilus_ViewFrame_report_load_failed,
	&impl_Nautilus_ViewFrame_set_title,
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
  
	return bonobo_object;
}

static void
impl_Nautilus_ViewFrame_open_location_in_this_window (PortableServer_Servant servant,
						      Nautilus_URI location,
						      CORBA_Environment *ev)
{
#if DEBUG_MATHIEU
	g_print ("open location \n");
#endif /* DEBUG_MATHIEU */

	nautilus_view_frame_open_location_in_this_window
		(((impl_POA_Nautilus_ViewFrame *) servant)->view, location);
}

static void
impl_Nautilus_ViewFrame_open_location_prefer_existing_window (PortableServer_Servant servant,
							      Nautilus_URI location,
							      CORBA_Environment *ev)
{
	nautilus_view_frame_open_location_prefer_existing_window
		(((impl_POA_Nautilus_ViewFrame *) servant)->view, location);
}

static void
impl_Nautilus_ViewFrame_open_location_force_new_window (PortableServer_Servant servant,
							Nautilus_URI location,
							const Nautilus_URIList *selection,
							CORBA_Environment *ev)
{
	GList *selection_as_g_list;

#if DEBUG_MATHIEU
	g_print ("open location in new window\n");
#endif /* DEBUG_MATHIEU */

	selection_as_g_list = nautilus_shallow_g_list_from_uri_list (selection);
	nautilus_view_frame_open_location_force_new_window
		(((impl_POA_Nautilus_ViewFrame *) servant)->view, location, selection_as_g_list);
	g_list_free (selection_as_g_list);
}

static void
impl_Nautilus_ViewFrame_report_selection_change (PortableServer_Servant servant,
						 const Nautilus_URIList *selection,
						 CORBA_Environment *ev)
{
	GList *selection_as_g_list;

#if DEBUG_MATHIEU
	g_print ("selection change \n");
#endif /* DEBUG_MATHIEU */
	
	selection_as_g_list = nautilus_shallow_g_list_from_uri_list (selection);
	nautilus_view_frame_report_selection_change
		(((impl_POA_Nautilus_ViewFrame *) servant)->view, selection_as_g_list);
	g_list_free (selection_as_g_list);
}

static void
impl_Nautilus_ViewFrame_report_status (PortableServer_Servant servant,
				       const CORBA_char *status,
				       CORBA_Environment *ev)
{
#if DEBUG_MATHIEU
	g_print ("report status \n");
#endif /* DEBUG_MATHIEU */

	nautilus_view_frame_report_status
		(((impl_POA_Nautilus_ViewFrame *) servant)->view, status);
}

static void
impl_Nautilus_ViewFrame_report_load_underway (PortableServer_Servant servant,
					      CORBA_Environment *ev)
{
#if DEBUG_MATHIEU
	g_print ("report load underway \n");
#endif /* DEBUG_MATHIEU */

	nautilus_view_frame_report_load_underway
		(((impl_POA_Nautilus_ViewFrame *) servant)->view);
}

static void
impl_Nautilus_ViewFrame_report_load_progress (PortableServer_Servant servant,
					      CORBA_float fraction_done,
					      CORBA_Environment *ev)
{
#if DEBUG_MATHIEU
	g_print ("report load progress \n");
#endif /* DEBUG_MATHIEU */

	nautilus_view_frame_report_load_progress
		(((impl_POA_Nautilus_ViewFrame *) servant)->view, fraction_done);
}

static void
impl_Nautilus_ViewFrame_report_load_complete (PortableServer_Servant servant,
					      CORBA_Environment *ev)
{
#if DEBUG_MATHIEU
	g_print ("report load complete \n");
#endif /* DEBUG_MATHIEU */

	nautilus_view_frame_report_load_complete
		(((impl_POA_Nautilus_ViewFrame *) servant)->view);
}

static void
impl_Nautilus_ViewFrame_report_load_failed (PortableServer_Servant servant,
					    CORBA_Environment *ev)
{
#if DEBUG_MATHIEU
	g_print ("report load failed \n");
#endif /* DEBUG_MATHIEU */

	nautilus_view_frame_report_load_failed
		(((impl_POA_Nautilus_ViewFrame *) servant)->view);
}

static void
impl_Nautilus_ViewFrame_set_title (PortableServer_Servant servant,
				   const CORBA_char *title,
				   CORBA_Environment *ev)
{
#if DEBUG_MATHIEU
	g_print ("set title \n");
#endif /* DEBUG_MATHIEU */

	nautilus_view_frame_set_title
		(((impl_POA_Nautilus_ViewFrame *) servant)->view, title);
}
