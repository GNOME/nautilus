/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Gene Z. Ragan <gzr@eazel.com>
 *
 */

/* nautilus-history-frame.c: CORBA server implementation of
   Nautilus::HistoryFrame interface of a Nautilus HistoryFrame. */

#include <config.h>
#include "nautilus-history-frame.h"

typedef struct {
	POA_Nautilus_HistoryFrame servant;
	BonoboObject *bonobo_object;
	NautilusViewFrame *view;
} impl_POA_Nautilus_HistoryFrame;

static Nautilus_History *impl_Nautilus_HistoryFrame_get_history_list (PortableServer_Servant  servant,
								      CORBA_Environment      *ev);

POA_Nautilus_HistoryFrame__epv impl_Nautilus_HistoryFrame_epv =
{
	NULL,
	impl_Nautilus_HistoryFrame_get_history_list,
};

static PortableServer_ServantBase__epv base_epv;
POA_Nautilus_HistoryFrame__vepv impl_Nautilus_HistoryFrame_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_HistoryFrame_epv
};

static void
impl_Nautilus_HistoryFrame__destroy (BonoboObject *object, 
                                      impl_POA_Nautilus_HistoryFrame *servant)
{
   PortableServer_ObjectId *object_id;
   CORBA_Environment ev;

   CORBA_exception_init (&ev);

   object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
   PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
   CORBA_free (object_id);
   object->servant = NULL;

   POA_Nautilus_HistoryFrame__fini ((PortableServer_Servant) servant, &ev);
   g_free (servant);

   CORBA_exception_free (&ev);
}

BonoboObject *
impl_Nautilus_HistoryFrame__create (NautilusViewFrame *view, 
                                     CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	impl_POA_Nautilus_HistoryFrame *servant;

	impl_Nautilus_HistoryFrame_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv();

	servant = g_new0 (impl_POA_Nautilus_HistoryFrame, 1);
	servant->servant.vepv = &impl_Nautilus_HistoryFrame_vepv;
	servant->view = view;
	POA_Nautilus_HistoryFrame__init ((PortableServer_Servant) servant, ev);

	bonobo_object = bonobo_object_new_from_servant (servant);

	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy", 
                       GTK_SIGNAL_FUNC (impl_Nautilus_HistoryFrame__destroy), servant);

	return bonobo_object;
}

static Nautilus_History *
impl_Nautilus_HistoryFrame_get_history_list (PortableServer_Servant servant,
					     CORBA_Environment *ev)
{
	return nautilus_view_frame_get_history_list
		(((impl_POA_Nautilus_HistoryFrame *)servant)->view);
}
