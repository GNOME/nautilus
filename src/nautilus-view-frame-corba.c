/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999 Red Hat, Inc.
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
/* ntl-view-frame-svr.c: CORBA server implementation of the object
   representing a data view frame. */

#include "nautilus.h"

typedef struct {
  POA_Nautilus_ViewFrame servant;
  gpointer gnome_object;

  NautilusView *view;
} impl_POA_Nautilus_ViewFrame;

static Nautilus_ViewWindow
impl_Nautilus_ViewFrame__get_main_window(impl_POA_Nautilus_ViewFrame *servant,
                                         CORBA_Environment *ev);
static void
impl_Nautilus_ViewFrame_request_location_change(impl_POA_Nautilus_ViewFrame * servant,
						Nautilus_NavigationRequestInfo * navinfo,
						CORBA_Environment * ev);

static void
impl_Nautilus_ViewFrame_request_selection_change(impl_POA_Nautilus_ViewFrame * servant,
						 Nautilus_SelectionRequestInfo * selinfo,
						 CORBA_Environment * ev);
static void
impl_Nautilus_ViewFrame_request_status_change(impl_POA_Nautilus_ViewFrame * servant,
                                              Nautilus_StatusRequestInfo * statinfo,
                                              CORBA_Environment * ev);

static void
nautilus_view_request_location_change(NautilusView *view,
				      Nautilus_NavigationRequestInfo *loc);

static void
nautilus_view_request_selection_change (NautilusView              *view,
					Nautilus_SelectionRequestInfo *loc);

static void
nautilus_view_request_status_change    (NautilusView              *view,
                                        Nautilus_StatusRequestInfo *loc);



POA_Nautilus_ViewFrame__epv impl_Nautilus_ViewFrame_epv =
{
   NULL,			/* _private */
   (void(*))&impl_Nautilus_ViewFrame__get_main_window,
   (void(*))&impl_Nautilus_ViewFrame_request_status_change,
   (void(*))&impl_Nautilus_ViewFrame_request_location_change,
   (void(*))&impl_Nautilus_ViewFrame_request_selection_change
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

POA_Nautilus_ViewFrame__vepv impl_Nautilus_ViewFrame_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_ViewFrame_epv
};

static void
impl_Nautilus_ViewFrame__destroy(GnomeObject *obj, impl_POA_Nautilus_ViewFrame *servant)
{
   PortableServer_ObjectId *objid;
   CORBA_Environment ev;
   NautilusViewClass *klass;
   void (*servant_destroy_func)(PortableServer_Servant, CORBA_Environment *);

   klass = NAUTILUS_VIEW_CLASS(GTK_OBJECT(servant->view)->klass);

   CORBA_exception_init(&ev);

   objid = PortableServer_POA_servant_to_id(bonobo_poa(), servant, &ev);
   PortableServer_POA_deactivate_object(bonobo_poa(), objid, &ev);
   CORBA_free(objid);
   obj->servant = NULL;

   servant_destroy_func = klass->servant_destroy_func;
   servant_destroy_func((PortableServer_Servant) servant, &ev);
   g_free(servant);
   CORBA_exception_free(&ev);
}

GnomeObject *
impl_Nautilus_ViewFrame__create(NautilusView *view, CORBA_Environment * ev)
{
   GnomeObject *retval;
   impl_POA_Nautilus_ViewFrame *newservant;
   NautilusViewClass *klass;
   void (*servant_init_func)(PortableServer_Servant, CORBA_Environment *);

   klass = NAUTILUS_VIEW_CLASS(GTK_OBJECT(view)->klass);
   newservant = g_new0(impl_POA_Nautilus_ViewFrame, 1);
   newservant->servant.vepv = klass->vepv;
   if(!newservant->servant.vepv->GNOME_Unknown_epv)
     newservant->servant.vepv->GNOME_Unknown_epv = gnome_object_get_epv();
   newservant->view = view;
   servant_init_func = klass->servant_init_func;
   servant_init_func((PortableServer_Servant) newservant, ev);

   retval = gnome_object_new_from_servant(newservant);

   gtk_signal_connect(GTK_OBJECT(retval), "destroy", GTK_SIGNAL_FUNC(impl_Nautilus_ViewFrame__destroy), newservant);

   return retval;
}

static Nautilus_ViewWindow
impl_Nautilus_ViewFrame__get_main_window(impl_POA_Nautilus_ViewFrame *servant,
                                         CORBA_Environment *ev)
{
  return CORBA_Object_duplicate(gnome_object_corba_objref(NAUTILUS_WINDOW(servant->view->main_window)->ntl_viewwindow), ev);
}

static void
impl_Nautilus_ViewFrame_request_location_change(impl_POA_Nautilus_ViewFrame * servant,
						Nautilus_NavigationRequestInfo * navinfo,
						CORBA_Environment * ev)
{
  NautilusView *view;
  
  view = servant->view;
  g_return_if_fail (view != NULL);
  g_return_if_fail (NAUTILUS_IS_VIEW (view));
  g_return_if_fail (NAUTILUS_VIEW (view)->main_window != NULL);

  nautilus_window_request_location_change(NAUTILUS_WINDOW(view->main_window), navinfo, GTK_WIDGET(view));
}

static void
impl_Nautilus_ViewFrame_request_selection_change(impl_POA_Nautilus_ViewFrame * servant,
						 Nautilus_SelectionRequestInfo * selinfo,
						 CORBA_Environment * ev)
{
  nautilus_window_request_selection_change(NAUTILUS_WINDOW(servant->view->main_window), 
                                           selinfo, 
                                           GTK_WIDGET(servant->view));  
}

static void
impl_Nautilus_ViewFrame_request_status_change(impl_POA_Nautilus_ViewFrame * servant,
                                              Nautilus_StatusRequestInfo * statinfo,
                                              CORBA_Environment * ev)
{
  nautilus_window_request_status_change(NAUTILUS_WINDOW(servant->view->main_window), 
                                        statinfo, 
                                        GTK_WIDGET(servant->view));  
}

