/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
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
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* nautilus-content-view-frame.c: Implementation for object that
   represents the frame a nautilus content view plugs into. */

#include <config.h>
#include "nautilus-content-view-frame.h"
#include "nautilus-view-frame-private.h"
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <bonobo/bonobo-control.h>


typedef struct {
  POA_Nautilus_View servant;
  gpointer bonobo_object;

  NautilusContentViewFrame *view;
} impl_POA_Nautilus_ContentView;

extern POA_Nautilus_View__epv libnautilus_Nautilus_View_epv;

static POA_Nautilus_ContentView__epv impl_Nautilus_ContentView_epv = {
  NULL /* _private */
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_ContentView__vepv impl_Nautilus_ContentView_vepv =
{
  &base_epv,
  NULL,
  &libnautilus_Nautilus_View_epv,
  &impl_Nautilus_ContentView_epv
};

static void nautilus_content_view_frame_initialize        (NautilusContentViewFrame      *view);
static void nautilus_content_view_frame_destroy           (NautilusContentViewFrame      *view);
static void nautilus_content_view_frame_initialize_class  (NautilusContentViewFrameClass *klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusContentViewFrame, nautilus_content_view_frame, NAUTILUS_TYPE_VIEW_FRAME)

static void
nautilus_content_view_frame_initialize (NautilusContentViewFrame *view)
{
}

NautilusContentViewFrame *
nautilus_content_view_frame_new (GtkWidget *widget)
{
  BonoboObject *control;
  
  control = BONOBO_OBJECT (bonobo_control_new (widget));

  return nautilus_content_view_frame_new_from_bonobo_control (control);
}

NautilusContentViewFrame *
nautilus_content_view_frame_new_from_bonobo_control (BonoboObject *bonobo_control)
{
  NautilusContentViewFrame *view;
  
  view = NAUTILUS_CONTENT_VIEW_FRAME (gtk_object_new (NAUTILUS_TYPE_CONTENT_VIEW_FRAME,
						      "bonobo_control", bonobo_control,
						      NULL));

  return view;
}

static void
nautilus_content_view_frame_destroy    (NautilusContentViewFrame *view)
{
  NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, GTK_OBJECT (view));
}

static void
nautilus_content_view_frame_initialize_class (NautilusContentViewFrameClass *klass)
{
  NautilusViewFrameClass *view_class;
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;
  object_class->destroy = (void (*)(GtkObject *))nautilus_content_view_frame_destroy;

  view_class = (NautilusViewFrameClass *) klass;
  view_class->servant_init_func = POA_Nautilus_ContentView__init;
  view_class->servant_destroy_func = POA_Nautilus_ContentView__fini;
  view_class->vepv = &impl_Nautilus_ContentView_vepv;
}

void
nautilus_content_view_frame_request_title_change (NautilusContentViewFrame *view,
					          const char *new_title)
{
  CORBA_Environment ev;

  g_return_if_fail (NAUTILUS_IS_CONTENT_VIEW_FRAME (view));
  g_return_if_fail (new_title != NULL);

  CORBA_exception_init(&ev);

  if (nautilus_view_frame_ensure_view_frame (NAUTILUS_VIEW_FRAME (view))) {
    Nautilus_ContentViewFrame_request_title_change (NAUTILUS_VIEW_FRAME (view)->private->view_frame, new_title, &ev);
    if (ev._major != CORBA_NO_EXCEPTION)
      {
	CORBA_Object_release(NAUTILUS_VIEW_FRAME (view)->private->view_frame, &ev);
	NAUTILUS_VIEW_FRAME (view)->private->view_frame = CORBA_OBJECT_NIL;
      }
  }

  CORBA_exception_free(&ev);
}
