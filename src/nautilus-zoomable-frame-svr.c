/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
/* ntl-zoomable-frame-svr.c: CORBA server implementation of
   Nautilus::ZoomableFrame interface of a nautilus ViewFrame. */

#include <config.h>
#include "nautilus.h"
#include "ntl-view-private.h"

typedef struct {
  POA_Nautilus_ZoomableFrame servant;
  gpointer bonobo_object;

  NautilusView *view;
} impl_POA_Nautilus_ZoomableFrame;

static void impl_Nautilus_ZoomableFrame_notify_zoom_level (impl_POA_Nautilus_ZoomableFrame *servant,
                                                           CORBA_double                     level,
                                                           CORBA_Environment               *ev);


POA_Nautilus_ZoomableFrame__epv impl_Nautilus_ZoomableFrame_epv =
{
   NULL,			/* _private */
   (void(*))&impl_Nautilus_ZoomableFrame_notify_zoom_level,
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

POA_Nautilus_ZoomableFrame__vepv impl_Nautilus_ZoomableFrame_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_ZoomableFrame_epv
};

static void
impl_Nautilus_ZoomableFrame__destroy (BonoboObject *obj, 
                                      impl_POA_Nautilus_ZoomableFrame *servant)
{
   PortableServer_ObjectId *objid;
   CORBA_Environment ev;
   NautilusViewClass *klass;
   void (*servant_destroy_func) (PortableServer_Servant, CORBA_Environment *);

   klass = NAUTILUS_VIEW_CLASS (GTK_OBJECT (servant->view)->klass);

   CORBA_exception_init(&ev);

   objid = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
   PortableServer_POA_deactivate_object (bonobo_poa (), objid, &ev);
   CORBA_free (objid);
   obj->servant = NULL;

   servant_destroy_func = klass->zoomable_servant_destroy_func;
   servant_destroy_func ((PortableServer_Servant) servant, &ev);
   g_free (servant);
   CORBA_exception_free (&ev);
}

BonoboObject *
impl_Nautilus_ZoomableFrame__create (NautilusView *view, 
                                     CORBA_Environment * ev)
{
   BonoboObject *retval;
   impl_POA_Nautilus_ZoomableFrame *newservant;
   NautilusViewClass *klass;
   void (*servant_init_func) (PortableServer_Servant, CORBA_Environment *);

   klass = NAUTILUS_VIEW_CLASS (GTK_OBJECT (view)->klass);
   newservant = g_new0 (impl_POA_Nautilus_ZoomableFrame, 1);

   newservant->servant.vepv = klass->vepv;

   if(!newservant->servant.vepv->Bonobo_Unknown_epv)
     newservant->servant.vepv->Bonobo_Unknown_epv = bonobo_object_get_epv();

   newservant->view = view;
   servant_init_func = klass->zoomable_servant_init_func;
   servant_init_func ((PortableServer_Servant) newservant, ev);

   retval = bonobo_object_new_from_servant (newservant);

   gtk_signal_connect (GTK_OBJECT (retval), "destroy", 
                       GTK_SIGNAL_FUNC (impl_Nautilus_ZoomableFrame__destroy), newservant);

   return retval;
}

static void
impl_Nautilus_ZoomableFrame_notify_zoom_level (impl_POA_Nautilus_ZoomableFrame *servant,
                                               CORBA_double       level,
                                               CORBA_Environment *ev)
{
  nautilus_view_notify_zoom_level (servant->view, level);
}
