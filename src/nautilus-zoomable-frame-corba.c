/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* nautilus-zoomable-frame-corba.c: CORBA server implementation of
   Nautilus::ZoomableFrame interface of a nautilus ViewFrame. */

#include <config.h>
#include "nautilus-view-frame-private.h"

#include <bonobo/bonobo-main.h>

typedef struct {
	POA_Nautilus_ZoomableFrame servant;
	gpointer bonobo_object;
	
	NautilusViewFrame *view;
} impl_POA_Nautilus_ZoomableFrame;

static void impl_Nautilus_ZoomableFrame_report_zoom_level_changed (PortableServer_Servant  servant,
							    CORBA_double            level,
							    CORBA_Environment      *ev);

POA_Nautilus_ZoomableFrame__epv impl_Nautilus_ZoomableFrame_epv =
{
   NULL,
   impl_Nautilus_ZoomableFrame_report_zoom_level_changed,
};

static PortableServer_ServantBase__epv base_epv;
POA_Nautilus_ZoomableFrame__vepv impl_Nautilus_ZoomableFrame_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_ZoomableFrame_epv
};

static void
impl_Nautilus_ZoomableFrame__destroy (BonoboObject *object, 
                                      impl_POA_Nautilus_ZoomableFrame *servant)
{
   PortableServer_ObjectId *object_id;
   CORBA_Environment ev;
   NautilusViewFrameClass *klass;

   klass = NAUTILUS_VIEW_FRAME_CLASS (GTK_OBJECT (servant->view)->klass);

   CORBA_exception_init (&ev);

   object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
   PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
   CORBA_free (object_id);
   object->servant = NULL;

   POA_Nautilus_ZoomableFrame__fini ((PortableServer_Servant) servant, &ev);
   g_free (servant);

   CORBA_exception_free (&ev);
}

BonoboObject *
impl_Nautilus_ZoomableFrame__create (NautilusViewFrame *view, 
                                     CORBA_Environment * ev)
{
   BonoboObject *retval;
   impl_POA_Nautilus_ZoomableFrame *newservant;
   NautilusViewFrameClass *klass;

   klass = NAUTILUS_VIEW_FRAME_CLASS (GTK_OBJECT (view)->klass);
   newservant = g_new0 (impl_POA_Nautilus_ZoomableFrame, 1);

   impl_Nautilus_ZoomableFrame_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv();

   newservant->servant.vepv = &impl_Nautilus_ZoomableFrame_vepv;

   newservant->view = view;
   POA_Nautilus_ZoomableFrame__init ((PortableServer_Servant) newservant, ev);

   retval = bonobo_object_new_from_servant (newservant);

   gtk_signal_connect (GTK_OBJECT (retval), "destroy", 
                       GTK_SIGNAL_FUNC (impl_Nautilus_ZoomableFrame__destroy), newservant);

   return retval;
}

static void
impl_Nautilus_ZoomableFrame_report_zoom_level_changed (PortableServer_Servant servant,
						       CORBA_double level,
						       CORBA_Environment *ev)
{
	nautilus_view_frame_zoom_level_changed
		(((impl_POA_Nautilus_ZoomableFrame *) servant)->view,
		 level);
}
