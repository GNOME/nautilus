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
#include <libnautilus-extensions/bonobo-stream-vfs.h>
#include <bonobo/bonobo-container.h>
#include <bonobo/bonobo-client-site.h>
#include <bonobo/bonobo-view-frame.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>

typedef struct {
  BonoboContainer *container;
  BonoboClientSite *client_site;
  BonoboViewFrame *view_frame;
} BonoboSubdocInfo;

static void
destroy_bonobo_subdoc_view (NautilusViewFrame *view, CORBA_Environment *ev)
{
  BonoboSubdocInfo *bsi = view->component_data;
  g_free(bsi);
}

static void
bonobo_subdoc_load_location (NautilusViewFrame *view,
                             Nautilus_URI location,
                             CORBA_Environment *ev)
{
  Bonobo_PersistStream persist;
  Bonobo_PersistFile persist_file;
  gchar *local_path;

  persist = bonobo_object_client_query_interface(view->client_object, "IDL:Bonobo/PersistStream:1.0", NULL);

  if((persist != NULL) && !CORBA_Object_is_nil(persist, ev))
    {
      BonoboStream *stream;
      
      stream = bonobo_stream_vfs_open(location, Bonobo_Storage_READ);
      if(stream == NULL)
        nautilus_view_frame_report_load_failed (view);
      else
        {
          nautilus_view_frame_report_load_underway (view);
          
          /* FIXME bugzilla.eazel.com 1248: 
           * Dan Winship points out that we should pass the
           * MIME type here to work with new implementers of
           * PersistStream that pay attention to the MIME type. It
           * doesn't matter right now, but we should fix it
           * eventually. Currently, we don't store the MIME type, but
           * it should be easy to keep it around and pass it in here.
           */
          Bonobo_PersistStream_load
            (persist,
             bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
             "", /* MIME type of stream */
             ev);
          
          nautilus_view_frame_report_load_complete (view);
        }

      Bonobo_Unknown_unref(persist, ev);
      CORBA_Object_release(persist, ev);

      if (stream != NULL)
        {
          return;
        }
    }
  else if (persist)
    {
      /* FIXME: Free it. */
    }


  /* FIXME: Need to implement ProgressiveDataSink. */

  persist_file = bonobo_object_client_query_interface(view->client_object, "IDL:Bonobo/PersistFile:1.0", NULL);

  /* FIXME:

     The OAF query may return a component that supports PersistFile
     even it it's not a file:/// URI.
  */

  local_path = nautilus_get_local_path_from_uri (location);

  if (persist_file != NULL
      && !CORBA_Object_is_nil (persist_file, ev)
      && local_path != NULL)
    {
      nautilus_view_frame_report_load_underway(view);

      Bonobo_PersistFile_load(persist_file, local_path, ev);

      /* FIXME: Find out whether the loading was successful. */

      Bonobo_Unknown_unref(persist_file, ev);
      CORBA_Object_release(persist_file, ev);

      g_free (local_path);

      nautilus_view_frame_report_load_complete(view);
    }
  else
    {
      if (persist_file)
        {
          if (!CORBA_Object_is_nil (persist_file, ev))
            {
              Bonobo_Unknown_unref(persist_file, ev);
              CORBA_Object_release(persist_file, ev);
            }
          else
            {
              /* FIXME: Free it. */
            }
        }

      g_free (local_path);
    }
}

static gboolean
bonobo_subdoc_try_load_client(NautilusViewFrame *view, CORBA_Object obj, CORBA_Environment *ev)
{
  BonoboSubdocInfo *bsi;
  Bonobo_UIHandler uih = bonobo_object_corba_objref(BONOBO_OBJECT(view->ui_handler));


  view->component_data = bsi = g_new0(BonoboSubdocInfo, 1);

  bsi->container = bonobo_container_new();
      
  bsi->client_site = bonobo_client_site_new(bsi->container);
  bonobo_client_site_bind_embeddable(bsi->client_site, view->client_object);
  bonobo_container_add(bsi->container, BONOBO_OBJECT (bsi->client_site));

  bsi->view_frame = bonobo_client_site_new_view (bsi->client_site, uih);

  bonobo_object_add_interface(BONOBO_OBJECT(bsi->view_frame), view->view_frame);
      
  view->client_widget = bonobo_view_frame_get_wrapper(bsi->view_frame);
      
  bonobo_wrapper_set_visibility (BONOBO_WRAPPER (view->client_widget), FALSE);

  bonobo_view_frame_set_covered (bsi->view_frame, FALSE); 

  return TRUE;
}

NautilusViewComponentType bonobo_subdoc_component_type = {
  "IDL:Bonobo/Embeddable:1.0",
  &bonobo_subdoc_try_load_client, /* try_load */
  &destroy_bonobo_subdoc_view, /* destroy */
  &bonobo_subdoc_load_location, /* load_location */
  NULL, /* stop_loading */
  NULL /* selection_changed */
};
