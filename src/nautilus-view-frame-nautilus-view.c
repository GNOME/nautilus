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

#include <config.h>

#include "nautilus-view-frame-private.h"
#include "nautilus-window.h"
#include <bonobo/bonobo-control-frame.h>

typedef struct {
  BonoboControlFrame *control_frame;
  CORBA_Object view_client;
} NautilusViewInfo;

static gboolean
nautilus_view_try_load_client(NautilusViewFrame *view, CORBA_Object obj, CORBA_Environment *ev)
{
  Bonobo_Control control;
  NautilusViewInfo *nvi;
  Bonobo_UIHandler uih = bonobo_object_corba_objref(BONOBO_OBJECT(view->ui_handler));
  nvi = view->component_data = g_new0(NautilusViewInfo, 1);

  control = Bonobo_Unknown_query_interface(obj, "IDL:Bonobo/Control:1.0", ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    control = CORBA_OBJECT_NIL;

  if(CORBA_Object_is_nil(control, ev))
    goto out;

  nvi->view_client = CORBA_Object_duplicate(obj, ev);

  nvi->control_frame = bonobo_control_frame_new(uih);
  bonobo_object_add_interface(BONOBO_OBJECT(nvi->control_frame), view->view_frame);

  bonobo_control_frame_bind_to_control(nvi->control_frame, control);
  view->client_widget = bonobo_control_frame_get_widget(nvi->control_frame);

  Bonobo_Unknown_unref(control, ev);
  CORBA_Object_release(control, ev);

  return TRUE;

 out:
  g_free(nvi);

  return FALSE;
}

static void
destroy_nautilus_view(NautilusViewFrame *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  CORBA_Object_release(nvi->view_client, ev);

  g_free(nvi);
}

static void
load_location(NautilusViewFrame *view, Nautilus_URI location, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_load_location(nvi->view_client, location, ev);

  if(ev->_major != CORBA_NO_EXCEPTION)
    /* FIXME: Is a destroy really sufficient here? Who does the unref? */
    gtk_object_destroy(GTK_OBJECT(view));
}

static void
stop_loading(NautilusViewFrame *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_stop_loading(nvi->view_client, ev);

  if(ev->_major != CORBA_NO_EXCEPTION)
    /* FIXME: Is a destroy really sufficient here? Who does the unref? */
    gtk_object_destroy(GTK_OBJECT(view));
}

static void
selection_changed(NautilusViewFrame *view, const Nautilus_URIList *selection, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_selection_changed(nvi->view_client, selection, ev);

  if(ev->_major != CORBA_NO_EXCEPTION)
    /* FIXME: Is a destroy really sufficient here? Who does the unref? */
    gtk_object_destroy(GTK_OBJECT(view));
}

NautilusViewComponentType nautilus_view_component_type = {
  "IDL:Nautilus/View:1.0",
  &nautilus_view_try_load_client, /* try_load */
  &destroy_nautilus_view, /* destroy */
  &load_location, /* load_location */
  &stop_loading, /* stop_loading */
  &selection_changed /* selection_changed */
};
