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

#include "nautilus.h"
#include "ntl-view-private.h"
#include <libnautilus-extensions/bonobo-stream-vfs.h>

typedef struct {
  BonoboObject *container, *client_site, *view_frame;
} BonoboSubdocInfo;

static void
destroy_bonobo_subdoc_view(NautilusView *view, CORBA_Environment *ev)
{
  BonoboSubdocInfo *bsi = view->component_data;

  g_free(bsi);
}

static void
bonobo_subdoc_notify_location_change(NautilusView *view, Nautilus_NavigationInfo *real_nav_ctx, CORBA_Environment *ev)
{
  Bonobo_PersistStream persist;

  if((persist = bonobo_object_client_query_interface(view->client_object, "IDL:Bonobo/PersistStream:1.0",
                                                     NULL))
     && !CORBA_Object_is_nil(persist, ev))
    {
      BonoboStream *stream;
      Nautilus_ProgressRequestInfo pri;
      
      stream = bonobo_stream_vfs_open(real_nav_ctx->actual_uri, Bonobo_Storage_READ);
      pri.amount = 0;
      if(stream)
        pri.type = Nautilus_PROGRESS_UNDERWAY;
      else
        pri.type = Nautilus_PROGRESS_DONE_ERROR;
      nautilus_view_request_progress_change(view, &pri);
      if(stream)
        {
          Bonobo_PersistStream_load (persist,
                                     (Bonobo_Stream) bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
                                     ev);
          Bonobo_Unknown_unref(persist, ev);
          CORBA_Object_release(persist, ev);
          pri.type = Nautilus_PROGRESS_DONE_OK;
          nautilus_view_request_progress_change(view, &pri);
        }
    }
}      

static gboolean
bonobo_subdoc_try_load_client(NautilusView *view, CORBA_Object obj, CORBA_Environment *ev)
{
  BonoboSubdocInfo *bsi;
  Bonobo_UIHandler uih = bonobo_object_corba_objref(BONOBO_OBJECT(nautilus_window_get_uih(NAUTILUS_WINDOW(view->main_window))));


  view->component_data = bsi = g_new0(BonoboSubdocInfo, 1);

  bsi->container = BONOBO_OBJECT(bonobo_container_new());
      
  bsi->client_site =
    BONOBO_OBJECT(bonobo_client_site_new(BONOBO_CONTAINER(bsi->container)));
  bonobo_client_site_bind_embeddable(BONOBO_CLIENT_SITE(bsi->client_site), view->client_object);
  bonobo_container_add(BONOBO_CONTAINER(bsi->container), bsi->client_site);

  bsi->view_frame = BONOBO_OBJECT (bonobo_client_site_new_view (BONOBO_CLIENT_SITE (bsi->client_site), uih));

  g_assert(bsi->view_frame);

  bonobo_object_add_interface(BONOBO_OBJECT(bsi->view_frame), view->view_frame);
      
  view->client_widget = bonobo_view_frame_get_wrapper(BONOBO_VIEW_FRAME(bsi->view_frame));
      
  bonobo_wrapper_set_visibility (BONOBO_WRAPPER (view->client_widget), FALSE);

  bonobo_view_frame_set_covered (BONOBO_VIEW_FRAME (bsi->view_frame), FALSE); 

  return TRUE;
}

NautilusViewComponentType bonobo_subdoc_component_type = {
  "IDL:Bonobo/Embeddable:1.0",
  &bonobo_subdoc_try_load_client, /* try_load */
  &destroy_bonobo_subdoc_view, /* destroy */
  NULL, /* save_state */
  NULL, /* load_state */
  &bonobo_subdoc_notify_location_change, /* notify_location_change */
  NULL, /* stop_location_change */
  NULL, /* notify_selection_change */
  NULL  /* show_properties */
};

